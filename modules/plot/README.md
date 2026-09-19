# 科学绘图模块

`eui::module_plot` 提供独立 XY 数据、笛卡尔与极坐标轴、二维图形、原始数据拾取和
OpenGL 离屏绘图。`PolarPlot` 接收 $(\theta,r)$ 数据，支持度/弧度、可配置零角及顺逆时针
方向，并绘制曲线和散点。`ScalarField`、`ColorScale` 和 `HeatmapPlot` 提供行优先矩阵、
连续/离散线性或对数色阶及 colorbar。`marchingSquares`、`filledContours` 和 `contourLabels`
提供缺失单元跳过的等高线、填充带和标签锚点。完整 API、生命周期、限制和测试入口见 [科学绘图](../../docs/科学绘图.md)。
`VectorField`、`vectorArrows` 和 `streamlines` 提供共享规则网格上的箭头与固定步 RK4 流线。
数学公式排版暂未实现；当前标注使用普通 UTF-8 文本。后续应接入成熟 TeX/MathML 引擎生成 SVG，
而不是在 FreeType 字符层手工拼接分式和根式。`ExportScene` 提供 PNG、SVG 和 PDF 输出。
`examples/scientific_plot_phase2.cpp` 将阶段二能力合并为一个可手测的 4×2 面板示例。

```cmake
set(EUI_ENABLE_MODULES ON CACHE BOOL "" FORCE)
set(EUI_ENABLE_PLOT ON CACHE BOOL "" FORCE)
add_subdirectory(EUI-NEO)
target_link_libraries(my_app PRIVATE eui::module_plot)
```

安装包使用相同 target。模块没有额外第三方依赖，不反向引入到 `eui::neo`。
`EUI_ENABLE_PLOT=OFF` 或删除模块目录后，绘图示例与测试自动跳过。
数值部分不使用 GPU 类型；当前窗口图像输出仅支持 OpenGL，Vulkan 不提供绘图视口。

`Plot3D`、`SceneRenderer3D`、`VolumeData` 提供三维几何、相机、深度拾取、切片、等值面及体积分。
交互视口使用 OpenGL 3.3 的 GPU BVH 求交与体光线投射；相机更新复用几何与体纹理。
CPU 双精度参考路径负责拾取、离线导出和超出 GPU 支持范围时的回退。
用法、预算、透明合成和输出限制见 [三维与体数据](../../docs/科学绘图三维与体数据.md)。

`examples/scientific_plot_phase3.cpp` 提供 3×2 三维集成展示；
`examples/scientific_plot_phase4.cpp` 提供 4×2 体数据与高级输出集成展示。
各面板可独立交互，整页可导出 PNG/SVG/PDF，第四阶段可保存恢复整页状态。

## 高频二维波形

`WaveformBuffer` 是面向采集线程的有界 UInt8/Int16/UInt16 历史缓冲。它接收交错多通道块，预分配原始
字节、峰值索引和快照池；`snapshot()` 返回可直接赋给 `Series::data` 的不可变 `Data`。
因此应用只负责驱动采集设备和处理 `tryAppend()` 的背压，不需要自己维护显示降采样、峰值索引
或拾取索引。旧快照被 UI 持有时，缓冲会返回 `false`，不会覆盖仍在绘制的数据。

默认配置为双路 100 kS/s、1 秒历史，固定池小于 1 MiB。`Config::forDuration()` 根据通道数、
采样率、保留秒数检查预算；`Config::dual125MS()` 显式启用约 5 秒的双路 125 MS/s 历史。
`WaveformInput` 适配不定长字节包，返回已接收前缀并保留未完成块。历史长度按完整块向上取整；65,536 点视口使用
缓存层级统计选取每列首/末/极值点，拾取始终回到原始采样索引。接口位于
`modules/plot/waveform.h`，完整吞吐和回绕验证在 `plot_throughput_probe --production`，
单元覆盖在 `plot_waveform`。

## 应用接入

`PlotSession` 统一注册图表关闭处理，`LatestValue<T>` 在采集线程和 UI 之间传递最新显示快照。
`examples/scientific_plot_acquisition.cpp` 是可独立构建的 Int16 上位机模板，包含暂停、跟随、
热力图拖拽缩放和探针。完整写法见 [科学绘图应用开发](../../docs/科学绘图应用开发.md)。
