#include "modules/plot/vulkan_renderer.h"

#if defined(EUI_RENDER_BACKEND_VULKAN)

#include "modules/plot/vulkan_plot_shaders.h"

#include <vulkan/vulkan.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

namespace modules::plot::detail {
namespace {

void checkVk(VkResult result, const char* operation) {
    if (result != VK_SUCCESS)
        throw std::runtime_error(std::string("plot: Vulkan ") + operation + " failed with VkResult " +
                                 std::to_string(static_cast<int>(result)));
}

std::uint32_t findMemoryType(VkPhysicalDevice physicalDevice, std::uint32_t bits,
                             VkMemoryPropertyFlags required, VkMemoryPropertyFlags preferred = 0) {
    VkPhysicalDeviceMemoryProperties properties{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &properties);
    std::uint32_t fallback = UINT32_MAX;
    for (std::uint32_t index = 0; index < properties.memoryTypeCount; ++index) {
        if ((bits & (1u << index)) == 0 ||
            (properties.memoryTypes[index].propertyFlags & required) != required)
            continue;
        if ((properties.memoryTypes[index].propertyFlags & preferred) == preferred)
            return index;
        if (fallback == UINT32_MAX)
            fallback = index;
    }
    if (fallback == UINT32_MAX)
        throw std::runtime_error("plot: Vulkan has no compatible memory type");
    return fallback;
}

VkShaderModule createShader(VkDevice device, const std::uint32_t* code, std::size_t size) {
    VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    info.codeSize = size;
    info.pCode = code;
    VkShaderModule shader = VK_NULL_HANDLE;
    checkVk(vkCreateShaderModule(device, &info, nullptr, &shader), "plot shader creation");
    return shader;
}

struct PushConstants {
    float targetSize[2];
    float padding[2];
    float color[4];
};
static_assert(sizeof(PushConstants) == 32, "Plot push constants must match the Vulkan shaders.");

} // namespace

struct VulkanPlotRenderer::Resources {
    struct Frame {
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkFence fence = VK_NULL_HANDLE;
        VkBuffer vertices = VK_NULL_HANDLE;
        VkDeviceMemory verticesMemory = VK_NULL_HANDLE;
        VkDeviceSize verticesCapacity = 0;
        VkDeviceSize verticesAllocationSize = 0;
        VkMemoryPropertyFlags verticesMemoryFlags = 0;
        bool submitted = false;
    };

    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    std::uint32_t queueFamily = 0;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkCommandBuffer uploadCommandBuffer = VK_NULL_HANDLE;
    std::array<Frame, 3> frames{};
    std::size_t nextFrame = 0;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    bool supportsSubpixelPrecision = false;

    void waitForFrames() {
        for (auto& frame : frames) {
            if (!frame.submitted)
                continue;
            vkWaitForFences(device, 1, &frame.fence, VK_TRUE, UINT64_MAX);
            frame.submitted = false;
        }
    }

    ~Resources() {
        if (device == VK_NULL_HANDLE)
            return;
        vkDeviceWaitIdle(device);
        for (auto& frame : frames) {
            if (frame.vertices)
                vkDestroyBuffer(device, frame.vertices, nullptr);
            if (frame.verticesMemory)
                vkFreeMemory(device, frame.verticesMemory, nullptr);
            if (frame.fence)
                vkDestroyFence(device, frame.fence, nullptr);
        }
        if (pipeline)
            vkDestroyPipeline(device, pipeline, nullptr);
        if (pipelineLayout)
            vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        if (renderPass)
            vkDestroyRenderPass(device, renderPass, nullptr);
        if (commandPool)
            vkDestroyCommandPool(device, commandPool, nullptr);
    }
};

namespace {

template <class T> void initializeResources(T& resources) {
    VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = resources.queueFamily;
    checkVk(vkCreateCommandPool(resources.device, &poolInfo, nullptr, &resources.commandPool),
            "plot command-pool creation");
    VkCommandBufferAllocateInfo commandInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    commandInfo.commandPool = resources.commandPool;
    commandInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandInfo.commandBufferCount = static_cast<std::uint32_t>(resources.frames.size() + 1);
    std::array<VkCommandBuffer, 4> commandBuffers{};
    checkVk(vkAllocateCommandBuffers(resources.device, &commandInfo, commandBuffers.data()),
            "plot command-buffer allocation");
    resources.uploadCommandBuffer = commandBuffers[0];
    for (std::size_t i = 0; i < resources.frames.size(); ++i) {
        resources.frames[i].commandBuffer = commandBuffers[i + 1];
        VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        checkVk(vkCreateFence(resources.device, &fenceInfo, nullptr, &resources.frames[i].fence),
                "plot frame fence creation");
    }

    VkAttachmentDescription attachment{};
    attachment.format = VK_FORMAT_R8G8B8A8_UNORM;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference colorAttachment{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachment;
    VkRenderPassCreateInfo renderPassInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &attachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    checkVk(vkCreateRenderPass(resources.device, &renderPassInfo, nullptr, &resources.renderPass),
            "plot render-pass creation");

    const VkPushConstantRange pushRange{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                         0, sizeof(PushConstants)};
    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushRange;
    checkVk(vkCreatePipelineLayout(resources.device, &layoutInfo, nullptr, &resources.pipelineLayout),
            "plot pipeline-layout creation");

    const auto vertexShader = createShader(resources.device, kPlotVertexSpirv, kPlotVertexSpirvSize);
    VkShaderModule fragmentShader = VK_NULL_HANDLE;
    try {
        fragmentShader = createShader(resources.device, kPlotFragmentSpirv, kPlotFragmentSpirvSize);
        VkPipelineShaderStageCreateInfo stages[2]{};
        stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vertexShader;
        stages[0].pName = "main";
        stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = fragmentShader;
        stages[1].pName = "main";

        VkVertexInputBindingDescription binding{};
        binding.binding = 0;
        binding.stride = sizeof(Vertex);
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        VkVertexInputAttributeDescription attribute{};
        attribute.location = 0;
        attribute.binding = 0;
        attribute.format = VK_FORMAT_R32G32_SFLOAT;
        attribute.offset = 0;
        VkPipelineVertexInputStateCreateInfo vertexInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        vertexInput.vertexBindingDescriptionCount = 1;
        vertexInput.pVertexBindingDescriptions = &binding;
        vertexInput.vertexAttributeDescriptionCount = 1;
        vertexInput.pVertexAttributeDescriptions = &attribute;
        VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        viewport.viewportCount = 1;
        viewport.scissorCount = 1;
        VkPipelineRasterizationStateCreateInfo rasterizer{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.cullMode = VK_CULL_MODE_NONE;
        rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        rasterizer.lineWidth = 1.f;
        VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        VkPipelineColorBlendAttachmentState blendAttachment{};
        blendAttachment.blendEnable = VK_TRUE;
        blendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        blendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        blend.attachmentCount = 1;
        blend.pAttachments = &blendAttachment;
        const VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamic.dynamicStateCount = 2;
        dynamic.pDynamicStates = dynamicStates;
        VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = stages;
        pipelineInfo.pVertexInputState = &vertexInput;
        pipelineInfo.pInputAssemblyState = &assembly;
        pipelineInfo.pViewportState = &viewport;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisample;
        pipelineInfo.pColorBlendState = &blend;
        pipelineInfo.pDynamicState = &dynamic;
        pipelineInfo.layout = resources.pipelineLayout;
        pipelineInfo.renderPass = resources.renderPass;
        pipelineInfo.subpass = 0;
        checkVk(vkCreateGraphicsPipelines(resources.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                          &resources.pipeline), "plot graphics-pipeline creation");
    } catch (...) {
        vkDestroyShaderModule(resources.device, vertexShader, nullptr);
        if (fragmentShader)
            vkDestroyShaderModule(resources.device, fragmentShader, nullptr);
        throw;
    }
    vkDestroyShaderModule(resources.device, vertexShader, nullptr);
    vkDestroyShaderModule(resources.device, fragmentShader, nullptr);
}

} // namespace

struct VulkanPlotRenderer::Texture {
    std::shared_ptr<Resources> resources;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory imageMemory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkFramebuffer framebuffer = VK_NULL_HANDLE;
    VkBuffer staging = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    VkDeviceSize stagingCapacity = 0;
    VkDeviceSize stagingAllocationSize = 0;
    VkMemoryPropertyFlags stagingMemoryFlags = 0;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    std::uint32_t width = 0;
    std::uint32_t height = 0;

    ~Texture() {
        if (!resources || resources->device == VK_NULL_HANDLE)
            return;
        resources->waitForFrames();
        const auto device = resources->device;
        if (framebuffer)
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        if (staging)
            vkDestroyBuffer(device, staging, nullptr);
        if (stagingMemory)
            vkFreeMemory(device, stagingMemory, nullptr);
        if (view)
            vkDestroyImageView(device, view, nullptr);
        if (image)
            vkDestroyImage(device, image, nullptr);
        if (imageMemory)
            vkFreeMemory(device, imageMemory, nullptr);
    }
};

VulkanPlotRenderer::VulkanPlotRenderer(const eui::GpuDeviceInfo& device) : resources_(std::make_shared<Resources>()) {
    resources_->device = reinterpret_cast<VkDevice>(device.device);
    resources_->physicalDevice = reinterpret_cast<VkPhysicalDevice>(device.physicalDevice);
    resources_->queue = reinterpret_cast<VkQueue>(device.graphicsQueue);
    resources_->queueFamily = device.graphicsQueueFamily;
    if (resources_->device == VK_NULL_HANDLE || resources_->physicalDevice == VK_NULL_HANDLE ||
        resources_->queue == VK_NULL_HANDLE)
        throw std::runtime_error("plot: Vulkan device information is incomplete");
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(resources_->physicalDevice, &properties);
    resources_->supportsSubpixelPrecision = properties.limits.subPixelPrecisionBits >= 8;
    initializeResources(*resources_);
}

VulkanPlotRenderer::~VulkanPlotRenderer() = default;

std::shared_ptr<VulkanPlotRenderer::Texture> VulkanPlotRenderer::createTexture(std::uint32_t width,
                                                                               std::uint32_t height) {
    auto texture = std::make_shared<Texture>();
    texture->resources = resources_;
    texture->width = width;
    texture->height = height;

    VkPhysicalDeviceProperties deviceProperties{};
    vkGetPhysicalDeviceProperties(resources_->physicalDevice, &deviceProperties);
    if (width > deviceProperties.limits.maxImageDimension2D || height > deviceProperties.limits.maxImageDimension2D)
        throw std::length_error("plot: Vulkan image dimension limit exceeded");

    VkFormatProperties formatProperties{};
    vkGetPhysicalDeviceFormatProperties(resources_->physicalDevice, VK_FORMAT_R8G8B8A8_UNORM, &formatProperties);
    const VkFormatFeatureFlags required = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
                                          VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT |
                                          VK_FORMAT_FEATURE_TRANSFER_DST_BIT |
                                          VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT |
                                          VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;
    if ((formatProperties.optimalTilingFeatures & required) != required)
        throw std::runtime_error("plot: Vulkan RGBA8 format lacks sampled-image or blended color-attachment support");

    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.extent = {width, height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    checkVk(vkCreateImage(resources_->device, &imageInfo, nullptr, &texture->image), "plot image creation");

    VkMemoryRequirements imageRequirements{};
    vkGetImageMemoryRequirements(resources_->device, texture->image, &imageRequirements);
    VkMemoryAllocateInfo imageAllocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    imageAllocation.allocationSize = imageRequirements.size;
    imageAllocation.memoryTypeIndex = findMemoryType(resources_->physicalDevice, imageRequirements.memoryTypeBits,
                                                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    checkVk(vkAllocateMemory(resources_->device, &imageAllocation, nullptr, &texture->imageMemory),
            "plot image memory allocation");
    checkVk(vkBindImageMemory(resources_->device, texture->image, texture->imageMemory, 0),
            "plot image memory binding");

    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = texture->image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    checkVk(vkCreateImageView(resources_->device, &viewInfo, nullptr, &texture->view), "plot image-view creation");

    VkFramebufferCreateInfo framebufferInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    framebufferInfo.renderPass = resources_->renderPass;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = &texture->view;
    framebufferInfo.width = width;
    framebufferInfo.height = height;
    framebufferInfo.layers = 1;
    checkVk(vkCreateFramebuffer(resources_->device, &framebufferInfo, nullptr, &texture->framebuffer),
            "plot framebuffer creation");
    return texture;
}

std::uint64_t VulkanPlotRenderer::imageViewHandle(const std::shared_ptr<Texture>& texture) const {
    std::uint64_t result = 0;
    const auto view = texture->view;
    static_assert(sizeof(view) <= sizeof(result));
    std::memcpy(&result, &view, sizeof(view));
    return result;
}

bool VulkanPlotRenderer::supportsSubpixelPrecision() const noexcept {
    return resources_->supportsSubpixelPrecision;
}

void VulkanPlotRenderer::upload(const std::shared_ptr<Texture>& texture, const std::vector<std::uint8_t>& rgba) {
    const VkDeviceSize required = static_cast<VkDeviceSize>(rgba.size());
    if (texture->staging == VK_NULL_HANDLE || texture->stagingCapacity < required) {
        if (texture->staging)
            vkDestroyBuffer(resources_->device, texture->staging, nullptr);
        if (texture->stagingMemory)
            vkFreeMemory(resources_->device, texture->stagingMemory, nullptr);
        texture->staging = VK_NULL_HANDLE;
        texture->stagingMemory = VK_NULL_HANDLE;
        texture->stagingCapacity = 0;

        VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufferInfo.size = required;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        checkVk(vkCreateBuffer(resources_->device, &bufferInfo, nullptr, &texture->staging),
                "plot staging buffer creation");
        VkMemoryRequirements requirements{};
        vkGetBufferMemoryRequirements(resources_->device, texture->staging, &requirements);
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = findMemoryType(resources_->physicalDevice, requirements.memoryTypeBits,
                                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                                                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        VkPhysicalDeviceMemoryProperties properties{};
        vkGetPhysicalDeviceMemoryProperties(resources_->physicalDevice, &properties);
        texture->stagingMemoryFlags = properties.memoryTypes[allocation.memoryTypeIndex].propertyFlags;
        texture->stagingAllocationSize = requirements.size;
        checkVk(vkAllocateMemory(resources_->device, &allocation, nullptr, &texture->stagingMemory),
                "plot staging memory allocation");
        checkVk(vkBindBufferMemory(resources_->device, texture->staging, texture->stagingMemory, 0),
                "plot staging memory binding");
        texture->stagingCapacity = requirements.size;
    }

    void* mapped = nullptr;
    checkVk(vkMapMemory(resources_->device, texture->stagingMemory, 0, texture->stagingAllocationSize, 0, &mapped),
            "plot staging memory map");
    std::memcpy(mapped, rgba.data(), rgba.size());
    if ((texture->stagingMemoryFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0) {
        VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
        range.memory = texture->stagingMemory;
        range.size = VK_WHOLE_SIZE;
        const VkResult flushResult = vkFlushMappedMemoryRanges(resources_->device, 1, &range);
        vkUnmapMemory(resources_->device, texture->stagingMemory);
        checkVk(flushResult, "plot staging memory flush");
    } else {
        vkUnmapMemory(resources_->device, texture->stagingMemory);
    }

    checkVk(vkResetCommandBuffer(resources_->uploadCommandBuffer, 0), "plot upload command reset");
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    checkVk(vkBeginCommandBuffer(resources_->uploadCommandBuffer, &begin), "plot upload command begin");

    VkImageMemoryBarrier toTransfer{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toTransfer.srcAccessMask = texture->layout == VK_IMAGE_LAYOUT_UNDEFINED ? 0 : VK_ACCESS_SHADER_READ_BIT;
    toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toTransfer.oldLayout = texture->layout;
    toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.image = texture->image;
    toTransfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toTransfer.subresourceRange.levelCount = 1;
    toTransfer.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(resources_->uploadCommandBuffer,
                         texture->layout == VK_IMAGE_LAYOUT_UNDEFINED ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
                                                                      : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toTransfer);

    VkBufferImageCopy copy{};
    copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.layerCount = 1;
    copy.imageExtent = {texture->width, texture->height, 1};
    vkCmdCopyBufferToImage(resources_->uploadCommandBuffer, texture->staging, texture->image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

    VkImageMemoryBarrier toSample{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toSample.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toSample.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    toSample.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toSample.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toSample.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toSample.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toSample.image = texture->image;
    toSample.subresourceRange = toTransfer.subresourceRange;
    vkCmdPipelineBarrier(resources_->uploadCommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toSample);
    checkVk(vkEndCommandBuffer(resources_->uploadCommandBuffer), "plot upload command end");
    VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence fence = VK_NULL_HANDLE;
    checkVk(vkCreateFence(resources_->device, &fenceInfo, nullptr, &fence), "plot upload fence creation");
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &resources_->uploadCommandBuffer;
    const VkResult submitResult = vkQueueSubmit(resources_->queue, 1, &submit, fence);
    if (submitResult == VK_SUCCESS) {
        const VkResult waitResult = vkWaitForFences(resources_->device, 1, &fence, VK_TRUE, UINT64_MAX);
        vkDestroyFence(resources_->device, fence, nullptr);
        checkVk(waitResult, "plot upload fence wait");
    } else {
        vkDestroyFence(resources_->device, fence, nullptr);
        checkVk(submitResult, "plot upload queue submit");
    }
    texture->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

void VulkanPlotRenderer::render(const std::shared_ptr<Texture>& texture, const std::vector<Batch>& batches,
                                int pixelsX, int pixelsY, double dpi) {
    std::size_t vertexCount = 0;
    for (const auto& batch : batches) {
        if (batch.vertices.size() > std::numeric_limits<std::size_t>::max() - vertexCount)
            throw std::length_error("plot: Vulkan vertex count overflow");
        vertexCount += batch.vertices.size();
    }
    if (vertexCount > std::numeric_limits<std::uint32_t>::max())
        throw std::length_error("plot: Vulkan draw vertex count exceeded");
    const VkDeviceSize vertexBytes = static_cast<VkDeviceSize>(vertexCount) * sizeof(Vertex);
    const std::size_t frameIndex = resources_->nextFrame;
    auto& frame = resources_->frames[frameIndex];
    if (frame.submitted) {
        checkVk(vkWaitForFences(resources_->device, 1, &frame.fence, VK_TRUE, UINT64_MAX),
                "plot frame-slot fence wait");
        frame.submitted = false;
    }
    checkVk(vkResetCommandBuffer(frame.commandBuffer, 0), "plot frame command reset");

    if (vertexBytes > frame.verticesCapacity) {
        if (frame.vertices)
            vkDestroyBuffer(resources_->device, frame.vertices, nullptr);
        if (frame.verticesMemory)
            vkFreeMemory(resources_->device, frame.verticesMemory, nullptr);
        frame.vertices = VK_NULL_HANDLE;
        frame.verticesMemory = VK_NULL_HANDLE;
        frame.verticesCapacity = 0;
        VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufferInfo.size = vertexBytes;
        bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        checkVk(vkCreateBuffer(resources_->device, &bufferInfo, nullptr, &frame.vertices),
                "plot vertex buffer creation");
        VkMemoryRequirements requirements{};
        vkGetBufferMemoryRequirements(resources_->device, frame.vertices, &requirements);
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = findMemoryType(resources_->physicalDevice, requirements.memoryTypeBits,
                                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                                                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        VkPhysicalDeviceMemoryProperties properties{};
        vkGetPhysicalDeviceMemoryProperties(resources_->physicalDevice, &properties);
        frame.verticesMemoryFlags = properties.memoryTypes[allocation.memoryTypeIndex].propertyFlags;
        frame.verticesAllocationSize = requirements.size;
        checkVk(vkAllocateMemory(resources_->device, &allocation, nullptr, &frame.verticesMemory),
                "plot vertex memory allocation");
        checkVk(vkBindBufferMemory(resources_->device, frame.vertices, frame.verticesMemory, 0),
                "plot vertex memory binding");
        frame.verticesCapacity = requirements.size;
    }
    if (vertexCount != 0) {
        void* mapped = nullptr;
        checkVk(vkMapMemory(resources_->device, frame.verticesMemory, 0,
                            frame.verticesAllocationSize, 0, &mapped), "plot vertex memory map");
        auto* destination = static_cast<Vertex*>(mapped);
        for (const auto& batch : batches) {
            for (const auto& vertex : batch.vertices) {
                const double x = std::round(static_cast<double>(vertex.x) * dpi * 256.0) / 256.0;
                const double y = std::round(static_cast<double>(vertex.y) * dpi * 256.0) / 256.0;
                *destination++ = {static_cast<float>(x), static_cast<float>(y)};
            }
        }
        if ((frame.verticesMemoryFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0) {
            VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
            range.memory = frame.verticesMemory;
            range.offset = 0;
            range.size = VK_WHOLE_SIZE;
            const VkResult flushResult = vkFlushMappedMemoryRanges(resources_->device, 1, &range);
            vkUnmapMemory(resources_->device, frame.verticesMemory);
            checkVk(flushResult, "plot vertex memory flush");
        } else {
            vkUnmapMemory(resources_->device, frame.verticesMemory);
        }
    }

    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    checkVk(vkBeginCommandBuffer(frame.commandBuffer, &begin), "plot render command begin");

    VkImageMemoryBarrier toColor{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toColor.srcAccessMask = texture->layout == VK_IMAGE_LAYOUT_UNDEFINED ? 0 : VK_ACCESS_SHADER_READ_BIT;
    toColor.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    toColor.oldLayout = texture->layout;
    toColor.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    toColor.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toColor.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toColor.image = texture->image;
    toColor.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toColor.subresourceRange.levelCount = 1;
    toColor.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(frame.commandBuffer,
                         texture->layout == VK_IMAGE_LAYOUT_UNDEFINED ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
                                                                      : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &toColor);

    VkClearValue clear{};
    clear.color.float32[0] = 10.f / 255.f;
    clear.color.float32[1] = 13.f / 255.f;
    clear.color.float32[2] = 18.f / 255.f;
    clear.color.float32[3] = 1.f;
    VkRenderPassBeginInfo renderPassInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    renderPassInfo.renderPass = resources_->renderPass;
    renderPassInfo.framebuffer = texture->framebuffer;
    renderPassInfo.renderArea.extent = {static_cast<std::uint32_t>(pixelsX), static_cast<std::uint32_t>(pixelsY)};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clear;
    vkCmdBeginRenderPass(frame.commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    VkViewport viewport{};
    viewport.width = static_cast<float>(pixelsX);
    viewport.height = static_cast<float>(pixelsY);
    viewport.minDepth = 0;
    viewport.maxDepth = 1;
    vkCmdSetViewport(frame.commandBuffer, 0, 1, &viewport);
    VkRect2D scissor{};
    scissor.extent = {static_cast<std::uint32_t>(pixelsX), static_cast<std::uint32_t>(pixelsY)};
    vkCmdSetScissor(frame.commandBuffer, 0, 1, &scissor);
    if (vertexCount != 0) {
        const VkDeviceSize offset = 0;
        vkCmdBindPipeline(frame.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, resources_->pipeline);
        vkCmdBindVertexBuffers(frame.commandBuffer, 0, 1, &frame.vertices, &offset);
        std::uint32_t firstVertex = 0;
        for (const auto& batch : batches) {
            if (batch.vertices.empty())
                continue;
            PushConstants push{{static_cast<float>(pixelsX), static_cast<float>(pixelsY)}, {0.f, 0.f},
                               {batch.color[0], batch.color[1], batch.color[2], batch.color[3]}};
            vkCmdPushConstants(frame.commandBuffer, resources_->pipelineLayout,
                               VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
            vkCmdDraw(frame.commandBuffer, static_cast<std::uint32_t>(batch.vertices.size()), 1, firstVertex, 0);
            firstVertex += static_cast<std::uint32_t>(batch.vertices.size());
        }
    }
    vkCmdEndRenderPass(frame.commandBuffer);

    VkImageMemoryBarrier toSample{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toSample.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    toSample.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    toSample.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    toSample.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toSample.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toSample.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toSample.image = texture->image;
    toSample.subresourceRange = toColor.subresourceRange;
    vkCmdPipelineBarrier(frame.commandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toSample);
    checkVk(vkEndCommandBuffer(frame.commandBuffer), "plot render command end");
    checkVk(vkResetFences(resources_->device, 1, &frame.fence), "plot frame fence reset");
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &frame.commandBuffer;
    const VkResult submitResult = vkQueueSubmit(resources_->queue, 1, &submit, frame.fence);
    if (submitResult != VK_SUCCESS) {
        vkDestroyFence(resources_->device, frame.fence, nullptr);
        frame.fence = VK_NULL_HANDLE;
        VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        checkVk(vkCreateFence(resources_->device, &fenceInfo, nullptr, &frame.fence),
                "plot frame fence recovery");
        checkVk(submitResult, "plot render queue submit");
    }
    frame.submitted = true;
    resources_->nextFrame = (frameIndex + 1) % resources_->frames.size();
    texture->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

} // namespace modules::plot::detail

#endif
