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

#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
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

void checkDisplayedPixel(core::window::Handle window, int x, int y) {
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
        if (GetRValue(pixel) > 220 && GetGValue(pixel) < 30 && GetBValue(pixel) < 30)
            return;
    }
    require(false, "Vulkan plot triangle was not visible in the window capture");
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

        modules::plot::Plot3D spatial;
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
        checkDisplayedPixel(window, 14, 41);
    #endif
        require(image->valid(), "imported plot image lost its resource owner");
        spatial.releaseGpu();
        renderer.release();
        runtime.shutdown(false);
        std::cout << "Vulkan 2D upload/revision/resize and 3D CPU fallback composition passed\n";
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
