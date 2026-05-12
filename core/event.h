#pragma once

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#include "core/primitive.h"

#include <cmath>
#include <string>
#include <unordered_map>
#include <utility>

namespace core {

enum class CursorShape {
    Arrow,
    Hand
};

struct PointerEvent {
    double x = 0.0;
    double y = 0.0;
    double deltaX = 0.0;
    double deltaY = 0.0;
    bool down = false;
    bool pressedThisFrame = false;
    bool releasedThisFrame = false;
    bool rightDown = false;
    bool rightPressedThisFrame = false;
    bool rightReleasedThisFrame = false;
};

struct KeyboardEvent {
    std::string text;
    std::string pasteText;
    bool backspace = false;
    bool del = false;
    bool enter = false;
    bool left = false;
    bool right = false;
    bool home = false;
    bool end = false;
    bool selectAll = false;
    bool copy = false;
    bool cut = false;
    bool shift = false;
    bool escape = false;

    bool hasInput() const {
        return !text.empty() || !pasteText.empty() || backspace || del || enter ||
               left || right || home || end || selectAll || copy || cut || escape;
    }
};

struct ScrollEvent {
    double x = 0.0;
    double y = 0.0;

    bool active() const {
        return x != 0.0 || y != 0.0;
    }
};

namespace detail {

struct InputQueue {
    std::string text;
    std::string pasteText;
    double scrollX = 0.0;
    double scrollY = 0.0;
    bool backspace = false;
    bool del = false;
    bool enter = false;
    bool left = false;
    bool right = false;
    bool home = false;
    bool end = false;
    bool selectAll = false;
    bool copy = false;
    bool cut = false;
    bool shift = false;
    bool escape = false;
};

struct PointerState {
    double lastX = 0.0;
    double lastY = 0.0;
    bool lastDown = false;
    bool lastRightDown = false;
};

inline std::unordered_map<GLFWwindow*, InputQueue>& inputQueues() {
    static std::unordered_map<GLFWwindow*, InputQueue> queues;
    return queues;
}

inline std::unordered_map<GLFWwindow*, PointerState>& pointerStates() {
    static std::unordered_map<GLFWwindow*, PointerState> states;
    return states;
}

inline InputQueue& inputQueue(GLFWwindow* window) {
    return inputQueues()[window];
}

inline PointerState& pointerState(GLFWwindow* window) {
    return pointerStates()[window];
}

inline void appendUtf8(std::string& output, unsigned int codepoint) {
    if (codepoint < 0x20) {
        return;
    }
    if (codepoint <= 0x7F) {
        output.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FF) {
        output.push_back(static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0xFFFF) {
        output.push_back(static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else {
        output.push_back(static_cast<char>(0xF0 | ((codepoint >> 18) & 0x07)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
}

} // namespace detail

inline void installInputCallbacks(GLFWwindow* window) {
    if (!window) {
        return;
    }

    glfwSetCharCallback(window, [](GLFWwindow* currentWindow, unsigned int codepoint) {
        detail::appendUtf8(detail::inputQueue(currentWindow).text, codepoint);
    });

    glfwSetScrollCallback(window, [](GLFWwindow* currentWindow, double xoffset, double yoffset) {
        detail::InputQueue& queue = detail::inputQueue(currentWindow);
        queue.scrollX += xoffset;
        queue.scrollY += yoffset;
    });

    glfwSetKeyCallback(window, [](GLFWwindow* currentWindow, int key, int, int action, int mods) {
        if (action != GLFW_PRESS && action != GLFW_REPEAT) {
            return;
        }

        detail::InputQueue& queue = detail::inputQueue(currentWindow);
        const bool ctrl = (mods & GLFW_MOD_CONTROL) != 0 || (mods & GLFW_MOD_SUPER) != 0;
        queue.shift = (mods & GLFW_MOD_SHIFT) != 0;
        if (ctrl && key == GLFW_KEY_V) {
            if (const char* clipboard = glfwGetClipboardString(currentWindow)) {
                queue.pasteText += clipboard;
            }
            return;
        }
        if (ctrl && key == GLFW_KEY_C) {
            queue.copy = true;
            return;
        }
        if (ctrl && key == GLFW_KEY_X) {
            queue.cut = true;
            return;
        }
        if (ctrl && key == GLFW_KEY_A) {
            queue.selectAll = true;
            return;
        }

        switch (key) {
        case GLFW_KEY_BACKSPACE:
            queue.backspace = true;
            break;
        case GLFW_KEY_DELETE:
            queue.del = true;
            break;
        case GLFW_KEY_ENTER:
        case GLFW_KEY_KP_ENTER:
            queue.enter = true;
            break;
        case GLFW_KEY_LEFT:
            queue.left = true;
            break;
        case GLFW_KEY_RIGHT:
            queue.right = true;
            break;
        case GLFW_KEY_HOME:
            queue.home = true;
            break;
        case GLFW_KEY_END:
            queue.end = true;
            break;
        case GLFW_KEY_ESCAPE:
            queue.escape = true;
            break;
        default:
            break;
        }
    });
}

inline std::pair<KeyboardEvent, ScrollEvent> consumeInputEvents(GLFWwindow* window) {
    detail::InputQueue& queue = detail::inputQueue(window);
    KeyboardEvent keyboard;
    keyboard.text = std::move(queue.text);
    keyboard.pasteText = std::move(queue.pasteText);
    keyboard.backspace = queue.backspace;
    keyboard.del = queue.del;
    keyboard.enter = queue.enter;
    keyboard.left = queue.left;
    keyboard.right = queue.right;
    keyboard.home = queue.home;
    keyboard.end = queue.end;
    keyboard.selectAll = queue.selectAll;
    keyboard.copy = queue.copy;
    keyboard.cut = queue.cut;
    keyboard.shift = queue.shift;
    keyboard.escape = queue.escape;

    ScrollEvent scroll{queue.scrollX, queue.scrollY};
    queue = {};
    return {std::move(keyboard), scroll};
}

inline std::pair<KeyboardEvent, ScrollEvent> consumeInputEvents() {
    return consumeInputEvents(glfwGetCurrentContext());
}

inline void releaseInputQueue(GLFWwindow* window) {
    detail::inputQueues().erase(window);
    detail::pointerStates().erase(window);
}

struct InteractionState {
    bool hover = false;
    bool pressed = false;
    bool clicked = false;
    bool pressStarted = false;
    bool released = false;
    bool drag = false;
    bool active = false;
    bool changed = false;
    double dragStartX = 0.0;
    double dragStartY = 0.0;
    double dragDeltaX = 0.0;
    double dragDeltaY = 0.0;

    void update(const Rect& bounds, const PointerEvent& event, bool topmostHover, bool enabled = true) {
        const bool oldHover = hover;
        const bool oldPressed = pressed;
        const bool oldDrag = drag;
        const bool oldActive = active;

        clicked = false;
        pressStarted = false;
        released = false;

        if (!enabled) {
            hover = false;
            pressed = false;
            drag = false;
            active = false;
            dragDeltaX = 0.0;
            dragDeltaY = 0.0;
            changed = oldHover != hover || oldPressed != pressed || oldDrag != drag || oldActive != active;
            return;
        }

        hover = topmostHover && bounds.contains(event.x, event.y);

        if (hover && event.pressedThisFrame) {
            active = true;
            pressStarted = true;
            dragStartX = event.x;
            dragStartY = event.y;
        }

        pressed = active && event.down;

        dragDeltaX = event.x - dragStartX;
        dragDeltaY = event.y - dragStartY;
        drag = pressed && (std::fabs(dragDeltaX) > 2.0 || std::fabs(dragDeltaY) > 2.0);

        if (event.releasedThisFrame) {
            released = active;
            clicked = active && hover;
            active = false;
            pressed = false;
            drag = false;
        }

        changed = oldHover != hover ||
                  oldPressed != pressed ||
                  oldDrag != drag ||
                  oldActive != active ||
                  pressStarted ||
                  released ||
                  clicked;
    }
};

inline PointerEvent readPointerEvent(GLFWwindow* window, float dpiScale = 1.0f) {
    detail::PointerState& state = detail::pointerState(window);

    double x = 0.0;
    double y = 0.0;
    glfwGetCursorPos(window, &x, &y);
    x *= dpiScale;
    y *= dpiScale;

    PointerEvent event;
    event.x = x;
    event.y = y;
    event.deltaX = x - state.lastX;
    event.deltaY = y - state.lastY;
    event.down = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
    event.rightDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    event.pressedThisFrame = event.down && !state.lastDown;
    event.releasedThisFrame = !event.down && state.lastDown;
    event.rightPressedThisFrame = event.rightDown && !state.lastRightDown;
    event.rightReleasedThisFrame = !event.rightDown && state.lastRightDown;

    state.lastX = x;
    state.lastY = y;
    state.lastDown = event.down;
    state.lastRightDown = event.rightDown;
    return event;
}

} // namespace core
