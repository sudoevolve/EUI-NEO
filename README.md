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
- A C++17 compiler
- OpenGL development files for the default renderer.
- Vulkan SDK is optional. Use a `build-vk` directory only when you want the Vulkan renderer.
- Platform OpenGL/windowing development files. Linux builds also need X11 and libcurl development packages.

Build-time sources for GLFW, glad, tray, FreeType, HarfBuzz, libpng, and zlib are vendored under `3rd/`. The default dependency mode is `auto`: CMake uses the local `3rd/` sources when they are present, and fetches only missing dependencies from pinned upstream URLs. Use `-DEUI_DEPS_MODE=bundled` for strict offline builds, or `-DEUI_DEPS_MODE=fetch` to force online dependency fetches. HarfBuzz shaping is enabled by default and can be disabled with `-DEUI_ENABLE_HARFBUZZ=OFF`.

Bundled and fetched dependencies are built for static linking by default, including GLFW. Release packages therefore do not need to ship a GLFW DLL / dylib / so. SDL2 may still be dynamic when you choose a system SDL2 package.

GLFW is the default window backend. SDL2 is optional and is not vendored. If GLFW is not available or you want to test SDL2, add `sdl2` to the build directory name:

```sh
cmake -S . -B build-sdl2
cmake --build build-sdl2
```

If a system SDL2 package is not available, add `-DEUI_DEPS_MODE=fetch` to download the pinned SDL2 source.

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

Build directory suffixes are recognized on first configure: `build` means GLFW + OpenGL, `build-sdl2` means SDL2 + OpenGL, `build-vk` means GLFW + Vulkan, and `build-sdl2-vk` means SDL2 + Vulkan. If a build directory already has a CMake cache, delete it or pass `-DEUI_WINDOW_BACKEND=...` / `-DEUI_RENDER_BACKEND=...` explicitly.

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
```

Top-level builds create one executable for each `examples/*.cpp` page source, such as `gallery` and `eui_demo`. After build, `assets/` is copied next to the executable automatically.

Tagged releases (`v*`) build Windows, Linux, and macOS packages through GitHub Actions and upload only runtime packages as release assets.

## Use In Your Project

There are three practical integration paths. Start with the public facade header unless you already need a custom window loop.

Minimal CMake project integration:

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

`app.cpp` only needs the public entry header plus an app config and compose function:

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

Build your project:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/my_app
```

### 1. Public Facade Header

This is the simplest app-level integration. Your app source includes one public header:

```cpp
#include "eui_neo.h"
```

Add EUI-NEO as a subdirectory and use the provided app main source:

```cmake
add_subdirectory(external/EUI-NEO)

add_executable(my_app external/EUI-NEO/core/app/glfw_app_main.cpp app.cpp)
eui_neo_configure_app(my_app)
```

Implement `app::dslAppConfig()` and `app::compose()` in `app.cpp`. EUI-NEO owns the window, event loop, selected render backend, and asset copying. This is a single public facade include, not a pure header-only library.

GLFW is the default backend. For SDL2, configure with `-DEUI_WINDOW_BACKEND=sdl2` and use `external/EUI-NEO/core/app/sdl2_app_main.cpp` instead of the GLFW app main source.

### 2. Static Library Target

For an existing application or a custom main loop, link the exported static library target directly:

```cmake
add_subdirectory(external/EUI-NEO)

add_executable(my_app main.cpp app.cpp)
target_link_libraries(my_app PRIVATE eui::neo)
eui_neo_copy_assets(my_app)
```

Use this path when your project already owns the native window, rendering context, event pump, or application lifecycle. Keep `#include "eui_neo.h"` for normal UI code; only the integration boundary should include lower-level runtime headers.

### 3. Develop Inside `examples/`

For quick experiments or new built-in demos, add a new file such as `examples/my_app.cpp`, include `eui_neo.h`, and implement `app::dslAppConfig()` plus `app::compose()`. Top-level builds automatically create one executable per `examples/*.cpp` file. For larger demos, use the `gallery` pattern: keep the app shell in `examples/gallery.cpp` and place QML-like page objects with local state under `examples/pages/*.h`.

When EUI-NEO is added as a subdirectory, bundled examples are disabled by default. Set `-DEUI_BUILD_APPS=ON` to build `gallery`, `eui_demo`, `serial_tool`, and the other sample apps. See [Integration Guide](docs/集成指南.md) for complete CMake snippets, `FetchContent`, and embedded GLFW loop notes.

## Project Layout

```text
assets/       Runtime assets: fonts, PNG, SVG, and icons
components/   Reusable UI components built on top of the DSL
core/         DSL, Runtime, primitives, text, image, network, and platform code
docs/         Implementation notes and API documentation
examples/     Standalone gallery and example application sources
include/      Public include path: eui_neo.h and eui/* facade headers
tests/        Probe sources, fixture apps, and local benchmark notes
3rd/          Vendored third-party build sources and single-file dependencies
```

## Docs

- [DSL Design And Current Implementation](docs/DSL.md)
- [Components](docs/组件.md)
- [Layout](docs/布局.md)
- [Events](docs/事件.md)
- [Animation](docs/动画.md)
- [Async](docs/异步.md)
- [Render Backend Architecture And Pipeline](docs/渲染后端架构.md)
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
