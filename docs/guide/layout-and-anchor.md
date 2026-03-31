# 布局与锚点

## 布局 DSL

可用容器：

- `ui.row()`：横向排布
- `ui.column()`：纵向排布
- `ui.flex()`：通用弹性容器，方向由 `.direction(...)` 决定

## flex 规则

- 固定尺寸优先
- 剩余空间按 `.flex(n)` 分配
- `column()` 子项默认横向撑满
- `row()` 子项默认保留自身高度

## 示例

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
    });
```

## 锚点定位

锚点基于屏幕坐标解析，最终位置受 `x/y + contextOffset` 共同影响。

支持锚点：

- `TopLeft`、`TopCenter`、`TopRight`
- `CenterLeft`、`Center`、`CenterRight`
- `BottomLeft`、`BottomCenter`、`BottomRight`

默认值：`TopLeft`

## 锚点示例

### 右下角确认按钮

```cpp
ui.button("dialog.confirm")
    .size(160.0f, 44.0f)
    .anchor(EUINEO::Anchor::BottomRight)
    .position(-24.0f, -24.0f)
    .text("Confirm")
    .build();
```

效果：按钮先锚到屏幕右下角，再向左上偏移 `24px`。

### 顶部居中标题

```cpp
ui.label("page.title")
    .anchor(EUINEO::Anchor::TopCenter)
    .position(0.0f, 40.0f)
    .text("Dashboard")
    .fontSize(28.0f)
    .color(EUINEO::Color(0.92f, 0.92f, 0.96f, 1.0f))
    .build();
```

效果：标题在顶部水平居中，`y=40` 表示向下偏移 40 像素。

### 屏幕中心卡片

```cpp
ui.panel("center.card")
    .anchor(EUINEO::Anchor::Center)
    .position(-180.0f, -120.0f)
    .size(360.0f, 240.0f)
    .background(EUINEO::Color(0.14f, 0.14f, 0.18f, 1.0f))
    .rounding(16.0f)
    .build();
```

效果：以屏幕中心为基准，把卡片左上角放到中心点左上方，实现居中面板。
