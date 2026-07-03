#include "core/render/render_backend.h"
#include "core/window/window_backend.h"

#if defined(EUI_WINDOW_BACKEND_SDL2) || defined(EUI_WINDOW_BACKEND_SDL3)
#if defined(EUI_WINDOW_BACKEND_SDL3)
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#else
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>
#endif
#else
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>
#endif

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <thread>

namespace {

int frameCountFromArgs(int argc, char** argv) {
    int frames = 3;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--manual") == 0) {
            return -1;
        }
        if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            frames = std::max(1, std::atoi(argv[++i]));
        }
    }
    return frames;
}

void sleepBriefly() {
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
}

bool pollOnce(core::window::Handle window) {
#if defined(EUI_WINDOW_BACKEND_SDL2) || defined(EUI_WINDOW_BACKEND_SDL3)
    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
#if defined(EUI_WINDOW_BACKEND_SDL3)
        if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
#else
        if (event.type == SDL_QUIT ||
            (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE)) {
#endif
            return false;
        }
    }
#else
    glfwPollEvents();
    if (glfwWindowShouldClose(static_cast<GLFWwindow*>(window))) {
        return false;
    }
#endif
    return true;
}

void shutdownWindowSystem() {
#if defined(EUI_WINDOW_BACKEND_SDL2) || defined(EUI_WINDOW_BACKEND_SDL3)
    SDL_Quit();
#else
    glfwTerminate();
#endif
}

bool initializeWindowSystem() {
#if defined(EUI_WINDOW_BACKEND_SDL3)
    return SDL_Init(SDL_INIT_VIDEO);
#elif defined(EUI_WINDOW_BACKEND_SDL2)
    SDL_SetMainReady();
    return SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0;
#else
    return glfwInit();
#endif
}

} // namespace

int main(int argc, char** argv) {
    const int frames = frameCountFromArgs(argc, argv);
    core::render::initializeRenderBackendLoader();

    if (!initializeWindowSystem()) {
        return 1;
    }

    core::window::WindowCreateRequest request;
    request.width = 800;
    request.height = 600;
    request.title = "Render Context Probe";
    request.renderApi = core::render::windowRenderApi();

    core::window::Handle window = core::window::createWindow(request);
    if (window == nullptr) {
        shutdownWindowSystem();
        return 1;
    }

    std::unique_ptr<core::render::RenderBackend> backend = core::render::createRenderBackend(window);
    if (!backend || !backend->initialize() || !backend->valid()) {
        core::window::destroyWindow(window);
        shutdownWindowSystem();
        return 1;
    }

    backend->makeCurrent();

#if defined(EUI_RENDER_BACKEND_OPENGL)
    if (core::window::currentContextKey() == nullptr) {
        backend.reset();
        core::window::destroyWindow(window);
        shutdownWindowSystem();
        return 1;
    }
#endif

    int renderedFrames = 0;
    while (frames < 0 || renderedFrames < frames) {
        if (!pollOnce(window)) {
            break;
        }
        sleepBriefly();
        ++renderedFrames;
    }

    backend->makeCurrent();
    backend->releaseRenderCache();
    backend.reset();
    core::window::destroyWindow(window);
    shutdownWindowSystem();
    return 0;
}
