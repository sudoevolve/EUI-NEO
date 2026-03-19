# EUI 现代 GL 优先改造路线

## 目的

窗口管理已经从具体渲染实现里拆出来了，下一步不建议直接跳到 Vulkan。

更稳的路线是：

1. 先把当前 legacy / fixed-function OpenGL 路径改成 modern OpenGL。
2. 先在 `GLFW + OpenGL` 和 `SDL2 + OpenGL` 上把这条新主线跑稳。
3. 再从这条 modern GL 路线分出 `GLES` 支持。
4. 最后再接 `Vulkan`。

这份文档的目标，是把后端改造顺序明确下来，避免后面一边补 SDL / GLFW / DPI / 文本，一边继续加深旧 OpenGL 技术债。

## 当前判断

当前仓库已经有两块对后续改造有帮助的基础：

- 运行时契约已经拆出来：
  - [`include/eui/runtime/contracts.h`](../include/eui/runtime/contracts.h)
  - [`include/eui/runtime/frame_context.h`](../include/eui/runtime/frame_context.h)
- 渲染后端契约已经有了一个很薄的雏形：
  - [`include/eui/renderer/contracts.h`](../include/eui/renderer/contracts.h)
  - [`include/eui/renderer/draw_data_adapter.h`](../include/eui/renderer/draw_data_adapter.h)

但当前真正跑起来的图形路径，仍然是桌面 OpenGL 旧管线：

- [`include/eui/app/detail/opengl_renderer_detail.inl`](../include/eui/app/detail/opengl_renderer_detail.inl)
  - 直接使用 `glMatrixMode` / `glLoadIdentity` / `glOrtho`
  - 直接使用 `glBegin` / `glEnd`
  - 直接使用 client-side array
- [`include/eui/app/detail/font_renderers_detail.inl`](../include/eui/app/detail/font_renderers_detail.inl)
  - 仍有 display list / fixed pipeline 风格路径
  - 纹理上传仍基于较旧的 GL 纹理格式使用方式
- [`include/EUI.h`](../include/EUI.h)
  - `EUI_BACKEND_VULKAN` 目前仍是未实现占位

换句话说：

- 现在的“窗口后端解耦”已经有了。
- 现在的“渲染后端真正可迁移”还没有完成。
- 如果后面确定要支持 `GLES` 和 `Vulkan`，当前这套 legacy OpenGL 不能继续作为长期主线。

## 为什么先做现代 GL

### 1. 现代 GL 是当前最小风险的下一步

直接上 Vulkan，改动面会同时覆盖：

- 渲染提交模型
- 资源生命周期
- 文本纹理上传
- 裁剪和脏区缓存
- 平台 surface 创建
- 同步与 present 责任边界

这会把“窗口解耦是否合理”和“Vulkan 后端是否实现正确”两个问题叠在一起，排查成本太高。

先做 modern GL，可以先只解决一件事：

- 把当前 `DrawCommand -> OpenGL fixed pipeline` 改成 `DrawCommand -> GPU buffer + shader pipeline`

这样更容易验证抽象边界到底对不对。

### 2. 现代 GL 是 GLES 的直接前置步骤

当前 renderer 里大量使用的这些旧接口，并不是 GLES 的自然兼容子集：

- fixed-function matrix state
- immediate mode
- display list
- `GL_QUADS`
- client-side vertex array

如果目标里明确包含 `GLES`，那 modern GL 并不是“可选优化”，而是必须先完成的迁移。

### 3. 现代 GL 的思维模型更接近 Vulkan

先完成这些事情：

- CPU 侧几何整理
- 顶点 / 索引缓冲上传
- shader 驱动的绘制
- 纹理 atlas 资源管理
- renderer capability 抽象

后面再做 Vulkan 时，虽然 API 会完全不同，但整体数据流会更接近，不需要再从 fixed pipeline 直接跨到 command buffer / pipeline state。

### 4. 可以保留当前对外宏入口，不增加新的公开复杂度

用户侧目前已经切到这种使用方式：

```cpp
#define EUI_BACKEND_OPENGL
#define EUI_PLATFORM_GLFW
#include "EUI.h"
```

或：

```cpp
#define EUI_BACKEND_OPENGL
#define EUI_PLATFORM_SDL2
#include "EUI.h"
```

这层公开入口不要再继续膨胀。

建议是：

- `EUI_BACKEND_OPENGL` 对外仍然只表示“OpenGL 路线”
- modern / legacy 的内部差异不新增公开宏
- 如确实需要保留旧路径，也只保留内部临时调试开关，不作为长期公开 API

## 路线选择结论

建议把后端演进顺序固定为：

1. 现代 GL 主线
2. GLES 复用这条主线
3. Vulkan 最后接入

不建议的顺序：

1. 继续堆当前 legacy OpenGL
2. 临时补 GLES 宏兼容
3. 再尝试 Vulkan

这种顺序只会让 renderer 内部越来越分裂。

## 本阶段范围

本阶段只做这些事：

- 把当前 OpenGL 主路径改成 modern GL
- 保持 `GLFW + OpenGL` 可运行
- 保持 `SDL2 + OpenGL` 可运行
- 保持现有 `DrawCommand` / `Context` / `quick` API 不动
- 保持 `stb truetype` 作为主文本路径

本阶段不做这些事：

- 不直接实现 Vulkan backend
- 不直接对外暴露新的 Vulkan 宏行为
- 不优先实现平台专用文本路径作为主路径
- 不为了 GLES 兼容继续保留 fixed-function 作为长期默认方案

## 文本路线约束

文本优先级建议明确固定为：

1. `stb_truetype`
2. Win32 / native 文本能力仅作兜底
3. 内置 bitmap 只作最后 fallback

也就是说：

- modern GL 路线里，字体与图标都优先走 atlas + shader 绘制
- Win32 文本不再作为主绘制路径，只保留 fallback 角色
- 这条规则要同时服务后续的 `GLES` 和 `Vulkan`

## 目标架构

建议把渲染路径拆成四层：

### 1. Core UI

负责：

- `Context`
- `DrawCommand`
- layout
- widgets
- 文本编辑逻辑

不负责：

- 窗口创建
- OpenGL 上下文
- Vulkan surface
- GPU 资源

### 2. Platform Backend

负责：

- 窗口生命周期
- 事件轮询
- 输入翻译
- clipboard
- DPI / framebuffer metrics
- time / frame clock

当前已经有初步契约：

- [`include/eui/runtime/contracts.h`](../include/eui/runtime/contracts.h)

### 3. Renderer Backend

负责：

- 把 `DrawCommand` 转成 GPU 可提交的数据
- atlas 纹理管理
- scissor / clip
- draw call 提交
- 可选的局部刷新能力

当前已经有初步契约：

- [`include/eui/renderer/contracts.h`](../include/eui/renderer/contracts.h)

### 4. App Runner

负责把前面三层拼起来：

- platform backend
- renderer backend
- `Context`
- 用户回调

## modern GL 目标形态

modern GL 完成后，主路径应至少具备这些特征：

- 不再依赖 immediate mode
- 不再依赖 fixed-function matrix state
- 不再依赖 display list
- 不再依赖 `GL_QUADS`
- 不再依赖 client-side array
- 统一改成：
  - CPU 侧组装顶点 / 索引
  - VBO / IBO 上传
  - shader program 绘制
  - atlas 纹理采样
  - scissor 裁剪

建议桌面主目标先定为：

- Desktop OpenGL 3.3 Core

这样后面转到：

- GLES 3.x

会自然很多。

如果必须考虑更老环境，也应该让旧路径变成临时兼容后备，而不是继续作为主实现。

## 分阶段计划

### 阶段 1：冻结当前 legacy OpenGL 路径

目标：

- 停止继续往当前 fixed-function renderer 加新能力
- 明确它只是过渡路径

建议动作：

- 将当前 renderer 标记为 legacy backend
- 新能力优先进入 modern GL 设计，不再继续堆到旧 renderer

### 阶段 2：补齐 renderer 内部数据流

目标：

- 让 `DrawCommand` 不再直接在 renderer 中一边遍历一边临时发 GL 命令

建议动作：

- 新增 CPU 侧绘制中间层，例如：
  - draw packet
  - mesh batch
  - glyph batch
- 先把这些中间数据结构做出来
- 让后续 modern GL / GLES / Vulkan 都能共用这层 CPU 预处理结果

### 阶段 3：现代 GL 资源层

目标：

- 把 GPU 资源生命周期从现有实现里独立出来

建议动作：

- shader program 封装
- vertex / index buffer 封装
- atlas texture 封装
- uniform / push data 组织
- renderer capability 定义

建议重点：

- 不要把“脏区缓存”继续写死为 OpenGL framebuffer copy 假设
- 应把它变成 renderer capability

### 阶段 4：文本绘制统一到 atlas + shader

目标：

- 让文本绘制真正跨 renderer backend

建议动作：

- `stb_truetype` 输出 glyph bitmap
- 统一上传到 atlas
- 文本绘制统一走 quad mesh + shader
- Win32 / native 文本只作为 fallback 字形来源或最后兜底方案

### 阶段 5：让 GLFW / SDL2 都切到 modern GL 主路径

目标：

- 同一套 modern renderer 同时跑在两种窗口后端上

验收重点：

- `GLFW + OpenGL`
- `SDL2 + OpenGL`

都能跑通现有示例。

### 阶段 6：从 modern GL 派生 GLES

目标：

- 基于同一套 mesh / atlas / shader 组织方式接入 GLES

这一步才处理：

- GLES context 创建
- GLES 纹理格式差异
- GLES shader 版本差异
- GLES 状态兼容问题

### 阶段 7：最后接 Vulkan

目标：

- 基于已经稳定的 CPU 侧提交模型实现 Vulkan renderer

这一步才处理：

- surface / swapchain
- descriptor / pipeline
- command buffer
- Vulkan 专用 partial redraw 与资源同步策略

## 文件组织建议

建议后面把实现逐步收敛到这些位置：

```text
include/eui/renderer/
  contracts.h
  draw_data_adapter.h
  capabilities.h
  mesh.h
  atlas.h

include/eui/app/detail/
  legacy_opengl_renderer_detail.inl
  modern_opengl_renderer_detail.inl
  renderer_common_detail.inl
```

如果后面再继续拆，可以再进一步演进到：

```text
include/eui/renderer/opengl/
include/eui/renderer/gles/
include/eui/renderer/vulkan/
```

但在 modern GL 还没跑稳之前，不建议一次性把目录拆太大。

## 对外接口原则

对用户保持简单：

- 平台仍由源码宏控制：
  - `EUI_PLATFORM_GLFW`
  - `EUI_PLATFORM_SDL2`
- 渲染后端仍由源码宏控制：
  - `EUI_BACKEND_OPENGL`
  - `EUI_BACKEND_VULKAN` 继续保留占位，暂不实现行为

不建议现在公开：

- `EUI_BACKEND_MODERN_GL`
- `EUI_BACKEND_LEGACY_GL`
- 一堆 renderer 内部调试宏

原因很简单：

- 用户想要的是“选 OpenGL 还是 Vulkan”
- 不是“还要继续理解 EUI 自己内部的渲染代际”

## 验收标准

modern GL 第一阶段完成时，至少应满足：

- 现有 examples 在 `GLFW + OpenGL` 下可运行
- 现有 examples 在 `SDL2 + OpenGL` 下可运行
- 默认文本主路径为 `stb truetype`
- `Win32/native` 与 bitmap 只作为 fallback
- 新主路径里不再出现 fixed-function immediate mode 依赖
- 后续接 `GLES` 时不需要再回头先推翻 modern GL 设计
- 后续接 `Vulkan` 时可以直接复用 CPU 侧绘制整理结果

## 最终结论

当前阶段最合理的主线不是：

- 直接做 Vulkan

而是：

- 先把当前 OpenGL renderer 改成 modern GL
- 用这条 modern GL 路线统一 SDL2 / GLFW
- 再从这条路派生 GLES
- 最后再落 Vulkan

简单说就是一句话：

**先把 OpenGL 做现代化，再谈 GLES 和 Vulkan。**
