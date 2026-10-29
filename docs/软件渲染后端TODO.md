# 软件渲染后端 TODO

## 目标与范围

为 EUI-NEO 增加纯 CPU 软件渲染后端：不创建或加载 OpenGL/Vulkan context、instance、device、shader、swapchain，不要求安装 OpenGL/Vulkan SDK。
CPU 后端消费现有 Runtime 下发的绘制命令，将结果光栅化到 CPU 像素缓冲区，再通过窗口呈现接口提交给桌面窗口。

后端应支持项目已有的 GLFW、SDL2 窗口路径和当前支持的平台；每一种组合都必须实际验证，未验证组合不得标记为支持。
本清单是实施规划，不表示相关能力已经存在。相关架构见[渲染后端架构](渲染后端架构.md)。

目标能力包括普通 UI 所需的几何、文字、图片、裁剪、透明混合、缓存和局部重绘。像素语义应与现有后端尽量一致；允许在文档声明的 CPU 支持范围内存在抗锯齿和边缘像素差异。

不属于本清单范围：OpenGL/Vulkan 或其他 GPU 后端、GPU shader、Shadertoy、多 pass/feedback、外部 GPU 图像直接采样、backdrop blur，以及必须依赖 GPU readback 才能工作的效果。以上能力在 CPU 模式下必须明确报告不支持或使用调用侧提供的普通 CPU 内容，不得悄悄触发 GPU 初始化。

## 架构约束

```text
Runtime 绘制命令
  -> CpuRenderBackend
  -> CPU backing store（像素缓冲区）
  -> WindowPresenter
  -> GLFW / SDL2 桌面窗口
```

- [ ] `CpuRenderBackend` 只实现绘制、缓存和帧生命周期；不处理窗口事件，不直接依赖 GLFW/SDL2 类型。
- [ ] 新增窗口无关的 CPU 像素呈现契约。明确像素指针、宽高、stride、像素格式、alpha 规则和 dirty rect 的所有权与有效时长。
- [ ] 明确 backing store 使用的通道顺序、预乘或非预乘 alpha、颜色空间、坐标原点、逻辑尺寸与 framebuffer 尺寸换算规则。
- [ ] 将 presenter 与 rasterizer 分离；窗口后端只负责窗口系统适配和像素提交，不承担 UI 光栅化。
- [ ] 不让 public facade、components、examples 或 Runtime 暴露 CPU raster library、GLFW、SDL2 专属类型。
- [ ] CPU 配置中不得初始化、动态加载或间接链接 OpenGL/Vulkan。确认项目选用的 raster library 确实支持纯 CPU 模式。

## 阶段一：呈现路径与构建隔离

- [ ] 定义 CPU 后端及 presenter 的窗口组合支持矩阵，覆盖 GLFW/SDL2 和项目支持的平台；先选可运行的最小组合建立端到端样例。
- [ ] 为窗口后端增加获取/更新 CPU 像素的能力。SDL2、GLFW 和各平台原生路径分别确认窗口 surface 的创建、呈现、resize 与销毁语义。
- [ ] CPU 窗口不得设置 OpenGL/Vulkan 专用创建标志；窗口创建失败时返回明确错误，不回退到 GPU 后端。
- [ ] 在 CMake `EUI_RENDER_BACKEND` 选项中加入 `cpu`，更新选项校验、工厂分派、源文件和编译定义。
- [ ] 将 CPU 专用依赖限制在 CPU 配置；CPU-only configure/build 不查找或链接 OpenGL/Vulkan SDK、loader 或 backend 源文件。
- [ ] 验证 `auto` 的既有选择行为不被 CPU 后端意外改变，文档说明 CPU 必须如何显式选择。
- [ ] 实现 CPU backend 的 initialize、valid、makeCurrent、beginFrame、present 和销毁流程；处理 surface 尺寸变化与零尺寸窗口。

完成条件：至少一个窗口后端可在无 OpenGL/Vulkan 安装的环境中配置、构建并显示 CPU 像素缓冲区；进程依赖和构建日志可证明没有 GPU backend 依赖。

## 阶段二：基础绘制命令

- [ ] 实现清屏和 framebuffer 尺寸管理；定义首次绘制、窗口恢复及完整重绘的确定行为。
- [ ] 实现 scissor 裁剪，统一浮点绘制坐标到像素边界的取整规则，验证嵌套裁剪和窗口边缘裁剪。
- [ ] 实现圆角矩形填充、透明度、基础渐变与边框；逐项对照 `RoundedRectDrawCommand` 字段定义支持范围。
- [ ] 实现多边形填充及现有 polygon 命令要求的颜色、透明度和裁剪行为。
- [ ] 实现灰度与彩色文字 atlas 采样，支持 atlas generation 更新、文字颜色/透明度、裁剪和高 DPI 尺寸。
- [ ] 实现普通 CPU 图片纹理的创建、更新、销毁和绘制；定义 RGBA 输入、stride、tint、透明混合和圆角裁剪行为。
- [ ] 为不支持的纹理格式、无效尺寸、空指针、超大分配和资源创建失败定义安全且可诊断的行为。
- [ ] 对尚未实现的圆角、边框、渐变、阴影或图片选项，显式决定支持、忽略并记录，或返回可诊断的降级结果；不得误称完全兼容。

完成条件：常见控件所需的矩形、多边形、文字和普通图片可以独立运行；每项已支持命令有确定性测试和能力记录。

## 阶段三：Runtime 缓存与完整帧流程

- [ ] 实现 `ensureRenderCache`、`renderCacheWasRecreated`、`releaseRenderCache` 和 resize 后的缓存重建。
- [ ] 实现 `beginRenderCacheFrame`、`endRenderCacheFrame` 与 `blitRenderCache`，覆盖 Full、Dirty、Existing 三种模式。
- [ ] 保证 dirty rect 仅影响指定区域，未变化区域保留前一帧内容；呈现接口能接收或安全退化为全量更新。
- [ ] 验证 render cache 不可用或分配失败时 Runtime 的 direct-render fallback 仍正确工作。
- [ ] 检查 `LayerHandle` 在 Runtime 中的实际使用；若 CPU 首版不支持 retained layer，明确返回不支持并验证调用侧降级；若支持，则补齐创建、resize、绘制和销毁测试。
- [ ] 主窗口与子窗口共用正确的 active backend；验证窗口切换、按需渲染、静置不重绘和 present 时序。
- [ ] 处理最小化、零 framebuffer 尺寸、DPI 改变、窗口恢复、频繁 resize 和反复创建销毁，不读取失效的像素缓冲区。
- [ ] 如呈现与光栅化分线程，定义同步和双缓冲规则，确保 presenter 不读取正在修改的缓冲区；首版可保持单线程。

完成条件：Runtime 的完整帧、dirty 重绘、缓存重建和 direct-render fallback 都有回归测试；窗口生命周期无资源泄漏或陈旧画面。

## 阶段四：跨平台适配与视觉验收

- [ ] 对支持矩阵中的每个平台/窗口后端组合验证像素呈现、resize、DPI、最小化/恢复、主子窗口和关闭流程。
- [ ] 建立离屏像素测试，断言清屏色、矩形边界、裁剪、预乘 alpha 混合、文字 atlas 和纹理更新结果。
- [ ] 建立 Runtime probe 或应用 smoke test，覆盖基本控件、文字、图片、dirty repaint 与窗口交互。
- [ ] 固定测试字体、DPI、颜色格式和容差；GPU/CPU 比较仅对双方都支持的基础效果执行，不比较明确排除的效果。
- [ ] 为各平台记录呈现方式、像素格式转换、已知限制和测试环境；将未验证的窗口/系统组合标为未支持。
- [ ] 在 Release 构建测量全窗口重绘、dirty 重绘、文字密集和图片密集场景的 CPU 时间、内存占用及帧率。
- [ ] 依据测量结果再实现脏区合并、tile/图层缓存或并行光栅化；优化不能改变像素正确性及 dirty rect 语义。
- [ ] 更新后端架构文档、构建说明和示例，列出 CPU 后端能力矩阵及明确不支持项。

完成条件：支持矩阵内的组合均通过自动化构建/像素测试和真实窗口 smoke test；性能基线、限制和依赖均有记录。

## 贯穿各阶段的约束

- [ ] 确认 CPU backing store 和临时图层的内存预算；分配失败不崩溃、不越界，并能继续尝试后续帧。
- [ ] 检查整数溢出、stride/尺寸不匹配、负尺寸、非有限坐标和极端 DPI 缩放输入。
- [ ] 颜色混合、裁剪和边界计算集中实现并单测，避免 GLFW/SDL2 presenter 各自改变绘制语义。
- [ ] CPU-only 测试可在无图形设备、无 GPU SDK 环境运行；真实窗口测试单独标记环境要求。
- [ ] 任务只有在实现、测试和文档完成后勾选，并记录构建配置、平台、窗口后端和测试入口。

## 完成项登记规则

本清单中的勾选表示该项已在指定平台和窗口后端实现并验证，不表示所有组合自动获得支持。
能力矩阵只记录已实测行为；不支持项保持明确，不通过静默忽略或 GPU fallback 伪装兼容。