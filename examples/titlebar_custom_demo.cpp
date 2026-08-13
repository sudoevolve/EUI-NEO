// 无边框窗口 + 自定义标题栏 demo(自定义内容版)。
// 与 titlebar_demo 的区别:用 titlebar 的 content() 回调完全自定义标题栏内容,
// 展示"想填什么自己填"的用法。背景拖动 / 四边四角 resize 仍由组件自动提供。
#include "eui_neo.h"

#include <iostream>

namespace app {

const DslAppConfig& dslAppConfig() {
    static const DslAppConfig config = DslAppConfig{}
        .title("Frameless Custom TitleBar Demo")
        .windowSize(800, 560)
        .fps(60.0)
        .frameless(true);
    return config;
}

void compose(eui::Ui& ui, const eui::Screen& screen) {
    const float titleHeight = 48.0f;

    ui.stack("root")
        .size(screen.width, screen.height)
        .content([&] {
            ui.column("content")
                .y(titleHeight)
                .size(screen.width, screen.height - titleHeight)
                .padding(32.0f)
                .gap(16.0f)
                .content([&] {
                    ui.text("hint")
                        .text("这是自定义标题栏版:标题栏内容完全用 content() 回调自由搭建。\n"
                              "左侧是应用图标 + 标题,右侧是自定义设置按钮 + 最小化 + 关闭。")
                        .fontSize(16.0f)
                        .wrap(true)
                        .build();

                    components::button(ui, "my_button")
                        .text("点我")
                        .onClick([] {
                            std::cout << "按钮被点击了！" << std::endl;
                        })
                        .build();
                })
                .build();

            // 自定义标题栏:content() 回调完全接管内容区
            components::titlebar(ui, "titlebar")
                .size(screen.width, screen.height)
                .titleHeight(titleHeight)
                .content([&] {
                    const float pad = 14.0f;
                    const float btn = 34.0f;
                    // 左侧:应用图标 + 标题
                    ui.text("tb.icon")
                        .x(pad)
                        .size(20.0f, titleHeight)
                        .icon(0xF015)
                        .fontSize(16.0f)
                        .color(core::Color{0.90f, 0.94f, 1.0f, 1.0f})
                        .horizontalAlign(core::HorizontalAlign::Left)
                        .verticalAlign(core::VerticalAlign::Center)
                        .build();
                    ui.text("tb.title")
                        .x(pad + 24.0f)
                        .size(screen.width - 260.0f, titleHeight)
                        .text("我的自定义应用")
                        .fontSize(16.0f)
                        .color(core::Color{0.92f, 0.94f, 0.97f, 1.0f})
                        .horizontalAlign(core::HorizontalAlign::Left)
                        .verticalAlign(core::VerticalAlign::Center)
                        .build();
                    // 右侧:自定义按钮 + 窗口控制按钮
                    ui.row("tb.buttons")
                        .x(screen.width - pad - btn * 3.0f - 12.0f)
                        .y((titleHeight - btn) * 0.5f)
                        .size(btn * 3.0f + 12.0f, btn)
                        .gap(6.0f)
                        .content([&] {
                            components::button(ui, "tb.settings")
                                .size(btn, btn).text("").icon(0xF013)
                                .onClick([] { std::cout << "设置按钮" << std::endl; })
                                .build();
                            components::button(ui, "tb.min")
                                .size(btn, btn).text("").icon(0xF2D1)
                                .onClick([] { app::minimizeWindow(); })
                                .build();
                            components::button(ui, "tb.close")
                                .size(btn, btn).text("").icon(0xF00D)
                                .onClick([] { app::requestWindowClose(); })
                                .build();
                        })
                        .build();
                })
                .build();
        })
        .build();
}

} // namespace app
