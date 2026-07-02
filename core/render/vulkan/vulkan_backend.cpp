#include "core/render/vulkan/vulkan_backend.h"

#include "core/render/render_types.h"

#if defined(EUI_WINDOW_BACKEND_SDL3)
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#elif defined(EUI_WINDOW_BACKEND_SDL2)
#include <SDL.h>
#include <SDL_vulkan.h>
#else
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>
#endif

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <limits>
#include <set>

namespace core::render::vulkan {

namespace {

bool hasExtension(const std::vector<VkExtensionProperties>& extensions, const char* name) {
    return std::any_of(extensions.begin(), extensions.end(), [&](const VkExtensionProperties& extension) {
        return std::strcmp(extension.extensionName, name) == 0;
    });
}

void addUniqueExtension(std::vector<const char*>& extensions, const char* name) {
    const auto exists = std::any_of(extensions.begin(), extensions.end(), [&](const char* extension) {
        return std::strcmp(extension, name) == 0;
    });
    if (!exists) {
        extensions.push_back(name);
    }
}

std::vector<VkExtensionProperties> instanceExtensions() {
    std::uint32_t count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> extensions(count);
    if (count > 0) {
        vkEnumerateInstanceExtensionProperties(nullptr, &count, extensions.data());
    }
    return extensions;
}

std::vector<VkExtensionProperties> deviceExtensions(VkPhysicalDevice device) {
    std::uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> extensions(count);
    if (count > 0) {
        vkEnumerateDeviceExtensionProperties(device, nullptr, &count, extensions.data());
    }
    return extensions;
}

VkSurfaceFormatKHR chooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
    for (const VkSurfaceFormatKHR& format : formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    return formats.empty() ? VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR} : formats.front();
}

VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& modes) {
#if defined(EUI_VULKAN_LOW_LATENCY_PRESENT)
    for (VkPresentModeKHR mode : modes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return mode;
        }
    }
#else
    (void)modes;
#endif
    return VK_PRESENT_MODE_FIFO_KHR;
}

bool platformSupportsIncrementalPresent() {
#if defined(__APPLE__)
    return false;
#else
    return true;
#endif
}

} // namespace

void VulkanRenderBackend::writeColor(float (&target)[4], const core::Color& color) {
    target[0] = color.r;
    target[1] = color.g;
    target[2] = color.b;
    target[3] = color.a;
}

VkShaderModule VulkanRenderBackend::createShaderModule(VkDevice device, const std::uint32_t* code, std::size_t codeSize) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = codeSize;
    createInfo.pCode = code;

    VkShaderModule module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &module) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    return module;
}

VkAccessFlags accessMaskForLayout(VkImageLayout layout) {
    switch (layout) {
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            return VK_ACCESS_TRANSFER_READ_BIT;
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            return VK_ACCESS_TRANSFER_WRITE_BIT;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            return VK_ACCESS_SHADER_READ_BIT;
        default:
            return 0;
    }
}

VkPipelineStageFlags sourceStageForLayout(VkImageLayout layout) {
    switch (layout) {
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            return VK_PIPELINE_STAGE_TRANSFER_BIT;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
            return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        default:
            return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    }
}

VkPipelineStageFlags destinationStageForLayout(VkImageLayout layout) {
    if (layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
        return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    }
    return sourceStageForLayout(layout);
}

void VulkanRenderBackend::transitionImageLayout(VkCommandBuffer commandBuffer,
                                                VkImage image,
                                                VkImageLayout oldLayout,
                                                VkImageLayout newLayout) {
    if (image == VK_NULL_HANDLE || oldLayout == newLayout) {
        return;
    }
    ++core::render::currentRenderFrameStats().backendBarriers;

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = accessMaskForLayout(oldLayout);
    barrier.dstAccessMask = accessMaskForLayout(newLayout);

    vkCmdPipelineBarrier(commandBuffer,
                         sourceStageForLayout(oldLayout),
                         destinationStageForLayout(newLayout),
                         0,
                         0,
                         nullptr,
                         0,
                         nullptr,
                         1,
                         &barrier);
}

VkRect2D VulkanRenderBackend::clampScissor(const core::Rect& rect, int windowWidth, int windowHeight) const {
    const float maxWidth = static_cast<float>(std::max(0, windowWidth));
    const float maxHeight = static_cast<float>(std::max(0, windowHeight));
    const float left = std::clamp(std::floor(rect.x), 0.0f, maxWidth);
    const float top = std::clamp(std::floor(rect.y), 0.0f, maxHeight);
    const float right = std::clamp(std::ceil(rect.x + rect.width), left, maxWidth);
    const float bottom = std::clamp(std::ceil(rect.y + rect.height), top, maxHeight);

    VkRect2D result{};
    result.offset = {
        static_cast<std::int32_t>(left),
        static_cast<std::int32_t>(top)
    };
    result.extent = {
        static_cast<std::uint32_t>(std::max(0.0f, right - left)),
        static_cast<std::uint32_t>(std::max(0.0f, bottom - top))
    };
    return result;
}

VkCommandBuffer VulkanRenderBackend::currentCommandBuffer() const {
    if (!hasCurrentCommandBuffer()) {
        return VK_NULL_HANDLE;
    }
    return commandBuffers_[currentImage_];
}

bool VulkanRenderBackend::hasCurrentCommandBuffer() const {
    return currentImage_ < commandBuffers_.size();
}

void VulkanRenderBackend::endActiveRenderPass() {
    if (!renderPassActive_ || !hasCurrentCommandBuffer()) {
        return;
    }
    vkCmdEndRenderPass(currentCommandBuffer());
    renderPassActive_ = false;
}

bool VulkanRenderBackend::applyDrawViewportAndScissor(int windowWidth, int windowHeight) {
    if (!hasCurrentCommandBuffer() || windowWidth <= 0 || windowHeight <= 0) {
        return false;
    }

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(windowWidth);
    viewport.height = static_cast<float>(windowHeight);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(currentCommandBuffer(), 0, 1, &viewport);

    const core::Rect fullRect{0.0f, 0.0f, static_cast<float>(windowWidth), static_cast<float>(windowHeight)};
    const VkRect2D scissor = clampScissor(scissorEnabled_ ? scissorRect_ : fullRect, windowWidth, windowHeight);
    if (scissor.extent.width == 0 || scissor.extent.height == 0) {
        return false;
    }
    vkCmdSetScissor(currentCommandBuffer(), 0, 1, &scissor);
    return true;
}

VulkanRenderBackend::VulkanRenderBackend(core::window::Handle window, RenderBackend*) : window_(window) {}

VulkanRenderBackend::~VulkanRenderBackend() {
    destroy();
}

bool VulkanRenderBackend::initialize() {
    if (window_ == nullptr) {
        std::fprintf(stderr, "EUI Vulkan: initialize failed: window is null\n");
        return false;
    }
    if (!createInstance()) {
        std::fprintf(stderr, "EUI Vulkan: initialize failed: createInstance\n");
        return false;
    }
    if (!createSurface()) {
        std::fprintf(stderr, "EUI Vulkan: initialize failed: createSurface\n");
        return false;
    }
    if (!pickDevice()) {
        std::fprintf(stderr, "EUI Vulkan: initialize failed: pickDevice\n");
        return false;
    }
    if (!createDevice()) {
        std::fprintf(stderr, "EUI Vulkan: initialize failed: createDevice\n");
        return false;
    }
    initialized_ = true;
    return initialized_;
}

bool VulkanRenderBackend::valid() const {
    return initialized_;
}

void VulkanRenderBackend::makeCurrent() {
    if (device_ == VK_NULL_HANDLE || frameActive_ || inFlight_ == VK_NULL_HANDLE) {
        return;
    }
    if (uploadArena_.pendingBuffers.empty() && pendingTextureDeletes_.empty() && uploadArena_.buffer == VK_NULL_HANDLE) {
        return;
    }
    if (vkGetFenceStatus(device_, inFlight_) == VK_SUCCESS) {
        releasePendingTextureDeletes();
        releasePendingUploads();
    }
}

void VulkanRenderBackend::beginFrame(const RenderSurface& surface) {
    if (!initialized_ || surface.framebufferWidth <= 0 || surface.framebufferHeight <= 0) {
        return;
    }
    if (swapchain_ == VK_NULL_HANDLE ||
        swapchainExtent_.width != static_cast<std::uint32_t>(surface.framebufferWidth) ||
        swapchainExtent_.height != static_cast<std::uint32_t>(surface.framebufferHeight)) {
        recreateSwapchain(surface);
    }
    if (swapchain_ == VK_NULL_HANDLE || commandBuffers_.empty()) {
        return;
    }

    vkWaitForFences(device_, 1, &inFlight_, VK_TRUE, UINT64_MAX);
    releasePendingTextureDeletes();
    releasePendingUploads();

    const VkResult acquire = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, imageAvailable_, VK_NULL_HANDLE, &currentImage_);
    if (acquire == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain(surface);
        return;
    }
    if (acquire != VK_SUCCESS && acquire != VK_SUBOPTIMAL_KHR) {
        return;
    }

    vkResetCommandBuffer(commandBuffers_[currentImage_], 0);
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if (vkBeginCommandBuffer(commandBuffers_[currentImage_], &beginInfo) != VK_SUCCESS) {
        return;
    }
    frameActive_ = true;
    frameRecorded_ = false;
    renderPassActive_ = false;
    renderTarget_ = RenderTarget::Swapchain;
    renderingToCache_ = false;
    activeLayer_ = nullptr;
    backdropReady_ = false;
    primitiveVertices_.used = 0;
    polygonEdges_.used = 0;
    textVertices_.used = 0;
    imageVertices_.used = 0;
}

void VulkanRenderBackend::present() {
    if (!frameActive_) {
        return;
    }
    core::render::RenderFrameStats& stats = core::render::currentRenderFrameStats();
    stats.backendIncrementalPresentSupported = incrementalPresentSupported_ ? 1 : 0;
    if (!frameRecorded_) {
        recordClearPass(clearColor_);
    }
    endActiveRenderPass();
    transitionSwapchainImage(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    vkEndCommandBuffer(commandBuffers_[currentImage_]);

    const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &imageAvailable_;
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers_[currentImage_];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &renderFinished_;
    vkResetFences(device_, 1, &inFlight_);
    if (vkQueueSubmit(graphicsQueue_, 1, &submitInfo, inFlight_) != VK_SUCCESS) {
        vkDestroyFence(device_, inFlight_, nullptr);
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        vkCreateFence(device_, &fenceInfo, nullptr, &inFlight_);
        frameActive_ = false;
        renderPassActive_ = false;
        return;
    }
    ++core::render::currentRenderFrameStats().backendSubmits;

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderFinished_;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain_;
    presentInfo.pImageIndices = &currentImage_;
    std::vector<VkRectLayerKHR> presentRects;
    VkPresentRegionKHR presentRegion{};
    VkPresentRegionsKHR presentRegions{};
    if (incrementalPresentSupported_ && !presentDirtyRects_.empty()) {
        presentRects.reserve(presentDirtyRects_.size());
        for (const core::Rect& rect : presentDirtyRects_) {
            const VkRect2D vkRect = clampScissor(rect,
                                                 static_cast<int>(swapchainExtent_.width),
                                                 static_cast<int>(swapchainExtent_.height));
            if (vkRect.extent.width == 0 || vkRect.extent.height == 0) {
                continue;
            }
            presentRects.push_back({vkRect.offset, vkRect.extent, 0});
        }
        if (!presentRects.empty()) {
            presentRegion.rectangleCount = static_cast<std::uint32_t>(presentRects.size());
            presentRegion.pRectangles = presentRects.data();
            presentRegions.sType = VK_STRUCTURE_TYPE_PRESENT_REGIONS_KHR;
            presentRegions.swapchainCount = 1;
            presentRegions.pRegions = &presentRegion;
            presentInfo.pNext = &presentRegions;
            ++stats.backendIncrementalPresents;
        }
    }
    if (!presentRects.empty()) {
        std::uint64_t presentPixels = 0;
        for (const VkRectLayerKHR& rect : presentRects) {
            presentPixels += static_cast<std::uint64_t>(rect.extent.width) *
                             static_cast<std::uint64_t>(rect.extent.height);
        }
        stats.backendPresentPixels += presentPixels;
    } else {
        stats.backendPresentPixels += static_cast<std::uint64_t>(swapchainExtent_.width) *
                                      static_cast<std::uint64_t>(swapchainExtent_.height);
    }
    const VkResult presentResult = vkQueuePresentKHR(presentQueue_, &presentInfo);
    ++stats.backendPresents;
    presentDirtyRects_.clear();
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
        swapchainExtent_ = {};
    }
    frameActive_ = false;
    backdropReady_ = false;
}

void VulkanRenderBackend::clear(const core::Color& color) {
    clearColor_ = color;
    if (!frameActive_) {
        return;
    }
    if (!renderPassActive_) {
        recordClearPass(color);
        return;
    }

    VkClearValue clearValue{};
    clearValue.color = {{color.r, color.g, color.b, color.a}};

    VkClearAttachment clearAttachment{};
    clearAttachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    clearAttachment.colorAttachment = 0;
    clearAttachment.clearValue = clearValue;

    const VkExtent2D extent = currentRenderExtent();
    const core::Rect fullRect{0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height)};
    VkClearRect clearRect{};
    clearRect.rect = clampScissor(scissorEnabled_ ? scissorRect_ : fullRect,
                                  static_cast<int>(extent.width),
                                  static_cast<int>(extent.height));
    clearRect.layerCount = 1;
    if (clearRect.rect.extent.width == 0 || clearRect.rect.extent.height == 0) {
        return;
    }
    vkCmdClearAttachments(commandBuffers_[currentImage_], 1, &clearAttachment, 1, &clearRect);
}

void VulkanRenderBackend::setScissor(bool enabled, const core::Rect& rect, int) {
    scissorEnabled_ = enabled;
    scissorRect_ = rect;
}

bool VulkanRenderBackend::createInstance() {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "EUI-NEO";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "EUI-NEO";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    std::vector<const char*> extensions;
#if defined(EUI_WINDOW_BACKEND_SDL3)
    Uint32 sdlExtensionCount = 0;
    const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);
    if (sdlExtensions == nullptr) {
        return false;
    }
    extensions.assign(sdlExtensions, sdlExtensions + sdlExtensionCount);
#elif defined(EUI_WINDOW_BACKEND_SDL2)
    unsigned int sdlExtensionCount = 0;
    if (SDL_Vulkan_GetInstanceExtensions(static_cast<SDL_Window*>(window_), &sdlExtensionCount, nullptr) != SDL_TRUE) {
        return false;
    }
    extensions.resize(sdlExtensionCount);
    if (SDL_Vulkan_GetInstanceExtensions(static_cast<SDL_Window*>(window_), &sdlExtensionCount, extensions.data()) != SDL_TRUE) {
        return false;
    }
#else
    std::uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    if (glfwExtensions == nullptr) {
        const char* description = nullptr;
        const int code = glfwGetError(&description);
        std::fprintf(stderr, "EUI Vulkan: glfwGetRequiredInstanceExtensions failed: %d %s\n",
                     code,
                     description != nullptr ? description : "");
        return false;
    }
    extensions.assign(glfwExtensions, glfwExtensions + glfwExtensionCount);
#endif

    VkInstanceCreateFlags flags = 0;
    const std::vector<VkExtensionProperties> availableExtensions = instanceExtensions();
    if (hasExtension(availableExtensions, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
        addUniqueExtension(extensions, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }
    if (hasExtension(availableExtensions, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME)) {
        addUniqueExtension(extensions, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    }

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.flags = flags;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    const VkResult result = vkCreateInstance(&createInfo, nullptr, &instance_);
    if (result != VK_SUCCESS) {
        std::fprintf(stderr, "EUI Vulkan: vkCreateInstance failed: %d\n", static_cast<int>(result));
        for (const char* extension : extensions) {
            std::fprintf(stderr, "EUI Vulkan: requested instance extension: %s\n", extension);
        }
        return false;
    }
    return true;
}

bool VulkanRenderBackend::createSurface() {
#if defined(EUI_WINDOW_BACKEND_SDL3)
    return SDL_Vulkan_CreateSurface(static_cast<SDL_Window*>(window_), instance_, nullptr, &surface_);
#elif defined(EUI_WINDOW_BACKEND_SDL2)
    return SDL_Vulkan_CreateSurface(static_cast<SDL_Window*>(window_), instance_, &surface_) == SDL_TRUE;
#else
    return glfwCreateWindowSurface(instance_, static_cast<GLFWwindow*>(window_), nullptr, &surface_) == VK_SUCCESS;
#endif
}

bool VulkanRenderBackend::pickDevice() {
    std::uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance_, &deviceCount, nullptr);
    if (deviceCount == 0) {
        return false;
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance_, &deviceCount, devices.data());

    for (VkPhysicalDevice device : devices) {
        std::uint32_t queueCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueCount, nullptr);
        std::vector<VkQueueFamilyProperties> queues(queueCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueCount, queues.data());
        for (std::uint32_t i = 0; i < queueCount; ++i) {
            VkBool32 presentSupported = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &presentSupported);
            if ((queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && presentSupported) {
                physicalDevice_ = device;
                graphicsFamily_ = i;
                presentFamily_ = i;
                return true;
            }
        }
    }
    return false;
}

bool VulkanRenderBackend::createDevice() {
    const float priority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo{};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = graphicsFamily_;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &priority;

    const std::vector<VkExtensionProperties> availableExtensions = deviceExtensions(physicalDevice_);
    std::vector<const char*> extensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    const bool enableIncrementalPresent = platformSupportsIncrementalPresent() &&
                                          hasExtension(availableExtensions, VK_KHR_INCREMENTAL_PRESENT_EXTENSION_NAME);
    if (enableIncrementalPresent) {
        extensions.push_back(VK_KHR_INCREMENTAL_PRESENT_EXTENSION_NAME);
    }
    if (hasExtension(availableExtensions, "VK_KHR_portability_subset")) {
        extensions.push_back("VK_KHR_portability_subset");
    }

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pQueueCreateInfos = &queueInfo;
    createInfo.enabledExtensionCount = static_cast<std::uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    if (vkCreateDevice(physicalDevice_, &createInfo, nullptr, &device_) != VK_SUCCESS) {
        return false;
    }
    vkGetDeviceQueue(device_, graphicsFamily_, 0, &graphicsQueue_);
    presentQueue_ = graphicsQueue_;
    incrementalPresentSupported_ = enableIncrementalPresent;
    return true;
}

bool VulkanRenderBackend::recreateSwapchain(const RenderSurface& surface) {
    vkDeviceWaitIdle(device_);
    destroySwapchain();

    VkSurfaceCapabilitiesKHR capabilities{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice_, surface_, &capabilities);

    std::uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    if (formatCount > 0) {
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount, formats.data());
    }

    std::uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice_, surface_, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    if (presentModeCount > 0) {
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice_, surface_, &presentModeCount, presentModes.data());
    }

    const VkSurfaceFormatKHR surfaceFormat = chooseSurfaceFormat(formats);
    const VkPresentModeKHR presentMode = choosePresentMode(presentModes);
    VkExtent2D extent{};
    if (capabilities.currentExtent.width != UINT32_MAX) {
        extent = capabilities.currentExtent;
    } else {
        extent.width = std::clamp(static_cast<std::uint32_t>(surface.framebufferWidth),
                                  capabilities.minImageExtent.width,
                                  capabilities.maxImageExtent.width);
        extent.height = std::clamp(static_cast<std::uint32_t>(surface.framebufferHeight),
                                   capabilities.minImageExtent.height,
                                   capabilities.maxImageExtent.height);
    }

    std::uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0) {
        imageCount = std::min(imageCount, capabilities.maxImageCount);
    }

    VkSwapchainCreateInfoKHR swapchainInfo{};
    swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainInfo.surface = surface_;
    swapchainInfo.minImageCount = imageCount;
    swapchainInfo.imageFormat = surfaceFormat.format;
    swapchainInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapchainInfo.imageExtent = extent;
    swapchainInfo.imageArrayLayers = 1;
    swapchainTransferSrcSupported_ = (capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0;
    swapchainTransferDstSupported_ = (capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) != 0;
    swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (swapchainTransferSrcSupported_) {
        swapchainInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    }
    if (swapchainTransferDstSupported_) {
        swapchainInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }
    swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainInfo.preTransform = capabilities.currentTransform;
    swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainInfo.presentMode = presentMode;
    swapchainInfo.clipped = VK_TRUE;
    if (vkCreateSwapchainKHR(device_, &swapchainInfo, nullptr, &swapchain_) != VK_SUCCESS) {
        return false;
    }

    swapchainFormat_ = surfaceFormat.format;
    swapchainExtent_ = extent;
    vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, nullptr);
    swapchainImages_.resize(imageCount);
    vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, swapchainImages_.data());
    swapchainImageLayouts_.assign(swapchainImages_.size(), VK_IMAGE_LAYOUT_UNDEFINED);
    swapchainImageCacheGenerations_.assign(swapchainImages_.size(), 0);

    swapchainImageViews_.resize(swapchainImages_.size());
    for (std::size_t i = 0; i < swapchainImages_.size(); ++i) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = swapchainImages_[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = swapchainFormat_;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;
        if (vkCreateImageView(device_, &viewInfo, nullptr, &swapchainImageViews_[i]) != VK_SUCCESS) {
            return false;
        }
    }

    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapchainFormat_;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    if (vkCreateRenderPass(device_, &renderPassInfo, nullptr, &renderPass_) != VK_SUCCESS) {
        return false;
    }
    if (!ensureRoundedRectPipeline()) {
        return false;
    }

    framebuffers_.resize(swapchainImageViews_.size());
    for (std::size_t i = 0; i < swapchainImageViews_.size(); ++i) {
        VkImageView attachments[] = {swapchainImageViews_[i]};
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass_;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapchainExtent_.width;
        framebufferInfo.height = swapchainExtent_.height;
        framebufferInfo.layers = 1;
        if (vkCreateFramebuffer(device_, &framebufferInfo, nullptr, &framebuffers_[i]) != VK_SUCCESS) {
            return false;
        }
    }

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = graphicsFamily_;
    if (vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool_) != VK_SUCCESS) {
        return false;
    }

    commandBuffers_.resize(framebuffers_.size());
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool_;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<std::uint32_t>(commandBuffers_.size());
    if (vkAllocateCommandBuffers(device_, &allocInfo, commandBuffers_.data()) != VK_SUCCESS) {
        return false;
    }

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    return vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &imageAvailable_) == VK_SUCCESS &&
           vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &renderFinished_) == VK_SUCCESS &&
           vkCreateFence(device_, &fenceInfo, nullptr, &inFlight_) == VK_SUCCESS;
}

void VulkanRenderBackend::recordClearPass(const core::Color& color) {
    if (renderTarget_ == RenderTarget::RenderCache) {
        transitionRenderCacheImage(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    } else if (renderTarget_ == RenderTarget::Layer) {
        if (activeLayer_ == nullptr) {
            return;
        }
        transitionLayerImage(*activeLayer_, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    } else {
        transitionSwapchainImage(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    }
    initializeBackdropImageIfNeeded();

    beginLoadPass();
    if (!renderPassActive_) {
        return;
    }

    VkClearValue clearValue{};
    clearValue.color = {{color.r, color.g, color.b, color.a}};

    VkClearAttachment clearAttachment{};
    clearAttachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    clearAttachment.colorAttachment = 0;
    clearAttachment.clearValue = clearValue;

    VkClearRect clearRect{};
    const VkExtent2D extent = currentRenderExtent();
    const core::Rect fullRect{0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height)};
    clearRect.rect = clampScissor(scissorEnabled_ ? scissorRect_ : fullRect,
                                  static_cast<int>(extent.width),
                                  static_cast<int>(extent.height));
    clearRect.layerCount = 1;
    if (clearRect.rect.extent.width == 0 || clearRect.rect.extent.height == 0) {
        return;
    }
    vkCmdClearAttachments(commandBuffers_[currentImage_], 1, &clearAttachment, 1, &clearRect);
}

void VulkanRenderBackend::beginLoadPass() {
    if (!frameActive_ || renderPassActive_ || renderPass_ == VK_NULL_HANDLE || framebuffers_.empty()) {
        return;
    }

    if (renderTarget_ == RenderTarget::RenderCache) {
        if (renderCacheFramebuffer_ == VK_NULL_HANDLE || renderCacheExtent_.width == 0 || renderCacheExtent_.height == 0) {
            return;
        }
        transitionRenderCacheImage(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    } else if (renderTarget_ == RenderTarget::Layer) {
        if (activeLayer_ == nullptr || activeLayer_->framebuffer == VK_NULL_HANDLE ||
            activeLayer_->extent.width == 0 || activeLayer_->extent.height == 0) {
            return;
        }
        transitionLayerImage(*activeLayer_, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    } else {
        transitionSwapchainImage(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    }

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass_;
    renderPassInfo.framebuffer = currentFramebuffer();
    renderPassInfo.renderArea = currentRenderArea();

    vkCmdBeginRenderPass(commandBuffers_[currentImage_], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    renderPassActive_ = true;
    frameRecorded_ = true;
    core::render::RenderFrameStats& stats = core::render::currentRenderFrameStats();
    ++stats.backendRenderPasses;
    stats.backendRenderPassPixels += static_cast<std::uint64_t>(renderPassInfo.renderArea.extent.width) *
                                     static_cast<std::uint64_t>(renderPassInfo.renderArea.extent.height);
}

VkExtent2D VulkanRenderBackend::currentRenderExtent() const {
    if (renderTarget_ == RenderTarget::RenderCache) {
        return renderCacheExtent_;
    }
    if (renderTarget_ == RenderTarget::Layer && activeLayer_ != nullptr) {
        return activeLayer_->extent;
    }
    return swapchainExtent_;
}

VkRect2D VulkanRenderBackend::currentRenderArea() const {
    const VkExtent2D extent = currentRenderExtent();
    if (renderTarget_ == RenderTarget::RenderCache && cacheRenderArea_.width > 0.0f && cacheRenderArea_.height > 0.0f) {
        return clampScissor(cacheRenderArea_, static_cast<int>(extent.width), static_cast<int>(extent.height));
    }
    return {{0, 0}, extent};
}

VkFramebuffer VulkanRenderBackend::currentFramebuffer() const {
    if (renderTarget_ == RenderTarget::RenderCache) {
        return renderCacheFramebuffer_;
    }
    if (renderTarget_ == RenderTarget::Layer && activeLayer_ != nullptr) {
        return activeLayer_->framebuffer;
    }
    return framebuffers_[currentImage_];
}

VkImage VulkanRenderBackend::currentRenderImage() const {
    if (renderTarget_ == RenderTarget::RenderCache) {
        return renderCacheImage_;
    }
    if (renderTarget_ == RenderTarget::Layer && activeLayer_ != nullptr) {
        return activeLayer_->texture.image;
    }
    if (currentImage_ < swapchainImages_.size()) {
        return swapchainImages_[currentImage_];
    }
    return VK_NULL_HANDLE;
}

VkImageLayout VulkanRenderBackend::currentRenderImageLayout() const {
    if (renderTarget_ == RenderTarget::RenderCache) {
        return renderCacheLayout_;
    }
    if (renderTarget_ == RenderTarget::Layer && activeLayer_ != nullptr) {
        return activeLayer_->texture.layout;
    }
    if (currentImage_ < swapchainImageLayouts_.size()) {
        return swapchainImageLayouts_[currentImage_];
    }
    return VK_IMAGE_LAYOUT_UNDEFINED;
}

void VulkanRenderBackend::setCurrentRenderImageLayout(VkImageLayout layout) {
    if (renderTarget_ == RenderTarget::RenderCache) {
        renderCacheLayout_ = layout;
    } else if (renderTarget_ == RenderTarget::Layer && activeLayer_ != nullptr) {
        activeLayer_->texture.layout = layout;
    } else if (currentImage_ < swapchainImageLayouts_.size()) {
        swapchainImageLayouts_[currentImage_] = layout;
    }
}

void VulkanRenderBackend::destroySwapchain() {
    if (device_ == VK_NULL_HANDLE) {
        return;
    }
    frameActive_ = false;
    frameRecorded_ = false;
    renderPassActive_ = false;
    renderTarget_ = RenderTarget::Swapchain;
    destroyRoundedRectPipeline();
    destroyPolygonPipeline();
    destroyBackdropResources();
    destroyTextPipeline();
    destroyTextResources();
    destroyImagePipeline();
    destroyImageResources();
    releaseAllLayerFramebuffers();
    destroyRenderCacheResolvePipeline();
    destroyRenderCacheResources();
    releasePendingTextureDeletes();
    releasePendingUploads();
    if (inFlight_ != VK_NULL_HANDLE) {
        vkDestroyFence(device_, inFlight_, nullptr);
        inFlight_ = VK_NULL_HANDLE;
    }
    if (renderFinished_ != VK_NULL_HANDLE) {
        vkDestroySemaphore(device_, renderFinished_, nullptr);
        renderFinished_ = VK_NULL_HANDLE;
    }
    if (imageAvailable_ != VK_NULL_HANDLE) {
        vkDestroySemaphore(device_, imageAvailable_, nullptr);
        imageAvailable_ = VK_NULL_HANDLE;
    }
    if (commandPool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device_, commandPool_, nullptr);
        commandPool_ = VK_NULL_HANDLE;
    }
    for (VkFramebuffer framebuffer : framebuffers_) {
        vkDestroyFramebuffer(device_, framebuffer, nullptr);
    }
    framebuffers_.clear();
    if (renderPass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device_, renderPass_, nullptr);
        renderPass_ = VK_NULL_HANDLE;
    }
    for (VkImageView view : swapchainImageViews_) {
        vkDestroyImageView(device_, view, nullptr);
    }
    swapchainImageViews_.clear();
    if (swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device_, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
    commandBuffers_.clear();
    swapchainImages_.clear();
    swapchainImageLayouts_.clear();
    swapchainImageCacheGenerations_.clear();
    swapchainTransferSrcSupported_ = false;
    swapchainTransferDstSupported_ = false;
    backdropReady_ = false;
    renderingToCache_ = false;
    activeLayer_ = nullptr;
}

void VulkanRenderBackend::destroy() {
    if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
    }
    destroySwapchain();
    releaseAllLayerResources();
    for (LayerResource* layer : layers_) {
        delete layer;
    }
    layers_.clear();
    destroyPrimitiveVertexBuffer();
    destroyPolygonEdgeBuffer();
    for (VkDescriptorPool pool : imageDescriptorPools_) {
        if (pool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(device_, pool, nullptr);
        }
    }
    imageDescriptorPools_.clear();
    imageDescriptorPool_ = VK_NULL_HANDLE;
    imageDescriptorPoolUsed_ = 0;
    imageDescriptorPoolCapacity_ = 0;
    if (imageDescriptorSetLayout_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device_, imageDescriptorSetLayout_, nullptr);
        imageDescriptorSetLayout_ = VK_NULL_HANDLE;
    }
    releasePendingUploads();
    destroyUploadBuffer();
    if (device_ != VK_NULL_HANDLE) {
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }
    if (surface_ != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        surface_ = VK_NULL_HANDLE;
    }
    if (instance_ != VK_NULL_HANDLE) {
        vkDestroyInstance(instance_, nullptr);
        instance_ = VK_NULL_HANDLE;
    }
    initialized_ = false;
}

void VulkanRenderBackend::destroyTextureResource(TextureResource& texture) {
    if (device_ == VK_NULL_HANDLE) {
        texture = {};
        return;
    }
    if (texture.descriptorPool != VK_NULL_HANDLE && texture.descriptorSet != VK_NULL_HANDLE) {
        vkFreeDescriptorSets(device_, texture.descriptorPool, 1, &texture.descriptorSet);
        texture.descriptorSet = VK_NULL_HANDLE;
        texture.descriptorPool = VK_NULL_HANDLE;
    }
    if (texture.sampler != VK_NULL_HANDLE) {
        vkDestroySampler(device_, texture.sampler, nullptr);
        texture.sampler = VK_NULL_HANDLE;
    }
    if (texture.view != VK_NULL_HANDLE) {
        vkDestroyImageView(device_, texture.view, nullptr);
        texture.view = VK_NULL_HANDLE;
    }
    if (texture.image != VK_NULL_HANDLE) {
        vkDestroyImage(device_, texture.image, nullptr);
        texture.image = VK_NULL_HANDLE;
    }
    if (texture.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device_, texture.memory, nullptr);
        texture.memory = VK_NULL_HANDLE;
    }
    texture = {};
}

bool VulkanRenderBackend::createTargetImage(TextureResource& texture,
                                            int width,
                                            int height,
                                            VkFormat format,
                                            VkImageUsageFlags usage) {
    if (device_ == VK_NULL_HANDLE || width <= 0 || height <= 0 || format == VK_FORMAT_UNDEFINED) {
        return false;
    }

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
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

    texture.width = width;
    texture.height = height;
    texture.channels = 4;
    texture.format = format;
    texture.layout = VK_IMAGE_LAYOUT_UNDEFINED;
    ++texture.generation;
    return true;
}

bool VulkanRenderBackend::ensureTextureSampler(TextureResource& texture) {
    if (texture.sampler != VK_NULL_HANDLE) {
        return true;
    }
    if (device_ == VK_NULL_HANDLE) {
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
    return vkCreateSampler(device_, &samplerInfo, nullptr, &texture.sampler) == VK_SUCCESS;
}

bool VulkanRenderBackend::ensureLayerResource(LayerResource& layer, int width, int height) {
    if (width <= 0 || height <= 0 || renderPass_ == VK_NULL_HANDLE || swapchainFormat_ == VK_FORMAT_UNDEFINED) {
        return false;
    }
    const auto targetWidth = static_cast<std::uint32_t>(width);
    const auto targetHeight = static_cast<std::uint32_t>(height);
    const bool textureMatches = layer.texture.image != VK_NULL_HANDLE &&
                                layer.texture.view != VK_NULL_HANDLE &&
                                layer.texture.width == width &&
                                layer.texture.height == height &&
                                layer.texture.format == swapchainFormat_;
    if (!textureMatches) {
        destroyLayerResource(layer);
    }

    if (layer.texture.image == VK_NULL_HANDLE) {
        const VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                        VK_IMAGE_USAGE_SAMPLED_BIT |
                                        VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                        VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        if (!createTargetImage(layer.texture, width, height, swapchainFormat_, usage)) {
            destroyLayerResource(layer);
            return false;
        }
    }
    if (!ensureTextureSampler(layer.texture)) {
        destroyLayerResource(layer);
        return false;
    }
    if (layer.framebuffer != VK_NULL_HANDLE &&
        layer.extent.width == targetWidth &&
        layer.extent.height == targetHeight) {
        return true;
    }
    if (layer.framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device_, layer.framebuffer, nullptr);
        layer.framebuffer = VK_NULL_HANDLE;
    }

    VkImageView attachments[] = {layer.texture.view};
    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = renderPass_;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = attachments;
    framebufferInfo.width = targetWidth;
    framebufferInfo.height = targetHeight;
    framebufferInfo.layers = 1;
    if (vkCreateFramebuffer(device_, &framebufferInfo, nullptr, &layer.framebuffer) != VK_SUCCESS) {
        destroyLayerResource(layer);
        return false;
    }

    layer.extent = {targetWidth, targetHeight};
    return true;
}

void VulkanRenderBackend::destroyLayerResource(LayerResource& layer) {
    if (device_ != VK_NULL_HANDLE && layer.framebuffer != VK_NULL_HANDLE) {
        vkDestroyFramebuffer(device_, layer.framebuffer, nullptr);
    }
    layer.framebuffer = VK_NULL_HANDLE;
    layer.extent = {};
    destroyTextureResource(layer.texture);
}

void VulkanRenderBackend::releaseAllLayerFramebuffers() {
    activeLayer_ = nullptr;
    previousLayerTarget_ = RenderTarget::Swapchain;
    for (LayerResource* layer : layers_) {
        if (layer != nullptr && layer->framebuffer != VK_NULL_HANDLE) {
            vkDestroyFramebuffer(device_, layer->framebuffer, nullptr);
            layer->framebuffer = VK_NULL_HANDLE;
            layer->extent = {};
        }
    }
}

void VulkanRenderBackend::releaseAllLayerResources() {
    activeLayer_ = nullptr;
    previousLayerTarget_ = RenderTarget::Swapchain;
    for (LayerResource* layer : layers_) {
        if (layer != nullptr) {
            destroyLayerResource(*layer);
        }
    }
}

bool VulkanRenderBackend::createUploadBuffer(VkDeviceSize capacity) {
    if (device_ == VK_NULL_HANDLE || capacity == 0) {
        return false;
    }

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = capacity;
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device_, &bufferInfo, nullptr, &uploadArena_.buffer) != VK_SUCCESS) {
        return false;
    }

    VkMemoryRequirements memoryRequirements{};
    vkGetBufferMemoryRequirements(device_, uploadArena_.buffer, &memoryRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memoryRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memoryRequirements.memoryTypeBits,
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (allocInfo.memoryTypeIndex == std::numeric_limits<std::uint32_t>::max() ||
        vkAllocateMemory(device_, &allocInfo, nullptr, &uploadArena_.memory) != VK_SUCCESS ||
        vkBindBufferMemory(device_, uploadArena_.buffer, uploadArena_.memory, 0) != VK_SUCCESS ||
        vkMapMemory(device_, uploadArena_.memory, 0, capacity, 0, &uploadArena_.mapped) != VK_SUCCESS) {
        destroyUploadBuffer();
        return false;
    }

    uploadArena_.capacity = capacity;
    uploadArena_.used = 0;
    return true;
}

bool VulkanRenderBackend::allocateUploadRegion(VkDeviceSize size, VkBuffer& buffer, VkDeviceSize& offset, void*& mapped) {
    if (device_ == VK_NULL_HANDLE || size == 0) {
        return false;
    }

    constexpr VkDeviceSize kUploadAlignment = 256;
    constexpr VkDeviceSize kInitialUploadCapacity = 1ull * 1024ull * 1024ull;
    const auto alignUp = [](VkDeviceSize value, VkDeviceSize alignment) {
        return (value + alignment - 1) & ~(alignment - 1);
    };

    VkDeviceSize alignedUsed = alignUp(uploadArena_.used, kUploadAlignment);
    if (uploadArena_.buffer == VK_NULL_HANDLE || alignedUsed + size > uploadArena_.capacity) {
        if (uploadArena_.buffer != VK_NULL_HANDLE) {
            if (uploadArena_.mapped != nullptr) {
                vkUnmapMemory(device_, uploadArena_.memory);
                uploadArena_.mapped = nullptr;
            }
            uploadArena_.pendingBuffers.push_back(uploadArena_.buffer);
            uploadArena_.pendingMemories.push_back(uploadArena_.memory);
            uploadArena_.buffer = VK_NULL_HANDLE;
            uploadArena_.memory = VK_NULL_HANDLE;
            uploadArena_.capacity = 0;
            uploadArena_.used = 0;
        }

        VkDeviceSize capacity = kInitialUploadCapacity;
        while (capacity < size) {
            capacity *= 2;
        }
        if (!createUploadBuffer(capacity)) {
            return false;
        }
        alignedUsed = 0;
    }

    buffer = uploadArena_.buffer;
    offset = alignedUsed;
    mapped = static_cast<unsigned char*>(uploadArena_.mapped) + static_cast<std::size_t>(offset);
    uploadArena_.used = alignedUsed + size;
    return mapped != nullptr;
}

void VulkanRenderBackend::destroyUploadBuffer() {
    if (device_ == VK_NULL_HANDLE) {
        uploadArena_.buffer = VK_NULL_HANDLE;
        uploadArena_.memory = VK_NULL_HANDLE;
        uploadArena_.mapped = nullptr;
        uploadArena_.capacity = 0;
        uploadArena_.used = 0;
        return;
    }
    if (uploadArena_.memory != VK_NULL_HANDLE && uploadArena_.mapped != nullptr) {
        vkUnmapMemory(device_, uploadArena_.memory);
        uploadArena_.mapped = nullptr;
    }
    if (uploadArena_.buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, uploadArena_.buffer, nullptr);
        uploadArena_.buffer = VK_NULL_HANDLE;
    }
    if (uploadArena_.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device_, uploadArena_.memory, nullptr);
        uploadArena_.memory = VK_NULL_HANDLE;
    }
    uploadArena_.capacity = 0;
    uploadArena_.used = 0;
}

void VulkanRenderBackend::releasePendingTextureDeletes() {
    if (device_ == VK_NULL_HANDLE) {
        for (TextureResource* texture : pendingTextureDeletes_) {
            delete texture;
        }
        pendingTextureDeletes_.clear();
        return;
    }
    for (TextureResource* texture : pendingTextureDeletes_) {
        if (texture != nullptr) {
            destroyTextureResource(*texture);
            delete texture;
        }
    }
    pendingTextureDeletes_.clear();
}

void VulkanRenderBackend::releasePendingUploads() {
    if (device_ == VK_NULL_HANDLE) {
        uploadArena_.pendingBuffers.clear();
        uploadArena_.pendingMemories.clear();
        return;
    }
    for (VkBuffer buffer : uploadArena_.pendingBuffers) {
        if (buffer != VK_NULL_HANDLE) {
            vkDestroyBuffer(device_, buffer, nullptr);
        }
    }
    for (VkDeviceMemory memory : uploadArena_.pendingMemories) {
        if (memory != VK_NULL_HANDLE) {
            vkFreeMemory(device_, memory, nullptr);
        }
    }
    uploadArena_.pendingBuffers.clear();
    uploadArena_.pendingMemories.clear();
    uploadArena_.used = 0;
    destroyUploadBuffer();
}

void VulkanRenderBackend::transitionSwapchainImage(VkImageLayout newLayout) {
    if (commandBuffers_.empty() || swapchainImages_.empty() || swapchainImageLayouts_.empty() ||
        currentImage_ >= swapchainImages_.size()) {
        return;
    }
    const VkImageLayout oldLayout = swapchainImageLayouts_[currentImage_];
    transitionImageLayout(commandBuffers_[currentImage_], swapchainImages_[currentImage_], oldLayout, newLayout);
    swapchainImageLayouts_[currentImage_] = newLayout;
}

std::uint32_t VulkanRenderBackend::findMemoryType(std::uint32_t filter, VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memoryProperties);
    for (std::uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i) {
        if ((filter & (1u << i)) &&
            (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return std::numeric_limits<std::uint32_t>::max();
}

} // namespace core::render::vulkan
