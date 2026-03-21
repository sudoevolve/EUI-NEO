# EUI 项目结构

最后更新：2026-03-21

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
- `docs/quick-ui-tutorial.zh-CN.md`
  - Quick UI、布局、锚点、动画、图片与纹理填充的上手说明。
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
  - 窗口运行时、OpenGL renderer、字体渲染、dirty cache、图片纹理加载与缓存。
- `include/eui/core/`
  - `Context` 状态、文本测量、基础工具。
- `include/eui/graphics/`
  - 图元、效果、变换描述，包含图片图元和图元纹理填充所需的数据结构。
- `include/eui/quick/detail/`
  - quick builder 的内部实现。
- `include/eui/runtime/`
  - 运行时契约与帧上下文。

## 当前渲染相关关键文件

- `include/eui/app/detail/modern_gl_detail.inl`
  - shader、VBO、proc loader 与共享 GL 提交工具。
- `include/eui/app/detail/opengl_renderer_detail.inl`
  - 当前统一 OpenGL renderer 主实现，包含图片解码上传、纹理缓存与回收。
- `include/eui/app/detail/font_renderers_detail.inl`
  - STB 文本主路径、Windows fallback、bitmap fallback。
- `include/eui/app/detail/glfw_run_loop_detail.inl`
  - GLFW 窗口循环。
- `include/eui/app/detail/sdl2_run_loop_detail.inl`
  - SDL2 窗口循环。
- `include/eui/app/detail/dirty_cache_detail.inl`
  - dirty-region cache 与缓存纹理回放。
- `include/eui/quick/ui.h`
  - Quick builder 公开 API，包括 `ui.image(...)`、`fill_image(...)`、锚点和布局接口。
- `include/eui/graphics/primitives.h`
  - 基础图元定义，包括图片图元和矩形纹理填充字段。

## examples/

当前示例目标名直接跟随 `examples/*.cpp` 文件名：

- `examples/minimal_quick_demo.cpp`
  - 最小可运行 Quick UI 入口、基础布局和常用控件。
- `examples/anchor_and_position_demo.cpp`
  - 锚点定位、`in(Rect{...})`、`at()+size()` 的直接对照示例。
- `examples/image_texture_demo.cpp`
  - `ui.image(...)`、`fill_image(...)` 与 `cover / contain / stretch / center` 演示。
- `examples/EUI_gallery.cpp`
  - 综合展示页，包含 Image 页面和 About 头像示例。
- `examples/financial_assistant_dashboard_demo.cpp`
  - 较完整的金融仪表盘页面示例。
- `examples/reference_dashboard_demo.cpp`
  - 参考风格的多区块 dashboard 示例。

示例相关资源目前也放在 `examples/` 或 `preview/`：

- `examples/avtar.jpg`
  - Gallery About 页头像资源。
- `examples/bg.jpg`
  - 示例背景图资源。
- `examples/EUI_gallery_icons.json`
  - Gallery 图标数据。
- `preview/`
  - 预览图片和图片纹理演示使用的素材。

示例通过文件内宏选择平台与后端，不要求用户另外维护复杂构建配置。

## 当前结构上的结论

- 窗口管理已经从单一大文件里拆出。
- 渲染主线已经收敛到 modern GL。
- `GLFW` / `SDL2` 是平台层差异，不再各自维护一套 renderer。
- 图片显示已经走统一 OpenGL 渲染路径，不再需要示例层自己处理纹理上传。
- `GLES` 是 OpenGL 主线下的兼容分支。
- `Vulkan` 仍然未进入当前稳定实现范围。
