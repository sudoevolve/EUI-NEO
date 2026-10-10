#include "eui_neo.h"

#if defined(EUI_WINDOW_BACKEND_SDL2)
#include <SDL.h>
#else
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#endif

namespace app {
namespace {

struct WindowControls {
    int minWidth = 720, minHeight = 520;
    int maxWidth = 1400, maxHeight = 1000;
    int savedX = 100, savedY = 100, savedWidth = 1000, savedHeight = 720;
    bool fullscreen = false, decorated = true;
    std::string status = "Drag the window edges to test the size limits.";
#if defined(EUI_WINDOW_BACKEND_SDL2)
    SDL_Window* window = nullptr;
#else
    GLFWwindow* window = nullptr;
#endif

    void capture() {
        if (window) return;
#if defined(EUI_WINDOW_BACKEND_SDL2)
        window = SDL_GetKeyboardFocus();
#else
        // 此 fixture 通过当前 OpenGL context 获取应用窗口，不替换框架事件回调。
        window = glfwGetCurrentContext();
#endif
    }

    void resize(int width, int height) {
        if (!window || fullscreen) return;
#if defined(EUI_WINDOW_BACKEND_SDL2)
        SDL_SetWindowSize(window, width, height);
#else
        glfwSetWindowSize(window, width, height);
#endif
        requestUpdate();
    }

    void applyLimits() {
        if (!window) return;
#if defined(EUI_WINDOW_BACKEND_SDL2)
        SDL_SetWindowMinimumSize(window, minWidth, minHeight);
        SDL_SetWindowMaximumSize(window, maxWidth, maxHeight);
#else
        glfwSetWindowSizeLimits(window, minWidth, minHeight, maxWidth, maxHeight);
#endif
        status = "Applied limits: " + std::to_string(minWidth) + " x " + std::to_string(minHeight) +
            "  to  " + std::to_string(maxWidth) + " x " + std::to_string(maxHeight);
        requestUpdate();
    }

    void toggleBorder() {
        if (!window || fullscreen) return;
        decorated = !decorated;
#if defined(EUI_WINDOW_BACKEND_SDL2)
        SDL_SetWindowBordered(window, decorated ? SDL_TRUE : SDL_FALSE);
#else
        glfwSetWindowAttrib(window, GLFW_DECORATED, decorated ? GLFW_TRUE : GLFW_FALSE);
#endif
        requestUpdate();
    }

    void toggleFullscreen() {
        if (!window) return;
        if (!fullscreen) {
#if defined(EUI_WINDOW_BACKEND_SDL2)
            SDL_GetWindowPosition(window, &savedX, &savedY);
            SDL_GetWindowSize(window, &savedWidth, &savedHeight);
            if (SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP) != 0) {
                status = SDL_GetError();
                return;
            }
#else
            glfwGetWindowPos(window, &savedX, &savedY);
            glfwGetWindowSize(window, &savedWidth, &savedHeight);
            GLFWmonitor* monitor = glfwGetPrimaryMonitor();
            int count = 0, bestOverlap = -1;
            GLFWmonitor** monitors = glfwGetMonitors(&count);
            for (int i = 0; i < count; ++i) {
                int x = 0, y = 0;
                glfwGetMonitorPos(monitors[i], &x, &y);
                const auto* mode = glfwGetVideoMode(monitors[i]);
                if (!mode) continue;
                const int overlap = std::max(0, std::min(savedX + savedWidth, x + mode->width) - std::max(savedX, x)) *
                    std::max(0, std::min(savedY + savedHeight, y + mode->height) - std::max(savedY, y));
                if (overlap > bestOverlap) { monitor = monitors[i]; bestOverlap = overlap; }
            }
            const auto* mode = monitor ? glfwGetVideoMode(monitor) : nullptr;
            if (!mode) { status = "No monitor is available."; return; }
            glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
#endif
            fullscreen = true;
        } else {
#if defined(EUI_WINDOW_BACKEND_SDL2)
            if (SDL_SetWindowFullscreen(window, 0) != 0) { status = SDL_GetError(); return; }
            SDL_SetWindowPosition(window, savedX, savedY);
            SDL_SetWindowSize(window, savedWidth, savedHeight);
#else
            glfwSetWindowMonitor(window, nullptr, savedX, savedY, savedWidth, savedHeight, GLFW_DONT_CARE);
#endif
            fullscreen = false;
            applyLimits();
        }
        requestUpdate();
    }

    void restore() {
        if (fullscreen) toggleFullscreen();
        if (!decorated) toggleBorder();
        resize(1000, 720);
    }

    void beginMove() {
        if (!window || fullscreen) return;
        status = eui::window::beginWindowMove(window)
            ? "Compositor move started: drag the window past the screen edges."
            : "Compositor move is unavailable on this platform/backend.";
        requestUpdate();
    }

    void beginResize(eui::window::WindowResizeEdge edge, const char* name) {
        if (!window || fullscreen) return;
        status = eui::window::beginWindowResize(window, edge)
            ? (std::string("Compositor resize started (") + name + "); move the pointer.")
            : "Compositor resize is unavailable on this platform/backend.";
        requestUpdate();
    }

    void close() {
        if (!window) return;
#if defined(EUI_WINDOW_BACKEND_SDL2)
        SDL_Event event{};
        event.type = SDL_QUIT;
        SDL_PushEvent(&event);
#else
        glfwSetWindowShouldClose(window, GLFW_TRUE);
#endif
    }
};

WindowControls controls;

void label(eui::Ui& ui, const std::string& id, const std::string& text, float size = 16.f) {
    ui.text(id).size(620.f, size + 12.f).fontSize(size).color("#E2E8F0").text(text).build();
}

void dimension(eui::Ui& ui, const std::string& id, const char* title, int& value, int minimum, int maximum) {
    ui.column(id).gap(6.f).content([&] {
        ui.text(id + ".label").size(240.f, 24.f).fontSize(15.f).color("#94A3B8").text(title).build();
        components::stepper(ui, id + ".value").size(250.f, 38.f).value(value)
            .step(20).min(minimum).max(maximum).onChange([&value](long long next) { value = static_cast<int>(next); }).build();
    }).build();
}

} // namespace

const DslAppConfig& dslAppConfig() {
#if defined(EUI_WINDOW_BACKEND_SDL2)
    constexpr const char* title = "EUI Window Controls Test - SDL2";
#else
    constexpr const char* title = "EUI Window Controls Test - GLFW";
#endif
    static const auto config = DslAppConfig{}.title(title).windowSize(1000, 720)
        .windowPosition(100, 100).minWindowSize(720, 520).maxWindowSize(1400, 1000)
        .clearColor("#0F172A").showDebugStatsInTitle(false).onKeyEvent([](const eui::KeyEvent& event) {
            if (event.action != core::KeyAction::Press) return;
            if (event.key == core::InputKey::F11) controls.toggleFullscreen();
            if (event.key == core::InputKey::Escape) controls.restore();
        });
    return config;
}

void compose(eui::Ui& ui, const eui::Screen& screen) {
    controls.capture();
    ui.column("window.controls").position(24.f, 20.f).size(screen.width - 48.f, screen.height - 40.f)
        .gap(12.f).content([&] {
            label(ui, "title", "Window controls", 28.f);
            label(ui, "size", "UI size: " + std::to_string(static_cast<int>(screen.width)) + " x " +
                std::to_string(static_cast<int>(screen.height)) + "   |   " +
                (controls.fullscreen ? "FULLSCREEN" : controls.decorated ? "WINDOWED" : "BORDERLESS"));
            ui.row("limits.width").gap(20.f).content([&] {
                dimension(ui, "min.width", "Minimum width", controls.minWidth, 640, controls.maxWidth);
                dimension(ui, "max.width", "Maximum width", controls.maxWidth, controls.minWidth, 3840);
            }).build();
            ui.row("limits.height").gap(20.f).content([&] {
                dimension(ui, "min.height", "Minimum height", controls.minHeight, 480, controls.maxHeight);
                dimension(ui, "max.height", "Maximum height", controls.maxHeight, controls.minHeight, 2160);
            }).build();
            ui.row("sizes").gap(12.f).content([&] {
                components::button(ui, "apply").size(160, 38).text("Apply limits").onClick([] { controls.applyLimits(); }).build();
                components::button(ui, "minimum").size(160, 38).text("Minimum size")
                    .onClick([] { controls.resize(controls.minWidth, controls.minHeight); }).build();
                components::button(ui, "maximum").size(160, 38).text("Maximum size")
                    .onClick([] { controls.resize(controls.maxWidth, controls.maxHeight); }).build();
            }).build();
            ui.row("modes").gap(12.f).content([&] {
                components::button(ui, "fullscreen").size(180, 38)
                    .text(controls.fullscreen ? "Exit fullscreen" : "Fullscreen (F11)")
                    .onClick([] { controls.toggleFullscreen(); }).build();
                components::button(ui, "border").size(180, 38).text(controls.decorated ? "Borderless" : "Show border")
                    .disabled(controls.fullscreen).onClick([] { controls.toggleBorder(); }).build();
                components::button(ui, "restore").size(160, 38).text("Restore (Esc)").onClick([] { controls.restore(); }).build();
            }).build();
            ui.row("native").gap(12.f).content([&] {
                components::button(ui, "native.move").size(170, 38).text("Compositor move")
                    .onClick([] { controls.beginMove(); }).build();
                components::button(ui, "native.resize.left").size(140, 38).text("Resize left")
                    .onClick([] { controls.beginResize(eui::window::WindowResizeEdge::Left, "left"); }).build();
                components::button(ui, "native.resize.bottomright").size(190, 38).text("Resize bottom right")
                    .onClick([] { controls.beginResize(eui::window::WindowResizeEdge::BottomRight, "bottom right"); }).build();
            }).build();
            label(ui, "hint", "F11: toggle fullscreen. Esc: restore window and border.", 14.f);
            label(ui, "status", controls.window ? controls.status : "This fixture requires a current OpenGL window.", 14.f);
            components::button(ui, "close").size(160, 36).text("Close window").onClick([] { controls.close(); }).build();
        }).build();
}

} // namespace app
