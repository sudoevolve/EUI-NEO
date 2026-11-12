#include "core/dsl_runtime.h"
#include "components/linechart.h"
#include "core/window/window_backend.h"
#if defined(EUI_WINDOW_BACKEND_SDL2)
#define SDL_MAIN_HANDLED
#include <SDL.h>
#else
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#endif
#if defined(EUI_RENDER_BACKEND_OPENGL)
#include <glad/glad.h>
#endif
#include <iostream>

int main() {
    core::render::initializeRenderBackendLoader();
#if defined(EUI_WINDOW_BACKEND_SDL2)
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
#else
    if (!glfwInit()) return 1;
#endif
    core::window::WindowCreateRequest request;
    request.width = 320; request.height = 240; request.title = "Polyline regression";
    request.renderApi = core::render::windowRenderApi();
    auto window = core::window::createWindow(request);
    if (!window) return 2;
    auto backend = core::render::createRenderBackend(window);
    if (!backend || !backend->initialize()) return 3;
    int failures = 0;
    {
        core::render::ScopedRenderBackend active(*backend);
        core::dsl::Runtime runtime;
        runtime.initialize(window);
        for (int i = 0; i < 3; ++i) {
            backend->makeCurrent();
            runtime.compose("line", 320, 240, [&](core::dsl::Ui& ui, const core::dsl::Screen&) {
                const auto nan = std::numeric_limits<float>::quiet_NaN();
                ui.polygon("path").size(320, 240).points({{30, 60}, {130, 60}, {nan, 0}, {190, 180}, {290, 180}})
                    .strokeWidth(8).color(i == 1 ? core::Color{0, 1, 0, 1} : core::Color{1, 0, 0, 1}).build();
            });
            runtime.update(window, 0, 1, 1, false);
            backend->beginFrame({window, core::window::nativeWindowInfo(window), 320, 240, 1});
            runtime.render(320, 240, 1, {0, 0, 0, 1});
#if defined(EUI_RENDER_BACKEND_OPENGL)
            unsigned char line[4]{}, gap[4]{};
            glReadPixels(80, 179, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, line);
            glReadPixels(160, 119, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, gap);
            if (line[i == 1 ? 1 : 0] < 200 || gap[0] > 30 || gap[1] > 30) ++failures;
#endif
            backend->present();
        }
        runtime.shutdown(false);
    }
    backend.reset();
    core::window::destroyWindow(window);
#if defined(EUI_WINDOW_BACKEND_SDL2)
    SDL_Quit();
#else
    glfwTerminate();
#endif
    if (failures) std::cerr << "polyline pixels or discontinuity failed\n";
    return failures ? 4 : 0;
}
