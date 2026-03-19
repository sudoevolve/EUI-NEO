#pragma once

#include <cstddef>
#include <cstdint>

namespace eui::runtime {

struct WindowMetrics {
    int window_w{1};
    int window_h{1};
    int framebuffer_w{1};
    int framebuffer_h{1};
    float dpi_scale_x{1.0f};
    float dpi_scale_y{1.0f};
    float dpi_scale{1.0f};
};

struct FrameClock {
    std::uint64_t frame_index{0};
    double now_seconds{0.0};
    double delta_seconds{0.0};
};

struct ClipboardTextView {
    const char* data{nullptr};
    std::size_t size{0};
};

enum class NativeWindowKind {
    None,
    GLFW,
    SDL2,
};

struct NativeWindowHandle {
    NativeWindowKind kind{NativeWindowKind::None};
    void* value{nullptr};

    bool valid() const {
        return value != nullptr;
    }
};

class PlatformBackend {
public:
    virtual ~PlatformBackend() = default;

    virtual bool should_close() const = 0;
    virtual void poll_events(bool blocking, double timeout_seconds) = 0;
    virtual WindowMetrics query_metrics() const = 0;
    virtual ClipboardTextView get_clipboard_text() const = 0;
    virtual void set_clipboard_text(const char* text) = 0;
    virtual double now_seconds() const = 0;
    virtual NativeWindowHandle native_window_handle() const = 0;
};

}  // namespace eui::runtime
