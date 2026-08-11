#include "core/render/vulkan/vulkan_backend.h"

#include "core/render/vulkan/vulkan_rounded_rect_shaders.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>
#include <limits>

namespace core::render::vulkan {

namespace {

constexpr std::size_t kPrimitiveVertexCapacity = 65536;
constexpr std::size_t kRoundedRectBatchVertexCapacity = 65536;
constexpr std::size_t kRoundedRectBatchMaxVertices = 6u * 2048u;

struct RoundedRectPushConstants {
    float windowAndShape[4] = {};
    float fillColor[4] = {};
    float gradientStart[4] = {};
    float gradientEnd[4] = {};
    float borderColor[4] = {};
    float rect[4] = {};
    float flags[4] = {};
    float flags2[4] = {};
};

static_assert(sizeof(RoundedRectPushConstants) == 128, "Rounded rect push constants must fit Vulkan 1.0 minimum size.");

struct RoundedRectBatchPushConstants {
    float windowSize[2] = {};
};

struct RoundedRectBatchVertex {
    float screen[3] = {};
    float local[2] = {};
    float fillColor[4] = {};
    float gradientStart[4] = {};
    float gradientEnd[4] = {};
    float borderColor[4] = {};
    float rect[4] = {};
    float shapeParams[4] = {};
    float modeParams[4] = {};
    float shadowParams[3] = {};
};

static_assert(sizeof(RoundedRectBatchVertex) == sizeof(float) * 36,
              "Rounded rect batch vertex layout must match the Vulkan shader.");

} // namespace

void VulkanRenderBackend::prepareBackdropBlur(const core::Rect& bounds, float blur, int windowWidth, int windowHeight) {
    flushRoundedRectBatch();
    if (!frameActive_ || blur <= 0.0f || windowWidth <= 0 || windowHeight <= 0 ||
        (!swapchainTransferSrcSupported_ && renderTarget_ == RenderTarget::Swapchain)) {
        backdropReady_ = false;
        return;
    }

    VkImage sourceImage = currentRenderImage();
    VkImageLayout sourceLayout = currentRenderImageLayout();
    const VkExtent2D sourceExtent = currentRenderExtent();
    if (sourceImage == VK_NULL_HANDLE || sourceExtent.width == 0 || sourceExtent.height == 0) {
        backdropReady_ = false;
        return;
    }

    const float sourceWidth = static_cast<float>(std::max(1u, sourceExtent.width));
    const float sourceHeight = static_cast<float>(std::max(1u, sourceExtent.height));
    const float leftF = std::clamp(std::floor(bounds.x - blur), 0.0f, std::max(sourceWidth - 1.0f, 0.0f));
    const float topF = std::clamp(std::floor(bounds.y - blur), 0.0f, std::max(sourceHeight - 1.0f, 0.0f));
    const float rightF = std::clamp(std::ceil(bounds.x + bounds.width + blur), leftF + 1.0f, sourceWidth);
    const float bottomF = std::clamp(std::ceil(bounds.y + bounds.height + blur), topF + 1.0f, sourceHeight);
    const auto left = static_cast<std::int32_t>(leftF);
    const auto top = static_cast<std::int32_t>(topF);
    const auto captureWidth = static_cast<std::uint32_t>(std::max(1.0f, rightF - leftF));
    const auto captureHeight = static_cast<std::uint32_t>(std::max(1.0f, bottomF - topF));

    if (!ensureRoundedRectPipeline() || !ensureBackdropResources(captureWidth, captureHeight)) {
        backdropReady_ = false;
        return;
    }
    if (!frameRecorded_) {
        recordClearPass(clearColor_);
    }
    endActiveRenderPass();

    VkCommandBuffer commandBuffer = currentCommandBuffer();
    if (renderTarget_ == RenderTarget::Swapchain) {
        transitionSwapchainImage(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    } else if (renderTarget_ == RenderTarget::RenderCache) {
        transitionRenderCacheImage(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    } else if (activeLayer_ != nullptr) {
        transitionLayerImage(*activeLayer_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    }
    transitionImageLayout(commandBuffer, backdropImage_, backdropImageLayout_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    backdropImageLayout_ = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    VkImageCopy copyRegion{};
    copyRegion.srcOffset = {left, top, 0};
    copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.srcSubresource.layerCount = 1;
    copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.dstSubresource.layerCount = 1;
    copyRegion.extent = {captureWidth, captureHeight, 1};
    vkCmdCopyImage(commandBuffer,
                   sourceImage,
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   backdropImage_,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1,
                   &copyRegion);

    transitionImageLayout(commandBuffer, backdropImage_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    backdropImageLayout_ = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    if (renderTarget_ == RenderTarget::Swapchain) {
        transitionSwapchainImage(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    } else if (renderTarget_ == RenderTarget::RenderCache) {
        transitionRenderCacheImage(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    } else if (activeLayer_ != nullptr) {
        transitionLayerImage(*activeLayer_, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    } else {
        setCurrentRenderImageLayout(sourceLayout);
    }
    backdropReady_ = true;
    beginLoadPass();
}

void VulkanRenderBackend::drawRoundedRect(const RoundedRectDrawCommand& command, int windowWidth, int windowHeight) {
    if (!frameActive_ || windowWidth <= 0 || windowHeight <= 0 || !roundedRectHasVisibleContent(command)) {
        return;
    }
    const bool needsBackdrop = !command.shadowPass && command.backdropBlur > 0.001f;
    if (!frameRecorded_) {
        recordClearPass(clearColor_);
    }
    if (!renderPassActive_) {
        return;
    }
    if (!needsBackdrop && appendRoundedRectBatch(command, windowWidth, windowHeight)) {
        return;
    }

    flushRoundedRectBatch();
    const std::uint32_t backdropWidth = needsBackdrop && backdropReady_ ? std::max(1u, backdropExtent_.width) : 1u;
    const std::uint32_t backdropHeight = needsBackdrop && backdropReady_ ? std::max(1u, backdropExtent_.height) : 1u;
    if (!ensureRoundedRectPipeline() || !ensureBackdropResources(backdropWidth, backdropHeight)) {
        return;
    }
    if (!renderPassActive_ || !ensurePrimitiveVertexBuffer(command.vertices.size())) {
        return;
    }

    const std::size_t vertexOffset = primitiveVertices_.used;
    auto* mappedVertices = static_cast<PrimitiveGeometryVertex*>(primitiveVertices_.mapped);
    std::copy(command.vertices.begin(), command.vertices.end(), mappedVertices + vertexOffset);
    primitiveVertices_.used += command.vertices.size();

    VkCommandBuffer commandBuffer = currentCommandBuffer();
    if (!applyDrawViewportAndScissor(windowWidth, windowHeight)) {
        return;
    }

    RoundedRectPushConstants constants{};
    constants.windowAndShape[0] = static_cast<float>(windowWidth);
    constants.windowAndShape[1] = static_cast<float>(windowHeight);
    constants.windowAndShape[2] = command.radius;
    constants.windowAndShape[3] = command.shadowPass ? 0.0f : command.border.width;
    writeColor(constants.fillColor, command.fillColor);
    writeColor(constants.gradientStart, command.gradient.start);
    writeColor(constants.gradientEnd, command.gradient.end);
    if (command.shadowPass) {
        constants.borderColor[0] = command.shadowOffset.x;
        constants.borderColor[1] = command.shadowOffset.y;
        constants.borderColor[2] = command.shadowSpread;
        constants.borderColor[3] = command.insetShadowPass ? 1.0f : 0.0f;
    } else {
        writeColor(constants.borderColor, command.border.color);
    }
    constants.rect[0] = command.rect.x;
    constants.rect[1] = command.rect.y;
    constants.rect[2] = command.rect.width;
    constants.rect[3] = command.rect.height;
    constants.flags[0] = command.opacity;
    constants.flags[1] = command.shadowPass ? command.shadowBlur : command.backdropOffset.x;
    constants.flags[2] = command.gradient.enabled && !command.shadowPass ? 1.0f : 0.0f;
    constants.flags[3] = static_cast<float>(command.gradient.direction == core::GradientDirection::Horizontal ? 0 : 1);
    constants.flags2[0] = command.shadowPass ? 1.0f : 0.0f;
    constants.flags2[1] = command.shadowPass ? 0.0f : command.backdropBlur;
    constants.flags2[2] = (!command.shadowPass && command.backdropBlur > 0.0f && backdropReady_) ? 1.0f : 0.0f;
    constants.flags2[3] = command.shadowPass
        ? (command.insetShadowPass ? 1.0f : 0.0f)
        : command.backdropOffset.y;

    const VkDeviceSize bufferOffset = static_cast<VkDeviceSize>(vertexOffset * sizeof(PrimitiveGeometryVertex));
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, roundedRectPipeline_);
    vkCmdBindDescriptorSets(commandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            roundedRectPipelineLayout_,
                            0,
                            1,
                            &roundedRectDescriptorSet_,
                            0,
                            nullptr);
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &primitiveVertices_.buffer, &bufferOffset);
    vkCmdPushConstants(commandBuffer,
                       roundedRectPipelineLayout_,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0,
                       sizeof(constants),
                       &constants);
    vkCmdDraw(commandBuffer, static_cast<std::uint32_t>(command.vertices.size()), 1, 0, 0);
}

bool VulkanRenderBackend::appendRoundedRectBatch(const RoundedRectDrawCommand& command,
                                                 int windowWidth,
                                                 int windowHeight) {
    if (roundedRectBatchCount_ > 0 &&
        (roundedRectBatchWindowWidth_ != windowWidth || roundedRectBatchWindowHeight_ != windowHeight)) {
        flushRoundedRectBatch();
    }
    if (roundedRectBatchCount_ + command.vertices.size() > kRoundedRectBatchMaxVertices) {
        flushRoundedRectBatch();
    }
    if (!ensureRoundedRectBatchPipeline() || !ensureRoundedRectBatchVertexBuffer(command.vertices.size())) {
        return false;
    }

    if (roundedRectBatchCount_ == 0) {
        roundedRectBatchStart_ = roundedRectBatchVertices_.used;
        roundedRectBatchWindowWidth_ = windowWidth;
        roundedRectBatchWindowHeight_ = windowHeight;
    }

    auto* vertices = static_cast<RoundedRectBatchVertex*>(roundedRectBatchVertices_.mapped);
    const auto copyColor = [](float (&target)[4], const core::Color& color) {
        target[0] = color.r;
        target[1] = color.g;
        target[2] = color.b;
        target[3] = color.a;
    };
    for (const PrimitiveGeometryVertex& source : command.vertices) {
        RoundedRectBatchVertex& target = vertices[roundedRectBatchVertices_.used++];
        target.screen[0] = source.screen.x;
        target.screen[1] = source.screen.y;
        target.screen[2] = source.screen.z;
        target.local[0] = source.local.x;
        target.local[1] = source.local.y;
        copyColor(target.fillColor, command.fillColor);
        copyColor(target.gradientStart, command.gradient.start);
        copyColor(target.gradientEnd, command.gradient.end);
        copyColor(target.borderColor, command.border.color);
        target.rect[0] = command.rect.x;
        target.rect[1] = command.rect.y;
        target.rect[2] = command.rect.width;
        target.rect[3] = command.rect.height;
        target.shapeParams[0] = command.radius;
        target.shapeParams[1] = command.shadowPass ? 0.0f : command.border.width;
        target.shapeParams[2] = command.opacity;
        target.shapeParams[3] = command.shadowBlur;
        target.modeParams[0] = command.gradient.enabled && !command.shadowPass ? 1.0f : 0.0f;
        target.modeParams[1] = command.gradient.direction == core::GradientDirection::Horizontal ? 0.0f : 1.0f;
        target.modeParams[2] = command.shadowPass ? 1.0f : 0.0f;
        target.modeParams[3] = command.insetShadowPass ? 1.0f : 0.0f;
        target.shadowParams[0] = command.shadowOffset.x;
        target.shadowParams[1] = command.shadowOffset.y;
        target.shadowParams[2] = command.shadowSpread;
    }
    roundedRectBatchCount_ += command.vertices.size();
    return true;
}

void VulkanRenderBackend::flushRoundedRectBatch() {
    if (roundedRectBatchCount_ == 0) {
        return;
    }

    const std::size_t batchStart = roundedRectBatchStart_;
    const std::size_t batchCount = roundedRectBatchCount_;
    const int windowWidth = roundedRectBatchWindowWidth_;
    const int windowHeight = roundedRectBatchWindowHeight_;
    roundedRectBatchStart_ = 0;
    roundedRectBatchCount_ = 0;
    roundedRectBatchWindowWidth_ = 0;
    roundedRectBatchWindowHeight_ = 0;

    if (!frameActive_ || !renderPassActive_ || roundedRectBatchPipeline_ == VK_NULL_HANDLE ||
        roundedRectBatchVertices_.buffer == VK_NULL_HANDLE ||
        !applyDrawViewportAndScissor(windowWidth, windowHeight)) {
        return;
    }

    RoundedRectBatchPushConstants constants{};
    constants.windowSize[0] = static_cast<float>(windowWidth);
    constants.windowSize[1] = static_cast<float>(windowHeight);
    const VkDeviceSize offset = static_cast<VkDeviceSize>(batchStart * sizeof(RoundedRectBatchVertex));
    VkCommandBuffer commandBuffer = currentCommandBuffer();
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, roundedRectBatchPipeline_);
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &roundedRectBatchVertices_.buffer, &offset);
    vkCmdPushConstants(commandBuffer,
                       roundedRectBatchPipelineLayout_,
                       VK_SHADER_STAGE_VERTEX_BIT,
                       0,
                       sizeof(constants),
                       &constants);
    vkCmdDraw(commandBuffer, static_cast<std::uint32_t>(batchCount), 1, 0, 0);
}

bool VulkanRenderBackend::ensureRoundedRectPipeline() {
    if (roundedRectPipeline_ != VK_NULL_HANDLE) {
        return true;
    }
    if (device_ == VK_NULL_HANDLE || renderPass_ == VK_NULL_HANDLE) {
        return false;
    }

    if (roundedRectDescriptorSetLayout_ == VK_NULL_HANDLE) {
        VkDescriptorSetLayoutBinding backdropBinding{};
        backdropBinding.binding = 0;
        backdropBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        backdropBinding.descriptorCount = 1;
        backdropBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo descriptorLayoutInfo{};
        descriptorLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorLayoutInfo.bindingCount = 1;
        descriptorLayoutInfo.pBindings = &backdropBinding;
        if (vkCreateDescriptorSetLayout(device_, &descriptorLayoutInfo, nullptr, &roundedRectDescriptorSetLayout_) != VK_SUCCESS) {
            return false;
        }
    }

    VkShaderModule vertexShader = createShaderModule(device_,
                                                     shaders::kRoundedRectVertexSpirv,
                                                     shaders::kRoundedRectVertexSpirvSize);
    VkShaderModule fragmentShader = createShaderModule(device_,
                                                       shaders::kRoundedRectFragmentSpirv,
                                                       shaders::kRoundedRectFragmentSpirvSize);
    if (vertexShader == VK_NULL_HANDLE || fragmentShader == VK_NULL_HANDLE) {
        if (vertexShader != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device_, vertexShader, nullptr);
        }
        if (fragmentShader != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device_, fragmentShader, nullptr);
        }
        return false;
    }

    VkPipelineShaderStageCreateInfo shaderStages[2]{};
    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = vertexShader;
    shaderStages[0].pName = "main";
    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = fragmentShader;
    shaderStages[1].pName = "main";

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(PrimitiveGeometryVertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 2> attributes{};
    attributes[0].binding = 0;
    attributes[0].location = 0;
    attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[0].offset = offsetof(PrimitiveGeometryVertex, screen);
    attributes[1].binding = 0;
    attributes[1].location = 1;
    attributes[1].format = VK_FORMAT_R32G32_SFLOAT;
    attributes[1].offset = offsetof(PrimitiveGeometryVertex, local);

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size());
    vertexInput.pVertexAttributeDescriptions = attributes.data();

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

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                          VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT |
                                          VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    std::array<VkDynamicState, 2> dynamicStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPushConstantRange pushConstant{};
    pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstant.offset = 0;
    pushConstant.size = sizeof(RoundedRectPushConstants);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &roundedRectDescriptorSetLayout_;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstant;
    if (vkCreatePipelineLayout(device_, &pipelineLayoutInfo, nullptr, &roundedRectPipelineLayout_) != VK_SUCCESS) {
        vkDestroyShaderModule(device_, fragmentShader, nullptr);
        vkDestroyShaderModule(device_, vertexShader, nullptr);
        return false;
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = roundedRectPipelineLayout_;
    pipelineInfo.renderPass = renderPass_;
    pipelineInfo.subpass = 0;

    const bool created = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &roundedRectPipeline_) == VK_SUCCESS;
    vkDestroyShaderModule(device_, fragmentShader, nullptr);
    vkDestroyShaderModule(device_, vertexShader, nullptr);
    if (!created) {
        destroyRoundedRectPipeline();
    }
    return created;
}

bool VulkanRenderBackend::ensureRoundedRectBatchPipeline() {
    if (roundedRectBatchPipeline_ != VK_NULL_HANDLE) {
        return true;
    }
    if (device_ == VK_NULL_HANDLE || renderPass_ == VK_NULL_HANDLE) {
        return false;
    }

    VkShaderModule vertexShader = createShaderModule(device_,
                                                     shaders::kRoundedRectBatchVertexSpirv,
                                                     shaders::kRoundedRectBatchVertexSpirvSize);
    VkShaderModule fragmentShader = createShaderModule(device_,
                                                       shaders::kRoundedRectBatchFragmentSpirv,
                                                       shaders::kRoundedRectBatchFragmentSpirvSize);
    if (vertexShader == VK_NULL_HANDLE || fragmentShader == VK_NULL_HANDLE) {
        if (vertexShader != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device_, vertexShader, nullptr);
        }
        if (fragmentShader != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device_, fragmentShader, nullptr);
        }
        return false;
    }

    VkPipelineShaderStageCreateInfo shaderStages[2]{};
    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = vertexShader;
    shaderStages[0].pName = "main";
    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = fragmentShader;
    shaderStages[1].pName = "main";

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(RoundedRectBatchVertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 10> attributes{};
    const auto attribute = [&](std::size_t index, std::uint32_t location, VkFormat format, std::size_t offset) {
        attributes[index].binding = 0;
        attributes[index].location = location;
        attributes[index].format = format;
        attributes[index].offset = static_cast<std::uint32_t>(offset);
    };
    attribute(0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(RoundedRectBatchVertex, screen));
    attribute(1, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(RoundedRectBatchVertex, local));
    attribute(2, 2, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(RoundedRectBatchVertex, fillColor));
    attribute(3, 3, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(RoundedRectBatchVertex, gradientStart));
    attribute(4, 4, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(RoundedRectBatchVertex, gradientEnd));
    attribute(5, 5, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(RoundedRectBatchVertex, borderColor));
    attribute(6, 6, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(RoundedRectBatchVertex, rect));
    attribute(7, 7, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(RoundedRectBatchVertex, shapeParams));
    attribute(8, 8, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(RoundedRectBatchVertex, modeParams));
    attribute(9, 9, VK_FORMAT_R32G32B32_SFLOAT, offsetof(RoundedRectBatchVertex, shadowParams));

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size());
    vertexInput.pVertexAttributeDescriptions = attributes.data();

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

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                          VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT |
                                          VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    std::array<VkDynamicState, 2> dynamicStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPushConstantRange pushConstant{};
    pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstant.size = sizeof(RoundedRectBatchPushConstants);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstant;
    if (vkCreatePipelineLayout(device_, &pipelineLayoutInfo, nullptr, &roundedRectBatchPipelineLayout_) != VK_SUCCESS) {
        vkDestroyShaderModule(device_, fragmentShader, nullptr);
        vkDestroyShaderModule(device_, vertexShader, nullptr);
        return false;
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = roundedRectBatchPipelineLayout_;
    pipelineInfo.renderPass = renderPass_;
    pipelineInfo.subpass = 0;

    const bool created = vkCreateGraphicsPipelines(device_,
                                                   VK_NULL_HANDLE,
                                                   1,
                                                   &pipelineInfo,
                                                   nullptr,
                                                   &roundedRectBatchPipeline_) == VK_SUCCESS;
    vkDestroyShaderModule(device_, fragmentShader, nullptr);
    vkDestroyShaderModule(device_, vertexShader, nullptr);
    if (!created) {
        destroyRoundedRectBatchResources();
    }
    return created;
}

bool VulkanRenderBackend::ensureBackdropResources(std::uint32_t width, std::uint32_t height) {
    if (device_ == VK_NULL_HANDLE || width == 0 || height == 0 || swapchainFormat_ == VK_FORMAT_UNDEFINED ||
        roundedRectDescriptorSetLayout_ == VK_NULL_HANDLE) {
        return false;
    }

    const VkExtent2D targetExtent{width, height};
    const bool placeholderRequest = targetExtent.width == 1 && targetExtent.height == 1;
    if (backdropImage_ != VK_NULL_HANDLE &&
        backdropImageView_ != VK_NULL_HANDLE &&
        backdropSampler_ != VK_NULL_HANDLE &&
        ((placeholderRequest && backdropExtent_.width >= 1 && backdropExtent_.height >= 1) ||
         (backdropExtent_.width == targetExtent.width && backdropExtent_.height == targetExtent.height))) {
        return ensureBackdropDescriptor();
    }

    destroyBackdropResources();

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {targetExtent.width, targetExtent.height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = swapchainFormat_;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateImage(device_, &imageInfo, nullptr, &backdropImage_) != VK_SUCCESS) {
        destroyBackdropResources();
        return false;
    }

    VkMemoryRequirements memoryRequirements{};
    vkGetImageMemoryRequirements(device_, backdropImage_, &memoryRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memoryRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (allocInfo.memoryTypeIndex == std::numeric_limits<std::uint32_t>::max() ||
        vkAllocateMemory(device_, &allocInfo, nullptr, &backdropImageMemory_) != VK_SUCCESS ||
        vkBindImageMemory(device_, backdropImage_, backdropImageMemory_, 0) != VK_SUCCESS) {
        destroyBackdropResources();
        return false;
    }

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = backdropImage_;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = swapchainFormat_;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    if (vkCreateImageView(device_, &viewInfo, nullptr, &backdropImageView_) != VK_SUCCESS) {
        destroyBackdropResources();
        return false;
    }

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.maxLod = 1.0f;
    if (vkCreateSampler(device_, &samplerInfo, nullptr, &backdropSampler_) != VK_SUCCESS) {
        destroyBackdropResources();
        return false;
    }

    backdropExtent_ = targetExtent;
    backdropImageLayout_ = VK_IMAGE_LAYOUT_UNDEFINED;
    return ensureBackdropDescriptor();
}

bool VulkanRenderBackend::ensureBackdropDescriptor() {
    if (device_ == VK_NULL_HANDLE || roundedRectDescriptorSetLayout_ == VK_NULL_HANDLE ||
        backdropImageView_ == VK_NULL_HANDLE || backdropSampler_ == VK_NULL_HANDLE) {
        return false;
    }
    if (roundedRectDescriptorSet_ != VK_NULL_HANDLE) {
        return true;
    }

    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.maxSets = 1;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    if (vkCreateDescriptorPool(device_, &poolInfo, nullptr, &roundedRectDescriptorPool_) != VK_SUCCESS) {
        return false;
    }

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = roundedRectDescriptorPool_;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &roundedRectDescriptorSetLayout_;
    if (vkAllocateDescriptorSets(device_, &allocInfo, &roundedRectDescriptorSet_) != VK_SUCCESS) {
        destroyBackdropDescriptorPool();
        return false;
    }

    VkDescriptorImageInfo imageInfo{};
    imageInfo.sampler = backdropSampler_;
    imageInfo.imageView = backdropImageView_;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = roundedRectDescriptorSet_;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.pImageInfo = &imageInfo;
    vkUpdateDescriptorSets(device_, 1, &descriptorWrite, 0, nullptr);
    return true;
}

void VulkanRenderBackend::initializeBackdropImageIfNeeded() {
    if (backdropImage_ == VK_NULL_HANDLE || backdropImageLayout_ != VK_IMAGE_LAYOUT_UNDEFINED ||
        commandBuffers_.empty() || currentImage_ >= commandBuffers_.size()) {
        return;
    }

    VkCommandBuffer commandBuffer = currentCommandBuffer();
    transitionImageLayout(commandBuffer, backdropImage_, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    backdropImageLayout_ = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    VkClearColorValue clearColor{};
    VkImageSubresourceRange range{};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.levelCount = 1;
    range.layerCount = 1;
    vkCmdClearColorImage(commandBuffer, backdropImage_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);

    transitionImageLayout(commandBuffer, backdropImage_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    backdropImageLayout_ = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

bool VulkanRenderBackend::ensurePrimitiveVertexBuffer(std::size_t vertexCount) {
    if (vertexCount == 0 || vertexCount > kPrimitiveVertexCapacity) {
        return false;
    }
    if (primitiveVertices_.buffer == VK_NULL_HANDLE) {
        const VkDeviceSize size = static_cast<VkDeviceSize>(kPrimitiveVertexCapacity * sizeof(PrimitiveGeometryVertex));
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(device_, &bufferInfo, nullptr, &primitiveVertices_.buffer) != VK_SUCCESS) {
            return false;
        }

        VkMemoryRequirements memoryRequirements{};
        vkGetBufferMemoryRequirements(device_, primitiveVertices_.buffer, &memoryRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memoryRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memoryRequirements.memoryTypeBits,
                                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (allocInfo.memoryTypeIndex == std::numeric_limits<std::uint32_t>::max() ||
            vkAllocateMemory(device_, &allocInfo, nullptr, &primitiveVertices_.memory) != VK_SUCCESS ||
            vkBindBufferMemory(device_, primitiveVertices_.buffer, primitiveVertices_.memory, 0) != VK_SUCCESS ||
            vkMapMemory(device_, primitiveVertices_.memory, 0, size, 0, &primitiveVertices_.mapped) != VK_SUCCESS) {
            destroyPrimitiveVertexBuffer();
            return false;
        }
        primitiveVertices_.capacity = kPrimitiveVertexCapacity;
    }
    return primitiveVertices_.mapped != nullptr && primitiveVertices_.used + vertexCount <= primitiveVertices_.capacity;
}

bool VulkanRenderBackend::ensureRoundedRectBatchVertexBuffer(std::size_t vertexCount) {
    if (vertexCount == 0 || vertexCount > kRoundedRectBatchVertexCapacity) {
        return false;
    }
    if (roundedRectBatchVertices_.buffer == VK_NULL_HANDLE) {
        const VkDeviceSize size = static_cast<VkDeviceSize>(kRoundedRectBatchVertexCapacity *
                                                            sizeof(RoundedRectBatchVertex));
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(device_, &bufferInfo, nullptr, &roundedRectBatchVertices_.buffer) != VK_SUCCESS) {
            return false;
        }

        VkMemoryRequirements memoryRequirements{};
        vkGetBufferMemoryRequirements(device_, roundedRectBatchVertices_.buffer, &memoryRequirements);
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memoryRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memoryRequirements.memoryTypeBits,
                                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (allocInfo.memoryTypeIndex == std::numeric_limits<std::uint32_t>::max() ||
            vkAllocateMemory(device_, &allocInfo, nullptr, &roundedRectBatchVertices_.memory) != VK_SUCCESS ||
            vkBindBufferMemory(device_, roundedRectBatchVertices_.buffer, roundedRectBatchVertices_.memory, 0) != VK_SUCCESS ||
            vkMapMemory(device_, roundedRectBatchVertices_.memory, 0, size, 0, &roundedRectBatchVertices_.mapped) != VK_SUCCESS) {
            destroyRoundedRectBatchResources();
            return false;
        }
        roundedRectBatchVertices_.capacity = kRoundedRectBatchVertexCapacity;
    }
    return roundedRectBatchVertices_.mapped != nullptr &&
           roundedRectBatchVertices_.used + vertexCount <= roundedRectBatchVertices_.capacity;
}

void VulkanRenderBackend::destroyRoundedRectPipeline() {
    destroyRoundedRectBatchResources();
    if (roundedRectPipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, roundedRectPipeline_, nullptr);
        roundedRectPipeline_ = VK_NULL_HANDLE;
    }
    if (roundedRectPipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, roundedRectPipelineLayout_, nullptr);
        roundedRectPipelineLayout_ = VK_NULL_HANDLE;
    }
    destroyBackdropDescriptorPool();
    if (roundedRectDescriptorSetLayout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_, roundedRectDescriptorSetLayout_, nullptr);
        roundedRectDescriptorSetLayout_ = VK_NULL_HANDLE;
    }
}

void VulkanRenderBackend::destroyRoundedRectBatchResources() {
    roundedRectBatchStart_ = 0;
    roundedRectBatchCount_ = 0;
    roundedRectBatchWindowWidth_ = 0;
    roundedRectBatchWindowHeight_ = 0;
    if (device_ == VK_NULL_HANDLE) {
        roundedRectBatchPipeline_ = VK_NULL_HANDLE;
        roundedRectBatchPipelineLayout_ = VK_NULL_HANDLE;
        roundedRectBatchVertices_ = {};
        return;
    }
    if (roundedRectBatchPipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, roundedRectBatchPipeline_, nullptr);
        roundedRectBatchPipeline_ = VK_NULL_HANDLE;
    }
    if (roundedRectBatchPipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, roundedRectBatchPipelineLayout_, nullptr);
        roundedRectBatchPipelineLayout_ = VK_NULL_HANDLE;
    }
    if (roundedRectBatchVertices_.mapped != nullptr && roundedRectBatchVertices_.memory != VK_NULL_HANDLE) {
        vkUnmapMemory(device_, roundedRectBatchVertices_.memory);
        roundedRectBatchVertices_.mapped = nullptr;
    }
    if (roundedRectBatchVertices_.buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, roundedRectBatchVertices_.buffer, nullptr);
    }
    if (roundedRectBatchVertices_.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device_, roundedRectBatchVertices_.memory, nullptr);
    }
    roundedRectBatchVertices_ = {};
}

void VulkanRenderBackend::destroyBackdropResources() {
    destroyBackdropDescriptorPool();
    if (device_ == VK_NULL_HANDLE) {
        backdropImage_ = VK_NULL_HANDLE;
        backdropImageMemory_ = VK_NULL_HANDLE;
        backdropImageView_ = VK_NULL_HANDLE;
        backdropSampler_ = VK_NULL_HANDLE;
        backdropImageLayout_ = VK_IMAGE_LAYOUT_UNDEFINED;
        backdropExtent_ = {};
        backdropReady_ = false;
        return;
    }
    if (backdropSampler_ != VK_NULL_HANDLE) {
        vkDestroySampler(device_, backdropSampler_, nullptr);
        backdropSampler_ = VK_NULL_HANDLE;
    }
    if (backdropImageView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, backdropImageView_, nullptr);
        backdropImageView_ = VK_NULL_HANDLE;
    }
    if (backdropImage_ != VK_NULL_HANDLE) {
        vkDestroyImage(device_, backdropImage_, nullptr);
        backdropImage_ = VK_NULL_HANDLE;
    }
    if (backdropImageMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(device_, backdropImageMemory_, nullptr);
        backdropImageMemory_ = VK_NULL_HANDLE;
    }
    backdropImageLayout_ = VK_IMAGE_LAYOUT_UNDEFINED;
    backdropExtent_ = {};
    backdropReady_ = false;
}

void VulkanRenderBackend::destroyBackdropDescriptorPool() {
    if (device_ != VK_NULL_HANDLE && roundedRectDescriptorPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_, roundedRectDescriptorPool_, nullptr);
    }
    roundedRectDescriptorPool_ = VK_NULL_HANDLE;
    roundedRectDescriptorSet_ = VK_NULL_HANDLE;
}

void VulkanRenderBackend::destroyPrimitiveVertexBuffer() {
    if (device_ == VK_NULL_HANDLE) {
        primitiveVertices_.buffer = VK_NULL_HANDLE;
        primitiveVertices_.memory = VK_NULL_HANDLE;
        primitiveVertices_.mapped = nullptr;
        primitiveVertices_.capacity = 0;
        primitiveVertices_.used = 0;
        return;
    }
    if (primitiveVertices_.memory != VK_NULL_HANDLE && primitiveVertices_.mapped != nullptr) {
        vkUnmapMemory(device_, primitiveVertices_.memory);
        primitiveVertices_.mapped = nullptr;
    }
    if (primitiveVertices_.buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, primitiveVertices_.buffer, nullptr);
        primitiveVertices_.buffer = VK_NULL_HANDLE;
    }
    if (primitiveVertices_.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device_, primitiveVertices_.memory, nullptr);
        primitiveVertices_.memory = VK_NULL_HANDLE;
    }
    primitiveVertices_.capacity = 0;
    primitiveVertices_.used = 0;
}

} // namespace core::render::vulkan
