# EUI 框架改造说明

这份文档用于记录当前框架分层的最终方向，而不是保留旧过渡方案。
如果你想看“当前代码已经落地到什么程度”，请优先看 `quick-decoupling-status.zh-CN.md`。

## 当前结论

- 旧的布局与兼容公共头已经移除。
- 旧的即时布局公共语法已经移除。
- 当前唯一保留并继续演进的上层写法是 `eui::quick::UI`。

## 当前分层

```text
include/
  EUI.h
  eui/
    animation/
    app/
    core/
    graphics/
    quick/
    renderer/
    runtime/
```

## 设计原则

- `EUI.h` 继续作为总入口。
- `eui::quick::UI` 是默认推荐写法。
- 示例、README 和对外文档只展示 quick 语法。
- 旧布局能力如果仍被 quick 层复用，只允许作为内部实现细节存在，不再作为公共 API 暴露。

## 推荐使用方式

```cpp
eui::quick::UI ui(ctx);

ui.panel("Dashboard")
    .in(frame_rect)
    .padding(16.0f)
    .begin([&](auto& root) {
        const auto columns = root.split_h_ratio(root.content(), 0.60f, 12.0f);
        ui.card("Overview").in(columns.first).begin([&](auto&) {
            ui.label("Quick API only").draw();
        });
        ui.card("Inspector").in(columns.second).begin([&](auto&) {
            ui.readonly("Status", "Ready").draw();
        });
    });
```

## 后续原则

- 新能力优先补到 quick builder 层。
- 示例文件名直接决定目标名与可执行文件名。
- 不再新增任何旧即时布局风格的公共 API。
