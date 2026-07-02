# DSL 设计与当前实现

当前推荐写法是声明式 DSL：应用只描述页面结构、样式、交互回调和目标状态，内部 Runtime 负责布局、状态缓存、事件、动画、脏区渲染和后端 primitive 同步。

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
- `Polygon`：多边形图元，支持点集、圆角、颜色、透明度、transform 和 hover/pressed 状态；当前图表 tooltip 指针和 pieChart 扇区都基于它。

组件不进入 core 枚举。组件层只是组合 DSL 图元，例如 `components::button(ui, id)` 内部使用 `Stack + Rect + Row + Text`。

## App 入口

DSL app 推荐只实现：

```cpp
#include "eui_neo.h"

namespace app {

const DslAppConfig& dslAppConfig();
void compose(eui::Ui& ui, const eui::Screen& screen);

} // namespace app
```

`include/eui/dsl_app.h` 已封装：

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

不设置 `.textFont(...)` 时使用 `core/render/text.cpp` 里的全局默认文本字体；不设置 `.iconFont(...)` 时使用全局默认图标字体。默认字体优先从可执行文件旁的 `assets/`、工作目录 `assets/`、上级运行目录 `assets/` 查找；找不到内置字体资源时会回退到平台系统字体，避免单 exe 漏带 assets 后普通文本整段不可见。

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
.justifyContent(eui::Align::CENTER)
.alignItems(eui::Align::CENTER)
.align(eui::Align::CENTER, eui::Align::CENTER)
.ignoreLayout()
.zIndex(value)
.clip()
```

`.zIndex(...)` 只影响同级元素的绘制顺序和 topmost hit-test，不参与布局计算；值越大越靠上。`.clip()` 会按该元素布局矩形裁剪自己和子树，并且命中测试也不会穿出裁剪区域。

`Row / Column` 的主轴 flex 分配会先用自身 fixed / fill 结果约束 inner size，再把结果作为子项测量约束；fixed、wrapContent 和 fill 子项都可以显式参与 grow / shrink，默认 `flexShrink` 为 0，避免固定尺寸控件被意外压扁。`Stack` 会用自身最终 inner size 重新测量 fill 子项，适合普通叠放背景层；`Stack.wrapContent()` 会把子项正向 x/y 偏移计入包裹尺寸，负向偏移仍视为向外溢出。

`.ignoreLayout()` 用于装饰背景、调试框等不应该占据布局流的覆盖层。它仍会渲染、仍遵守 zIndex / hit-test，但不参与父容器 measure、gap、Row / Column / Flow 排列，也不会撑开 wrapContent 或 scrollView 内容高度。不要用 `z(-1)` 代替 `.ignoreLayout()`；zIndex 只表达绘制层级，不表达布局语义。

## Loader 与实例状态

`loader` 是 DSL 级生命周期 scope，不是可视图元。它用于按 `active` 动态挂载一段 UI 子树，并决定这段子树隐藏时是否释放实例状态。

```cpp
ui.loader("settings.loader")
    .active(showSettings)
    .destroyOnHide()
    .content([&] {
        settingsPage.compose(ui, width, height);
    })
    .build();

ui.loader("editor.loader")
    .active(showEditor)
    .keepAlive()
    .content([&] {
        editorPage.compose(ui, width, height);
    })
    .build();
```

模式：

- `destroyOnHide()`：`active(false)` 时不 compose 内容，并释放该 loader scope 下通过 `ui.state<T>(id)` 创建的实例状态。
- `keepAlive()`：`active(false)` 时不 compose 内容，但保留该 loader scope 下的实例状态，下一次 active 后恢复。

Loader 内部会给内容 id 增加 scope 前缀，避免不同 loader 中的同名元素和实例状态相互串扰。组件作者可以用 `ui.state<T>(id)` 存放纯交互状态；状态 key 会自动包含当前 `pageId` 和 loader scope。

```cpp
struct CounterState {
    int value = 0;
};

ui.loader("counter.loader")
    .active(showCounter)
    .keepAlive()
    .content([&] {
        CounterState& state = ui.state<CounterState>("counter");
        ui.text("value")
            .text(std::to_string(state.value))
            .build();
    })
    .build();
```

组件内部的纯交互状态应统一走 `ui.state<T>(id)`，不要再使用组件级静态 `id -> state` 表。当前已接入该模型的状态包括 `input`、`mouseArea`、`carousel`、`datePicker`、`timePicker`、`colorPicker`、`pieChart`、`workshop::heartSwitch` 和 `workshop::neumorphicButton`。

## 通用交互 DSL

`Row / Column / Stack / Rect / Text / Image / Polygon` 都支持通用交互方法：

```cpp
.interactive(true)
.disabled(false)
.cursor(eui::CursorShape::Hand)
.hitTestMode(eui::dsl::HitTestMode::Transformed)
.transformedHitTest()
.onClick(callback)
.onPress(callback)
.onRelease(callback)
.onMove(callback)
.onContextMenu(callback)
.onHover(callback)
.focusable()
.onFocusChanged(callback)
.onTextInput(callback)
.onScroll(callback)
.onDrag(callback)
```

`.onClick(...)` 会自动开启 interactive，并把 cursor 设置为手型。Runtime 会做 topmost hit-test、按下捕获、点击判定和回调派发。命中测试按实际绘制顺序从最上层往下找，同一位置只会选中第一个命中的 interactive 元素；`zIndex` 越高越靠上，同级同 z 时后声明的元素更靠上。默认命中使用布局矩形；需要旋转、缩放或 2.5D 投影后的视觉区域参与命中时，用 `.hitTestMode(eui::dsl::HitTestMode::Transformed)` 或 `.transformedHitTest()`。`HitTestMode::Layout` 是默认布局命中，`Transformed` 会按当前投影矩阵反算命中，`None` 表示该元素自身不参与命中。

focus 命中也遵守同样的 topmost 顺序。鼠标按下时，如果最上层命中的是 focusable 元素，就聚焦它；如果最上层命中的是非 focusable 的 interactive 元素，例如 sidebar scrim、modal panel 背景或透明 hit rect，Runtime 会停止向下查找并清空焦点，避免点击穿透到底层输入框。

底层 DSL 的 `onPress/onRelease/onMove/onDrag/onScroll` 回调直接使用 Runtime 原始事件。`onMove` 只在元素是当前 topmost hover 目标且指针移动或刚进入 hover 时派发，适合确实需要回调改业务状态或组件私有视觉状态的场景。纯视觉、高频 pointer-follow transform 应优先用 Runtime binding，例如 `.runtimePointerTransformFrom(...)` / `.runtimePointerTiltFrom(...)`，避免每次 mouse move 触发组件状态写入和 compose。页面或组件需要 tap、拖拽阈值、滚轮步进、局部坐标、进入/离开 hover 时，优先用组件层的 `components::mouseArea(ui, id)`。

示例：

```cpp
ui.text("github.link")
    .size(260.0f, 34.0f)
    .text("GitHub")
    .color(accent)
    .onClick([] {
        eui::platform::openUrl("https://github.com/sudoevolve/EUI-NEO");
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
    .insetShadow(12.0f, 4.0f, 4.0f, {0.0f, 0.0f, 0.0f, 0.22f})
    .transition(0.2f, eui::Ease::OutCubic)
    .build();
```

Rect 支持：

```cpp
.color(...)
.gradient(...)
.radius(...)
.border(...)
.shadow(...)
.insetShadow(...)
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
.rotate(...)
.perspective(...)
.transformOrigin(...)
.states(normal, hover, pressed)
```

`.states(normal, hover, pressed)` 是 Rect 专用的 hover / pressed 视觉状态，会开启交互并由 Runtime 维护 blend。

`.shadow(...)` 绘制圆角外阴影，会扩大元素的 visual dirty rect。`.insetShadow(...)` 绘制裁剪在圆角内部的内阴影，不扩大 visual rect，适合拟态按压、内凹面板、输入框内侧暗边等效果。两者都是单阴影字段；需要多重阴影时，叠多个透明 `rect` 图层，每层使用稳定 id。

## Text DSL

```cpp
ui.text("title")
    .size(420.0f, 48.0f)
    .text("EUI Gallery")
    .fontFamily("YouSheBiaoTiHei")
    .fontSize(38.0f)
    .lineHeight(44.0f)
    .color({0.94f, 0.97f, 1.0f, 1.0f})
    .horizontalAlign(eui::HorizontalAlign::Center)
    .verticalAlign(eui::VerticalAlign::Center)
    .build();
```

Text 支持：

```cpp
.text(...)
.icon(codepoint)
.fontFamily(...)
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
.rotate(...)
.perspective(...)
.transformOrigin(...)
.maxWidth(...)
.wrap(...)
.horizontalAlign(...)
.verticalAlign(...)
.lineHeight(...)
```

`.icon(...)` 会自动使用图标字体；图标字体默认来自 `core/render/text.cpp`，也可以通过配置里的 `.iconFont(...)` 按 app 覆盖。找不到内置图标字体资源时会尝试平台 symbol/icon 字体兜底；但 FontAwesome 图标使用 FontAwesome 自己的 codepoint，系统字体不一定有兼容 glyph，发布包仍建议携带默认 `assets/` 或显式配置 `.iconFont(...)`。

底层文本使用 FreeType 渲染 glyph，启用 HarfBuzz 时会进行复杂文本 shaping。`fontFamily("monospace")` 会选择跨平台等宽字体，`fontFamily("Emoji")` 会选择平台 emoji 字体；如果指定字体或内置 assets 字体加载失败，文本栈会继续尝试默认 UI 字体和系统字体兜底。需要精确光标位置或命中测试时，使用 `core::TextPrimitive::measureTextMetrics(...)` 获取 shaped caret stops；返回的 `byteIndices` 是 UTF-8 byte offset，`caretX` 是对应的逻辑 x，和实际渲染使用同一套 fallback、emoji 缩放和 glyph advance。

Text 的 transform 作用在生成后的 glyph 顶点上，适合做滚轮、轻量缩放和旋转动效；默认命中测试仍按未 transform 的布局 frame 计算，需要跟随视觉变换时开启 `.transformedHitTest()`。

`ui.text(id)` 是标准文本入口。

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

ui.svg("heart.icon")
    .size(48.0f, 48.0f)
    .source(R"svg(<svg viewBox="0 0 24 24" xmlns="http://www.w3.org/2000/svg">
  <path fill="#ffffff" d="M12 21s-8-4.7-8-11a5 5 0 0 1 8-4 5 5 0 0 1 8 4c0 6.3-8 11-8 11z"/>
</svg>)svg")
    .tint({1.0f, 0.36f, 0.58f, 1.0f})
    .contain()
    .build();
```

Image 支持：

```cpp
.source(pathOrUrl)
.bingDaily(idx, mkt)
.svg(id).source(svgMarkup)
.fit(eui::ImageFit::Cover)
.cover()
.contain()
.stretch()
.radius(...)
.opacity(...)
.tint(...)
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
.radius(...)
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

`radius(...)` 会对多边形顶点做圆润过渡，适合 tooltip 指针这类小三角形；命中测试会复用同一套圆角几何。渲染后端为 `Polygon` 使用独立 polygon shader，OpenGL 和 Vulkan 都按多边形边段计算覆盖率抗锯齿，不再借圆角矩形 shader 或 bounding box 填充。

## Transform / 2.5D DSL

Transform 是渲染阶段能力，不参与 measure / layout。所有可视元素和 `Row` / `Column` / `Stack` 容器都可以声明 transform，父容器 transform 会以投影矩阵继承到子树。

```cpp
ui.stack("flip.card")
    .size(220.0f, 132.0f)
    .rotateY(open ? 3.14159f : 0.0f)
    .perspective(520.0f)
    .transformOrigin(0.5f, 0.5f)
    .transition(0.42f, eui::Ease::OutBack)
    .animate(eui::AnimProperty::Transform)
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
.rotate(radians)
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
    .transition(0.42f, eui::Ease::OutBack)
    .animate(eui::AnimProperty::Frame |
             eui::AnimProperty::Opacity |
             eui::AnimProperty::Transform)
    .build();
```

Frame 动画需要显式 `.animate(eui::AnimProperty::Frame)`。窗口大小变化、页面切换导致的普通布局尺寸变化不会默认产生长宽动画。

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
- `components::image(ui, id)`：返回套用 theme token 的 `ImageBuilder`。
- `components::markdown(ui, id)`：基于 MD4C 解析 Markdown，在组件层组合 `Column / Row / Stack / Rect / Text` 显示 CommonMark 与启用的 MD4C 扩展。当前接入 MD4C 的 block/span/text 回调面：标题、段落、引用、无序/有序/任务列表、分隔线、fenced/indented code、HTML block、表格和表格对齐，以及 emphasis、strong、link/autolink、image、inline code、strikethrough、LaTeX math、wiki link、underline、inline HTML、entity、soft/hard break 等 inline 内容。inline code、链接、图片、math、wiki、HTML、删除线和下划线会在组件内部 line-box 排版中映射为可见文本、胶囊或装饰线；HTML/CSS 不执行，图片显示为占位文本。默认由 `EUI_ENABLE_MARKDOWN=ON` 启用；关闭后退化为纯文本段落。
- `components::mouseArea(ui, id)`：透明输入热区，封装 tap、press、release、hover、move、drag、scroll、context menu。
- `components::button(ui, id)`：薄 builder，内部组合 `Stack + Rect + Row + Text`。
- `components::checkbox(ui, id)`：无状态 checkbox，点击回调 next checked。
- `components::radio(ui, id)`：无状态 radio，点击回调 next selected。
- `components::toggleSwitch(ui, id)`：无状态 switch，点击回调 next checked。
- `components::progress(ui, id)`：进度条，value 范围 `0.0f - 1.0f`。
- `components::slider(ui, id)`：滑块，点击或拖拽回调 next value。
- `components::input(ui, id)`：基础文本输入，页面传 value，组件回调 next value。
- `components::segmented(ui, id)`：分段选择，点击回调 next index。
- `components::tabs(ui, id)`：标签页切换，点击回调 next index。
- `components::scroll(ui, id)`：滚动条，绑定 Runtime scroll state 后由 Runtime 更新 thumb transform。
- `components::dropdown(ui, id)`：下拉选择，页面传 selected/open，组件回调 next index 和 open 状态。
- `components::datePicker(ui, id)`：dialog 式日期选择器，页面传 date/open，面板内调整是 draft，点击 `Done` 后才回调 next date。
- `components::timePicker(ui, id)`：dialog 式时间选择器，页面传 time/open，面板内调整是 draft，点击 `Done` 后才回调 next time。
- `components::colorPicker(ui, id)`：dialog 式颜色选择器，页面传 color/open，RGB slider 和色块只改 draft，点击 `Done` 后才回调 next color。
- `components::dataTable(ui, id)`：简单数据表。
- `components::dialog(ui, id)`：模态对话框，页面传 open 状态。
- `components::toast(ui, id)`：toast 提示，支持 duration。
- `components::tooltip(ui, id)`：轻量提示浮层，支持 anchor、title/value、hover source 和圆润 Polygon 指针，指针边缘走 polygon shader 抗锯齿。
- `components::contextMenu(ui, id)`：右键菜单，支持 position、screen、items、open / onOpenChange。
- `components::lineChart(ui, id)`：折线图，通过 `.style(components::LineStyle::...)` 支持 Linear / Curve / Step 线型，折线段使用 capsule polygon 绘制，hover 数据点显示 tooltip。
- `components::barChart(ui, id)`：柱状图，hover 柱子显示 tooltip。
- `components::pieChart(ui, id)`：饼图，用 `Polygon` 绘制扇区，hover 扇区显示 tooltip。

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

内部 Runtime 负责：

- 持有 `Ui`。
- 调用 `ui.layout()` 计算逻辑坐标。
- 按 id 缓存 Rect / Text / Image / Polygon primitive 实例。
- 每帧回收已经不在 DSL 树里的 primitive、交互状态和 dirty key 实例。
- 统一处理 pointer event、hit-test、press capture、click。
- disabled 父节点会禁用整棵子树的交互、焦点、文本输入和 IME 光标状态。
- 维护 scroll state；滚轮和滚动条拖动只更新滚动 transform 和 dirty rect，不触发整页 compose。
- interactive blocker 会阻断下层 hover / click / focus；弹层、侧边栏、遮罩和面板背景应声明透明或实体 hit rect 来吃掉事件。
- 维护 hover / press 动画状态。
- 推进 transition 动画。
- 维护 dirty rect。
- 使用离屏 framebuffer cache + scissor 做 Runtime 层脏区重绘；后端再负责 cache blit 和窗口 present。
- 在 `layout()` 后缓存同级绘制顺序和子树能力标记，避免 update / hit-test / render 热路径反复分配、排序和扫描无关子树。
- 自动 retained layer cache 会缓存稳定静态子树；候选判断使用 layout/update 阶段缓存的子树标记，避免动画帧里重复递归扫描。
- 对静态、无交互、无动画、无 timer、无 scroll、无 dirty key 的子树做保守 early-out；指针不在子树 bounds 内且没有继承 transform / opacity 变化时，可以复用上一帧 paint bounds。
- 指针没有移动、没有按键边沿、树结构没有变化且上一帧没有动画时，Runtime 会复用上一帧 hover 命中目标，避免复杂静态页面每帧重新全树 hit-test。
- 处理 DPI scale。
- render / shutdown。

纯 hover / press / transition 视觉变化不会重新 compose 页面。click 回调通常会修改 app 状态，因此 Runtime 会设置 `composeRequested()`，`include/eui/dsl_app.h` 再重新 compose 并保守触发 full paint。

## 当前限制

- 已有基础 z-index、矩形 clip 和 Runtime scroll state；复杂圆角 clip、嵌套滚动区域的事件冒泡还没做。
- `components::scrollView` 是推荐滚动区域；底层 `components::scroll` 只负责滚动条，需要和内容容器绑定同一个 Runtime scroll state。
- 已有基础键盘 focus / text input / 选择 / 剪贴板 / 撤销 / 重做 / IME 预编辑组合串和系统候选窗口定位。
- 还没有事件冒泡。
- 已有 click / press / release / pointer move / hover / context menu / text input / scroll / drag 回调；更顺手的手势开发优先用 `components::mouseArea`。
- 默认 hit-test 按布局矩形计算；开启 `.transformedHitTest()` 后会按元素当前 transform 和父容器继承矩阵反投影命中。
- 脏区渲染是保守矩形，复杂重叠场景可能扩大重绘区域。Runtime 可以只重绘脏区，Vulkan 可以按 dirty rect 同步 render cache；最终 present 是否也是脏区提交取决于平台窗口系统、图形 API 和驱动能力。
- 当前优化是 Runtime 级遍历、framebuffer cache、dirty rect 和自动 retained layer cache 的组合；OpenGL/Vulkan 后端都提供 retained layer 资源。它不是完整 retained scene graph，复杂 blur、动态 image/svg、交互和动画子树仍会走普通 dirty repaint。
