# 快速开始

## 框架功能概览

- 声明式组件 DSL（`ui.xxx().build()`）
- 事件驱动渲染（仅在需要时重绘）
- 布局容器（`row/column/flex`）
- 大列表虚拟渲染（`ListView::Compose`）
- 图片资源加载（本地 / HTTP / Bing）
- 主题与字体体系（Light/Dark + SDF 文本）

## 项目结构

- `app/`：业务页面入口与逻辑
- `src/`：框架源码（渲染、组件、布局、运行时）
- `docs/`：文档与示意图

## 编译

```bash
cmake -B build -G Ninja
cmake --build build --config Release
```

## 最小 DSL 入口

```cpp
#include "app/DslAppRuntime.h"

int main() {
    EUINEO::DslAppConfig config;
    config.title = "EUI Demo";
    config.width = 900;
    config.height = 600;
    config.pageId = "demo";
    config.fps = 120;

    return EUINEO::RunDslApp(config, [](EUINEO::UIContext& ui, const EUINEO::RectFrame& screen) {
        EUINEO::UseDslDarkTheme(EUINEO::Color(0.0f, 0.0f, 0.0f, 1.0f));

        ui.panel("bg")
            .position(0.0f, 0.0f)
            .size(screen.width, screen.height)
            .background(EUINEO::Color(0.10f, 0.10f, 0.12f, 1.0f))
            .build();

        ui.button("hello")
            .position(24.0f, 24.0f)
            .size(140.0f, 40.0f)
            .text("Hello EUI-NEO")
            .build();
    });
}
```

## 参考入口

- `main.cpp`
- `app/basic_demo.cpp`
- `app/listview_demo.cpp`
- `src/components/ListView.h`
