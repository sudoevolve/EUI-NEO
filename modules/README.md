# EUI-NEO Modules

`modules/` contains optional feature modules that sit above `core/` and
`components/`.

Modules may compose existing DSL primitives and components, and may expose a
small public API for application code. Platform-specific work must stay behind
module-local boundaries so `include/`, `components/`, and examples do not gain
direct backend dependencies.

Module boundaries:

- `keyboard/`: on-screen keyboard surface and input binding API.
- `serial/`: cross-platform serial discovery, connection, and I/O.

CMake behavior:

- `EUI_ENABLE_MODULES=ON` scans `modules/` by default.
- A module is added only when its directory contains `CMakeLists.txt`.
- Missing module directories are skipped without an error.
- Present modules may start as header-only `INTERFACE` targets and grow into
  compiled targets later.
- Installed modules keep the same `eui::module_*` target names as source-tree
  builds.
