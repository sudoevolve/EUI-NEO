# 基础图元

## 常用图元与用途

- `panel`：矩形/圆角矩形容器，最常用
- `glassPanel`：带玻璃效果的面板
- `label`：文本显示
- `button`：点击交互
- `polygon`：三角形/多边形自定义形状
- `image`：本地或网络图片显示

## 面板 + 文本最小组合

```cpp
ui.panel("card")
    .position(24.0f, 24.0f)
    .size(280.0f, 120.0f)
    .background(EUINEO::Color(0.14f, 0.14f, 0.18f, 1.0f))
    .rounding(12.0f)
    .border(1.0f, EUINEO::Color(0.24f, 0.24f, 0.30f, 1.0f))
    .build();

ui.label("card.title")
    .position(40.0f, 70.0f)
    .text("Card Title")
    .fontSize(18.0f)
    .color(EUINEO::Color(0.90f, 0.90f, 0.95f, 1.0f))
    .build();
```

## 按钮点击

```cpp
ui.button("action.submit")
    .position(24.0f, 170.0f)
    .size(140.0f, 40.0f)
    .text("提交")
    .onClick([]() {
        EUINEO::OpenDslUrl("https://github.com");
    })
    .build();
```

## 图片加载（本地 / 网络 / Bing）

```cpp
ui.image("photo.local")
    .position(24.0f, 230.0f)
    .size(220.0f, 140.0f)
    .path("assets/demo.jpg")
    .build();

ui.image("photo.remote")
    .position(260.0f, 230.0f)
    .size(220.0f, 140.0f)
    .url("https://picsum.photos/440/280")
    .build();

ui.image("photo.bing")
    .position(496.0f, 230.0f)
    .size(220.0f, 140.0f)
    .bingDaily(0, "zh-CN")
    .build();
```

## 多边形

```cpp
ui.polygon("triangle")
    .position(24.0f, 390.0f)
    .size(72.0f, 64.0f)
    .background(CurrentTheme->primary)
    .points({
        Point2{0.50f, 0.00f},
        Point2{1.00f, 1.00f},
        Point2{0.00f, 1.00f},
    })
    .build();
```
