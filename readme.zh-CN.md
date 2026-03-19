# EUI（中文说明）

[English README](readme.md)

EUI 是一个轻量、头文件式（header-only）的 C++ UI 工具包，偏重实际可用的即时模式（immediate-mode）工作流。
核心 API 在 `include/EUI.h` 中，只负责产出绘制命令。
当前内置了可选 OpenGL 应用运行时，可挂接 GLFW 和 SDL2 窗口后端。
当前版本已经补齐了更完整的文本链路，覆盖混合文本/图标渲染、可编辑输入框和可滚动文本编辑框。
目前主要控件也已经内置了轻量动效，覆盖悬停、按下、聚焦、下拉展开和进度变化等状态反馈。
当前默认动效也专门做过收敛，尽量兼容事件驱动渲染和更低的 GPU 占用。

## 预览

<table>
  <tr>
    <td width="50%"><img src="preview/0.jpg" alt="Preview 0" width="100%" /></td>
    <td width="50%"><img src="preview/1.jpg" alt="Preview 1" width="100%" /></td>
  </tr>
  <tr>
    <td width="50%"><img src="preview/2.jpg" alt="Preview 2" width="100%" /></td>
    <td width="50%"><img src="preview/3.jpg" alt="Preview 3" width="100%" /></td>
  </tr>
</table>

## 项目现状分析（基于当前代码）

### 1）整体架构

- **核心层（`eui::Context`）**
  - 即时模式 UI API，输出 `DrawCommand` + 文本缓冲区（text arena）。
  - 核心不强依赖 GLFW/OpenGL，可独立接入你自己的渲染后端。
- **可选应用运行时层（`eui::app`）**
  - 负责窗口循环、输入采集、DPI 计算、剪贴板桥接、帧调度。
  - 通过 `FrameContext` 回调驱动你写的 UI。
- **渲染层（同在 `EUI.h`）**
  - OpenGL 指令渲染，支持裁剪与批处理。
- 可选使用 `stb_truetype` 渲染文本/图标并带字形纹理缓存（检测到 `stb_truetype.h` 时自动启用）。
- 文本测量会跟随当前实际渲染后端配置，尽量保证光标、选区、换行和命中测试与最终渲染结果一致。
- 采用“文本字体 + 图标字体”双字体回退（私有区码点优先图标字体）。
- 图标字体默认使用仓库内置 `include/Font Awesome 7 Free-Solid-900.otf`，文本继续使用系统默认字体。
- 仅在字体加载或渲染失败时回退到内置位图文字路径。
- 也可在编译时通过 `-DEUI_ENABLE_STB_TRUETYPE=0` 强制关闭 STB 路径。

### 2）当前渲染链路

1. `ui.begin_frame(...)`
2. 构建控件与布局
3. `ui.take_frame(...)` 取出命令和文本缓冲
4. 对整帧命令做 hash
5. 与上一帧相同则跳过重绘
6. 不同则与上一帧计算脏矩形
7. 仅重绘脏区（scissor），其余区域复用缓存纹理

### 3）已落地的性能机制

- 默认事件驱动渲染（`continuous_render = false`）。
- 帧级 hash 快速早退，避免无意义 GPU 重绘。
- 前后帧命令差异计算脏区。
- 缓存帧缓冲纹理 + 脏区局部刷新。
- 脏区过大时会直接回退为整帧重绘，避免缓存回放和过多局部重绘叠加。
- Core 层有 clip stack 与命令级裁剪。
- 命令量较大时启用基于 tile 的命令分桶。
- 动画状态只有在尚未收敛时才会继续请求重绘，尽量不破坏事件驱动渲染收益。
- 很弱的辉光会被提前裁掉，大面积表面保持静态，微小动效变化也会更快收敛。

### 4）文本与编辑模型

- 单行输入框支持光标移动、拖拽选区、剪贴板快捷键，以及随光标自动水平滚动。
- `text_area` 支持多行编辑、自动换行、拖拽选区、滚动，以及带 `preferred x` 的 `Up` / `Down` 光标导航。
- 混合文本 + 图标时会做 icon-aware 测量，选区和光标位置会尽量贴近实际渲染结果。
- 应用运行时已接管编辑类按键的重复输入，包括 `Backspace`、`Delete`、`Enter`、方向键、`Home`、`End`。

## 已实现功能

### 主题

- `ThemeMode`（`Light` / `Dark`）
- 主色（`set_primary_color`）
- 圆角（`set_corner_radius`）
- 暗色模式下会自动提亮过暗主色，提升可读性和对比度

### 布局

- `ui.panel(...).begin(...)`
- `ui.card(...).begin(...)`
- `dock_top` / `dock_bottom` / `dock_left` / `dock_right`
- `split_h_ratio` / `split_v_ratio`
- `row().items(...)`
- `anchor()`
- `scope()` / `stack()` / `clip()`
- `spacer`

### 控件

- `label`
- `button`（`Primary`、`Secondary`、`Ghost`，额外支持 `text_scale`）
- `tab`
- `slider_float`（拖动 + 右键数值编辑）
- `input_float`（光标、选区、`Ctrl+A/C/V/X`）
- `input_text`（单行可编辑文本输入）
- `input_readonly`
  - 支持 `align_right`、`value_font_scale`、`muted`
- `progress`
- `begin_dropdown` / `end_dropdown`
- `begin_scroll_area` / `end_scroll_area`
  - 支持拖拽、滚轮、惯性、回弹、滚动条参数
- `text_area`（可编辑、可选择、光标、滚动）
- `text_area_readonly`
- 内置控件动效
  - 按钮 / Tab 的轻微按压反馈
  - 输入框聚焦时的柔和描边与克制辉光
  - 下拉框展开与箭头旋转
  - Slider、滚动条滑块、Progress 填充的缓动反馈

### 输出与集成

- `end_frame()` 返回 `std::vector<DrawCommand>`
- `take_frame(...)` 可高效转移本帧命令缓冲
- `text_arena()` 返回文本绘制命令使用的文本存储

## 仓库结构

```text
EUI/
|- .github/
|  `- workflows/
|     `- release.yml
|- cmake/
|  `- EUIConfig.cmake.in
|- docs/
|  |- backend-abstraction-checklist.md
|  |- backend-abstraction-issue.md
|  |- concise-ui-syntax-proposal.zh-CN.md
|  |- framework-revamp-proposal.zh-CN.md
|  |- include-cleanup-record.zh-CN.md
|  |- project-structure.zh-CN.md
|  `- quick-decoupling-status.zh-CN.md
|- include/
|  `- EUI.h
|  |- stb_truetype.h
|  `- Font Awesome 7 Free-Solid-900.otf
|- examples/
|  |- anchor_layout_demo.cpp
|  |- basic_widgets_demo.cpp
|  |- calculator_demo.cpp
|  |- graphics_showcase_demo.cpp
|  `- sidebar_navigation_demo.cpp
|- CMakeLists.txt
|- readme.md
`- readme.zh-CN.md
```

## 构建

推荐使用 `Ninja` 生成器。

### 1）仅构建核心（不依赖具体窗口后端）

```bash
cmake -S . -B build -G Ninja -DEUI_BUILD_EXAMPLES=OFF
cmake --build build
```

会生成：

- `EUI::eui`（interface）

### 2）构建示例（OpenGL + GLFW / SDL2）

```bash
cmake -S . -B build -G Ninja -DEUI_BUILD_EXAMPLES=ON
cmake --build build
```

当 OpenGL 与至少一个启用的窗口后端都可用时，会生成：

- `basic_widgets_demo`（`examples/basic_widgets_demo.cpp`）
- `anchor_layout_demo`（`examples/anchor_layout_demo.cpp`）
- `sidebar_navigation_demo`（`examples/sidebar_navigation_demo.cpp`）
- `calculator_demo`（`examples/calculator_demo.cpp`）
- `graphics_showcase_demo`（`examples/graphics_showcase_demo.cpp`）

可执行文件名直接跟随对应的 `examples/*.cpp` 文件名生成。
旧名字如 `eui_demo`、`eui_minimal_demo` 只作为 CMake 兼容 target 保留。

推荐直接使用高层后端模式：

- `-DEUI_WINDOW_BACKEND=AUTO`
  - 默认推荐模式。
  - 优先使用本机已安装的 SDL2。
  - 如果本机没有 SDL2，则回退到 GLFW。
  - 默认不会自动联网下载依赖。
  - 如果本机两者都没有，只有显式打开 fetch 选项时才会继续尝试下载。
- `-DEUI_WINDOW_BACKEND=GLFW`
  - 只构建 GLFW 窗口后端。
- `-DEUI_WINDOW_BACKEND=SDL2`
  - 只构建 SDL2 窗口后端。
- `-DEUI_WINDOW_BACKEND=ALL`
  - 两个后端都尝试接入，运行时再通过 `AppOptions::window_backend` 选择。

重要 CMake 选项：

```bash
-DEUI_BUILD_EXAMPLES=ON|OFF
-DEUI_STRICT_WARNINGS=ON|OFF
-DEUI_WINDOW_BACKEND=AUTO|GLFW|SDL2|ALL
-DEUI_FETCH_GLFW_FROM_GIT=ON|OFF
-DEUI_FETCH_SDL2_FROM_GIT=ON|OFF
-DEUI_ENABLE_GLFW_WINDOW_BACKEND=ON|OFF
-DEUI_ENABLE_SDL2_WINDOW_BACKEND=ON|OFF
-DEUI_GLFW_GIT_TAG=3.4
-DEUI_SDL2_GIT_TAG=SDL2
-DEUI_ENABLE_STB_TRUETYPE=1|0
```

其中：

- `EUI_WINDOW_BACKEND` 是推荐入口。
- `EUI_ENABLE_GLFW_WINDOW_BACKEND` / `EUI_ENABLE_SDL2_WINDOW_BACKEND` 仍保留兼容，但更偏底层覆盖。
- `EUI_FETCH_GLFW_FROM_GIT` / `EUI_FETCH_SDL2_FROM_GIT` 默认都是 `OFF`。

若网络或 Git 不可用：

```bash
cmake -S . -B build -G Ninja -DEUI_BUILD_EXAMPLES=ON -DEUI_FETCH_GLFW_FROM_GIT=OFF
```

常见用法：

```bash
# 默认：本机有 SDL2 就优先 SDL2，否则走 GLFW
cmake -S . -B build -G Ninja -DEUI_WINDOW_BACKEND=AUTO

# 强制 GLFW，不管本机有没有 SDL2
cmake -S . -B build -G Ninja -DEUI_WINDOW_BACKEND=GLFW

# 强制 SDL2，本机没有就自动拉取
cmake -S . -B build -G Ninja -DEUI_WINDOW_BACKEND=SDL2 -DEUI_FETCH_SDL2_FROM_GIT=ON

# 两个后端都编进去，运行时自己选
cmake -S . -B build -G Ninja -DEUI_WINDOW_BACKEND=ALL
```

如果你用 VSCode 的 CMake Tools 侧边栏，仓库现在也提供了这些 preset：

- `auto`
- `glfw`
- `sdl2`
- `all`
- `auto-fetch`
- `glfw-fetch`
- `sdl2-fetch`
- `all-fetch`

前四个只用本机已有依赖，不会默认联网。
后四个才会在缺依赖时尝试从 Git 下载。

## 自动发版与打包

仓库已经新增 GitHub Actions 工作流：[`.github/workflows/release.yml`](.github/workflows/release.yml)。
只要推送以 `v` 开头的 tag，就会自动在 Windows 和 Linux 上执行 CMake 配置、构建，并通过 `cpack` 产出发布包。
同一个工作流也支持在 GitHub Actions 页面手动运行，并填写 `release_tag`，适合给已有 release 重新构建并补传资产。

常规发布方式：

```bash
git tag v0.1.0
git push origin v0.1.0
```

自动产物：

- Windows：`.zip`
- Linux：`.tar.gz`

打包内容包括：

- `include/`
- CMake 包配置文件（`EUIConfig.cmake`、`EUIConfigVersion.cmake`、导出 targets）
- `readme.md` 与 `readme.zh-CN.md`
- `docs/`
- `examples/`
- 若打包时启用了示例目标，也会带上对应示例可执行文件

## 运行示例

```bash
# 基础控件
cmake --build build --target basic_widgets_demo

# 计算器示例
cmake --build build --target calculator_demo

# 锚点布局示例
cmake --build build --target anchor_layout_demo

# 侧边栏导航示例
cmake --build build --target sidebar_navigation_demo

# 图形能力示例
cmake --build build --target graphics_showcase_demo
```

## Quick 最小用法

```cpp
#include "EUI.h"

eui::Context ctx;
eui::InputState input{};
std::string value_text = "0.50";

ctx.begin_frame(1280.0f, 720.0f, input);
ctx.set_theme_mode(eui::ThemeMode::Dark);

eui::quick::UI ui(ctx);
ui.panel("Demo")
    .in(eui::Rect{20.0f, 20.0f, 640.0f, 220.0f})
    .padding(16.0f)
    .begin([&](auto& panel) {
        const auto row = ui.split_h_ratio(panel.content(), 0.50f, 12.0f);
        ui.button("Run").in(row.first).primary().draw();
        ui.input("Value", value_text).in(row.second).draw();
        ui.progress("Loading", 0.42f).height(8.0f).draw();
    });

const auto& commands = ctx.end_frame();
const auto& text_arena = ctx.text_arena();
```

## 常用布局模板

### 宽度规则（重点）

- 控件宽度由布局决定，不是控件参数直接指定。
- `row().items({...})` 用来声明当前一行的宽度模型。
- `ui.fixed(px)` 固定槽位宽度。
- `ui.flex(weight)` 按权重分摊剩余宽度。
- `ui.content(min, max)` 让槽位随内容伸缩，但仍受边界约束。
- `scope(rect, ...)` 可把布局临时切到某个子区域，不必手动维护全局坐标。
- `dock_*` 适合按顺序吃掉边缘区域，`split_*` 适合显式切出分支区域。

### 侧边栏图标与文本垂直对齐方案

- 侧边栏按钮如果需要左对齐，请在 `label` 前加 `\t`，会启用左对齐并带左侧内边距。
- 图标 + 文本请使用 **两个 ASCII 空格** 分隔（例如 `u8"\uF015  Dashboard"`）。
- EUI 会把图标和文本拆开分别渲染，垂直居中会稳定很多。
- 想看完整的极简侧边栏 + 页面切换示例，可直接参考 `examples/sidebar_navigation_demo.cpp`。

```cpp
// 左对齐侧边栏项：图标 + 文本，垂直居中更稳定
ui.button("\t" u8"\uF015  Dashboard", eui::ButtonStyle::Secondary, 34.0f);
```

### 1）侧边栏 + 主内容

```cpp
ui.panel("Workspace")
    .in(frame_rect)
    .padding(16.0f)
    .begin([&](auto& page) {
        const Rect sidebar = page.dock_left(220.0f, 16.0f);
        const Rect main = page.content();

        ui.card("Navigation")
            .in(sidebar)
            .begin([&](auto&) {
                ui.button("Dashboard").ghost().height(34.0f).draw();
                ui.button("Projects").ghost().height(34.0f).draw();
                ui.button("Settings").ghost().height(34.0f).draw();
            });

        ui.card("Overview")
            .in(main)
            .begin([&](auto&) {
                ui.label("Main content area").draw();
            });
    });
```

### 2）顶栏左右分布

```cpp
ui.card("Top bar")
    .in(rect)
    .begin([&](auto&) {
        ui.row()
            .items({
                ui.fixed(96.0f),
                ui.fixed(96.0f),
                ui.flex(1.0f),
                ui.fixed(120.0f),
                ui.fixed(120.0f),
            })
            .gap(8.0f)
            .align_center()
            .begin([&] {
                ui.button("Back").ghost().height(34.0f).draw();
                ui.button("Forward").ghost().height(34.0f).draw();
                ui.label("Search / breadcrumbs / page title").muted().draw();
                ui.button("Search").ghost().height(34.0f).draw();
                ui.button("Profile").primary().height(34.0f).draw();
            });
    });
```

### 3）三段式工具栏（左/中/右）

```cpp
ui.card("Toolbar")
    .in(rect)
    .begin([&](auto&) {
        ui.row()
            .items({
                ui.content(150.0f, 220.0f),
                ui.flex(1.0f),
                ui.content(160.0f, 240.0f),
            })
            .gap(12.0f)
            .align_center()
            .begin([&] {
                ui.readonly("Left", "New / Save").height(34.0f).draw();
                ui.label("Build #128").font(13.0f).muted().draw();
                ui.readonly("Right", "Run / Deploy").height(34.0f).draw();
            });
    });
```

### 4）两列设置页

```cpp
std::string gamma = "2.2";
std::string exposure = "124";

const auto columns = ui.split_h_ratio(rect, 0.50f, 10.0f);

ui.card("General")
    .in(columns.first)
    .begin([&](auto&) {
        ui.input("Gamma", gamma).height(36.0f).draw();
    });

ui.card("Display")
    .in(columns.second)
    .begin([&](auto&) {
        ui.input("Exposure", exposure).height(36.0f).draw();
    });
```

### 5）把徽标锚到上一个卡片上

```cpp
ui.metric("Build", "Ready")
    .in(rect)
    .tag("LIVE")
    .draw();

const Rect badge = ui.anchor()
    .to_last()
    .top(12.0f)
    .right(12.0f)
    .size(72.0f, 24.0f)
    .resolve();

ui.shape()
    .in(badge)
    .radius(12.0f)
    .fill(0x16A34A)
    .draw();

ui.text("SYNC")
    .in(badge)
    .font(11.0f)
    .center()
    .draw();
```

## 可选应用运行时用法

```cpp
#define EUI_BACKEND_OPENGL
#define EUI_PLATFORM_GLFW
#include "EUI.h"

int main() {
    eui::app::AppOptions options{};
    options.width = 960;
    options.height = 710;
    options.title = "EUI App";
    options.vsync = true;
    options.continuous_render = false;
    options.max_fps = 240.0;

    options.text_font_family = "Segoe UI";
    options.text_font_weight = 600; // 100-900，值越大越粗
    options.icon_font_family = "Font Awesome 7 Free Solid";
    options.icon_font_file = "include/Font Awesome 7 Free-Solid-900.otf";
    options.text_backend = eui::app::AppOptions::TextBackend::Auto;
    // 非 Windows 建议显式指定字体文件路径，图标显示更稳定。
    // options.text_font_file = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
    // options.icon_font_file = "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf";
    options.enable_icon_font_fallback = true;

    return eui::app::run(
        [&](eui::app::FrameContext frame) {
            auto& ctx = frame.context();
            ctx.set_theme_mode(eui::ThemeMode::Dark);

            eui::quick::UI ui(ctx);
            ui.panel("Demo")
                .in(eui::Rect{20.0f, 20.0f, 320.0f, 180.0f})
                .padding(16.0f)
                .begin([&](auto&) {
                    ui.label("Hello EUI").draw();
                    ui.spacer(10.0f);
                    ui.button("Run").primary().height(36.0f).draw();
                });

            // 事件驱动模式下，如需动画请主动请求下一帧。
            frame.request_next_frame();
        },
        options
    );
}
```

### 文本后端说明

- `AppOptions::TextBackend::Auto`
  - Windows 下若启用了 `stb_truetype`，默认优先走 STB TrueType，再回退到 Win32，最后才回退到内置位图字形。
  - 非 Windows 下若可用，则优先走 STB 文本路径。
- `AppOptions::TextBackend::Stb`
  - 使用 `stb_truetype` 做字形栅格化和纹理图集缓存。
- `AppOptions::TextBackend::Win32`
  - 仅限 Windows，使用基于 GDI/WGL 的文本渲染路径。

如果要提升图标覆盖率，建议保留 `enable_icon_font_fallback = true`，并显式携带图标字体文件。

## 说明

- `index.html` 是视觉原型参考，不参与 C++ 构建产物。
- 建议源码统一使用 UTF-8，避免 Windows 工具链下 C4819 和字符串乱码问题。
