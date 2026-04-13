# EUI-NEO

<p align="center">
  <img src="docs/icon.png" width="120" height="120" />
</p>

EUI-NEO 是一个基于 OpenGL + GLFW 的声明式 2D GUI 框架，支持组件化 UI、即时模式组合与事件驱动渲染。

<p align="center">
  <img src="./docs/1.jpg" alt="EUI-NEO Preview 1" width="49%" />
  <img src="./docs/2.jpg" alt="EUI-NEO Preview 2" width="49%" />
  <img src="./docs/3.jpg" alt="EUI-NEO Preview 3" width="49%" />
  <img src="./docs/4.jpg" alt="EUI-NEO Preview 4" width="49%" />
</p>

## 编译

```bash
cmake -B build -G Ninja
cmake --build build --config Release
```

## 基础示例

### 1) 最小 DSL 入口

```cpp
#include "app/DslAppRuntime.h"

int main() {
    EUINEO::DslAppConfig config;
    config.title = "EUI Demo";
    config.width = 900;
    config.height = 600;
    config.pageId = "demo";
    config.fps = 120;
    config.darkTitleBar = true;

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
## 详细文档

更多细节参考 `docs/guide`：

- [docs/guide/index.md](./docs/guide/index.md)
- [docs/guide/getting-started.md](./docs/guide/getting-started.md)
- [docs/guide/primitives.md](./docs/guide/primitives.md)
- [docs/guide/listview.md](./docs/guide/listview.md)
- [docs/guide/layout-and-anchor.md](./docs/guide/layout-and-anchor.md)
- [docs/guide/components-and-custom-node.md](./docs/guide/components-and-custom-node.md)
- [docs/guide/runtime-network-fonts.md](./docs/guide/runtime-network-fonts.md)

## ⭐ Project Stats

<p align="center">
  <img src="https://img.shields.io/github/stars/sudoevolve/EUI?style=for-the-badge" />
  <img src="https://img.shields.io/github/forks/sudoevolve/EUI?style=for-the-badge" />
  <img src="https://img.shields.io/github/issues/sudoevolve/EUI?style=for-the-badge" />
</p>

---

## 📈 Star History

[![Star History Chart](https://api.star-history.com/svg?repos=sudoevolve/EUI&type=date&legend=top-left)](https://www.star-history.com/?repos=sudoevolve%2FEUI&type=date&legend=top-left)
