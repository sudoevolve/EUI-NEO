#pragma once

#include "eui/core/context_fwd.h"
#include "eui/runtime/contracts.h"

namespace eui::runtime {

struct FrameContext {
    eui::Context* ui{nullptr};
    NativeWindowHandle native_window{};
    FrameClock clock{};
    WindowMetrics metrics{};
    bool* repaint_flag{nullptr};

    eui::Context& context() const {
        return *ui;
    }

    double delta_seconds() const {
        return clock.delta_seconds;
    }

    float delta_seconds_f32() const {
        return static_cast<float>(clock.delta_seconds);
    }

    void request_next_frame() const {
        if (repaint_flag != nullptr) {
            *repaint_flag = true;
        }
    }

    WindowMetrics window_metrics() const {
        return metrics;
    }

    NativeWindowHandle native_window_handle() const {
        return native_window;
    }
};

}  // namespace eui::runtime
