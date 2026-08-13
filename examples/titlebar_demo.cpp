// 无边框窗口 + 自定义标题栏 demo。
// 行为:
//   - frameless(true) 移除系统标题栏
//   - 拖动顶部标题栏移动窗口
//   - 标题栏右侧按钮控制最小化 / 最大化 / 关闭
//   - 拖动窗口四边 / 四角缩放窗口
#include "eui_neo.h"

#include <iostream>

namespace app {

const DslAppConfig& dslAppConfig() {
    static const DslAppConfig config = DslAppConfig{}
        .title("Frameless TitleBar Demo")
        .windowSize(800, 560)
        .fps(60.0)
        .frameless(true); // 无边框窗口,标题栏由 titlebar 组件自绘
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
                        .text("这是一个无边框窗口,顶部是自定义标题栏。\n"
                              "拖动标题栏移动窗口,拖动四边/四角缩放窗口,右侧按钮控制最小化/最大化/关闭。")
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

            // 覆盖整个窗口:标题栏 + 四边四角 resize 手柄。
            // 默认布局(不传 content):左上角标题 + 右上角三按钮(最小化/全屏/关闭),
            // 全屏后图标自动切换为"还原"。
            components::titlebar(ui, "titlebar")
                .size(screen.width, screen.height)
                .titleHeight(titleHeight)
                .title("EUI-NEO 无边框标题栏 Demo")
                .build();
        })
        .build();
}

} // namespace app
