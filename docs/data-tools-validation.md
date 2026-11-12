# 数据工具变更验证

本轮按功能在 dev 分支分别提交：过期异步回调、串口失败回滚和独占所有权、键盘与语义、撤销预算、虚拟表格、保存与目录选择、实时曲线。没有引入外部依赖，复用现有 CMake、虚拟列表、输入状态及渲染批次。

## 复现环境与方法

- Windows 11（10.0.26300），Intel Core Ultra 5 125H，Intel Arc Graphics，物理内存 25,263,726,592 字节。
- Visual Studio 18 2026 / MSVC，Release x64，GLFW + OpenGL。
- `linechart_realtime --benchmark` 比较同一二进制的原展示路径与实时路径，每种预热 3 次，采样 10 次；计时覆盖 compose + layout，不包含 GPU 或 FPS。
- 图宽 356、高 312，5,000 个采样；实时路径每次递增 revision，包含重新降采样。展示路径保留原有逐点标签与 Tooltip，实时路径省略它们，因此两者产品能力有差别，测量说明大数据实时模式的代价，不代表等功能的通用加速倍数。
- 撤销比较同一 InputModel 对 1 MiB 文本保存 128 次编辑：仅深度限制（预算最大值）与默认 8 MiB 预算。指标是快照对象和字符串容量计费，非进程峰值内存。

## 行为覆盖

- async_tasks：完成后重启/取消、forget 后复用 key、同批次回调取消后续回调。
- serial_port：不可复制、可移动、无效设备和配置失败；POSIX PTY 分支需要 Linux/macOS 环境运行。Windows 无真实串口硬件验收。
- accessibility_probe：真实窗口、Tab/Shift+Tab、禁用跳过、Enter/Space、slider、组方向键、dropdown、模态约束及恢复、焦点框布局。
- datatable：十万行节点上限、稳定数值排序、缺失列、筛选与 revision、滚动偏移、列宽键盘调整、排序后点击原始索引。
- platform_dialog：Windows 原生打开/保存/目录对话框自动取消、超长默认文件名失败；Linux stub 覆盖参数及错误输出。Windows 的保存选中/覆盖确认由交互 fixture 验证入口提供，自动测试不写用户文件。
- input_history_budget：预算淘汰、0 预算、超大快照、undo/redo、UTF-8。
- linechart_realtime：峰谷保留、输出上限、断点、空数据、区间和数值范围、节点数及描边命中/边界。
- polyline_probe：实际后端绘制与颜色刷新；OpenGL 读像素验证线段、断点。Vulkan 运行同一渲染路径，尚未增加 framebuffer 读回断言。

## 交互入口

`data_tools_viewer` 展示十万行表格与实时曲线、筛选、表头排序、列宽拖动、保存、目录选择和长文本编辑。键盘操作说明见 [无障碍文档](keyboard-accessibility.md)。
