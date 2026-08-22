#pragma once

#include "core/render/render_types.h"

#include <cmath>
#include <cstdint>
#include <string>

namespace core {

enum class CursorShape {
    Arrow,
    Hand
};

enum class InputKey : std::uint16_t {
    Unknown,
    Backspace, Tab, Enter, Escape, Space,
    Insert, Delete, Home, End, PageUp, PageDown,
    Left, Right, Up, Down,
    PrintScreen, ScrollLock, Pause, CapsLock, NumLock,
    LeftShift, RightShift, LeftControl, RightControl,
    LeftAlt, RightAlt, LeftSuper, RightSuper, Menu,
    Digit0, Digit1, Digit2, Digit3, Digit4,
    Digit5, Digit6, Digit7, Digit8, Digit9,
    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
    Apostrophe, Comma, Minus, Period, Slash, Semicolon, Equal,
    LeftBracket, Backslash, RightBracket, GraveAccent,
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    F13, F14, F15, F16, F17, F18, F19, F20, F21, F22, F23, F24,
    Numpad0, Numpad1, Numpad2, Numpad3, Numpad4,
    Numpad5, Numpad6, Numpad7, Numpad8, Numpad9,
    NumpadDecimal, NumpadDivide, NumpadMultiply, NumpadSubtract,
    NumpadAdd, NumpadEnter, NumpadEqual,
    Count
};

enum class KeyAction {
    Press,
    Repeat,
    Release
};

struct KeyModifiers {
    bool control = false;
    bool shift = false;
    bool alt = false;
    bool super = false;
    bool capsLock = false;
    bool numLock = false;

    bool shortcut() const {
#if defined(__APPLE__)
        return super;
#else
        return control;
#endif
    }
};

struct KeyEvent {
    InputKey key = InputKey::Unknown;
    KeyAction action = KeyAction::Press;
    KeyModifiers modifiers;
    int scanCode = 0;

    bool isDown() const {
        return action == KeyAction::Press || action == KeyAction::Repeat;
    }
};

struct TextInputEvent {
    std::string text;
    std::string pasteText;
    std::string compositionText;
    bool composing = false;
    bool compositionChanged = false;

    bool hasInput() const {
        return !text.empty() || !pasteText.empty() || compositionChanged || composing ||
               !compositionText.empty();
    }
};

enum class PointerButton : std::uint8_t {
    None,
    Left,
    Middle,
    Right,
    X1,
    X2
};

class PointerButtons {
public:
    constexpr PointerButtons() = default;
    constexpr PointerButtons(PointerButton button) : bits_(bit(button)) {}

    constexpr bool contains(PointerButton button) const {
        return button != PointerButton::None && (bits_ & bit(button)) != 0;
    }

    constexpr bool empty() const { return bits_ == 0; }

    void set(PointerButton button, bool down) {
        if (button == PointerButton::None) {
            return;
        }
        if (down) {
            bits_ |= bit(button);
        } else {
            bits_ &= ~bit(button);
        }
    }

    friend constexpr PointerButtons operator|(PointerButtons left, PointerButtons right) {
        return PointerButtons(left.bits_ | right.bits_);
    }

    friend constexpr bool operator==(PointerButtons left, PointerButtons right) {
        return left.bits_ == right.bits_;
    }

    friend constexpr bool operator!=(PointerButtons left, PointerButtons right) {
        return !(left == right);
    }

private:
    explicit constexpr PointerButtons(std::uint32_t bits) : bits_(bits) {}

    static constexpr std::uint32_t bit(PointerButton button) {
        return button == PointerButton::None
            ? 0u
            : 1u << (static_cast<std::uint32_t>(button) - 1u);
    }

    std::uint32_t bits_ = 0;
};

constexpr PointerButtons operator|(PointerButton left, PointerButton right) {
    return PointerButtons(left) | PointerButtons(right);
}

enum class PointerAction {
    Move,
    Press,
    Release,
    Cancel
};

struct PointerEvent {
    double x = 0.0;
    double y = 0.0;
    double deltaX = 0.0;
    double deltaY = 0.0;
    PointerAction action = PointerAction::Move;
    PointerButton button = PointerButton::None;
    PointerButtons buttons;
    KeyModifiers modifiers;

    bool isDown(PointerButton value) const { return buttons.contains(value); }
    bool isPress(PointerButton value) const {
        return action == PointerAction::Press && button == value;
    }
    bool isRelease(PointerButton value) const {
        return action == PointerAction::Release && button == value;
    }
};

struct ScrollEvent {
    double x = 0.0;
    double y = 0.0;

    bool active() const { return x != 0.0 || y != 0.0; }
};

struct InteractionState {
    bool hover = false;
    bool pressed = false;
    bool clicked = false;
    bool pressStarted = false;
    bool released = false;
    bool canceled = false;
    bool drag = false;
    bool active = false;
    bool changed = false;
    PointerButton activeButton = PointerButton::None;
    double dragStartX = 0.0;
    double dragStartY = 0.0;
    double dragDeltaX = 0.0;
    double dragDeltaY = 0.0;

    void update(const Rect& bounds,
                const PointerEvent& event,
                bool topmostHover,
                PointerButtons acceptedButtons,
                double dragThreshold = 2.0,
                bool enabled = true) {
        const bool oldHover = hover;
        const bool oldPressed = pressed;
        const bool oldDrag = drag;
        const bool oldActive = active;
        const PointerButton oldActiveButton = activeButton;

        clicked = false;
        pressStarted = false;
        released = false;
        canceled = false;

        if (!enabled) {
            hover = false;
            pressed = false;
            drag = false;
            active = false;
            activeButton = PointerButton::None;
            dragDeltaX = 0.0;
            dragDeltaY = 0.0;
            changed = oldHover != hover || oldPressed != pressed || oldDrag != drag ||
                      oldActive != active || oldActiveButton != activeButton;
            return;
        }

        hover = topmostHover && bounds.contains(event.x, event.y);
        if (!active && hover && event.action == PointerAction::Press &&
            acceptedButtons.contains(event.button)) {
            active = true;
            activeButton = event.button;
            pressStarted = true;
            dragStartX = event.x;
            dragStartY = event.y;
        }

        pressed = active && event.buttons.contains(activeButton);
        dragDeltaX = event.x - dragStartX;
        dragDeltaY = event.y - dragStartY;
        drag = pressed && (std::fabs(dragDeltaX) > dragThreshold ||
                           std::fabs(dragDeltaY) > dragThreshold);

        const bool matchingEnd = active && event.button == activeButton &&
            (event.action == PointerAction::Release || event.action == PointerAction::Cancel);
        if (matchingEnd) {
            released = true;
            canceled = event.action == PointerAction::Cancel;
            clicked = !canceled && hover;
            active = false;
            pressed = false;
            drag = false;
            activeButton = PointerButton::None;
        }

        changed = oldHover != hover || oldPressed != pressed || oldDrag != drag ||
                  oldActive != active || oldActiveButton != activeButton || pressStarted ||
                  released || clicked || canceled;
    }
};

} // namespace core
