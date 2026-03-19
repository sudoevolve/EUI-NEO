# EUI 项目结构

最后更新：2026-03-19

这份文档只描述当前仍然有维护价值的目录和职责。

## 顶层目录

```text
EUI/
  cmake/
  docs/
  examples/
  include/
  preview/
  CMakeLists.txt
  readme.md
  readme.zh-CN.md
```

## docs/

当前只保留三份文档：

- `docs/README.md`
  - 文档入口与阅读顺序。
- `docs/modern-gl-roadmap.zh-CN.md`
  - modern GL 主线、GLES 兼容和 Vulkan 占位状态。
- `docs/project-structure.zh-CN.md`
  - 当前目录职责说明。

## include/

### 公开入口

- `include/EUI.h`
  - 总入口头文件。
  - 负责公开 API 聚合和平台/后端宏接入。

### 核心分层

- `include/eui/core.h`
- `include/eui/app.h`
- `include/eui/graphics.h`
- `include/eui/renderer.h`
- `include/eui/runtime.h`
- `include/eui/quick.h`

### 关键实现目录

- `include/eui/app/detail/`
  - 窗口运行时、OpenGL renderer、字体渲染、dirty cache。
- `include/eui/core/`
  - `Context` 状态、文本测量、基础工具。
- `include/eui/graphics/`
  - 图元、效果、变换描述。
- `include/eui/quick/detail/`
  - quick builder 的内部实现。
- `include/eui/runtime/`
  - 运行时契约与帧上下文。

## 当前渲染相关关键文件

- `include/eui/app/detail/modern_gl_detail.inl`
  - shader、VBO、proc loader 与共享 GL 提交工具。
- `include/eui/app/detail/opengl_renderer_detail.inl`
  - 当前统一 OpenGL renderer 主实现。
- `include/eui/app/detail/font_renderers_detail.inl`
  - STB 文本主路径、Windows fallback、bitmap fallback。
- `include/eui/app/detail/glfw_run_loop_detail.inl`
  - GLFW 窗口循环。
- `include/eui/app/detail/sdl2_run_loop_detail.inl`
  - SDL2 窗口循环。
- `include/eui/app/detail/dirty_cache_detail.inl`
  - dirty-region cache 与缓存纹理回放。

## examples/

当前示例文件名直接对应可执行文件名：

- `anchor_layout_demo.cpp`
- `basic_widgets_demo.cpp`
- `calculator_demo.cpp`
- `graphics_showcase_demo.cpp`
- `sidebar_navigation_demo.cpp`

示例通过文件内宏选择平台与后端，不要求用户另外维护复杂构建配置。

## 当前结构上的结论

- 窗口管理已经从单一大文件里拆出。
- 渲染主线已经收敛到 modern GL。
- `GLFW` / `SDL2` 是平台层差异，不再各自维护一套 renderer。
- `GLES` 是 OpenGL 主线下的兼容分支。
- `Vulkan` 仍然未进入当前稳定实现范围。
