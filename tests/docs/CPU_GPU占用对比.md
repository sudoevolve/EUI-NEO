# CPU / GPU 占用对比

本文记录当前构建在同一台 macOS 机器上的 CPU/GPU 采样结果。旧测试记录已删除，本文件只保留本轮重新采样结果。

## 环境

- 时间：2026-06-04 11:18 CST
- 系统：macOS 26.5，Darwin 25.5.0，arm64
- CPU：Apple M5
- 构建目录：`build/probes-opengl-glfw`
- 构建配置：`Release`、`EUI_RENDER_BACKEND=opengl`、`EUI_WINDOW_BACKEND=glfw`、`EUI_BUILD_TEST_FIXTURES=ON`
- CPU 采样方法：使用 `ps -o %cpu=` 每秒采样。
- GPU 采样状态：本机 `powermetrics --samplers gpu_power` 需要 superuser 权限，本轮未采到可验证的 GPU utilization 数据。

## CPU 结果

| 目标 | 窗口尺寸 | 采样说明 | CPU 平均 | CPU 范围 | RSS 平均 |
| --- | --- | --- | ---: | ---: | ---: |
| `window_only_probe` | 800x600 | 启动后等待 3 秒，采样 6 次 | 0.6% | 0.5-0.7% | 85.4 MiB |
| `render_context_probe` | 800x600 | 启动后等待 3 秒，采样 6 次 | 0.5% | 0.5-0.6% | 85.5 MiB |
| `render_clear_probe` | 800x600 | 启动后等待 3 秒，采样 6 次 | 4.8% | 4.4-5.2% | 85.7 MiB |
| `empty_window` | 800x600 | 启动后等待 3 秒，采样 6 次 | 0.0% | 0.0-0.0% | 86.0 MiB |
| `animation_240fps` | 1280x720 | 启动后等待 8 秒，采样 10 次 | 31.2% | 28.6-35.4% | 103.9 MiB |

## GPU 结果

本轮没有记录 GPU 占用百分比，因为 macOS 上可用的系统级 GPU 采样工具需要 superuser 权限：

```sh
powermetrics --samplers gpu_power -n 1 -i 1000
# powermetrics must be invoked as the superuser
```

因此本轮 GPU 对比只记录为“未采样”，不沿用旧 Windows/NVIDIA 数据，也不使用无法验证的替代口径。

| 目标 | GPU 占用 |
| --- | --- |
| `window_only_probe` | 未采样：本机无 superuser 权限运行 `powermetrics` |
| `render_context_probe` | 未采样：本机无 superuser 权限运行 `powermetrics` |
| `render_clear_probe` | 未采样：本机无 superuser 权限运行 `powermetrics` |
| `empty_window` | 未采样：本机无 superuser 权限运行 `powermetrics` |
| `animation_240fps` | 未采样：本机无 superuser 权限运行 `powermetrics` |

## 结论

- `window_only_probe` / `render_context_probe` CPU 都在约 0.5-0.6%，说明窗口创建和上下文初始化后静置成本很低。
- `render_clear_probe` 持续 clear/present，CPU 上升到约 4.8%，这部分是持续帧循环和后端 present 的底层成本。
- `empty_window` 设置 `fps(0.0)` 且没有 UI 内容，采样窗口内 CPU 基本为 0%。
- `animation_240fps` 是真实 DSL 动画压力场景，预热后 CPU 平均约 31.2%，明显高于底层 probe；这才更接近 UI 动画负载下的 CPU 成本。
- GPU 占用需要在具备权限的 macOS 环境用 `sudo powermetrics`，或在 Windows/NVIDIA 环境用 GPU Engine counter 重新采样。

## 复测命令

```sh
cmake --build build/probes-opengl-glfw --target \
  window_only_probe render_context_probe render_clear_probe empty_window animation_240fps --parallel

build/probes-opengl-glfw/window_only_probe --manual
build/probes-opengl-glfw/render_context_probe --manual
build/probes-opengl-glfw/render_clear_probe --manual
build/probes-opengl-glfw/empty_window
build/probes-opengl-glfw/animation_240fps
```
