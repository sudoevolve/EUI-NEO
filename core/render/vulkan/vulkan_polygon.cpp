#include "core/render/vulkan/vulkan_backend.h"

#include "core/render/vulkan/vulkan_polygon_shaders.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace core::render::vulkan {

namespace {

constexpr std::size_t kMaxPolygonEdges = 128;
constexpr std::size_t kPolygonEdgeCapacity = 4096;

struct PolygonEdgeGpu {
    float data[4] = {};
};

struct PolygonPushConstants {
    float windowAndOpacity[4] = {};
    float fillColor[4] = {};
    float edgeRange[4] = {};
};

static_assert(sizeof(PolygonPushConstants) == 48, "Polygon push constants must fit Vulkan minimum size.");

} // namespace

void VulkanRenderBackend::drawPolygon(const PolygonDrawCommand& command, int windowWidth, int windowHeight) {
    if (!frameActive_ || windowWidth <= 0 || windowHeight <= 0 || command.vertices.empty() ||
        command.edges.size() < 3 || command.opacity <= 0.001f || command.fillColor.a <= 0.001f) {
        return;
    }
    flushRoundedRectBatch();
    if (!ensurePolygonPipeline() ||
        !ensurePrimitiveVertexBuffer(command.vertices.size()) ||
        !ensurePolygonEdgeBuffer(std::min(command.edges.size(), kMaxPolygonEdges))) {
        return;
    }
    if (!frameRecorded_) {
        recordClearPass(clearColor_);
    }
    if (!renderPassActive_) {
        return;
    }

    const std::size_t vertexOffset = primitiveVertices_.used;
    auto* mappedVertices = static_cast<PrimitiveGeometryVertex*>(primitiveVertices_.mapped);
    std::copy(command.vertices.begin(), command.vertices.end(), mappedVertices + vertexOffset);
    primitiveVertices_.used += command.vertices.size();

    const std::size_t edgeCount = std::min(command.edges.size(), kMaxPolygonEdges);
    const std::size_t edgeOffset = polygonEdges_.used;
    auto* mappedEdges = static_cast<PolygonEdgeGpu*>(polygonEdges_.mapped);
    for (std::size_t index = 0; index < edgeCount; ++index) {
        const PolygonEdgeData& edge = command.edges[index];
        mappedEdges[edgeOffset + index].data[0] = edge.from.x;
        mappedEdges[edgeOffset + index].data[1] = edge.from.y;
        mappedEdges[edgeOffset + index].data[2] = edge.to.x;
        mappedEdges[edgeOffset + index].data[3] = edge.to.y;
    }
    polygonEdges_.used += edgeCount;

    VkCommandBuffer commandBuffer = currentCommandBuffer();
    if (!applyDrawViewportAndScissor(windowWidth, windowHeight)) {
        return;
    }

    PolygonPushConstants constants{};
    constants.windowAndOpacity[0] = static_cast<float>(windowWidth);
    constants.windowAndOpacity[1] = static_cast<float>(windowHeight);
    constants.windowAndOpacity[2] = command.opacity;
    constants.windowAndOpacity[3] = 0.0f;
    writeColor(constants.fillColor, command.fillColor);
    constants.edgeRange[0] = static_cast<float>(edgeOffset);
    constants.edgeRange[1] = static_cast<float>(edgeCount);

    const VkDeviceSize vertexBufferOffset = static_cast<VkDeviceSize>(vertexOffset * sizeof(PrimitiveGeometryVertex));
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, polygonPipeline_);
    vkCmdBindDescriptorSets(commandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            polygonPipelineLayout_,
                            0,
                            1,
                            &polygonDescriptorSet_,
                            0,
                            nullptr);
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &primitiveVertices_.buffer, &vertexBufferOffset);
    vkCmdPushConstants(commandBuffer,
                       polygonPipelineLayout_,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0,
                       sizeof(constants),
                       &constants);
    vkCmdDraw(commandBuffer, static_cast<std::uint32_t>(command.vertices.size()), 1, 0, 0);
}

bool VulkanRenderBackend::ensurePolygonPipeline() {
    if (polygonPipeline_ != VK_NULL_HANDLE) {
        return true;
    }
    if (device_ == VK_NULL_HANDLE || renderPass_ == VK_NULL_HANDLE) {
        return false;
    }

    if (polygonDescriptorSetLayout_ == VK_NULL_HANDLE) {
        VkDescriptorSetLayoutBinding edgeBinding{};
        edgeBinding.binding = 0;
        edgeBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        edgeBinding.descriptorCount = 1;
        edgeBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo descriptorLayoutInfo{};
        descriptorLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorLayoutInfo.bindingCount = 1;
        descriptorLayoutInfo.pBindings = &edgeBinding;
        if (vkCreateDescriptorSetLayout(device_, &descriptorLayoutInfo, nullptr, &polygonDescriptorSetLayout_) != VK_SUCCESS) {
            return false;
        }
    }

    VkShaderModule vertexShader = createShaderModule(device_,
                                                     shaders::kPolygonVertexSpirv,
                                                     shaders::kPolygonVertexSpirvSize);
    VkShaderModule fragmentShader = createShaderModule(device_,
                                                       shaders::kPolygonFragmentSpirv,
                                                       shaders::kPolygonFragmentSpirvSize);
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
    pushConstant.size = sizeof(PolygonPushConstants);

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &polygonDescriptorSetLayout_;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstant;
    if (vkCreatePipelineLayout(device_, &pipelineLayoutInfo, nullptr, &polygonPipelineLayout_) != VK_SUCCESS) {
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
    pipelineInfo.layout = polygonPipelineLayout_;
    pipelineInfo.renderPass = renderPass_;
    pipelineInfo.subpass = 0;

    const bool created = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &polygonPipeline_) == VK_SUCCESS;
    vkDestroyShaderModule(device_, fragmentShader, nullptr);
    vkDestroyShaderModule(device_, vertexShader, nullptr);
    if (!created) {
        destroyPolygonPipeline();
    }
    return created;
}

bool VulkanRenderBackend::ensurePolygonEdgeBuffer(std::size_t edgeCount) {
    if (edgeCount == 0 || edgeCount > kMaxPolygonEdges || device_ == VK_NULL_HANDLE) {
        return false;
    }
    if (polygonEdges_.buffer == VK_NULL_HANDLE) {
        const VkDeviceSize size = static_cast<VkDeviceSize>(kPolygonEdgeCapacity * sizeof(PolygonEdgeGpu));
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(device_, &bufferInfo, nullptr, &polygonEdges_.buffer) != VK_SUCCESS) {
            return false;
        }

        VkMemoryRequirements memoryRequirements{};
        vkGetBufferMemoryRequirements(device_, polygonEdges_.buffer, &memoryRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memoryRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memoryRequirements.memoryTypeBits,
                                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (allocInfo.memoryTypeIndex == std::numeric_limits<std::uint32_t>::max() ||
            vkAllocateMemory(device_, &allocInfo, nullptr, &polygonEdges_.memory) != VK_SUCCESS ||
            vkBindBufferMemory(device_, polygonEdges_.buffer, polygonEdges_.memory, 0) != VK_SUCCESS ||
            vkMapMemory(device_, polygonEdges_.memory, 0, size, 0, &polygonEdges_.mapped) != VK_SUCCESS) {
            destroyPolygonEdgeBuffer();
            return false;
        }
        polygonEdges_.capacity = kPolygonEdgeCapacity;
    }

    if (polygonDescriptorPool_ == VK_NULL_HANDLE) {
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSize.descriptorCount = 1;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.maxSets = 1;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        if (vkCreateDescriptorPool(device_, &poolInfo, nullptr, &polygonDescriptorPool_) != VK_SUCCESS) {
            return false;
        }

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = polygonDescriptorPool_;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &polygonDescriptorSetLayout_;
        if (vkAllocateDescriptorSets(device_, &allocInfo, &polygonDescriptorSet_) != VK_SUCCESS) {
            vkDestroyDescriptorPool(device_, polygonDescriptorPool_, nullptr);
            polygonDescriptorPool_ = VK_NULL_HANDLE;
            polygonDescriptorSet_ = VK_NULL_HANDLE;
            return false;
        }

        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = polygonEdges_.buffer;
        bufferInfo.offset = 0;
        bufferInfo.range = VK_WHOLE_SIZE;

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = polygonDescriptorSet_;
        descriptorWrite.dstBinding = 0;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        descriptorWrite.pBufferInfo = &bufferInfo;
        vkUpdateDescriptorSets(device_, 1, &descriptorWrite, 0, nullptr);
    }

    return polygonEdges_.mapped != nullptr &&
           polygonDescriptorSet_ != VK_NULL_HANDLE &&
           polygonEdges_.used + edgeCount <= polygonEdges_.capacity;
}

void VulkanRenderBackend::destroyPolygonPipeline() {
    if (polygonPipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, polygonPipeline_, nullptr);
        polygonPipeline_ = VK_NULL_HANDLE;
    }
    if (polygonPipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, polygonPipelineLayout_, nullptr);
        polygonPipelineLayout_ = VK_NULL_HANDLE;
    }
    if (polygonDescriptorPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device_, polygonDescriptorPool_, nullptr);
        polygonDescriptorPool_ = VK_NULL_HANDLE;
        polygonDescriptorSet_ = VK_NULL_HANDLE;
    }
    if (polygonDescriptorSetLayout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_, polygonDescriptorSetLayout_, nullptr);
        polygonDescriptorSetLayout_ = VK_NULL_HANDLE;
    }
}

void VulkanRenderBackend::destroyPolygonEdgeBuffer() {
    if (device_ == VK_NULL_HANDLE) {
        polygonEdges_ = {};
        return;
    }
    if (polygonEdges_.memory != VK_NULL_HANDLE && polygonEdges_.mapped != nullptr) {
        vkUnmapMemory(device_, polygonEdges_.memory);
        polygonEdges_.mapped = nullptr;
    }
    if (polygonEdges_.buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, polygonEdges_.buffer, nullptr);
        polygonEdges_.buffer = VK_NULL_HANDLE;
    }
    if (polygonEdges_.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device_, polygonEdges_.memory, nullptr);
        polygonEdges_.memory = VK_NULL_HANDLE;
    }
    polygonEdges_.capacity = 0;
    polygonEdges_.used = 0;
}

} // namespace core::render::vulkan
