# 渲染后端架构与流程

本文说明 EUI-NEO 的 OpenGL/Vulkan 渲染后端架构、模块职责、资源生命周期和渲染流程。

## 目标

EUI-NEO 支持以下后端组合与共享能力：

- GLFW / SDL2 双窗口后端。
- OpenGL / Vulkan 双渲染后端。
- 同一套 DSL、组件、layout、text/image CPU 逻辑。
- OpenGL 与 Vulkan 输出同一 DSL 时视觉语义一致。

核心原则：

- 窗口后端只管理窗口、输入、IME、剪贴板、光标、图标、native handle、父子窗口和 modal 行为。
- 渲染后端只管理 GPU API、surface/context、swapchain/framebuffer、frame lifecycle、render cache、primitive/text/image GPU 资源。
- Runtime 只管理 DSL 树、layout、事件、dirty rect、动画和 render dispatch，不知道 GLFW/SDL2/OpenGL/Vulkan 细节。
- Public facade、components、examples 不暴露 OpenGL/Vulkan/GLFW/SDL2 专属类型。
- 模块调整需同步更新调用路径、头文件引用、构建选项和文档，避免重复实现。

## 方向依赖

```text
examples / user app
  |
include/eui_neo.h, include/eui/*
  |
core/app
AppRunner / MainWindowRuntime / DslWindowManager
  |
  +--> core/window  -----> GLFW / SDL2 / native window handles
  |
  +--> core/input   -----> backend-neutral input events
  |
  +--> core/dsl runtime
          |
          +--> core/render public API
                   |
                   +--> core/render/opengl  -----> OpenGL
                   |
                   +--> core/render/vulkan  -----> Vulkan
```

边界要求：

- `core/app` 可以调 `core/window`、`core/input`、`core/render` 公共接口，但不能 include `<glad/glad.h>` 或 Vulkan 头。
- `core/window` 可以 include GLFW/SDL2 头，但不能创建 shader、texture、swapchain 或 render cache。
- `core/render` 公共层只能放后端无关接口和类型。
- `core/render/opengl` 可以 include glad/OpenGL 头。
- `core/render/vulkan` 可以 include Vulkan 头。
- `include/`、`components/`、`examples/` 不得 include 后端专属头。

### 外部 GPU 图像

公共图片组件通过 `GpuImage` 和显式 revision 接收借用纹理，依赖方向仍是 DSL/Runtime →
图片 primitive → RenderBackend。`GpuDeviceInfo`/`GpuImageDescriptor` 使用带 API 标记的整数句柄，
不暴露后端具体类或引入 SDK 头。原生资源创建、布局和生产者同步属于应用的 GPU 适配层；
框架负责采样、缓存失效和最后一次 GPU 读取后的引用回收。

OpenGL 要求使用同一上下文；Vulkan 要求使用同一设备和图形队列。不支持跨 API 或跨设备资源共享。
通过实例 identity 拒绝错误窗口的描述，避免原生句柄数值复用误认成相同设备。
后端直接采样外部资源，不进行 CPU 读回或像素上传。
同资源内容更新只变 revision，不应反复创建纹理包装；静态图像不启动持续动画循环。

主/子窗口的 active backend 作用域覆盖 update/compose/render，供应用在正确窗口线程查询设备。
`DslAppConfig::onShutdown` 在主窗口设备销毁前供应用停止生产者并清理引用。
具体使用限制及两端同步约定见 [动态纹理](../guides/dynamic-textures.md)。

Vulkan 呈现完成信号量按 swapchain image 索引分配，不能用单个 in-flight submit fence
推断上一轮 presentation 已经消费信号量。同一图像重新 acquire 后才能复用其呈现信号量。
外部图像 probe 在启用 validation layer 时将 Validation Error/VUID 输出视为测试失败。

窗口尺寸变化时，Vulkan 后端等待设备空闲，释放依赖旧尺寸的渲染资源，并通过
`VkSwapchainCreateInfoKHR::oldSwapchain` 将仍存活的旧交换链交给新链创建调用。
该调用返回后销毁旧链；创建失败时也不能继续 acquire 已退役的旧链。
图像视图、framebuffer、命令池或同步对象创建失败时，释放本次已创建的交换链资源，
跳过该帧提交，后续帧重新尝试创建完整资源。

## 模块职责

### Public Facade

稳定入口：

- `include/eui_neo.h`
- `include/eui/*.h`

不得暴露：

- `GLFWwindow*`
- `SDL_Window*`
- `<glad/glad.h>`
- `<vulkan/vulkan.h>`
- OpenGL/Vulkan 具体后端类

### AppRunner / App Main

职责：

- 统一主循环状态机。
- 统一主窗口和子窗口 update/render 流程。
- 处理帧时间、Debug 标题统计、按需渲染、主动节流。
- 调用窗口事件、runtime update、render backend frame。

非职责：

- 不创建 OpenGL context。
- 不创建 Vulkan instance/surface/swapchain。
- 不直接调用 `gl*` / `vk*`。
- 不实现 primitive/text/image 绘制。

### WindowBackend

职责：

- 创建、销毁、显示、隐藏、raise、focus 窗口。
- 管理 GLFW/SDL2 的窗口 flag 和 GL/Vulkan window hint。
- 查询 window size、framebuffer size、DPI、pointer scale。
- 翻译窗口、鼠标、键盘、滚轮、文本输入事件。
- 输入法候选框定位、剪贴板、光标、窗口图标。
- 提供 `NativeWindowInfo` 给平台能力和 Vulkan surface 创建。
- 处理父子窗口、modal、置顶/focus 等窗口语义。

非职责：

- 不绘制。
- 不持有 GPU resource。
- 不持有 render cache。
- 不在事件泵里写 OpenGL/Vulkan draw/present 分支。

### Runtime

职责：

- DSL tree compose/layout。
- 事件派发、focus、IME rect、dirty rect、动画状态。
- 调用 primitive/text/image/Shadertoy facade。
- 通过 `RenderBackend` 下发后端无关 draw command。

非职责：

- 不创建 OpenGL/Vulkan resource。
- 不包含 GLFW/SDL2 类型。
- 不包含 OpenGL/Vulkan 分支。

### RenderBackend

职责：

- 管理 OpenGL context 或 Vulkan instance/surface/device/swapchain。
- 管理 frame lifecycle：begin frame、clear、render cache、draw、present、resize、cleanup。
- 管理后端专属 primitive/text/image/Shadertoy GPU 资源。
- 处理 minimized/zero-size 窗口。

非职责：

- 不读取窗口事件。
- 不决定窗口关闭、隐藏托盘、modal focus。
- 不修改 DSL tree 或 layout。

## 目录结构

主要模块与文件如下：

```text
core/app/
  app_runner.h
  main_window_runtime.h
  dsl_window_manager.h
  dsl_window_runtime.h
  glfw_app_main.cpp
  sdl2_app_main.cpp

core/input/
  input_types.h
  input_state.h

core/window/
  window_types.h
  window_backend.h/.cpp

core/platform/
  async.h/.cpp
  network.h/.cpp
  platform.h/.cpp
  native_bridge.*
  tray_bridge.*
  ime_bridge.*

core/render/
  render_backend.h/.cpp
  render_surface.h
  render_types.h
  primitive.h/.cpp
  primitive_geometry.h
  text.h/.cpp
  text_types.h
  image.h
  image.cpp
  image_source.h/.cpp
  stb_image_impl.cpp
  shadertoy.h/.cpp
  shadertoy_json.cpp
  shadertoy_primitive.h/.cpp

core/render/opengl/
  opengl_backend.h/.cpp
  opengl_primitives.cpp
  opengl_text.cpp
  opengl_image.cpp
  opengl_shadertoy.cpp

core/render/vulkan/
  vulkan_backend.h/.cpp
  vulkan_cache.cpp
  vulkan_primitives.cpp
  vulkan_polygon.cpp
  vulkan_text.cpp
  vulkan_image.cpp
  vulkan_shadertoy.cpp
  vulkan_shadertoy_shaders.h
  vulkan_*_shaders.h
  shaders/
    shadertoy.vert
```

文件职责：

- `render_backend.h/.cpp`：后端无关接口、active backend、backend factory、当前窗口需要的 `RenderApi`。
- `render_surface.h`：窗口到渲染后端的握手数据。
- `primitive.cpp`：后端无关 primitive 状态、transform、dispatch。
- `primitive_geometry.h`：基础 `Rect`、shadow、polygon 等 CPU 几何语义；`Rect` 的 `radius = 0` 表示普通矩形，`radius > 0` 表示圆角矩形。
- `text.cpp`：字体解析、fallback、metrics、glyph vertices、atlas CPU 数据。
- `image.cpp`：`ImagePrimitive` 状态、fit/cover、transform、GIF 帧推进、静态图 texture cache 和后端无关 image draw dispatch。
- `image_source.*`：图片路径解析、remote/Bing 下载、SVG 栅格化、静态图/GIF CPU 解码缓存，以及基于已解码 RGBA 像素的主题色采样和缓存。`ShadertoyChannel::image` 复用该解码路径。
- `shadertoy.h/.cpp`：后端无关 graph、pass、四路 2D channel、uniform、校验、hash 和 fragment wrapper。`shadertoy_json.cpp` 负责 EUI JSON；`shadertoy_primitive.*` 管理图元时钟、区域内输入、feedback reset、后端句柄和最终 UI 合成。
- `opengl_primitives.cpp` / `vulkan_primitives.cpp`：各自的基础 `Rect` batch pipeline、backdrop immediate pipeline、vertex stream 和 draw。内部沿用 rounded-rect 命名，但该路径同时处理普通矩形和圆角矩形。
- `opengl_primitives.cpp` / `vulkan_polygon.cpp`：各自的 polygon GPU pipeline、边段数据、覆盖率抗锯齿和 draw；覆盖率依据多边形边段计算。
- `opengl_text.cpp` / `vulkan_text.cpp`：各自的 text atlas upload、descriptor/pipeline、vertex buffer、draw。
- `opengl_shadertoy.cpp` / `vulkan_shadertoy.cpp`：各自独占 program/pipeline、每 Pass 离屏目标、固定 2D sampler、descriptor/FBO/render pass 和 readback；公共 API 不暴露原始 GPU 对象。
- `opengl_image.cpp` / `vulkan_image.cpp`：各自的 image texture upload、retained layer texture/framebuffer、descriptor/pipeline、vertex buffer、draw。
- `vulkan_cache.cpp`：Vulkan render cache image/framebuffer、layer image layout transition、copy/采样 resolve pipeline、cache 资源释放。
- `opengl_backend.cpp` / `vulkan_backend.cpp`：各自的 context/surface/swapchain/frame lifecycle 总控。

Vulkan shader 源位于 `core/render/vulkan/shaders/`，生成头位于 `core/render/vulkan/vulkan_*_shaders.h`。修改 shader 源后必须运行：

```bash
python3 scripts/generate_vulkan_shaders.py
cmake --build build-vk --target eui_check_vulkan_shaders
```

生成脚本依赖 `glslangValidator`。不要手工编辑生成头，也不要只改 GLSL 源而不更新 SPIR-V 头。新增 shader 组时必须同时更新 `scripts/generate_vulkan_shaders.py`，例如 polygon 的 `polygon.vert` / `polygon.frag` 会生成 `vulkan_polygon_shaders.h`。

OpenGL 和 Vulkan 都会批量提交兼容的基础 `Rect`，包括纯色/渐变填充、border、外阴影和 inset shadow；这不是仅针对圆角矩形的优化。OpenGL 在 CPU 侧累积顶点并成批上传，Vulkan 使用专用 batch shader 和帧内持续映射的顶点流，且映射写入 offset 在一帧内只向前推进。text、image、polygon、Shadertoy、framebuffer/layer 切换、scissor/clear 等顺序或资源边界会先 flush 当前 Rect batch，容量和窗口尺寸变化也会触发 flush，以保持 paint order。Backdrop blur 需要读取已经绘制的 framebuffer，因此会绕过 batch，继续走 immediate 路径。

## Shadertoy 资源边界

公共 `shadertoy` 模块负责后端无关 graph、pass、Image/Buffer/Self/None channel、
uniform、校验、内容 hash 和两端 fragment wrapper；`ShaderToyPrimitive` 只管理图元
状态、局部时间、区域内 `iMouse`、feedback reset、后端句柄和最终 UI texture 合成。
Runtime 不持有原始 GPU 对象，公共 API 也不暴露 GL/Vulkan handle。

每个 Pass 在相同 extent 下拥有两张固定 RGBA32F ping-pong target。extent 只由图元尺寸和
`ShadertoyBuilder::resolutionScale()` 决定，不存在 Pass 级尺寸或格式。OpenGL 的
program/FBO/texture 与 Vulkan 的 pipeline/render pass/image/descriptor 分别由
`opengl_shadertoy.cpp` 和 `vulkan_shadertoy.cpp` 独占。`iResolution` 和 `iMouse`
使用这份 graph 尺寸，`iChannelResolution` 来自实际绑定的 2D 图片或源 Pass。

所有通道在 GLSL 中固定为 `sampler2D`。图片使用 linear/repeat 并在加载时垂直翻转；
Buffer、Self 和空纹理使用 linear/clamp-to-edge。该策略是固定运行时契约，不在公共 API
中暴露 sampler 配置。图片复用 `image_source` 的现有解码 cache，不引入第二套图片或
媒体依赖。

`shadertoy_json.cpp` 复用已有 `eui::json`/yyjson，将 UTF-8 JSON 配置转换为
同一个 `ShaderToyGraph`。相对 fragment、SPIR-V 和图片路径以配置文件目录为基准。
`assets/shaders/shadertoy/eui-shadertoy.schema.json` 描述紧凑 channel 字符串和标准
自定义 uniform；schema 和 Blackhole、Fish 预设都是普通 assets，不进入核心 GPU 模块。

文件与 inline source 共用 graph。OpenGL 由驱动从文件或内存编译；Vulkan 运行时只加载
SPIR-V，可消费离线产物，也可用 `eui_compile_shadertoy()`、
`eui_compile_shadertoy_inline()` 或 `eui_compile_shadertoy_config()` 在应用构建期生成。
配置 helper 会读取 JSON，自动发现所有文件 Pass、传递 uniform 声明并按
`spirv` 相对路径生成输出。两端热更新都先创建候选 GPU 对象，
完全成功后才替换；失败时保留旧画面并报告结构化错误。wrapper 和
`glslangValidator` 仅存在于 Vulkan 构建工具链，不链接到 `eui::neo`，运行时不启动
编译器；OpenGL 配置不会探测 Vulkan SDK。

所有 Pass 先在局部离屏目标执行，最终 Pass 再走普通 UI image 合成，统一处理圆角、
opacity、transform、祖先 clip 和 z-order。Shadertoy 鼠标状态只在变换及裁剪后的图元
区域内更新；区域内开始的按下会捕获到释放，不会被 UI 其他区域的指针活动驱动。

Keyboard、Video、Sound、Cubemap、Volume、动态纹理/GPU import、YUV、可配置
sampler 和每 Pass 格式/分辨率不在 Shadertoy 模块的支持范围内。OpenGL
与 Vulkan 使用相同的公共 API、shader/assets、fixture、示例和文档，公共能力需在两端保持一致。
完整语义和兼容矩阵见
[Shadertoy 底层图元](../guides/shadertoy.md)。

## 后端选择

CMake 只通过两个维度选择后端：

- `EUI_WINDOW_BACKEND=glfw|sdl2`
- `EUI_RENDER_BACKEND=auto|opengl|vulkan`

策略：

- `opengl`：默认。使用 OpenGL。
- `auto`：Vulkan SDK/loader 可用时选择 Vulkan，否则回退 OpenGL。
- `vulkan`：强制 Vulkan。Vulkan SDK/loader 或 EUI Vulkan 后端不可用时配置失败，不静默回退。

首次配置时会按 build 目录名推断常用组合：

- `build`：GLFW + OpenGL。
- `build-sdl2`：SDL2 + OpenGL。
- `build-vk` / `build-vulkan`：GLFW + Vulkan。
- `build-sdl2-vk` / `build-sdl2-vulkan`：SDL2 + Vulkan。

显式 `-D` 仍然可用，并且已有 CMake cache 会优先于目录名推断。

## 渲染流程

本节说明当前 `core/app/glfw_app_main.cpp`、`core/app/sdl2_app_main.cpp`、`include/eui_neo.h` 和 `core::dsl::Runtime` 的真实行为：主动帧率节流、按需渲染、脏区缓存、blur 的特殊处理。

### 流程相关文件职责

- `core/app/glfw_app_main.cpp` / `core/app/sdl2_app_main.cpp`：窗口创建、渲染后端创建、DPI 和鼠标比例计算、后端事件泵。
- `include/eui/app.h`：应用生命周期接口。
- `include/eui_neo.h`：完整 app 的单入口 facade，聚合 DSL app、组件、网络、平台和图片能力。
- `include/eui/dsl_app.h`：把 DSL compose 接到 Runtime，管理是否需要重新 compose。
- `core/dsl_runtime.h`：布局后的状态同步、交互、动画推进、dirty rect、framebuffer cache、实际渲染。
- `core/render/primitive.h` / `core/render/opengl/*` / `core/render/vulkan/*`：后端无关 primitive 状态和具体 GPU 后端绘制。

### 主循环

主循环采用事件驱动和按需渲染；动画期间按目标帧率更新。

静止时：

```text
后端 wait events
```

有动画时：

```text
按窗口所在显示器 refreshRate 计算下一帧时间
等待到下一帧时间点
update
如果有 dirty/full paint:
    render
    present
后端 poll events
```

Runtime 滚动惯性也计入动画状态：速度衰减期间主循环继续按目标帧率 update，只重绘滚动 viewport 的 dirty rect；速度降到停止阈值或滚动到边界后，Runtime 结束动画状态，主循环恢复 wait events。静止页面不会因为启用惯性而持续占用 CPU。

当前使用主动节流：

- GLFW/SDL2 当前都关闭 swap interval，不再依赖 present 的 vsync 阻塞。
- GLFW 通过窗口和显示器重叠面积判断窗口当前主要在哪个显示器上；Windows GLFW 路径会优先用 Win32 当前显示模式读取真实刷新率，避免 `glfwGetVideoMode()` 在部分高刷/混合刷新率环境里回落到 60Hz；SDL2 使用窗口所在 display 的 current mode。
- 使用显示器 refresh rate 作为默认动画帧率。
- 如果配置里的 `.fps(...)` 是正数，则使用 `min(refreshRate, fps)`。
- Windows 下使用 `timeBeginPeriod(1)` 和高精度 waitable timer 做 frame pacing。动画帧按目标时间点睡眠唤醒，避免普通事件等待把 120/240Hz 拖低，也避免用忙等换帧率导致 CPU 占用升高。

部分示例会显式设置 `.fps(...)` 作为动画上限，例如 `gallery`、`calculator`、`clock` 和 `serial_tool` 默认使用 90 FPS；`gallery` 的 Settings 中打开 `Unlock 90 FPS limit` 后会把 `.fps(...)` 同步为 `0.0`，动画节流回到屏幕刷新率。

### App 生命周期

底层入口在 `include/eui/app.h`：

```cpp
namespace app {

const char* windowTitle();
bool showDebugStatsInTitle();
double debugTitleUpdateInterval();
bool showDebugOverlay();
double frameRateLimit();
int initialWindowWidth();
int initialWindowHeight();
bool initialize(GLFWwindow* window);
bool update(GLFWwindow* window,
            float deltaSeconds,
            int windowWidth,
            int windowHeight,
            float dpiScale,
            float pointerScale);
bool isAnimating();
void render(int windowWidth, int windowHeight, float dpiScale);
void shutdown();

}
```

`showDebugStatsInTitle()` 用于控制窗口标题中的调试统计，`debugTitleUpdateInterval()` 返回标题统计刷新周期。`Debug` 构建默认开启，`Release` 构建默认关闭；可以通过 `DslAppConfig::showDebugStatsInTitle(false)` 手动关闭。标题会显示 FPS、进程 CPU 占用估算，以及当前渲染后端一次 frame 的平均提交耗时。这里的 `GPU ms` 是渲染帧耗时指标，不等同于系统任务管理器里的整机 GPU 占用率。

`showDebugOverlay()` 只在 `DslAppConfig` 同时启用 `showDebugOverlay(true)` 并设置 `onDebugOverlay(...)` 回调时返回 `true`。Runtime 会在业务 `compose` 后调用该回调，因此布局调试框或性能标记属于独立的诊断层，不会混入业务组件；Release 构建默认不调用该层。

渲染统计字段用于拆分 Runtime、render cache 和后端提交成本：

- `Dirty N/P%`：Runtime 本帧维护的脏区数量和脏区面积占窗口面积的比例。
- `Draw R/P/TP/T/I`：本帧 Runtime 提交的基础 `Rect`、polygon、text prepare、text draw 和 image draw 数量；其中 `R` 是逻辑 Rect 提交量，不是 batch 合并后的物理 GPU draw call 数。
- `Pass X C Y B Z/P%`：Runtime 层渲染 pass、clear 次数、render cache blit 次数和 blit 面积比例。
- `Pipe RP/P%`：后端实际 render pass 数量和 render pass 面积比例。
- `Pipe Cp/Ba/Sub`：后端 copy region、pipeline barrier 和 queue submit 数量。
- `Pipe Pr/P%`：后端 present 次数和 present 面积比例。
- `Pipe Inc/A`：实际使用 incremental/damage present 的次数，以及当前后端是否报告支持该能力。
- `Pipe Rs`：后端 fallback resolve draw 次数，例如不能直接 copy 时用全屏三角形采样 render cache。
- `Full/Cache/Re`：full paint、使用 render cache、render cache 重建的帧比例。

这些诊断字段用于区分 CPU/Runtime 重绘、GPU cache 同步和窗口系统 present 的耗时。

DSL app 一般不直接实现这些函数，而是包含 `include/eui/dsl_app.h`，然后实现：

```cpp
const DslAppConfig& dslAppConfig();
void compose(eui::Ui& ui, const eui::Screen& screen);
```

### DSL App 更新流程

`include/eui/dsl_app.h` 当前不会每轮循环都重新 compose。

它只在这些情况 compose：

- 首次运行。
- 逻辑窗口尺寸变化。
- Runtime 检测到点击回调导致业务状态变化，并设置 `composeRequested()`。

流程大致是：

```text
如果首次或逻辑尺寸变化:
    composeFrame()

changed = runtime.update(...)

如果 runtime.composeRequested():
    composeFrame()
    runtime.update(..., deltaSeconds = 0)
    runtime.requestFullPaint()
    changed = true

return changed
```

点击后多做一次 `deltaSeconds = 0` 的 Runtime update，是为了让新 DSL 树和 Runtime 缓存实例马上同步。否则可能出现“新声明 + 旧实例坐标”混在同一帧里，导致文本或控件短暂错位。

### 脏区渲染

当前 Runtime 使用一个离屏 framebuffer cache 做保守脏区渲染。这里的“脏区”分成三层，不能混成一个概念：

1. Runtime repaint：只重新绘制和 dirty rect 相交的 DSL primitive。
2. Render cache blit：把离屏 cache 同步到当前窗口 backbuffer / swapchain image。
3. Present：把当前窗口 backbuffer / swapchain image 交给系统窗口合成器。

第 1 层由 Runtime 控制。第 2 层由具体 render backend 控制。第 3 层取决于平台窗口系统、图形 API 和驱动是否暴露 damage / incremental present 能力。

核心策略：

1. Runtime 按 id 缓存每个 `Rect` / `Text` 的动画值和 primitive。
2. `Ui::layout()` 后缓存根节点和每个子节点列表的绘制顺序，后续 update、hit-test、prepare text、render 都复用这份顺序，不在递归热路径反复构造 vector 和 stable sort。
3. layout 后同步缓存子树能力标记：是否需要 update、是否包含 dependent visual、是否包含 backdrop blur、是否阻塞 retained layer。Runtime 用这些标记跳过无关子树扫描。
4. 元素视觉值变化时，计算变化前后的 visual rect。
5. 对旧 rect 和新 rect 取 union，加入 dirty rect 列表。
6. render 时先渲染到离屏 cache。
7. full paint 时重画整棵 UI。
8. 局部变化时：
   - 绑定 cache framebuffer。
   - 对 dirty rect 开 `glScissor`。
   - 清理 dirty rect 内背景。
   - 只重画与 dirty rect 相交的元素。
   - 把 cache blit 到默认 framebuffer 或 swapchain image。

Vulkan 后端会按 swapchain image 记录 render cache generation。局部 repaint 后，如果当前 swapchain image 已经同步到最新 cache generation，可以跳过 cache blit；如果只缺少少量 generation，会只 copy 这些 generation 的 dirty rect；如果缺失历史、dirty rect 过多或面积过大，则回退为 full blit，保证视觉正确。

OpenGL 后端会按默认双缓冲 backbuffer 记录 render cache generation。局部 repaint 后，如果当前 backbuffer 已经同步到最新 cache generation，可以跳过 cache blit；如果只缺少少量 generation，会只 blit 这些 generation 的 dirty rect；如果缺失历史、dirty rect 过多或面积过大，则回退为 full blit，避免未变区域出现旧内容、黑块或闪烁。

Vulkan 的 true dirty present 依赖设备暴露 `VK_KHR_incremental_present`。后端只在该扩展可用时把 dirty rect 通过 `VkPresentRegionsKHR` 传给 `vkQueuePresentKHR`；如果驱动不暴露该扩展，`Pr` 仍会显示全窗口 present。这不是 EUI 可以强行开启的能力。

当前 dirty rect 已考虑：

- frame 变化。
- color / opacity / radius / border / shadow / blur 变化。
- 元素自身 transform 以及父容器继承 transform 的投影矩阵外接矩形，包括 2D transform 和 2.5D 透视投影。
- Runtime scroll state 变化，包括滚轮惯性推进。滚动内容和滚动条 thumb 通过 transform 更新，Runtime 只标记 viewport dirty rect，不重新 compose 页面。
- Text 的 frame / color / opacity / transform 变化。
- visualStateFrom 带来的按下缩放。

Runtime 每帧会给已访问的 Rect / Polygon / Text / Image / Svg / interaction / dirty key 实例打 seen 标记。本帧 DSL 树里没有再出现的 id 会在 update 末尾释放对应 primitive 和缓存状态，避免动态 UI 长期累积旧实例。

Runtime update 还有两类保守 CPU early-out：

- 静态子树 early-out：如果子树没有动画、交互、timer、scroll、dirty key、dependent visual、动态 image/svg 等运行时依赖，且本帧没有全树 update、没有 focus、没有祖先 frame/disabled 变化，指针也不在上一帧子树 paint bounds 内，就复用上一帧子树 paint bounds，跳过大部分 update 递归。
- hover target cache：如果指针位置、按键状态、DPI、树结构都没变，上一帧也没有动画，并且没有按下捕获，Runtime 复用上一帧 topmost hover 命中结果，避免复杂静态页面每帧做一次全树 interactive hit-test。

Retained layer cache 也是 Runtime 级底层优化，不需要组件 opt-in。它会为稳定静态子树创建离屏 layer texture，也会把相邻、稳定且可缓存的 sibling 合并成一个 paint run layer；OpenGL 使用 texture-backed framebuffer，Vulkan 使用 sampled color-attachment image + framebuffer，并在合成时按 premultiplied alpha 混合。render 热路径只读取 layout/update 阶段缓存的 blocker 和 subtree animation 标记，不再每帧递归扫描候选子树。Shadertoy、backdrop blur 等 dependent content 会阻止其所在子树进入 layer，并切断 sibling run，但不会阻止前后稳定 sibling 分别组成独立 run。这样 Gallery 这类复杂静态页面可以复用更少的 layer draw，而 `animation_240fps` 这类大量 leaf primitive 的页面不会被 layer 候选判断拖慢。

新建或重建 retained layer 的当帧不会立刻采样这个 layer。Runtime 会先把 layer 标记为可用，同时让该子树当帧走普通 primitive 绘制，并请求下一帧 full paint；下一帧再从稳定的 layer/render cache 状态接管。这样能避免 OpenGL backbuffer/render cache 首帧同步时出现静态侧边栏一类内容占位存在但画面未显示的问题，同时不关闭静态 UI 后续的 layer cache 命中。

这些优化不改变用户组件写法。它们减少 CPU 侧遍历、分配、排序和无关扫描；GPU 侧仍以 framebuffer cache + dirty rect repaint 为基础，并在合适的静态子树上叠加 retained layer cache。

### Blur 与脏区

`blur` 是特殊图元，不等同于普通半透明 rect。

当前 blur 的实现方式是 backdrop blur：

1. 从当前 framebuffer/cache 中取 blur rect 附近的 backdrop。
2. 用 framebuffer blit 复制到 1/2 尺寸的 backdrop texture。
3. fragment shader 多次采样这块降采样 texture。
4. 把采样结果和自身颜色混合。

这意味着 blur 依赖“它背后的已绘制内容”。如果只做局部 dirty patch，某些情况下 cache 里可能是半旧半新的：

- dirty rect 被 scissor 清掉了一部分。
- 周围卡片 hover/press/rotate 后只重画了局部。
- blur 自己或附近元素上一帧的内容还残留在 cache 的其他区域。

这时 blur 会把“半旧半新的 cache”采进去，就可能出现竖条、污染、错位的模糊背景。

Runtime 使用 `promoteBackdropBlurDirtyRegions()` 扩展受影响的脏区：

- 将本帧脏区合并为保守矩形。
- 如果脏区与 blur 的 backdrop 采样范围相交，将完整采样范围加入脏区。
- 重复检查直到不再扩展，覆盖相互影响的 blur 区域，然后按绘制顺序重画。

采样范围由 `backdropCaptureRect()` 根据当前 frame、blur 和 transform 计算。blur 不会仅因相交就直接要求 full paint；扩展后面积过大时，统一脏区策略仍可能转为整帧绘制。

代价是：blur 附近交互会比普通 rect 更贵。`gallery` 默认关闭 Glass/blur，也是为了默认功耗更稳。

### full paint 触发

这些情况会触发或升级为 full paint：

- 首帧。
- framebuffer 尺寸变化。
- Runtime cache 创建或重建。
- retained layer 创建或重建后用于同步接管的下一帧。
- 点击回调导致业务状态变化并重新 compose。
- 脏区合并后覆盖面积过大等情况触发整帧绘制策略。
- 调用 `Runtime::requestFullPaint()`。

### 当前流程限制

- 脏区是保守矩形，不是像素级 diff。
- 所有内容仍共用一个 framebuffer cache，retained layer cache 只是保守缓存静态子树，不是完整 layer tree。
- 当前有 Runtime 级 paint order / 子树能力缓存、静态子树 early-out 和自动 retained layer cache；OpenGL/Vulkan 都支持后端 layer 资源，复杂 blur、动态 image/svg、交互和动画子树仍会走普通 dirty repaint。
- blur 依赖 framebuffer 内容，会扩展受影响的采样范围，可能扩大到整帧。
- Vulkan 已支持 dirty-aware cache blit，但 true dirty present 仍取决于驱动是否支持 `VK_KHR_incremental_present`。
- OpenGL 已支持 dirty-aware cache blit，但 `SwapBuffers()` 仍会把当前 backbuffer 作为整帧交给系统；要进一步优化系统合成层 present 成本，需要 EGL/ANGLE swap damage 或其他平台 native damage present 路径。
- 已有基础矩形 clip 和 z-index；clip 通过 scissor 合成，z-index 影响同级绘制和 topmost hit-test。
- 容器 transform 会继承到子树渲染，支持 2.5D 透视投影；默认 hit-test 按布局矩形计算，开启 `.transformedHitTest()` 后会用投影矩阵反算命中。
- 当前不是 QML 那种完整 retained scene graph，也不是跨 Rect、polygon、text、image 等全部图元的通用 batch renderer；基础 `Rect` 已在 OpenGL/Vulkan 后端批量提交，其他复杂图元在高刷新率下成本会更明显。

## 维护约束

- 应用入口按窗口后端组织，渲染后端通过 `RenderBackend` 接入，不单独创建 Vulkan 应用入口。
- GLFW/SDL2 事件泵不包含 GPU 绘制或呈现逻辑。
- 文本、图片、多边形和缓存资源由对应渲染模块管理，后端主文件负责设备与帧生命周期。
- Public facade 不暴露 OpenGL/Vulkan/GLFW/SDL2 专属类型。
- OpenGL/Vulkan 共享图片解码与图元状态管理逻辑，并分别验证绘制结果。

## 验证

每轮架构改动至少执行：

```bash
git diff --check
rg -n "glad|OpenGL|Vulkan|GLFWwindow\\*|SDL_Window\\*" include components examples -g'*.h' -g'*.cpp'
python3 scripts/generate_vulkan_shaders.py --check
```

构建验证：

```bash
cmake -S . -B build
cmake --build build --target gallery --parallel

cmake -S . -B build-vk
cmake --build build-vk --target gallery --parallel
```

SDL2 改动额外验证：

```bash
cmake -S . -B build-sdl2 -DCMAKE_BUILD_TYPE=Release
cmake --build build-sdl2 --target gallery serial_tool --parallel

cmake -S . -B build-sdl2-vk -DCMAKE_BUILD_TYPE=Release
cmake --build build-sdl2-vk --target gallery --parallel
```

人工验收：

- GLFW/OpenGL gallery。
- GLFW/Vulkan gallery。
- SDL2/OpenGL gallery。
- SDL2/Vulkan gallery。
- resize、最小化、多窗口、modal、IME、托盘/隐藏恢复。
- OpenGL/Vulkan 文本、图标、图片、blur、render cache 视觉一致性。
