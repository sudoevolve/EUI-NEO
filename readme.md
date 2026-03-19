# EUI

[中文说明](readme.zh-CN.md)

EUI is a lightweight, header-only C++ UI toolkit focused on practical immediate-mode workflows.
The core API in `include/EUI.h` generates draw commands only.
An optional OpenGL app runtime is available through the built-in GLFW and SDL2 window backends.
It now includes a more complete text pipeline for mixed text/icon rendering, editable inputs, and scrolling text areas.
Major widgets also include built-in lightweight motion feedback for hover, press, focus, dropdown reveal, and progress changes.
The current motion defaults are tuned to stay compatible with event-driven rendering and restrained GPU usage.

## Preview

<table>
  <tr>
    <td width="50%"><img src="preview/0.jpg" alt="Preview 0" width="100%" /></td>
    <td width="50%"><img src="preview/1.jpg" alt="Preview 1" width="100%" /></td>
  </tr>
  <tr>
    <td width="50%"><img src="preview/2.jpg" alt="Preview 2" width="100%" /></td>
    <td width="50%"><img src="preview/3.jpg" alt="Preview 3" width="100%" /></td>
  </tr>
</table>

## Project Analysis (Current Code)

### 1) Architecture

- **Core layer (`eui::Context`)**
  - Immediate-mode UI API that emits `DrawCommand` + text arena.
  - No hard dependency on GLFW/OpenGL for core usage.
- **Optional app runtime (`eui::app`)**
  - Window/input loop, DPI extraction, clipboard bridge, frame scheduling.
  - Calls your UI builder callback with `FrameContext`.
- **Renderer layer (inside `EUI.h`)**
  - OpenGL command renderer with clipping and batching.
- Optional `stb_truetype` renderer for text/icons with glyph texture caching (auto-enabled when `stb_truetype.h` is available).
- Text measurement is configured to follow the active renderer backend so caret, selection, wrapping, and hit-testing stay aligned with what is actually rendered.
- Uses text font + icon font fallback (private-use codepoints prefer icon font).
- Icon font defaults to bundled `include/Font Awesome 7 Free-Solid-900.otf`; text keeps system default.
- Falls back to built-in bitmap text only when font loading/rendering fails.
- You can force-disable STB at compile time with `-DEUI_ENABLE_STB_TRUETYPE=0`.

### 2) Rendering Pipeline

1. `ui.begin_frame(...)`
2. Build UI widgets and layout.
3. `ui.take_frame(...)` gets command buffer + text arena.
4. Runtime hashes frame payload.
5. If unchanged, skip redraw.
6. If changed, compute dirty regions vs previous frame.
7. Repaint only dirty scissor regions; reuse cached framebuffer texture for the rest.

### 3) Performance Mechanisms Already Implemented

- Event-driven rendering (`continuous_render = false` by default).
- Frame hash early-out to avoid redundant GPU work.
- Dirty-region diff between previous and current draw command streams.
- Cached framebuffer texture + partial redraw via scissor.
- Large dirty-area changes fall back to full redraw instead of forcing cache replay + many partial updates.
- Clip stack + command clipping in core.
- Tile-assisted command bucketing for large command counts.
- Motion states only request redraw while hover/focus/press/value animations are still settling.
- Weak glows are culled early, large surfaces stay static, and tiny motion deltas snap to rest quickly.

### 4) Text / Editing Model

- Single-line inputs support caret movement, drag selection, clipboard shortcuts, and horizontal scroll-to-caret.
- `text_area` supports multi-line editing, wrapping, drag selection, scrolling, and `Up` / `Down` caret navigation with preferred x tracking.
- Mixed text + icon labels use icon-aware measurement so selection and caret placement stay closer to rendered output.
- The app runtime handles key repeat for editing keys such as `Backspace`, `Delete`, `Enter`, arrows, `Home`, and `End`.

## Implemented Features

### Theme

- `ThemeMode` (`Light` / `Dark`)
- Primary color (`set_primary_color`)
- Corner radius (`set_corner_radius`)
- Dark mode auto-lifts too-dark primary colors for better contrast

### Layout

- `ui.panel(...).begin(...)`
- `ui.card(...).begin(...)`
- `dock_top` / `dock_bottom` / `dock_left` / `dock_right`
- `split_h_ratio` / `split_v_ratio`
- `row().items(...)`
- `anchor()`
- `scope()` / `stack()` / `clip()`
- `spacer`

### Widgets

- `label`
- `button` (`Primary`, `Secondary`, `Ghost`, optional `text_scale`)
- `tab`
- `slider_float` (drag + right-click numeric edit)
- `input_float` (caret, selection, `Ctrl+A/C/V/X`)
- `input_text` (single-line editable text input)
- `input_readonly`
  - supports `align_right`, `value_font_scale`, `muted`
- `progress`
- `begin_dropdown` / `end_dropdown`
- `begin_scroll_area` / `end_scroll_area`
  - drag, wheel, inertia, overscroll bounce, scrollbar options
- `text_area` (editable, selection, caret, scrolling)
- `text_area_readonly`
- Built-in control motion
  - subtle button/tab press motion
  - restrained input focus ring and glow
  - dropdown reveal with rotating chevron
  - slider/thumb, scrollbar thumb, and progress fill easing

### Output / Integration

- `end_frame()` returns `std::vector<DrawCommand>`
- `take_frame(...)` for moving frame buffers out efficiently
- `text_arena()` returns text storage used by text commands

## Repository Layout

```text
EUI/
|- .github/
|  `- workflows/
|     `- release.yml
|- cmake/
|  `- EUIConfig.cmake.in
|- docs/
|  |- backend-abstraction-checklist.md
|  |- backend-abstraction-issue.md
|  |- concise-ui-syntax-proposal.zh-CN.md
|  |- framework-revamp-proposal.zh-CN.md
|  |- include-cleanup-record.zh-CN.md
|  |- project-structure.zh-CN.md
|  `- quick-decoupling-status.zh-CN.md
|- include/
|  `- EUI.h
|  |- stb_truetype.h
|  `- Font Awesome 7 Free-Solid-900.otf
|- examples/
|  |- anchor_layout_demo.cpp
|  |- basic_widgets_demo.cpp
|  |- calculator_demo.cpp
|  |- graphics_showcase_demo.cpp
|  `- sidebar_navigation_demo.cpp
|- CMakeLists.txt
|- readme.md
`- readme.zh-CN.md
```

## Build

Recommended generator: `Ninja`.

### 1) Build core only (no window backend required)

```bash
cmake -S . -B build -G Ninja -DEUI_BUILD_EXAMPLES=OFF
cmake --build build
```

Targets:

- `EUI::eui` (interface)

### 2) Build examples (OpenGL + GLFW / SDL2)

```bash
cmake -S . -B build -G Ninja -DEUI_BUILD_EXAMPLES=ON
cmake --build build
```

When OpenGL and at least one enabled window backend are available, CMake creates:

- `basic_widgets_demo` (`examples/basic_widgets_demo.cpp`)
- `anchor_layout_demo` (`examples/anchor_layout_demo.cpp`)
- `sidebar_navigation_demo` (`examples/sidebar_navigation_demo.cpp`)
- `calculator_demo` (`examples/calculator_demo.cpp`)
- `graphics_showcase_demo` (`examples/graphics_showcase_demo.cpp`)

Executable names are generated directly from the corresponding `examples/*.cpp` filename.
Legacy names such as `eui_demo` and `eui_minimal_demo` are kept only as CMake compatibility targets.

Prefer the higher-level backend mode option:

- `-DEUI_WINDOW_BACKEND=AUTO`
  - Recommended default mode.
  - Prefer a locally installed SDL2 first.
  - Fall back to GLFW when SDL2 is not available locally.
  - It does not fetch dependencies from the network by default.
  - If neither backend is available locally, fetching is attempted only when you explicitly enable the fetch options.
- `-DEUI_WINDOW_BACKEND=GLFW`
  - Build the GLFW window backend only.
- `-DEUI_WINDOW_BACKEND=SDL2`
  - Build the SDL2 window backend only.
- `-DEUI_WINDOW_BACKEND=ALL`
  - Try to wire both backends, then choose at runtime through `AppOptions::window_backend`.

Important options:

```bash
-DEUI_BUILD_EXAMPLES=ON|OFF
-DEUI_STRICT_WARNINGS=ON|OFF
-DEUI_WINDOW_BACKEND=AUTO|GLFW|SDL2|ALL
-DEUI_FETCH_GLFW_FROM_GIT=ON|OFF
-DEUI_FETCH_SDL2_FROM_GIT=ON|OFF
-DEUI_ENABLE_GLFW_WINDOW_BACKEND=ON|OFF
-DEUI_ENABLE_SDL2_WINDOW_BACKEND=ON|OFF
-DEUI_GLFW_GIT_TAG=3.4
-DEUI_SDL2_GIT_TAG=SDL2
-DEUI_ENABLE_STB_TRUETYPE=1|0
```

Where:

- `EUI_WINDOW_BACKEND` is the recommended entry point.
- `EUI_ENABLE_GLFW_WINDOW_BACKEND` / `EUI_ENABLE_SDL2_WINDOW_BACKEND` remain as lower-level compatibility overrides.
- `EUI_FETCH_GLFW_FROM_GIT` / `EUI_FETCH_SDL2_FROM_GIT` both default to `OFF`.

If network/Git access is restricted:

```bash
cmake -S . -B build -G Ninja -DEUI_BUILD_EXAMPLES=ON -DEUI_FETCH_GLFW_FROM_GIT=OFF
```

Common setups:

```bash
# Default: prefer local SDL2, otherwise use GLFW
cmake -S . -B build -G Ninja -DEUI_WINDOW_BACKEND=AUTO

# Force GLFW even if SDL2 exists locally
cmake -S . -B build -G Ninja -DEUI_WINDOW_BACKEND=GLFW

# Force SDL2 and auto-fetch it if missing locally
cmake -S . -B build -G Ninja -DEUI_WINDOW_BACKEND=SDL2 -DEUI_FETCH_SDL2_FROM_GIT=ON

# Build both backends and choose at runtime
cmake -S . -B build -G Ninja -DEUI_WINDOW_BACKEND=ALL
```

If you use the VSCode CMake Tools sidebar, the repo also ships matching presets:

- `auto`
- `glfw`
- `sdl2`
- `all`
- `auto-fetch`
- `glfw-fetch`
- `sdl2-fetch`
- `all-fetch`

The first four stay local-only and do not fetch by default.
The `*-fetch` presets are the ones that allow Git downloads for missing backends.

## Release Packaging

GitHub Actions now includes an automated release workflow at [`.github/workflows/release.yml`](.github/workflows/release.yml).
Any pushed tag that starts with `v` triggers a release build on Windows and Linux, then runs CMake packaging through `cpack`.
The same workflow can also be started manually from the GitHub Actions page by providing a `release_tag`, which is useful when you need to rebuild and re-upload assets for an existing release.

Typical release flow:

```bash
git tag v0.1.0
git push origin v0.1.0
```

Produced release assets:

- Windows package: `.zip`
- Linux package: `.tar.gz`

The generated packages install:

- `include/`
- CMake package config files (`EUIConfig.cmake`, `EUIConfigVersion.cmake`, exported targets)
- `readme.md` and `readme.zh-CN.md`
- `docs/`
- `examples/`
- example executables when example targets are enabled during packaging

## Run Examples

```bash
# basic widgets
cmake --build build --target basic_widgets_demo

# calculator example
cmake --build build --target calculator_demo

# anchor layout
cmake --build build --target anchor_layout_demo

# sidebar navigation example
cmake --build build --target sidebar_navigation_demo

# graphics showcase example
cmake --build build --target graphics_showcase_demo
```

## Minimal Quick Usage

```cpp
#include "EUI.h"

eui::Context ctx;
eui::InputState input{};
std::string value_text = "0.50";

ctx.begin_frame(1280.0f, 720.0f, input);
ctx.set_theme_mode(eui::ThemeMode::Dark);

eui::quick::UI ui(ctx);
ui.panel("Demo")
    .in(eui::Rect{20.0f, 20.0f, 640.0f, 220.0f})
    .padding(16.0f)
    .begin([&](auto& panel) {
        const auto row = ui.split_h_ratio(panel.content(), 0.50f, 12.0f);
        ui.button("Run").in(row.first).primary().draw();
        ui.input("Value", value_text).in(row.second).draw();
        ui.progress("Loading", 0.42f).height(8.0f).draw();
    });

const auto& commands = ctx.end_frame();
const auto& text_arena = ctx.text_arena();
```

## Common Layout Recipes

### Width Rules (Important)

- Item width is controlled by layout, not widget args.
- `row().items({...})` declares the width model for the current strip.
- `ui.fixed(px)` locks a slot to a fixed width.
- `ui.flex(weight)` shares the remaining width with other flex slots.
- `ui.content(min, max)` keeps a slot content-sized inside explicit bounds.
- `scope(rect, ...)` temporarily routes layout into a sub-rect without mutating global coordinates.
- `dock_*` consumes edges in order, while `split_*` branches a rect explicitly.

### Sidebar Icon/Text Vertical Alignment

- For left-aligned sidebar buttons, prefix label with `\t` to enable left align with built-in left padding.
- For icon + text, use **two ASCII spaces** between them (for example `u8"\uF015  Dashboard"`).
- EUI will split icon/text and render them separately, which keeps vertical alignment stable.
- See `examples/sidebar_navigation_demo.cpp` for a minimal left-sidebar + page-transition sample.

```cpp
// Left-aligned nav item with icon + text (stable vertical centering)
ui.button("\t" u8"\uF015  Dashboard", eui::ButtonStyle::Secondary, 34.0f);
```

### 1) Sidebar + Main Content

```cpp
ui.panel("Workspace")
    .in(frame_rect)
    .padding(16.0f)
    .begin([&](auto& page) {
        const Rect sidebar = page.dock_left(220.0f, 16.0f);
        const Rect main = page.content();

        ui.card("Navigation")
            .in(sidebar)
            .begin([&](auto&) {
                ui.button("Dashboard").ghost().height(34.0f).draw();
                ui.button("Projects").ghost().height(34.0f).draw();
                ui.button("Settings").ghost().height(34.0f).draw();
            });

        ui.card("Overview")
            .in(main)
            .begin([&](auto&) {
                ui.label("Main content area").draw();
            });
    });
```

### 2) Top Bar (Left / Right)

```cpp
ui.card("Top bar")
    .in(rect)
    .begin([&](auto&) {
        ui.row()
            .items({
                ui.fixed(96.0f),
                ui.fixed(96.0f),
                ui.flex(1.0f),
                ui.fixed(120.0f),
                ui.fixed(120.0f),
            })
            .gap(8.0f)
            .align_center()
            .begin([&] {
                ui.button("Back").ghost().height(34.0f).draw();
                ui.button("Forward").ghost().height(34.0f).draw();
                ui.label("Search / breadcrumbs / page title").muted().draw();
                ui.button("Search").ghost().height(34.0f).draw();
                ui.button("Profile").primary().height(34.0f).draw();
            });
    });
```

### 3) Three-Zone Toolbar (Left / Center / Right)

```cpp
ui.card("Toolbar")
    .in(rect)
    .begin([&](auto&) {
        ui.row()
            .items({
                ui.content(150.0f, 220.0f),
                ui.flex(1.0f),
                ui.content(160.0f, 240.0f),
            })
            .gap(12.0f)
            .align_center()
            .begin([&] {
                ui.readonly("Left", "New / Save").height(34.0f).draw();
                ui.label("Build #128").font(13.0f).muted().draw();
                ui.readonly("Right", "Run / Deploy").height(34.0f).draw();
            });
    });
```

### 4) Two-Column Settings Page

```cpp
std::string gamma = "2.2";
std::string exposure = "124";

const auto columns = ui.split_h_ratio(rect, 0.50f, 10.0f);

ui.card("General")
    .in(columns.first)
    .begin([&](auto&) {
        ui.input("Gamma", gamma).height(36.0f).draw();
    });

ui.card("Display")
    .in(columns.second)
    .begin([&](auto&) {
        ui.input("Exposure", exposure).height(36.0f).draw();
    });
```

### 5) Anchor a Badge to the Previous Card

```cpp
ui.metric("Build", "Ready")
    .in(rect)
    .tag("LIVE")
    .draw();

const Rect badge = ui.anchor()
    .to_last()
    .top(12.0f)
    .right(12.0f)
    .size(72.0f, 24.0f)
    .resolve();

ui.shape()
    .in(badge)
    .radius(12.0f)
    .fill(0x16A34A)
    .draw();

ui.text("SYNC")
    .in(badge)
    .font(11.0f)
    .center()
    .draw();
```

## Optional App Runtime Usage

```cpp
#define EUI_BACKEND_OPENGL
#define EUI_PLATFORM_GLFW
#include "EUI.h"

int main() {
    eui::app::AppOptions options{};
    options.width = 960;
    options.height = 710;
    options.title = "EUI App";
    options.vsync = true;
    options.continuous_render = false;
    options.max_fps = 240.0;

    options.text_font_family = "Segoe UI";
    options.text_font_weight = 600; // 100-900, larger = bolder
    options.icon_font_family = "Font Awesome 7 Free Solid";
    options.icon_font_file = "include/Font Awesome 7 Free-Solid-900.otf";
    options.text_backend = eui::app::AppOptions::TextBackend::Auto;
    // Optional but recommended on non-Windows: set explicit font file paths.
    // options.text_font_file = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
    // options.icon_font_file = "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf";
    options.enable_icon_font_fallback = true;

    return eui::app::run(
        [&](eui::app::FrameContext frame) {
            auto& ctx = frame.context();
            ctx.set_theme_mode(eui::ThemeMode::Dark);

            eui::quick::UI ui(ctx);
            ui.panel("Demo")
                .in(eui::Rect{20.0f, 20.0f, 320.0f, 180.0f})
                .padding(16.0f)
                .begin([&](auto&) {
                    ui.label("Hello EUI").draw();
                    ui.spacer(10.0f);
                    ui.button("Run").primary().height(36.0f).draw();
                });

            // request_next_frame() if animation is needed in event-driven mode.
            frame.request_next_frame();
        },
        options
    );
}
```

### Text Backend Notes

- `AppOptions::TextBackend::Auto`
  - On Windows, prefers the STB true-type path when `stb_truetype` is enabled, then falls back to Win32, and only then to the built-in bitmap glyphs.
  - On non-Windows, uses the STB text path when available.
- `AppOptions::TextBackend::Stb`
  - Uses `stb_truetype` glyph rasterization and atlas caching.
- `AppOptions::TextBackend::Win32`
  - Windows-only text renderer based on GDI/WGL font APIs.

For best icon coverage, keep `enable_icon_font_fallback = true` and ship an explicit icon font file.

## Notes

- `index.html` is a visual/prototype reference, not part of C++ build output.
- Keep source files in UTF-8 to avoid C4819/garbled literal issues on Windows toolchains.
