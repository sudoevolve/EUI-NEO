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
