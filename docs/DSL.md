# DSL 设计与当前实现

当前推荐写法是声明式 DSL：应用只描述页面结构、样式、交互回调和目标状态，`core::dsl::Runtime` 负责布局、状态缓存、事件、动画、脏区渲染和 OpenGL primitive 同步。

## 核心元素

```cpp
enum class ElementKind {
    Row,
    Column,
    Stack,
    Rect,
    Text,
    Image,
    Polygon
};
```

- `Row`：横向布局容器。
- `Column`：纵向布局容器。
- `Stack`：叠放布局容器。
- `Rect`：基础视觉图元，支持颜色、渐变、圆角、边框、阴影、透明度、blur、transform、hover/pressed 状态。
- `Text`：文本图元，支持字体、字号、颜色、换行、行高、对齐、透明度和 transform。
- `Image`：图片图元，支持本地图片、网络图片、SVG、Bing daily、cover/contain/stretch。
- `Polygon`：多边形图元，支持点集、颜色、透明度、transform 和 hover/pressed 状态；当前图表 tooltip 指针和 piechart 扇区都基于它。

组件不进入 core 枚举。组件层只是组合 DSL 图元，例如 `components::button(ui, id)` 内部使用 `Stack + Rect + Row + Text`。

## App 入口

DSL app 推荐只实现：

```cpp
#include "app/dsl_app.h"

namespace app {

const DslAppConfig& dslAppConfig();
void compose(core::dsl::Ui& ui, const core::dsl::Screen& screen);

} // namespace app
```

`app/dsl_app.h` 已封装：

- initialize
- update
- isAnimating
- render
- shutdown
- app::async 后台任务
- 按屏幕刷新率主动节流
- 无动画时等待事件休眠

`DslAppConfig` 推荐使用链式写法，配置项名会直接写在调用里。它也支持为单个 app 覆盖默认字体：

```cpp
static const DslAppConfig config = DslAppConfig{}
    .title("Clock")
    .pageId("clock")
    .clearColor({0.965f, 0.966f, 0.970f, 1.0f})
    .windowSize(1600, 1080)
    .fps(90.0)
    .textFont("YouSheBiaoTiHei-2.ttf");
```

托盘后台运行默认关闭。需要托盘的页面可以在 `DslAppConfig` 中显式调用 `.tray(true)`，例如串口工具。启用托盘后，关闭或最小化窗口会隐藏到托盘并释放图形资源；托盘 `Show` 会重新显示窗口，`Exit` 才真正退出。

不设置 `.textFont(...)` 时使用 `core/text.cpp` 顶部的全局默认文本字体；不设置 `.iconFont(...)` 时使用全局默认图标字体。

## 布局 DSL

容器：

```cpp
ui.row("toolbar")
ui.column("content")
ui.stack("root")
```

通用布局属性：

```cpp
.x(value)
.y(value)
.position(x, y)
.width(value)
.height(value)
.size(width, height)
.fill()
.wrapContent()
.margin(value)
.margin(horizontal, vertical)
.margin(left, top, right, bottom)
.gap(value)
.spacing(value)
.justifyContent(core::Align::CENTER)
.alignItems(core::Align::CENTER)
.align(core::Align::CENTER, core::Align::CENTER)
.zIndex(value)
.clip()
.overflowHidden()
```

`.zIndex(...)` 只影响同级元素的绘制顺序和 topmost hit-test，不参与布局计算；值越大越靠上。`.clip()` 会按该元素布局矩形裁剪自己和子树，并且命中测试也不会穿出裁剪区域。

`Row / Column` 的主轴 flex 分配会把结果作为子项测量约束，fixed、wrapContent 和 fill 子项都可以参与 grow / shrink。`Stack.wrapContent()` 会把子项正向 x/y 偏移计入包裹尺寸；负向偏移仍视为向外溢出。

## 通用交互 DSL

`Row / Column / Stack / Rect / Text / Image / Polygon` 都支持通用交互方法：

```cpp
.interactive(true)
.disabled(false)
.enabled(true)
.cursor(core::CursorShape::Hand)
.hitTestMode(core::dsl::HitTestMode::Transformed)
.transformedHitTest()
.onClick(callback)
.onPress(callback)
.onRelease(callback)
.onContextMenu(callback)
.onHoverChanged(callback)
.focusable()
.onFocusChanged(callback)
.onTextInput(callback)
.onScroll(callback)
.onDrag(callback)
```

`.onClick(...)` 会自动开启 interactive，并把 cursor 设置为手型。Runtime 会做 topmost hit-test、按下捕获、点击判定和回调派发。默认命中使用布局矩形；需要旋转、缩放或 2.5D 投影后的视觉区域参与命中时，用 `.hitTestMode(core::dsl::HitTestMode::Transformed)` 或 `.transformedHitTest()`。`HitTestMode::Layout` 是默认布局命中，`Transformed` 会按当前投影矩阵反算命中，`None` 表示该元素自身不参与命中。

底层 DSL 的 `onPress/onRelease/onDrag/onScroll` 回调直接使用 Runtime 原始事件。页面或组件需要 tap、拖拽阈值、滚轮步进、局部坐标、进入/离开 hover 时，优先用组件层的 `components::mouseArea(ui, id)`。

示例：

```cpp
ui.text("github.link")
    .size(260.0f, 34.0f)
    .text("GitHub")
    .color(accent)
    .onClick([] {
        core::platform::openUrl("https://github.com/sudoevolve/EUI-NEO");
    })
    .build();
```

## Rect DSL

```cpp
ui.rect("card")
    .size(360.0f, 260.0f)
    .color({0.10f, 0.12f, 0.16f, 1.0f})
    .radius(18.0f)
    .border(1.0f, {0.23f, 0.29f, 0.38f, 1.0f})
    .shadow(26.0f, 0.0f, 8.0f, {0.0f, 0.0f, 0.0f, 0.26f})
    .transition(0.2f, core::Ease::OutCubic)
    .build();
```

Rect 支持：

```cpp
.color(...)
.background(...)
.gradient(...)
.radius(...)
.rounding(...)
.border(...)
.shadow(...)
.blur(...)
.opacity(...)
.translate(...)
.translate3d(...)
.translateX(...)
.translateY(...)
.translateZ(...)
.scale(...)
.rotate(...)
.rotateX(...)
.rotateY(...)
.rotateZ(...)
.rotation(...)
.perspective(...)
.transformOrigin(...)
.states(normal, hover, pressed)
```

`.states(normal, hover, pressed)` 是 Rect 专用的 hover / pressed 视觉状态，会开启交互并由 Runtime 维护 blend。

## Text DSL

```cpp
ui.text("title")
    .size(420.0f, 48.0f)
    .text("EUI Gallery")
    .customFont("YouSheBiaoTiHei")
    .fontSize(38.0f)
    .lineHeight(44.0f)
    .color({0.94f, 0.97f, 1.0f, 1.0f})
    .horizontalAlign(core::HorizontalAlign::Center)
    .verticalAlign(core::VerticalAlign::Center)
    .build();
```

Text 支持：

```cpp
.text(...)
.icon(codepoint)
.fontFamily(...)
.font(...)
.customFont(...)
.fontSize(...)
.fontWeight(...)
.color(...)
.opacity(...)
.translate(...)
.translate3d(...)
.translateX(...)
.translateY(...)
.translateZ(...)
.scale(...)
.rotate(...)
.rotateX(...)
.rotateY(...)
.rotateZ(...)
.rotation(...)
.perspective(...)
.transformOrigin(...)
.maxWidth(...)
.wrap(...)
.horizontalAlign(...)
.verticalAlign(...)
.lineHeight(...)
```

`.icon(...)` 会自动使用图标字体；图标字体默认来自 `core/text.cpp`，也可以通过配置里的 `.iconFont(...)` 按 app 覆盖。

底层文本使用 FreeType 渲染 glyph，启用 HarfBuzz 时会进行复杂文本 shaping。`fontFamily("monospace")` 是跨平台等宽字体别名，`fontFamily("Emoji")` 会选择平台 emoji 字体。需要精确光标位置或命中测试时，使用 `core::TextPrimitive::measureTextMetrics(...)` 获取 shaped caret stops；返回的 `byteIndices` 是 UTF-8 byte offset，`caretX` 是对应的逻辑 x，和实际渲染使用同一套 fallback、emoji 缩放和 glyph advance。

Text 的 transform 作用在生成后的 glyph 顶点上，适合做滚轮、轻量缩放和旋转动效；默认命中测试仍按未 transform 的布局 frame 计算，需要跟随视觉变换时开启 `.transformedHitTest()`。

`ui.label(id)` 是 `ui.text(id)` 的别名。

## Image DSL

```cpp
ui.image("cover")
    .size(320.0f, 180.0f)
    .source("assets/icon.png")
    .cover()
    .radius(16.0f)
    .build();

ui.image("bing")
    .size(420.0f, 220.0f)
    .bingDaily(0, "zh-CN")
    .cover()
    .build();
```

Image 支持：

```cpp
.source(pathOrUrl)
.path(path)
.url(url)
.bingDaily(idx, mkt)
.fit(core::ImageFit::Cover)
.cover()
.contain()
.stretch()
.radius(...)
.opacity(...)
.tint(...)
.color(...)
.translate(...)
.translate3d(...)
.translateX(...)
.translateY(...)
.translateZ(...)
.scale(...)
.rotate(...)
.rotateX(...)
.rotateY(...)
.rotateZ(...)
.perspective(...)
.transformOrigin(...)
```

默认 fit 是 `Cover`，图片会适应裁剪，不会强行压缩变形。

## Polygon DSL

```cpp
ui.polygon("slice")
    .size(120.0f, 120.0f)
    .points({
        {60.0f, 60.0f},
        {120.0f, 0.0f},
        {120.0f, 120.0f},
    })
    .color({0.22f, 0.50f, 0.88f, 1.0f})
    .build();
```

Polygon 支持：

```cpp
.points(...)
.point(x, y)
.clearPoints()
.color(...)
.opacity(...)
.translate(...)
.translate3d(...)
.translateX(...)
.translateY(...)
.translateZ(...)
.scale(...)
.rotate(...)
.rotateX(...)
.rotateY(...)
.rotateZ(...)
.perspective(...)
.transformOrigin(...)
.states(normal, hover, pressed)
```

## Transform / 2.5D DSL

Transform 是渲染阶段能力，不参与 measure / layout。所有可视元素和 `Row` / `Column` / `Stack` 容器都可以声明 transform，父容器 transform 会以投影矩阵继承到子树。

```cpp
ui.stack("flip.card")
    .size(220.0f, 132.0f)
    .rotateY(open ? 3.14159f : 0.0f)
    .perspective(520.0f)
    .transformOrigin(0.5f, 0.5f)
    .transition(0.42f, core::Ease::OutBack)
    .animate(core::AnimProperty::Transform)
    .content([&] {
        ui.rect("flip.card.bg")
            .size(220.0f, 132.0f)
            .radius(18.0f)
            .color({0.18f, 0.28f, 0.72f, 1.0f})
            .build();
    })
    .build();
```

2D API：

```cpp
.translate(x, y)
.translateX(x)
.translateY(y)
.scale(value)
.scale(x, y)
.rotate(radians)
.rotateZ(radians)
.rotation(radians)
.transformOrigin(xRatio, yRatio)
```

2.5D API：

```cpp
.translate3d(x, y, z)
.translateZ(z)
.rotateX(radians)
.rotateY(radians)
.perspective(distance)
```

`rotateX` / `rotateY` 会把元素所在平面投影回屏幕坐标；`perspective(distance)` 是透视距离，值越小透视越强，`0` 表示关闭透视。`translateZ` 只有配合 perspective 才会产生明显视觉缩放。当前不会启用真实 depth buffer，绘制顺序仍由 DSL 树顺序和 `zIndex` 决定；默认 hit-test 仍按未 transform 的布局 frame 计算，需要跟随视觉投影时开启 `.transformedHitTest()`。

## 动画 DSL

动画目标写在元素属性上，Runtime 负责从当前值插值到目标值：

```cpp
ui.rect("actor")
    .x(active ? 420.0f : 40.0f)
    .opacity(active ? 0.4f : 1.0f)
    .rotate(active ? 0.4f : 0.0f)
    .transition(0.42f, core::Ease::OutBack)
    .animate(core::AnimProperty::Frame |
             core::AnimProperty::Opacity |
             core::AnimProperty::Transform)
    .build();
```

Frame 动画需要显式 `.animate(core::AnimProperty::Frame)`。窗口大小变化、页面切换导致的普通布局尺寸变化不会默认产生长宽动画。

容器 `Row` / `Column` / `Stack` 也支持 `opacity` 和 transform。Runtime 会把容器的 `translate`、`scale`、`rotate`、`rotateX`、`rotateY`、`translateZ`、`perspective`、`transformOrigin` 组合成投影矩阵并继承到子树，因此弹窗、下拉、菜单、卡片翻转和透视动画会作用到内部 Rect / Text / Image / Polygon。布局占位仍由未 transform 的逻辑 frame 决定。

当前可动画属性：

- Rect：frame、color、opacity、radius、border、shadow、blur、transform。
- Text：frame、text color、opacity、transform。
- Image：frame、tint/color、opacity、radius、transform。
- Polygon：frame、color、opacity、transform。

## 组件写法

组件层在 `components/`，不要直接持有 primitive，也不要绕过 Runtime。

当前组件：

- `components::panel(ui, id)`：返回套用 theme token 的 `RectBuilder`。
- `components::text(ui, id)`：返回套用 theme token 文本色的 `TextBuilder`。
- `components::label(ui, id)`：返回套用 theme token 文本色的 label builder。
- `components::image(ui, id)`：返回套用 theme token 的 `ImageBuilder`。
- `components::mouseArea(ui, id)`：透明输入热区，封装 tap、press、release、hover、drag、scroll、context menu。
- `components::button(ui, id)`：薄 builder，内部组合 `Stack + Rect + Row + Text`。
- `components::checkbox(ui, id)`：无状态 checkbox，点击回调 next checked。
- `components::radio(ui, id)`：无状态 radio，点击回调 select / next checked。
- `components::toggleSwitch(ui, id)`：无状态 switch，点击回调 next checked。
- `components::progress(ui, id)`：进度条，value 范围 `0.0f - 1.0f`。
- `components::slider(ui, id)`：滑块，点击或拖拽回调 next value。
- `components::input(ui, id)`：基础文本输入，页面传 value，组件回调 next value。
- `components::segmented(ui, id)`：分段选择，点击回调 next index。
- `components::tabs(ui, id)`：标签页切换，点击回调 next index。
- `components::scroll(ui, id)`：滚动条/offset 控制器，页面传 viewport/content/offset，组件回调 next offset。
- `components::dropdown(ui, id)`：下拉选择，页面传 selected/open，组件回调 next index 和 open 状态。
- `components::datepicker(ui, id)`：dialog 式日期选择器，页面传 date/open，面板内调整是 draft，点击 `Done` 后才回调 next date。
- `components::timepicker(ui, id)`：dialog 式时间选择器，页面传 time/open，面板内调整是 draft，点击 `Done` 后才回调 next time。
- `components::colorpicker(ui, id)`：dialog 式颜色选择器，页面传 color/open，RGB slider 和色块只改 draft，点击 `Done` 后才回调 next color。
- `components::dataTable(ui, id)`：简单数据表，`components::datatable(ui, id)` 是兼容别名。
- `components::dialog(ui, id)`：模态对话框，页面传 open 状态。
- `components::toast(ui, id)`：toast 提示，支持 duration / autoDismiss。
- `components::contextMenu(ui, id)`：右键菜单，支持 position、screen、items、dismiss。
- `components::linechart(ui, id)`：折线图，hover 数据点显示 tooltip，`components::lineChart(ui, id)` 是兼容别名。
- `components::barchart(ui, id)`：柱状图，hover 柱子显示 tooltip，`components::barChart(ui, id)` 是兼容别名。
- `components::piechart(ui, id)`：饼图，用 `Polygon` 绘制扇区，hover 扇区显示 tooltip，`components::pieChart(ui, id)` 是兼容别名。

按钮示例：

```cpp
components::button(ui, "save")
    .size(180.0f, 54.0f)
    .icon(0xF0C7)
    .text("Save")
    .colors(normal, hover, pressed)
    .transition(0.18f)
    .onClick([] {
        // state change
    })
    .build();
```

## Runtime 行为

`core::dsl::Runtime` 负责：

- 持有 `Ui`。
- 调用 `ui.layout()` 计算逻辑坐标。
- 按 id 缓存 Rect / Text / Image / Polygon primitive 实例。
- 每帧回收已经不在 DSL 树里的 primitive、交互状态和 dirty key 实例。
- 统一处理 pointer event、hit-test、press capture、click。
- 维护 hover / press 动画状态。
- 推进 transition 动画。
- 维护 dirty rect。
- 使用离屏 framebuffer cache + scissor 做脏区渲染。
- 处理 DPI scale。
- render / shutdown。

纯 hover / press / transition 视觉变化不会重新 compose 页面。click 回调通常会修改 app 状态，因此 Runtime 会设置 `needsCompose()`，`app/dsl_app.h` 再重新 compose 并保守触发 full redraw。

## 当前限制

- 已有基础 z-index 和矩形 clip；复杂圆角 clip、嵌套滚动区域的事件冒泡还没做。
- `components::scroll` 现在负责滚动条和 offset，内容区可以用 `.clip()` + `y(-offset)` 组合实现裁剪滚动。
- 已有基础键盘 focus / text input / 选择 / 剪贴板 / 撤销 / 重做；IME 组合态还没做。
- 还没有事件冒泡。
- 已有 click / press / release / hover changed / context menu / text input / scroll / drag 回调；更顺手的手势开发优先用 `components::mouseArea`。
- 默认 hit-test 按布局矩形计算；开启 `.transformedHitTest()` 后会按元素当前 transform 和父容器继承矩阵反投影命中。
- 脏区渲染是保守矩形，复杂重叠场景可能扩大重绘区域。
