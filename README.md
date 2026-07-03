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
  <img alt="GLFW / SDL2 / SDL3" src="https://img.shields.io/badge/GLFW%20%2F%20SDL2%20%2F%20SDL3-windowing-111111">
  <a href="https://github.com/sudoevolve/EUI-NEO/stargazers"><img alt="GitHub stars" src="https://img.shields.io/github/stars/sudoevolve/EUI-NEO?style=flat"></a>
</p>

<p align="center">
  <a href="README.zh-CN.md">简体中文</a>
  ·
  <a href="https://sudoevolve.github.io/EUI-NEO/">Website</a>
</p>

EUI-NEO is a cross-platform, high-performance, low-overhead C++17 UI framework with GLFW/SDL2/SDL3 window backends and OpenGL/Vulkan render backends.

## Preview

|  |  |
| --- | --- |
| ![preview 1](docs/pic/1.jpg) | ![preview 2](docs/pic/2.jpg) |
| ![preview 3](docs/pic/3.jpg) | ![preview 4](docs/pic/4.jpg) |
| ![example 1](docs/pic/示例1.jpg) | ![example 2](docs/pic/示例2.jpg) |

## Quick Start

Requirements:

- CMake 3.14+
- A C++17 compiler
- OpenGL development files for the default renderer.
- Vulkan SDK is optional. Use a `build-vk` directory only when you want the Vulkan renderer.
- Platform OpenGL/windowing development files. Linux builds also need X11 and libcurl development packages.

Build-time sources for GLFW, glad, tray, FreeType, HarfBuzz, libpng, and zlib are vendored under `3rd/`. The default dependency mode is `auto`: CMake uses existing parent targets for `glfw` / `glad` first, then package targets, then the local `3rd/` sources or pinned fetch fallback. Use `-DEUI_DEPS_MODE=bundled` for strict offline builds, or `-DEUI_DEPS_MODE=fetch` to force online dependency fetches. HarfBuzz shaping is enabled by default and can be disabled with `-DEUI_ENABLE_HARFBUZZ=OFF`.

Bundled and fetched dependencies are built for static linking by default, including GLFW, SDL2, and SDL3. Release packages therefore do not need to ship a GLFW DLL / dylib / so. SDL2 or SDL3 may still be dynamic when you choose a system package. The `eui_neo` target itself is static by default; configure with `-DEUI_BUILD_SHARED=ON` when you want to build and install it as a shared library.

GLFW is the default window backend. SDL2 and SDL3 are optional and are not vendored. If GLFW is not available or you want to test an SDL backend, add `sdl2` or `sdl3` to the build directory name:

```sh
cmake -S . -B build-sdl2
cmake --build build-sdl2

cmake -S . -B build-sdl3
cmake --build build-sdl3
```

If a system SDL2 or SDL3 package is not available, add `-DEUI_DEPS_MODE=fetch` to download the pinned SDL source.

macOS / Linux example:

```sh
cmake -S . -B build
cmake --build build
./build/gallery
```

Explicit render backend examples:

```sh
cmake -S . -B build-vk
cmake --build build-vk --target gallery
```

Build directory suffixes are recognized on first configure: `build` means GLFW + OpenGL, `build-sdl2` means SDL2 + OpenGL, `build-sdl3` means SDL3 + OpenGL, `build-vk` means GLFW + Vulkan, `build-sdl2-vk` means SDL2 + Vulkan, and `build-sdl3-vk` means SDL3 + Vulkan. If a build directory already has a CMake cache, delete it or pass `-DEUI_WINDOW_BACKEND=...` / `-DEUI_RENDER_BACKEND=...` explicitly.

Windows / PowerShell example:

```powershell
cmake -S . -B build
cmake --build build --config Release
.\build\Release\gallery.exe
```

Linux package hint:

```sh
sudo apt-get install -y ninja-build libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev libcurl4-openssl-dev
# Optional for -DEUI_WINDOW_BACKEND=sdl2:
sudo apt-get install -y libsdl2-dev
# Optional for -DEUI_WINDOW_BACKEND=sdl3:
sudo apt-get install -y libsdl3-dev
```

Top-level builds create one executable for each `examples/*.cpp` page source, such as `gallery`, `chat`, and `eui_demo`. After build, `assets/` is copied next to the executable automatically.

User apps can live under `apps/` and are built the same way by default in a top-level checkout. Use either a flat `apps/my_app.cpp` file or a directory app such as `apps/my_app/app.cpp`. Directory apps may include their own `apps/my_app/assets/`; those files are copied into the executable `assets/` directory after the framework assets. Disable this scan with `-DEUI_BUILD_USER_APPS=OFF`.

Tagged releases (`v*`) build Windows, Linux, and macOS packages through GitHub Actions and upload runtime and SDK packages as release assets. Runtime packages automatically collect every executable generated from `examples/*.cpp`.

## Use In Your Project

The recommended path is to add EUI-NEO as a CMake subdirectory, use the provided app main source, and write your UI through the public facade header.

Minimal CMake:

```cmake
cmake_minimum_required(VERSION 3.14)
project(MyProject LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(external/EUI-NEO)

add_executable(my_app
    external/EUI-NEO/core/app/glfw_app_main.cpp
    app.cpp
)
eui_neo_configure_app(my_app)
```

Minimal `app.cpp`:

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

EUI-NEO owns the window, event loop, selected render backend, and asset copying in this setup. For SDL2, SDL3, Vulkan, `FetchContent`, custom main loops, or building the bundled examples from a parent project, see the [Integration Guide](docs/集成指南.md).

## Project Layout

```text
assets/       Runtime assets: fonts, PNG, SVG, and icons
components/   Reusable UI components built on top of the DSL
core/         DSL, Runtime, primitives, text, image, network, and platform code
docs/         Implementation notes and API documentation
examples/     Standalone gallery and example application sources
apps/         User application sources; flat .cpp files or app folders
include/      Public include path: eui_neo.h and eui/* facade headers
tests/        Probe sources, fixture apps, and local benchmark notes
3rd/          Vendored third-party build sources and single-file dependencies
```

## Docs

- [DSL Design And Current Implementation](docs/DSL.md)
- [Components](docs/组件.md)
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
- [Development And Release](docs/开发与发布.md)

## License

EUI-NEO's original source code is licensed under the Apache License 2.0. Third-party code under `3rd/`, optional build-time dependencies fetched by CMake, and bundled fonts or icon fonts under `assets/` follow their respective upstream licenses and copyright notices.

## Star History

<a href="https://www.star-history.com/#sudoevolve/EUI-NEO&Date">
  <img alt="Star History Chart" src="https://api.star-history.com/svg?repos=sudoevolve/EUI-NEO&type=Date">
</a>
