#pragma once

#include "core/dsl.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>

namespace components {

struct MouseEvent {
    float x = 0.0f;
    float y = 0.0f;
    float globalX = 0.0f;
    float globalY = 0.0f;
    float deltaX = 0.0f;
    float deltaY = 0.0f;
    core::Rect bounds;
    bool down = false;
    bool rightDown = false;
    bool inside = false;
};

struct MouseDragEvent : MouseEvent {
    float totalX = 0.0f;
    float totalY = 0.0f;
};

struct MouseScrollEvent {
    float deltaX = 0.0f;
    float deltaY = 0.0f;
    float stepX = 0.0f;
    float stepY = 0.0f;
};

class MouseAreaBuilder {
public:
    MouseAreaBuilder(core::dsl::Ui& ui, std::string id)
        : ui_(ui), id_(std::move(id)) {}

    MouseAreaBuilder& x(float value) { x_ = value; hasX_ = true; return *this; }
    MouseAreaBuilder& y(float value) { y_ = value; hasY_ = true; return *this; }
    MouseAreaBuilder& position(float xValue, float yValue) { return x(xValue).y(yValue); }
    MouseAreaBuilder& size(float width, float height) { width_ = width; height_ = height; return *this; }
    MouseAreaBuilder& width(float value) { width_ = value; return *this; }
    MouseAreaBuilder& height(float value) { height_ = value; return *this; }
    MouseAreaBuilder& zIndex(int value) { zIndex_ = value; hasZIndex_ = true; return *this; }
    MouseAreaBuilder& radius(float value) { radius_ = std::max(0.0f, value); return *this; }
    MouseAreaBuilder& color(const core::Color& value) { color_ = value; return *this; }
    MouseAreaBuilder& cursor(core::CursorShape value) { cursor_ = value; return *this; }
    MouseAreaBuilder& disabled(bool value = true) { disabled_ = value; return *this; }
    MouseAreaBuilder& preserveFocusOnPress(bool value = true) { preserveFocusOnPress_ = value; return *this; }
    MouseAreaBuilder& scrollStep(float value) { scrollStep_ = std::max(0.0f, value); return *this; }
    MouseAreaBuilder& maxScrollStep(float value) { maxScrollStep_ = std::max(0.0f, value); return *this; }
    MouseAreaBuilder& dragThreshold(float value) { dragThreshold_ = std::max(0.0f, value); return *this; }
    MouseAreaBuilder& suppressClickAfterDrag(bool value = true) { suppressClickAfterDrag_ = value; return *this; }

    MouseAreaBuilder& onTap(std::function<void()> callback) { onClick_ = std::move(callback); return *this; }
    MouseAreaBuilder& onTap(std::function<void(const MouseEvent&)> callback) { onClickAt_ = std::move(callback); return *this; }
    MouseAreaBuilder& onPress(std::function<void(const MouseEvent&)> callback) { onPress_ = std::move(callback); return *this; }
    MouseAreaBuilder& onRelease(std::function<void(const MouseEvent&)> callback) { onRelease_ = std::move(callback); return *this; }
    MouseAreaBuilder& onHover(std::function<void(bool)> callback) { onHover_ = std::move(callback); return *this; }
    MouseAreaBuilder& onEnter(std::function<void()> callback) { onEnter_ = std::move(callback); return *this; }
    MouseAreaBuilder& onLeave(std::function<void()> callback) { onLeave_ = std::move(callback); return *this; }
    MouseAreaBuilder& onMove(std::function<bool(const MouseEvent&)> callback) { onMove_ = std::move(callback); return *this; }
    template <typename Callback,
              typename = std::enable_if_t<!std::is_convertible_v<Callback, std::function<bool(const MouseEvent&)>>>>
    MouseAreaBuilder& onMove(Callback callback) {
        onMove_ = [callback = std::move(callback)](const MouseEvent& event) mutable {
            callback(event);
            return true;
        };
        return *this;
    }
    MouseAreaBuilder& onDragStart(std::function<void(const MouseEvent&)> callback) { onDragStart_ = std::move(callback); return *this; }
    MouseAreaBuilder& onDrag(std::function<void(const MouseDragEvent&)> callback) { onDrag_ = std::move(callback); return *this; }
    MouseAreaBuilder& onDragEnd(std::function<void(const MouseDragEvent&)> callback) { onDragEnd_ = std::move(callback); return *this; }
    MouseAreaBuilder& onScroll(std::function<void(const MouseScrollEvent&)> callback) { onScroll_ = std::move(callback); return *this; }
    MouseAreaBuilder& onContextMenu(std::function<void(const MouseEvent&)> callback) { onContextMenu_ = std::move(callback); return *this; }

    void build() {
        MouseAreaState* state = &ui_.state<MouseAreaState>(id_);
        const float safeWidth = std::max(1.0f, width_);
        const float safeHeight = std::max(1.0f, height_);
        const float scrollStep = scrollStep_;
        const float maxScrollStep = maxScrollStep_;
        const float dragThreshold = dragThreshold_;
        const bool suppressClickAfterDrag = suppressClickAfterDrag_;
        const std::function<void()> onClick = onClick_;
        const std::function<void(const MouseEvent&)> onClickAt = onClickAt_;
        const std::function<void(const MouseEvent&)> onPress = onPress_;
        const std::function<void(const MouseEvent&)> onRelease = onRelease_;
        const std::function<void(bool)> onHover = onHover_;
        const std::function<void()> onEnter = onEnter_;
        const std::function<void()> onLeave = onLeave_;
        const std::function<bool(const MouseEvent&)> onMove = onMove_;
        const std::function<void(const MouseEvent&)> onDragStart = onDragStart_;
        const std::function<void(const MouseDragEvent&)> onDrag = onDrag_;
        const std::function<void(const MouseDragEvent&)> onDragEnd = onDragEnd_;
        const std::function<void(const MouseScrollEvent&)> onScroll = onScroll_;
        const std::function<void(const MouseEvent&)> onContextMenu = onContextMenu_;

        auto area = ui_.rect(id_);
        if (hasX_) {
            area.x(x_);
        }
        if (hasY_) {
            area.y(y_);
        }
        if (hasZIndex_) {
            area.zIndex(zIndex_);
        }

        area.size(safeWidth, safeHeight)
            .color(color_)
            .radius(radius_)
            .disabled(disabled_)
            .preserveFocusOnPress(preserveFocusOnPress_)
            .interactive()
            .cursor(cursor_);

        if (onClick || onClickAt || onPress || onDragStart || onDrag || onDragEnd || onRelease) {
            area.onPress([state, safeWidth, safeHeight, onPress](const core::PointerEvent& event, const core::Rect& bounds) {
                state->bounds = bounds;
                state->width = safeWidth;
                state->height = safeHeight;
                state->pressed = true;
                state->dragging = false;
                state->dragged = false;
                state->lastDrag = {};
                state->lastEvent = makeMouseEvent(event, bounds, safeWidth, safeHeight);
                state->pressEvent = state->lastEvent;
                if (onPress) {
                    onPress(state->lastEvent);
                }
            });
        }

        if (onClick || onClickAt || onRelease || onDragEnd) {
            area.onRelease([state, onClick, onClickAt, onRelease, onDragEnd, suppressClickAfterDrag](const core::PointerEvent& event, const core::Rect& bounds) {
                state->lastEvent = makeMouseEvent(event, bounds, state->width, state->height);
                if (state->lastEvent.inside && (!suppressClickAfterDrag || !state->dragged)) {
                    if (onClick) {
                        onClick();
                    }
                    if (onClickAt) {
                        onClickAt(state->lastEvent);
                    }
                }
                if (onRelease) {
                    onRelease(state->lastEvent);
                }
                if (onDragEnd && state->dragging) {
                    MouseDragEvent drag = makeMouseDragEvent(event, state->bounds, state->width, state->height, 0.0, 0.0);
                    drag.totalX = state->lastDrag.totalX;
                    drag.totalY = state->lastDrag.totalY;
                    onDragEnd(drag);
                }
                state->pressed = false;
                state->dragging = false;
            });
        }

        if (onHover || onEnter || onLeave) {
            area.onHover([onHover, onEnter, onLeave](bool hover) {
                if (onHover) {
                    onHover(hover);
                }
                if (hover && onEnter) {
                    onEnter();
                }
                if (!hover && onLeave) {
                    onLeave();
                }
            });
        }

        if (onMove) {
            area.onMove([safeWidth, safeHeight, onMove](const core::PointerEvent& event, const core::Rect& bounds) {
                return onMove(makeMouseEvent(event, bounds, safeWidth, safeHeight));
            });
        }

        if (onDrag || onDragStart || onDragEnd) {
            area.onDrag([state, safeWidth, safeHeight, onDrag, onDragStart, dragThreshold](const core::dsl::DragEvent& event) {
                if (state->bounds.width <= 0.0f || state->bounds.height <= 0.0f) {
                    state->bounds = {0.0f, 0.0f, safeWidth, safeHeight};
                    state->width = safeWidth;
                    state->height = safeHeight;
                }
                MouseDragEvent drag = makeMouseDragEvent(event, state->bounds, state->width, state->height);
                if (!state->dragged && (std::fabs(drag.totalX) >= dragThreshold || std::fabs(drag.totalY) >= dragThreshold)) {
                    state->dragged = true;
                }
                if (!state->dragged) {
                    return;
                }
                if (!state->dragging) {
                    if (onDragStart) {
                        onDragStart(state->pressEvent);
                    }
                    state->dragging = true;
                }
                state->lastEvent = drag;
                state->lastDrag = drag;
                if (onDrag) {
                    onDrag(drag);
                }
            });
        }

        if (onScroll) {
            area.onScroll([onScroll, scrollStep, maxScrollStep](const core::ScrollEvent& event) {
                MouseScrollEvent scroll;
                scroll.deltaX = static_cast<float>(event.x);
                scroll.deltaY = static_cast<float>(event.y);
                scroll.stepX = std::clamp(scroll.deltaX, -maxScrollStep, maxScrollStep) * scrollStep;
                scroll.stepY = std::clamp(scroll.deltaY, -maxScrollStep, maxScrollStep) * scrollStep;
                onScroll(scroll);
            });
        }

        if (onContextMenu) {
            area.onContextMenu([safeWidth, safeHeight, onContextMenu](const core::PointerEvent& event, const core::Rect& bounds) {
                onContextMenu(makeMouseEvent(event, bounds, safeWidth, safeHeight));
            });
        }

        area.build();
    }

private:
    struct MouseAreaState {
        core::Rect bounds;
        float width = 1.0f;
        float height = 1.0f;
        bool pressed = false;
        bool dragging = false;
        bool dragged = false;
        MouseEvent pressEvent;
        MouseEvent lastEvent;
        MouseDragEvent lastDrag;
    };

    static float scaleFor(float actual, float expected) {
        return actual > 0.0f && expected > 0.0f ? actual / expected : 1.0f;
    }

    static MouseEvent makeMouseEvent(const core::PointerEvent& event, const core::Rect& bounds, float width, float height) {
        const float scaleX = scaleFor(bounds.width, width);
        const float scaleY = scaleFor(bounds.height, height);
        MouseEvent result;
        result.x = static_cast<float>((event.x - bounds.x) / std::max(0.001f, scaleX));
        result.y = static_cast<float>((event.y - bounds.y) / std::max(0.001f, scaleY));
        result.globalX = static_cast<float>(event.x / std::max(0.001f, scaleX));
        result.globalY = static_cast<float>(event.y / std::max(0.001f, scaleY));
        result.deltaX = static_cast<float>(event.deltaX / std::max(0.001f, scaleX));
        result.deltaY = static_cast<float>(event.deltaY / std::max(0.001f, scaleY));
        result.bounds = {
            static_cast<float>(bounds.x / std::max(0.001f, scaleX)),
            static_cast<float>(bounds.y / std::max(0.001f, scaleY)),
            width,
            height
        };
        result.down = event.down;
        result.rightDown = event.rightDown;
        result.inside = result.x >= 0.0f && result.y >= 0.0f && result.x <= width && result.y <= height;
        return result;
    }

    static MouseDragEvent makeMouseDragEvent(const core::dsl::DragEvent& event, const core::Rect& bounds, float width, float height) {
        const float scaleX = scaleFor(bounds.width, width);
        const float scaleY = scaleFor(bounds.height, height);
        MouseDragEvent result;
        result.x = static_cast<float>((event.x - bounds.x) / std::max(0.001f, scaleX));
        result.y = static_cast<float>((event.y - bounds.y) / std::max(0.001f, scaleY));
        result.globalX = static_cast<float>(event.x / std::max(0.001f, scaleX));
        result.globalY = static_cast<float>(event.y / std::max(0.001f, scaleY));
        result.deltaX = static_cast<float>(event.deltaX / std::max(0.001f, scaleX));
        result.deltaY = static_cast<float>(event.deltaY / std::max(0.001f, scaleY));
        result.totalX = static_cast<float>(event.totalX / std::max(0.001f, scaleX));
        result.totalY = static_cast<float>(event.totalY / std::max(0.001f, scaleY));
        result.bounds = {
            static_cast<float>(bounds.x / std::max(0.001f, scaleX)),
            static_cast<float>(bounds.y / std::max(0.001f, scaleY)),
            width,
            height
        };
        result.down = true;
        result.inside = result.x >= 0.0f && result.y >= 0.0f && result.x <= width && result.y <= height;
        return result;
    }

    static MouseDragEvent makeMouseDragEvent(const core::PointerEvent& event,
                                             const core::Rect& bounds,
                                             float width,
                                             float height,
                                             double totalX,
                                             double totalY) {
        MouseDragEvent result;
        static_cast<MouseEvent&>(result) = makeMouseEvent(event, bounds, width, height);
        const float scaleX = scaleFor(bounds.width, width);
        const float scaleY = scaleFor(bounds.height, height);
        result.totalX = static_cast<float>(totalX / std::max(0.001f, scaleX));
        result.totalY = static_cast<float>(totalY / std::max(0.001f, scaleY));
        return result;
    }

    core::dsl::Ui& ui_;
    std::string id_;
    core::Color color_ = {0.0f, 0.0f, 0.0f, 0.0f};
    core::CursorShape cursor_ = core::CursorShape::Hand;
    std::function<void()> onClick_;
    std::function<void(const MouseEvent&)> onClickAt_;
    std::function<void(const MouseEvent&)> onPress_;
    std::function<void(const MouseEvent&)> onRelease_;
    std::function<void(bool)> onHover_;
    std::function<void()> onEnter_;
    std::function<void()> onLeave_;
    std::function<bool(const MouseEvent&)> onMove_;
    std::function<void(const MouseEvent&)> onDragStart_;
    std::function<void(const MouseDragEvent&)> onDrag_;
    std::function<void(const MouseDragEvent&)> onDragEnd_;
    std::function<void(const MouseScrollEvent&)> onScroll_;
    std::function<void(const MouseEvent&)> onContextMenu_;
    float x_ = 0.0f;
    float y_ = 0.0f;
    float width_ = 1.0f;
    float height_ = 1.0f;
    float radius_ = 0.0f;
    float scrollStep_ = 1.0f;
    float maxScrollStep_ = 1.0f;
    float dragThreshold_ = 2.0f;
    int zIndex_ = 0;
    bool hasX_ = false;
    bool hasY_ = false;
    bool hasZIndex_ = false;
    bool disabled_ = false;
    bool preserveFocusOnPress_ = false;
    bool suppressClickAfterDrag_ = true;
};

inline MouseAreaBuilder mouseArea(core::dsl::Ui& ui, const std::string& id) {
    return MouseAreaBuilder(ui, id);
}

} // namespace components
