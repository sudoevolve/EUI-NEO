#include "core/window/window_backend.h"

#if defined(EUI_WINDOW_BACKEND_SDL2)

#include "core/input/input_state.h"

#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>

namespace core::window {

namespace {

constexpr double kPointerOutsideWindow = -1000000.0;

core::PointerButtons sdlPointerButtons(Uint32 state) {
    core::PointerButtons buttons;
    buttons.set(core::PointerButton::Left, (state & SDL_BUTTON_LMASK) != 0);
    buttons.set(core::PointerButton::Middle, (state & SDL_BUTTON_MMASK) != 0);
    buttons.set(core::PointerButton::Right, (state & SDL_BUTTON_RMASK) != 0);
    buttons.set(core::PointerButton::X1, (state & SDL_BUTTON_X1MASK) != 0);
    buttons.set(core::PointerButton::X2, (state & SDL_BUTTON_X2MASK) != 0);
    return buttons;
}

core::KeyModifiers sdlModifiers(SDL_Keymod state) {
    core::KeyModifiers modifiers;
    modifiers.control = (state & KMOD_CTRL) != 0;
    modifiers.shift = (state & KMOD_SHIFT) != 0;
    modifiers.alt = (state & KMOD_ALT) != 0;
    modifiers.super = (state & KMOD_GUI) != 0;
    modifiers.capsLock = (state & KMOD_CAPS) != 0;
    modifiers.numLock = (state & KMOD_NUM) != 0;
    return modifiers;
}

bool refreshFocusedPointer(Handle window) {
    if (SDL_GetMouseFocus() != static_cast<SDL_Window*>(window)) {
        return false;
    }
    int x = 0;
    int y = 0;
    const Uint32 buttons = SDL_GetMouseState(&x, &y);
    core::queuePointerMotion(window,
                             static_cast<double>(x),
                             static_cast<double>(y),
                             sdlPointerButtons(buttons),
                             sdlModifiers(SDL_GetModState()));
    return true;
}

void getSdlCursorPosition(Handle window, double& x, double& y) {
    if (window == nullptr) {
        x = kPointerOutsideWindow;
        y = kPointerOutsideWindow;
        return;
    }

    if (core::detail::queuedPointerPosition(window, x, y)) {
        return;
    }
    refreshFocusedPointer(window);
    if (core::detail::queuedPointerPosition(window, x, y)) {
        return;
    }

    x = kPointerOutsideWindow;
    y = kPointerOutsideWindow;
}

} // namespace

void installInputCallbacks(Handle) {
    SDL_StartTextInput();
}

void uninstallInputCallbacks(Handle) {}

bool queryImeComposition(Handle, std::string&, bool&) { return false; }

void getCursorPosition(Handle window, double& x, double& y) {
    getSdlCursorPosition(window, x, y);
}

} // namespace core::window

#else

#include "core/input/input_state.h"
#include "core/platform/ime_bridge.h"

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

namespace core::window {

namespace {

core::InputKey mapGlfwKey(int key) {
    if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9) {
        return static_cast<core::InputKey>(
            static_cast<int>(core::InputKey::Digit0) + key - GLFW_KEY_0);
    }
    if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z) {
        return static_cast<core::InputKey>(
            static_cast<int>(core::InputKey::A) + key - GLFW_KEY_A);
    }
    if (key >= GLFW_KEY_F1 && key <= GLFW_KEY_F24) {
        return static_cast<core::InputKey>(
            static_cast<int>(core::InputKey::F1) + key - GLFW_KEY_F1);
    }
    if (key >= GLFW_KEY_KP_0 && key <= GLFW_KEY_KP_9) {
        return static_cast<core::InputKey>(
            static_cast<int>(core::InputKey::Numpad0) + key - GLFW_KEY_KP_0);
    }
    switch (key) {
    case GLFW_KEY_BACKSPACE: return core::InputKey::Backspace;
    case GLFW_KEY_TAB: return core::InputKey::Tab;
    case GLFW_KEY_ENTER: return core::InputKey::Enter;
    case GLFW_KEY_ESCAPE: return core::InputKey::Escape;
    case GLFW_KEY_SPACE: return core::InputKey::Space;
    case GLFW_KEY_INSERT: return core::InputKey::Insert;
    case GLFW_KEY_DELETE: return core::InputKey::Delete;
    case GLFW_KEY_HOME: return core::InputKey::Home;
    case GLFW_KEY_END: return core::InputKey::End;
    case GLFW_KEY_PAGE_UP: return core::InputKey::PageUp;
    case GLFW_KEY_PAGE_DOWN: return core::InputKey::PageDown;
    case GLFW_KEY_LEFT: return core::InputKey::Left;
    case GLFW_KEY_RIGHT: return core::InputKey::Right;
    case GLFW_KEY_UP: return core::InputKey::Up;
    case GLFW_KEY_DOWN: return core::InputKey::Down;
    case GLFW_KEY_PRINT_SCREEN: return core::InputKey::PrintScreen;
    case GLFW_KEY_SCROLL_LOCK: return core::InputKey::ScrollLock;
    case GLFW_KEY_PAUSE: return core::InputKey::Pause;
    case GLFW_KEY_CAPS_LOCK: return core::InputKey::CapsLock;
    case GLFW_KEY_NUM_LOCK: return core::InputKey::NumLock;
    case GLFW_KEY_LEFT_SHIFT: return core::InputKey::LeftShift;
    case GLFW_KEY_RIGHT_SHIFT: return core::InputKey::RightShift;
    case GLFW_KEY_LEFT_CONTROL: return core::InputKey::LeftControl;
    case GLFW_KEY_RIGHT_CONTROL: return core::InputKey::RightControl;
    case GLFW_KEY_LEFT_ALT: return core::InputKey::LeftAlt;
    case GLFW_KEY_RIGHT_ALT: return core::InputKey::RightAlt;
    case GLFW_KEY_LEFT_SUPER: return core::InputKey::LeftSuper;
    case GLFW_KEY_RIGHT_SUPER: return core::InputKey::RightSuper;
    case GLFW_KEY_MENU: return core::InputKey::Menu;
    case GLFW_KEY_APOSTROPHE: return core::InputKey::Apostrophe;
    case GLFW_KEY_COMMA: return core::InputKey::Comma;
    case GLFW_KEY_MINUS: return core::InputKey::Minus;
    case GLFW_KEY_PERIOD: return core::InputKey::Period;
    case GLFW_KEY_SLASH: return core::InputKey::Slash;
    case GLFW_KEY_SEMICOLON: return core::InputKey::Semicolon;
    case GLFW_KEY_EQUAL: return core::InputKey::Equal;
    case GLFW_KEY_LEFT_BRACKET: return core::InputKey::LeftBracket;
    case GLFW_KEY_BACKSLASH: return core::InputKey::Backslash;
    case GLFW_KEY_RIGHT_BRACKET: return core::InputKey::RightBracket;
    case GLFW_KEY_GRAVE_ACCENT: return core::InputKey::GraveAccent;
    case GLFW_KEY_KP_DECIMAL: return core::InputKey::NumpadDecimal;
    case GLFW_KEY_KP_DIVIDE: return core::InputKey::NumpadDivide;
    case GLFW_KEY_KP_MULTIPLY: return core::InputKey::NumpadMultiply;
    case GLFW_KEY_KP_SUBTRACT: return core::InputKey::NumpadSubtract;
    case GLFW_KEY_KP_ADD: return core::InputKey::NumpadAdd;
    case GLFW_KEY_KP_ENTER: return core::InputKey::NumpadEnter;
    case GLFW_KEY_KP_EQUAL: return core::InputKey::NumpadEqual;
    default: return core::InputKey::Unknown;
    }
}

core::KeyModifiers glfwModifiers(int state) {
    core::KeyModifiers modifiers;
    modifiers.control = (state & GLFW_MOD_CONTROL) != 0;
    modifiers.shift = (state & GLFW_MOD_SHIFT) != 0;
    modifiers.alt = (state & GLFW_MOD_ALT) != 0;
    modifiers.super = (state & GLFW_MOD_SUPER) != 0;
    modifiers.capsLock = (state & GLFW_MOD_CAPS_LOCK) != 0;
    modifiers.numLock = (state & GLFW_MOD_NUM_LOCK) != 0;
    return modifiers;
}

core::PointerButton mapGlfwButton(int button) {
    switch (button) {
    case GLFW_MOUSE_BUTTON_LEFT: return core::PointerButton::Left;
    case GLFW_MOUSE_BUTTON_MIDDLE: return core::PointerButton::Middle;
    case GLFW_MOUSE_BUTTON_RIGHT: return core::PointerButton::Right;
    case GLFW_MOUSE_BUTTON_4: return core::PointerButton::X1;
    case GLFW_MOUSE_BUTTON_5: return core::PointerButton::X2;
    default: return core::PointerButton::None;
    }
}

} // namespace

void getCursorPosition(Handle window, double& x, double& y) {
    glfwGetCursorPos(static_cast<GLFWwindow*>(window), &x, &y);
}

void installInputCallbacks(Handle window) {
    if (window == nullptr) {
        return;
    }

    auto* glfwWindow = static_cast<GLFWwindow*>(window);
    glfwSetInputMode(glfwWindow, GLFW_LOCK_KEY_MODS, GLFW_TRUE);
    eui_ime_install_message_filter(glfwWindow);
    glfwSetCharCallback(glfwWindow, [](GLFWwindow* currentWindow, unsigned int codepoint) {
        core::detail::InputQueue& queue = core::detail::inputQueue(currentWindow);
        core::detail::appendUtf8(queue.text, codepoint);
        eui_ime_clear_composition(currentWindow);
        queue.compositionText.clear();
        queue.compositionChanged = true;
        core::detail::setComposing(currentWindow, false);
        core::detail::setCompositionText(currentWindow, {});
    });
    glfwSetScrollCallback(glfwWindow, [](GLFWwindow* currentWindow, double xoffset, double yoffset) {
        core::queueScrollInput(currentWindow, xoffset, yoffset);
    });
    glfwSetCursorPosCallback(glfwWindow, [](GLFWwindow* currentWindow, double x, double y) {
        const core::detail::PointerState& state = core::detail::pointerState(currentWindow);
        core::queuePointerMotion(currentWindow, x, y, state.buttons,
                                 core::detail::currentModifiers(currentWindow));
    });
    glfwSetMouseButtonCallback(glfwWindow, [](GLFWwindow* currentWindow, int button, int action, int mods) {
        const core::PointerButton mapped = mapGlfwButton(button);
        if (mapped == core::PointerButton::None ||
            (action != GLFW_PRESS && action != GLFW_RELEASE)) {
            return;
        }
        double x = 0.0;
        double y = 0.0;
        glfwGetCursorPos(currentWindow, &x, &y);
        core::queuePointerButton(currentWindow, x, y, mapped,
            action == GLFW_PRESS ? core::PointerAction::Press : core::PointerAction::Release,
            glfwModifiers(mods));
    });
    glfwSetKeyCallback(glfwWindow, [](GLFWwindow* currentWindow, int key, int scanCode, int action, int mods) {
        core::KeyAction keyAction = core::KeyAction::Press;
        if (action == GLFW_REPEAT) {
            keyAction = core::KeyAction::Repeat;
        } else if (action == GLFW_RELEASE) {
            keyAction = core::KeyAction::Release;
        } else if (action != GLFW_PRESS) {
            return;
        }
        core::detail::setComposing(currentWindow, eui_ime_is_composing(currentWindow) != 0);
        core::queueKeyInput(currentWindow, {
            mapGlfwKey(key), keyAction, glfwModifiers(mods), scanCode
        });
    });
}

void uninstallInputCallbacks(Handle window) {
    if (window != nullptr) {
        eui_ime_uninstall_message_filter(static_cast<GLFWwindow*>(window));
    }
}

bool queryImeComposition(Handle window, std::string& text, bool& composing) {
#if defined(_WIN32) || defined(__APPLE__)
    char compositionBuffer[512]{};
    const int compositionLength = eui_ime_get_composition_string_utf8(
        static_cast<GLFWwindow*>(window),
        compositionBuffer,
        static_cast<int>(sizeof(compositionBuffer)));
    composing = compositionLength > 0;
    text = composing ? std::string(compositionBuffer) : std::string{};
    return true;
#else
    (void)window;
    (void)text;
    (void)composing;
    return false;
#endif
}

} // namespace core::window

#endif
