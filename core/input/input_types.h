#pragma once

#include "core/render/render_types.h"

#include <cmath>
#include <string>
#include <vector>

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

enum class InputKey {
    Backspace,
    Delete,
    Enter,
    Left,
    Right,
    Up,
    Down,
    Home,
    End,
    Escape,
    A,
    C,
    V,
    X,
    Y,
    Z
};

enum class KeyAction {
    Press,
    Repeat
};

struct KeyModifiers {
    bool shortcut = false;
    bool shift = false;
};

struct KeyEvent {
    InputKey key = InputKey::Enter;
    KeyAction action = KeyAction::Press;
    KeyModifiers modifiers;
};

struct KeyboardEvent {
    std::string text;
    std::string pasteText;
    std::string compositionText;
    std::vector<KeyEvent> keys;
    bool composing = false;
    bool compositionChanged = false;

    const KeyEvent* findKey(InputKey key) const {
        for (const KeyEvent& event : keys) {
            if (event.key == key) {
                return &event;
            }
        }
        return nullptr;
    }

    bool hasKey(InputKey key) const {
        return findKey(key) != nullptr;
    }

    bool hasShortcut(InputKey key) const {
        for (const KeyEvent& event : keys) {
            if (event.key == key && event.modifiers.shortcut) {
                return true;
            }
        }
        return false;
    }

    bool hasUnshiftedShortcut(InputKey key) const {
        for (const KeyEvent& event : keys) {
            if (event.key == key && event.modifiers.shortcut && !event.modifiers.shift) {
                return true;
            }
        }
        return false;
    }

    bool hasShiftedShortcut(InputKey key) const {
        for (const KeyEvent& event : keys) {
            if (event.key == key && event.modifiers.shortcut && event.modifiers.shift) {
                return true;
            }
        }
        return false;
    }

    bool hasInput() const {
        return !text.empty() || !pasteText.empty() || compositionChanged || composing ||
               !compositionText.empty() || !keys.empty();
    }
};

struct ScrollEvent {
    double x = 0.0;
    double y = 0.0;

    bool active() const {
        return x != 0.0 || y != 0.0;
    }
};

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

} // namespace core
