# 后端抽象化开发清单

## 目标

让 EUI 的核心保持“渲染后端无关”和“平台无关”，从而可以跑在：

- GLFW
- SDL2
- SDL3
- 自定义引擎窗口系统

并能对接这些图形后端：

- OpenGL
- OpenGL ES
- Vulkan
- 后续自定义 GPU 后端

## 当前耦合情况评估

### 已经比较合适的部分

- `eui::Context` 输出的数据本身已经基本与后端无关：
  - `DrawCommand`
  - text arena
  - 输入驱动的即时模式状态
- 大多数控件逻辑本身并不直接依赖 GLFW 或 OpenGL。
- 当前的命令模型仍然适合作为 renderer 之前的统一中间层。

### 当前仍然耦合得很重的部分

- 启用窗口后端时，`include/EUI.h` 仍然直接包含 GLFW / SDL2 头。
- `app::run(...)` 里同时承担了：
  - 创建窗口
  - 拉取输入
  - DPI 查询
  - 剪贴板桥接
  - 帧调度
  - swap / present
- `Renderer` 是一个具体的 OpenGL renderer，不是抽象接口。
- 当前脏区缓存逻辑默认依赖 OpenGL 风格的 framebuffer copy 行为。
- 文本后端选择逻辑也有一部分还混在当前 app/runtime 里。

### 实际结论

- Core UI 已经接近可移植。
- app/runtime 层目前还不可移植。
- renderer 层目前也不可移植。
- 下一步真正大的架构动作应该是：
  - 拆 core
  - 拆平台运行时
  - 拆图形 renderer

## 强建议

不要继续把这些逻辑都塞在一个巨大的 `EUI.h` 里。

### 建议目录结构

```text
include/
  EUI.h                         # core only：Context、DrawCommand、widgets、layout
  eui/
    runtime.h                   # 平台无关的 app/runtime 接口
    platform/
      glfw.h
      sdl2.h
      sdl3.h
    renderer/
      opengl.h
      gles.h
      vulkan.h
```

### 原因

- 平台抽象和 renderer 抽象后面都会快速膨胀。
- GLFW + OpenGL + Win32 文本 + 未来 SDL/Vulkan 如果都塞在一个头里，维护成本会越来越高。
- 现在拆文件的代价，远小于 SDL/Vulkan 接入以后再回头拆。

## 目标架构

### 第 1 层：Core UI

负责：

- `Context`
- `DrawCommand`
- layout
- widgets
- 文本编辑逻辑
- 动画状态

不应该知道：

- GLFW
- SDL
- Vulkan
- OpenGL
- swapchain
- native window handle

### 第 2 层：平台运行时

负责：

- 窗口生命周期
- 事件循环
- 输入采集
- 剪贴板
- 时间
- DPI / content scale
- sleep / wait / poll
- close flag

不应该知道控件内部细节。

它的职责应该只是把 native input 翻译成 `eui::InputState`。

### 第 3 层：图形渲染后端

负责：

- GPU 资源生命周期
- 字体纹理上传 / atlas 资源
- 命令光栅化或三角化
- clipping / scissor
- render target 提交
- 如果后端支持，负责局部刷新 / frame cache 策略

不应该创建窗口。

### 第 4 层：App Runner

负责把这些东西拼起来：

- `Context`
- platform backend
- renderer backend
- 用户回调

## 建议的公开接口

### 平台无关的窗口指标

```cpp
struct WindowMetrics {
    int window_w;
    int window_h;
    int framebuffer_w;
    int framebuffer_h;
    float dpi_scale_x;
    float dpi_scale_y;
    float dpi_scale;
};
```

### 平台无关的帧上下文

```cpp
struct FrameContext {
    Context& ui;
    float dt;
    WindowMetrics metrics;
    bool* repaint_flag;

    void request_next_frame() const;
};
```

### 平台后端接口

```cpp
struct PlatformBackend {
    virtual ~PlatformBackend() = default;

    virtual bool should_close() const = 0;
    virtual void poll_events(bool blocking, double timeout_seconds) = 0;
    virtual WindowMetrics query_metrics() const = 0;
    virtual InputState collect_input() = 0;
    virtual const char* get_clipboard_text() = 0;
    virtual void set_clipboard_text(const char* text) = 0;
    virtual double now_seconds() const = 0;
    virtual void present() = 0;
};
```

### 渲染后端接口

```cpp
struct RendererBackend {
    virtual ~RendererBackend() = default;

    virtual void begin_frame(const WindowMetrics& metrics) = 0;
    virtual void render(const std::vector<DrawCommand>& commands,
                        const std::vector<char>& text_arena,
                        const WindowMetrics& metrics) = 0;
    virtual void end_frame() = 0;
    virtual void release_resources() = 0;
};
```

## 一个关键设计点

平台后端和图形后端必须是两个不同概念。

### 原因

- SDL 可以承载 OpenGL、GLES、Vulkan。
- GLFW 也可以承载 OpenGL 或 Vulkan。
- 你自己的引擎也可能已经有现成窗口和 swapchain。

所以不要把抽象根做成：

- `GlfwOpenGlBackend`

而应该拆成：

- platform backend
- renderer backend
- 针对常用组合提供可选 glue

## Native Handle 策略

### 不要在通用公开 API 里暴露 GLFW 类型

不好的例子：

```cpp
GLFWwindow* window;
```

更合理的方向：

```cpp
enum class NativeWindowKind {
    None,
    Win32,
    X11,
    Wayland,
    Cocoa,
    SDL,
    GLFW
};

struct NativeWindowHandle {
    NativeWindowKind kind;
    void* handle0;
    void* handle1;
};
```

### 原因

- Vulkan surface 创建可能需要 native handle。
- OpenGL / GLES 也可能需要 current-context 相关入口。
- 但 core runtime API 仍然不应该绑死某一种窗口库。

## Renderer 抽象说明

### `DrawCommand` 继续作为跨后端 UI 命令层

当前这个方向仍然是对的。

不同 renderer 只需要把 `DrawCommand` 转成自己的提交形式：

- 立即模式 GL 绘制
- GLES 绘制
- Vulkan 顶点 / 索引缓冲
- retained GPU batch

### 不要把 dirty-region cache 绑死在 OpenGL 假设上

当前缓存策略默认依赖 OpenGL 风格 framebuffer copy。

这件事不能 1:1 套到所有后端。

更合理的方向是：

- 把 partial redraw / cache 策略放到 renderer capability 背后
- 让 renderer 自己声明：
  - 是否支持 partial redraw
  - 是否支持 framebuffer copy cache
  - 是否更适合 full redraw

### 建议的 renderer capability 模型

```cpp
struct RendererCaps {
    bool supports_partial_redraw;
    bool supports_frame_cache_copy;
    bool supports_scissor;
};
```

## 文本后端也要拆概念

文本后端不应该和窗口后端绑成一个概念。

### 应该拆开的三个维度

- 平台 / 窗口后端
- GPU renderer 后端
- 文本栅格化后端

### 原因

- Win32 text 是 Windows 特有，不是 GLFW 特有。
- STB text 是跨平台的，不是 OpenGL 特有。
- Vulkan / OpenGL / GLES 完全可以共用一套文本栅格来源，只要纹理上传路径抽象出来。

## 迁移计划

### 阶段 1：把 GLFW 从通用 Frame API 中拿掉

- 从通用 `FrameContext` 中移除 `GLFWwindow*`
- 换成中性的 `WindowMetrics`
- 如果确实需要 native access，就在 core 之外暴露可选 opaque handle API

### 阶段 2：抽出运行时循环契约

- 把当前 `app::run(...)` 变成依赖以下两者的通用 runner：
  - 一个 platform backend
  - 一个 renderer backend
- 当前 GLFW + OpenGL 路径先作为第一套具体实现

### 阶段 3：把当前 GLFW 逻辑抽成后端

- 新建 `GlfwPlatformBackend`
- 把这些逻辑移进去：
  - init / terminate
  - callbacks
  - event pump
  - DPI 查询
  - clipboard
  - input translation
  - swap interval

### 阶段 4：把当前 OpenGL renderer 抽成后端

- 新建 `OpenGlRendererBackend`
- 把这些逻辑移进去：
  - renderer state
  - cache texture
  - scissor
  - draw command 提交
  - 字体 GL 资源管理

### 阶段 5：重安放 dirty-region 策略

- 当前 dirty-region diff 逻辑可以继续共用
- 当前 framebuffer copy cache 必须改成 renderer 可选能力
- 通用 runner 后面要按 renderer capability 分支，而不是按 OpenGL 假设分支

### 阶段 6：先接 SDL，不动 core

- 增加 `SdlPlatformBackend`
- 第一目标先做：
  - SDL + 当前 OpenGL renderer
- 这样可以先验证平台抽象，再动 Vulkan

### 阶段 7：接 GLES renderer

- 先重新审视当前 GL 路径是否兼容：
  - immediate mode 不能直接保留
  - client-state fixed pipeline 不能直接保留
- 实际大概率结论是：
  - 需要把 OpenGL renderer 改成现代 buffer/shader 路径
  - 然后再从这条路径派生 desktop GL + GLES

### 阶段 8：接 Vulkan renderer

- 保持 `DrawCommand` 输入不变
- 转成顶点 / 索引缓冲
- 显式管理 glyph atlas / image 资源
- 重新实现适合 Vulkan 的 scissor 和 partial redraw 策略

## 关键警告

如果你后面一定要 Vulkan 和 GLES，就不要继续加深现在这套 fixed-function OpenGL renderer。

### 原因

当前 renderer 还是偏 legacy immediate/fixed pipeline 风格。

它对今天的 GLFW + desktop GL 示例运行路径来说还算够用。

但它不是这些目标的长期基础：

- GLES 可移植
- Vulkan 后端对齐
- 多现代 GPU 后端共享实现

### 更合理的长期路线

- 保留当前 GL renderer 作为 legacy backend
- 新引入一套 modern renderer backend interface
- 新后端统一往这些方向靠：
  - vertex / index buffer
  - shader
  - texture atlas abstraction
  - explicit pipeline state

## 建议的工作顺序

1. anchor layout 已完成；后续只维护 quick-only 方向
2. 如有需要，再补公开 primitive / clip API
3. 先把 GLFW 从通用 frame API 中移掉
4. 拆 runtime backend interface
5. 拆 renderer backend interface
6. 把当前 GLFW + GL 路径放进这些接口背后
7. 增加 SDL + GL 路径
8. 增加 modern GL / GLES 路径
9. 增加 Vulkan 路径

## 开发清单

### API 清理

- 从共享头里移除后端特有公开类型
- 定义平台无关 runtime struct
- 定义平台无关 renderer interface

### 平台抽象

- 定义时间源接口
- 定义剪贴板接口
- 定义输入翻译契约
- 定义 DPI / content scale 契约
- 定义 native handle 导出契约

### 渲染抽象

- 定义 renderer capability flags
- 定义生命周期钩子
- 定义资源释放钩子
- 定义 partial redraw / cache 归属

### 兼容性

- 让当前 `app::run(...)` 通过兼容包装继续工作
- 不要一下子打断现有 examples
- 为当前用户保留默认 backend 组合

### 验证

- 同一套示例必须能跑在：
  - GLFW + OpenGL
  - SDL + OpenGL
- 同一套 core UI callback 不应该被重写
- 对不支持 cache-copy 优化的后端，dirty-region 行为也必须优雅降级

## 最低验收标准

- `Context` 在 core-only 模式下可以不依赖 GLFW 编译
- 通用 frame API 不再暴露 `GLFWwindow*`
- 旧 `eui_*_demo` 名称仍可通过 CMake 兼容 target 继续使用
- SDL backend 能正确喂给 `InputState`
- 更换 renderer backend 时不需要改 widget 代码
- 后端特有代码已经从 core 主路径里移走
