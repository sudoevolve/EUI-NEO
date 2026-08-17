#include "eui_neo.h"

#include <string>

namespace app {
namespace {

std::string& nameValue() {
    static std::string value;
    return value;
}

std::string& emailValue() {
    static std::string value;
    return value;
}

void labeledFocusableRect(eui::Ui& ui,
                          const std::string& id,
                          const std::string& label,
                          float x,
                          float y,
                          float width,
                          float height) {
    const bool focused = ui.isFocused(id);
    ui.rect(id)
        .position(x, y)
        .size(width, height)
        .color(focused ? eui::Color{0.24f, 0.42f, 0.62f, 1.0f}
                       : eui::Color{0.18f, 0.22f, 0.30f, 1.0f})
        .border(focused ? 2.0f : 1.0f,
                focused ? eui::Color{0.34f, 0.72f, 1.0f, 1.0f}
                        : eui::Color{0.32f, 0.38f, 0.48f, 1.0f})
        .focusable()
        .onClick([] {})
        .build();
    ui.text(id + ".label")
        .position(x, y)
        .size(width, height)
        .text(label)
        .fontSize(14.0f)
        .lineHeight(18.0f)
        .color({0.90f, 0.94f, 1.0f, 1.0f})
        .verticalAlign(eui::VerticalAlign::Center)
        .build();
}

} // namespace

const DslAppConfig& dslAppConfig() {
    static const DslAppConfig config = DslAppConfig{}
        .title("Focus Traversal Viewer")
        .pageId("focus_traversal_viewer")
        .clearColor({0.14f, 0.16f, 0.19f, 1.0f})
        .windowSize(720, 560)
        .showDebugStatsInTitle(false)
        .fps(0.0)
        .iconPath("");
    return config;
}

void compose(eui::Ui& ui, const eui::Screen& screen) {
    const float x = 60.0f;
    const float width = screen.width - 120.0f;
    const float inputHeight = 42.0f;
    const float gap = 16.0f;

    ui.text("hint")
        .position(x, 24.0f)
        .size(width, 44.0f)
        .text("Tab / Shift+Tab 遍历焦点，循环；禁用元素被跳过；components::button 默认不可 Tab 聚焦，"
              "下面的按钮是显式 .focusable() 的裸 rect")
        .fontSize(14.0f)
        .lineHeight(20.0f)
        .color({0.78f, 0.83f, 0.92f, 1.0f})
        .wrap()
        .build();

    ui.stack("field.name.wrap")
        .position(x, 92.0f)
        .size(width, inputHeight)
        .content([&] {
            components::input(ui, "field.name")
                .size(width, inputHeight)
                .value(nameValue())
                .placeholder("Name（焦点边框由 input 组件自绘）")
                .onChange([](const std::string& value) { nameValue() = value; })
                .build();
        })
        .build();

    ui.stack("field.email.wrap")
        .position(x, 92.0f + (inputHeight + gap))
        .size(width, inputHeight)
        .content([&] {
            components::input(ui, "field.email")
                .size(width, inputHeight)
                .value(emailValue())
                .placeholder("Email")
                .onChange([](const std::string& value) { emailValue() = value; })
                .build();
        })
        .build();

    // Disabled subtree: the wrapped input renders but is skipped by Tab.
    ui.stack("field.disabled.wrap")
        .position(x, 92.0f + 2.0f * (inputHeight + gap))
        .size(width, inputHeight)
        .disabled(true)
        .content([&] {
            components::input(ui, "field.disabled")
                .size(width, inputHeight)
                .value(std::string("Disabled field（Tab 跳过）"))
                .build();
        })
        .build();

    const float rowY = 92.0f + 3.0f * (inputHeight + gap);
    labeledFocusableRect(ui, "field.button", "Focusable rect（显式 .focusable()）", x, rowY, 260.0f, 40.0f);
    labeledFocusableRect(ui, "field.rect", "另一个 focusable", x + 280.0f, rowY, 220.0f, 40.0f);
}

} // namespace app
