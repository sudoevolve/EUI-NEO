# EUI Docs

This directory is intentionally small.

Current documents:

1. `quick-ui-tutorial.zh-CN.md`
2. `project-structure.zh-CN.md`
3. `README.md`

Recommended reading order:

1. `quick-ui-tutorial.zh-CN.md`
2. `project-structure.zh-CN.md`

Current repo notes:

- The public renderer route stays `EUI_BACKEND_OPENGL`.
- `GLFW` and `SDL2` share the same OpenGL renderer path.
- Runtime image rendering is available through `ui.image(...)` and textured fills such as `fill_image(...)`.
- Renderer-side image loading and cache cleanup live in `include/eui/app/detail/opengl_renderer_detail.inl`.
- `Vulkan` is still not implemented.

There is currently no separate roadmap/status document under `docs/`.
The source of truth is the code under `include/`, plus the examples under `examples/`.
