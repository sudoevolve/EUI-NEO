# SDL2 / GLFW 触摸与手势验证

## 实现边界

- 保留工作区已有的输入队列、DSL、Runtime、SDL2 与 Linux 触摸接入，在其基础上完善识别与平台行为。
- 平台桥接负责原生事件和坐标转换；共享 `TouchTracker` 负责单指 pointer、像素滚动和多指手势；Runtime 负责命中与捕获。
- 全部触点参与中心、缩放和旋转计算；触点数量变化时重建基线，重合触点及无效坐标不产生无效几何值。
- 多指开始取消单指按压与待处理滚动；降为单指后不恢复点击，直到所有触点抬起。
- Windows GLFW 注册 `WM_TOUCH`，过滤触摸模拟鼠标；`WM_GESTURE` 的平移、缩放、旋转分别维护基线，统一在 `GID_END` 结束。
- Linux 原生触控板序列取消或结束后忽略旧 Update/End；触屏与原生触控板输入切换时取消旧序列。
- 交互遮罩阻挡手势命中；失焦取消触摸 pointer 时保留 `touch=true`。
- 未增加项目外部依赖。

## 自动验证

Windows 使用现有四个独立构建目录完成配置、完整 Release 构建与全量 CTest；最终取消标记修正后增量重建相关输入目标并再次验证。

| 组合 | 全量测试 | 按设计跳过 |
| --- | --- | --- |
| GLFW + OpenGL | 49 项，零失败 | Linux / SDL2 触摸探针 |
| SDL2 + OpenGL | 49 项，零失败 | Linux / Windows GLFW 触摸探针 |
| GLFW + Vulkan | 49 项，零失败 | Linux / SDL2 触摸探针、markdown viewport probe |
| SDL2 + Vulkan | 49 项，零失败 | Linux / Windows GLFW 触摸探针、markdown viewport probe |

Linux 在本机 WSL Ubuntu 24.04 中构建 OpenGL 组合；以下每条路径均通过 3 项针对性测试：

| 组合 | 环境 | 测试 |
| --- | --- | --- |
| SDL2 + X11 | Xvfb | pointer_input、sdl_touch_probe、input_runtime_probe |
| SDL2 + Wayland | WSLg | pointer_input、sdl_touch_probe、input_runtime_probe |
| GLFW + X11 | Xvfb | pointer_input、linux_touch_probe、input_runtime_probe |
| GLFW + Wayland | WSLg | pointer_input、linux_touch_probe、input_runtime_probe |

Linux 从 Windows 工作区编译时，vendored libpng 的 awk 输入含 CRLF，验证构建使用 `-DAWK:FILEPATH=OFF` 选择 libpng 自带的预生成配置头；未修改第三方源码。

触摸探针使用合成输入验证平台转换、状态机、窗口隔离、缩放与旋转。Windows 探针还检查实际触摸注册和 `WM_CANCELMODE`；Linux 探针检查 X11 / Wayland 初始化。`check_docs.py` 与 `git diff --check` 通过。

四组全量 CTest 并行复测时，SDL2 + OpenGL 的 `gpu_image_probe` 曾一次报告 `replaced resources accumulate`；该探针独立连续运行三次通过。首次完整测试和后续串行全量复测通过。此偶发 GPU 资源回收问题未修改，触摸相关测试均通过。

## 审查结论与验收边界

自动代码审查发现并修复了三指几何遗漏、Windows 交错手势基线混用及提前结束、Linux 旧手势事件继续派发、遮罩手势穿透、触摸取消标记丢失，并补充相应回归覆盖。

本轮未进行物理触屏或触控板操作。WSLg 合成器未提供 pointer-gestures 协议，Wayland 触控板验证使用注入回调数据；Windows 精确式触控板是否提供 WM_GESTURE 取决于系统和驱动。Linux Vulkan 未单独运行。

实机验收应覆盖：单指点击与滚动、双指同时平移缩放旋转、三指以上增减触点、跨窗口与高 DPI、失焦/隐藏/模态禁用期间取消，以及所有触点抬起后重新开始操作。
