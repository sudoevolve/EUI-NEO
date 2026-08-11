// 1. 包含主头文件，一切从这里开始
#include "eui_neo.h"

#include <iostream>

// 2. 所有的代码都放在 app 命名空间下
namespace app {

// 3. 应用配置：标题、窗口大小等
const DslAppConfig& dslAppConfig() {
    static const DslAppConfig config = DslAppConfig{}
        .title("我的第一个应用")
        .windowSize(800, 600)
        .fps(90.0); // 帧率上限
    return config;
}

// 4. 核心界面构建函数
void compose(eui::Ui& ui, const eui::Screen& screen) {
    // 创建根列，填满整个屏幕
    ui.column("root")
        .size(screen.width, screen.height)
        .padding(32.0f) // 内边距
        .gap(0.0f) // 子元素间距
        .content([&] { // 子元素开始
            // 一个标题文本
            ui.text("title")
                .text("你好，EUI-NEO！")
                .fontSize(32.0f)
                .build();

            // 一个按钮，点击时在控制台输出
            components::button(ui, "my_button")
                .text("点我")
                .onClick([] {
                    std::cout << "按钮被点击了！" << std::endl;
                })
                .build();
        })
        .build(); // 根列构建完成
}

} // namespace app
