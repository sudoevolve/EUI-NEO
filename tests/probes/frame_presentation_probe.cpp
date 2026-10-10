#include "core/app/main_window_runtime.h"
#include "eui/detail/dsl_app_impl.h"
#include "eui_neo.h"
#if defined(EUI_WINDOW_BACKEND_SDL2)
#define SDL_MAIN_HANDLED
#include <SDL.h>
#if defined(EUI_RENDER_BACKEND_VULKAN)
#include <SDL_vulkan.h>
#endif
#else
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#endif
#include <cstdio>

namespace {
int failures = 0;
void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "frame_presentation_probe: %s\n", message);
        ++failures;
    }
}
void resize(core::window::Handle window, int w, int h) {
#if defined(EUI_WINDOW_BACKEND_SDL2)
    SDL_SetWindowSize(static_cast<SDL_Window*>(window), w, h);
#else
    glfwSetWindowSize(static_cast<GLFWwindow*>(window), w, h);
#endif
}
app::MainWindowMetrics metrics(core::window::Handle window) {
    int w = 0, h = 0;
#if defined(EUI_WINDOW_BACKEND_SDL2)
#if defined(EUI_RENDER_BACKEND_VULKAN)
    SDL_Vulkan_GetDrawableSize(static_cast<SDL_Window*>(window), &w, &h);
#else
    SDL_GL_GetDrawableSize(static_cast<SDL_Window*>(window), &w, &h);
#endif
#else
    glfwGetFramebufferSize(static_cast<GLFWwindow*>(window), &w, &h);
#endif
    return {w, h, 1, 1};
}
} // namespace
namespace app {
const DslAppConfig& dslAppConfig() {
    static const auto config = DslAppConfig{}.iconPath("").tray(false).clearColor({0.8f, 0.6f, 0.4f, 1});
    return config;
}
void compose(eui::Ui& ui, const eui::Screen& screen) {
    ui.rect("fill").size(screen.width, screen.height).color({0.8f, 0.6f, 0.4f, 1}).build();
}
} // namespace app
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
    request.visible = false;
    request.width = 320;
    request.height = 240;
    request.renderApi = core::render::windowRenderApi();
    auto window = core::window::createWindow(request);
#if defined(EUI_WINDOW_BACKEND_SDL2)
    require(window && !(SDL_GetWindowFlags(static_cast<SDL_Window*>(window)) & SDL_WINDOW_SHOWN),
            "hidden creation exposed window");
#else
    require(window && !glfwGetWindowAttrib(static_cast<GLFWwindow*>(window), GLFW_VISIBLE),
            "hidden creation exposed window");
#endif
    auto backend = core::render::createRenderBackend(window);
    if (!window || !backend || !backend->initialize())
        return 1;
    backend->makeCurrent();
    core::render::ScopedRenderBackend scope(*backend);
    if (!app::initialize(window))
        return 1;
    app::AppRunner runner;
    int readyCalls = 0;
    app::MainWindowRuntime runtime(runner, [&] {
        ++readyCalls;
        require(core::render::currentRenderFrameStats().framebufferWidth == metrics(window).framebufferWidth,
                "window mapped before a frame at its current size was drawn");
        require(core::render::currentRenderFrameStats().backendPresents == 0,
                "first frame was swapped before mapping");
#if defined(EUI_WINDOW_BACKEND_SDL2)
        require(!(SDL_GetWindowFlags(static_cast<SDL_Window*>(window)) & SDL_WINDOW_SHOWN),
                "window mapped before first frame readiness");
        SDL_ShowWindow(static_cast<SDL_Window*>(window));
#else
        require(!glfwGetWindowAttrib(static_cast<GLFWwindow*>(window), GLFW_VISIBLE),
                "window mapped before first frame readiness");
        glfwShowWindow(static_cast<GLFWwindow*>(window));
#endif
    });
    const auto original = metrics(window);
    const bool stalePresented = runtime.updateAndRender(
        window, *backend, original, 0, false, true, [&] { resize(window, 480, 360); },
        [&] { return metrics(window); });
    require(stalePresented, "update-time resize did not produce a fresh-size frame");
    require(readyCalls == 1, "initial fresh-size frame was not made ready");
    require(core::render::lastRenderFrameStats().framebufferWidth == metrics(window).framebufferWidth,
            "presented old-size frame after resize during update");
    runner.paintRequested = true;
    require(runtime.updateAndRender(
                window, *backend, metrics(window), 0, false, true, [] {}, [&] { return metrics(window); }),
            "fresh-size frame was not presented");
    require(core::render::lastRenderFrameStats().framebufferWidth == metrics(window).framebufferWidth,
            "presented size differs from drawable");
    const auto presents = core::render::lastRenderFrameStats().backendPresents;
    const auto frames = runner.renderedFrames;
    const auto verifyDeferred = [&](app::MainWindowMetrics changed, const char* message) {
        runner.paintRequested = true;
        const auto current = metrics(window);
        int reads = 0;
        require(!runtime.updateAndRender(
                    window, *backend, current, 0, false, true, [] {},
                    [&] { return ++reads == 1 ? changed : current; }),
                message);
        require(runner.paintRequested && runner.renderedFrames == frames,
                "deferred frame cleared repaint or counted a present");
        require(core::render::lastRenderFrameStats().backendPresents == presents,
                "deferred frame changed present statistics");
    };
    auto changed = metrics(window);
    changed.dpiScale = 2;
    verifyDeferred(changed, "DPI change presented stale layout");
    changed = metrics(window);
    changed.pointerScale = 2;
    verifyDeferred(changed, "pointer scale change presented stale layout");
    verifyDeferred({0, 0, 1, 1}, "zero drawable after update was presented");
    require(!runtime.updateAndRender(
                window, *backend, {0, 0, 1, 1}, 0, false, true, [] {}, [&] { return metrics(window); }),
            "zero drawable before update was presented");
    require(runtime.updateAndRender(
                window, *backend, metrics(window), 0, false, true, [] {}, [&] { return metrics(window); }),
            "valid drawable did not recover after unavailable frame");
    runner.paintRequested = false;
    require(!runtime.updateAndRender(
                window, *backend, metrics(window), 0, false, true, [] {}, [&] { return metrics(window); }),
            "unchanged idle frame was unnecessarily presented");
    require(readyCalls == 1, "first-frame readiness callback did not run exactly once");
    app::shutdown();
    backend.reset();
    core::window::destroyWindow(window);
#if defined(EUI_WINDOW_BACKEND_SDL2)
    SDL_Quit();
#else
    glfwTerminate();
#endif
    return failures ? 1 : 0;
}
