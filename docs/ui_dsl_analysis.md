# EUI-NEO 声明式 UI DSL 与结构守则

## 目标

1. 页面现在应该怎么写
2. 组件现在应该怎么接入
3. `UIContext` 的职责边界是什么
4. 以后扩展时，什么做法算正确，什么做法算结构回退

---

## 当前结论

EUI-NEO 当前已经稳定在下面这套模型上：

- 页面是 `header-only` 组合层
- 组件定义集中在 `src/components`
- 一个组件尽量就是一个 `.h`
- `UIContext` 只做通用节点收集、compose 生命周期、clip / offset 继承、builder 入口
- 页面优先写 `Compose(...)`
- 新组件优先直接用 `ui.node<T>()`
- 只有组件 API 已经稳定时，才考虑补 `ui.xxx()` 别名

这意味着：

- 新增一个组件，不应该再去 `UIContext` 里补专用同步逻辑
- 新增一个页面，不应该再拆成一堆运行时桥接文件
- 不应该再出现 `src/ui/components` 这种镜像组件目录

---

## 页面层的正确形态

当前推荐的页面形态是：

- 页面本身是一个 `.h`
- 页面只暴露 `Compose(...)`
- 页面内部只保留布局计算、页面状态、回调绑定和 DSL 声明

典型结构：

```cpp
class LayoutPage {
public:
    struct Actions {
        std::function<void(float)> onSplitChange;
    };

    static void Compose(UIContext& ui, const std::string& idPrefix,
                        const RectFrame& bounds, float splitRatio,
                        const Actions& actions) {
        // layout
        // ui.panel(...)
        // ui.slider(...)
        // ui.label(...)
    }
};
```

当前真实页面：

- `src/pages/HomePage.h`
- `src/pages/AnimationPage.h`
- `src/pages/LayoutPage.h`

当前页面路由入口：

- `src/pages/MainPage.h`

`MainPage` 当前只负责：

- 外层 shell 布局
- sidebar
- content bounds
- view switch
- 主题切换
- 把不同页面分发到 `Compose(...)`

`MainPage` 不再负责：

- 每个页面单独的旧式运行时桥接
- 每个页面单独的 draw / update 接线
- 页面特判优化

---

## 组件层的正确形态

组件层当前推荐结构：

```cpp
class XxxNode : public UINode {
public:
    class Builder : public UIBuilderBase<XxxNode, Builder> { ... };

    explicit XxxNode(const std::string& key);
    static constexpr const char* StaticTypeName();
    const char* typeName() const override;

    void update() override;
    void draw() override;

protected:
    void resetDefaults() override;
};
```

组件自己的事情，必须在组件内部完成：

- 默认值
- 私有状态
- hover / pressed / focus
- 动画轨道
- 绘制

这些不应该放到：

- 页面层
- `UIContext`
- 额外桥接文件

---

## `UIContext` 的职责边界

`UIContext` 当前只应该做通用能力：

- `begin()` / `end()`
- 通过 key 获取或创建节点
- compose 周期收集
- 通用 `ui.node<T>()`
- 通用 `ui.component<T>()`
- `UIComponents.def` 注册出来的 `ui.xxx()`
- clip 栈继承
- offset 栈继承
- 节点顺序和 layer bounds 统计

`UIContext` 不应该负责：

- 识别组件私有字段
- 识别组件如何 update
- 识别组件如何 draw
- 维护组件专用 map
- 维护组件专用 switch case
- 写 `SyncButton` / `DrawButton` / `UpdateButton` 这种中央调度逻辑

一句话：

`UIContext` 是通用 compose 容器，不是组件实现仓库。

---

## `ui.node<T>()` 和 `ui.xxx()` 的关系

### 路径 A：先直接用 `ui.node<T>()`

这是默认推荐路径。

```cpp
ui.node<TemplateCardNode>("stats.cpu")
    .position(120.0f, 80.0f)
    .size(220.0f, 96.0f)
    .call(&TemplateCardNode::setTitle, std::string("CPU"))
    .call(&TemplateCardNode::setValue, std::string("42%"))
    .build();
```

优点：

- 不需要改 `UIContext`
- 不需要立刻写专用 builder
- 不需要先定死最终 DSL 语义

### 路径 B：组件稳定后再补 `ui.xxx()`

只有当组件真的需要更顺手的语义化入口时，再去 `src/ui/UIComponents.def` 增加一行：

```cpp
EUI_UI_COMPONENT(statsCard, StatsCardNode)
```

这一层只是加别名，不应该引入新的结构层。

---

## 是否每个组件都要写专用 Builder

不需要。

当前明确规则：

- 简单组件：优先 `DefaultNodeBuilder` 或 `ui.node<T>()`
- 复杂组件：再写专用 `Builder`

什么时候值得写专用 `Builder`：

- 组件有强语义 API
- 页面里反复出现同一串 `.call(...)`
- 组件希望把配置写得更可读

已经适合专用 builder 的组件：

- `Sidebar`
- `ComboBox`
- `Button`
- `InputBox`

完全可以先走 `ui.node<T>()` 的组件：

- 演示卡片
- 自定义统计块
- 某个页面专用 widget

---

## 当前内置组件

当前内置别名来自 `src/ui/UIComponents.def`：

- `panel`
- `glassPanel`
- `popupPanel`
- `label`
- `button`
- `progress`
- `slider`
- `combo`
- `input`
- `segmented`
- `sidebar`

对应实现都在：

- `src/components/*.h`

当前仓库已经没有“组件定义一份，UI 层再包一份”的双份结构。

---

## 当前最小正确扩展路径

### 新增一个页面

1. 在 `src/pages` 新建一个 `.h`
2. 写 `Compose(UIContext&, idPrefix, bounds, ...)`
3. 在 `MainPageView.h` 增加枚举
4. 在 `MainPage.h` 里接入 sidebar 和 `ComposeCurrentPage(...)`

### 新增一个组件

1. 复制 `src/components/CustomNodeTemplate.h`
2. 改类名和 `StaticTypeName()`
3. 完成 `resetDefaults()` / `update()` / `draw()`
4. 页面里先用 `ui.node<T>()`
5. 只有确认 API 稳定后，再补 `UIComponents.def`

---

## 明确禁止的结构回退

下面这些做法直接视为结构回退：

- 再建 `src/ui/components`
- 组件实现拆成“节点本体 + UI 壳 + 中央同步逻辑”三层
- 为了一个新组件去 `UIContext` 里加专用字段和专用流程
- 为了一个新页面去补运行时桥接文件
- 页面层直接操作组件内部动画状态
- 用页面特判替代通用组件能力

---

## DSL 与渲染的边界

当前 DSL 只负责声明结构，不负责重新发明渲染系统。

所以 DSL 层不要重新引入：

- 脏区传播
- 局部回放
- 页面特判缓存
- 组件专用渲染旁路

性能路线应该留在当前稳定基线：

- 事件驱动重绘
- Backdrop 层缓存
- 节点级表面缓存
- 稳定 compose / 稳定 key

---

## 代码评审清单

以后每次改 DSL 或组件结构，至少先过这份清单：

1. 新组件是否只改了 `src/components`
2. 新组件是否可以先不改 `UIContext`
3. 新增的 `ui.xxx()` 是否只是别名，而不是新桥接层
4. 页面是否仍然保持 `Compose(...)` 形态
5. 是否没有重新引入双份组件定义
6. 是否没有重新引入页面特判优化
7. 自定义节点是否自己处理了 `paintBounds()`
8. 是否没有重新引入脏区 / 局部回放链路

只要其中任意一项答不上来，这次改动就不合格。

---

## 一句话结论

当前 DSL 的核心边界只有一句话：

页面声明化，组件自描述，核心最小化。

