# EUI-NEO

EUI-NEO 是一个基于 OpenGL + GLFW 的声明式 2D GUI 框架。

- 组件放在 `src/components`，尽量一个组件一个 `.h`

<p align="center">
  <img src="./docs/1.jpg" alt="EUI-NEO Preview 1" width="49%" />
  <img src="./docs/2.jpg" alt="EUI-NEO Preview 2" width="49%" />
  <img src="./docs/3.jpg" alt="EUI-NEO Preview 3" width="49%" />
  <img src="./docs/4.jpg" alt="EUI-NEO Preview 4" width="49%" />
</p>

## 目录

```text
EUI-NEO/
├─ main.cpp
├─ README.md
├─ app/
│  ├─ basic_demo.cpp
│  ├─ calculator_demo.cpp
│  └─ calculator_logic.h
├─ docs/
│  ├─ ui_dsl_analysis.md
│  └─ gpui_full_redraw_optimization.md
├─ src/
│  ├─ EUINEO.h
│  ├─ EUINEO.cpp
│  ├─ app/
│  │  └─ DslAppRuntime.h
│  ├─ components/
│  │  ├─ Button.h
│  │  ├─ ComboBox.h
│  │  ├─ CustomNodeTemplate.h
│  │  ├─ InputBox.h
│  │  ├─ Label.h
│  │  ├─ Panel.h
│  │  ├─ Polygon.h
│  │  ├─ ProgressBar.h
│  │  ├─ ScrollArea.h
│  │  ├─ SegmentedControl.h
│  │  ├─ Sidebar.h
│  │  └─ Slider.h
│  ├─ pages/
│  │  ├─ AnimationPage.h
│  │  ├─ HomePage.h
│  │  ├─ LayoutPage.h
│  │  ├─ MainPage.h
│  │  └─ TypographyPage.h
│  ├─ ui/
│  │  ├─ UIBuilder.h
│  │  ├─ UIComponents.def
│  │  ├─ UIContext.cpp
│  │  ├─ UIContext.h
│  │  ├─ UINode.h
│  │  ├─ UIPrimitive.cpp
│  │  └─ UIPrimitive.h
│  └─ font/
│     ├─ Font Awesome 7 Free-Solid-900.otf
│     └─ YouSheBiaoTiHei-2.ttf
└─ CMakeLists.txt
```

## 当前演示页面

- `Home`
  - 基础控件页
  - 顶部按钮区 + 下方两列表单区
  - `Outline` 按钮会随机切换主题主色

- `Animation`
  - 自定义 `AnimationCardNode`
  - 演示 `Color / scale / rotation / gradient / queue` 动画轨道

- `Layout`
  - `row() / column() / flex()` 最小示例页
  - 一个滑条控制左右 `flex` 比例
  - 用来测试底层布局 DSL

- `Typography`
  - SDF 文本预览页
  - 展示不同字号、混排文本和图标字体效果

## 编译

```bash
cmake -B build -G Ninja
cmake --build build --config Release
```

## 运行

入口在 `main.cpp`。主循环是事件驱动重绘：

```cpp
EUINEO::MainPage mainPage{};

while (!glfwWindowShouldClose(window)) {
    if (EUINEO::State.needsRepaint ||
        EUINEO::State.animationTimeLeft > 0.0f ||
        mainPage.WantsContinuousUpdate()) {
        mainPage.Update();
    }

    if (EUINEO::Renderer::ShouldRepaint()) {
        EUINEO::Renderer::BeginFrame();
        mainPage.Draw();
    }
}
```

## app 目录说明

`app/` 是业务开发目录，给使用者放自己的页面入口和逻辑代码，不是框架底层目录。

- `app/*.cpp`：一个文件对应一个可执行入口（一个 demo / 一个应用入口）
- `app/*.h`：页面逻辑、状态、业务函数
- `src/`：框架源码（组件、渲染、布局、运行时）

当前示例：

- `app/basic_demo.cpp`
- `app/calculator_demo.cpp`
- `app/calculator_logic.h`

如果你使用声明式 UI 运行时，入口函数是 `src/app/DslAppRuntime.h` 里的 `RunDslApp`。常见写法是：

```cpp
// app/xxx_demo.cpp
int main() {
    EUINEO::DslAppConfig config;
    // window / page / fps 配置
    return EUINEO::RunDslApp(config, composeFn);
}
```

```cpp
// app/xxx_logic.h
struct XxxLogic {
    // 状态、输入处理、计算逻辑
};
```

## DSL 运行配置

`DslAppConfig` 现在支持帧率配置：

```cpp
EUINEO::DslAppConfig config;
config.title = "EUI Demo";
config.width = 800;
config.height = 600;
config.pageId = "demo";
config.fps = 120;
```

- `config.fps <= 0`：默认垂直同步（VSync）
- `config.fps > 0`：关闭 VSync，按目标帧率限帧
- 不写 `fps` 时默认值是 `0`，等价于垂直同步

## 字体与回退

字体加载入口也在 `main.cpp`，当前主要配置是：

```cpp
constexpr const char* kUIFontFile = "YouSheBiaoTiHei-2.ttf";
constexpr const char* kIconFontFile = "Font Awesome 7 Free-Solid-900.otf";
constexpr float kUiSdfLoadSize = 72.0f;
constexpr float kIconSdfLoadSize = 96.0f;
constexpr float kCjkSdfLoadSize = 72.0f;
```

项目字体默认会从下面两个目录里找：

- `src/font/`
- `font/`

如果你要改主文本字体：

1. 把字体文件放到 `src/font/`
2. 修改 `main.cpp` 里的 `kUIFontFile`
3. 按需要调整 `kUiSdfLoadSize`

如果你要改图标字体：

1. 把图标字体放到 `src/font/`
2. 修改 `main.cpp` 里的 `kIconFontFile`
3. 按需要调整 `main.cpp` 里预加载的 icon codepoint

当前文本渲染约定：

- 普通文本走 SDF
- 图标字体走普通 bitmap alpha，不走 SDF

当前默认系统字体回退：

- 主 UI 字体文件如果加载失败，会按下面顺序回退：
  - `C:/Windows/Fonts/msyh.ttc`
  - `C:/Windows/Fonts/arial.ttf`
- 中文缺字回退使用：
  - `C:/Windows/Fonts/msyh.ttc`

当前中文缺字回退是延迟加载：

- 启动时不会直接把 `msyh.ttc` 整包读进内存
- 只有项目字体缺某个字时，才会加载 `msyh.ttc` 并生成对应 glyph

## 页面写法

页面层目标是只写布局和声明，不回流到组件内部实现。

```cpp
ui.begin("main");

ui.sidebar("sidebar")
    .position(sidebarX, sidebarY)
    .size(sidebarWidth, sidebarHeight)
    .width(60.0f, 86.0f)
    .brand("EUI", "NEO")
    .selectedIndex(static_cast<int>(currentView_))
    .item("\xEF\x80\x95", "Home", [this] { SwitchView(MainPageView::Home); })
    .item("\xEF\x81\x8B", "Animation", [this] { SwitchView(MainPageView::Animation); })
    .item("\xEF\x80\x89", "Layout", [this] { SwitchView(MainPageView::Layout); })
    .item("\xEF\x80\xB1", "Typography", [this] { SwitchView(MainPageView::Typography); })
    .themeToggle([this] { ToggleTheme(); })
    .build();

ComposeCurrentPage(PageBounds());

ui.end();
```

当前真实参考：

- `src/pages/MainPage.h`
- `src/pages/HomePage.h`
- `src/pages/AnimationPage.h`
- `src/pages/LayoutPage.h`
- `src/pages/TypographyPage.h`

## Row / Column / Flex

现在底层已经有三种布局写法：

- `ui.row()`
- `ui.column()`
- `ui.flex()`

其中：

- `row()` 是横向排布
- `column()` 是纵向排布
- `flex()` 是通用弹性容器，靠 `.direction(...)` 选横向或纵向
- 子项用 `.flex(n)` 吃剩余空间

最简单示例：

```cpp
ui.column()
    .position(bounds.x, bounds.y)
    .size(bounds.width, bounds.height)
    .gap(16.0f)
    .content([&] {
        ui.row()
            .height(40.0f)
            .gap(12.0f)
            .content([&] {
                ui.button("a").flex(1.0f).text("A").build();
                ui.button("b").flex(1.0f).text("B").build();
            });

        ui.flex()
            .direction(EUINEO::FlexDirection::Row)
            .flex(1.0f)
            .gap(16.0f)
            .content([&] {
                ui.panel("left").flex(0.35f).build();
                ui.panel("right").flex(0.65f).build();
            });
    });
```

当前规则很简单：

- 主轴尺寸：固定尺寸优先，剩余空间交给 `.flex(n)`
- `column()` 子项默认横向撑满
- `row()` 子项默认保留自己的高度
- 需要容器内边距时直接 `.padding(...)`

## 当前锚点定位

现在这套锚点定位不是约束布局，也不是相对父容器自动排版；它本质上是：

- `9` 宫格屏幕锚点
- 组件自己的 `x / y / width / height`
- 再叠加当前上下文偏移 `contextOffsetX / contextOffsetY`

当前支持的锚点枚举：

- `TopLeft`
- `TopCenter`
- `TopRight`
- `CenterLeft`
- `Center`
- `CenterRight`
- `BottomLeft`
- `BottomCenter`
- `BottomRight`

默认锚点是 `TopLeft`。

实际计算规则在 `src/ui/UIPrimitive.cpp`，等价于：

```cpp
finalX = x + contextOffsetX + anchorOffsetX(screenW, width);
finalY = y + contextOffsetY + anchorOffsetY(screenH, height);
```

也就是：

- `TopLeft`：`x/y` 就是左上角绝对坐标
- `TopCenter`：先把组件放到屏幕顶部水平居中，再加 `x/y`
- `TopRight`：先贴到屏幕右上角，再加 `x/y`
- `Center`：先放到屏幕中心，再加 `x/y`
- `BottomRight`：先贴到屏幕右下角，再加 `x/y`

示例：

```cpp
ui.button("confirm")
    .size(160.0f, 44.0f)
    .anchor(EUINEO::Anchor::BottomRight)
    .position(-24.0f, -24.0f)
    .text("Confirm")
    .build();
```

这表示按钮先按右下角锚定到屏幕，再向左上回退 `24px`。

要注意两点：

- 这个锚点当前是基于 `State.screenW / State.screenH` 解析的，也就是基于屏幕，不是基于父面板局部宽高。
- `scrollArea`、`popup` 这类容器效果，本质上靠 `contextOffset + clip` 实现，不是新的锚点系统。

所以项目当前页面布局的真实写法仍然是“先手工算出区域 bounds，再把控件放进去”。例如：

- `MainPage` 先算 `sidebar` 和 `content` 区域
- `LayoutPage` 先算左右分栏宽度，再放组件
- 页面头部和 section 复用 `src/ui/ThemeTokens.h` 里的 helper

## 自定义组件

推荐路径是先直接用 `ui.node<T>()`，不要一上来就改 `UIContext`。

```cpp
ui.node<EUINEO::TemplateCardNode>("stats.cpu")
    .position(120.0f, 80.0f)
    .size(220.0f, 96.0f)
    .call(&EUINEO::TemplateCardNode::setTitle, std::string("CPU"))
    .call(&EUINEO::TemplateCardNode::setValue, std::string("42%"))
    .call(&EUINEO::TemplateCardNode::setAccent, EUINEO::Color(0.30f, 0.65f, 1.0f, 1.0f))
    .build();
```

如果后面确实想写成 `ui.templateCard("...")`，再去 `src/ui/UIComponents.def` 注册别名：

```cpp
EUI_UI_COMPONENT(templateCard, TemplateCardNode)
```

可复制模板：

- `src/components/CustomNodeTemplate.h`

## 当前内置组件

- `panel`
- `glassPanel`
- `label`
- `button`
- `progress`
- `slider`
- `combo`
- `input`
- `segmented`
- `sidebar`

## 相关文档

- `docs/ui_dsl_analysis.md`
- `docs/gpui_full_redraw_optimization.md`

## Polygon

`ui.polygon("id")` 用来做自定义形状，顶点通过 `points({...})` 提供。

- `points` 里的坐标是当前节点内部的归一化坐标
- `Point2{0.0f, 0.0f}` 是左上角
- `Point2{1.0f, 1.0f}` 是右下角
- `3` 个点就是三角形
- `4+` 个点可以做更多多边形

最简单的三角形：

```cpp
ui.polygon("triangle")
    .position(420.0f, 180.0f)
    .size(72.0f, 64.0f)
    .background(CurrentTheme->primary)
    .points({
        Point2{0.50f, 0.00f},
        Point2{1.00f, 1.00f},
        Point2{0.00f, 1.00f},
    })
    .build();
```

`polygon` 和 `panel` 一样，也支持：

- `.background()`
- `.border()`
- `.opacity()`
- `.rotation()`
- `.animate*()` / `.hover*()`

## Theme Token

页面如果要和现有界面保持一致，不要直接写死字号、颜色、间距，优先从 `src/ui/ThemeTokens.h` 取 token。

页面层常用入口：

```cpp
const PageVisualTokens visuals = CurrentPageVisuals();
```

常用字段：

- 字号：`visuals.headerTitleSize`、`visuals.headerSubtitleSize`、`visuals.labelSize`
- 间距：`visuals.sectionGap`、`visuals.sectionInset`
- 卡片：`visuals.cardColor`、`visuals.mutedCardColor`、`visuals.sectionRounding`
- 颜色：`visuals.titleColor`、`visuals.subtitleColor`、`visuals.bodyColor`

如果是组件或基础图元颜色，直接用：

```cpp
CurrentTheme->primary
CurrentTheme->surface
CurrentTheme->text
CurrentTheme->border
```

页面里推荐这样写：

```cpp
const PageVisualTokens visuals = CurrentPageVisuals();

ui.label("title")
    .text("Demo")
    .position(bounds.x, bounds.y + visuals.headerTopInset)
    .fontSize(visuals.headerTitleSize)
    .color(visuals.titleColor)
    .build();
```

## 动画 DSL

当前最基础的形状就是：

- `ui.panel`：矩形 / 圆角矩形
- `ui.polygon`：三角形 / 多边形

动画 DSL 分两种：

- 循环动画：`.animateScale()`、`.animateRotation()`、`.animateOpacity()`、`.animateTranslateX()`、`.animateTranslateY()`、`.animateBackground()`
- Hover 动画：`.hoverScale()`、`.hoverRotation()`、`.hoverOpacity()`、`.hoverTranslateX()`、`.hoverTranslateY()`、`.hoverBackground()`

`button` 组件当前支持：

- `.hoverScale(idle, hover, duration)`

`panel` hover 示例：

```cpp
ui.panel("card")
    .position(120.0f, 160.0f)
    .size(120.0f, 72.0f)
    .background(CurrentTheme->primary)
    .rounding(16.0f)
    .hoverScale(1.0f, 1.08f, 0.18f)
    .hoverTranslateY(0.0f, -6.0f, 0.18f)
    .hoverOpacity(0.72f, 1.0f, 0.18f)
    .build();
```

`polygon` hover 示例：

```cpp
ui.polygon("triangle.spin")
    .position(320.0f, 160.0f)
    .size(72.0f, 64.0f)
    .background(CurrentTheme->primary)
    .points({
        Point2{0.50f, 0.00f},
        Point2{1.00f, 1.00f},
        Point2{0.00f, 1.00f},
    })
    .hoverRotation(-12.0f, 12.0f, 0.18f)
    .hoverOpacity(0.45f, 1.0f, 0.18f)
    .build();
```

如果不需要 hover，直接把 `hover*` 改成 `animate*` 就可以做循环播放。
