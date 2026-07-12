# Modules

EUI-NEO keeps large, optional, or niche features in `modules/` so the core library stays small and predictable. A checkout may include all modules, a subset of modules, or no modules at all.

## Build Behavior

Modules are enabled by default in top-level builds:

```sh
cmake -S . -B build -DEUI_ENABLE_MODULES=ON
```

If `modules/` or a module subdirectory is missing, CMake skips it without failing. Set `-DEUI_ENABLE_MODULES=OFF` to disable module discovery entirely. Bundled examples that depend on a module are skipped when that module is not available.

Known module folders:

- `modules/keyboard`: on-screen keyboard components and bindings.
- `modules/media`: reserved for media-facing optional features.
- `modules/serial`: reserved for serial-port optional features.

Each module owns its own `CMakeLists.txt`, README, headers, examples, and tests when needed.

## Using A Module

Include the module header and instantiate the module object from application code. For the keyboard module, bind input component ids when the keyboard is created:

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

The keyboard opens when a bound input receives focus, writes text back to the bound value, supports multiline Enter bindings, theme/accent updates, dragging, and edge resizing.

## Development Policy

Put a feature in a module when it is large, platform-specific, dependency-heavy, experimental, or useful only to a narrow set of applications. Examples include virtual keyboards, media playback/recording, serial devices, hardware integrations, and specialized tools.

Keep core changes limited to the small generic hooks a module needs, such as event preservation, builder passthroughs, or optional build discovery. Avoid making every application pay for a feature it does not use.

Before adding a module, check:

- It can be absent without breaking configure, build, install, or examples.
- Bundled examples that depend on it are optional and are skipped when it is unavailable.
- It does not introduce required dependencies into `eui::neo`.
- Its public API is simple enough for applications to instantiate and configure directly.
- Its examples and tests are optional and do not assume the module exists in downstream copies.
