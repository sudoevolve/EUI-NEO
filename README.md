# EUI-NEO

<p align="center">
  <img src="assets/icon.svg" width="104" alt="EUI icon">
</p>

<p align="center">
  <a href="https://github.com/sudoevolve/EUI-NEO/actions/workflows/release.yml"><img alt="Release Build" src="https://github.com/sudoevolve/EUI-NEO/actions/workflows/release.yml/badge.svg"></a>
  <a href="https://github.com/sudoevolve/EUI-NEO/releases"><img alt="Release" src="https://img.shields.io/github/v/release/sudoevolve/EUI-NEO?include_prereleases&sort=semver"></a>
  <a href="LICENSE"><img alt="License" src="https://img.shields.io/badge/license-Apache%202.0-blue"></a>
  <img alt="C++17" src="https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus&logoColor=white">
  <img alt="CMake 3.14+" src="https://img.shields.io/badge/CMake-3.14%2B-064F8C?logo=cmake&logoColor=white">
  <img alt="OpenGL / Vulkan" src="https://img.shields.io/badge/OpenGL%20%2F%20Vulkan-rendering-5586A4?logo=vulkan&logoColor=white">
  <img alt="GLFW / SDL2" src="https://img.shields.io/badge/GLFW%20%2F%20SDL2-windowing-111111">
  <a href="https://github.com/sudoevolve/EUI-NEO/stargazers"><img alt="GitHub stars" src="https://img.shields.io/github/stars/sudoevolve/EUI-NEO?style=flat"></a>
</p>

<p align="center">
  <a href="README.zh-CN.md">简体中文</a>
  ·
  <a href="https://sudoevolve.github.io/EUI-NEO/">Website</a>
</p>

EUI-NEO is a cross-platform, high-performance, low-overhead C++17 UI framework with GLFW/SDL2 window backends and OpenGL/Vulkan render backends.

## Preview

|  |  |
| --- | --- |
| ![preview 1](docs/pic/1.jpg) | ![preview 2](docs/pic/2.jpg) |
| ![preview 3](docs/pic/3.jpg) | ![preview 4](docs/pic/4.jpg) |
| ![example 1](docs/pic/示例1.jpg) | ![example 2](docs/pic/示例2.jpg) |

## Quick Start

Requirements:

- CMake 3.14+
- A C++17 compiler: MSVC 19.29+ (Visual Studio 2019 16.11+), GCC/MinGW-w64 12+, or Clang 14+
- OpenGL development files for the default renderer

Add EUI-NEO under `external/EUI-NEO`, then create:

```cmake
cmake_minimum_required(VERSION 3.14)
project(MyProject LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(external/EUI-NEO)

add_executable(my_app app.cpp)
eui_neo_configure_app(my_app)
```

`app.cpp`:

```cpp
#include "eui_neo.h"

namespace app {

const DslAppConfig& dslAppConfig() {
    static const DslAppConfig config = DslAppConfig{}
        .title("My App")
        .pageId("my_app")
        .windowSize(960, 640);
    return config;
}

void compose(eui::Ui& ui, const eui::Screen& screen) {
    ui.column("root")
        .size(screen.width, screen.height)
        .padding(32.0f)
        .content([&] {
            ui.text("title")
                .text("Hello EUI-NEO")
                .fontSize(28.0f)
                .build();
        })
        .build();
}

} // namespace app
```

Build:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/my_app
```

`eui_neo_configure_app()` adds the selected GLFW/SDL2 entry point, links `eui::neo`, applies platform executable settings, and deploys the runtime assets. The application does not reference files under `core/`.

The release SDK supports the same target setup:

```cmake
find_package(EuiNeo CONFIG REQUIRED)
add_executable(my_app app.cpp)
eui_neo_configure_app(my_app)
```

See the [Integration Guide](docs/集成指南.md) for installation, `FetchContent`, SDL2/Vulkan selection, and custom main loops. See [Development And Release](docs/开发与发布.md) for building this repository and dependency requirements.

### Xmake

Xmake 3.0+ is supported for source builds and xrepo library consumers:

```lua
set_languages("cxx17")
add_requires("eui-neo")

target("my_app")
    set_kind("binary")
    add_files("main.cpp", "app.cpp")
    add_packages("eui-neo")
```

The xrepo package provides the `eui_neo` library; `main.cpp` owns the window
loop and application entry point. When building this repository directly,
the `eui.app` rule adds the selected platform entry point and deploys assets.

Build a repository example:

```powershell
xmake f -m debug -y --apps=y --user_apps=y
xmake build gallery
xmake f -m release -y --apps=y --user_apps=y
xmake build gallery
```

On Windows, the executable is `.xmake/build/windows/x64/<debug|release>/gallery.exe`.
See the [Integration Guide](docs/集成指南.md) for backend options and other targets.

## Optional Modules

Optional feature modules live under `modules/` and are documented in the [Modules Guide](docs/modules.md).

## Project Layout

```text
assets/       Runtime assets: fonts, PNG, SVG, and icons
components/   Reusable UI components built on top of the DSL
core/         DSL, Runtime, primitives, text, image, network, and platform code
docs/         Implementation notes and API documentation
examples/     Short, single-screen API demonstrations
modules/      Optional feature modules such as keyboard and serial
apps/         Longer or multi-page applications; each app lives in its own folder
include/      Public include path: eui_neo.h and eui/* facade headers
tests/        Probe sources, fixture apps, and local benchmark notes
3rd/          Vendored third-party build sources and single-file dependencies
```

## Docs

- [DSL Design And Current Implementation](docs/DSL.md)
- [Components](docs/组件.md)
- [Modules](docs/modules.md)
- [State Model](docs/状态.md)
- [Layout](docs/布局.md)
- [Events](docs/事件.md)
- [Animation](docs/动画.md)
- [Async](docs/异步.md)
- [Render Backend Architecture And Pipeline](docs/渲染后端架构.md)
- [Retained Layer Cache](docs/retained_layer_cache.md)
- [Images](docs/图片.md)
- [Network](docs/网络.md)
- [Platform Capabilities](docs/平台能力.md)
- [Integration Guide](docs/集成指南.md)
- [Shadertoy Primitive](docs/Shadertoy.md)
- [Development And Release](docs/开发与发布.md)

## License

EUI-NEO's original source code is licensed under the Apache License 2.0. Third-party code under `3rd/`, optional build-time dependencies fetched by CMake, and bundled fonts or icon fonts under `assets/` follow their respective upstream licenses and copyright notices.

## Contributors

Thank you to everyone who has contributed code, improved the documentation, reported issues, or shared feedback with EUI-NEO.

<a href="https://github.com/sudoevolve/EUI-NEO/graphs/contributors">
  <img alt="EUI-NEO contributors" src="https://contrib.rocks/image?repo=sudoevolve/eui-neo&max=100&columns=10">
</a>
