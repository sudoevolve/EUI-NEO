#pragma once

namespace eui::app {

enum class WindowBackend {
    Auto,
    GLFW,
    SDL2,
};

struct AppOptions {
    enum class TextBackend {
        Auto,
        Stb,
        Win32,
    };

    int width{1150};
    int height{820};
    const char* title{"EUI App"};
    bool vsync{true};
    bool continuous_render{false};
    double idle_wait_seconds{0.25};
    double max_fps{60.0};
    bool enable_dirty_cache{true};
    WindowBackend window_backend{WindowBackend::Auto};
    const char* text_font_family{"Segoe UI"};
    int text_font_weight{600};
    const char* text_font_file{nullptr};
    const char* icon_font_family{"Font Awesome 7 Free Solid"};
    const char* icon_font_file{"include/Font Awesome 7 Free-Solid-900.otf"};
    bool enable_icon_font_fallback{true};
    TextBackend text_backend{TextBackend::Auto};
};

}  // namespace eui::app
