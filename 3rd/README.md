# Vendored Third-Party Sources

EUI-NEO prefers these local source snapshots by default, so a normal CMake configure does not download build-time dependencies while the `3rd/` sources are present.

`dependencies.cmake` is the project-owned dependency wiring file. It keeps third-party discovery, fetch fallback, and bundled warning isolation out of the root `CMakeLists.txt`.

Bundled build dependencies:

- `glfw`: GLFW 3.4
- `glad`: `libigl/libigl-glad` snapshot `651a425101365aa6e8504988ef9bb363d066c5ee`
- `tray`: `zserge/tray` snapshot `8dd1358b92562faf7c032cf5362fa97cbc7e13e9`
- `freetype`: FreeType 2.13.3, trimmed to build-required sources
- `libpng-1.6.43`: libpng 1.6.43, trimmed to build-required sources
- `zlib-1.3.1`: zlib 1.3.1, trimmed to build-required sources
- single-file headers already used by the project, such as `stb_image.h`, `stb_truetype.h`, and `nanosvg.h`

SDL2 is intentionally not vendored. The default GLFW backend does not require SDL2. When configuring `-DEUI_WINDOW_BACKEND=sdl2`, use a system SDL2 package in `auto` mode or use `-DEUI_DEPS_MODE=fetch` to download the pinned SDL2 source.

CMake dependency modes:

- `auto` (default): use `3rd/` when present, and fetch only missing dependencies.
- `bundled`: only use sources under `3rd/`; fail if a bundled dependency is missing.
- `fetch`: fetch build-time dependencies from the pinned upstream URLs.

Strict offline configure:

```sh
cmake -S . -B build -DEUI_DEPS_MODE=bundled
```

Online configure:

```sh
cmake -S . -B build -DEUI_DEPS_MODE=fetch
```

The vendored source trees intentionally exclude upstream tests, generated release assets, large fuzzing fixtures, and documentation that is not needed by the EUI-NEO build.
