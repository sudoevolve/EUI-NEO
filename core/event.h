#pragma once

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#include "core/primitive.h"

#include <cmath>

namespace core {

struct PointerEvent {
    double x = 0.0;
    double y = 0.0;
    double deltaX = 0.0;
    double deltaY = 0.0;
    bool down = false;
};

struct InteractionState {
    bool hover = false;
    bool pressed = false;
    bool clicked = false;
    bool drag = false;
    bool changed = false;
    double dragStartX = 0.0;
    double dragStartY = 0.0;
    double dragDeltaX = 0.0;
    double dragDeltaY = 0.0;

    void update(const Rect& bounds, const PointerEvent& event) {
        const bool oldHover = hover;
        const bool oldPressed = pressed;
        const bool oldDrag = drag;

        clicked = false;
        hover = bounds.contains(event.x, event.y);

        if (hover && event.down && !pressed) {
            dragStartX = event.x;
            dragStartY = event.y;
        }

        clicked = hover && pressed && !event.down;
        pressed = hover && event.down;

        dragDeltaX = event.x - dragStartX;
        dragDeltaY = event.y - dragStartY;
        drag = pressed && (std::fabs(dragDeltaX) > 2.0 || std::fabs(dragDeltaY) > 2.0);

        changed = oldHover != hover || oldPressed != pressed || oldDrag != drag || clicked;
    }
};

inline PointerEvent readPointerEvent(GLFWwindow* window, float dpiScale = 1.0f) {
    static double lastX = 0.0;
    static double lastY = 0.0;

    double x = 0.0;
    double y = 0.0;
    glfwGetCursorPos(window, &x, &y);
    x *= dpiScale;
    y *= dpiScale;

    PointerEvent event;
    event.x = x;
    event.y = y;
    event.deltaX = x - lastX;
    event.deltaY = y - lastY;
    event.down = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

    lastX = x;
    lastY = y;
    return event;
}

} // namespace core
