# Slint 渲染方式与 EUI-NEO 优化对比

本文聚焦 Slint 的渲染后端、renderer 选择、局部渲染和性能优化思路，并与 EUI-NEO 当前 OpenGL/Vulkan 渲染架构、Runtime 脏区策略和帧调度做对比。

调研时间：2026-06-05。

## 结论摘要

Slint 的渲染体系强调“平台 backend”和“renderer”分离：backend 负责窗口系统、事件循环和平台集成，renderer 负责把 UI scene 画成像素。Slint 提供多条成熟渲染路径，包括 Qt/QPainter 软件渲染、Software renderer、FemtoVG/OpenGL、FemtoVG/WGPU、Skia GPU/Software。不同 renderer 面向不同目标：嵌入式和 MCU 偏向 software partial rendering，桌面复杂 UI 偏向 Skia/FemtoVG GPU 路径。

EUI-NEO 当前架构和 Slint 在“平台与渲染分层”上方向接近：窗口后端是 GLFW/SDL2，渲染后端是 OpenGL/Vulkan，Runtime 不直接知道具体窗口和 GPU API。区别在于 Slint 更依赖成熟通用 renderer 生态，EUI-NEO 则自研 primitive/text/image 绘制和 framebuffer cache，因此控制力更强，但优化责任也完全落在项目自身。

EUI-NEO 当前最有价值的优化资产是 Runtime 级 dirty rect + render cache。它和 Slint Software renderer 的 partial rendering 目标类似，都是避免静止 UI 全量重绘；但 EUI-NEO 的实现是 GPU framebuffer cache + scissor 局部重画，能覆盖 OpenGL/Vulkan，代价是 blur、transform、clip、z-index 和多窗口场景需要更保守的正确性处理。

建议 EUI-NEO 后续优化重点：

1. 保留当前 dirty rect + framebuffer cache 路线，补强 debug overlay 和 redraw reason。
2. 把 primitive/text/image 的“是否需要更新 GPU 资源”和“是否需要重画像素”继续拆细。
3. 对文本、图片、圆角阴影、blur 做更明确的 cache/layer 策略。
4. 学习 Slint 的 renderer 选择思想，为低端设备准备 software 或 lighter renderer，而不是让 Vulkan/OpenGL 承担所有平台。
5. 长期从“单 framebuffer cache”演进到“局部 layer cache / retained render nodes”，减少 blur 和大面积 transform 导致的 full redraw。

## Slint 渲染架构

Slint 官方把 backend 和 renderer 拆成两个概念：

- Backend：封装操作系统和窗口系统交互，例如窗口创建、事件循环、平台抽象。
- Renderer：负责把 Slint scene 中的元素转换为像素。

Slint 可以编译多个 backend，并在应用启动时选择。选择优先级大致是：应用显式设置的平台、`SLINT_BACKEND` 环境变量、内置 backend 尝试顺序。官方文档列出的默认尝试顺序是 Qt、Winit、LinuxKMS。Slint 也允许应用不使用内置 backend，而是实现自己的 platform abstraction 和 window adapter。

### Backend 与 Renderer 组合

| 层级 | Slint 方案 | 主要用途 | 性能取向 |
| --- | --- | --- | --- |
| Backend | Qt | 使用 Qt 做窗口系统集成、渲染和原生控件风格。 | 依赖 Qt 生态，适合已有 Qt 环境。 |
| Backend | Winit | 使用 Rust `winit` 做跨平台窗口系统集成。 | 桌面主路径，renderer 可选。 |
| Backend | LinuxKMS | 无桌面合成器的嵌入式 Linux KMS/DRI。 | 嵌入式、设备端。 |
| Renderer | Qt Renderer | QPainter 软件渲染，无 GPU 加速。 | 简单可靠，但不追求 GPU 性能。 |
| Renderer | Software | CPU 软件渲染，支持 partial rendering 和 line-by-line 渲染。 | 轻量、可移植，适合 MCU/嵌入式；功能有限。 |
| Renderer | FemtoVG | OpenGL GPU 渲染；WGPU 变体可走 Metal/Vulkan/Direct3D。 | 轻量 GPU 矢量渲染，质量和复杂路径能力不如 Skia 稳。 |
| Renderer | Skia | GPU 渲染覆盖 OpenGL/Metal/Vulkan/Direct3D，也有软件路径。 | 功能强、质量好，但体积和构建成本高。 |

Winit backend 下，Slint 可通过 `SLINT_BACKEND` 指定 `winit-femtovg`、`winit-femtovg-wgpu`、`winit-skia`、`winit-skia-software`、`winit-skia-opengl`、`winit-software`。未显式指定时，会按启用情况先尝试 Skia，再尝试 FemtoVG，最后使用 software renderer。

### Slint Renderer 取舍

Slint 的 renderer 选择体现了一个重要思路：没有单一 renderer 同时最适合所有平台。

Software renderer 的优势：

- 不依赖 GPU。
- 可在极低资源环境运行。
- 支持 partial rendering。
- 可 line-by-line 输出，适合内存紧张设备。

Software renderer 的限制：

- 不支持旋转和缩放。
- 不支持 drop shadow。
- `clip: true` 与 `border-radius` 组合有限制。
- 文本渲染脚本覆盖有限。

FemtoVG 的优势：

- GPU 加速。
- OpenGL 路径较轻。
- WGPU 变体能覆盖 Metal、Vulkan、Direct3D。

FemtoVG 的限制：

- 官方文档提示文本和 path 渲染质量有时不够理想。
- 复杂图形质量和完整性不如 Skia。

Skia 的优势：

- GPU API 覆盖完整。
- 图形、路径、文本、滤镜等能力成熟。
- 更适合复杂桌面 UI 和高质量绘制。

Skia 的代价：

- 依赖重。
- 磁盘体积和构建复杂度高。
- 跨平台编译故障面更大。

## EUI-NEO 当前渲染架构

EUI-NEO 当前分层：

```text
examples / user app
  -> include/eui_neo.h, include/eui/*
  -> core/app
  -> core/window + core/input
  -> core/dsl_runtime.h
  -> core/render public API
  -> core/render/opengl 或 core/render/vulkan
```

窗口后端：

- GLFW
- SDL2

渲染后端：

- OpenGL
- Vulkan

Runtime 负责：

- DSL tree compose/layout。
- 事件派发、focus、IME rect、动画状态。
- dirty rect 和 full redraw 管理。
- 调用后端无关 primitive/text/image facade。
- 通过 `RenderBackend` 下发绘制。

RenderBackend 负责：

- OpenGL context 或 Vulkan instance/surface/device/swapchain。
- frame lifecycle。
- framebuffer/render cache。
- primitive/text/image GPU 资源。
- minimized/zero-size 窗口处理。

这和 Slint 的 backend/renderer 拆分方向接近。EUI-NEO 的窗口 backend 对应 Slint backend 的一部分，EUI-NEO 的 OpenGL/Vulkan RenderBackend 对应 Slint renderer + surface/frame lifecycle 的组合。

## 渲染数据流对比

| 阶段 | Slint | EUI-NEO |
| --- | --- | --- |
| UI 描述 | `.slint` 编译或解释成组件定义。 | C++ DSL 在 `compose()` 中构建 `core::dsl::Ui` tree。 |
| 状态系统 | 属性、binding、callback、动画由 Slint Runtime 管理。 | 业务状态多在 C++ app/examples 中，Runtime 管理交互、动画和 primitive 缓存。 |
| Layout | Slint 组件和布局系统计算 scene。 | `ui_.layout(screen)` 计算 Row/Column/Stack/元素 frame。 |
| 渲染对象 | Slint scene items 交给 renderer。 | Rect/Text/Image/Polygon 通过 primitive facade 同步到后端。 |
| 后端选择 | 运行时可通过 `SLINT_BACKEND` 影响 backend/renderer。 | CMake 选择 GLFW/SDL2 和 OpenGL/Vulkan。 |
| 局部刷新 | Software renderer 明确支持 partial rendering；GPU renderer 依实现而定。 | Runtime 统一 dirty rect，OpenGL/Vulkan 共用 render cache 策略。 |
| 静止帧 | 由 backend/event loop 控制。 | 静止时 wait events；有动画时主动按 refresh rate/fps 节流。 |

EUI-NEO 的一个特点是脏区策略在 Runtime 层，而不是只存在于某个 renderer。这样 OpenGL 和 Vulkan 可以共享同一套“哪些区域需要重新画”的判断。Slint 的 partial rendering 在官方说明中主要强调 software renderer，GPU 路径更多依赖 FemtoVG/Skia 内部实现和 backend 策略。

## 脏区与局部渲染对比

### Slint 的 partial rendering

Slint 官方文档明确列出 Software renderer 支持 partial rendering。这个设计通常适合低功耗设备：当 UI 只有小范围变化时，CPU renderer 可以只更新受影响区域，减少像素填充和内存带宽。

从取舍看，Slint software partial rendering 的优势是：

- 不需要 GPU framebuffer cache。
- 对嵌入式屏幕和 MCU 友好。
- 可以和 line-by-line rendering 结合，降低内存占用。

但它也有边界：

- 功能覆盖不如 GPU renderer。
- transform、shadow、复杂 clip、复杂文本能力受限。
- 大屏桌面复杂 UI 下 CPU fill 仍可能成为瓶颈。

### EUI-NEO 的 dirty rect + render cache

EUI-NEO 当前使用离屏 framebuffer cache：

```text
Runtime 计算 dirty rect
  -> beginRenderCacheFrame()
  -> full redraw 或 scissor 局部 redraw
  -> endRenderCacheFrame()
  -> blitRenderCache()
```

局部 dirty 时：

1. 对每个 dirty rect 设置 scissor。
2. 清理 dirty rect 内背景。
3. 只重画与 dirty rect 相交的元素。
4. 关闭 scissor。
5. 把 cache blit 到默认 framebuffer。

已考虑的 dirty 来源：

- frame 变化。
- color、opacity、radius、border、shadow、blur 变化。
- 元素 transform 和父容器继承 transform。
- Text 的 frame、color、opacity、transform。
- visualStateFrom 带来的 hover/press 缩放。
- 元素结构变化时升级 full redraw。

EUI-NEO 方案的优势：

- 对 OpenGL/Vulkan 共用，后端一致性强。
- 静止 UI 可以避免全量重画复杂树。
- 动画局部变化时减少 draw 和 fill。
- 对 C++ DSL 透明，组件不用关心脏区。

EUI-NEO 方案的代价：

- 需要维护 framebuffer cache。
- 局部重画仍要遍历并判断元素与 dirty rect 相交。
- 多个 dirty rect 会重复绘制重叠元素。
- blur、clip、transform、shadow 需要保守扩大 dirty。
- 最后始终需要 blit cache 到默认 framebuffer。

## Backdrop Blur 的关键差异

EUI-NEO 当前支持 backdrop blur。它不是普通 rect，而是依赖“背后已经绘制好的 framebuffer/cache 内容”：

1. 从当前 framebuffer/cache 取 blur rect 附近背景。
2. blit 到 1/2 尺寸 backdrop texture。
3. fragment shader 多次采样。
4. 与自身颜色混合。

这让脏区策略更复杂。局部重画时，如果 blur 背后的 cache 有一部分是旧内容，blur 会把旧内容采进去，出现污染、竖条或错位。因此 EUI-NEO 当前做了保守策略：dirty rect 碰到 blur 保护范围时升级 full redraw。

Slint 的 Software renderer 因为功能限制，不支持 drop shadow，复杂 blur/filter 能力也不是它的强项。Slint 如果走 Skia，则可以借助成熟图形库处理更多滤镜语义，但代价是依赖和体积。EUI-NEO 自研 blur 的好处是可控，坏处是需要自己承担 cache 正确性。

优化建议：

- 短期保留 blur 触发 full redraw 的正确性策略。
- 增加 blur redraw reason 统计，确认 full redraw 是否主要由 blur 引起。
- 给 blur 元素可选 `staticBackdrop` 或 layer cache，用于背景静态的卡片/面板。
- 对 blur 保护范围从固定 1.2 倍升级为和 blur radius、downsample scale 相关的精确膨胀。
- 中长期引入 layer tree，把 blur 背景和前景拆层，减少整屏重画。

## 文本渲染对比

Slint renderer 选择会影响文本质量。官方文档提到 FemtoVG 文本质量有时不够理想，Software renderer 文本渲染目前有限制；Skia 则通常在复杂文本和跨平台质量上更稳。

EUI-NEO 当前文本路径：

- FreeType raster glyph。
- 启用 HarfBuzz 时做 shaping。
- 支持 fallback 和 emoji 字体选择。
- `TextPrimitive::measureTextMetrics()` 提供 caret stops 和 byte indices。
- OpenGL/Vulkan 各自上传 text atlas、descriptor/pipeline、vertex buffer。

EUI-NEO 的优势：

- 文本测量和输入框 caret 使用同一套 metrics。
- 可针对项目需求定制 fallback、emoji 缩放、atlas 策略。
- OpenGL/Vulkan 共享 CPU shaping 和 metrics。

EUI-NEO 的优化点：

- 区分 text 内容变化、颜色变化、frame 变化、transform 变化。
- 内容不变但颜色/opacity 变化时，不应重新 shape 或更新 atlas。
- 多 Text 共用相同字体和 glyph 时，atlas upload 应尽量增量化。
- 多行文本需要缓存 line layout，避免输入框和列表频繁测量。
- 为长文本/日志/聊天场景准备虚拟化和可见行 shaping。

和 Slint 的启发关系：

- Slint 用 renderer 生态换文本复杂度，Skia 这类成熟库能减少自研压力。
- EUI-NEO 选择自研文本，就应该把 metrics、atlas、layout cache 做成核心优化资产。

## 图片与资源缓存对比

Slint 的资源管线由自身编译器/runtime 和 renderer 配合处理，具体图像上传和缓存取决于 renderer。

EUI-NEO 当前图片路径更显式：

- `image_source.*` 负责路径解析、remote/Bing、SVG、静态图/GIF CPU 解码。
- `image.cpp` 做 ImagePrimitive 状态、fit/cover、transform、GIF 帧推进、静态图 texture cache。
- `opengl_image.cpp` / `vulkan_image.cpp` 各自处理 texture upload、descriptor/pipeline、vertex buffer 和 draw。

优化建议：

- 对静态图片做跨窗口或跨 backend 生命周期策略，避免频繁销毁重建。
- GIF/动态图只把变化帧区域标 dirty，不把整个页面标 dirty。
- SVG 栅格化按目标尺寸和 DPI 建 cache key。
- remote image ready 目前会触发 full redraw，可以进一步只标记对应 Image 元素 visual rect。
- 图片 scale/cover 改变时尽量只更新 vertex/uv，不重新上传纹理。

## Primitive 与绘制批处理对比

Slint 的 GPU renderer 依赖 FemtoVG/Skia，这类库内部会做路径、文字、纹理和 batch 管理。EUI-NEO 自研 OpenGL/Vulkan primitive，因此需要显式考虑批处理。

EUI-NEO 当前 primitive 包括：

- Rect：颜色、渐变、圆角、边框、阴影、blur、transform。
- Text：glyph quad。
- Image：纹理 quad。
- Polygon：图表、tooltip 指针等。

优化方向：

- 对大量同类 Rect 做 instance/batch，减少 draw calls。
- 对纯色 rect、border、shadow 分离 pipeline 或 uniform path，避免过度通用 shader。
- 对图表 Polygon 和数据点做动态 vertex buffer ring。
- 对 zIndex 和 clip 做排序优化，但要保持 topmost hit-test 与绘制一致。
- 对 transform 树缓存 world matrix，避免每帧重复递归计算。
- 对 shadow 可以预生成九宫格/模糊贴图，减少实时 shader 或几何成本。

## 帧调度与功耗对比

Slint backend 负责事件循环和 renderer 驱动。具体帧调度随 backend/renderer 而异。

EUI-NEO 当前主循环特点：

- 静止时 wait events。
- 有动画时按显示器 refresh rate 计算下一帧。
- app 可配置 fps，上限取 `min(refreshRate, fps)`。
- Windows 下提高 timer resolution。
- GLFW/SDL2 当前关闭 swap interval，使用主动节流控制帧率。

这个策略的优点：

- 静止功耗低。
- 动画节奏统一，不强依赖 present 阻塞。
- 高刷新率显示器下行为可控。

风险和优化点：

- 关闭 vsync 后，如果主动节流不准，可能有 tearing 或 frame pacing 波动。
- 多显示器刷新率变化时需要及时更新 interval。
- Vulkan/OpenGL present 行为不同，应该记录实际 frame time、present time、sleep time。
- 对无动画但有异步图片/网络完成的帧，应只唤醒一次并标记对应 dirty。

建议增加：

- frame pacing overlay。
- `needsRender` reason 统计。
- 每帧 dirty rect 数量、面积占比。
- full redraw reason 分类。
- backend present latency 采样。

## 渲染优化对照表

| 优化主题 | Slint 思路 | EUI-NEO 当前状态 | EUI-NEO 建议 |
| --- | --- | --- | --- |
| Renderer 选择 | 多 renderer 面向不同平台。 | OpenGL/Vulkan 自研双后端。 | 保留双后端，同时评估 software/lite renderer 用于低端设备。 |
| 局部渲染 | Software renderer 支持 partial rendering。 | Runtime dirty rect + framebuffer cache。 | 做 dirty overlay、面积阈值、rect 合并策略。 |
| 全量重画 | 由 renderer/backend 策略决定。 | 首帧、结构变化、尺寸变化、blur 保护区等触发 full redraw。 | 记录 full redraw reason，减少不必要升级。 |
| 文本 | Skia 强，FemtoVG/Software 有取舍。 | FreeType + HarfBuzz + 自研 atlas。 | 强化 shaping cache、line cache、atlas 增量更新。 |
| 图片 | renderer/runtime 管理。 | image_source + ImagePrimitive + 后端 texture。 | remote ready 改为元素级 dirty，SVG/GIF 分层 cache。 |
| Blur/filter | Skia 能力成熟，software 路径有限。 | 自研 backdrop blur，局部重画保守升级。 | 精确 blur 保护范围，探索 layer cache。 |
| Batch | 由 Skia/FemtoVG 内部处理。 | 自研 draw path，需要显式优化。 | Rect/Text/Image batch、instance、pipeline state cache。 |
| Clip | renderer 内部处理。 | clip 通过 scissor 合成。 | 多层 clip 合并，减少 scissor 切换。 |
| Transform | renderer 内部处理。 | Runtime 继承 transform，dirty 外接矩形。 | 缓存 world matrix，避免重复计算和过度扩大 dirty。 |
| 功耗 | backend/event loop 控制。 | wait events + 主动节流。 | 加 frame pacing 和 idle wakeup 统计。 |

## EUI-NEO 的短期优化清单

### 1. Dirty Debug Overlay

新增开发开关：

```text
EUI_DEBUG_DIRTY_RECTS=1
EUI_DEBUG_REDRAW_REASON=1
```

显示：

- dirty rect 边框。
- 本帧 dirty 面积 / framebuffer 面积。
- full redraw reason。
- render cache 是否重建。
- blur 是否触发升级。

收益：让脏区优化从“感觉快”变成可观测。

### 2. Dirty Rect 合并策略

当前多个 dirty rect 可能导致重复绘制。建议：

- 小 rect 数量超过阈值时合并。
- dirty 总面积超过屏幕一定比例时 full redraw。
- rect 之间距离很近时合并。
- 保留调试统计，比较合并前后 draw count 和 fill area。

### 3. 元素级 Redraw Reason

给 Rect/Text/Image/Polygon 的状态更新记录原因：

- geometry。
- paint。
- opacity。
- transform。
- text content。
- image frame。
- hover/press state。
- async resource ready。

收益：快速定位“为什么这个静态页面还在重画”。

### 4. Text Cache 分层

建议分成：

- shaping cache：文本、字体、字号、wrap width、lineHeight。
- glyph atlas cache：字体 face + glyph id + size。
- vertex cache：frame、align、transform 变化时可重算 vertex，但不重 shape。

### 5. Image Dirty 精细化

当前 `ImagePrimitive::consumeRemoteImageReady()` 会让 Runtime full redraw。建议改为：

- ready 事件携带 image key 或 primitive id。
- Runtime 找到使用该 image 的元素 visual rect。
- 只标对应区域 dirty。
- 若图片参与 blur 背景或被 transform 大幅影响，再升级。

### 6. Blur Layer 实验

为 blur-heavy 页面增加实验路径：

- 静态背景 layer。
- blur layer。
- 前景交互 layer。

当按钮 hover 只影响前景时，不必重建 blur 背景。这个方向接近 retained layer tree，适合长期推进。

## 中长期架构建议

### Runtime 保持脏区决策中心

EUI-NEO 不应把脏区判断下沉到 OpenGL/Vulkan 各自实现里。Runtime 拥有 DSL tree、layout、transform、zIndex、clip、blur 语义，最适合判断“什么变了”和“影响哪里”。后端只负责高效执行 dirty redraw。

### 从 Framebuffer Cache 演进到 Layer Cache

当前单 cache 简单可靠，但遇到 blur、固定背景、复杂 transform 时容易 full redraw。中长期可以引入 layer：

```text
root cache
  static background layer
  scroll content layer
  blur/backdrop layer
  overlay layer
```

收益：

- 固定背景不随前景 hover 重画。
- 滚动区域可独立 cache。
- blur 依赖关系更清晰。
- modal/tooltip/toast 可局部合成。

代价：

- layer invalidation 复杂。
- 内存占用上升。
- OpenGL/Vulkan 都要支持多 render target / image。

### Renderer Profile 分级

借鉴 Slint 多 renderer 取舍，EUI-NEO 可以定义渲染能力档：

- `full`：OpenGL/Vulkan，支持 blur、shadow、transform、复杂文本。
- `lite`：OpenGL/Vulkan，但关闭 blur 或降级 shadow。
- `software`：未来可选，面向无 GPU 或测试环境。

这样用户不用在所有设备上承担最高视觉特性的成本。

## 总体判断

Slint 的优势在于成熟 renderer 生态和清晰 backend/renderer 抽象；EUI-NEO 的优势在于 Runtime 层掌握 DSL、dirty rect、animation 和 render dispatch，因此可以做跨 OpenGL/Vulkan 的统一脏区策略。

EUI-NEO 当前不需要追求“像 Slint 一样有很多 renderer”，更应该先把已有自研路径做到可观测、可量化、可调优：

- dirty rect 可视化。
- full redraw reason。
- text/image cache 分层。
- blur layer 化。
- draw call / fill area / upload bytes 指标化。

当这些指标稳定后，再考虑是否引入 software/lite renderer。否则过早增加 renderer 数量，会把优化问题从一个后端扩散到多个后端。

## 参考资料

- Slint Backends & Renderers：<https://docs.slint.dev/latest/docs/slint/guide/backends-and-renderers/backends_and_renderers/>
- Slint Winit Backend：<https://docs.slint.dev/latest/docs/slint/guide/backends-and-renderers/backend_winit/>
- EUI-NEO 渲染后端架构与流程：`docs/渲染后端架构.md`
- EUI-NEO DSL 文档：`docs/DSL.md`
