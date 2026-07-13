# Vendored Third-Party Sources

`dependencies.cmake` is the only project-owned CMake file in this directory. CMake files inside dependency source directories belong to those upstream projects and are required by their builds.

## Drop-in source discovery

To replace a bundled dependency, unpack one supported upstream source tree directly under `3rd/` and configure the project. The directory name is irrelevant: names such as `zlib`, `zlib-1.3.2`, or `my-zlib-source` are all accepted. No EUI-NEO CMake file needs to be edited.

EUI-NEO recognizes supported libraries by these source signatures:

| Library | Required files |
| --- | --- |
| GLFW | `include/GLFW/glfw3.h`, `src/context.c`, `CMakeLists.txt` |
| glad | `include/glad/glad.h`, `src/glad.c` |
| tray | `tray.h` |
| FreeType | `include/freetype/freetype.h`, `src/base/ftsystem.c`, `CMakeLists.txt` |
| zlib | `zlib.h`, `zconf.h`, `inflate.c` |
| libpng | `png.h`, `pngconf.h`, `png.c`, `CMakeLists.txt` |
| MD4C | `src/md4c.h`, `src/md4c.c` |

Keep only one matching source tree for each library. Configuration fails with the matching paths if two versions are present, instead of choosing one unpredictably. Unknown libraries cannot be wired automatically because their target names, source lists, and link contracts are not standardized.

The current snapshots are GLFW 3.4, FreeType 2.13.3, libpng 1.6.43, zlib 1.3.1, MD4C 0.5.3, and pinned glad/tray revisions. `stb_image.h`, `nanosvg.h`, and `nanosvgrast.h` are project-used single-file dependencies.

SDL2 is intentionally not vendored. The default GLFW backend does not require SDL2. When configuring `-DEUI_WINDOW_BACKEND=sdl2`, use a system SDL2 package in `auto` mode or use `-DEUI_DEPS_MODE=fetch` to download the pinned SDL2 source.

CMake dependency modes:

- `auto` (default): discover supported sources under `3rd/`, and fetch only missing dependencies.
- `bundled`: only use discovered sources under `3rd/`; fail if a required dependency is missing.
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
