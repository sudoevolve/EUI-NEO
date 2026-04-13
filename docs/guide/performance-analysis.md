# EUI-NEO 交互期 CPU/GPU 性能分析（阶段性）

> 目标边界：本分析只讨论底层渲染链路优化，不通过降低帧率、减少动画内容、删减视觉效果来“换性能”。
优化应由底层承担，不该让业务页面承担渲染策略。

## 1. 当前现象

- 交互期间（滚动、侧栏切换、局部动画）CPU/GPU 占用明显升高。
- 已排除“模糊效果本身”作为主因后，问题仍持续出现。
- 占用抬升与“刷新范围过大”“合成频率偏高”更相关，而不是单一 shader 或单组件开销。

## 2. 根因结论（先说明原因）

## 2.1 重绘调度偏全局

- 多处节点调用 `requestVisualRepaint(...)` 后，会落到全局 `Renderer::RequestRepaint(...)`。
- 当前路径更接近“有改动 -> 全局 repaint 标记”，而不是“仅脏子树/脏层重绘”。
- 结果是：局部状态变化容易放大成整帧刷新，CPU 侧更新和 GPU 侧提交都被连带拉高。

关键链路（按调用语义）：

1. `UINode::requestVisualRepaint(...)`
2. `Renderer::RequestRepaint(...)`
3. 主循环执行整帧更新与绘制

## 2.2 UIContext 更新遍历范围较大

- `UIContext::update()` 现阶段仍以全局节点更新为主路径。
- 即使 draw order 维护中做了 stamp 判定，实际交互时仍会触发大量节点参与更新流程。
- 当交互频繁发生时，CPU 消耗呈线性放大。

## 2.3 Compose 阶段在状态变化时可能重复执行

- `MainPage::Update()` 中存在“先 Compose + update，再按状态决定再次 Compose”的模式。
- 在状态变化密集的交互场景下，Compose 成本会被放大，CPU 峰值更容易抬升。
- 该问题与动画内容无关，属于调度路径放大。

## 2.4 缓存面与离屏策略的 GPU 成本偏高

- 渲染器存在缓存面 supersample 策略（当前为 2.0 级别）。
- 这会提高缓存清晰度，但显著增加离屏纹理尺寸、显存带宽与合成负担。
- 当无效重绘比例偏高时，GPU 负担会被进一步放大。

## 2.5 Layer 失效到“局部刷新”尚未完全闭环

- 现有架构已有 layer 与 invalidation 基础能力。
- 但从触发、传播到执行，仍存在“脏信息回落到全局刷新”的路径。
- 也就是：具备局部刷新的基础设施，但尚未在主链路上彻底收敛到局部更新。

## 3. 证据点（代码位置）

- `src/ui/UINode.h`
  - `requestVisualRepaint(...)` 会设置缓存脏并请求渲染器重绘。
- `src/EUINEO.cpp`
  - `Renderer::RequestRepaint(...)` 走全局 repaint 调度语义。
  - 缓存 supersample 比例在此定义并影响缓存面开销。
- `src/ui/UIContext.cpp`
  - `UIContext::update()` 中存在面向全局节点的更新流程。
  - draw order 受 compose stamp 驱动，在交互中会频繁参与维护。
- `src/pages/MainPage.h`
  - `Update()` 流程在特定条件下会触发二次 Compose。

## 4. 为什么这会同时拉高 CPU 和 GPU

- CPU 侧：重建/更新范围扩大（Compose、节点更新、排序/状态维护）导致每帧工作量上升。
- GPU 侧：离屏缓存与整帧提交增多，纹理与带宽压力叠加，交互期峰值更明显。
- 两者是同一调度问题在不同阶段的表现，不是孤立瓶颈。

## 5. 改造优先级（仅策略，不改画面内容）

1. 先完善可观测指标：区分“请求重绘次数、实际重绘区域、层命中率、Compose 次数/帧”。
2. 收敛重绘触发语义：把 `requestVisualRepaint` 从“默认全局”逐步改为“优先脏层/脏区”。
3. 把 `UIContext::update()` 切为增量路径：优先更新脏子树，避免全量遍历常态化。
4. 降低重复 Compose 触发概率：确保一次交互仅在必要时发生一次有效重建。
5. 复核缓存 supersample 的启用条件：按内容类型、缩放、运动状态做分级，而非固定全量高倍率。

## 6. 阶段性结论

当前高占用的核心并非“某个视觉效果太重”，而是**交互触发后刷新范围和更新频度过大**。  
因此应优先改“调度与失效传播”，再做绘制细节优化。这样能在不牺牲帧率与动画体验的前提下，持续降低 CPU/GPU 占用。

## 7. 改造进度（进行中）

### 7.1 已实施（第一批）

- 已对 `UIContext` 引入 draw order 签名缓存：
  - 新增签名计算与 `ensureDrawOrder()`；
  - `update()/draw()` 统一走同一套排序保障逻辑；
  - 仅在 draw order 签名变化时触发 `stable_sort`。
- `begin()` 阶段不再无条件清空 draw order 缓存；仅在节点 GC 删除时重置缓存。
- 已将节点/图元 repaint 路径收敛为“优先 layer 失效”：
  - `UINode::requestVisualRepaint(...)` 改为优先 `Renderer::InvalidateLayer(primitive_.renderLayer)`；
  - `RequestPrimitiveRepaint(...)` 改为优先按 `primitive.renderLayer` 失效；
  - 仅在 `duration > 0` 时追加 `Renderer::RequestRepaint(duration)` 以维持动画时长。
- 已在 `UIContext::update()` 增加更新门控：
  - 仅当“本帧有输入活动”或“节点持续更新”或“节点当前为脏”时才执行 `node->update()`；
  - 对不可见节点仍保持清脏逻辑，避免缓存状态积累。
- 已在 `UIContext::update()` 增加“鼠标移动热点更新”底层路径：
  - 当输入仅为 pointer move 时，不再对全部可见节点执行 `update()`；
  - 仅更新“当前命中节点 + 上一帧命中节点 + 持续动画节点 + 脏节点”；
  - 该机制对业务层透明，普通 hover 组件无需额外手写性能逻辑。
- 已在 `Renderer::DrawCachedSurface(...)` 增加按面积分级 supersample：
  - 小面积缓存继续保持高质量 supersample；
  - 中/大面积缓存自动降档，降低 FBO 重建和显存带宽峰值；
  - 该机制完全位于底层渲染器，对业务组件透明。
- 已清理业务侧直接全局 `Renderer::RequestRepaint(...)` 调用（`ImageNode`）：
  - 远程图片加载等待阶段改为 `requestVisualRepaint(...)`；
  - repaint 语义统一为“优先 layer 失效 + 按需时长刷新”，避免无差别全局请求。
- 已收紧 app/runtime 输入回调的 repaint 触发条件：
  - 鼠标事件仅在左/右键状态实际变化时请求 repaint；
  - 滚轮偏移为 0 时不再触发 repaint；
  - 降低入口层无效全局刷新请求的频次。

预期收益：

- 降低交互期每帧排序开销（CPU）；
- 降低无输入帧中的节点更新遍历成本（CPU）；
- 降低 hover 等 pointer move 场景下的全树 `update()` 成本（CPU）；
- 降低偶发缓存重建带来的 GPU 峰值（内存/带宽）；
- 为后续“按脏节点/脏层更新”改造提供稳定基础。

### 7.2 下一批计划

1. 继续清理 app/demo 入口剩余直接全局 `Renderer::RequestRepaint(...)` 调用，逐步迁移到 layer/区域语义。
2. 评估按节点脏集驱动 update 的可行性，继续缩小遍历范围。
3. 细化缓存 supersample 阈值（结合 DPI/平台）并补充可观测性指标。
