# EUI 项目目录与文件职责

最后更新：2026-03-19

这份文档只描述当前仓库真实存在的目录、文件和职责，不沿用旧命名。

## 顶层目录

```text
EUI/
  .github/
    workflows/
      release.yml
  build/
  cmake/
    EUIExamples.cmake
    EUIConfig.cmake.in
  docs/
    backend-abstraction-checklist.md
    backend-abstraction-issue.md
    concise-ui-syntax-proposal.zh-CN.md
    framework-revamp-proposal.zh-CN.md
    include-cleanup-record.zh-CN.md
    project-structure.zh-CN.md
    quick-decoupling-status.zh-CN.md
  examples/
    anchor_layout_demo.cpp
    basic_widgets_demo.cpp
    calculator_demo.cpp
    graphics_showcase_demo.cpp
    sidebar_navigation_demo.cpp
  include/
    EUI.h
    Font Awesome 7 Free-Solid-900.otf
    stb_truetype.h
    eui/
      animation.h
      app.h
      core.h
      graphics.h
      quick.h
      renderer.h
      runtime.h
      animation/
        animator.h
        easing.h
        timeline.h
      app/
        frame_context.h
        options.h
        detail/
          dirty_cache_detail.inl
          app_entry_detail.inl
          font_renderers_detail.inl
          glfw_run_loop_detail.inl
          glfw_runtime_detail.inl
          opengl_renderer_detail.inl
          runtime_state_detail.inl
          sdl2_run_loop_detail.inl
          sdl2_runtime_detail.inl
          text_support_detail.inl
      core/
        context_fwd.h
        context_state.h
        context_text_measure.h
        context_utils.h
        foundation.h
      graphics/
        effects.h
        primitives.h
        transforms.h
      quick/
        ui.h
        detail/
          anchor_spec.h
          primitive_painter.h
      renderer/
        contracts.h
        draw_data_adapter.h
      runtime/
        contracts.h
        frame_context.h
  preview/
    0.jpg
    1.jpg
    2.jpg
    3.jpg
  .gitignore
  CMakeLists.txt
  LICENSE
  readme.md
  readme.zh-CN.md
```

`build/` 属于生成目录，不纳入源码职责说明。

## 根目录文件

- `CMakeLists.txt`
  - 项目总构建入口。
  - 定义 `EUI::eui` 这个 header-only interface target。
  - 负责安装、打包和示例构建入口。

- `readme.md`
  - 英文总说明。

- `readme.zh-CN.md`
  - 中文总说明。

- `LICENSE`
  - 许可证文件。

- `.gitignore`
  - 忽略构建产物和本地环境杂项。

## `.github/`

- `.github/workflows/release.yml`
  - GitHub Release 自动构建与打包流程。

## `cmake/`

- `cmake/EUIExamples.cmake`
  - 示例构建脚本。
  - 自动扫描 `examples/*.cpp` 生成同名可执行文件。
  - 旧 `eui_*_demo` 名称只作为 CMake 兼容 target 保留，不再作为公开示例名。
  - 负责探测并接入 GLFW / SDL2 窗口后端。

- `cmake/EUIConfig.cmake.in`
  - 安装后给 `find_package(EUI)` 使用的 CMake package 配置模板。

## `docs/`

- `docs/backend-abstraction-checklist.md`
  - 后端抽象化开发清单。

- `docs/backend-abstraction-issue.md`
  - 后端抽象化问题定义与目标架构。

- `docs/concise-ui-syntax-proposal.zh-CN.md`
  - quick 语法糖方向提案。

- `docs/framework-revamp-proposal.zh-CN.md`
  - 框架改造方向提案。

- `docs/include-cleanup-record.zh-CN.md`
  - `include/` 目录清理与拆分记录。

- `docs/project-structure.zh-CN.md`
  - 当前这份目录与职责总览文档。

- `docs/quick-decoupling-status.zh-CN.md`
  - quick 语法糖与旧语法解耦状态说明。

## `examples/`

- `examples/anchor_layout_demo.cpp`
  - 锚点布局最小展示示例。

- `examples/basic_widgets_demo.cpp`
  - 基础控件总览示例。

- `examples/calculator_demo.cpp`
  - quick 布局 + 输入 + 状态更新示例。

- `examples/graphics_showcase_demo.cpp`
  - 图形能力展示示例。
  - 主要覆盖模糊、阴影、3D 倾斜、渐变和参数调节。

- `examples/sidebar_navigation_demo.cpp`
  - 侧边栏切换和多区块布局示例。

## `preview/`

- `preview/0.jpg`
- `preview/1.jpg`
- `preview/2.jpg`
- `preview/3.jpg`
  - 预览图资源。

## `include/` 顶层文件

- `include/EUI.h`
  - 项目总入口。
  - 仍然承载 `Context` 主实现。
  - 当启用窗口后端时，也会带入 app/runtime/OpenGL backend glue。

- `include/Font Awesome 7 Free-Solid-900.otf`
  - 内置图标字体资源。

- `include/stb_truetype.h`
  - 第三方 `stb_truetype` 头文件。

## `include/eui/` 聚合头

- `include/eui/animation.h`
  - 动画模块聚合头。

- `include/eui/app.h`
  - 应用运行时层聚合头。
  - 对外暴露 `AppOptions` 与 `FrameContext`。

- `include/eui/core.h`
  - core 前向声明入口。

- `include/eui/graphics.h`
  - graphics 模块聚合头。

- `include/eui/quick.h`
  - quick 语法糖聚合头。

- `include/eui/renderer.h`
  - 渲染抽象层聚合头。

- `include/eui/runtime.h`
  - runtime 抽象层聚合头。

## `include/eui/core/`

- `context_fwd.h`
  - `Context` 与基础类型前向声明。

- `context_state.h`
  - `Context` 的内部状态结构。

- `context_text_measure.h`
  - 文本测量配置、缓存状态、UTF-8 解码与字符宽度工具。

- `context_utils.h`
  - hash、rect、alpha、命令比较等公共小工具。

- `foundation.h`
  - `Color`、`Rect`、`Theme`、`InputState`、`DrawCommand` 等基础模型。

## `include/eui/animation/`

- `animator.h`
  - transform 动画插值与动画辅助。

- `easing.h`
  - `CubicBezier` 与 easing 采样。

- `timeline.h`
  - 动画时间线片段模型。

## `include/eui/graphics/`

- `effects.h`
  - 颜色、渐变、描边、阴影、模糊等效果模型。

- `primitives.h`
  - 矩形、图片、图标等 primitive 模型。

- `transforms.h`
  - 2D / 3D 变换模型。

## `include/eui/quick/`

- `ui.h`
  - quick 公共 builder / scope / 控件 / 布局主入口。

- `detail/anchor_spec.h`
  - 锚点布局内部求解器。

- `detail/primitive_painter.h`
  - quick primitive 到底层 `Context` 的绘制桥。

## `include/eui/runtime/`

- `contracts.h`
  - 平台无关 runtime 契约。
  - 定义 `WindowMetrics`、`FrameClock`、`NativeWindowHandle`、`PlatformBackend`。
  - 当前 `NativeWindowHandle` 已经能区分 GLFW / SDL2 句柄来源，但还不是完整 OS-native handle 抽象。

- `frame_context.h`
  - 中性的帧上下文模型。
  - 把 `Context`、时钟、窗口指标和重绘请求能力组合起来。

## `include/eui/renderer/`

- `contracts.h`
  - 渲染后端抽象契约。
  - 定义 `DrawDataView`、`ClearState`、`RendererBackend`。

- `draw_data_adapter.h`
  - 把命令数组和文本 arena 适配成 `DrawDataView`。

## `include/eui/app/`

- `options.h`
  - 应用运行时参数。
  - 目前包含窗口尺寸、标题、VSync、文本后端和窗口后端选择。

- `frame_context.h`
  - `eui::app::FrameContext`。
  - 当前直接别名到 `eui::runtime::FrameContext`。

### `include/eui/app/detail/`

- `runtime_state_detail.inl`
  - 共享运行时状态。
  - 包含输入累积、dirty cache 和缓存纹理状态。

- `glfw_runtime_detail.inl`
  - GLFW 事件回调桥。

- `glfw_run_loop_detail.inl`
  - GLFW 窗口后端主循环实现。

- `sdl2_runtime_detail.inl`
  - SDL2 事件翻译和运行时辅助。

- `sdl2_run_loop_detail.inl`
  - SDL2 窗口后端主循环实现。

- `text_support_detail.inl`
  - 字体文件探测、内置图标字体路径解析和 UTF-8 文本辅助。

- `font_renderers_detail.inl`
  - Win32 / STB 文本渲染实现。

- `opengl_renderer_detail.inl`
  - 当前具体的 OpenGL 渲染器实现。

- `dirty_cache_detail.inl`
  - dirty region 比较、缓存贴图和局部刷新辅助逻辑。

- `app_entry_detail.inl`
  - `eui::app::run(...)` 的窗口后端选择与分发入口。

## 后端图形 API 与窗口 API 是否已经解耦

## 简短结论

现在的状态比之前更进一步，但仍然不是“完全解耦”。

当前最准确的判断是：

- 公共命名已经从 `demo` 收口为 `app`。
- 窗口层现在已经可以在 GLFW 和 SDL2 之间切换。
- 图形渲染层仍然是共享的一套具体 OpenGL renderer。
- `runtime/contracts.h` 和 `renderer/contracts.h` 仍然没有完全接管默认主执行链路。

所以当前状态应描述为：

- 窗口后端已经开始具备可切换能力。
- 渲染后端还没有真正做到可替换。
- 总体仍处于“部分解耦，主链路未完全契约化”的阶段。

## 已经完成的部分

### 1. 公共运行时命名已从 `demo` 改为 `app`

现在对外推荐使用的是：

```cpp
eui::app::AppOptions options{};
return eui::app::run(...);
```

这比把框架默认运行层叫成 `demo` 更准确。

### 2. 窗口后端已经可以切换

当前 `AppOptions` 里已经引入：

- `WindowBackend::Auto`
- `WindowBackend::GLFW`
- `WindowBackend::SDL2`

也就是说，同一套 `eui::app::run(...)` 入口，已经可以在 GLFW 和 SDL2 两个窗口后端之间切换。

### 3. 窗口后端与 OpenGL 渲染器已经不是同一份文件强绑

当前目录职责已经明显分开：

- GLFW 主循环在 `glfw_run_loop_detail.inl`
- SDL2 主循环在 `sdl2_run_loop_detail.inl`
- OpenGL renderer 在 `opengl_renderer_detail.inl`

这说明“窗口管理”和“OpenGL 命令渲染”已经不再塞在同一份实现里。

## 还没有完全解耦的部分

### 1. 共享主路径仍然直接依赖具体 backend 细节

`eui::app::run(...)` 现在虽然能选 GLFW / SDL2，但仍然直接分支到：

- `run_with_glfw(...)`
- `run_with_sdl2(...)`

也就是说，当前主执行链路仍然是“具体 backend 组合”，不是“纯 contracts 驱动”。

### 2. `NativeWindowHandle` 只完成了第一步抽象

当前它已经不再只是匿名 `void*`，而是会标注：

- `NativeWindowKind::GLFW`
- `NativeWindowKind::SDL2`

但这仍然只是“窗口库级别句柄”，不是：

- Win32 HWND
- X11 Window
- Wayland surface
- Cocoa NSWindow

所以这部分属于“开始解耦了，但还没有抽象到底”。

### 3. 渲染层仍然只有 OpenGL 这一个具体实现

当前 renderer 还没有变成：

- `OpenGlRendererBackend`
- `GlesRendererBackend`
- `VulkanRendererBackend`

这种平行实现结构。

所以图形 API 的“真正可替换”还没有完成。

### 3. `EUI.h` 仍然会在启用后端时直接包含窗口库头

当启用相应宏时，`EUI.h` 仍会直接引入：

- GLFW 头
- SDL2 头

这说明总入口头还没有彻底退化成纯 core + contracts。

### 4. `PlatformBackend` / `RendererBackend` 还没有成为默认主入口

虽然这些契约已经定义出来了，但当前默认主路径还不是：

- `runner + PlatformBackend + RendererBackend`

而仍然是：

- `app::run + GLFW/SDL2 run loop + shared OpenGL renderer`

## 当前关于“是否解耦”的准确表述

如果问：

“窗口管理是否已经可切换？”

答案是：

- 是，当前已经支持 GLFW / SDL2 两条窗口后端路径。

如果问：

“窗口层和图形渲染层是否已经完全通过 contracts 解耦？”

答案是：

- 还没有。

所以最终结论是：

- 命名已经收口
- 窗口后端已经可切换
- OpenGL renderer 仍是具体实现
- runtime / renderer 契约还没有完全接管默认主链路
