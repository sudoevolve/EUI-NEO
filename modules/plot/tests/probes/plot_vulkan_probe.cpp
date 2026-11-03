#include "core/dsl_runtime.h"
#include "core/render/render_backend.h"
#include "core/window/window_backend.h"
#include "modules/plot/plot.h"
#include "modules/plot/plot3d.h"

#include <vulkan/vulkan.h>

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

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#endif

namespace {
void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

#ifdef _WIN32
void pumpEvents() {
#if defined(EUI_WINDOW_BACKEND_SDL2)
    SDL_Event event;
    while (SDL_PollEvent(&event)) {}
#else
    glfwPollEvents();
#endif
}

void checkDisplayedPixel(core::window::Handle window, int x, int y,
                         std::array<int, 3> expected = {255, 0, 0}) {
#if defined(EUI_WINDOW_BACKEND_SDL2)
    const auto native = core::window::nativeWindowInfo(window);
    const HWND handle = static_cast<HWND>(native.platformWindow);
#else
    const HWND handle = glfwGetWin32Window(static_cast<GLFWwindow*>(window));
#endif
    COLORREF pixel = CLR_INVALID;
    for (int attempt = 0; attempt < 30; ++attempt) {
        pumpEvents();
        Sleep(20);
        HDC dc = GetDC(handle);
        HDC capture = CreateCompatibleDC(dc);
        HBITMAP bitmap = CreateCompatibleBitmap(dc, 160, 120);
        HGDIOBJ previous = SelectObject(capture, bitmap);
        PrintWindow(handle, capture, PW_CLIENTONLY | 2);
        pixel = GetPixel(capture, x, y);
        SelectObject(capture, previous);
        DeleteObject(bitmap);
        DeleteDC(capture);
        ReleaseDC(handle, dc);
        if (std::abs(int(GetRValue(pixel)) - expected[0]) <= 2 &&
            std::abs(int(GetGValue(pixel)) - expected[1]) <= 2 &&
            std::abs(int(GetBValue(pixel)) - expected[2]) <= 2)
            return;
    }
    std::cerr << "pixel (" << x << ',' << y << ") actual " << int(GetRValue(pixel)) << ','
              << int(GetGValue(pixel)) << ',' << int(GetBValue(pixel)) << '\n';
    require(false, "Vulkan displayed pixel differs from expected output");
}
#endif
} // namespace

int main() {
    core::render::initializeRenderBackendLoader();
#if defined(EUI_WINDOW_BACKEND_SDL2)
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        return 1;
#else
    if (!glfwInit())
        return 1;
#endif
    core::window::WindowCreateRequest request;
    request.width = 160;
    request.height = 120;
    request.title = "Plot Vulkan probe";
    request.renderApi = core::window::RenderApi::Vulkan;
    const auto window = core::window::createWindow(request);
    auto backend = core::render::createRenderBackend(window);
    int result = 0;
    try {
        require(window && backend && backend->initialize(), "Vulkan window backend initialization");
    #ifdef _WIN32
        const auto native = core::window::nativeWindowInfo(window);
        const HWND windowHandle = static_cast<HWND>(native.platformWindow);
        ShowWindow(windowHandle, SW_SHOW);
        SetWindowPos(windowHandle, HWND_TOPMOST, 100, 100, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
        SetForegroundWindow(windowHandle);
    #endif
        core::render::ScopedRenderBackend scope(*backend);
        core::dsl::Runtime runtime;
        require(runtime.initialize(window), "runtime initialization");
        struct Shutdown {
            core::dsl::Runtime& runtime;
            ~Shutdown() { runtime.shutdown(false); }
        } shutdown{runtime};
        modules::plot::Renderer renderer;
        modules::plot::Batch triangle{{{4, 4}, {52, 4}, {4, 36}}, {1, 0, 0, 1}};
        renderer.render({triangle}, 64, 48, 1);
        auto image = renderer.image();
        require(image && image->descriptor().device.api == eui::GpuApi::Vulkan,
                "2D plot did not create a Vulkan image");
        require(image->descriptor().width == 64 && image->descriptor().height == 48,
                "2D plot image dimensions mismatch");
        const auto firstView = image->descriptor().imageView;
        const auto firstRevision = renderer.revision();
        renderer.render({triangle}, 64, 48, 1);
        require(renderer.revision() == firstRevision + 1, "2D plot revision did not advance");
        require(renderer.image()->descriptor().imageView == firstView,
            "same-size plot render recreated the Vulkan image");
        renderer.render({triangle}, 80, 48, 1);
        require(renderer.image()->descriptor().width == 80, "2D plot resize did not recreate image");
        require(renderer.image()->descriptor().imageView != firstView,
            "plot resize reused an incompatible Vulkan image");
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(
            reinterpret_cast<VkPhysicalDevice>(backend->gpuDeviceInfo().physicalDevice), &properties);
        std::cout << "Vulkan device: " << properties.deviceName << " API "
                  << VK_VERSION_MAJOR(properties.apiVersion) << '.' << VK_VERSION_MINOR(properties.apiVersion)
                  << " maxImageDimension2D " << properties.limits.maxImageDimension2D << '\n';
        if (properties.limits.maxImageDimension2D < 32768) {
            const auto width = properties.limits.maxImageDimension2D + 1;
            bool rejected = false;
            try {
                renderer.uploadRgba(width, 1, std::vector<std::uint8_t>(std::size_t(width) * 4));
            } catch (const std::length_error&) {
                rejected = true;
            }
            require(rejected, "device image limit not rejected before Vulkan allocation");
        }
        const auto stableView = renderer.image()->descriptor().imageView;
        const auto stableRevision = renderer.revision();
        bool tinyRejected = false;
        try {
            renderer.render({}, std::numeric_limits<double>::denorm_min(), 1,
                            std::numeric_limits<double>::denorm_min());
        } catch (const std::invalid_argument&) {
            tinyRejected = true;
        }
        require(tinyRejected, "underflowed physical image dimensions accepted");
        bool rejected = false;
        try {
            renderer.render({{{{0, 0}}, {1, 0, 0, 1}}}, 80, 48);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        require(rejected && renderer.revision() == stableRevision &&
                    renderer.image()->descriptor().imageView == stableView,
                "invalid batch changed committed renderer state");
        for (int i = 0; i < 64; ++i) {
            modules::plot::Renderer temporary;
            temporary.render({triangle}, 64 + (i % 3), 48, 1);
            auto retained = temporary.image();
            temporary.release();
            require(retained->valid(), "released renderer invalidated retained image");
            retained.reset();
        }

        modules::plot::Plot3D spatial;
        modules::plot::Renderer pixels;
        std::vector<std::uint8_t> rgba(16 * 16 * 4);
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x) {
                const auto p = (y * 16 + x) * 4;
                rgba[p + 1] = y < 8 ? 255 : 0;
                rgba[p + 2] = y < 8 ? 0 : 255;
                rgba[p + 3] = 255;
            }
        pixels.uploadRgba(16, 16, rgba);
        const auto pixelsView = pixels.image()->descriptor().imageView;
        pixels.uploadRgba(16, 16, rgba);
        require(pixels.image()->descriptor().imageView == pixelsView,
                "same-size RGBA upload recreated image");
        modules::plot::Renderer alpha;
        modules::plot::Batch square{{{0, 0}, {16, 0}, {16, 16}, {0, 0}, {16, 16}, {0, 16}}, {1, 0, 0, .5f}};
        alpha.render({square}, 16, 16, 1);
        auto geometry = std::make_shared<modules::plot::Geometry3D>();
        geometry->positions = {{-1, -1, 0}, {1, -1, 0}, {0, 1, 0}};
        geometry->triangles = {{0, 1, 2}};
        modules::plot::Material3D material;
        material.color = {1, 0, 0, 1};
        material.lighting = false;
        auto scene = spatial.scene();
        scene.showAxes = false;
        scene.objects.push_back({geometry, material, true});
        spatial.setScene(std::move(scene));
        runtime.compose("plot-vulkan", 160, 120, [&](core::dsl::Ui& ui, const core::dsl::Screen&) {
            ui.image("plot-vulkan.2d")
                .position(4, 4)
                .size(80, 48)
                .texture(renderer.image(), renderer.revision())
                .flipVertically()
                .build();
            ui.image("plot-vulkan.rows")
                .position(4, 64)
                .size(16, 16)
                .texture(pixels.image(), pixels.revision())
                .build();
            ui.image("plot-vulkan.alpha")
                .position(32, 64)
                .size(16, 16)
                .texture(alpha.image(), alpha.revision())
                .flipVertically()
                .build();
            ui.stack("plot-vulkan.spatial-wrapper")
                .position(90, 8)
                .size(64, 96)
                .content([&] { spatial.compose(ui, "plot-vulkan.spatial", 60, 80, 1); })
                .build();
        });
        runtime.update(window, 0.f, 1.f, 1.f, false);
        backend->beginFrame({window, core::window::nativeWindowInfo(window), 160, 120, 1.f});
        runtime.render(160, 120, 1.f, {0, 0, 0, 1});
        backend->present();
        require(vkDeviceWaitIdle(reinterpret_cast<VkDevice>(backend->gpuDeviceInfo().device)) == VK_SUCCESS,
            "wait for presented plot frame");
    #ifdef _WIN32
        checkDisplayedPixel(window, 14, 14);
        checkDisplayedPixel(window, 10, 66, {0, 255, 0});
        checkDisplayedPixel(window, 10, 77, {0, 0, 255});
        checkDisplayedPixel(window, 36, 68, {133, 7, 9});
        checkDisplayedPixel(window, 36, 69, {133, 7, 9});
#endif
        require(image->valid(), "imported plot image lost its resource owner");
        // 已呈现图像再次上传，覆盖同队列读取到下一次写入的同步与内容失效。
        for (int frame = 0; frame < 4; ++frame) {
            for (std::size_t p = 0; p < rgba.size(); p += 4) {
                rgba[p] = frame % 2 ? 255 : 0;
                rgba[p + 1] = frame % 2 ? 0 : 255;
                rgba[p + 2] = 0;
            }
            pixels.uploadRgba(16, 16, rgba);
            runtime.compose("plot-vulkan", 160, 120, [&](core::dsl::Ui& ui, const core::dsl::Screen&) {
                ui.image("plot-vulkan.rows")
                    .position(4, 64)
                    .size(16, 16)
                    .texture(pixels.image(), pixels.revision())
                    .build();
            });
            runtime.update(window, 0.f, 1.f, 1.f, false);
            backend->beginFrame({window, core::window::nativeWindowInfo(window), 160, 120, 1.f});
            runtime.render(160, 120, 1.f, {0, 0, 0, 1});
            backend->present();
#ifdef _WIN32
            checkDisplayedPixel(window, 10, 66,
                                frame % 2 ? std::array<int, 3>{255, 0, 0} : std::array<int, 3>{0, 255, 0});
#endif
        }
        spatial.releaseGpu();
        pixels.release();
        alpha.release();
        renderer.release();
        std::cout << "Vulkan 2D GPU draw/revision/resize and 3D CPU fallback composition passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        result = 1;
    }
    backend.reset();
    core::window::destroyWindow(window);
#if defined(EUI_WINDOW_BACKEND_SDL2)
    SDL_Quit();
#else
    glfwTerminate();
#endif
    return result;
}
