# EUI Quick 语法说明

这份文档描述当前保留的 quick 语法方向。旧语法糖提案已经结束，旧公共 API 已全部下线。
如果你想确认当前仓库已经完成了哪些解耦，请结合 `quick-decoupling-status.zh-CN.md` 一起看。

## 保留的写法

```cpp
eui::quick::UI ui(ctx);
```

围绕这一个入口继续扩展：

- `panel()` / `card()`
- `row().items(...)`
- `scope()` / `stack()` / `clip()`
- `anchor()`
- `shape()` / `text()` / `icon()` / `image()`
- `button()` / `input()` / `readonly()` / `metric()` / `progress()`

## 宽度模型

```cpp
ui.row()
    .items({
        ui.fixed(120.0f),
        ui.flex(1.0f),
        ui.content(140.0f, 220.0f),
    })
    .gap(10.0f)
    .align_center()
    .begin([&] {
        ui.button("Back").ghost().draw();
        ui.label("Adaptive center area").draw();
        ui.button("Apply").primary().draw();
    });
```

## 容器模型

```cpp
ui.panel("Shell")
    .in(frame_rect)
    .padding(16.0f)
    .begin([&](auto& root) {
        const Rect sidebar = root.dock_left(220.0f, 16.0f);
        const Rect content = root.content();

        ui.card("Navigation").in(sidebar).begin([&](auto&) {
            ui.button("Overview").ghost().draw();
            ui.button("Settings").ghost().draw();
        });

        ui.card("Content").in(content).begin([&](auto&) {
            ui.label("Quick-only layout flow").draw();
        });
    });
```

## 锚点模型

```cpp
ui.metric("Build", "Ready")
    .in(rect)
    .draw();

const Rect badge = ui.anchor()
    .to_last()
    .top(12.0f)
    .right(12.0f)
    .size(72.0f, 24.0f)
    .resolve();

ui.shape()
    .in(badge)
    .radius(12.0f)
    .fill(0x16A34A)
    .draw();
```

## 说明

- 不再保留旧容器入口的对外语法说明。
- 不再保留 quick builder 到旧接口的映射关系文档。
- quick builder 现在就是正式 API，不再是提案层包装。
