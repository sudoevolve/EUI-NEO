#pragma once

#include "core/input/input_types.h"
#include "core/window/window_backend.h"

#include <array>
#include <unordered_map>
#include <utility>
#include <vector>

namespace core {

namespace detail {

struct QueuedPointerEvent {
    double x = 0.0;
    double y = 0.0;
    PointerAction action = PointerAction::Move;
    PointerButton button = PointerButton::None;
    PointerButtons buttons;
    KeyModifiers modifiers;
};

struct InputQueue {
    std::string text;
    std::string pasteText;
    std::string compositionText;
    std::vector<KeyEvent> keys;
    std::array<bool, static_cast<std::size_t>(InputKey::Count)> keysDown{};
    KeyModifiers modifiers;
    double scrollX = 0.0;
    double scrollY = 0.0;
    bool compositionChanged = false;
};

struct PointerState {
    double dispatchedX = 0.0;
    double dispatchedY = 0.0;
    double x = 0.0;
    double y = 0.0;
    PointerButtons buttons;
    KeyModifiers modifiers;
    std::vector<QueuedPointerEvent> events;
    float dispatchedScale = 1.0f;
    bool dispatchedPositionValid = false;
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

inline InputQueue& inputQueue(window::Handle window) { return inputQueues()[window]; }
inline PointerState& pointerState(window::Handle window) { return pointerStates()[window]; }

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

inline bool queuedPointerPosition(window::Handle window, double& x, double& y) {
    const auto iterator = pointerStates().find(window);
    if (iterator == pointerStates().end()) {
        return false;
    }
    const PointerState& state = iterator->second;
    if (!state.hasPosition || (!state.inside && state.buttons.empty())) {
        return false;
    }
    x = state.x;
    y = state.y;
    return true;
}

inline KeyModifiers currentModifiers(window::Handle window) {
    const auto iterator = inputQueues().find(window);
    return iterator == inputQueues().end() ? KeyModifiers{} : iterator->second.modifiers;
}

inline void pushPointerEvent(PointerState& state, QueuedPointerEvent event) {
    if (event.action == PointerAction::Move && !state.events.empty() &&
        state.events.back().action == PointerAction::Move) {
        state.events.back() = std::move(event);
        return;
    }
    state.events.push_back(std::move(event));
}

} // namespace detail

inline void queuePointerMotion(window::Handle window,
                               double x,
                               double y,
                               PointerButtons buttons,
                               const KeyModifiers& modifiers) {
    detail::PointerState& state = detail::pointerState(window);
    state.x = x;
    state.y = y;
    state.buttons = buttons;
    state.modifiers = modifiers;
    state.hasPosition = true;
    state.inside = true;
    detail::pushPointerEvent(state, {x, y, PointerAction::Move, PointerButton::None, buttons, modifiers});
}

inline void queuePointerButton(window::Handle window,
                               double x,
                               double y,
                               PointerButton button,
                               PointerAction action,
                               const KeyModifiers& modifiers) {
    if (button == PointerButton::None ||
        (action != PointerAction::Press && action != PointerAction::Release)) {
        return;
    }
    detail::PointerState& state = detail::pointerState(window);
    state.x = x;
    state.y = y;
    state.buttons.set(button, action == PointerAction::Press);
    state.modifiers = modifiers;
    state.hasPosition = true;
    state.inside = true;
    detail::pushPointerEvent(state, {x, y, action, button, state.buttons, modifiers});
}

inline void queuePointerPresence(window::Handle window, bool inside) {
    detail::pointerState(window).inside = inside;
}

inline void cancelPointerInput(window::Handle window) {
    detail::PointerState& state = detail::pointerState(window);
    constexpr PointerButton buttons[] = {
        PointerButton::Left, PointerButton::Middle, PointerButton::Right,
        PointerButton::X1, PointerButton::X2
    };
    for (PointerButton button : buttons) {
        if (!state.buttons.contains(button)) {
            continue;
        }
        state.buttons.set(button, false);
        detail::pushPointerEvent(state, {
            state.x, state.y, PointerAction::Cancel, button, state.buttons, state.modifiers
        });
    }
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
    queue.modifiers = event.modifiers;
    const std::size_t index = static_cast<std::size_t>(event.key);
    if (event.key != InputKey::Unknown && index < queue.keysDown.size()) {
        if (event.action == KeyAction::Press) {
            queue.keysDown[index] = true;
        } else if (event.action == KeyAction::Release) {
            queue.keysDown[index] = false;
        }
    }

    if (event.isDown() && event.modifiers.shortcut() && event.key == InputKey::V) {
        queue.pasteText += core::window::clipboardText(window);
    }

    queue.keys.push_back(event);
}

inline void cancelKeyboardInput(window::Handle window) {
    detail::InputQueue& queue = detail::inputQueue(window);
    for (std::size_t index = 1; index < queue.keysDown.size(); ++index) {
        if (!queue.keysDown[index]) {
            continue;
        }
        queue.keysDown[index] = false;
        queue.keys.push_back({static_cast<InputKey>(index), KeyAction::Release, {}, 0});
    }
    queue.modifiers = {};
}

inline void cancelInput(window::Handle window) {
    cancelPointerInput(window);
    cancelKeyboardInput(window);
}

inline std::vector<KeyEvent> consumeKeyEvents(window::Handle window) {
    detail::InputQueue& queue = detail::inputQueue(window);
    std::vector<KeyEvent> events = std::move(queue.keys);
    queue.keys.clear();
    return events;
}

inline TextInputEvent consumeTextInput(window::Handle window) {
    detail::InputQueue& queue = detail::inputQueue(window);
    const bool wasComposing = detail::isComposing(window);
    const std::string previousCompositionText = detail::compositionText(window);
    const bool queuedCompositionChanged = queue.compositionChanged;
    TextInputEvent input;
    input.text = std::move(queue.text);
    input.pasteText = std::move(queue.pasteText);
    input.compositionText = std::move(queue.compositionText);
    input.composing = detail::isComposing(window);
    if (!queuedCompositionChanged && input.composing) {
        input.compositionText = previousCompositionText;
    }
    std::string nativeComposition;
    bool nativeComposing = false;
    if (window::queryImeComposition(window, nativeComposition, nativeComposing)) {
        if (nativeComposing) {
            input.compositionText = std::move(nativeComposition);
            input.composing = true;
            detail::setComposing(window, true);
        } else if (!queuedCompositionChanged) {
            input.compositionText.clear();
            input.composing = false;
            detail::setComposing(window, false);
        }
    }
    input.compositionChanged = queuedCompositionChanged ||
                               wasComposing != input.composing ||
                               previousCompositionText != input.compositionText;
    detail::setCompositionText(window, input.compositionText);

    queue.text.clear();
    queue.pasteText.clear();
    queue.compositionText.clear();
    queue.compositionChanged = false;
    return input;
}

inline ScrollEvent consumeScrollInput(window::Handle window) {
    detail::InputQueue& queue = detail::inputQueue(window);
    ScrollEvent scroll{queue.scrollX, queue.scrollY};
    queue.scrollX = 0.0;
    queue.scrollY = 0.0;
    return scroll;
}

inline std::vector<PointerEvent> consumePointerEvents(window::Handle window, float dpiScale = 1.0f) {
    detail::PointerState& state = detail::pointerState(window);
    std::vector<detail::QueuedPointerEvent> queued = std::move(state.events);
    state.events.clear();
    if (queued.empty()) {
        const bool dispatchPosition = state.inside || !state.buttons.empty();
        queued.push_back({
            dispatchPosition ? state.x : -1000000.0,
            dispatchPosition ? state.y : -1000000.0,
            PointerAction::Move, PointerButton::None,
            state.buttons, state.modifiers
        });
    }

    std::vector<PointerEvent> events;
    events.reserve(queued.size());
    if (state.dispatchedScale != dpiScale) {
        state.dispatchedPositionValid = false;
        state.dispatchedScale = dpiScale;
    }
    for (const detail::QueuedPointerEvent& raw : queued) {
        PointerEvent event;
        event.x = raw.x * dpiScale;
        event.y = raw.y * dpiScale;
        const bool positionValid = raw.x > -999999.0 && raw.y > -999999.0;
        if (positionValid && state.dispatchedPositionValid) {
            event.deltaX = event.x - state.dispatchedX;
            event.deltaY = event.y - state.dispatchedY;
        }
        event.action = raw.action;
        event.button = raw.button;
        event.buttons = raw.buttons;
        event.modifiers = raw.modifiers;
        if (positionValid) {
            state.dispatchedX = event.x;
            state.dispatchedY = event.y;
        }
        state.dispatchedPositionValid = positionValid;
        events.push_back(event);
    }
    return events;
}

inline bool hasPendingPointerInput(window::Handle window, float = 1.0f) {
    const auto state = detail::pointerStates().find(window);
    return state != detail::pointerStates().end() && !state->second.events.empty();
}

inline void releaseInputQueue(window::Handle window) {
    window::uninstallInputCallbacks(window);
    detail::inputQueues().erase(window);
    detail::pointerStates().erase(window);
    detail::composingStates().erase(window);
    detail::compositionTextStates().erase(window);
}

} // namespace core
