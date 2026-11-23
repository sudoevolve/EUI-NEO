# 科学绘图二进制 SDK 与商业交付（临时草案）

本草案用于技术交付和商务讨论，不构成新的项目许可证。当前仓库 LICENSE 为 Apache-2.0；
已授予接收方的开源权利不会因二进制交付或新合同被撤回。只有拥有相关代码完整授权权利的主体
才能另行提供商业许可，第三方代码仍遵循各自许可。

## 交付与授权

现有能力适合交付静态库 .lib / .a、公开 C++ 头文件、CMake 导入目标、依赖库及资源。
不交付 plot 的 .cpp；客户仍需要头文件才能编译。安装流程还提供应用入口 app_main.cpp 和核心
接口需要的内联实现，因此不是完全无源码包，不能承诺二进制不可逆向。

| 交付项 | 必须明确的内容 |
| --- | --- |
| 版本标识 | SDK 版本、提交号、构建日期、包 SHA256、回归报告 |
| 平台 | OS、CPU 架构、编译器/工具集、C++ 标准、Debug/Release |
| 运行库 | MSVC /MD 或 /MDd、_ITERATOR_DEBUG_LEVEL、异常和 RTTI |
| 后端 | GLFW/SDL2、OpenGL/Vulkan、驱动要求、已验证功能 |
| 资源和依赖 | 实际部署文件、版本、许可和 NOTICE |

plot 公共接口含 STL 类型并通过 C++ 异常报告错误，没有稳定 C ABI 或跨版本 ABI 保证。
不得混用 MSVC 和 MinGW，不得把 Release 库当作 Debug ABI；升级应成套重编译应用。
当前模块没有独立共享库导出宏契约，优先交付经过验证的静态库。

合同可逐项填写被许可法人、项目/产品、开发席位、部署和再分发范围、是否可转交 SDK、期限、
升级窗口、支持响应、缺陷修复范围和责任限制。明确哪些运行库可随最终应用分发。独立转售限制
仅适用于有权另行授权的部分。当前没有激活服务、license key 或运行时授权校验；合同授权与
技术防护是不同事项。

## 生成 SDK

Windows x64 静态库示例，编译器以安装环境为准：

```powershell
cmake -S . -B build-sdk -A x64 -DEUI_DEPS_MODE=bundled -DEUI_ENABLE_MODULES=ON -DEUI_ENABLE_PLOT=ON -DEUI_ENABLE_INSTALL=ON -DEUI_BUILD_APPS=OFF -DEUI_BUILD_USER_APPS=OFF -DEUI_RENDER_BACKEND=opengl -DEUI_WINDOW_BACKEND=glfw
cmake --build build-sdk --config Release
cmake --install build-sdk --config Release --prefix dist/plot-sdk
```

install 是开发 SDK 基础输出；发布方仍需核对依赖、字体和资产的再分发权利，附上实际使用的
许可/NOTICE，填写构建矩阵。项目 LICENSE 和新色表许可随安装包提供，后者位于
share/eui-neo/modules/plot/licenses/plot-colormaps.txt。plot 实现头 numeric.h、colormaps.h、
data_storage.h、gpu_scene3d_shader.h、software_raster.h 不安装；其他核心/模块头仍按现有规则提供，
当前包不等于完全隔离的独立 plot 产品。

Vulkan 已增加可复现的[发布门禁](科学绘图Vulkan发布验收.md)。运行
`python3 modules/plot/scripts/test_plot_vulkan_release.py` 可使用 Python 3 标准库构建、验证、安装和测试独立消费者，并输出文件 SHA256 清单。脚本不固定 CMake 生成器；目标平台仍需具备 Vulkan SDK、验证层和图形桌面。
该流程设置 EUI_INSTALL_DEMO_ASSETS=OFF；普通 install 默认仍包含演示资产，切勿混同。
务必使用新安装前缀，关闭选项不会删除旧包已有的资源。Vulkan SDK 开发文件仍是客户编译依赖；
最终应用运行需要驱动/loader 和相匹配的 MSVC 运行库。字体可以使用系统回退或自行提供已授权资源。

安装包增加 GLFW、FreeType（选择 FTL）、zlib、libpng、MD4C、yyjson、tray 的许可文件和
binary-sdk-notices.txt。stb_image、NanoSVG、miniaudio 的原许可随完整第三方头文件保留。
客户再分发最终应用时也应带上相关许可，而不能只在开发 SDK 中保留。其他后端和替换依赖需要
核对对应版本；本次没有完成所有可选依赖和演示资产的权利审计。

## 客户接入

可直接构建仓库 modules/plot/examples/plot_binary_sdk，安装后也位于 share/eui-neo/modules/plot/examples/plot_binary_sdk。
其中数据样例可以运行 CTest；窗口样例使用 PlotSession 管理关闭和 GPU 资源生命周期。

```cmake
cmake_minimum_required(VERSION 3.14)
project(PlotConsumer LANGUAGES CXX)
find_package(EuiNeo CONFIG REQUIRED)
add_executable(plot_data main.cpp)
target_compile_features(plot_data PRIVATE cxx_std_17)
target_link_libraries(plot_data PRIVATE eui::module_plot)
```

main.cpp 可先验证无窗口色阶：

```cpp
#include "modules/plot/field.h"
#include <iostream>
int main() {
    modules::plot::ColorScale scale;
    scale.setColorMap(modules::plot::ColorMap::Viridis);
    scale.setRange({0, 1});
    const auto rgba = scale.map(0.5);
    std::cout << rgba[0] << ' ' << rgba[1] << ' ' << rgba[2] << '\n';
}
```

```powershell
cmake -S consumer -B consumer-build -A x64 -DCMAKE_PREFIX_PATH=C:/sdk/plot-sdk
cmake --build consumer-build --config Release
./consumer-build/Release/plot_data.exe
```

窗口应用使用 eui_neo_configure_app(target) 并链接 eui::module_plot，该 helper 提供入口和资源
部署。UI 线程配置快照和调用 compose()，后台采集通过邮箱发布快照，设备销毁前 releaseGpu()。
完整用法见[快速开始](科学绘图快速开始.md)与[应用开发](科学绘图应用开发.md)。

## 发布验收

在干净目录仅指向安装前缀构建 consumer，确认没有引用源码目录；运行数据与导出测试及目标机器
图形探针。SVG 和 PDF 保留路径/文本 alpha。当前 PDF 内置 Helvetica，不嵌入 Unicode 字体；
中文等内容使用显式字体 PNG 或外部 SVG 渲染。PNG 为 RGBA8 显示输出。
精度承诺引用[数值契约](科学绘图精度与数值契约.md)，写明数据类型、算法、误差和平台。
