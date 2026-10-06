#include "modules/plot/renderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#if defined(EUI_RENDER_BACKEND_OPENGL)
#include <glad/glad.h>
#elif defined(EUI_RENDER_BACKEND_VULKAN)
#include <vulkan/vulkan.h>
#endif

namespace modules::plot {

#if defined(EUI_RENDER_BACKEND_OPENGL)
namespace {
// 模块在 compose 中绘制，恢复全部所触及状态以保持核心 renderer 的状态缓存有效。
struct GlState {
    GLint program, vao, buffer, drawFramebuffer, readFramebuffer, activeTexture, texture, unpackBuffer;
    GLint viewport[4], blendSrcRgb, blendDstRgb, blendSrcAlpha, blendDstAlpha, blendRgb, blendAlpha;
    GLboolean blend, depth, cull, scissor, stencil, srgb, rasterizerDiscard, colorMask[4];
    GLfloat clearColor[4];
    GLint unpackAlignment, unpackRowLength, unpackSkipRows, unpackSkipPixels;
    GlState() {
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &unpackAlignment);
        glGetIntegerv(GL_UNPACK_ROW_LENGTH, &unpackRowLength);
        glGetIntegerv(GL_UNPACK_SKIP_ROWS, &unpackSkipRows);
        glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &unpackSkipPixels);
        glGetIntegerv(GL_CURRENT_PROGRAM, &program);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer);
        glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpackBuffer);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
        glGetIntegerv(GL_VIEWPORT, viewport);
        glGetIntegerv(GL_BLEND_SRC_RGB, &blendSrcRgb);
        glGetIntegerv(GL_BLEND_DST_RGB, &blendDstRgb);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrcAlpha);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDstAlpha);
        glGetIntegerv(GL_BLEND_EQUATION_RGB, &blendRgb);
        glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &blendAlpha);
        glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColor);
        glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
        blend = glIsEnabled(GL_BLEND);
        depth = glIsEnabled(GL_DEPTH_TEST);
        cull = glIsEnabled(GL_CULL_FACE);
        scissor = glIsEnabled(GL_SCISSOR_TEST);
        stencil = glIsEnabled(GL_STENCIL_TEST);
        srgb = glIsEnabled(GL_FRAMEBUFFER_SRGB);
        rasterizerDiscard = glIsEnabled(GL_RASTERIZER_DISCARD);
    }
    ~GlState() {
        glPixelStorei(GL_UNPACK_ALIGNMENT, unpackAlignment);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, unpackRowLength);
        glPixelStorei(GL_UNPACK_SKIP_ROWS, unpackSkipRows);
        glPixelStorei(GL_UNPACK_SKIP_PIXELS, unpackSkipPixels);
        glUseProgram(program);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, buffer);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, unpackBuffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFramebuffer);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, readFramebuffer);
        glActiveTexture(activeTexture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        glBlendFuncSeparate(blendSrcRgb, blendDstRgb, blendSrcAlpha, blendDstAlpha);
        glBlendEquationSeparate(blendRgb, blendAlpha);
        glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
        glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
        const auto restore = [](GLenum capability, GLboolean enabled) {
            if (enabled)
                glEnable(capability);
            else
                glDisable(capability);
        };
        restore(GL_BLEND, blend);
        restore(GL_DEPTH_TEST, depth);
        restore(GL_CULL_FACE, cull);
        restore(GL_SCISSOR_TEST, scissor);
        restore(GL_STENCIL_TEST, stencil);
        restore(GL_FRAMEBUFFER_SRGB, srgb);
        restore(GL_RASTERIZER_DISCARD, rasterizerDiscard);
    }
};
struct Texture {
    GLuint texture = 0;
    ~Texture() {
        if (texture)
            glDeleteTextures(1, &texture);
    }
};
GLuint shader(GLenum type, const char* source) {
    const GLuint value = glCreateShader(type);
    glShaderSource(value, 1, &source, nullptr);
    glCompileShader(value);
    GLint compiled = 0;
    glGetShaderiv(value, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char log[2048]{};
        glGetShaderInfoLog(value, sizeof(log), nullptr, log);
        glDeleteShader(value);
        throw std::runtime_error(std::string("plot: shader compilation: ") + log);
    }
    return value;
}
GLuint program() {
    const auto vertex =
        shader(GL_VERTEX_SHADER,
               "#version 330 core\nlayout(location=0) in vec2 position; uniform vec2 size;"
               "void main(){gl_Position=vec4(position.x/size.x*2.0-1.0,1.0-position.y/size.y*2.0,0,1);}");
    GLuint fragment = 0, value = 0;
    try {
        fragment = shader(GL_FRAGMENT_SHADER,
                          "#version 330 core\nuniform vec4 color; out vec4 pixel; void main(){pixel=color;}");
        value = glCreateProgram();
        glAttachShader(value, vertex);
        glAttachShader(value, fragment);
        glLinkProgram(value);
        GLint linked = 0;
        glGetProgramiv(value, GL_LINK_STATUS, &linked);
        if (!linked)
            throw std::runtime_error("plot: shader link failed");
    } catch (...) {
        glDeleteShader(vertex);
        if (fragment)
            glDeleteShader(fragment);
        if (value)
            glDeleteProgram(value);
        throw;
    }
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    return value;
}
} // namespace
#elif defined(EUI_RENDER_BACKEND_VULKAN)
namespace {
struct VulkanTexture {
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    std::uint32_t queueFamily = 0;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory imageMemory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkBuffer staging = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    VkDeviceSize stagingCapacity = 0;
    VkMemoryPropertyFlags stagingProperties = 0;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
    std::uint32_t width = 0;
    std::uint32_t height = 0;

    ~VulkanTexture() {
        if (device == VK_NULL_HANDLE)
            return;
        if (staging)
            vkDestroyBuffer(device, staging, nullptr);
        if (stagingMemory)
            vkFreeMemory(device, stagingMemory, nullptr);
        if (commandPool)
            vkDestroyCommandPool(device, commandPool, nullptr);
        if (view)
            vkDestroyImageView(device, view, nullptr);
        if (image)
            vkDestroyImage(device, image, nullptr);
        if (imageMemory)
            vkFreeMemory(device, imageMemory, nullptr);
    }
};

void checkVk(VkResult result, const char* operation) {
    if (result != VK_SUCCESS)
        throw std::runtime_error(std::string("plot: Vulkan ") + operation + " failed with VkResult " +
                                 std::to_string(static_cast<int>(result)));
}

std::uint32_t findMemoryType(VkPhysicalDevice physicalDevice, std::uint32_t bits,
                             VkMemoryPropertyFlags required, VkMemoryPropertyFlags preferred = 0) {
    VkPhysicalDeviceMemoryProperties properties{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &properties);
    std::uint32_t fallback = std::numeric_limits<std::uint32_t>::max();
    for (std::uint32_t index = 0; index < properties.memoryTypeCount; ++index) {
        if ((bits & (1u << index)) == 0 ||
            (properties.memoryTypes[index].propertyFlags & required) != required)
            continue;
        if ((properties.memoryTypes[index].propertyFlags & preferred) == preferred)
            return index;
        if (fallback == std::numeric_limits<std::uint32_t>::max())
            fallback = index;
    }
    if (fallback == std::numeric_limits<std::uint32_t>::max())
        throw std::runtime_error("plot: Vulkan has no compatible memory type");
    return fallback;
}

std::shared_ptr<VulkanTexture> createVulkanTexture(const eui::GpuDeviceInfo& info,
                                                   std::uint32_t width, std::uint32_t height) {
    auto texture = std::make_shared<VulkanTexture>();
    texture->device = reinterpret_cast<VkDevice>(info.device);
    texture->physicalDevice = reinterpret_cast<VkPhysicalDevice>(info.physicalDevice);
    texture->queue = reinterpret_cast<VkQueue>(info.graphicsQueue);
    texture->queueFamily = info.graphicsQueueFamily;
    texture->width = width;
    texture->height = height;
    if (texture->device == VK_NULL_HANDLE || texture->physicalDevice == VK_NULL_HANDLE ||
        texture->queue == VK_NULL_HANDLE)
        throw std::runtime_error("plot: Vulkan device information is incomplete");

    VkFormatProperties formatProperties{};
    vkGetPhysicalDeviceFormatProperties(texture->physicalDevice, VK_FORMAT_R8G8B8A8_UNORM,
                                        &formatProperties);
    const VkFormatFeatureFlags required = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
                                         VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT |
                                         VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
    if ((formatProperties.optimalTilingFeatures & required) != required)
        throw std::runtime_error("plot: Vulkan RGBA8 images cannot be sampled and uploaded");

    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.extent = {width, height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    checkVk(vkCreateImage(texture->device, &imageInfo, nullptr, &texture->image), "image creation");

    VkMemoryRequirements imageRequirements{};
    vkGetImageMemoryRequirements(texture->device, texture->image, &imageRequirements);
    VkMemoryAllocateInfo imageAllocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    imageAllocation.allocationSize = imageRequirements.size;
    imageAllocation.memoryTypeIndex = findMemoryType(texture->physicalDevice,
        imageRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    checkVk(vkAllocateMemory(texture->device, &imageAllocation, nullptr, &texture->imageMemory),
            "image memory allocation");
    checkVk(vkBindImageMemory(texture->device, texture->image, texture->imageMemory, 0), "image memory binding");

    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = texture->image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    checkVk(vkCreateImageView(texture->device, &viewInfo, nullptr, &texture->view), "image view creation");

    VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = texture->queueFamily;
    checkVk(vkCreateCommandPool(texture->device, &poolInfo, nullptr, &texture->commandPool), "command pool creation");
    VkCommandBufferAllocateInfo commandInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    commandInfo.commandPool = texture->commandPool;
    commandInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandInfo.commandBufferCount = 1;
    checkVk(vkAllocateCommandBuffers(texture->device, &commandInfo, &texture->commandBuffer),
            "command buffer allocation");
    return texture;
}

void ensureStaging(VulkanTexture& texture, VkDeviceSize required) {
    if (texture.staging != VK_NULL_HANDLE && texture.stagingCapacity >= required)
        return;
    if (texture.staging)
        vkDestroyBuffer(texture.device, texture.staging, nullptr);
    if (texture.stagingMemory)
        vkFreeMemory(texture.device, texture.stagingMemory, nullptr);
    texture.staging = VK_NULL_HANDLE;
    texture.stagingMemory = VK_NULL_HANDLE;
    texture.stagingCapacity = 0;

    VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = required;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    checkVk(vkCreateBuffer(texture.device, &bufferInfo, nullptr, &texture.staging), "staging buffer creation");
    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(texture.device, texture.staging, &requirements);
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = findMemoryType(texture.physicalDevice, requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VkPhysicalDeviceMemoryProperties properties{};
    vkGetPhysicalDeviceMemoryProperties(texture.physicalDevice, &properties);
    texture.stagingProperties = properties.memoryTypes[allocation.memoryTypeIndex].propertyFlags;
    checkVk(vkAllocateMemory(texture.device, &allocation, nullptr, &texture.stagingMemory),
            "staging memory allocation");
    checkVk(vkBindBufferMemory(texture.device, texture.staging, texture.stagingMemory, 0),
            "staging memory binding");
    texture.stagingCapacity = requirements.size;
}

void uploadVulkanTexture(VulkanTexture& texture, const std::uint8_t* rgba, std::size_t byteCount,
                         bool flipRows) {
    ensureStaging(texture, static_cast<VkDeviceSize>(byteCount));
    void* mapped = nullptr;
    checkVk(vkMapMemory(texture.device, texture.stagingMemory, 0, texture.stagingCapacity, 0, &mapped),
            "staging memory map");
    if (flipRows) {
        auto* destination = static_cast<std::uint8_t*>(mapped);
        const std::size_t rowBytes = static_cast<std::size_t>(texture.width) * 4;
        for (std::uint32_t row = 0; row < texture.height; ++row)
            std::memcpy(destination + static_cast<std::size_t>(texture.height - 1 - row) * rowBytes,
                        rgba + static_cast<std::size_t>(row) * rowBytes, rowBytes);
    } else {
        std::memcpy(mapped, rgba, byteCount);
    }
    if ((texture.stagingProperties & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0) {
        VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE};
        range.memory = texture.stagingMemory;
        range.size = VK_WHOLE_SIZE;
        const VkResult flushResult = vkFlushMappedMemoryRanges(texture.device, 1, &range);
        vkUnmapMemory(texture.device, texture.stagingMemory);
        checkVk(flushResult, "staging memory flush");
    } else {
        vkUnmapMemory(texture.device, texture.stagingMemory);
    }

    checkVk(vkResetCommandPool(texture.device, texture.commandPool, 0), "command pool reset");
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    checkVk(vkBeginCommandBuffer(texture.commandBuffer, &begin), "command buffer begin");
    VkImageMemoryBarrier toTransfer{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toTransfer.srcAccessMask = texture.layout == VK_IMAGE_LAYOUT_UNDEFINED ? 0 : VK_ACCESS_SHADER_READ_BIT;
    toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toTransfer.oldLayout = texture.layout;
    toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.image = texture.image;
    toTransfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toTransfer.subresourceRange.levelCount = 1;
    toTransfer.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(texture.commandBuffer,
        texture.layout == VK_IMAGE_LAYOUT_UNDEFINED ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
                                                    : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toTransfer);

    VkBufferImageCopy copy{};
    copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy.imageSubresource.layerCount = 1;
    copy.imageExtent = {texture.width, texture.height, 1};
    vkCmdCopyBufferToImage(texture.commandBuffer, texture.staging, texture.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

    VkImageMemoryBarrier toSample{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    toSample.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toSample.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    toSample.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toSample.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    toSample.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toSample.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toSample.image = texture.image;
    toSample.subresourceRange = toTransfer.subresourceRange;
    vkCmdPipelineBarrier(texture.commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toSample);
    checkVk(vkEndCommandBuffer(texture.commandBuffer), "command buffer end");

    VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence fence = VK_NULL_HANDLE;
    checkVk(vkCreateFence(texture.device, &fenceInfo, nullptr, &fence), "upload fence creation");
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &texture.commandBuffer;
    const VkResult submitResult = vkQueueSubmit(texture.queue, 1, &submit, fence);
    if (submitResult == VK_SUCCESS) {
        const VkResult waitResult = vkWaitForFences(texture.device, 1, &fence, VK_TRUE, UINT64_MAX);
        vkDestroyFence(texture.device, fence, nullptr);
        checkVk(waitResult, "upload fence wait");
    } else {
        vkDestroyFence(texture.device, fence, nullptr);
        checkVk(submitResult, "queue submit");
    }
    texture.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

std::uint64_t imageViewHandle(VkImageView view) {
    std::uint64_t result = 0;
    static_assert(sizeof(view) <= sizeof(result));
    std::memcpy(&result, &view, sizeof(view));
    return result;
}

std::vector<std::uint8_t> rasterize(const std::vector<Batch>& batches, int width, int height, double dpi) {
    const std::size_t pixels = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    std::vector<std::uint8_t> rgba(pixels * 4);
    for (std::size_t index = 0; index < pixels; ++index) {
        rgba[index * 4] = 10;
        rgba[index * 4 + 1] = 13;
        rgba[index * 4 + 2] = 18;
        rgba[index * 4 + 3] = 255;
    }
    const auto edge = [](double ax, double ay, double bx, double by, double x, double y) {
        return (x - ax) * (by - ay) - (y - ay) * (bx - ax);
    };
    for (const auto& batch : batches) {
        const double alpha = batch.color[3];
        if (alpha <= 0)
            continue;
        const double inverseAlpha = 1.0 - alpha;
        const double sourceRed = batch.color[0] * alpha * 255.0;
        const double sourceGreen = batch.color[1] * alpha * 255.0;
        const double sourceBlue = batch.color[2] * alpha * 255.0;
        for (std::size_t triangle = 0; triangle < batch.vertices.size(); triangle += 3) {
            const auto& a = batch.vertices[triangle];
            const auto& b = batch.vertices[triangle + 1];
            const auto& c = batch.vertices[triangle + 2];
            const double ax = double(a.x) * dpi, ay = double(a.y) * dpi;
            const double bx = double(b.x) * dpi, by = double(b.y) * dpi;
            const double cx = double(c.x) * dpi, cy = double(c.y) * dpi;
            const double area = edge(ax, ay, bx, by, cx, cy);
            if (std::abs(area) < 1e-12)
                continue;
            const double orientation = area > 0 ? 1.0 : -1.0;
            const double edgeTolerance = std::abs(area) * 1e-9;
            const int left = static_cast<int>(std::clamp(std::floor(std::min({ax, bx, cx})), 0.0,
                                                         double(width - 1)));
            const int right = static_cast<int>(std::clamp(std::ceil(std::max({ax, bx, cx})), 0.0,
                                                            double(width)));
            const int top = static_cast<int>(std::clamp(std::floor(std::min({ay, by, cy})), 0.0,
                                                        double(height - 1)));
            const int bottom = static_cast<int>(std::clamp(std::ceil(std::max({ay, by, cy})), 0.0,
                                                           double(height)));
            const double startX = left + 0.5;
            const double startY = top + 0.5;
            double rowEdge0 = orientation * edge(bx, by, cx, cy, startX, startY);
            double rowEdge1 = orientation * edge(cx, cy, ax, ay, startX, startY);
            double rowEdge2 = orientation * edge(ax, ay, bx, by, startX, startY);
            const double edge0StepX = orientation * (cy - by);
            const double edge1StepX = orientation * (ay - cy);
            const double edge2StepX = orientation * (by - ay);
            const double edge0StepY = orientation * (bx - cx);
            const double edge1StepY = orientation * (cx - ax);
            const double edge2StepY = orientation * (ax - bx);
            for (int y = top; y < bottom; ++y) {
                double e0 = rowEdge0;
                double e1 = rowEdge1;
                double e2 = rowEdge2;
                for (int x = left; x < right; ++x) {
                    if (e0 >= -edgeTolerance && e1 >= -edgeTolerance && e2 >= -edgeTolerance) {
                        const std::size_t offset = (static_cast<std::size_t>(y) * width + x) * 4;
                        rgba[offset] = static_cast<std::uint8_t>(sourceRed + rgba[offset] * inverseAlpha + 0.5);
                        rgba[offset + 1] =
                            static_cast<std::uint8_t>(sourceGreen + rgba[offset + 1] * inverseAlpha + 0.5);
                        rgba[offset + 2] =
                            static_cast<std::uint8_t>(sourceBlue + rgba[offset + 2] * inverseAlpha + 0.5);
                    }
                    e0 += edge0StepX;
                    e1 += edge1StepX;
                    e2 += edge2StepX;
                }
                rowEdge0 += edge0StepY;
                rowEdge1 += edge1StepY;
                rowEdge2 += edge2StepY;
            }
        }
    }
    return rgba;
}
} // namespace
#endif

struct Renderer::Impl {
    Budget budget;
    std::shared_ptr<const eui::GpuImage> image;
    std::uint64_t revision = 0;
#if defined(EUI_RENDER_BACKEND_OPENGL)
    GLuint program = 0, vao = 0, vbo = 0, framebuffer = 0;
    std::size_t capacity = 0;
    std::vector<Vertex> uploaded;
    std::uint64_t device = 0;
    ~Impl() {
        image.reset();
        if (framebuffer)
            glDeleteFramebuffers(1, &framebuffer);
        if (vbo)
            glDeleteBuffers(1, &vbo);
        if (vao)
            glDeleteVertexArrays(1, &vao);
        if (program)
            glDeleteProgram(program);
    }
#elif defined(EUI_RENDER_BACKEND_VULKAN)
    std::shared_ptr<VulkanTexture> vulkanTexture;
    std::uint64_t device = 0;
    ~Impl() {
        image.reset();
        vulkanTexture.reset();
    }
#endif
};
Renderer::Renderer(Budget budget) : impl_(std::make_unique<Impl>()) { impl_->budget = budget; }
Renderer::~Renderer() = default;
std::shared_ptr<const eui::GpuImage> Renderer::image() const { return impl_->image; }
std::uint64_t Renderer::revision() const noexcept { return impl_->revision; }
void Renderer::release() {
    const auto budget = impl_->budget;
    const auto revision = impl_->revision;
    impl_ = std::make_unique<Impl>();
    impl_->budget = budget;
    impl_->revision = revision;
}
void Renderer::uploadRgba(std::uint32_t width, std::uint32_t height, const std::vector<std::uint8_t>& rgba) {
#if defined(EUI_RENDER_BACKEND_OPENGL)
    if (!width || !height || width > 32768 || height > 32768 ||
        std::uint64_t(width) * height * 4 != rgba.size())
        throw std::invalid_argument("plot: invalid RGBA dimensions");
    if (rgba.size() > impl_->budget.textureBytes)
        throw std::length_error("plot: GPU texture budget exceeded");
    const auto device = eui::image::gpuDevice();
    if (device.api != eui::GpuApi::OpenGL || !device.identity ||
        (impl_->device && impl_->device != device.identity))
        throw std::runtime_error("plot: RGBA upload requires the owning OpenGL device");
    GlState state;
    GLint maximum = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximum);
    if (width > static_cast<std::uint32_t>(maximum) || height > static_cast<std::uint32_t>(maximum))
        throw std::length_error("plot: GPU texture limit exceeded");
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
    glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
    if (!impl_->image || impl_->image->descriptor().width != static_cast<int>(width) ||
        impl_->image->descriptor().height != static_cast<int>(height)) {
        auto owner = std::make_shared<Texture>();
        glGenTextures(1, &owner->texture);
        glBindTexture(GL_TEXTURE_2D, owner->texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        eui::GpuImageDescriptor descriptor;
        descriptor.device = device;
        descriptor.width = static_cast<int>(width);
        descriptor.height = static_cast<int>(height);
        descriptor.texture = owner->texture;
        auto image = eui::image::importGpuImage(descriptor, owner);
        if (!image) throw std::runtime_error("plot: GPU image import failed");
        impl_->image = std::move(image);
    } else {
        glBindTexture(GL_TEXTURE_2D, impl_->image->descriptor().texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    }
    impl_->device = device.identity;
    ++impl_->revision;
#elif defined(EUI_RENDER_BACKEND_VULKAN)
    if (!width || !height || width > 32768 || height > 32768 ||
        std::uint64_t(width) * height * 4 != rgba.size())
        throw std::invalid_argument("plot: invalid RGBA dimensions");
    if (rgba.size() > impl_->budget.textureBytes)
        throw std::length_error("plot: GPU texture budget exceeded");
    const auto device = eui::image::gpuDevice();
    if (device.api != eui::GpuApi::Vulkan || !device.identity ||
        (impl_->device && impl_->device != device.identity))
        throw std::runtime_error("plot: RGBA upload requires the owning Vulkan device");
    const bool recreate = !impl_->vulkanTexture || !impl_->image ||
        impl_->image->descriptor().width != static_cast<int>(width) ||
        impl_->image->descriptor().height != static_cast<int>(height);
    auto texture = recreate ? createVulkanTexture(device, width, height) : impl_->vulkanTexture;
    uploadVulkanTexture(*texture, rgba.data(), rgba.size(), true);
    if (recreate) {
        eui::GpuImageDescriptor descriptor;
        descriptor.device = device;
        descriptor.width = static_cast<int>(width);
        descriptor.height = static_cast<int>(height);
        descriptor.imageView = imageViewHandle(texture->view);
        auto image = eui::image::importGpuImage(descriptor, texture);
        if (!image)
            throw std::runtime_error("plot: Vulkan GPU image import failed");
        impl_->vulkanTexture = std::move(texture);
        impl_->image = std::move(image);
    }
    impl_->device = device.identity;
    ++impl_->revision;
#else
    (void)width; (void)height; (void)rgba;
    throw std::runtime_error("plot: Vulkan upload is not implemented");
#endif
}
void Renderer::render(const std::vector<Batch>& batches, double width, double height, double dpi) {
#if defined(EUI_RENDER_BACKEND_OPENGL)
    const auto device = eui::image::gpuDevice();
    if (device.api != eui::GpuApi::OpenGL || !device.identity)
        throw std::runtime_error("plot: render requires an active OpenGL window");
    if (impl_->device && impl_->device != device.identity)
        throw std::runtime_error("plot: renderer cannot cross window devices");
    if (!std::isfinite(width) || !std::isfinite(height) || !std::isfinite(dpi) || width <= 0 || height <= 0 ||
        dpi <= 0 || width * dpi > 32768 || height * dpi > 32768)
        throw std::invalid_argument("plot: invalid render dimensions");
    const int pixelsX = static_cast<int>(std::ceil(width * dpi));
    const int pixelsY = static_cast<int>(std::ceil(height * dpi));
    if (std::size_t(pixelsX) * pixelsY > impl_->budget.textureBytes / 4)
        throw std::length_error("plot: texture budget exceeded");
    std::size_t count = 0;
    for (const auto& batch : batches) {
        if (batch.vertices.size() % 3 != 0)
            throw std::invalid_argument("plot: incomplete triangle");
        if (batch.vertices.size() > impl_->budget.vertexBytes / sizeof(Vertex) - count)
            throw std::length_error("plot: vertex budget exceeded");
        count += batch.vertices.size();
        for (float value : batch.color)
            if (!std::isfinite(value) || value < 0 || value > 1)
                throw std::invalid_argument("plot: invalid batch color");
        for (const auto& vertex : batch.vertices)
            if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y))
                throw std::invalid_argument("plot: nonfinite vertex");
    }
    if (count > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max()))
        throw std::length_error("plot: draw vertex count exceeded");
    GlState state;
    impl_->device = device.identity;
    GLint maximum = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximum);
    if (pixelsX > maximum || pixelsY > maximum)
        throw std::length_error("plot: GPU texture limit exceeded");
    if (!impl_->program)
        impl_->program = program();
    if (!impl_->vao)
        glGenVertexArrays(1, &impl_->vao);
    if (!impl_->vbo)
        glGenBuffers(1, &impl_->vbo);
    if (!impl_->framebuffer)
        glGenFramebuffers(1, &impl_->framebuffer);
    if (!impl_->image || impl_->image->descriptor().width != pixelsX ||
        impl_->image->descriptor().height != pixelsY) {
        auto owner = std::make_shared<Texture>();
        glGenTextures(1, &owner->texture);
        glBindTexture(GL_TEXTURE_2D, owner->texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, pixelsX, pixelsY, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        eui::GpuImageDescriptor descriptor;
        descriptor.device = device;
        descriptor.width = pixelsX;
        descriptor.height = pixelsY;
        descriptor.texture = owner->texture;
        auto image = eui::image::importGpuImage(descriptor, owner);
        if (!image)
            throw std::runtime_error("plot: GPU image import failed");
        impl_->image = std::move(image);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, impl_->framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           impl_->image->descriptor().texture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        throw std::runtime_error("plot: framebuffer incomplete");
    glViewport(0, 0, pixelsX, pixelsY);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_FRAMEBUFFER_SRGB);
    glDisable(GL_RASTERIZER_DISCARD);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    // 不透明绘图区使颜色保持 straight alpha，避免 UI 再合成时重复预乘。
    glClearColor(0.04f, 0.05f, 0.07f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(impl_->program);
    glUniform2f(glGetUniformLocation(impl_->program, "size"), float(width), float(height));
    glBindVertexArray(impl_->vao);
    glBindBuffer(GL_ARRAY_BUFFER, impl_->vbo);
    if (count * sizeof(Vertex) > impl_->capacity) {
        impl_->capacity = count * sizeof(Vertex);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(impl_->capacity), nullptr, GL_DYNAMIC_DRAW);
        impl_->uploaded.clear();
    }
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
    glEnableVertexAttribArray(0);
    std::size_t offset = 0;
    for (const auto& batch : batches) {
        if (batch.vertices.empty())
            continue;
        // 固定大小块比较，数据局部变化且几何布局不变时只上传受影响区间。
        constexpr std::size_t chunkVertices = 4096;
        for (std::size_t start = 0; start < batch.vertices.size(); start += chunkVertices) {
            const auto length = std::min(chunkVertices, batch.vertices.size() - start);
            const auto first = offset + start;
            const bool changed = first + length > impl_->uploaded.size() ||
                                 std::memcmp(impl_->uploaded.data() + first, batch.vertices.data() + start,
                                             length * sizeof(Vertex)) != 0;
            if (changed) {
                glBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>(first * sizeof(Vertex)),
                                static_cast<GLsizeiptr>(length * sizeof(Vertex)),
                                batch.vertices.data() + start);
                if (first + length > impl_->uploaded.size())
                    impl_->uploaded.resize(first + length);
                std::copy_n(batch.vertices.data() + start, length, impl_->uploaded.data() + first);
            }
        }
        glUniform4fv(glGetUniformLocation(impl_->program, "color"), 1, batch.color.data());
        glDrawArrays(GL_TRIANGLES, static_cast<GLint>(offset), static_cast<GLsizei>(batch.vertices.size()));
        offset += batch.vertices.size();
    }
    ++impl_->revision;
#elif defined(EUI_RENDER_BACKEND_VULKAN)
    const auto device = eui::image::gpuDevice();
    if (device.api != eui::GpuApi::Vulkan || !device.identity ||
        (impl_->device && impl_->device != device.identity))
        throw std::runtime_error("plot: render requires the owning Vulkan device");
    if (!std::isfinite(width) || !std::isfinite(height) || !std::isfinite(dpi) || width <= 0 || height <= 0 ||
        dpi <= 0 || width * dpi > 32768 || height * dpi > 32768)
        throw std::invalid_argument("plot: invalid render dimensions");
    const int pixelsX = static_cast<int>(std::ceil(width * dpi));
    const int pixelsY = static_cast<int>(std::ceil(height * dpi));
    if (std::size_t(pixelsX) * pixelsY > impl_->budget.textureBytes / 4)
        throw std::length_error("plot: texture budget exceeded");
    std::size_t count = 0;
    for (const auto& batch : batches) {
        if (batch.vertices.size() % 3 != 0)
            throw std::invalid_argument("plot: incomplete triangle");
        const std::size_t maximumVertices = impl_->budget.vertexBytes / sizeof(Vertex);
        if (count > maximumVertices || batch.vertices.size() > maximumVertices - count)
            throw std::length_error("plot: vertex budget exceeded");
        count += batch.vertices.size();
        for (float value : batch.color)
            if (!std::isfinite(value) || value < 0 || value > 1)
                throw std::invalid_argument("plot: invalid batch color");
        for (const auto& vertex : batch.vertices)
            if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y))
                throw std::invalid_argument("plot: nonfinite vertex");
    }
    auto rgba = rasterize(batches, pixelsX, pixelsY, dpi);
    uploadRgba(static_cast<std::uint32_t>(pixelsX), static_cast<std::uint32_t>(pixelsY), rgba);
#else
    (void)batches;
    (void)width;
    (void)height;
    (void)dpi;
    throw std::runtime_error("plot: Vulkan rendering is not implemented");
#endif
}
} // namespace modules::plot
