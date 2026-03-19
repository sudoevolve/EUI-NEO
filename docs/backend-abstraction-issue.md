# refactor(backend): 抽象平台与渲染后端，解耦 core 对 GLFW/OpenGL 的依赖

## 背景

EUI 当前的 Core UI 已经接近“后端无关”，`eui::Context` 输出的 `DrawCommand`、text arena、即时模式输入状态本身基本具备跨后端潜力。

但 app/runtime 和 renderer 层目前仍然强耦合：

- `include/EUI.h` 在启用窗口后端时仍直接包含 GLFW / SDL2 相关头
- `app::run(...)` 仍同时承担窗口创建、输入采集、DPI 查询、剪贴板桥接、帧调度、`swap / present`
- `Renderer` 仍是具体的 OpenGL renderer，而不是抽象接口
- dirty-region cache 逻辑默认依赖 OpenGL 风格 framebuffer copy
- 文本后端逻辑也有一部分混在 app/runtime 中

如果目标包括 SDL、GLES、Vulkan 或自定义引擎窗口系统，那么平台层、渲染层、文本层必须拆开。

## 目标

让 EUI Core 保持“平台无关”和“渲染后端无关”，并支持未来对接：

- GLFW
- SDL2
- SDL3
- 自定义引擎窗口系统

以及这些图形后端：

- OpenGL
- OpenGL ES
- Vulkan
- 后续自定义 GPU 后端

## 目标架构

建议拆成 4 层：

### 1. Core UI

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

### 2. 平台运行时

负责：

- 窗口生命周期
- 事件循环
- 输入采集
- 剪贴板
- 时间
- DPI / content scale
- `sleep / wait / poll`
- close flag

职责是把 native input 翻译成 `eui::InputState`。

### 3. 图形渲染后端

负责：

- GPU 资源生命周期
- 字体纹理上传 / atlas 资源
- 命令光栅化或三角化
- clipping / scissor
- render target 提交
- partial redraw / frame cache 策略

不应该创建窗口。

### 4. App Runner

负责把这些组件拼装起来：

- `Context`
- platform backend
- renderer backend
- 用户回调

## 建议目录结构

```text
include/
  EUI.h
  eui/
    runtime.h
    platform/
      glfw.h
      sdl2.h
      sdl3.h
    renderer/
      opengl.h
      gles.h
      vulkan.h
```

其中：

- `EUI.h` 只保留 core-only 内容
- 平台相关接口和实现移到 `eui/platform/*`
- 渲染相关接口和实现移到 `eui/renderer/*`

## 接口草案

平台无关窗口指标：

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

平台无关帧上下文：

```cpp
struct FrameContext {
    Context& ui;
    float dt;
    WindowMetrics metrics;
    bool* repaint_flag;

    void request_next_frame() const;
};
```

平台后端接口：

```cpp
struct PlatformBackend {
    virtual ~PlatformBackend() = default;

    virtual bool should_close() const = 0;
    virtual void poll_events(bool blocking, double timeout_seconds) = 0;
    virtual WindowMetrics query_metrics() const = 0;
    virtual InputState collect_input() = 0;
    virtual const char* get_clipboard_text() const = 0;
    virtual void set_clipboard_text(const char* text) = 0;
    virtual double now_seconds() const = 0;
    virtual void present() = 0;
};
```

渲染后端接口：

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

## 关键设计点

平台后端和图形后端必须是两个不同概念，而不是做成类似 `GlfwOpenGlBackend` 这种绑定式抽象。

原因：

- SDL 可以承载 OpenGL、GLES、Vulkan
- GLFW 也可以承载 OpenGL 或 Vulkan
- 自定义引擎也可能已经有自己的窗口和 swapchain

正确方向应为：

- platform backend
- renderer backend
- 常用组合提供可选 glue

## Native Handle 策略

通用公开 API 不应直接暴露 GLFW 类型。

不建议：

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

这样可以在需要 Vulkan surface、OpenGL/GLES current context 等 native access 时，通过 core 之外的可选 API 暴露能力，而不把 core 绑死到某个窗口库。

## Renderer 抽象要求

- `DrawCommand` 继续作为跨后端 UI 命令层
- 不同 renderer 只负责把 `DrawCommand` 转为自己的提交形式
- dirty-region diff 可以继续保留为通用逻辑
- framebuffer copy cache 不能再写死为 OpenGL 假设
- partial redraw / cache 行为应放到 renderer capability 背后

建议增加 capability 描述：

```cpp
struct RendererCaps {
    bool supports_partial_redraw;
    bool supports_frame_cache_copy;
    bool supports_scissor;
};
```

## 文本后端拆分要求

文本后端不应与窗口后端绑死。

需要拆开的维度：

- 平台 / 窗口后端
- GPU renderer 后端
- 文本栅格化后端

原因：

- Win32 text 是 Windows 特有，不是 GLFW 特有
- STB text 是跨平台的，不是 OpenGL 特有
- Vulkan / OpenGL / GLES 可以共用文本栅格来源，只需抽象纹理上传路径

## 实施任务

- [ ] 从共享头中移除后端特有公开类型
- [ ] 定义平台无关的 runtime struct
- [ ] 定义平台无关的 renderer interface
- [ ] 从通用 `FrameContext` 中移除 `GLFWwindow*`
- [ ] 用 `WindowMetrics` 替代当前窗口相关直接暴露
- [ ] 抽出通用 runner，使其依赖 `PlatformBackend` + `RendererBackend`
- [ ] 新建 `GlfwPlatformBackend`
- [ ] 将窗口初始化、事件循环、DPI、剪贴板、输入翻译、swap interval 移入 `GlfwPlatformBackend`
- [ ] 新建 `OpenGlRendererBackend`
- [ ] 将 renderer state、cache texture、scissor、draw command 提交、字体资源管理移入 `OpenGlRendererBackend`
- [ ] 重构 dirty-region / framebuffer copy cache 的归属
- [ ] 让 runner 根据 renderer capability 分支，而不是根据 OpenGL 假设分支
- [ ] 增加 `SdlPlatformBackend`
- [ ] 先实现 SDL + 当前 OpenGL renderer 组合，用来验证平台抽象
- [ ] 评估并推进 modern GL / GLES renderer 路径
- [ ] 为 Vulkan renderer 预留 `DrawCommand` 到顶点/索引缓冲的接入路径
- [ ] 让当前 `app::run(...)` 通过兼容包装继续工作
- [ ] 保留现有 examples 的默认 backend 组合，避免一次性破坏现有用法

## 风险与警告

- 如果后续一定要做 GLES 和 Vulkan，就不应该继续加深当前 fixed-function OpenGL renderer 的技术债
- 当前 legacy immediate/fixed pipeline 风格可以暂时保留为 legacy backend
- 但长期应引入 modern renderer backend 路线，统一走：
- vertex / index buffer
- shader
- texture atlas abstraction
- explicit pipeline state

## 建议顺序

- [x] anchor layout 已完成（当前对外只保留 quick-only 方向）
- [ ] 如有需要，再补公开 primitive / clip API
- [ ] 把 GLFW 从通用 frame API 中移除
- [ ] 拆 runtime backend interface
- [ ] 拆 renderer backend interface
- [ ] 把当前 GLFW + GL 路径放进新接口背后
- [ ] 增加 SDL + GL 路径
- [ ] 增加 modern GL / GLES 路径
- [ ] 增加 Vulkan 路径

## 验收标准

- `Context` 在 core-only 模式下可不依赖 GLFW 编译
- 通用 frame API 不再暴露 `GLFWwindow*`
- 旧 `eui_*_demo` 名称仍可通过 CMake 兼容 target 继续使用
- SDL backend 能正确生成并喂给 `InputState`
- 更换 renderer backend 时不需要改 widget 代码
- 后端特有代码已经从 core 主路径中移走
- 同一套示例至少能跑在 `GLFW + OpenGL` 与 `SDL + OpenGL`
- 对不支持 cache-copy 的后端，dirty-region 行为能优雅降级
