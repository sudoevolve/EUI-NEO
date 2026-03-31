# 组件与自定义节点

## 内置组件

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
