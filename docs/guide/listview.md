# ListView 指南

## 作用

`ListView::Compose` 提供虚拟列表渲染能力，只绘制可视范围附近条目，适合大数据量场景。

## 基础用法

```cpp
#include "components/ListView.h"

const float itemHeight = 48.0f;
const int itemCount = 1000000;

EUINEO::ListView::Compose(ui, "demo.list", x, y, width, height, itemCount, itemHeight,
    [&](int index, float itemY) {
        ui.panel("item.bg." + std::to_string(index))
            .position(x, itemY)
            .size(width - 10.0f, itemHeight - 4.0f)
            .background(EUINEO::Color(0.15f, 0.15f, 0.18f, 1.0f))
            .rounding(8.0f)
            .build();

        ui.label("item.text." + std::to_string(index))
            .position(x + 16.0f, itemY + 30.0f)
            .text("List Item #" + std::to_string(index + 1))
            .fontSize(16.0f)
            .color(EUINEO::Color(0.82f, 0.82f, 0.82f, 1.0f))
            .build();
    });
```

## 常用页面写法

```cpp
const float headerHeight = 72.0f;
const float listX = 20.0f;
const float listY = headerHeight + 12.0f;
const float listWidth = screen.width - 40.0f;
const float listHeight = screen.height - listY - 20.0f;
const float itemHeight = 48.0f;

ui.panel("list.header")
    .position(0.0f, 0.0f)
    .size(screen.width, headerHeight)
    .background(EUINEO::Color(0.12f, 0.12f, 0.14f, 1.0f))
    .build();

ui.label("list.header.title")
    .position(20.0f, 46.0f)
    .text("Virtual List")
    .fontSize(22.0f)
    .color(EUINEO::Color(1.0f, 1.0f, 1.0f, 1.0f))
    .build();

EUINEO::ListView::Compose(ui, "demo.list", listX, listY, listWidth, listHeight, 1000000, itemHeight,
    [&](int index, float itemY) {
        ui.panel("item.bg." + std::to_string(index))
            .position(listX, itemY)
            .size(listWidth - 10.0f, itemHeight - 4.0f)
            .background(index % 2 == 0
                ? EUINEO::Color(0.15f, 0.15f, 0.18f, 1.0f)
                : EUINEO::Color(0.13f, 0.13f, 0.16f, 1.0f))
            .rounding(8.0f)
            .build();

        ui.label("item.text." + std::to_string(index))
            .position(listX + 16.0f, itemY + 30.0f)
            .text("List Item #" + std::to_string(index + 1))
            .fontSize(16.0f)
            .color(EUINEO::Color(0.82f, 0.82f, 0.82f, 1.0f))
            .build();
    });
```

## 参数说明

- `id`：滚动容器唯一键
- `x/y/width/height`：可视区域
- `itemCount`：总条目数
- `itemHeight`：固定条目高度
- `composeItem`：条目渲染回调，支持 `(int index)` 或 `(int index, float itemY)`
- `scrollStep`：滚轮步进，默认 `48.0f`

## 实战建议

- `id` 在页面内保持稳定且唯一
- 条目内容不要做重型阻塞计算
- 优先固定条目高度，利于可视区索引计算
- 条目节点 key 建议带上 `index`，避免状态串扰
- 如果需要点击条目，优先把点击区域做成 `button` 或 `panel + onClick`
- 头部区域与列表区域分离，避免滚动区覆盖标题交互

## 参考示例

- `app/listview_demo.cpp`
- `src/components/ListView.h`
