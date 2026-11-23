# 模块

EUI-NEO 将大型、可选、平台相关或小众功能放在 `modules/` 下，避免核心库变重。一个项目可以携带全部模块、只携带部分模块，或者完全不带模块。

## 构建行为

直接在 EUI-NEO 源码根目录构建时，模块发现默认启用（绘图仍需单独开启）；作为其他项目的
`FetchContent` 或子目录依赖时，模块默认关闭，因此不会额外生成仓库自带
示例、应用或模块目标。外部项目只需构建自己的应用目标。

顶层构建默认启用模块发现：

```sh
cmake -S . -B build -DEUI_ENABLE_MODULES=ON
```

如果 `modules/` 或某个模块子目录不存在，CMake 会直接跳过，不会报错。需要完全关闭模块发现时传入 `-DEUI_ENABLE_MODULES=OFF`。依赖某个模块的内置示例会在该模块不可用时跳过。

已知模块目录：

- `modules/keyboard`：屏幕键盘组件和输入绑定。
- `modules/serial`：跨平台串口发现、连接与读写能力。
- `modules/plot`：可选科学绘图，包含二维曲线、热力图、等高线、矢量场、三维/体数据、采集波形及 PNG/SVG/PDF 导出。

每个模块按需维护自己的 `CMakeLists.txt`、README、头文件、示例和测试。
启用安装时，现有模块 target 会随 `EuiNeoTargets.cmake` 一起导出；源码接入和 `find_package` 接入使用相同的 `eui::module_*` 名称。

## 使用模块

应用只需要包含模块头文件并实例化模块对象。键盘模块可以在实例化时绑定输入控件 id：

```cpp
#include "modules/keyboard/keyboard.h"

std::string name;
std::string note;

modules::keyboard::KeyboardPanelController keyboard({
    {"name.input", name},
    {"note.input", note, true},
});

void compose(eui::Ui& ui, const eui::Screen& screen) {
    components::input(ui, "name.input")
        .value(name)
        .onChange([&](const std::string& value) { name = value; })
        .build();

    keyboard.compose(ui, "keyboard", screen.width, screen.height);
}
```

键盘会在绑定的输入框获得焦点时自动打开，将输入写回绑定的字符串，并支持多行 Enter、主题/强调色更新、拖动和边缘缩放。

## 科学绘图

`plot` 分支的模块现已同步到 `dev`。绘图模块默认关闭，需同时设置 `EUI_ENABLE_MODULES=ON` 和 `EUI_ENABLE_PLOT=ON`；核心 `eui::neo` 不会反向依赖绘图模块。应用显式链接 `eui::module_plot`，并包含 `modules/plot/plot.h` 等模块头文件。

源码接入时，在配置 EUI-NEO 之前启用模块：

```cmake
set(EUI_ENABLE_MODULES ON CACHE BOOL "Build optional modules")
set(EUI_ENABLE_PLOT ON CACHE BOOL "Build scientific plotting")
add_subdirectory(EUI-NEO)
add_executable(my_plot main.cpp)
eui_neo_configure_app(my_plot)
target_link_libraries(my_plot PRIVATE eui::module_plot)
```

已安装的 SDK 使用 `find_package(EuiNeo CONFIG REQUIRED)` 和相同 target；SDK 必须在启用绘图模块时构建。`EUI_BUILD_APPS=ON` 注册 `scientific_plot_minimal`、`scientific_plot`、`scientific_plot_acquisition` 及 phase2/3/4 示例；`EUI_BUILD_TEST_FIXTURES=ON` 注册模块测试。

性能基准默认不构建、不注册到 CTest。只有显式设置 `EUI_PLOT_BUILD_BENCHMARKS=ON` 才启用 Gallery GPU、数值/空间及交互性能基准；这些测试负载较高，建议单独执行。

OpenGL 与 Vulkan 支持二维 GPU 光栅化，设备子像素精度不足时使用 CPU 参考路径。三维/体数据的 OpenGL 交互视口使用 GPU，Vulkan 当前使用 CPU 参考渲染后上传。详细 API、采集线程、预算及导出限制见[模块 README](https://github.com/sudoevolve/EUI-NEO/blob/dev/modules/plot/README.md)和[快速开始](https://github.com/sudoevolve/EUI-NEO/blob/dev/modules/plot/docs/科学绘图快速开始.md)。

## 开发约束

大型功能、小众功能、平台相关功能、依赖较重的功能、实验性功能应优先做成模块。例如虚拟键盘、媒体播放/录制、串口设备、硬件集成、专用工具等。

核心库只保留模块确实需要的通用能力，例如事件焦点保留、builder 透传、可选构建发现。不要让所有应用为不使用的功能承担构建、依赖、体积或维护成本。

模块的 `CMakeLists.txt` 可以向全局属性 `EUI_NEO_MODULE_BUILD_SCRIPTS` 追加本模块构建脚本的
绝对路径。顶层会在应用/测试 helpers 定义后、安装导出前执行这些脚本，使模块能自行按
`EUI_BUILD_APPS`、`EUI_BUILD_TEST_FIXTURES` 注册示例和测试，无需在顶层硬编码模块名称。
脚本应使用明确的源码和构建目录调用 `add_subdirectory()`，避免依赖调用者当前目录。

模块需要控制公开头文件集合时，向全局属性 `EUI_NEO_MODULE_OWN_HEADER_DIRS` 追加其相对于
`modules/` 的目录名，并自行提供 `install(FILES ...)` 规则。通用头文件安装会排除该目录，
防止私有实现、示例和测试头一起泄漏。即使模块关闭，也应在返回前登记此排除项。
`EUI_INSTALL_DEMO_ASSETS=OFF` 可关闭 SDK 的演示资源安装；必须使用新安装目录，选项不会清理旧文件。

新增模块前需要确认：

- 模块缺失时不会破坏配置、构建、安装或示例扫描。
- 依赖该模块的内置示例是可选的，并会在模块不可用时跳过。
- 模块不会把必需依赖引入 `eui::neo` 核心 target。
- 模块 API 足够简单，应用可以直接实例化并填写基础属性。
- 模块示例和测试都是可选的，不假设下游一定携带该模块。
