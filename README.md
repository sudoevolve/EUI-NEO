# EUI-NEO

EUI-NEO 是一个基于 OpenGL + GLFW 的声明式 2D GUI 框架。当前仓库已经收敛到一套比较明确的结构：

- 页面放在 `src/pages`，尽量一页一个 `.h`
- 组件放在 `src/components`，尽量一个组件一个 `.h`
- `UIContext` 只做通用节点收集、compose 生命周期、clip 继承和 builder 入口
- 渲染层走事件驱动重绘、Backdrop 层缓存和节点级 surface cache
- 不再使用脏区传播、局部回放、多矩形拆分这条旧路线

<p align="center">
  <img src="./1.jpg" alt="EUI-NEO Preview 1" width="49%" />
  <img src="./2.jpg" alt="EUI-NEO Preview 2" width="49%" />
</p>

## 目录

```text
EUI-NEO/
├─ main.cpp
├─ README.md
├─ docs/
│  ├─ ui_dsl_analysis.md
│  ├─ ui_dsl_guardrails.md
│  ├─ gpui_full_redraw_optimization.md
│  └─ fix.md
├─ src/
│  ├─ EUINEO.h
│  ├─ EUINEO.cpp
│  ├─ components/
│  │  ├─ Button.h
│  │  ├─ ComboBox.h
│  │  ├─ CustomNodeTemplate.h
│  │  ├─ InputBox.h
│  │  ├─ Label.h
│  │  ├─ Panel.h
│  │  ├─ ProgressBar.h
│  │  ├─ SegmentedControl.h
│  │  ├─ Sidebar.h
│  │  └─ Slider.h
│  ├─ pages/
│  │  ├─ AnimationPage.h
│  │  ├─ HomePage.h
│  │  ├─ LayoutPage.h
│  │  ├─ MainPage.h
│  │  └─ MainPageView.h
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
│     └─ Mountain and Nature.ttf
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
  - 最简单的布局示例页
  - 一个滑条控制左右分栏比例
  - 用来测试 split layout 的写法

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

## 当前渲染策略

- 事件驱动重绘：`Renderer::RequestRepaint()` / `ShouldRepaint()`
- Backdrop 层缓存：用于玻璃面板和 blur 背景
- 节点级 cached surface：`UIContext::draw()` 对非 Backdrop 节点走 `DrawCachedSurface`
- 文本宽度缓存：`Renderer::MeasureTextWidth()` 已做缓存

这一套的关键点是：

- 不做复杂遮挡推导
- 不做脏区矩形拆分
- 不做局部回放链路
- 用“稳定节点 key + 精确缓存失效”来降低普通交互时的 CPU / GPU 占用

## 相关文档

- `docs/ui_dsl_analysis.md`
- `docs/ui_dsl_guardrails.md`
- `docs/gpui_full_redraw_optimization.md`
- `docs/fix.md`
