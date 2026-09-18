# 科学绘图模块

`eui::module_plot` 提供独立 XY 数据、科学坐标轴、二维图形、原始数据拾取和
OpenGL 离屏绘图。完整 API、生命周期、限制和测试入口见 [科学绘图](../../docs/科学绘图.md)。

```cmake
set(EUI_ENABLE_MODULES ON CACHE BOOL "" FORCE)
set(EUI_ENABLE_PLOT ON CACHE BOOL "" FORCE)
add_subdirectory(EUI-NEO)
target_link_libraries(my_app PRIVATE eui::module_plot)
```

安装包使用相同 target。模块没有额外第三方依赖，不反向引入到 `eui::neo`。
`EUI_ENABLE_PLOT=OFF` 或删除模块目录后，绘图示例与测试自动跳过。
数值部分不使用 GPU 类型；当前图像输出仅支持 OpenGL，Vulkan 不提供绘图能力。
