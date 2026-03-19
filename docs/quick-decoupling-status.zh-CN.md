# EUI Quick 解耦现状

这份文档描述当前仓库里“quick 语法糖”和底层框架的实际边界。
它不是提案文档，而是当前代码状态的收口说明。

## 当前结论

- 旧的公共布局入口已经移除。
- 旧的公共兼容入口已经移除。
- 当前唯一保留并继续扩展的上层写法是 `eui::quick::UI`。
- `quick` 不是旧接口的临时包装层，而是正式公共 API。

## 当前公共 API 边界

对外推荐使用：

```cpp
#include "EUI.h"

eui::Context ctx;
eui::quick::UI ui(ctx);
```

公开布局能力以 quick 为准：

- `panel()` / `card()`
- `scope()` / `stack()` / `clip()`
- `row().items(...)`
- `anchor()`
- `dock_top()` / `dock_bottom()` / `dock_left()` / `dock_right()`
- `split_h_ratio()` / `split_v_ratio()`

公开控件能力也以 quick builder 为准：

- `button()` / `input()` / `readonly()`
- `metric()` / `progress()`
- `text()` / `icon()` / `shape()` / `image()`

## 已移除的旧公共语法

以下方向已经不再作为公共 API 保留：

- `eui/layout.h`
- `eui/compat.h`
- 旧的 panel/card/row/columns/waterfall 容器入口
- 旧的“quick 到旧接口”的对照或映射关系文档

换句话说，现在对外不再存在“两套布局语法并行”的状态。

## 当前内部实现边界

目前 quick 层仍然可以复用内部细节，但这些细节不再作为公共头暴露。

当前 quick 内部落点主要是：

```text
include/eui/quick/detail/anchor_spec.h
include/eui/quick/detail/primitive_painter.h
```

这些文件属于 quick 自己的内部实现，不属于对外稳定 API。

原则是：

- quick 可以复用内部细节
- core 不为旧语法保留公共入口
- 外部调用方不应依赖 `quick/detail/*`

## 示例与文档规则

当前仓库的对外展示规则已经调整为：

- README 只展示 quick 写法
- 示例文件名直接对应示例目标名与可执行文件名

也就是说，文档和示例不再继续教育用户旧布局写法。

## 还没有完全解耦的部分

“语法糖解耦”已经基本完成，但“整套框架彻底解耦”还没有全部结束。

当前仍然属于后续工作范围的部分：

- runtime 与平台窗口层的彻底拆分
- renderer 与具体 OpenGL 路径的彻底拆分
- `EUI.h` 作为总入口时的进一步瘦身
- backend capability 与 app runner 边界的继续整理

这部分请继续参考：

- `backend-abstraction-issue.md`
- `backend-abstraction-checklist.md`

## 建议的阅读顺序

如果你要理解当前仓库，请按这个顺序看：

1. `readme.zh-CN.md`
2. `quick-decoupling-status.zh-CN.md`
3. `concise-ui-syntax-proposal.zh-CN.md`
4. `framework-revamp-proposal.zh-CN.md`
5. `examples/`

如果你要继续推进 backend/runtime 分层，再看：

1. `backend-abstraction-issue.md`
2. `backend-abstraction-checklist.md`
