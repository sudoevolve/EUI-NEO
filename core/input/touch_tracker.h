#pragma once

#include "core/input/input_state.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace core::input {

/** @brief 将单个窗口、同一设备的触点转换为单指 pointer 和多指增量手势。 */
class TouchTracker {
public:
    explicit TouchTracker(window::Handle window) : window_(window) {}

    void down(std::int64_t device, std::int64_t id, double x, double y) {
        if ((!fingers_.empty() && fingers_.front().device != device) ||
            find(device, id) != fingers_.end()) {
            return;
        }
        if (!std::isfinite(x) || !std::isfinite(y)) return;
        fingers_.push_back({device, id, x, y, x, y});
        if (fingers_.size() == 1) {
            queuePointerButton(window_, x, y, PointerButton::Left, PointerAction::Press,
                               detail::currentModifiers(window_), true);
        } else if (!active_) {
            if (!consumed_) {
                cancelTouchPointerInput(window_);
                clearTouchScrollInput(window_);
            }
            consumed_ = true;
            active_ = true;
            emit(GesturePhase::Begin);
        } else {
            rebase();
        }
    }

    void move(std::int64_t device, std::int64_t id, double x, double y, float pointerScale) {
        if (!std::isfinite(x) || !std::isfinite(y)) return;
        auto finger = find(device, id);
        if (finger == fingers_.end()) return;
        const double dx = x - finger->x;
        const double dy = y - finger->y;
        finger->x = x;
        finger->y = y;
        if (consumed_) {
            if (active_) emit(GesturePhase::Update);
        } else {
            queueTouchScrollInput(window_, dx * pointerScale, dy * pointerScale);
            queuePointerMotion(window_, x, y, PointerButton::Left,
                               detail::currentModifiers(window_), true);
        }
    }

    void up(std::int64_t device, std::int64_t id, double x, double y) {
        auto finger = find(device, id);
        if (finger == fingers_.end()) return;
        if (!std::isfinite(x) || !std::isfinite(y)) {
            x = finger->x;
            y = finger->y;
        }
        const bool moved = finger->x != x || finger->y != y;
        finger->x = x;
        finger->y = y;
        if (!consumed_) {
            queuePointerButton(window_, x, y, PointerButton::Left, PointerAction::Release,
                               detail::currentModifiers(window_), true);
        }
        if (active_ && moved) emit(GesturePhase::Update);
        if (active_ && fingers_.size() == 2) {
            emit(GesturePhase::End);
            active_ = false;
        }
        fingers_.erase(finger);
        if (active_) rebase();
        if (fingers_.empty()) consumed_ = false;
    }

    void reset() {
        fingers_.clear();
        active_ = false;
        consumed_ = false;
    }

    bool empty() const { return fingers_.empty(); }

    void rescale(double xScale, double yScale) {
        for (Finger& finger : fingers_) {
            finger.x *= xScale;
            finger.y *= yScale;
        }
        if (active_) rebase();
    }

private:
    struct Finger {
        std::int64_t device;
        std::int64_t id;
        double x;
        double y;
        double previousX;
        double previousY;
    };

    using Iterator = std::vector<Finger>::iterator;
    Iterator find(std::int64_t device, std::int64_t id) {
        return std::find_if(fingers_.begin(), fingers_.end(), [&](const Finger& finger) {
            return finger.device == device && finger.id == id;
        });
    }

    void geometry(double& x, double& y) const {
        x = 0.0;
        y = 0.0;
        for (const Finger& finger : fingers_) {
            x += finger.x;
            y += finger.y;
        }
        x /= static_cast<double>(fingers_.size());
        y /= static_cast<double>(fingers_.size());
    }

    void rebase() {
        geometry(centerX_, centerY_);
        for (Finger& finger : fingers_) {
            finger.previousX = finger.x;
            finger.previousY = finger.y;
        }
    }

    void emit(GesturePhase phase) {
        double x, y;
        geometry(x, y);
        GestureEvent event;
        event.phase = phase;
        event.x = x;
        event.y = y;
        event.touchCount = static_cast<unsigned int>(fingers_.size());
        if (phase == GesturePhase::Update) {
            event.deltaX = x - centerX_;
            event.deltaY = y - centerY_;
            // 全部触点参与几何计算，避免第三根手指移动时只产生平移。
            double previousSpread = 0.0, spread = 0.0, dot = 0.0, cross = 0.0;
            for (const Finger& finger : fingers_) {
                const double px = finger.previousX - centerX_;
                const double py = finger.previousY - centerY_;
                const double nx = finger.x - x;
                const double ny = finger.y - y;
                previousSpread += px * px + py * py;
                spread += nx * nx + ny * ny;
                dot += px * nx + py * ny;
                cross += px * ny - py * nx;
            }
            if (previousSpread > 0.000001 && spread > 0.000001) {
                event.scale = std::sqrt(spread / previousSpread);
                event.rotation = std::atan2(cross, dot) * (180.0 / 3.14159265358979323846);
            }
        }
        queueGestureInput(window_, event);
        rebase();
    }

    window::Handle window_;
    std::vector<Finger> fingers_;
    bool active_ = false;
    bool consumed_ = false;
    double centerX_ = 0.0;
    double centerY_ = 0.0;
};

} // namespace core::input
