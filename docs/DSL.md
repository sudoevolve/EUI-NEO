# DSL 设计与当前实现

这版 DSL 的边界已经调整过：`core::dsl` 只描述基础图元和布局，不再把 `Button`、`Panel` 这类组件塞进 core 枚举里。

当前核心枚举是：

```cpp
enum class ElementKind {
    Row,
    Column,
    Stack,
    Rect,
    Text
};
```

含义：

- `Row / Column / Stack`：布局节点
- `Rect`：基础矩形图元，包含颜色、渐变、圆角、边框、阴影、透明度、变换、交互状态
- `Text`：基础文本图元，使用 SDF 字体渲染

组件不属于 core 图元。组件应该在 `components` 层用基础图元组合出来，比如现在的按钮 DSL 就是 `components::button(ui, id)`，内部用 `Stack + Rect + Text` 组成。

## 1. 目标写法

app 侧应该接近 EUI：

```cpp
runtime.compose("demo", logicalWidth, logicalHeight, [&](core::dsl::Ui& ui, const core::dsl::Screen& screen) {
    ui.stack("root")
        .size(screen.width, screen.height)
        .align(core::Align::CENTER, core::Align::CENTER)
        .content([&] {
            ui.rect("card")
                .size(360.0f, 260.0f)
                .radius(18.0f)
                .gradient({0.10f, 0.12f, 0.16f, 1.0f}, {0.05f, 0.07f, 0.10f, 1.0f})
                .build();

            components::button(ui, "primary")
                .size(240.0f, 70.0f)
                .text("Click Me")
                .onClick([] {
                    // click
                })
                .build();
        });
});
```

demo 里不应该再写：

- 手动创建 `Panel/Text/Button` 实例
- 手动 `applyPanel/applyText/applyButton`
- 手动把 layout frame 同步到组件
- 手动处理每个组件的初始化和销毁

这些已经交给 `core::dsl::Runtime`。

## 2. Runtime 职责

文件：

```text
core/dsl_runtime.h
```

核心职责：

- 持有 `core::dsl::Ui`
- 每帧 compose 页面声明
- 调用 `ui.layout()` 计算逻辑坐标
- 维护图元实例缓存
- 把 `Rect` 同步到 `core::RoundedRectPrimitive`
- 把 `Text` 同步到 `core::TextPrimitive`
- 统一处理 DPI：逻辑尺寸转 framebuffer 像素
- 统一处理 `Rect` 交互：hover / pressed / click
- 统一判断是否仍在交互动画中
- 统一 render / shutdown

app 生命周期现在只需要：

```cpp
#include "app/dsl_app.h"

namespace app {

const DslAppConfig& dslAppConfig();
void compose(core::dsl::Ui& ui, const core::dsl::Screen& screen);

} // namespace app
```

`app/dsl_app.h` 已经把 `initialize / update / isAnimating / render / shutdown` 收进去了，demo 不需要再重复写这些样板。

## 3. 基础布局 DSL

容器：

```cpp
ui.row("toolbar")
ui.column("content")
ui.stack("root")
```

通用布局属性：

```cpp
.position(x, y)
.x(value)
.y(value)
.width(value)
.height(value)
.size(width, height)
.width(core::SizeValue::fill())
.height(core::SizeValue::wrapContent())
.fill()
.wrapContent()
.margin(value)
.margin(horizontal, vertical)
.margin(left, top, right, bottom)
```

容器对齐：

```cpp
.gap(value)
.spacing(value)
.justifyContent(core::Align::CENTER)
.alignItems(core::Align::CENTER)
.align(core::Align::CENTER, core::Align::CENTER)
```

规则：

- `Row`：主轴是横向，`alignItems` 控制垂直方向
- `Column`：主轴是纵向，`alignItems` 控制水平方向
- `Stack`：子节点叠放，按声明顺序绘制

## 4. Rect 图元

`Rect` 是 GUI 里最基础的视觉图元：

```cpp
ui.rect("card")
    .size(360.0f, 260.0f)
    .color({0.10f, 0.12f, 0.16f, 1.0f})
    .radius(18.0f)
    .border(1.0f, {0.23f, 0.29f, 0.38f, 1.0f})
    .shadow(26.0f, 0.0f, 8.0f, {0, 0, 0, 0.26f})
    .build();
```

支持：

```cpp
.color(...)
.background(...)
.gradient(...)
.radius(...)
.border(...)
.shadow(...)
.opacity(...)
.translate(...)
.scale(...)
.rotate(...)
.transformOrigin(...)
```

交互也是 `Rect` 的基础能力，不是 Button 专属：

```cpp
ui.rect("hit.area")
    .size(240.0f, 70.0f)
    .states(normal, hover, pressed)
    .onClick([] {
        // click
    })
    .build();
```

## 5. Text 图元

```cpp
ui.text("title")
    .size(300.0f, 38.0f)
    .text("Light GUI")
    .fontSize(30.0f)
    .fontWeight(400)
    .color({0.94f, 0.97f, 1.0f, 1.0f})
    .horizontalAlign(core::HorizontalAlign::Center)
    .verticalAlign(core::VerticalAlign::Top)
    .lineHeight(38.0f)
    .build();
```

支持：

```cpp
.text(...)
.fontFamily(...)
.fontSize(...)
.fontWeight(...)
.color(...)
.maxWidth(...)
.wrap(...)
.horizontalAlign(...)
.verticalAlign(...)
.lineHeight(...)
```

`ui.label(id)` 仍然保留为 `ui.text(id)` 的别名。

## 6. 组件应该怎么写

组件层应该是“组合基础图元”，而不是让 core DSL 认识每一种组件。

现在按钮在 `components/button.h` 里提供了一个 DSL builder：

```cpp
components::button(ui, "primary")
    .size(240.0f, 70.0f)
    .text("Click Me")
    .build();
```

所以 app 写：

```cpp
components::button(ui, "primary")
    .size(240.0f, 70.0f)
    .text("Click Me")
    .onClick(...)
    .build();
```

这个组件内部会生成：

- `id`：Stack 布局节点
- `id.bg`：可交互 Rect 背景
- `id.text`：居中的 Text 文本

这就是“组件自动继承基础图元能力”的方向：组件不是 core 枚举，而是用基础图元组合。后续可以继续把常用图元属性透传到组件 builder 上。

后续如果做 `input / checkbox / slider / panel`，也应该优先用这种方式组合：

- 简单组件：返回某个基础图元 builder
- 复杂组件：提供 `Compose(ui, id, ...)`，内部创建多个 `Rect/Text`
- 稳定之后再考虑给 `Ui` 增加别名

## 7. 当前限制

当前 runtime 还是轻量版：

- 还没有真正 diff，只是按 id 缓存图元实例
- 旧 id 移除后实例暂时保留在缓存里，但不会再绘制
- 交互 hit-test 目前只对 `Rect` 生效
- 组件还没有独立状态对象
- 还没有 clip / scroll / z-index
- 还没有全局主题系统

但 app 侧写法已经从“手动同步组件”改成了“声明 UI + runtime 执行”，这才是继续往 EUI 那种结构走的正确方向。
