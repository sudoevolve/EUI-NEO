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
