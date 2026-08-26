#include "eui_neo.h"

namespace app {

const DslAppConfig& dslAppConfig() {
    static const DslAppConfig config = DslAppConfig{}
        .title("Control States Demo")
        .windowSize(920, 680)
        .fps(60.0)
        .showDebugStatsInTitle(false);
    return config;
}

void compose(eui::Ui& ui, const eui::Screen& screen) {
    const auto tokens = components::theme::dark();
    const auto page = components::theme::pageVisuals(tokens);
    float* sliderValue = &ui.state<float>("slider.value");
    if (*sliderValue <= 0.0f) {
        *sliderValue = 0.62f;
    }

    ui.column("root")
        .size(screen.width, screen.height)
        .padding(32.0f)
        .gap(18.0f)
        .content([&] {
            ui.text("title")
                .text("控件交互状态")
                .fontSize(page.headerTitleSize)
                .color(page.titleColor)
                .build();
            ui.text("subtitle")
                .text("把鼠标移到控件上，按住鼠标查看 pressed 状态；输入框点击后显示 focused 状态。")
                .fontSize(page.headerSubtitleSize)
                .color(page.subtitleColor)
                .wrap(true)
                .build();

            ui.text("button.label")
                .text("Button · normal / hover / pressed / disabled")
                .fontSize(tokens.metrics.typography.label)
                .color(tokens.text)
                .build();
            ui.row("buttons")
                .size(856.0f, 58.0f)
                .gap(14.0f)
                .content([&] {
                    components::button(ui, "button.normal")
                        .size(190.0f, 48.0f)
                        .text("Normal / Hover")
                        .theme(tokens, true)
                        .build();
                    components::button(ui, "button.secondary")
                        .size(190.0f, 48.0f)
                        .text("Secondary")
                        .theme(tokens, false)
                        .build();
                    components::button(ui, "button.disabled")
                        .size(190.0f, 48.0f)
                        .text("Disabled")
                        .theme(tokens, true)
                        .disabled()
                        .build();
                })
                .build();

            ui.text("selection.label")
                .text("Selection · unchecked / checked / hover")
                .fontSize(tokens.metrics.typography.label)
                .color(tokens.text)
                .build();
            ui.row("selection")
                .size(856.0f, 52.0f)
                .gap(20.0f)
                .content([&] {
                    components::checkbox(ui, "checkbox.off")
                        .text("Unchecked")
                        .checked(false)
                        .theme(tokens)
                        .build();
                    components::checkbox(ui, "checkbox.on")
                        .text("Checked")
                        .checked(true)
                        .theme(tokens)
                        .build();
                    components::toggleSwitch(ui, "switch")
                        .text("Switch")
                        .checked(true)
                        .theme(tokens)
                        .build();
                })
                .build();

            ui.text("input.label")
                .text("Input · normal / hover / focused")
                .fontSize(tokens.metrics.typography.label)
                .color(tokens.text)
                .build();
            components::input(ui, "input")
                .size(520.0f, 48.0f)
                .placeholder("点击这里获得焦点，然后输入文字")
                .theme(tokens)
                .build();

            ui.text("slider.label")
                .text("Slider · hover / focused / disabled")
                .fontSize(tokens.metrics.typography.label)
                .color(tokens.text)
                .build();
            components::slider(ui, "slider")
                .size(520.0f, 38.0f)
                .value(*sliderValue)
                .theme(tokens)
                .onChange([sliderValue](float value) { *sliderValue = value; })
                .build();

            components::slider(ui, "slider.disabled")
                .size(520.0f, 38.0f)
                .value(0.62f)
                .theme(tokens)
                .disabled()
                .build();

            ui.stack("state.note")
                .size(856.0f, 84.0f)
                .content([&] {
                    ui.rect("state.note.background")
                        .size(856.0f, 84.0f)
                        .color(tokens.surface)
                        .radius(tokens.metrics.radius.card)
                        .build();
                    ui.text("state.note.text")
                        .position(18.0f, 14.0f)
                        .size(820.0f, 56.0f)
                        .text("Hover 是鼠标位置；focus 是键盘/程序操作目标。\n"
                              "本页面用于观察现有控件反馈：Slider 是一个主要依赖鼠标拖动的控件，后续可接入统一状态。")
                        .fontSize(tokens.metrics.typography.body)
                        .lineHeight(24.0f)
                        .color(page.bodyColor)
                        .wrap(true)
                        .build();
                })
                .build();
        })
        .build();
}

} // namespace app
