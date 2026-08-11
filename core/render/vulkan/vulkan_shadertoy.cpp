#include "core/render/vulkan/vulkan_backend.h"
#include "core/render/vulkan/vulkan_shadertoy_shaders.h"

#include "core/render/image_source.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

namespace core::render::vulkan {
namespace {

constexpr std::uint64_t kInvalidFrameToken = std::numeric_limits<std::uint64_t>::max();
constexpr VkFormat kShaderToyTargetFormat = VK_FORMAT_R32G32B32A32_SFLOAT;

struct alignas(16) ShaderToyUniformBlock {
    float resolutionTime[4]{};
    float timeData[4]{};
    float date[4]{};
    float mouse[4]{};
    float channelTime[4]{};
    float channelResolution[4][4]{};
    float sampleFrame[4]{};
    float customUniforms[kShaderToyCustomUniformCount][4]{};
};

bool readSpirv(const std::string& path,
               std::vector<std::uint32_t>& words,
               ShaderToyError* error,
               const ShaderToyPass& pass) {
    std::ifstream input(std::filesystem::u8path(path), std::ios::binary | std::ios::ate);
    if (!input) {
        if (error != nullptr) {
            *error = {ShaderToyErrorCode::SourceReadFailed, {}, pass.name, "fragment",
                      path, 0, "Unable to read precompiled Shadertoy SPIR-V."};
        }
        return false;
    }
    const std::streamsize size = input.tellg();
    if (size <= 0 || size % 4 != 0) {
        if (error != nullptr) {
            *error = {ShaderToyErrorCode::SourceReadFailed, {}, pass.name, "fragment",
                      path, 0, "Shadertoy SPIR-V is empty or not 4-byte aligned."};
        }
        return false;
    }
    input.seekg(0);
    words.resize(static_cast<std::size_t>(size) / sizeof(std::uint32_t));
    if (!input.read(reinterpret_cast<char*>(words.data()), size)) {
        if (error != nullptr) {
            *error = {ShaderToyErrorCode::SourceReadFailed, {}, pass.name,
                      "fragment", path, 0,
                      "Unable to read complete Shadertoy SPIR-V."};
        }
        return false;
    }
    constexpr std::uint32_t kSpirvMagic = 0x07230203u;
    constexpr std::size_t kSpirvHeaderWordCount = 5;
    if (words.size() < kSpirvHeaderWordCount || words[0] != kSpirvMagic) {
        if (error != nullptr) {
            *error = {ShaderToyErrorCode::ShaderCompileFailed, {}, pass.name,
                      "fragment", path, 0,
                      "Shadertoy SPIR-V has an invalid module header."};
        }
        return false;
    }
    return true;
}

struct SpirvDecoration {
    std::uint32_t descriptorSet = 0;
    std::uint32_t binding = 0;
    bool hasDescriptorSet = false;
    bool hasBinding = false;
};

struct SpirvVariable {
    std::uint32_t resultType = 0;
    std::uint32_t resultId = 0;
    std::uint32_t storageClass = 0;
};

void setSpirvInterfaceError(ShaderToyError* error,
                            const ShaderToyPass& pass,
                            const std::string& message) {
    if (error == nullptr) return;
    *error = {ShaderToyErrorCode::ShaderCompileFailed, {}, pass.name,
              "fragment", pass.spirvPath, 0, message};
}

bool validateSpirvChannelTypes(const std::vector<std::uint32_t>& words,
                               const ShaderToyPass& pass,
                               ShaderToyError* error) {
    constexpr std::size_t kHeaderWordCount = 5;
    constexpr std::uint16_t kOpTypeImage = 25;
    constexpr std::uint16_t kOpTypeSampledImage = 27;
    constexpr std::uint16_t kOpTypePointer = 32;
    constexpr std::uint16_t kOpVariable = 59;
    constexpr std::uint16_t kOpDecorate = 71;
    constexpr std::uint32_t kStorageClassUniformConstant = 0;
    constexpr std::uint32_t kDecorationBinding = 33;
    constexpr std::uint32_t kDecorationDescriptorSet = 34;

    std::unordered_map<std::uint32_t, SpirvDecoration> decorations;
    std::unordered_map<std::uint32_t, std::uint32_t> pointerTypes;
    std::unordered_map<std::uint32_t, std::uint32_t> sampledImageTypes;
    std::unordered_map<std::uint32_t, std::uint32_t> imageDimensions;
    std::vector<SpirvVariable> variables;

    for (std::size_t offset = kHeaderWordCount; offset < words.size();) {
        const std::uint32_t instruction = words[offset];
        const std::uint16_t wordCount =
            static_cast<std::uint16_t>(instruction >> 16u);
        const std::uint16_t opcode =
            static_cast<std::uint16_t>(instruction & 0xffffu);
        if (wordCount == 0 || wordCount > words.size() - offset) {
            setSpirvInterfaceError(
                error, pass,
                "Shadertoy SPIR-V contains a truncated instruction at word " +
                    std::to_string(offset) + ".");
            return false;
        }

        const std::uint32_t* operands = words.data() + offset;
        switch (opcode) {
        case kOpDecorate:
            if (wordCount >= 4 &&
                (operands[2] == kDecorationBinding ||
                 operands[2] == kDecorationDescriptorSet)) {
                SpirvDecoration& decoration = decorations[operands[1]];
                if (operands[2] == kDecorationBinding) {
                    decoration.binding = operands[3];
                    decoration.hasBinding = true;
                } else {
                    decoration.descriptorSet = operands[3];
                    decoration.hasDescriptorSet = true;
                }
            }
            break;
        case kOpTypeImage:
            if (wordCount < 9) {
                setSpirvInterfaceError(
                    error, pass,
                    "Shadertoy SPIR-V contains an invalid OpTypeImage instruction.");
                return false;
            }
            imageDimensions[operands[1]] = operands[3];
            break;
        case kOpTypeSampledImage:
            if (wordCount != 3) {
                setSpirvInterfaceError(
                    error, pass,
                    "Shadertoy SPIR-V contains an invalid OpTypeSampledImage instruction.");
                return false;
            }
            sampledImageTypes[operands[1]] = operands[2];
            break;
        case kOpTypePointer:
            if (wordCount != 4) {
                setSpirvInterfaceError(
                    error, pass,
                    "Shadertoy SPIR-V contains an invalid OpTypePointer instruction.");
                return false;
            }
            if (operands[2] == kStorageClassUniformConstant) {
                pointerTypes[operands[1]] = operands[3];
            }
            break;
        case kOpVariable:
            if (wordCount < 4) {
                setSpirvInterfaceError(
                    error, pass,
                    "Shadertoy SPIR-V contains an invalid OpVariable instruction.");
                return false;
            }
            variables.push_back({operands[1], operands[2], operands[3]});
            break;
        default:
            break;
        }
        offset += wordCount;
    }

    std::array<bool, kShaderToyChannelCount> validated{};
    for (const SpirvVariable& variable : variables) {
        const auto decoration = decorations.find(variable.resultId);
        if (decoration == decorations.end() ||
            !decoration->second.hasBinding ||
            !decoration->second.hasDescriptorSet ||
            decoration->second.descriptorSet != 0 ||
            decoration->second.binding < 1 ||
            decoration->second.binding > kShaderToyChannelCount) {
            continue;
        }

        const std::size_t channelIndex =
            static_cast<std::size_t>(decoration->second.binding - 1);
        const std::string channelName =
            "iChannel" + std::to_string(channelIndex);
        if (validated[channelIndex]) {
            setSpirvInterfaceError(
                error, pass,
                "Shadertoy SPIR-V declares multiple variables for descriptor set 0 binding " +
                    std::to_string(decoration->second.binding) + " (" +
                    channelName + ").");
            return false;
        }
        validated[channelIndex] = true;

        if (variable.storageClass != kStorageClassUniformConstant) {
            setSpirvInterfaceError(
                error, pass,
                "Shadertoy SPIR-V descriptor set 0 binding " +
                    std::to_string(decoration->second.binding) + " (" +
                    channelName + ") is not a UniformConstant sampler.");
            return false;
        }
        const auto pointerType = pointerTypes.find(variable.resultType);
        const auto sampledImageType =
            pointerType != pointerTypes.end()
                ? sampledImageTypes.find(pointerType->second)
                : sampledImageTypes.end();
        const auto imageDimension =
            sampledImageType != sampledImageTypes.end()
                ? imageDimensions.find(sampledImageType->second)
                : imageDimensions.end();
        if (pointerType == pointerTypes.end() ||
            sampledImageType == sampledImageTypes.end() ||
            imageDimension == imageDimensions.end()) {
            setSpirvInterfaceError(
                error, pass,
                "Shadertoy SPIR-V descriptor set 0 binding " +
                    std::to_string(decoration->second.binding) + " (" +
                    channelName + ") is not a combined image sampler.");
            return false;
        }

        constexpr std::uint32_t kSpirvImageDim2D = 1;
        if (imageDimension->second != kSpirvImageDim2D) {
            setSpirvInterfaceError(
                error, pass,
                "Shadertoy SPIR-V descriptor set 0 binding " +
                    std::to_string(decoration->second.binding) + " (" +
                    channelName + ") must be a sampler2D.");
            return false;
        }
    }

    return true;
}

void setResourceError(ShaderToyError* error,
                      const ShaderToyPass* pass,
                      const char* stage,
                      const char* message) {
    if (error == nullptr) return;
    *error = {ShaderToyErrorCode::ResourceCreationFailed, {},
              pass != nullptr ? pass->name : std::string{},
              stage != nullptr ? stage : std::string{},
              pass != nullptr ? pass->spirvPath : std::string{},
              0, message != nullptr ? message : "Vulkan Shadertoy resource creation failed."};
}

void setHotReloadError(ShaderToyError& error,
                       const ShaderToyPass& pass,
                       const char* message) {
    error = {ShaderToyErrorCode::ShaderCompileFailed, {}, pass.name,
             "fragment", pass.spirvPath, 0,
             message != nullptr ? message :
                 "Unable to reload Shadertoy SPIR-V."};
}

std::string imageKey(const ShaderToyChannel& channel) {
    return channel.source;
}

bool createSampler(VkDevice device,
                   bool repeat,
                   VkSampler& sampler) {
    VkSamplerCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    info.magFilter = VK_FILTER_LINEAR;
    info.minFilter = info.magFilter;
    info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    info.addressModeU = repeat ? VK_SAMPLER_ADDRESS_MODE_REPEAT
                               : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    info.addressModeV = info.addressModeU;
    info.addressModeW = info.addressModeU;
    info.maxLod = 0.0f;
    return vkCreateSampler(device, &info, nullptr, &sampler) == VK_SUCCESS;
}

bool validRgba8Image(const image::StaticImageData& source) {
    if (!source.pixels || source.width <= 0 || source.height <= 0) {
        return false;
    }
    const std::size_t width = static_cast<std::size_t>(source.width);
    const std::size_t height = static_cast<std::size_t>(source.height);
    return width <= std::numeric_limits<std::size_t>::max() / height &&
           width * height <= std::numeric_limits<std::size_t>::max() / 4u &&
           source.byteCount == width * height * 4u;
}

bool createShaderToyRenderPass(VkDevice device,
                               VkFormat format,
                               VkRenderPass& renderPass) {
    VkAttachmentDescription attachment{};
    attachment.format = format;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference attachmentReference{
        0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &attachmentReference;

    VkRenderPassCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    info.attachmentCount = 1;
    info.pAttachments = &attachment;
    info.subpassCount = 1;
    info.pSubpasses = &subpass;
    return vkCreateRenderPass(device, &info, nullptr, &renderPass) == VK_SUCCESS;
}

std::filesystem::file_time_type spirvSourceTime(const std::string& path,
                                                 bool& valid) {
    std::error_code fileError;
    const auto result = std::filesystem::last_write_time(
        std::filesystem::u8path(path), fileError);
    valid = !fileError;
    return result;
}

bool createShaderToyPipeline(VkDevice device,
                             VkPipelineLayout pipelineLayout,
                             VkRenderPass renderPass,
                             VkShaderModule vertexShader,
                             VkShaderModule fragmentShader,
                             VkPipeline& pipeline) {
    VkPipelineShaderStageCreateInfo shaderStages[2]{};
    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = vertexShader;
    shaderStages[0].pName = "main";
    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = fragmentShader;
    shaderStages[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                     VK_COLOR_COMPONENT_G_BIT |
                                     VK_COLOR_COMPONENT_B_BIT |
                                     VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo blend{};
    blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount = 1;
    blend.pAttachments = &blendAttachment;
    const VkDynamicState dynamicStates[]{VK_DYNAMIC_STATE_VIEWPORT,
                                        VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic{};
    dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic.dynamicStateCount = 2;
    dynamic.pDynamicStates = dynamicStates;
    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisample;
    pipelineInfo.pColorBlendState = &blend;
    pipelineInfo.pDynamicState = &dynamic;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;
    return vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                     nullptr, &pipeline) == VK_SUCCESS;
}
bool shaderToyFormatSupported(VkPhysicalDevice physicalDevice,
                              std::string& missingFeatures) {
    VkFormatProperties properties{};
    vkGetPhysicalDeviceFormatProperties(
        physicalDevice, kShaderToyTargetFormat, &properties);
    const VkFormatFeatureFlags available = properties.optimalTilingFeatures;
    const VkFormatFeatureFlags required =
        VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
        VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
    if ((available & required) != required) {
        missingFeatures =
            "color attachment, sampled image, and linear filtering";
        return false;
    }

    VkImageFormatProperties imageProperties{};
    const VkImageUsageFlags usage =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_SAMPLED_BIT |
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
        VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (vkGetPhysicalDeviceImageFormatProperties(
            physicalDevice, kShaderToyTargetFormat, VK_IMAGE_TYPE_2D,
            VK_IMAGE_TILING_OPTIMAL, usage, 0,
            &imageProperties) != VK_SUCCESS) {
        missingFeatures =
            "color attachment, sampled image, and transfer usage";
        return false;
    }
    return true;
}

} // namespace

struct VulkanRenderBackend::ShaderToyResource {
    struct Pass {
        VkRenderPass renderPass = VK_NULL_HANDLE;
        VkPipeline pipeline = VK_NULL_HANDLE;
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        VkBuffer uniformBuffer = VK_NULL_HANDLE;
        VkDeviceMemory uniformMemory = VK_NULL_HANDLE;
        ShaderToyUniformBlock* uniforms = nullptr;
        std::array<TextureResource, 2> targets;
        std::array<VkFramebuffer, 2> framebuffers{};
        std::array<VkImageView, 2> attachmentViews{};
        std::array<VkSampler, kShaderToyChannelCount> samplers{};
        int width = 0;
        int height = 0;
        std::filesystem::file_time_type spirvTime{};
        bool spirvTimeValid = false;
    };

    struct Image {
        std::shared_ptr<const image::StaticImageData> source;
        TextureResource texture;
    };

    std::vector<Pass> passes;
    std::unordered_map<std::string, Image> images;
    TextureResource empty;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    int width = 0;
    int height = 0;
    int currentIndex = 0;
    bool hasOutput = false;
    bool imagesUploaded = false;
    ShaderToyError hotReloadError;
    std::uint64_t lastFrameToken = kInvalidFrameToken;
};

VulkanRenderBackend::ShaderToyHandle VulkanRenderBackend::createShaderToy(
    const ShaderToyGraph& graph,
    ShaderToyError* error) {
    if (error != nullptr) *error = {};
    if (device_ == VK_NULL_HANDLE) {
        setResourceError(error, nullptr, "device", "Vulkan device is not initialized.");
        return nullptr;
    }
    const ShaderToyValidationResult validation = validateShaderToyGraph(graph);
    if (!validation.valid()) {
        if (error != nullptr) *error = validation.errors.front();
        return nullptr;
    }

    auto toy = std::make_unique<ShaderToyResource>();
    toy->passes.resize(graph.passes.size());

    std::string missingFeatures;
    if (!shaderToyFormatSupported(physicalDevice_, missingFeatures)) {
        setResourceError(
            error, nullptr, "target-format",
            (std::string("Vulkan RGBA32F does not support required ") +
             missingFeatures + " features.").c_str());
        return nullptr;
    }

    std::array<VkDescriptorSetLayoutBinding, 5> bindings{};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    for (std::uint32_t index = 1; index < bindings.size(); ++index) {
        bindings[index].binding = index;
        bindings[index].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[index].descriptorCount = 1;
        bindings[index].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();
    if (vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr,
                                    &toy->descriptorSetLayout) != VK_SUCCESS) {
        setResourceError(error, nullptr, "descriptor", "Unable to create Shadertoy descriptor layout.");
        destroyShaderToyResource(*toy);
        return nullptr;
    }

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &toy->descriptorSetLayout;
    if (vkCreatePipelineLayout(device_, &pipelineLayoutInfo, nullptr,
                               &toy->pipelineLayout) != VK_SUCCESS) {
        setResourceError(error, nullptr, "pipeline-layout", "Unable to create Shadertoy pipeline layout.");
        destroyShaderToyResource(*toy);
        return nullptr;
    }

    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0] = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    static_cast<std::uint32_t>(toy->passes.size())};
    poolSizes[1] = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    static_cast<std::uint32_t>(toy->passes.size() * kShaderToyChannelCount)};
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = static_cast<std::uint32_t>(toy->passes.size());
    poolInfo.poolSizeCount = static_cast<std::uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    if (vkCreateDescriptorPool(device_, &poolInfo, nullptr, &toy->descriptorPool) != VK_SUCCESS) {
        setResourceError(error, nullptr, "descriptor-pool", "Unable to create Shadertoy descriptor pool.");
        destroyShaderToyResource(*toy);
        return nullptr;
    }

    VkShaderModule vertexShader = createShaderModule(
        device_, shaders::kShaderToyVertexSpirv, shaders::kShaderToyVertexSpirvSize);
    if (vertexShader == VK_NULL_HANDLE) {
        setResourceError(error, nullptr, "vertex", "Unable to create fixed Shadertoy vertex shader.");
        destroyShaderToyResource(*toy);
        return nullptr;
    }

    for (std::size_t passIndex = 0; passIndex < toy->passes.size(); ++passIndex) {
        ShaderToyResource::Pass& pass = toy->passes[passIndex];
        const ShaderToyPass& passShape = graph.passes[passIndex];
        if (!createShaderToyRenderPass(device_, kShaderToyTargetFormat,
                                       pass.renderPass)) {
            setResourceError(
                error, &passShape, "render-pass",
                "Unable to create RGBA32F Shadertoy render pass.");
            vkDestroyShaderModule(device_, vertexShader, nullptr);
            destroyShaderToyResource(*toy);
            return nullptr;
        }

        std::vector<std::uint32_t> spirv;
        if (!readSpirv(passShape.spirvPath, spirv, error, passShape)) {
            vkDestroyShaderModule(device_, vertexShader, nullptr);
            destroyShaderToyResource(*toy);
            return nullptr;
        }
        if (!validateSpirvChannelTypes(spirv, passShape, error)) {
            vkDestroyShaderModule(device_, vertexShader, nullptr);
            destroyShaderToyResource(*toy);
            return nullptr;
        }
        VkShaderModule fragmentShader =
            createShaderModule(device_, spirv.data(), spirv.size() * sizeof(std::uint32_t));
        if (fragmentShader == VK_NULL_HANDLE) {
            setResourceError(error, &passShape, "fragment", "Unable to create Shadertoy fragment shader module.");
            vkDestroyShaderModule(device_, vertexShader, nullptr);
            destroyShaderToyResource(*toy);
            return nullptr;
        }

        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = sizeof(ShaderToyUniformBlock);
        bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(device_, &bufferInfo, nullptr, &pass.uniformBuffer) != VK_SUCCESS) {
            vkDestroyShaderModule(device_, fragmentShader, nullptr);
            vkDestroyShaderModule(device_, vertexShader, nullptr);
            setResourceError(error, &passShape, "uniform-buffer", "Unable to create Shadertoy uniform buffer.");
            destroyShaderToyResource(*toy);
            return nullptr;
        }
        VkMemoryRequirements memoryRequirements{};
        vkGetBufferMemoryRequirements(device_, pass.uniformBuffer, &memoryRequirements);
        VkMemoryAllocateInfo memoryInfo{};
        memoryInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memoryInfo.allocationSize = memoryRequirements.size;
        memoryInfo.memoryTypeIndex = findMemoryType(
            memoryRequirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (memoryInfo.memoryTypeIndex == std::numeric_limits<std::uint32_t>::max() ||
            vkAllocateMemory(device_, &memoryInfo, nullptr, &pass.uniformMemory) != VK_SUCCESS ||
            vkBindBufferMemory(device_, pass.uniformBuffer, pass.uniformMemory, 0) != VK_SUCCESS ||
            vkMapMemory(device_, pass.uniformMemory, 0, sizeof(ShaderToyUniformBlock), 0,
                        reinterpret_cast<void**>(&pass.uniforms)) != VK_SUCCESS) {
            vkDestroyShaderModule(device_, fragmentShader, nullptr);
            vkDestroyShaderModule(device_, vertexShader, nullptr);
            setResourceError(error, &passShape, "uniform-memory", "Unable to map Shadertoy uniform memory.");
            destroyShaderToyResource(*toy);
            return nullptr;
        }

        VkDescriptorSetAllocateInfo descriptorInfo{};
        descriptorInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        descriptorInfo.descriptorPool = toy->descriptorPool;
        descriptorInfo.descriptorSetCount = 1;
        descriptorInfo.pSetLayouts = &toy->descriptorSetLayout;
        if (vkAllocateDescriptorSets(device_, &descriptorInfo, &pass.descriptorSet) != VK_SUCCESS) {
            vkDestroyShaderModule(device_, fragmentShader, nullptr);
            vkDestroyShaderModule(device_, vertexShader, nullptr);
            setResourceError(error, &passShape, "descriptor-set", "Unable to allocate Shadertoy descriptor set.");
            destroyShaderToyResource(*toy);
            return nullptr;
        }
        for (std::size_t channelIndex = 0;
             channelIndex < kShaderToyChannelCount;
             ++channelIndex) {
            if (!createSampler(device_,
                               passShape.channels[channelIndex].kind ==
                                   ShaderToyChannelKind::Image,
                               pass.samplers[channelIndex])) {
                vkDestroyShaderModule(device_, fragmentShader, nullptr);
                vkDestroyShaderModule(device_, vertexShader, nullptr);
                setResourceError(error, &passShape, "sampler",
                                 "Unable to create Shadertoy sampler.");
                destroyShaderToyResource(*toy);
                return nullptr;
            }
        }

        const bool pipelineCreated = createShaderToyPipeline(
            device_, toy->pipelineLayout, pass.renderPass, vertexShader,
            fragmentShader, pass.pipeline);
        vkDestroyShaderModule(device_, fragmentShader, nullptr);
        if (!pipelineCreated) {
            vkDestroyShaderModule(device_, vertexShader, nullptr);
            setResourceError(error, &passShape, "pipeline", "Unable to create Shadertoy graphics pipeline.");
            destroyShaderToyResource(*toy);
            return nullptr;
        }
        pass.spirvTime = spirvSourceTime(passShape.spirvPath,
                                         pass.spirvTimeValid);
    }
    vkDestroyShaderModule(device_, vertexShader, nullptr);


    for (const ShaderToyPass& pass : graph.passes) {
        for (const ShaderToyChannel& channel : pass.channels) {
            if (channel.kind != ShaderToyChannelKind::Image) {
                continue;
            }
            const std::string key = imageKey(channel);
            if (toy->images.find(key) != toy->images.end()) continue;
            ShaderToyResource::Image imageResource;
            bool pending = false;
            imageResource.source = image::loadStaticImage(
                channel.source, true, &pending);
            if (!imageResource.source ||
                !validRgba8Image(*imageResource.source)) {
                if (error != nullptr) {
                    *error = {ShaderToyErrorCode::SourceReadFailed, {},
                              pass.name, "image", channel.source, 0,
                              pending ? "Shadertoy image channel is not ready." :
                                        "Unable to decode an RGBA8 Shadertoy image channel."};
                }
                destroyShaderToyResource(*toy);
                return nullptr;
            }
            toy->images.emplace(key, std::move(imageResource));
        }
    }

    ShaderToyResource* handle = toy.release();
    shaderToys_.push_back(handle);
    return handle;
}

namespace {

void fillUniformBlock(ShaderToyUniformBlock& target,
                      const ShaderToyGraph& graph,
                      const ShaderToyFrameData& frame,
                      int width,
                      int height) {
    target = {};
    target.resolutionTime[0] = static_cast<float>(width);
    target.resolutionTime[1] = static_cast<float>(height);
    target.resolutionTime[2] = 1.0f;
    target.resolutionTime[3] = frame.time;
    target.timeData[0] = frame.deltaTime;
    target.timeData[1] = frame.frameRate;
    target.timeData[2] = static_cast<float>(frame.frame);
    std::copy(frame.date.begin(), frame.date.end(), target.date);
    std::copy(frame.mouse.begin(), frame.mouse.end(), target.mouse);
    std::copy(frame.channelTime.begin(), frame.channelTime.end(), target.channelTime);
    target.sampleFrame[0] = frame.sampleRate;
    for (std::size_t index = 0;
         index < graph.uniforms.size() && index < kShaderToyCustomUniformCount;
         ++index) {
        std::copy(graph.uniforms[index].values.begin(),
                  graph.uniforms[index].values.end(),
                  target.customUniforms[index]);
    }
}

} // namespace

bool VulkanRenderBackend::updateShaderToyTexture(
    TextureResource& texture,
    const unsigned char* pixels,
    int width,
    int height) {
    if (!frameActive_ || pixels == nullptr || width <= 0 || height <= 0) {
        return false;
    }

    VkDeviceSize uploadSize = 4u;
    const auto appendDimension = [&](std::uint64_t value) {
        if (value > std::numeric_limits<VkDeviceSize>::max() / uploadSize) {
            return false;
        }
        uploadSize *= static_cast<VkDeviceSize>(value);
        return true;
    };
    if (!appendDimension(static_cast<std::uint64_t>(width)) ||
        !appendDimension(static_cast<std::uint64_t>(height)) ||
        uploadSize > std::numeric_limits<std::size_t>::max()) {
        return false;
    }

    endActiveRenderPass();
    const bool replace = texture.image == VK_NULL_HANDLE ||
        texture.width != width || texture.height != height ||
        texture.format != VK_FORMAT_R8G8B8A8_UNORM;
    TextureResource replacement;
    TextureResource* upload = &texture;
    if (replace) {
        if (!createTargetImage(
                replacement, width, height, VK_FORMAT_R8G8B8A8_UNORM,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                    VK_IMAGE_USAGE_SAMPLED_BIT)) {
            beginLoadPass();
            return false;
        }
        upload = &replacement;
    }

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceSize stagingOffset = 0;
    void* mapped = nullptr;
    if (!allocateUploadRegion(uploadSize, stagingBuffer, stagingOffset,
                              mapped)) {
        if (replace) destroyTextureResource(replacement);
        beginLoadPass();
        return false;
    }
    std::memcpy(mapped, pixels, static_cast<std::size_t>(uploadSize));

    VkCommandBuffer commandBuffer = currentCommandBuffer();
    transitionImageLayout(commandBuffer, upload->image, upload->layout,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkBufferImageCopy copy{};
    copy.bufferOffset = stagingOffset;
    copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.layerCount = 1;
    copy.imageExtent = {static_cast<std::uint32_t>(width),
                        static_cast<std::uint32_t>(height),
                        1};
    vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, upload->image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
    transitionImageLayout(commandBuffer, upload->image,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    upload->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    if (replace) {
        if (texture.image != VK_NULL_HANDLE) {
            auto* retired = new TextureResource(texture);
            pendingTextureDeletes_.push_back(retired);
        }
        texture = replacement;
        replacement = {};
    }
    ++texture.generation;
    beginLoadPass();
    return true;
}
VulkanRenderBackend::TextureHandle VulkanRenderBackend::renderShaderToy(
    ShaderToyHandle handle,
    const ShaderToyGraph& graph,
    int width,
    int height,
    const ShaderToyFrameData& frame,
    bool paused,
    bool reset,
    ShaderToyError* error) {
    auto* toy = static_cast<ShaderToyResource*>(handle);
    if (error != nullptr) *error = {};
    if (!frameActive_ || toy == nullptr || width <= 0 || height <= 0 ||
        graph.passes.size() != toy->passes.size()) {
        setResourceError(error, nullptr, "render", "Vulkan Shadertoy rendering requires an active frame.");
        return nullptr;
    }
    endActiveRenderPass();

    for (std::size_t passIndex = 0; passIndex < toy->passes.size();
         ++passIndex) {
        ShaderToyResource::Pass& pass = toy->passes[passIndex];
        const ShaderToyPass& passShape = graph.passes[passIndex];
        bool nextTimeValid = false;
        const auto nextTime = spirvSourceTime(passShape.spirvPath,
                                              nextTimeValid);
        if ((nextTimeValid && pass.spirvTimeValid &&
             pass.spirvTime == nextTime) ||
            (!nextTimeValid && !pass.spirvTimeValid)) {
            continue;
        }

        pass.spirvTime = nextTime;
        pass.spirvTimeValid = nextTimeValid;
        ShaderToyError reloadError;
        std::vector<std::uint32_t> spirv;
        if (!readSpirv(passShape.spirvPath, spirv, &reloadError,
                       passShape)) {
            toy->hotReloadError = std::move(reloadError);
            continue;
        }
        if (!validateSpirvChannelTypes(spirv, passShape, &reloadError)) {
            toy->hotReloadError = std::move(reloadError);
            continue;
        }
        VkShaderModule vertexShader = createShaderModule(
            device_, shaders::kShaderToyVertexSpirv,
            shaders::kShaderToyVertexSpirvSize);
        VkShaderModule fragmentShader = createShaderModule(
            device_, spirv.data(), spirv.size() * sizeof(std::uint32_t));
        if (vertexShader == VK_NULL_HANDLE ||
            fragmentShader == VK_NULL_HANDLE) {
            if (fragmentShader != VK_NULL_HANDLE) {
                vkDestroyShaderModule(device_, fragmentShader, nullptr);
            }
            if (vertexShader != VK_NULL_HANDLE) {
                vkDestroyShaderModule(device_, vertexShader, nullptr);
            }
            setHotReloadError(
                toy->hotReloadError, passShape,
                "Unable to create replacement Shadertoy shader module.");
            continue;
        }

        VkPipeline replacement = VK_NULL_HANDLE;
        const bool created = createShaderToyPipeline(
            device_, toy->pipelineLayout, pass.renderPass, vertexShader,
            fragmentShader, replacement);
        vkDestroyShaderModule(device_, fragmentShader, nullptr);
        vkDestroyShaderModule(device_, vertexShader, nullptr);
        if (!created) {
            setHotReloadError(
                toy->hotReloadError, passShape,
                "Unable to create replacement Shadertoy graphics pipeline.");
            continue;
        }

        if (pass.pipeline != VK_NULL_HANDLE) {
            pendingShaderToyPipelineDeletes_.push_back(pass.pipeline);
        }
        pass.pipeline = replacement;
        if (toy->hotReloadError.passName == passShape.name) {
            toy->hotReloadError = {};
        }
    }
    if (error != nullptr) *error = toy->hotReloadError;

    endActiveRenderPass();

    if (!toy->imagesUploaded) {
        const std::array<unsigned char, 4> black{0, 0, 0, 255};
        if (!updateTexture(&toy->empty, black.data(), 1, 1)) {
            setResourceError(error, nullptr, "empty-texture", "Unable to upload Shadertoy empty texture.");
            return nullptr;
        }
        for (auto& entry : toy->images) {
            ShaderToyResource::Image& image = entry.second;
            const bool uploaded = image.source &&
                updateShaderToyTexture(
                    image.texture, image.source->pixels.get(),
                    image.source->width, image.source->height);
            if (!uploaded) {
                setResourceError(error, nullptr, "image",
                                 "Unable to upload Shadertoy image channel.");
                if (error != nullptr) error->sourcePath = entry.first;
                return nullptr;
            }
        }
        toy->imagesUploaded = true;
        endActiveRenderPass();
    }

    const bool resize = toy->width != width || toy->height != height ||
                        toy->passes.empty() || toy->passes.front().targets[0].image == VK_NULL_HANDLE;
    if (resize) {
        struct ReplacementPass {
            std::array<TextureResource, 2> targets{};
            std::array<VkFramebuffer, 2> framebuffers{};
            std::array<VkImageView, 2> attachmentViews{};
        };
        std::vector<ReplacementPass> replacements(toy->passes.size());
        auto destroyReplacements = [&]() {
            for (ReplacementPass& pass : replacements) {
                for (VkFramebuffer& framebuffer : pass.framebuffers) {
                    if (framebuffer != VK_NULL_HANDLE) {
                        vkDestroyFramebuffer(device_, framebuffer, nullptr);
                        framebuffer = VK_NULL_HANDLE;
                    }
                }
                for (VkImageView& view : pass.attachmentViews) {
                    if (view != VK_NULL_HANDLE) {
                        vkDestroyImageView(device_, view, nullptr);
                        view = VK_NULL_HANDLE;
                    }
                }
                for (TextureResource& target : pass.targets) {
                    destroyTextureResource(target);
                }
            }
        };
        bool replacementValid = true;
        for (std::size_t passIndex = 0;
             passIndex < replacements.size(); ++passIndex) {
            ReplacementPass& replacement = replacements[passIndex];
            ShaderToyResource::Pass& pass = toy->passes[passIndex];
            const ShaderToyPass& passShape = graph.passes[passIndex];
            for (int index = 0; index < 2; ++index) {
                if (!createTargetImage(
                        replacement.targets[index], width, height,
                        kShaderToyTargetFormat,
                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                        VK_IMAGE_USAGE_SAMPLED_BIT |
                        VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                        VK_IMAGE_USAGE_TRANSFER_DST_BIT) ||
                    !ensureTextureSampler(replacement.targets[index])) {
                    setResourceError(
                        error, &passShape, "target",
                        "Unable to create RGBA32F Shadertoy targets.");
                    replacementValid = false;
                    break;
                }
                VkImageViewCreateInfo attachmentViewInfo{};
                attachmentViewInfo.sType =
                    VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                attachmentViewInfo.image = replacement.targets[index].image;
                attachmentViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                attachmentViewInfo.format = kShaderToyTargetFormat;
                attachmentViewInfo.subresourceRange.aspectMask =
                    VK_IMAGE_ASPECT_COLOR_BIT;
                attachmentViewInfo.subresourceRange.levelCount = 1;
                attachmentViewInfo.subresourceRange.layerCount = 1;
                if (vkCreateImageView(device_, &attachmentViewInfo, nullptr,
                                      &replacement.attachmentViews[index]) !=
                    VK_SUCCESS) {
                    setResourceError(error, &passShape, "attachment-view",
                                     "Unable to create Shadertoy attachment view.");
                    replacementValid = false;
                    break;
                }
                VkImageView attachment = replacement.attachmentViews[index];
                VkFramebufferCreateInfo framebufferInfo{};
                framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                framebufferInfo.renderPass = pass.renderPass;
                framebufferInfo.attachmentCount = 1;
                framebufferInfo.pAttachments = &attachment;
                framebufferInfo.width = static_cast<std::uint32_t>(width);
                framebufferInfo.height = static_cast<std::uint32_t>(height);
                framebufferInfo.layers = 1;
                if (vkCreateFramebuffer(
                        device_, &framebufferInfo, nullptr,
                        &replacement.framebuffers[index]) != VK_SUCCESS) {
                    setResourceError(error, &passShape, "framebuffer",
                                     "Unable to create Shadertoy framebuffer.");
                    replacementValid = false;
                    break;
                }
            }
            if (!replacementValid) break;
        }
        if (!replacementValid) {
            destroyReplacements();
            return toy->hasOutput ? &toy->passes.back().targets[toy->currentIndex] : nullptr;
        }
        for (std::size_t passIndex = 0; passIndex < toy->passes.size(); ++passIndex) {
            ShaderToyResource::Pass& pass = toy->passes[passIndex];
            for (int index = 0; index < 2; ++index) {
                if (pass.framebuffers[index] != VK_NULL_HANDLE) {
                    vkDestroyFramebuffer(device_, pass.framebuffers[index], nullptr);
                }
                if (pass.attachmentViews[index] != VK_NULL_HANDLE) {
                    vkDestroyImageView(device_, pass.attachmentViews[index],
                                       nullptr);
                }
                destroyTextureResource(pass.targets[index]);
                pass.targets[index] = replacements[passIndex].targets[index];
                pass.framebuffers[index] = replacements[passIndex].framebuffers[index];
                pass.attachmentViews[index] =
                    replacements[passIndex].attachmentViews[index];
                replacements[passIndex].targets[index] = {};
                replacements[passIndex].framebuffers[index] = VK_NULL_HANDLE;
                replacements[passIndex].attachmentViews[index] =
                    VK_NULL_HANDLE;
            }
            pass.width = width;
            pass.height = height;
        }
        toy->width = width;
        toy->height = height;
        toy->currentIndex = 0;
        toy->hasOutput = false;
        toy->lastFrameToken = kInvalidFrameToken;
        reset = true;
    }

    VkCommandBuffer commandBuffer = currentCommandBuffer();
    auto clearTarget = [&](TextureResource& target) {
        transitionImageLayout(commandBuffer, target.image, target.layout,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        target.layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        VkClearColorValue clear{{0.0f, 0.0f, 0.0f, 1.0f}};
        VkImageSubresourceRange range{};
        range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        range.levelCount = 1;
        range.layerCount = 1;
        vkCmdClearColorImage(commandBuffer, target.image,
                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear, 1, &range);
        transitionImageLayout(commandBuffer, target.image, target.layout,
                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        target.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    };
    if (reset) {
        for (ShaderToyResource::Pass& pass : toy->passes) {
            clearTarget(pass.targets[0]);
            clearTarget(pass.targets[1]);
        }
        toy->currentIndex = 0;
        toy->hasOutput = false;
        toy->lastFrameToken = kInvalidFrameToken;
    }
    if ((paused && toy->hasOutput) ||
        (toy->lastFrameToken == frame.frameToken && toy->hasOutput)) {
        return &toy->passes.back().targets[toy->currentIndex];
    }

    const int previousIndex = toy->currentIndex;
    const int targetIndex = 1 - previousIndex;
    for (std::size_t passIndex = 0; passIndex < toy->passes.size(); ++passIndex) {
        ShaderToyResource::Pass& pass = toy->passes[passIndex];
        const ShaderToyPass& passShape = graph.passes[passIndex];
        TextureResource& target = pass.targets[targetIndex];
        transitionImageLayout(commandBuffer, target.image, target.layout,
                              VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        target.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        std::array<TextureResource*, kShaderToyChannelCount> channels{};
        for (std::size_t channelIndex = 0;
             channelIndex < kShaderToyChannelCount;
            ++channelIndex) {
            const ShaderToyChannel& channel = passShape.channels[channelIndex];
            TextureResource* texture = &toy->empty;
            if (channel.kind == ShaderToyChannelKind::Image) {
                const auto found = toy->images.find(imageKey(channel));
                if (found != toy->images.end()) {
                    texture = &found->second.texture;
                }
            } else if (channel.kind == ShaderToyChannelKind::Self) {
                if (toy->hasOutput) {
                    texture = &pass.targets[previousIndex];
                }
            } else if (channel.kind == ShaderToyChannelKind::Buffer) {
                const ShaderToyPass* sourcePass = graph.findPass(channel.source);
                if (sourcePass != nullptr) {
                    const std::size_t sourceIndex =
                        static_cast<std::size_t>(sourcePass - graph.passes.data());
                    if (sourceIndex < passIndex || toy->hasOutput) {
                        const int readIndex = sourceIndex < passIndex ? targetIndex : previousIndex;
                        texture = &toy->passes[sourceIndex].targets[readIndex];
                    }
                }
            }
            if (texture->layout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
                transitionImageLayout(commandBuffer, texture->image, texture->layout,
                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                texture->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            }
            channels[channelIndex] = texture;
        }

        fillUniformBlock(*pass.uniforms, graph, frame,
                         pass.width, pass.height);
        for (std::size_t channelIndex = 0;
             channelIndex < kShaderToyChannelCount;
             ++channelIndex) {
            pass.uniforms->channelResolution[channelIndex][0] =
                static_cast<float>(channels[channelIndex]->width);
            pass.uniforms->channelResolution[channelIndex][1] =
                static_cast<float>(channels[channelIndex]->height);
            pass.uniforms->channelResolution[channelIndex][2] = 1.0f;
        }

        VkDescriptorBufferInfo uniformInfo{};
        uniformInfo.buffer = pass.uniformBuffer;
        uniformInfo.range = sizeof(ShaderToyUniformBlock);
        std::array<VkDescriptorImageInfo, kShaderToyChannelCount> imageInfos{};
        std::array<VkWriteDescriptorSet, 1 + kShaderToyChannelCount> writes{};
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = pass.descriptorSet;
        writes[0].dstBinding = 0;
        writes[0].descriptorCount = 1;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[0].pBufferInfo = &uniformInfo;
        for (std::size_t channelIndex = 0;
             channelIndex < kShaderToyChannelCount;
             ++channelIndex) {
            imageInfos[channelIndex].sampler = pass.samplers[channelIndex];
            imageInfos[channelIndex].imageView = channels[channelIndex]->view;
            imageInfos[channelIndex].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            writes[channelIndex + 1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[channelIndex + 1].dstSet = pass.descriptorSet;
            writes[channelIndex + 1].dstBinding = static_cast<std::uint32_t>(channelIndex + 1);
            writes[channelIndex + 1].descriptorCount = 1;
            writes[channelIndex + 1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writes[channelIndex + 1].pImageInfo = &imageInfos[channelIndex];
        }
        vkUpdateDescriptorSets(device_, static_cast<std::uint32_t>(writes.size()),
                               writes.data(), 0, nullptr);

        VkClearValue clear{};
        clear.color = {{0.0f, 0.0f, 0.0f, 1.0f}};
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = pass.renderPass;
        renderPassInfo.framebuffer = pass.framebuffers[targetIndex];
        renderPassInfo.renderArea.extent = {
            static_cast<std::uint32_t>(pass.width),
            static_cast<std::uint32_t>(pass.height)
        };
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clear;
        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        VkViewport viewport{};
        viewport.width = static_cast<float>(pass.width);
        viewport.height = static_cast<float>(pass.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        VkRect2D scissor{{0, 0}, renderPassInfo.renderArea.extent};
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pass.pipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                toy->pipelineLayout, 0, 1, &pass.descriptorSet, 0, nullptr);
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
        vkCmdEndRenderPass(commandBuffer);

        transitionImageLayout(commandBuffer, target.image, target.layout,
                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        target.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        frameRecorded_ = true;
        ++currentRenderFrameStats().backendRenderPasses;
        currentRenderFrameStats().backendRenderPassPixels +=
            static_cast<std::uint64_t>(pass.width) *
            static_cast<std::uint64_t>(pass.height);
    }

    toy->currentIndex = targetIndex;
    toy->hasOutput = true;
    toy->lastFrameToken = frame.frameToken;
    if (error != nullptr) *error = toy->hotReloadError;
    return &toy->passes.back().targets[targetIndex];
}

void VulkanRenderBackend::destroyShaderToyResource(ShaderToyResource& toy) {
    for (ShaderToyResource::Pass& pass : toy.passes) {
        for (VkFramebuffer framebuffer : pass.framebuffers) {
            if (framebuffer != VK_NULL_HANDLE) {
                vkDestroyFramebuffer(device_, framebuffer, nullptr);
            }
        }
        for (VkImageView view : pass.attachmentViews) {
            if (view != VK_NULL_HANDLE) {
                vkDestroyImageView(device_, view, nullptr);
            }
        }
        for (VkSampler sampler : pass.samplers) {
            if (sampler != VK_NULL_HANDLE) {
                vkDestroySampler(device_, sampler, nullptr);
            }
        }
        for (TextureResource& target : pass.targets) {
            destroyTextureResource(target);
        }
        if (pass.uniformMemory != VK_NULL_HANDLE && pass.uniforms != nullptr) {
            vkUnmapMemory(device_, pass.uniformMemory);
            pass.uniforms = nullptr;
        }
        if (pass.uniformBuffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device_, pass.uniformBuffer, nullptr);
        }
        if (pass.uniformMemory != VK_NULL_HANDLE) {
            vkFreeMemory(device_, pass.uniformMemory, nullptr);
        }
        if (pass.pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device_, pass.pipeline, nullptr);
        }
        if (pass.renderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(device_, pass.renderPass, nullptr);
        }
    }
    for (auto& entry : toy.images) {
        destroyTextureResource(entry.second.texture);
    }
    destroyTextureResource(toy.empty);
    if (toy.descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_, toy.descriptorPool, nullptr);
    }
    if (toy.pipelineLayout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, toy.pipelineLayout, nullptr);
    }
    if (toy.descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_, toy.descriptorSetLayout, nullptr);
    }

    toy = {};
}

void VulkanRenderBackend::destroyShaderToy(ShaderToyHandle handle) {
    auto* toy = static_cast<ShaderToyResource*>(handle);
    if (toy == nullptr) return;
    shaderToys_.erase(std::remove(shaderToys_.begin(), shaderToys_.end(), toy),
                      shaderToys_.end());
    if (frameActive_) {
        pendingShaderToyDeletes_.push_back(toy);
        return;
    }
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
        destroyShaderToyResource(*toy);
    }
    delete toy;
}

void VulkanRenderBackend::releasePendingShaderToyPipelines() {
    for (VkPipeline pipeline : pendingShaderToyPipelineDeletes_) {
        if (pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device_, pipeline, nullptr);
        }
    }
    pendingShaderToyPipelineDeletes_.clear();
}

void VulkanRenderBackend::releasePendingShaderToys() {
    for (ShaderToyResource* toy : pendingShaderToyDeletes_) {
        if (toy != nullptr) {
            destroyShaderToyResource(*toy);
            delete toy;
        }
    }
    pendingShaderToyDeletes_.clear();
}

void VulkanRenderBackend::releaseAllShaderToys() {
    releasePendingShaderToyPipelines();
    releasePendingShaderToys();
    for (ShaderToyResource* toy : shaderToys_) {
        if (toy != nullptr) {
            destroyShaderToyResource(*toy);
            delete toy;
        }
    }
    shaderToys_.clear();
}

bool VulkanRenderBackend::readShaderToyPixel(ShaderToyHandle handle, float* rgba) {
    auto* toy = static_cast<ShaderToyResource*>(handle);
    if (toy == nullptr || rgba == nullptr || toy->passes.empty()) return false;
    const TextureResource& source = toy->passes.back().targets[toy->currentIndex];
    std::vector<float> pixels(
        static_cast<std::size_t>(source.width) * static_cast<std::size_t>(source.height) * 4u);
    if (!readShaderToyPixels(handle, pixels.data(), pixels.size())) return false;
    std::copy_n(pixels.data(), 4, rgba);
    return true;
}

bool VulkanRenderBackend::readShaderToyPixels(ShaderToyHandle handle,
                                              float* rgba,
                                              std::size_t floatCount) {
    auto* toy = static_cast<ShaderToyResource*>(handle);
    if (toy == nullptr || rgba == nullptr || !toy->hasOutput || frameActive_ ||
        device_ == VK_NULL_HANDLE || commandPool_ == VK_NULL_HANDLE ||
        inFlight_ == VK_NULL_HANDLE || toy->passes.empty()) {
        return false;
    }
    if (vkWaitForFences(device_, 1, &inFlight_, VK_TRUE, UINT64_MAX) !=
        VK_SUCCESS) {
        return false;
    }
    TextureResource& source =
        toy->passes.back().targets[toy->currentIndex];
    if (source.image == VK_NULL_HANDLE || source.width <= 0 ||
        source.height <= 0 || source.format != kShaderToyTargetFormat) {
        return false;
    }
    const std::size_t pixelCount = static_cast<std::size_t>(source.width) *
                                   static_cast<std::size_t>(source.height);
    if (pixelCount > std::numeric_limits<std::size_t>::max() / 4u) {
        return false;
    }
    const std::size_t required = pixelCount * 4u;
    if (floatCount < required ||
        required > std::numeric_limits<std::size_t>::max() / sizeof(float)) {
        return false;
    }
    const std::size_t rawByteCount = required * sizeof(float);
    const VkDeviceSize byteCount = static_cast<VkDeviceSize>(rawByteCount);

    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = byteCount;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device_, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        return false;
    }
    auto destroyReadbackBuffer = [&]() {
        if (buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device_, buffer, nullptr);
            buffer = VK_NULL_HANDLE;
        }
        if (memory != VK_NULL_HANDLE) {
            vkFreeMemory(device_, memory, nullptr);
            memory = VK_NULL_HANDLE;
        }
    };

    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device_, buffer, &requirements);
    VkMemoryAllocateInfo memoryInfo{};
    memoryInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryInfo.allocationSize = requirements.size;
    memoryInfo.memoryTypeIndex = findMemoryType(
        requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (memoryInfo.memoryTypeIndex ==
            std::numeric_limits<std::uint32_t>::max() ||
        vkAllocateMemory(device_, &memoryInfo, nullptr, &memory) != VK_SUCCESS ||
        vkBindBufferMemory(device_, buffer, memory, 0) != VK_SUCCESS) {
        destroyReadbackBuffer();
        return false;
    }

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.commandPool = commandPool_;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(device_, &allocateInfo, &commandBuffer) !=
        VK_SUCCESS) {
        destroyReadbackBuffer();
        return false;
    }
    auto freeCommandBuffer = [&]() {
        if (commandBuffer != VK_NULL_HANDLE) {
            vkFreeCommandBuffers(device_, commandPool_, 1, &commandBuffer);
            commandBuffer = VK_NULL_HANDLE;
        }
    };

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        freeCommandBuffer();
        destroyReadbackBuffer();
        return false;
    }
    const VkImageLayout originalLayout = source.layout;
    transitionImageLayout(commandBuffer, source.image, originalLayout,
                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    VkBufferImageCopy copy{};
    copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.layerCount = 1;
    copy.imageExtent = {
        static_cast<std::uint32_t>(source.width),
        static_cast<std::uint32_t>(source.height),
        1
    };
    vkCmdCopyImageToBuffer(commandBuffer, source.image,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buffer, 1,
                           &copy);
    transitionImageLayout(commandBuffer, source.image,
                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        freeCommandBuffer();
        destroyReadbackBuffer();
        return false;
    }

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &commandBuffer;
    const bool completed =
        vkQueueSubmit(graphicsQueue_, 1, &submit, VK_NULL_HANDLE) == VK_SUCCESS &&
        vkQueueWaitIdle(graphicsQueue_) == VK_SUCCESS;
    if (completed) {
        source.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    void* mapped = nullptr;
    const bool mappedOk = completed &&
        vkMapMemory(device_, memory, 0, byteCount, 0, &mapped) == VK_SUCCESS;
    if (mappedOk) {
        std::memcpy(rgba, mapped, rawByteCount);
        vkUnmapMemory(device_, memory);
    }
    freeCommandBuffer();
    destroyReadbackBuffer();
    return mappedOk;
}

} // namespace core::render::vulkan
