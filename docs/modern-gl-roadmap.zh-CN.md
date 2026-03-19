# EUI 现代 GL 主线

最后更新：2026-03-19

## 结论

当前仓库的图形主线已经收敛为：

1. `EUI_BACKEND_OPENGL` 表示统一的 OpenGL 路线。
2. 默认桌面路径走 modern GL，而不是旧 fixed-function 主路径。
3. `GLFW` 和 `SDL2` 共享同一套 renderer 实现。
4. `GLES` 作为这条 OpenGL 主线下的兼容分支处理。
5. `Vulkan` 继续保留占位，不在当前阶段实现。

## 对外入口

示例侧仍然保持代码内宏选择，不引入额外的构建配置心智负担：

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

如需切到 OpenGL ES 兼容上下文，可额外定义：

```cpp
#define EUI_OPENGL_ES
```

## 当前已经落实的方向

- 主绘制路径已经改成 shader + VBO + batched vertices。
- 文本主路径优先 `stb_truetype`。
- Windows 下的 Win32/native 文本只保留为非 ES 桌面路径的兜底方案。
- dirty cache 与 backdrop blur 已经接入 modern GL 提交路径。
- `GLFW` 和 `SDL2` 都会为 modern GL 设置 proc loader。

## Desktop GL 与 GLES 的当前差异

- Desktop GL:
  - 描边和 chevron 继续用 `GL_LINES`，优先维持更低顶点量和更低内存占用。
- GLES:
  - 描边和 chevron 改用三角形厚线，避免依赖 `glLineWidth` 的兼容性。
- 共享部分:
  - shader 驱动的普通图元绘制
  - 统一 VBO 上传
  - 文本 atlas
  - 缓存纹理绘制

## 为了 GLES 已做的兼容处理

- GLFW/SDL2 窗口层已经支持 ES context hint。
- 文本 atlas 纹理格式改成 `GL_LUMINANCE`，shader 从 `.r` 采样。
- modern GL shader 已按 desktop GL / GLES 分开处理。
- Windows 下 ES 模式不再启用 Win32 fixed-function 文本 fallback。

## 当前仍未完成的部分

- `Vulkan` renderer 未实现。
- renderer capability 还没有提炼成完整的公共抽象层。
- dirty cache 仍然带有 OpenGL framebuffer copy 假设。
- 部分历史兼容 helper 还留在代码里，但已经不再是主路径。

## 当前建议

- 新功能优先继续落在 modern GL 主线上。
- `GLES` 兼容按同一套数据流继续补齐，不再回头加深旧 fixed-function 逻辑。
- `Vulkan` 等到 renderer 边界再稳定一轮之后再接。
