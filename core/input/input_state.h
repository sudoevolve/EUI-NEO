#pragma once

#include "core/input/input_types.h"
#include "core/window/window_backend.h"

#include <unordered_map>
#include <utility>

namespace core {

namespace detail {

struct InputQueue {
    std::string text;
    std::string pasteText;
    std::string compositionText;
    std::vector<KeyEvent> keys;
    double scrollX = 0.0;
    double scrollY = 0.0;
    bool compositionChanged = false;
};

struct PointerState {
    double lastX = 0.0;
    double lastY = 0.0;
    double x = 0.0;
    double y = 0.0;
    bool lastDown = false;
    bool lastRightDown = false;
    bool down = false;
    bool rightDown = false;
    bool hasPosition = false;
    bool inside = false;
};

inline std::unordered_map<window::Handle, InputQueue>& inputQueues() {
    static std::unordered_map<window::Handle, InputQueue> queues;
    return queues;
}

inline std::unordered_map<window::Handle, PointerState>& pointerStates() {
    static std::unordered_map<window::Handle, PointerState> states;
    return states;
}

inline std::unordered_map<window::Handle, bool>& composingStates() {
    static std::unordered_map<window::Handle, bool> states;
    return states;
}

inline std::unordered_map<window::Handle, std::string>& compositionTextStates() {
    static std::unordered_map<window::Handle, std::string> states;
    return states;
}

inline InputQueue& inputQueue(window::Handle window) {
    return inputQueues()[window];
}

inline PointerState& pointerState(window::Handle window) {
    return pointerStates()[window];
}

inline bool isComposing(window::Handle window) {
    const auto it = composingStates().find(window);
    return it != composingStates().end() && it->second;
}

inline std::string compositionText(window::Handle window) {
    const auto it = compositionTextStates().find(window);
    return it == compositionTextStates().end() ? std::string{} : it->second;
}

inline void setComposing(window::Handle window, bool composing) {
    composingStates()[window] = composing;
}

inline void setCompositionText(window::Handle window, const std::string& text) {
    compositionTextStates()[window] = text;
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

inline bool hasQueuedPointerState(window::Handle window) {
    const auto iterator = pointerStates().find(window);
    return iterator != pointerStates().end() && iterator->second.hasPosition;
}

inline bool queuedPointerPosition(window::Handle window, double& x, double& y) {
    const auto iterator = pointerStates().find(window);
    if (iterator == pointerStates().end()) {
        return false;
    }
    const PointerState& state = iterator->second;
    if (!state.hasPosition || (!state.inside && !state.down && !state.rightDown)) {
        return false;
    }
    x = state.x;
    y = state.y;
    return true;
}

inline bool queuedPointerButtonDown(window::Handle window, int button) {
    const auto iterator = pointerStates().find(window);
    if (iterator == pointerStates().end()) {
        return false;
    }
    return button == 1 ? iterator->second.rightDown : iterator->second.down;
}

} // namespace detail

inline void queuePointerMotion(window::Handle window,
                               double x,
                               double y,
                               bool down,
                               bool rightDown) {
    detail::PointerState& state = detail::pointerState(window);
    state.x = x;
    state.y = y;
    state.down = down;
    state.rightDown = rightDown;
    state.hasPosition = true;
    state.inside = true;
}

inline void queuePointerButton(window::Handle window,
                               double x,
                               double y,
                               int button,
                               bool down) {
    detail::PointerState& state = detail::pointerState(window);
    state.x = x;
    state.y = y;
    state.hasPosition = true;
    state.inside = true;
    if (button == 0) {
        state.down = down;
    } else if (button == 1) {
        state.rightDown = down;
    }
}

inline void queuePointerInput(window::Handle window,
                              double x,
                              double y,
                              bool down,
                              bool rightDown = false) {
    queuePointerMotion(window, x, y, down, rightDown);
}

inline void queuePointerPresence(window::Handle window, bool inside) {
    detail::pointerState(window).inside = inside;
}

inline void clearPointerInput(window::Handle window) {
    const auto iterator = detail::pointerStates().find(window);
    if (iterator == detail::pointerStates().end()) {
        return;
    }
    detail::PointerState& state = iterator->second;
    state.down = false;
    state.rightDown = false;
    state.hasPosition = false;
    state.inside = false;
}

inline void queueTextInput(window::Handle window, const std::string& text) {
    detail::InputQueue& queue = detail::inputQueue(window);
    queue.text += text;
    queue.compositionText.clear();
    queue.compositionChanged = true;
    detail::setComposing(window, false);
    detail::setCompositionText(window, {});
}

inline void queueTextEditing(window::Handle window, const std::string& text) {
    detail::InputQueue& queue = detail::inputQueue(window);
    queue.compositionText = text;
    queue.compositionChanged = true;
    detail::setComposing(window, !text.empty());
    detail::setCompositionText(window, text);
}

inline void queueScrollInput(window::Handle window, double x, double y) {
    detail::InputQueue& queue = detail::inputQueue(window);
    queue.scrollX += x;
    queue.scrollY += y;
}

inline void queueKeyInput(window::Handle window, const KeyEvent& event) {
    detail::InputQueue& queue = detail::inputQueue(window);
    if (event.modifiers.shortcut && event.key == InputKey::V) {
        queue.pasteText += core::window::clipboardText(window);
        return;
    }

    if (detail::isComposing(window) &&
        (event.key == InputKey::Backspace || event.key == InputKey::Delete)) {
        return;
    }

    queue.keys.push_back(event);
}

inline std::pair<KeyboardEvent, ScrollEvent> consumeInputEvents(window::Handle window) {
    detail::InputQueue& queue = detail::inputQueue(window);
    const bool wasComposing = detail::isComposing(window);
    const std::string previousCompositionText = detail::compositionText(window);
    const bool queuedCompositionChanged = queue.compositionChanged;
    KeyboardEvent keyboard;
    keyboard.text = std::move(queue.text);
    keyboard.pasteText = std::move(queue.pasteText);
    keyboard.compositionText = std::move(queue.compositionText);
    keyboard.keys = std::move(queue.keys);
    keyboard.composing = detail::isComposing(window);
    if (!queuedCompositionChanged && keyboard.composing) {
        keyboard.compositionText = previousCompositionText;
    }
    std::string nativeComposition;
    bool nativeComposing = false;
    if (window::queryImeComposition(window, nativeComposition, nativeComposing)) {
        if (nativeComposing) {
            keyboard.compositionText = std::move(nativeComposition);
            keyboard.composing = true;
            detail::setComposing(window, true);
        } else if (!queuedCompositionChanged) {
            keyboard.compositionText.clear();
            keyboard.composing = false;
            detail::setComposing(window, false);
        }
    }
    keyboard.compositionChanged = queuedCompositionChanged ||
                                  wasComposing != keyboard.composing ||
                                  previousCompositionText != keyboard.compositionText;
    detail::setCompositionText(window, keyboard.compositionText);

    ScrollEvent scroll{queue.scrollX, queue.scrollY};
    queue = {};
    return {std::move(keyboard), scroll};
}

inline bool hasPendingPointerInput(window::Handle window, float dpiScale = 1.0f) {
    const auto stateIt = detail::pointerStates().find(window);
    if (stateIt == detail::pointerStates().end()) {
        return false;
    }

    double x = 0.0;
    double y = 0.0;
    bool down = false;
    bool rightDown = false;
    if (stateIt->second.hasPosition) {
        x = stateIt->second.x * dpiScale;
        y = stateIt->second.y * dpiScale;
        down = stateIt->second.down;
        rightDown = stateIt->second.rightDown;
    } else {
        core::window::getCursorPosition(window, x, y);
        x *= dpiScale;
        y *= dpiScale;
        down = core::window::isMouseButtonDown(window, 0);
        rightDown = core::window::isMouseButtonDown(window, 1);
    }

    const detail::PointerState& state = stateIt->second;
    return x != state.lastX ||
           y != state.lastY ||
           down != state.lastDown ||
           rightDown != state.lastRightDown;
}

inline void releaseInputQueue(window::Handle window) {
    window::uninstallInputCallbacks(window);
    detail::inputQueues().erase(window);
    detail::pointerStates().erase(window);
    detail::composingStates().erase(window);
    detail::compositionTextStates().erase(window);
}

inline PointerEvent readPointerEvent(window::Handle window, float dpiScale = 1.0f) {
    detail::PointerState& state = detail::pointerState(window);

    double x = 0.0;
    double y = 0.0;
    bool down = false;
    bool rightDown = false;
    if (state.hasPosition) {
        x = state.x * dpiScale;
        y = state.y * dpiScale;
        down = state.down;
        rightDown = state.rightDown;
    } else {
        core::window::getCursorPosition(window, x, y);
        x *= dpiScale;
        y *= dpiScale;
        down = core::window::isMouseButtonDown(window, 0);
        rightDown = core::window::isMouseButtonDown(window, 1);
    }

    PointerEvent event;
    event.x = x;
    event.y = y;
    event.deltaX = x - state.lastX;
    event.deltaY = y - state.lastY;
    event.down = down;
    event.rightDown = rightDown;
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
