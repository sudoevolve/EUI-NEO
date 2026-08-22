#include "core/dsl_runtime.h"
#include "core/input/input_state.h"
#include "core/window/window_backend.h"

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

#include <iostream>

namespace {

core::window::RenderApi configuredRenderApi() {
#if defined(EUI_RENDER_BACKEND_VULKAN)
    return core::window::RenderApi::Vulkan;
#else
    return core::window::RenderApi::OpenGL;
#endif
}

bool initializeWindowBackend() {
#if defined(EUI_WINDOW_BACKEND_SDL2)
    SDL_SetMainReady();
    return SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0;
#else
    return glfwInit() == GLFW_TRUE;
#endif
}

void terminateWindowBackend() {
#if defined(EUI_WINDOW_BACKEND_SDL2)
    SDL_Quit();
#else
    glfwTerminate();
#endif
}

} // namespace

int main() {
    if (!initializeWindowBackend()) {
        return 1;
    }

    core::window::WindowCreateRequest request;
    request.width = 200;
    request.height = 160;
    request.title = "Input Runtime Probe";
    request.resizable = false;
    request.renderApi = configuredRenderApi();
    core::window::Handle window = core::window::createWindow(request);
    if (window == nullptr) {
        terminateWindowBackend();
        return 2;
    }

    int presses = 0;
    int releases = 0;
    int middleDrags = 0;
    int rightDrags = 0;
    int contextMenus = 0;
    int focusedKeys = 0;
    int defaultFocusedKeys = 0;
    int applicationKeys = 0;

    core::dsl::Runtime runtime;
    runtime.setKeyEventHandler([&](const core::KeyEvent&) { ++applicationKeys; });
    runtime.initialize(window);
    runtime.compose("input", 200.0f, 160.0f,
        [&](core::dsl::Ui& ui, const core::dsl::Screen&) {
            ui.rect("target")
                .position(10.0f, 10.0f)
                .size(120.0f, 100.0f)
                .acceptedButtons(core::PointerButton::Left |
                                 core::PointerButton::Middle |
                                 core::PointerButton::Right)
                .dragThreshold(4.0f)
                .onPress([&](const core::PointerEvent&, const core::Rect&) {
                    ++presses;
                })
                .onRelease([&](const core::PointerEvent&, const core::Rect&) {
                    ++releases;
                })
                .onDrag([&](const core::dsl::DragEvent& event) {
                    if (event.button == core::PointerButton::Middle) {
                        ++middleDrags;
                    } else if (event.button == core::PointerButton::Right) {
                        ++rightDrags;
                    }
                })
                .onContextMenu([&](const core::PointerEvent&, const core::Rect&) {
                    ++contextMenus;
                })
                .onKeyEvent([&](const core::KeyEvent& event) {
                    ++focusedKeys;
                    return event.key == core::InputKey::F1;
                })
                .build();
            ui.rect("default-focus")
                .position(140.0f, 10.0f)
                .size(50.0f, 100.0f)
                .onKeyEvent([&](const core::KeyEvent&) {
                    ++defaultFocusedKeys;
                    return true;
                })
                .build();
        });

    const core::KeyModifiers modifiers{};
    core::queuePointerButton(window, 20.0, 20.0, core::PointerButton::Middle,
                             core::PointerAction::Press, modifiers);
    core::queuePointerMotion(window, 30.0, 20.0,
                             core::PointerButton::Middle, modifiers);
    core::queuePointerButton(window, 30.0, 20.0, core::PointerButton::Middle,
                             core::PointerAction::Release, modifiers);
    runtime.update(window, 1.0f / 60.0f, 1.0f, 1.0f);

    core::queuePointerButton(window, 20.0, 20.0, core::PointerButton::Right,
                             core::PointerAction::Press, modifiers);
    core::queuePointerButton(window, 20.0, 20.0, core::PointerButton::Right,
                             core::PointerAction::Release, modifiers);
    runtime.update(window, 1.0f / 60.0f, 1.0f, 1.0f);

    core::queuePointerButton(window, 20.0, 20.0, core::PointerButton::Right,
                             core::PointerAction::Press, modifiers);
    core::queuePointerMotion(window, 32.0, 20.0,
                             core::PointerButton::Right, modifiers);
    core::queuePointerButton(window, 32.0, 20.0, core::PointerButton::Right,
                             core::PointerAction::Release, modifiers);
    runtime.update(window, 1.0f / 60.0f, 1.0f, 1.0f);

    core::queuePointerButton(window, 20.0, 20.0, core::PointerButton::Left,
                             core::PointerAction::Press, modifiers);
    core::queuePointerButton(window, 20.0, 20.0, core::PointerButton::Left,
                             core::PointerAction::Release, modifiers);
    runtime.update(window, 1.0f / 60.0f, 1.0f, 1.0f);

    core::queuePointerButton(window, 150.0, 20.0, core::PointerButton::Right,
                             core::PointerAction::Press, modifiers);
    core::queuePointerButton(window, 150.0, 20.0, core::PointerButton::Right,
                             core::PointerAction::Release, modifiers);
    runtime.update(window, 1.0f / 60.0f, 1.0f, 1.0f);

    core::queueKeyInput(window, {core::InputKey::F1, core::KeyAction::Press, {}});
    core::queueKeyInput(window, {core::InputKey::F2, core::KeyAction::Press, {}});
    runtime.update(window, 1.0f / 60.0f, 1.0f, 1.0f);

    const bool passed = presses == 4 && releases == 4 &&
        middleDrags > 0 && rightDrags > 0 && contextMenus == 1 &&
        focusedKeys == 2 && defaultFocusedKeys == 0 && applicationKeys == 1;
    if (!passed) {
        std::cerr << "Input Runtime dispatch failed: presses=" << presses
                  << " releases=" << releases
                  << " middleDrags=" << middleDrags
                  << " rightDrags=" << rightDrags
                  << " contextMenus=" << contextMenus
                  << " focusedKeys=" << focusedKeys
                  << " defaultFocusedKeys=" << defaultFocusedKeys
                  << " applicationKeys=" << applicationKeys << "\n";
    }

    runtime.shutdown(false);
    core::releaseInputQueue(window);
    core::window::destroyWindow(window);
    terminateWindowBackend();
    return passed ? 0 : 3;
}
