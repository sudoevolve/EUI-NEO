#include "core/render/vulkan/vulkan_backend.h"

#include "core/render/vulkan/vulkan_text_shaders.h"

#include <array>
#include <cstring>
#include <limits>

namespace core::render::vulkan {

namespace {

constexpr std::size_t kTextVertexFloatCapacity = 262144;

struct TextPushConstants {
    float windowSize[4] = {};
    float color[4] = {};
};

} // namespace

void VulkanRenderBackend::drawText(const TextDrawCommand& command, int windowWidth, int windowHeight) {
    if (!frameActive_ || command.vertices == nullptr || command.vertexFloatCount == 0 ||
        windowWidth <= 0 || windowHeight <= 0 || command.color.a <= 0.001f) {
        return;
    }

    flushRoundedRectBatch();
    if (!ensureTextPipeline()) {
        return;
    }
    if (!frameRecorded_) {
        recordClearPass(clearColor_);
    }

    const bool uploadNeeded = textAtlasNeedsUpload(command.grayAtlas) || textAtlasNeedsUpload(command.colorAtlas);
    if (uploadNeeded && renderPassActive_) {
        endActiveRenderPass();
    }

    if (!ensureTextAtlas(command.grayAtlas)) {
        if (!renderPassActive_) {
            beginLoadPass();
        }
        return;
    }
    if (command.colorAtlas.pixels != nullptr && command.colorAtlas.width > 0 && command.colorAtlas.height > 0) {
        ensureTextAtlas(command.colorAtlas);
    }
    if (uploadNeeded && !renderPassActive_) {
        beginLoadPass();
    }
    if (!renderPassActive_ || !ensureTextDescriptor() || !ensureTextVertexBuffer(command.vertexFloatCount)) {
        return;
    }

    const std::size_t floatOffset = textVertices_.used;
    auto* mappedTextVertices = static_cast<float*>(textVertices_.mapped);
    std::memcpy(mappedTextVertices + floatOffset, command.vertices, command.vertexFloatCount * sizeof(float));
    textVertices_.used += command.vertexFloatCount;

    VkCommandBuffer commandBuffer = currentCommandBuffer();
    if (!applyDrawViewportAndScissor(windowWidth, windowHeight)) {
        return;
    }

    TextPushConstants constants{};
    constants.windowSize[0] = static_cast<float>(windowWidth);
    constants.windowSize[1] = static_cast<float>(windowHeight);
    writeColor(constants.color, command.color);

    const VkDeviceSize offset = static_cast<VkDeviceSize>(floatOffset * sizeof(float));
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, textPipeline_);
    vkCmdBindDescriptorSets(commandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            textPipelineLayout_,
                            0,
                            1,
                            &textDescriptorSet_,
                            0,
                            nullptr);
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &textVertices_.buffer, &offset);
    vkCmdPushConstants(commandBuffer,
                       textPipelineLayout_,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0,
                       sizeof(constants),
                       &constants);
    vkCmdDraw(commandBuffer, static_cast<std::uint32_t>(command.vertexFloatCount / 5), 1, 0, 0);
}

bool VulkanRenderBackend::ensureTextPipeline() {
    if (textPipeline_ != VK_NULL_HANDLE) {
        return true;
    }
    if (device_ == VK_NULL_HANDLE || renderPass_ == VK_NULL_HANDLE) {
        return false;
    }

    if (textDescriptorSetLayout_ == VK_NULL_HANDLE) {
        std::array<VkDescriptorSetLayoutBinding, 2> bindings{};
        for (std::uint32_t i = 0; i < bindings.size(); ++i) {
            bindings[i].binding = i;
            bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            bindings[i].descriptorCount = 1;
            bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        }

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();
        if (vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr, &textDescriptorSetLayout_) != VK_SUCCESS) {
            return false;
        }
    }

    VkShaderModule vertexShader = createShaderModule(device_, shaders::kTextVertexSpirv, shaders::kTextVertexSpirvSize);
    VkShaderModule fragmentShader = createShaderModule(device_, shaders::kTextFragmentSpirv, shaders::kTextFragmentSpirvSize);
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
    binding.stride = sizeof(float) * 5;
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 3> attributes{};
    attributes[0].binding = 0;
    attributes[0].location = 0;
    attributes[0].format = VK_FORMAT_R32G32_SFLOAT;
    attributes[0].offset = 0;
    attributes[1].binding = 0;
    attributes[1].location = 1;
    attributes[1].format = VK_FORMAT_R32G32_SFLOAT;
    attributes[1].offset = sizeof(float) * 2;
    attributes[2].binding = 0;
    attributes[2].location = 2;
    attributes[2].format = VK_FORMAT_R32_SFLOAT;
    attributes[2].offset = sizeof(float) * 4;

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
    pushConstant.size = sizeof(TextPushConstants);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &textDescriptorSetLayout_;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstant;
    if (vkCreatePipelineLayout(device_, &pipelineLayoutInfo, nullptr, &textPipelineLayout_) != VK_SUCCESS) {
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
    pipelineInfo.layout = textPipelineLayout_;
    pipelineInfo.renderPass = renderPass_;
    pipelineInfo.subpass = 0;

    const bool created = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &textPipeline_) == VK_SUCCESS;
    vkDestroyShaderModule(device_, fragmentShader, nullptr);
    vkDestroyShaderModule(device_, vertexShader, nullptr);
    if (!created) {
        destroyTextPipeline();
    }
    return created;
}

bool VulkanRenderBackend::textAtlasNeedsUpload(const TextAtlasPageData& page) const {
    if (page.pixels == nullptr || page.width <= 0 || page.height <= 0 || page.channels <= 0) {
        return false;
    }
    const TextureResource& texture = page.kind == TextAtlasPageKind::Color ? textColorAtlas_ : textGrayAtlas_;
    return texture.image == VK_NULL_HANDLE ||
           texture.width != page.width ||
           texture.height != page.height ||
           texture.channels != page.channels ||
           texture.generation != page.generation;
}

bool VulkanRenderBackend::ensureTextAtlas(const TextAtlasPageData& page) {
    if (page.pixels == nullptr || page.width <= 0 || page.height <= 0 || page.channels <= 0 ||
        commandBuffers_.empty() || currentImage_ >= commandBuffers_.size()) {
        return false;
    }
    TextureResource& texture = page.kind == TextAtlasPageKind::Color ? textColorAtlas_ : textGrayAtlas_;
    if (!textAtlasNeedsUpload(page)) {
        return true;
    }

    const VkFormat format = page.channels == 4 ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8_UNORM;
    if (texture.image == VK_NULL_HANDLE ||
        texture.width != page.width ||
        texture.height != page.height ||
        texture.channels != page.channels) {
        destroyTextureResource(texture);

        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent = {static_cast<std::uint32_t>(page.width), static_cast<std::uint32_t>(page.height), 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateImage(device_, &imageInfo, nullptr, &texture.image) != VK_SUCCESS) {
            destroyTextureResource(texture);
            return false;
        }

        VkMemoryRequirements memoryRequirements{};
        vkGetImageMemoryRequirements(device_, texture.image, &memoryRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memoryRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (allocInfo.memoryTypeIndex == std::numeric_limits<std::uint32_t>::max() ||
            vkAllocateMemory(device_, &allocInfo, nullptr, &texture.memory) != VK_SUCCESS ||
            vkBindImageMemory(device_, texture.image, texture.memory, 0) != VK_SUCCESS) {
            destroyTextureResource(texture);
            return false;
        }

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = texture.image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;
        if (vkCreateImageView(device_, &viewInfo, nullptr, &texture.view) != VK_SUCCESS) {
            destroyTextureResource(texture);
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
        if (vkCreateSampler(device_, &samplerInfo, nullptr, &texture.sampler) != VK_SUCCESS) {
            destroyTextureResource(texture);
            return false;
        }

        texture.width = page.width;
        texture.height = page.height;
        texture.channels = page.channels;
        texture.layout = VK_IMAGE_LAYOUT_UNDEFINED;
        textDescriptorDirty_ = true;
    }

    const VkDeviceSize uploadSize = static_cast<VkDeviceSize>(page.width) *
                                    static_cast<VkDeviceSize>(page.height) *
                                    static_cast<VkDeviceSize>(page.channels);
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceSize stagingOffset = 0;
    void* mapped = nullptr;
    if (!allocateUploadRegion(uploadSize, stagingBuffer, stagingOffset, mapped)) {
        return false;
    }
    std::memcpy(mapped, page.pixels, static_cast<std::size_t>(uploadSize));

    VkCommandBuffer commandBuffer = currentCommandBuffer();
    transitionImageLayout(commandBuffer, texture.image, texture.layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    texture.layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    VkBufferImageCopy copyRegion{};
    copyRegion.bufferOffset = stagingOffset;
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageExtent = {static_cast<std::uint32_t>(page.width), static_cast<std::uint32_t>(page.height), 1};
    vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    transitionImageLayout(commandBuffer, texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    texture.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    texture.generation = page.generation;
    textDescriptorDirty_ = true;
    return true;
}

bool VulkanRenderBackend::ensureTextDescriptor() {
    if (device_ == VK_NULL_HANDLE ||
        textDescriptorSetLayout_ == VK_NULL_HANDLE ||
        textGrayAtlas_.view == VK_NULL_HANDLE ||
        textGrayAtlas_.sampler == VK_NULL_HANDLE) {
        return false;
    }
    if (textDescriptorPool_ == VK_NULL_HANDLE) {
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = 2;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.maxSets = 1;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        if (vkCreateDescriptorPool(device_, &poolInfo, nullptr, &textDescriptorPool_) != VK_SUCCESS) {
            return false;
        }

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = textDescriptorPool_;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &textDescriptorSetLayout_;
        if (vkAllocateDescriptorSets(device_, &allocInfo, &textDescriptorSet_) != VK_SUCCESS) {
            vkDestroyDescriptorPool(device_, textDescriptorPool_, nullptr);
            textDescriptorPool_ = VK_NULL_HANDLE;
            textDescriptorSet_ = VK_NULL_HANDLE;
            return false;
        }
        textDescriptorDirty_ = true;
    }

    if (!textDescriptorDirty_) {
        return true;
    }

    const TextureResource& color = textColorAtlas_.view != VK_NULL_HANDLE ? textColorAtlas_ : textGrayAtlas_;
    std::array<VkDescriptorImageInfo, 2> images{};
    images[0].sampler = textGrayAtlas_.sampler;
    images[0].imageView = textGrayAtlas_.view;
    images[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    images[1].sampler = color.sampler;
    images[1].imageView = color.view;
    images[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    std::array<VkWriteDescriptorSet, 2> writes{};
    for (std::uint32_t i = 0; i < writes.size(); ++i) {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = textDescriptorSet_;
        writes[i].dstBinding = i;
        writes[i].descriptorCount = 1;
        writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[i].pImageInfo = &images[i];
    }
    vkUpdateDescriptorSets(device_, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
    textDescriptorDirty_ = false;
    return true;
}

bool VulkanRenderBackend::ensureTextVertexBuffer(std::size_t floatCount) {
    if (floatCount == 0 || floatCount > kTextVertexFloatCapacity) {
        return false;
    }
    if (textVertices_.buffer == VK_NULL_HANDLE) {
        const VkDeviceSize size = static_cast<VkDeviceSize>(kTextVertexFloatCapacity * sizeof(float));
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(device_, &bufferInfo, nullptr, &textVertices_.buffer) != VK_SUCCESS) {
            return false;
        }

        VkMemoryRequirements memoryRequirements{};
        vkGetBufferMemoryRequirements(device_, textVertices_.buffer, &memoryRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memoryRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memoryRequirements.memoryTypeBits,
                                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (allocInfo.memoryTypeIndex == std::numeric_limits<std::uint32_t>::max() ||
            vkAllocateMemory(device_, &allocInfo, nullptr, &textVertices_.memory) != VK_SUCCESS ||
            vkBindBufferMemory(device_, textVertices_.buffer, textVertices_.memory, 0) != VK_SUCCESS ||
            vkMapMemory(device_, textVertices_.memory, 0, size, 0, &textVertices_.mapped) != VK_SUCCESS) {
            destroyTextResources();
            return false;
        }
        textVertices_.capacity = kTextVertexFloatCapacity;
    }
    return textVertices_.mapped != nullptr && textVertices_.used + floatCount <= textVertices_.capacity;
}

void VulkanRenderBackend::destroyTextPipeline() {
    if (textPipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, textPipeline_, nullptr);
        textPipeline_ = VK_NULL_HANDLE;
    }
    if (textPipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, textPipelineLayout_, nullptr);
        textPipelineLayout_ = VK_NULL_HANDLE;
    }
    if (textDescriptorPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_, textDescriptorPool_, nullptr);
        textDescriptorPool_ = VK_NULL_HANDLE;
        textDescriptorSet_ = VK_NULL_HANDLE;
    }
    if (textDescriptorSetLayout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_, textDescriptorSetLayout_, nullptr);
        textDescriptorSetLayout_ = VK_NULL_HANDLE;
    }
    textDescriptorDirty_ = true;
}

void VulkanRenderBackend::destroyTextResources() {
    if (device_ == VK_NULL_HANDLE) {
        textVertices_.buffer = VK_NULL_HANDLE;
        textVertices_.memory = VK_NULL_HANDLE;
        textVertices_.mapped = nullptr;
        textVertices_.capacity = 0;
        textVertices_.used = 0;
        textGrayAtlas_ = {};
        textColorAtlas_ = {};
        textDescriptorDirty_ = true;
        return;
    }
    if (textVertices_.memory != VK_NULL_HANDLE && textVertices_.mapped != nullptr) {
        vkUnmapMemory(device_, textVertices_.memory);
        textVertices_.mapped = nullptr;
    }
    if (textVertices_.buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, textVertices_.buffer, nullptr);
        textVertices_.buffer = VK_NULL_HANDLE;
    }
    if (textVertices_.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device_, textVertices_.memory, nullptr);
        textVertices_.memory = VK_NULL_HANDLE;
    }
    textVertices_.capacity = 0;
    textVertices_.used = 0;
    destroyTextureResource(textGrayAtlas_);
    destroyTextureResource(textColorAtlas_);
    textDescriptorDirty_ = true;
}

} // namespace core::render::vulkan
