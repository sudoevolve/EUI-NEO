# 数据工具变更验证

本轮按功能在 dev 分支分别提交：过期异步回调、串口失败回滚和独占所有权、键盘与语义、撤销预算、虚拟表格、保存与目录选择、实时曲线。没有引入外部依赖，复用现有 CMake、虚拟列表、输入状态及渲染批次。

## 复现环境与方法

- Windows 11（10.0.26300），Intel Core Ultra 5 125H，Intel Arc Graphics，物理内存 25,263,726,592 字节。
- Visual Studio 18 2026 / MSVC 19.51.36247，Release x64（项目配置 `/O1 /Ob2 /MD`），GLFW + OpenGL。
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

`data_tools_viewer` 展示十万行表格与实时曲线、筛选、表头排序、列宽拖动、保存、目录选择和长文本编辑。键盘操作说明见 [无障碍文档](../../docs/guides/keyboard-accessibility.md)。

`data_tools_probe` 自动输入 `Sensor 3`，断言十万行筛选后为 6,250 行、输入框仍有焦点且实时曲线存在。OpenGL 可运行 `data_tools_probe output.ppm` 导出当前组合截图；默认 CTest 不写截图。实际检查了 GLFW + OpenGL 导出图中的列对齐、输入焦点、曲线峰谷和文本布局。

## 同机测量原始数据

2026-11-12，四组合编译和测试完成后测量，避免并发构建负载。复现：

```powershell
cmake --build build-dev-glfw-opengl --config Release --target linechart_realtime
./build-dev-glfw-opengl/Release/linechart_realtime.exe --benchmark
```

CSV 前六列为场景、模式、采样规模、序号、UI 节点数、compose+layout 微秒：

```csv
chart,legacy,5000,0,35006,653980
chart,legacy,5000,1,35006,710688
chart,legacy,5000,2,35006,832449
chart,legacy,5000,3,35006,775891
chart,legacy,5000,4,35006,819235
chart,legacy,5000,5,35006,792573
chart,legacy,5000,6,35006,785122
chart,legacy,5000,7,35006,792484
chart,legacy,5000,8,35006,814662
chart,legacy,5000,9,35006,810646
chart,realtime,5000,0,9,19.7
chart,realtime,5000,1,9,20.2
chart,realtime,5000,2,9,19.8
chart,realtime,5000,3,9,27.2
chart,realtime,5000,4,9,19.5
chart,realtime,5000,5,9,19.4
chart,realtime,5000,6,9,19.3
chart,realtime,5000,7,9,19.2
chart,realtime,5000,8,9,19
chart,realtime,5000,9,9,19.2
undo,18446744073709551615,128,134225920
undo,8388608,7,7340480
```

中位数：展示路径 792.529 ms，实时路径 0.01945 ms；节点数 35,006 → 9。两条路径的 UI 功能不同，不能将此值当作同等功能绘图或 GPU 加速倍数。撤销数据最后三列是预算、保留快照数、计费字节数：仅限制深度为 134,225,920 字节；8 MiB 预算为 7,340,480 字节。未测量 GPU 时间、RSS 峰值或 FPS。

## 后端验证

四个独立目录都完成配置、完整 Release 构建（含 fixtures）、CTest：

| 组合 | 测试结果 |
| --- | --- |
| GLFW + OpenGL | 46 / 46 通过 |
| SDL2 + OpenGL | 46 / 46 通过 |
| GLFW + Vulkan | 45 通过，1 跳过，0 失败 |
| SDL2 + Vulkan | 45 通过，1 跳过，0 失败 |

Vulkan 跳过的是已有 `markdown_viewport_probe`，其像素读回验证只支持 OpenGL。新加的 accessibility、data_tools、polyline 探针在四组合均通过。SDL2 初次回归暴露初始化前 `SDL_PushEvent` 的 timer 竞争；后端现在仅在 SDL 事件子系统有效时发送唤醒事件，`async_tasks` 在 SDL2 + Vulkan 连续运行 30 次通过。

最终审查修正了初始列宽低于最小值、表头与虚拟列表 scrollbar gap 不一致、弹层内 Escape 恢复焦点和日期/时间列键盘调整。最后的列宽对齐修正后，四组合再次完成完整 Release 构建，并分别补跑 `datatable` 与 `data_tools_probe`，均为 2 / 2 通过。`git diff --check` 通过。共享数据的 revision、线程约束与实时模式的展示取舍均已在组件文档说明。

本机未运行 Linux/macOS 原生分支、真实串口硬件、系统屏幕阅读器或 Vulkan 曲线像素读回；后两项不在本阶段实现范围。Windows 对话框自动验证取消与错误路径，选中路径和覆盖确认仍需交互验收。
