#include "eui_neo.h"

#if defined(EUI_WINDOW_BACKEND_SDL2)
#include <SDL.h>
#else
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#endif

namespace app {
namespace {

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
    // 通过当前 OpenGL context 获取应用窗口，不替换框架事件回调。
    window = glfwGetCurrentContext();
#endif
}

} // namespace

const DslAppConfig& dslAppConfig() {
#if defined(EUI_WINDOW_BACKEND_SDL2)
    constexpr const char* title = "EUI Transparent Window Test - SDL2";
#else
    constexpr const char* title = "EUI Transparent Window Test - GLFW";
#endif
    static const auto config = DslAppConfig{}.title(title).windowSize(760, 480)
        .decorated(false).transparent(true).clearColor({0.0f, 0.0f, 0.0f, 0.0f});
    return config;
}

void compose(eui::Ui& ui, const eui::Screen& screen) {
    capture();
    const bool granted = eui::window::framebufferTransparent(window);
    const float margin = 36.f;
    const float inset = 40.f;
    ui.stack("root").size(screen.width, screen.height).content([&] {
        ui.rect("panel")
            .position(margin, margin)
            .size(screen.width - margin * 2.f, screen.height - margin * 2.f)
            .radius(28.f)
            .color("#0F172A")
            .build();
        ui.column("content")
            .position(margin + inset, margin + 36.f)
            .size(screen.width - (margin + inset) * 2.f, screen.height - (margin + 36.f) * 2.f)
            .gap(12.f)
            .content([&] {
                ui.text("headline").size(520.f, 34.f).fontSize(24.f).color("#E2E8F0")
                    .text("Transparent window").build();
                ui.text("status").size(620.f, 24.f).fontSize(15.f)
                    .color(granted ? "#4ADE80" : "#F87171")
                    .text(granted
                              ? "framebufferTransparent: true - desktop shows through corners and margins"
                              : "framebufferTransparent: false - platform did not grant an alpha surface")
                    .build();
                ui.text("hint").size(620.f, 24.f).fontSize(14.f).color("#94A3B8")
                    .text("Rounded corners and the outer margin are fully transparent.").build();
            }).build();
    }).build();
}

} // namespace app
