#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include "eui_neo.h"
#include "eui/detail/dsl_app_impl.h"
#include "core/render/render_backend.h"
#include "core/window/window_backend.h"
#if defined(EUI_WINDOW_BACKEND_SDL2)
#define SDL_MAIN_HANDLED
#include <SDL.h>
#else
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#endif

#include <cstdio>

namespace {
int starts = 0;
int shutdowns = 0;
int composes = 0;
bool startBeforeFirstCompose = false;

bool require(bool condition, const char* message) {
    if (!condition)
        std::fprintf(stderr, "dsl_app_lifecycle_probe: %s\n", message);
    return condition;
}
}

namespace app {
const DslAppConfig& dslAppConfig() {
    static const auto config = DslAppConfig{}
                                   .title("DSL app lifecycle probe")
                                   .onStart([] { ++starts; })
                                   .onShutdown([] { ++shutdowns; });
    return config;
}

void compose(eui::Ui& ui, const eui::Screen&) {
    ++composes;
    startBeforeFirstCompose = starts == 1;
    ui.text("lifecycle.probe").text("ready").build();
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
    request.width = 320;
    request.height = 240;
    request.title = "DSL app lifecycle probe";
    request.renderApi = core::render::windowRenderApi();
    auto window = core::window::createWindow(request);
    auto backend = core::render::createRenderBackend(window);
    int result = 0;
    bool initialized = false;
    if (!require(window && backend && backend->initialize(), "window backend initialization")) {
        result = 1;
    } else {
        backend->makeCurrent();
        core::render::ScopedRenderBackend scope(*backend);
        initialized = app::initialize(window);
        if (!require(initialized, "app initialization")) {
            result = 1;
        } else {
            if (!require(starts == 1 && composes == 0, "onStart did not run once before first compose") ||
                !require(app::update(window, 0, 320, 240, 1, 1, false), "initial app update") ||
                !require(composes == 1 && startBeforeFirstCompose, "first compose preceded onStart")) {
                result = 1;
            }
            app::shutdown();
            initialized = false;
            if (!require(shutdowns == 1, "onShutdown did not pair with onStart"))
                result = 1;
        }
    }
    if (initialized)
        app::shutdown();
    backend.reset();
    core::window::destroyWindow(window);
#if defined(EUI_WINDOW_BACKEND_SDL2)
    SDL_Quit();
#else
    glfwTerminate();
#endif
    return result;
}