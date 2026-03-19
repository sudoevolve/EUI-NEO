# Include 清理记录

最后更新：2026-03-19

## 目标

- 旧布局语法彻底移除，不再作为公开 API 保留。
- `eui::quick` 成为唯一推荐且唯一持续演进的 UI 语法层。
- `include/` 里的重复能力收敛到单一路径。
- 对外暴露但未真正实现的能力，要么补齐，要么降级为内部能力。

## 当前结论

- 旧的公开兼容头基本已经移除：
  - `eui/layout.h`
  - `eui/compat.h`
  - 旧 panel/card/row/columns/waterfall 兼容入口
- `quick` 和 `Context` 之间已经加了内部 access shim。
- [`include/eui/quick/ui.h`](../include/eui/quick/ui.h) 不再直接耦合旧 public 控件/布局方法。
- 当前仓库里这批旧控件/旧布局调用，已经只剩 `ContextAccess` 这一处内部桥接点。
- `quick` 已补齐这轮迁移需要的 builder：
  - `tab`
  - `slider`
  - `input_float`
  - `text_area`
  - `dropdown`
  - `scroll_area`
- `examples/` 里原先的旧控件/旧布局直调已经迁走，当前示例层只保留 quick 写法和 `Context` 的基础配置调用。
- [`include/EUI.h`](../include/EUI.h) 里的旧控件/旧布局接口已从 public 收回 private/internal。
- 当前剩下的主问题已经不是“旧语法还暴露着”，而是：
  - `include/` 里仍有重复能力
  - 部分 public 能力仍未真正实现或未接线

## 分步修复清单

### Step 1: 给 quick 建立内部访问层

状态：已完成

目的：

- 先让 `quick` 不再直接依赖 `Context` 的 public 旧接口。
- 为后面把旧接口从 public 收回 private/internal 做准备。

本步范围：

- 在 `Context` 和 `quick` 之间增加内部 access shim。
- `quick/ui.h` 后续只通过 access shim 访问旧控件/布局实现。

结果：

- `quick/ui.h` 已经不再直接粘着 `Context` 的旧 public 控件/布局方法。
- 后续即使把这些旧方法从 public 收回，也只需要调整 access shim，不需要在 quick 各处散改。

### Step 2: 补齐 quick 缺失 builder

状态：已完成

缺口主要在：

- `tab`
- `slider_float`
- `input_float`
- `text_area`
- `dropdown`
- `scroll_area`

结果：

- quick 侧已经补出一版可用 builder / scope 封装。
- 迁移示例时不再需要回退到旧 `Context` 控件接口。

### Step 3: 迁移 examples 到 quick-only

状态：已完成

重点文件：

- [`examples/basic_widgets_demo.cpp`](../examples/basic_widgets_demo.cpp)
- [`examples/graphics_showcase_demo.cpp`](../examples/graphics_showcase_demo.cpp)
- 其余仍直接调用 `ctx.*` 的示例

要求：

- 示例不再直接调用旧 `Context` 控件/布局 API。
- 示例只保留 quick builder / quick scope 写法。

结果：

- [`examples/basic_widgets_demo.cpp`](../examples/basic_widgets_demo.cpp) 已切到 quick builder。
- [`examples/graphics_showcase_demo.cpp`](../examples/graphics_showcase_demo.cpp) 已切到 quick builder。
- 当前扫描结果里，`examples/` 已没有旧控件/旧布局直调残留。

### Step 4: 收缩 `Context` public 面

状态：已完成

目标：

- 把旧控件、旧布局方法从 `Context` public 移出。
- 仅保留底层框架能力、主题/帧生命周期、绘制底座和必要运行时接口。

结果：

- 旧布局方法已转入 private/internal：
  - `push_layout_rect/pop_layout_rect`
  - `begin_stack/end_stack`
  - `dock_left/right/top/bottom`
  - `split_h/split_v`
  - `begin_flex_row/end_flex_row`
- 旧控件方法已转入 private/internal：
  - `label`
  - `spacer`
  - `button`
  - `tab`
  - `slider_float`
  - `input_float`
  - `input_text`
  - `input_readonly`
  - `begin_scroll_area/end_scroll_area`
  - `text_area`
  - `text_area_readonly`
  - `progress`
  - `begin_dropdown/end_dropdown`
- 同时这些 private/internal 实现名也已经统一改成 `internal_*`，避免仓库内部继续沿用旧语法命名。
- `scroll_area` 的 options 已经从 `Context` 视角解耦，quick 侧现在使用独立的 `eui::quick::ScrollAreaOptions`。

### Step 5: 清理 include 重复能力

状态：进行中

目标：

- 决定哪些 abstraction 真正保留为公共接口。
- 其余重复/未接线部分要么接通，要么隐藏。

当前已完成：

- `Color/Rect` 与 `graphics::Color/Rect` 的转换 helper 已统一收敛到 [`include/EUI.h`](../include/EUI.h)。
- [`include/eui/quick/ui.h`](../include/eui/quick/ui.h) 不再重复维护 `to_graphics_*`。
- [`include/eui/quick/detail/primitive_painter.h`](../include/eui/quick/detail/primitive_painter.h) 不再重复维护 `to_legacy_*`。
- `app::FrameContext` 已收敛为 [`include/eui/runtime/frame_context.h`](../include/eui/runtime/frame_context.h) 的别名，不再保留独立重复结构。
- GLFW app run loop 现在直接构造 `runtime::FrameContext`，并补齐了 `frame_index / now_seconds / delta_seconds`。
- 2D/3D 矩形变换 helper 已统一收敛到 [`include/EUI.h`](../include/EUI.h)，`primitive_painter` 不再重复维护同一套 rect transform 逻辑。
- `paint_image()` 已开始真正消费 `ImageFit`，占位渲染现在会按 `fill / contain / cover / stretch / center` 计算内容区域。
- [`include/EUI.h`](../include/EUI.h) 顶部的基础模型层已外提到 [`include/eui/core/foundation.h`](../include/eui/core/foundation.h)。
- `Context` 内部 state 数据层已外提到 [`include/eui/core/context_state.h`](../include/eui/core/context_state.h)。
- `Context` 内部 hash / rect / alpha 小工具已外提到 [`include/eui/core/context_utils.h`](../include/eui/core/context_utils.h)。
- `Context` 文本测量配置、缓存状态、UTF-8 解码与字符宽度工具已外提到 [`include/eui/core/context_text_measure.h`](../include/eui/core/context_text_measure.h)。
- `EUI.h` 主头文件物理行数已从 4640 降到 3202。

下一批继续做：

- 收敛 3D 投影 bounds 与 painter fallback 两条路径的职责边界。
- 处理剩余未接线 public 能力的降级或补齐，优先是 `IconPrimitive::font_family`。
- 继续拆分 `Context` 内部输入编辑 / 文本域逻辑，降低 `EUI.h` 主文件物理体积。

## 旧公开语法残留点

这些接口原先挂在 [`include/EUI.h`](../include/EUI.h) public 面上，现在已经转入 internal：

- 布局：
  - `dock_left/right/top/bottom`
  - `split_h/split_v`
  - `begin_flex_row/end_flex_row`
- 控件：
  - `button`
  - `tab`
  - `slider_float`
  - `input_float`
  - `input_text`
  - `input_readonly`
  - `begin_scroll_area/end_scroll_area`
  - `text_area`
  - `text_area_readonly`
  - `progress`
  - `begin_dropdown/end_dropdown`

当前这部分已经不再构成公开语法面。

## include 里的重复能力

### 1. 重复的公开 UI 表面

- 一套是 `eui::Context` 直调式控件/布局 API。
- 一套是 `eui::quick::UI` builder API。

问题：

- 用户会看到两套写法并存。
- 文档、示例、维护成本都会持续分裂。

### 2. 重复的 frame context 模型

- [`include/eui/app/frame_context.h`](../include/eui/app/frame_context.h)
- [`include/eui/runtime/frame_context.h`](../include/eui/runtime/frame_context.h)

当前状态：

- 已收敛。
- `eui::app::FrameContext` 现在直接别名到 `eui::runtime::FrameContext`。
- 示例与 run loop 已切到统一模型。

### 3. 重复的几何/颜色模型与桥接

- `eui::Color` / `eui::Rect` 在 [`include/EUI.h`](../include/EUI.h)
- `eui::graphics::Color` / `eui::graphics::Rect` 在 `include/eui/graphics/*`
- 转换 helper 现已统一收敛到 [`include/EUI.h`](../include/EUI.h)

问题：

- 数据模型双轨并存。
- 后续仍需要决定 legacy `Color/Rect` 是继续保留，还是继续下沉成更明确的兼容层。

### 4. 3D 投影/变换逻辑重复

- [`include/EUI.h`](../include/EUI.h) 里有：
  - `transform_3d_is_identity`
  - `project_rect_point_3d`
  - `projected_rect_bounds`
- [`include/eui/quick/detail/primitive_painter.h`](../include/eui/quick/detail/primitive_painter.h) 里又有：
  - `apply_transform_3d_fallback`
  - 基于 projected bounds 的二次处理

问题：

- `Rect` 级别的 2D/3D transform helper 已经先收敛到 `EUI.h`。
- 但目前仍存在“bounds 投影”和“绘制 fallback”两条并行语义路径。
- 后续需要继续明确：哪些 primitive 应走真实投影边界，哪些只能走近似 fallback。

## 对外暴露但未完成/未接线的能力

### 1. `image()` 还是占位实现

- quick 已经公开 `image()` builder。
- 当前 [`include/eui/quick/detail/primitive_painter.h`](../include/eui/quick/detail/primitive_painter.h) 已改成带内容区、裁切和 fit 语义的占位渲染。

结论：

- 仍然是占位，不是正式图片解码 / 纹理上传 / 图片缓存能力。

### 2. `ImageFit` 已暴露，并已接到占位渲染

- `ImagePrimitive::fit` 在 graphics public model 里已经有。
- `paint_image()` 现在已经会读取并应用它。
- 但这仍然只是占位内容的 fit，不是真实图片内容的 fit。

### 3. `IconPrimitive::font_family` 已暴露，但渲染未使用

- glyph builder 会写入 `font_family`。
- 当前 icon 渲染并没有消化这个字段。

### 4. 动画 origin 字段已暴露，但插值未使用

- `TransformOrigin2D`
- `TransformOrigin3D`
- `animate_transform()`
- `interpolate_transform()`

问题：

- public 数据结构已经有 origin。
- 实际插值路径没有完整消费这些 origin 语义。

### 5. `AnimatorState` 暴露但价值不清晰

- 目前看更像中间态结构，不像稳定公共能力。

### 6. runtime / renderer abstraction 已暴露，但主路径没真正接通

- [`include/eui/runtime/contracts.h`](../include/eui/runtime/contracts.h)
- [`include/eui/renderer/contracts.h`](../include/eui/renderer/contracts.h)
- [`include/eui/renderer/draw_data_adapter.h`](../include/eui/renderer/draw_data_adapter.h)

问题：

- 对外已经像正式分层。
- 但主运行路径仍 heavily 依赖当前 app / GLFW / OpenGL 路径。

## 接下来按这个顺序修

1. 先做 quick 内部 access shim。
2. 再补 quick 缺失 builder。
3. 然后迁移所有示例到 quick-only。
4. 最后收掉 `Context` 的旧公开入口。

## 本次变更记录

- 2026-03-19：新建本记录文件。
- 2026-03-19：已建立 `quick -> ContextAccess -> Context` 的内部访问层，完成 Step 1。
- 2026-03-19：已补齐 quick 缺失 builder，完成 Step 2。
- 2026-03-19：已迁移 `basic_widgets_demo` 与 `graphics_showcase_demo`，完成 Step 3。
- 2026-03-19：已将旧控件/旧布局方法从 `Context` public 收回 private/internal，完成 Step 4。
- 2026-03-19：已把 `Context` 私有层旧式命名统一改成 `internal_*`，并把 `scroll_area` options 从 quick 侧解耦。
- 2026-03-19：已把 `Color/Rect` 与 graphics 层的转换 helper 统一收敛到 `include/EUI.h`，去掉 quick 侧重复实现。
- 2026-03-19：已把 `app::FrameContext` 收敛为 `runtime::FrameContext` 单一模型，并迁移仓库示例与文档用法。
- 2026-03-19：已把 2D/3D 矩形变换 helper 收敛到 `include/EUI.h`，去掉 `primitive_painter` 内部重复实现。
- 2026-03-19：已把 `ImageFit` 接到 `paint_image()` 占位渲染，图片占位内容现在会按 fit 模式计算内容区域和裁切。
- 2026-03-19：已把 `Color / Rect / Theme / InputState / DrawCommand` 等基础模型层从 `EUI.h` 顶部外提到 `include/eui/core/foundation.h`。
- 2026-03-19：已把 `Context` 内部 state 数据层从 `EUI.h` 外提到 `include/eui/core/context_state.h`。
- 2026-03-19：已把 `Context` 内部 hash / rect / alpha 小工具从 `EUI.h` 外提到 `include/eui/core/context_utils.h`。
- 2026-03-19：已把 `Context` 文本测量配置、缓存状态和字符宽度工具从 `EUI.h` 外提到 `include/eui/core/context_text_measure.h`，并把 `EUI.h` 进一步降到 3202 行。
