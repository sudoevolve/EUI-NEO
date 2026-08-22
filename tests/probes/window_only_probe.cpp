#include "core/window/window_backend.h"
#include "core/input/input_state.h"

#if defined(EUI_WINDOW_BACKEND_SDL2)
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>
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
#include <thread>

namespace {

core::window::RenderApi configuredRenderApi() {
#if defined(EUI_RENDER_BACKEND_VULKAN)
    return core::window::RenderApi::Vulkan;
#else
    return core::window::RenderApi::OpenGL;
#endif
}

void sleepBriefly() {
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
}

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

bool pollOnce(core::window::Handle window) {
#if defined(EUI_WINDOW_BACKEND_SDL2)
    SDL_Event event{};
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT ||
            (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE)) {
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

#if defined(EUI_WINDOW_BACKEND_SDL2)
bool verifySdlWindowInput(core::window::Handle firstWindow,
                          const core::window::WindowCreateRequest& request) {
    core::window::Handle secondWindow = core::window::createWindow(request);
    if (secondWindow == nullptr) {
        return false;
    }

    auto* second = static_cast<SDL_Window*>(secondWindow);
    SDL_HideWindow(second);

    core::KeyModifiers shiftModifiers;
    shiftModifiers.shift = true;
    core::queuePointerMotion(firstWindow, 31.0, 47.0, {}, shiftModifiers);

    double firstX = 0.0;
    double firstY = 0.0;
    double secondX = 0.0;
    double secondY = 0.0;
    core::window::getCursorPosition(firstWindow, firstX, firstY);
    core::window::getCursorPosition(secondWindow, secondX, secondY);
    if (firstX != 31.0 || firstY != 47.0 ||
        (secondX == firstX && secondY == firstY)) {
        core::window::destroyWindow(secondWindow);
        return false;
    }

    core::queuePointerButton(firstWindow, 31.0, 47.0,
                             core::PointerButton::Middle,
                             core::PointerAction::Press,
                             shiftModifiers);
    core::queuePointerMotion(firstWindow, 35.0, 51.0,
                             core::PointerButton::Middle,
                             shiftModifiers);
    core::queuePointerButton(firstWindow, 35.0, 51.0,
                             core::PointerButton::Middle,
                             core::PointerAction::Release,
                             shiftModifiers);
    if (!core::hasPendingPointerInput(firstWindow) ||
        core::hasPendingPointerInput(secondWindow)) {
        core::window::destroyWindow(secondWindow);
        return false;
    }

    core::queueTextInput(firstWindow, "ok");
    core::queueTextEditing(firstWindow, "ime");
    core::queueScrollInput(firstWindow, 1.25, -2.5);
    core::queueKeyInput(firstWindow,
                        {core::InputKey::LeftShift,
                         core::KeyAction::Press,
                         shiftModifiers});
    core::queueKeyInput(firstWindow,
                        {core::InputKey::Left,
                         core::KeyAction::Repeat,
                         {false, true}});
    core::queueKeyInput(firstWindow,
                        {core::InputKey::LeftShift,
                         core::KeyAction::Release,
                         {}});

    const std::vector<core::KeyEvent> keys = core::consumeKeyEvents(firstWindow);
    const core::TextInputEvent text = core::consumeTextInput(firstWindow);
    const core::ScrollEvent scroll = core::consumeScrollInput(firstWindow);
    const auto findKey = [&](core::InputKey key, core::KeyAction action) {
        return std::find_if(keys.begin(), keys.end(), [&](const core::KeyEvent& event) {
            return event.key == key && event.action == action;
        });
    };
    const auto left = findKey(core::InputKey::Left, core::KeyAction::Repeat);
    const std::vector<core::PointerEvent> pointer = core::consumePointerEvents(firstWindow);
    const bool queued = text.text == "ok" &&
        text.compositionText == "ime" && text.composing &&
        left != keys.end() && left->modifiers.shift &&
        findKey(core::InputKey::LeftShift, core::KeyAction::Press) != keys.end() &&
        findKey(core::InputKey::LeftShift, core::KeyAction::Release) != keys.end() &&
        scroll.x == 1.25 && scroll.y == -2.5 &&
        pointer.size() == 4 &&
        pointer[0].action == core::PointerAction::Move &&
        pointer[1].isPress(core::PointerButton::Middle) &&
        pointer[1].modifiers.shift &&
        pointer[2].action == core::PointerAction::Move &&
        pointer[2].isDown(core::PointerButton::Middle) &&
        pointer[3].isRelease(core::PointerButton::Middle) &&
        !pointer[3].isDown(core::PointerButton::Middle);

    core::releaseInputQueue(secondWindow);
    core::window::destroyWindow(secondWindow);
    return queued;
}
#endif

} // namespace

int main(int argc, char** argv) {
    const int frames = frameCountFromArgs(argc, argv);

#if defined(EUI_WINDOW_BACKEND_SDL2)
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        return 1;
    }
#else
    if (!glfwInit()) {
        return 1;
    }
#endif

    core::window::WindowCreateRequest request;
    request.width = 800;
    request.height = 600;
    request.title = "Window Only Probe";
    request.renderApi = configuredRenderApi();

    core::window::Handle window = core::window::createWindow(request);
    if (window == nullptr) {
#if defined(EUI_WINDOW_BACKEND_SDL2)
        SDL_Quit();
#else
        glfwTerminate();
#endif
        return 1;
    }

#if defined(EUI_WINDOW_BACKEND_SDL2)
    if (!verifySdlWindowInput(window, request)) {
        core::releaseInputQueue(window);
        core::window::destroyWindow(window);
        SDL_Quit();
        return 2;
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

    core::releaseInputQueue(window);
    core::window::destroyWindow(window);
#if defined(EUI_WINDOW_BACKEND_SDL2)
    SDL_Quit();
#else
    glfwTerminate();
#endif
    return 0;
}
