# EUI-NEO 渲染策略改造方案（对齐 Flutter）

## 1. 目标

当前 EUI-NEO 在交互时 CPU/GPU 占用偏高，目标是：

- 保留现有 DSL 与组件 API，不破坏业务侧调用方式。
- 明显降低交互时帧开销（尤其是 hover/动画/弹窗期间）。
- 将渲染调度从“全局触发”升级为“分层、按需、可观测”。
- 逐步对齐 Flutter 的渲染策略（而不是一次性重写）。

---

## 2. 现有渲染流水线（当前实现）

### 2.1 主循环阶段

代码位置：

- `src/app/DslAppRuntime.h`
- `src/ui/UIContext.cpp`
- `src/pages/MainPage.h`

当前每次可见帧大致是：

1. `ui.begin(pageId)`
2. 执行页面 compose（构建/复用节点）
3. `ui.end()`（统计脏层并触发重绘请求）
4. `ui.update()`（输入、动画、节点状态推进）
5. 若需要 recompose，再 compose 一次
6. `ui.render()`（分层合成 + 节点 draw）

### 2.2 现有优化点（已有）

- 节点复用：`UIContext::acquire` 按 key 复用节点。
- 脏层失效：`UIContext::end/update` 里按 layer 标记 invalidate。
- 绘制排序：`RenderLayer + zIndex` 排序绘制。
- 部分缓存：`usesCachedSurface()` 为 true 的节点走缓存表面绘制。

### 2.3 当前瓶颈

1. **调度粒度偏粗**
   - `RequestRepaint(duration)` 是全局层面的重绘窗口，容易把“局部动画”放大成“全局高频刷新”。
2. **更新与绘制耦合重**
   - 节点 `wantsContinuousUpdate()` 会推动整套 UIContext 进入持续更新，而非仅针对受影响层/区域。
3. **Popup 类组件成本偏高**
   - 弹窗/Toast/Tooltip 属于局部变化，但常带动背景层重复合成。
4. **缺少系统化性能观测**
   - 当前缺帧阶段耗时拆分（build/layout/update/raster）和分层 GPU 占比指标，难以快速定位回归。

---

## 3. 与 Flutter 的关键差异

## 3.1 Flutter 的核心思路（简化）

- 声明式树：Widget -> Element -> RenderObject（保留树）。
- 明确流水线：build -> layout -> paint -> compositing -> raster。
- 帧调度统一：`SchedulerBinding` + vsync。
- 图层保留：Layer Tree + Raster Cache，尽量避免无效重绘。

## 3.2 EUI-NEO 当前差异点

- 你目前是“即时 compose + 节点保留”的混合模式。
- recompose 和 update 触发链仍偏全局，局部动画常扩大为全局刷新。
- layer invalidation 已有基础，但“只更新受影响层/区域”还不彻底。

---

## 4. 改造总原则

1. **先可观测，再大改**
   - 没有可量化指标的优化基本不可控。
2. **先调度，后渲染细节**
   - 先把“何时刷新、刷新多少”做对，再优化 shader/绘制细节。
3. **优先保证交互稳定**
   - 不牺牲输入一致性、弹窗阻断语义、动画正确性。
4. **分阶段可回滚**
   - 每一步必须可开关、可回退、可 A/B 对比。

---

## 5. 分阶段实施计划

## Phase A：建立性能观测（1-2 天）

目标：拿到可行动数据。

实施：

- 在主循环埋点输出以下耗时（毫秒）：
  - compose
  - end(invalidate)
  - update
  - render
  - swap
- 增加每帧统计：
  - invalidate 的 layer 数量
  - 实际 draw 的 node 数量
  - `wantsContinuousUpdate` 返回 true 的 node 列表（采样）
- 输出滑动窗口统计（P50/P95/max）。

验收：

- 能回答“哪一阶段最贵、哪个组件导致持续更新”。

## Phase B：收紧刷新调度（2-4 天）

目标：交互期间显著降低 CPU/GPU。

实施：

- 规范 `RequestRepaint(duration)` 用法：
  - 组件禁止申请长时间全局刷新窗口。
  - 动画类组件通过 `wantsContinuousUpdate()` 驱动续帧。
- 在 `UIContext` 中增加“按层续帧请求”：
  - 例如 `requestLayerRefresh(RenderLayer layer, duration)`。
  - Popup 动画只续 `Popup` 层，避免拖动 `Backdrop/Content`。
- 清理页面层“临时硬刷逻辑”，统一走调度接口。

验收：

- Toast/Tooltip/Dialog 动画期间，GPU 峰值明显下降。
- 功能行为与现有一致（自动隐藏、输入阻断、层级正确）。

## Phase C：分离 Update 与 Raster 成本（3-6 天）

目标：减少“局部变化触发全局重绘”。

实施：

- 引入更明确的“层脏标记”与“可跳过层合成”路径。
- 对静态背景层（Backdrop）增加更强缓存策略：
  - 无脏则不重画；
  - 仅在主题变化/窗口变化时重建。
- 组件 `paintBounds()` 更严格返回实际脏区，减少过度 invalidate。

验收：

- Hover 高频场景中，非交互层 draw 次数显著下降。

## Phase D：对标 Flutter 的渲染树分层（中期）

目标：建立长期可扩展架构。

实施：

- 逐步引入 RenderObject-like 层（布局对象与绘制对象职责分离）。
- 将 `compose` 与 `layout/paint` 依赖解耦，减少无效 recompose。
- 为复杂组件（ListView/Table）建立独立的局部脏区更新策略。

验收：

- 大列表、复杂页面交互时，帧时间稳定性提升。

---

## 6. 立即可执行的短期清单

1. 给 `UIContext` 和 `DslAppRuntime` 加性能埋点（先不改行为）。
2. 增加 `requestLayerRefresh` 接口（先支持 Popup 层）。
3. 检查所有组件中的 `requestVisualRepaint(duration)` 参数，去掉长窗口滥用。
4. 建立 `perf-baseline.md`，固定 3 个场景做回归：
   - 高频 hover
   - Dialog 打开并操作
   - Table 点击触发 Toast 连续 30 次

---

## 7. 风险与回滚策略

- 风险 1：改调度后动画“断帧/不消失”
  - 对策：给关键组件加状态机日志（show/open/visible/remainingTime）。
- 风险 2：仅刷新 Popup 层后出现残影
  - 对策：保留全局刷新兜底开关（debug flag）。
- 风险 3：输入阻断语义回归
  - 对策：弹窗期间统一校验 `inputBlockedByPopup` 行为。

---

## 8. 验收指标（建议）

- 帧时间：
  - 交互场景 P95 < 16.6ms（60Hz）
- GPU：
  - Toast/Tooltip 动画时 GPU 峰值较现状下降 >= 30%
- CPU：
  - 交互期间平均 CPU 占用下降 >= 20%
- 功能：
  - Toast 自动隐藏、Dialog 阻断输入、Tab/Table/ContextMenu 行为无回归

---

## 9. 结论

EUI-NEO 已有“层 + 节点复用 + 脏标记”的基础，离 Flutter 风格高效渲染并不远。  
最关键不是立即重写，而是按“观测 -> 调度 -> 分层缓存 -> 架构演进”顺序推进。  
这样能在保持现有 DSL 稳定的前提下，持续降低交互时 CPU/GPU 占用。

