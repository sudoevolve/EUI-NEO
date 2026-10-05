#include "eui/image.h"
#include "core/dsl_runtime.h"
#include "core/render/render_backend.h"
#include "core/window/window_backend.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#if defined(EUI_RENDER_BACKEND_OPENGL)
#include <glad/glad.h>
#else
#include <vulkan/vulkan.h>
#endif
#if defined(EUI_WINDOW_BACKEND_SDL2)
#define SDL_MAIN_HANDLED
#include <SDL.h>
#else
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif
#endif

#include <chrono>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <unordered_set>

#if defined(EUI_RENDER_BACKEND_VULKAN)
namespace {
struct SwapchainProbe {
    std::unordered_set<VkSwapchainKHR> live;
    unsigned int handoffs = 0;
    unsigned int faults = 0;
    bool invalidHandoff = false;
    bool failCreate = false;
    int framebuffersBeforeFailure = -1;
} swapchainProbe;
}

// 通过设备分发表转发真实调用，仅在测试中注入失败并检查交换链所有权。
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateSwapchainKHR(VkDevice device,
    const VkSwapchainCreateInfoKHR* info, const VkAllocationCallbacks* allocator, VkSwapchainKHR* output) {
    if (info->oldSwapchain != VK_NULL_HANDLE) {
        ++swapchainProbe.handoffs;
        if (!swapchainProbe.live.count(info->oldSwapchain)) swapchainProbe.invalidHandoff = true;
    } else if (!swapchainProbe.live.empty()) {
        swapchainProbe.invalidHandoff = true;
    }
    if (swapchainProbe.failCreate) {
        swapchainProbe.failCreate = false;
        ++swapchainProbe.faults;
        *output = VK_NULL_HANDLE;
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    const auto call = reinterpret_cast<PFN_vkCreateSwapchainKHR>(vkGetDeviceProcAddr(device, "vkCreateSwapchainKHR"));
    const auto result = call(device, info, allocator, output);
    if (result == VK_SUCCESS) swapchainProbe.live.insert(*output);
    return result;
}

extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroySwapchainKHR(VkDevice device,
    VkSwapchainKHR swapchain, const VkAllocationCallbacks* allocator) {
    if (swapchain != VK_NULL_HANDLE && swapchainProbe.live.erase(swapchain) != 1) {
        swapchainProbe.invalidHandoff = true;
    }
    const auto call = reinterpret_cast<PFN_vkDestroySwapchainKHR>(vkGetDeviceProcAddr(device, "vkDestroySwapchainKHR"));
    call(device, swapchain, allocator);
}

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateFramebuffer(VkDevice device,
    const VkFramebufferCreateInfo* info, const VkAllocationCallbacks* allocator, VkFramebuffer* output) {
    if (swapchainProbe.framebuffersBeforeFailure == 0) {
        swapchainProbe.framebuffersBeforeFailure = -1;
        ++swapchainProbe.faults;
        *output = VK_NULL_HANDLE;
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    if (swapchainProbe.framebuffersBeforeFailure > 0) --swapchainProbe.framebuffersBeforeFailure;
    const auto call = reinterpret_cast<PFN_vkCreateFramebuffer>(vkGetDeviceProcAddr(device, "vkCreateFramebuffer"));
    return call(device, info, allocator, output);
}
#endif

namespace {
#ifdef _WIN32
HWND nativeWindow(core::window::Handle window) {
#if defined(EUI_WINDOW_BACKEND_SDL2)
    return static_cast<HWND>(core::window::nativeWindowInfo(window).platformWindow);
#else
    return glfwGetWin32Window(static_cast<GLFWwindow*>(window));
#endif
}
#endif
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct NativeImage {
    eui::GpuDeviceInfo device;
    int* released = nullptr;
#if defined(EUI_RENDER_BACKEND_OPENGL)
    GLuint texture = 0;
    ~NativeImage() { if (texture) glDeleteTextures(1, &texture); if (released) ++*released; }
    void fill(bool green) {
        GLint previous = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous);
        GLuint framebuffer = 0;
        glGenFramebuffers(1, &framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
        require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "external FBO incomplete");
        glDisable(GL_SCISSOR_TEST);
        glClearColor(green ? 0.f : 1.f, green ? 1.f : 0.f, 0.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT);
        glBindFramebuffer(GL_FRAMEBUFFER, previous);
        glDeleteFramebuffers(1, &framebuffer);
    }
#else
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    bool initialized = false;
    VkDevice vkDevice() const { return reinterpret_cast<VkDevice>(device.device); }
    ~NativeImage() {
        if (view) vkDestroyImageView(vkDevice(), view, nullptr);
        if (image) vkDestroyImage(vkDevice(), image, nullptr);
        if (memory) vkFreeMemory(vkDevice(), memory, nullptr);
        if (released) ++*released;
    }
    void fill(bool green) {
        const auto vk = vkDevice();
        VkCommandPool pool = VK_NULL_HANDLE;
        VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        poolInfo.queueFamilyIndex = device.graphicsQueueFamily;
        require(vkCreateCommandPool(vk, &poolInfo, nullptr, &pool) == VK_SUCCESS, "producer command pool");
        VkCommandBufferAllocateInfo alloc{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        alloc.commandPool = pool;
        alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc.commandBufferCount = 1;
        VkCommandBuffer command = VK_NULL_HANDLE;
        require(vkAllocateCommandBuffers(vk, &alloc, &command) == VK_SUCCESS, "producer command buffer");
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        require(vkBeginCommandBuffer(command, &begin) == VK_SUCCESS, "producer begin");
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.srcAccessMask = initialized ? VK_ACCESS_SHADER_READ_BIT : 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.oldLayout = initialized ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdPipelineBarrier(command, initialized ? VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        const VkClearColorValue color{{green ? 0.f : 1.f, green ? 1.f : 0.f, 0.f, 1.f}};
        vkCmdClearColorImage(command, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color, 1, &barrier.subresourceRange);
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
        require(vkEndCommandBuffer(command) == VK_SUCCESS, "producer end");
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &command;
        const auto queue = reinterpret_cast<VkQueue>(device.graphicsQueue);
        require(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE) == VK_SUCCESS, "producer submit");
        // command pool 在函数返回时销毁，回收前须等待其提交的命令执行完成。
        require(vkQueueWaitIdle(queue) == VK_SUCCESS, "producer wait");
        vkDestroyCommandPool(vk, pool, nullptr);
        initialized = true;
    }
#endif
};

std::shared_ptr<NativeImage> createNative(const eui::GpuDeviceInfo& device, int size, int& released) {
    auto native = std::make_shared<NativeImage>();
    native->device = device;
    native->released = &released;
#if defined(EUI_RENDER_BACKEND_OPENGL)
    glGenTextures(1, &native->texture);
    glBindTexture(GL_TEXTURE_2D, native->texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size, size, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
#else
    VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = VK_FORMAT_R8G8B8A8_UNORM;
    info.extent = {static_cast<std::uint32_t>(size), static_cast<std::uint32_t>(size), 1};
    info.mipLevels = info.arrayLayers = 1;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    require(vkCreateImage(native->vkDevice(), &info, nullptr, &native->image) == VK_SUCCESS, "external image creation");
    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(native->vkDevice(), native->image, &requirements);
    VkPhysicalDeviceMemoryProperties properties{};
    vkGetPhysicalDeviceMemoryProperties(reinterpret_cast<VkPhysicalDevice>(device.physicalDevice), &properties);
    std::uint32_t type = 0;
    while (type < properties.memoryTypeCount && !(requirements.memoryTypeBits & (1u << type))) ++type;
    require(type < properties.memoryTypeCount, "external memory type");
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = type;
    require(vkAllocateMemory(native->vkDevice(), &allocation, nullptr, &native->memory) == VK_SUCCESS, "external memory");
    require(vkBindImageMemory(native->vkDevice(), native->image, native->memory, 0) == VK_SUCCESS, "external bind");
    VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view.image = native->image;
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = info.format;
    view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    require(vkCreateImageView(native->vkDevice(), &view, nullptr, &native->view) == VK_SUCCESS, "external view");
#endif
    native->fill(false);
    return native;
}

std::shared_ptr<const eui::GpuImage> importNative(const std::shared_ptr<NativeImage>& native, int size) {
    eui::GpuImageDescriptor descriptor{native->device, size, size};
#if defined(EUI_RENDER_BACKEND_OPENGL)
    descriptor.texture = native->texture;
#else
    std::memcpy(&descriptor.imageView, &native->view, sizeof(native->view));
#endif
    auto result = eui::image::importGpuImage(descriptor, native);
    require(result != nullptr, "external import rejected");
    ++descriptor.device.identity;
    require(!eui::image::importGpuImage(descriptor, native), "wrong device accepted");
    return result;
}

void pump() {
#if defined(EUI_WINDOW_BACKEND_SDL2)
    SDL_Event event;
    while (SDL_PollEvent(&event)) {}
#else
    glfwPollEvents();
#endif
}

void checkWindowPixel(core::window::Handle window, bool green) {
#if defined(_WIN32) && defined(EUI_RENDER_BACKEND_VULKAN)
    const auto hwnd = nativeWindow(window);
    COLORREF pixel = CLR_INVALID;
    for (int attempt = 0; attempt < 30; ++attempt) {
        pump();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        HDC dc = GetDC(hwnd);
        HDC capture = CreateCompatibleDC(dc);
        HBITMAP bitmap = CreateCompatibleBitmap(dc, 256, 256);
        HGDIOBJ previous = SelectObject(capture, bitmap);
        PrintWindow(hwnd, capture, PW_CLIENTONLY | 2);
        pixel = GetPixel(capture, 128, 128);
        SelectObject(capture, previous);
        DeleteObject(bitmap);
        DeleteDC(capture);
        ReleaseDC(hwnd, dc);
        if ((green ? GetGValue(pixel) : GetRValue(pixel)) > 220 &&
            (green ? GetRValue(pixel) : GetGValue(pixel)) < 30 && GetBValue(pixel) < 30) return;
    }
    std::cerr << "Pixel: " << static_cast<int>(GetRValue(pixel)) << ',' << static_cast<int>(GetGValue(pixel)) << ',' << static_cast<int>(GetBValue(pixel)) << " HWND " << hwnd << '\n';
    require(false, "displayed external texture pixel mismatch");
#else
    (void)window;
    (void)green;
#if defined(EUI_RENDER_BACKEND_VULKAN)
    std::cout << "Vulkan window pixel capture requires Windows\n";
#endif
#endif
}
}

int main() {
    core::render::initializeRenderBackendLoader();
#if defined(EUI_WINDOW_BACKEND_SDL2)
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) return 1;
#else
    if (!glfwInit()) return 1;
#endif
    core::window::WindowCreateRequest request;
    request.width = request.height = 256;
    request.highDpi = false;
    request.title = "External GPU Image Probe";
    request.renderApi = core::render::windowRenderApi();
    auto window = core::window::createWindow(request);
    auto backend = core::render::createRenderBackend(window);
    int result = 0;
    int released = 0;
    try {
        require(window && backend && backend->initialize(), "backend initialization");
#ifdef _WIN32
        const auto hwnd = nativeWindow(window);
        ShowWindow(hwnd, SW_SHOW);
        SetWindowPos(hwnd, HWND_TOPMOST, 100, 100, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
        SetForegroundWindow(hwnd);
#endif
        core::render::ScopedRenderBackend scope(*backend);
        core::dsl::Runtime runtime;
        require(runtime.initialize(window), "runtime initialization");
        auto native = createNative(eui::image::gpuDevice(), 8, released);
        auto image = importNative(native, 8);
        int extent = 256;
        auto frame = [&](const std::shared_ptr<const eui::GpuImage>& source, std::uint64_t revision) {
            backend->makeCurrent();
            runtime.compose("external", float(extent), float(extent), [&](core::dsl::Ui& ui, const core::dsl::Screen&) {
                ui.image("external.image").size(float(extent), float(extent)).texture(source, revision).stretch().build();
            });
            runtime.update(window, 0.f, 1.f, 1.f, false);
            backend->beginFrame({window, core::window::nativeWindowInfo(window), extent, extent, 1.f});
            runtime.render(extent, extent, 1.f, {0, 0, 0, 1});
#if defined(EUI_RENDER_BACKEND_OPENGL)
            if (source && released == 0) {
                unsigned char pixel[4]{};
                glReadPixels(128, 128, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
                const bool green = revision == 1;
                require(pixel[green ? 1 : 0] > 220 && pixel[green ? 0 : 1] < 30 && pixel[2] < 30,
                    "GL framebuffer pixel mismatch");
            }
#endif
            backend->present();
            pump();
        };
#if defined(EUI_RENDER_BACKEND_OPENGL)
        {
            auto stream = std::make_shared<eui::ImageStream>();
            const auto pixels = std::make_shared<const std::vector<std::uint8_t>>(
                std::vector<std::uint8_t>{255, 0, 255, 255, 255, 0, 255, 255,
                                          255, 0, 255, 255, 255, 0, 255, 255});
            eui::ImageFrame sourceFrame{pixels, 2, 2, 8, eui::ImagePixelFormat::RGBA8, 1};
            require(stream->submit(sourceFrame), "RGBA ImageStream frame submission");
            const auto consumedFrame = stream->consumeLatest();
            require(consumedFrame.has_value(), "RGBA ImageStream frame consumption");
            std::vector<std::uint8_t> rgba;
            require(consumedFrame->convertToRgba8(rgba), "RGBA ImageStream conversion");

            GLint previousBuffer = 0, previousAlignment = 0, previousRowLength = 0;
            GLint previousSkipPixels = 0, previousSkipRows = 0, previousSwapBytes = 0;
            glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &previousBuffer);
            glGetIntegerv(GL_UNPACK_ALIGNMENT, &previousAlignment);
            glGetIntegerv(GL_UNPACK_ROW_LENGTH, &previousRowLength);
            glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &previousSkipPixels);
            glGetIntegerv(GL_UNPACK_SKIP_ROWS, &previousSkipRows);
            glGetIntegerv(GL_UNPACK_SWAP_BYTES, &previousSwapBytes);

            GLuint unpackBuffer = 0;
            glGenBuffers(1, &unpackBuffer);
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, unpackBuffer);
            glBufferData(GL_PIXEL_UNPACK_BUFFER, 256, nullptr, GL_STREAM_DRAW);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 8);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 17);
            glPixelStorei(GL_UNPACK_SKIP_PIXELS, 3);
            glPixelStorei(GL_UNPACK_SKIP_ROWS, 2);
            glPixelStorei(GL_UNPACK_SWAP_BYTES, GL_TRUE);

            auto textureHandle = backend->createTexture(rgba.data(), 2, 2);
            require(textureHandle != nullptr, "RGBA ImageStream texture creation");
            const GLuint texture = *static_cast<const GLuint*>(textureHandle);
            std::vector<std::uint8_t> readback(rgba.size());
            glBindTexture(GL_TEXTURE_2D, texture);
            glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, readback.data());
            require(readback == rgba, "RGBA ImageStream texture upload ignored inherited unpack state");

            const std::vector<std::uint8_t> updatedRgba{
                0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255, 0, 255};
            require(backend->updateTexture(textureHandle, updatedRgba.data(), 2, 2),
                    "RGBA ImageStream texture update");
            glBindTexture(GL_TEXTURE_2D, texture);
            glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, readback.data());
            require(readback == updatedRgba, "RGBA ImageStream texture update ignored inherited unpack state");
            GLint actualBuffer = 0, actualAlignment = 0, actualRowLength = 0;
            GLint actualSkipPixels = 0, actualSkipRows = 0, actualSwapBytes = 0;
            glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &actualBuffer);
            glGetIntegerv(GL_UNPACK_ALIGNMENT, &actualAlignment);
            glGetIntegerv(GL_UNPACK_ROW_LENGTH, &actualRowLength);
            glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &actualSkipPixels);
            glGetIntegerv(GL_UNPACK_SKIP_ROWS, &actualSkipRows);
            glGetIntegerv(GL_UNPACK_SWAP_BYTES, &actualSwapBytes);
            require(actualBuffer == static_cast<GLint>(unpackBuffer) && actualAlignment == 8 &&
                        actualRowLength == 17 && actualSkipPixels == 3 && actualSkipRows == 2 &&
                        actualSwapBytes == GL_TRUE,
                    "ImageStream upload leaked pixel unpack state");

            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, static_cast<GLuint>(previousBuffer));
            glPixelStorei(GL_UNPACK_ALIGNMENT, previousAlignment);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, previousRowLength);
            glPixelStorei(GL_UNPACK_SKIP_PIXELS, previousSkipPixels);
            glPixelStorei(GL_UNPACK_SKIP_ROWS, previousSkipRows);
            glPixelStorei(GL_UNPACK_SWAP_BYTES, previousSwapBytes);
            glDeleteBuffers(1, &unpackBuffer);
            backend->destroyTexture(textureHandle);
        }
#endif
        frame(image, 0);
        checkWindowPixel(window, false);
        require(!runtime.isAnimating(), "static texture keeps render loop alive");
        native->fill(true);
        frame(image, 1);
        checkWindowPixel(window, true);
        require(!runtime.update(window, 0.f, 1.f, 1.f, false), "unchanged texture requests repaint");
        for (int resize = 0; resize < 24; ++resize) {
            const int size = resize % 2 == 0 ? 320 : 256;
            extent = size;
#if defined(EUI_WINDOW_BACKEND_SDL2)
            SDL_SetWindowSize(static_cast<SDL_Window*>(window), size, size);
#else
            glfwSetWindowSize(static_cast<GLFWwindow*>(window), size, size);
#endif
            pump();
#if defined(EUI_RENDER_BACKEND_VULKAN)
            const bool injectFailure = resize == 8 || resize == 16;
            if (resize == 8) swapchainProbe.failCreate = true;
            if (resize == 16) swapchainProbe.framebuffersBeforeFailure = 1;
            if (injectFailure) {
                frame(image, 1);
                require(swapchainProbe.live.empty(), "failed resize retained a swapchain");
                require(core::render::currentRenderFrameStats().backendSubmits == 0,
                    "failed resize submitted an incomplete frame");
            }
#endif
            frame(image, 1);
            checkWindowPixel(window, true);
        }
#if defined(EUI_RENDER_BACKEND_VULKAN)
        require(swapchainProbe.handoffs >= 24 && !swapchainProbe.invalidHandoff,
            "resize did not hand off the live swapchain");
        require(swapchainProbe.faults == 2, "swapchain failure recovery was not exercised");
#endif
        native.reset();
        image.reset();
        frame(nullptr, 0);
        for (int i = 0; i < 256; ++i) {
            auto next = createNative(eui::image::gpuDevice(), 8 + i % 3, released);
            auto imported = importNative(next, 8 + i % 3);
            next.reset();
            frame(imported, 0);
        }
        require(released >= 250, "replaced resources accumulate");
        runtime.shutdown(false);
#if defined(EUI_RENDER_BACKEND_OPENGL)
        glFinish();
#else
        require(vkDeviceWaitIdle(reinterpret_cast<VkDevice>(eui::image::gpuDevice().device)) == VK_SUCCESS,
            "shutdown GPU wait");
#endif
        backend->makeCurrent();
        require(released == 257, "shutdown retained GPU resources while Runtime is still alive");
        std::cout << "Pixels, revisions, idle rendering, device rejection, 24 resizes, 256 replacements passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    backend.reset();
#if defined(EUI_RENDER_BACKEND_VULKAN)
    if (!swapchainProbe.live.empty() || swapchainProbe.invalidHandoff) {
        std::cerr << "Swapchain ownership was not balanced\n";
        result = 1;
    }
#endif
    if (result == 0 && released != 257) {
        std::cerr << "Leaked native images: " << 257 - released << '\n';
        result = 1;
    }
    core::window::destroyWindow(window);
#if defined(EUI_WINDOW_BACKEND_SDL2)
    SDL_Quit();
#else
    glfwTerminate();
#endif
    return result;
}
