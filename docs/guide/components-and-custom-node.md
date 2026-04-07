# 组件与自定义节点

## 内置组件

- `panel`
- `glassPanel`
- `popupPanel`
- `polygon`
- `label`
- `button`
- `progress`
- `slider`
- `combo`
- `image`
- `input`
- `textArea`
- `segmented`
- `switcher`
- `checkbox`
- `radio`
- `dialog`
- `tooltip`
- `sidebar`

## 新增组件速览

### 表单与选择

- `textArea`：多行输入，支持 `.multiline(true)`、`.text(...)`、`.onChange(...)`
- `switcher`：开关选择，支持 `.checked(...)`、`.label(...)`、`.onChange(...)`
- `checkbox`：复选框，支持 `.checked(...)`、`.text(...)`、`.onChange(...)`
- `radio`：单选项，支持 `.selected(...)`、`.text(...)`、`.onChange(...)`

```cpp
ui.textArea("form.desc")
    .position(120.0f, 120.0f)
    .size(320.0f, 120.0f)
    .placeholder("Description...")
    .multiline(true)
    .text(descText)
    .onChange([&](const std::string& text) { descText = text; })
    .build();

ui.switcher("form.enabled")
    .position(120.0f, 260.0f)
    .checked(enabled)
    .label("Enabled")
    .onChange([&](bool value) { enabled = value; })
    .build();
```

### 弹层与提示

- `dialog`：模态弹窗，支持 `.open(...)`、`.onConfirm(...)`、`.onCancel(...)`、`.onClose(...)`
- `tooltip`：提示浮层，支持 `.text(...)`、`.triggerOnHover(true)`、`.followMouse(true)`
- `popupPanel`：通用弹层容器，通常配合 `.popupLayer()` 与较高 `zIndex`

```cpp
ui.dialog("confirm.delete")
    .open(showDialog)
    .title("Confirm")
    .message("Delete this item?")
    .confirmText("Delete")
    .cancelText("Cancel")
    .onConfirm([&] { doDelete(); })
    .onClose([&] { showDialog = false; })
    .build();

ui.tooltip("color.primary.tip")
    .position(120.0f, 80.0f)
    .size(80.0f, 40.0f)
    .text("Primary")
    .triggerOnHover(true)
    .followMouse(true)
    .build();
```

## 自定义节点

推荐先用 `ui.node<T>()` 直接接入：

```cpp
ui.node<EUINEO::TemplateCardNode>("stats.cpu")
    .position(120.0f, 80.0f)
    .size(220.0f, 96.0f)
    .call(&EUINEO::TemplateCardNode::setTitle, std::string("CPU"))
    .call(&EUINEO::TemplateCardNode::setValue, std::string("42%"))
    .build();
```

如需 DSL 别名，再在 `src/ui/UIComponents.def` 注册组件名。

## Polygon

`ui.polygon("id")` 用于自定义多边形。

- 点坐标为归一化局部坐标
- `Point2{0.0f, 0.0f}` 为左上
- `Point2{1.0f, 1.0f}` 为右下

## 图层与层级（Layer / Z）

渲染顺序先看 `layer`，再看 `zIndex`。

### Layer 顺序

- `Backdrop`：最底层，常用于背景与玻璃底
- `Content`：主内容层（默认）
- `Chrome`：导航、工具条等上层 UI
- `Popup`：弹层、下拉、候选框等最上层

从下到上固定为：

`Backdrop -> Content -> Chrome -> Popup`

### DSL 写法

默认是 `Content`，需要切层时：

```cpp
ui.panel("bg.blur")
    .layer(EUINEO::RenderLayer::Backdrop)
    .build();

ui.sidebar("app.sidebar")
    .layer(EUINEO::RenderLayer::Chrome)
    .build();

ui.panel("search.popup")
    .popupLayer()
    .build();
```

### zIndex 规则

- `zIndex` 只在同一 `layer` 内比较
- 不同 `layer` 间，`zIndex` 不会越层
- 也就是说：`Popup` 层的 `zIndex=0` 依然会盖住 `Chrome` 层的 `zIndex=999`

```cpp
ui.panel("card.a")
    .layer(EUINEO::RenderLayer::Content)
    .zIndex(1)
    .build();

ui.panel("card.b")
    .layer(EUINEO::RenderLayer::Content)
    .zIndex(2)
    .build();
```

上面例子中 `card.b` 会压在 `card.a` 上面，因为它们在同一层且 `zIndex` 更大。

## 交互冲突怎么处理（推荐）

如果遇到 hover/click 冲突，建议按这个顺序做：

- 先用 `layer` 分层，再在同层用 `zIndex`
- 优先使用弹层输入阻断（`Popup` 节点自动阻断底层 hover/scroll/click）
- 不要让两个可交互节点长期重叠在同一区域
- 透明点击层要单独管理显示与启用状态

### 弹层输入阻断（内置）

当前内置 `dialog`、`combo` 下拉在打开时会自动阻断底层交互：

- 底层节点不再触发 hover
- 底层 `scrollArea` 不再响应滚轮/拖拽滚动
- 底层 click 不会被穿透触发

### 自定义弹层节点（推荐实现）

如果你自定义了 `Popup` 节点，建议覆写 `blocksUnderlyingInput()`：

```cpp
class MyPopupNode : public EUINEO::UINode {
public:
    bool blocksUnderlyingInput() const override {
        return isOpen_ || openAnim_ > 0.001f;
    }
};
```

这样可以统一复用框架的输入阻断逻辑，而不需要手动给每个底层控件写 `.enabled(false)`。

### 兼容写法：手动禁用底层点击

```cpp
bool popupOpen = true;

ui.button("page.action")
    .position(120.0f, 120.0f)
    .size(140.0f, 40.0f)
    .enabled(!popupOpen)
    .text("Action")
    .build();

if (popupOpen) {
    ui.panel("page.popup")
        .position(100.0f, 90.0f)
        .size(320.0f, 220.0f)
        .popupLayer()
        .zIndex(300)
        .build();
}
```

这样做可以避免“上层弹窗显示了，但下层按钮还在响应点击”的问题。
