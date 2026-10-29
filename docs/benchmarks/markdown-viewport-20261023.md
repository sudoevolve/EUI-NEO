# Markdown viewport paired measurement (2026-10-23)

Ubuntu 24.04, Linux 7.0.0-38-generic, Intel Core Ultra 5 225H (14 CPUs),
30 GiB RAM, GCC 13.3.0, CMake 3.28.3, Ninja, GLFW/OpenGL Release
(`-O3 -DNDEBUG`, EUI's target `-Os -fno-exceptions -fno-rtti`). The CPU-only
component benchmark creates no window/GPU; actual viewer and pixel checks used
Xvfb/Mesa llvmpipe. Host desktop processes remained active; no frequency pinning.

The baseline is upstream dev **22f0ca35d19c06e989d64998ce18a27101ffd55c**'s
`markdown.h`; both benchmark executables link the identical current core library.
The core changes only add font-configuration invalidation and do not change
measurement metrics. This isolates the Markdown header algorithm. It is not a
comparison against another OS, renderer, font, or Release configuration.

Input: repeat `中文 English 😀 e\xCC\x81 ` until at least 18,000 UTF-8 bytes,
width 420, default MarkdownStyle, position (11,7), margin (3,5,7,9). Viewport:
top 500, height 180, overscan 32. Each process: five warmups, 60 timed builds,
layout, node counting and teardown. The geometry oracle's map-copying is excluded.
Steady uses the same paragraph; append adds the changing decimal sample index.
Three paired AB/BA/AB rounds per font/workload; table values are the median of
three per-run p50/p95 values, not a pooled percentile. RSS is per-process maximum
from `/usr/bin/time`, including startup, warmups and font loading.

| Font/workload | Baseline p50/p95 ms | Viewport p50/p95 ms | Nodes old/new | RSS old/new KiB |
| --- | --- | --- | --- | --- |
| Bundled TTF, steady | 88.246 / 90.208 | 0.693 / 0.842 | 9003 / 400 | 112060 / 14980 |
| Bundled TTF, append | 88.625 / 91.130 | 22.240 / 22.680 | 9005 / 400 | 112352 / 21600 |
| Noto Sans SC CFF, steady | 95.153 / 96.614 | 0.739 / 0.793 | 9003 / 432 | 111524 / 14668 |
| Noto Sans SC CFF, append | 94.791 / 96.608 | 25.253 / 25.700 | 9005 / 432 | 111788 / 20840 |

[Raw chronological samples and font/binary SHA256](markdown-viewport-20261023.json).
The TTF is the existing bundled JingNanJunJunTi asset; CFF is Noto Sans SC 2.004
regular OTF, SHA256 `faa6c9df652116dde789d351359f3d7e5d2285a2b2a1f04a2d7244df706d5ea9`.
No font/resource is added. Reproduce from a Release Ninja fixture build configured
with `CMAKE_EXPORT_COMPILE_COMMANDS=ON` (build `markdown_viewport` first):

```sh
python3 docs/benchmarks/markdown-viewport-pair.py --build /absolute/build/glfw-opengl \
  --font assets/JingNanJunJunTi-JinNanJunJunTi-Bold-2.ttf > paired-ttf.json
python3 docs/benchmarks/markdown-viewport-pair.py --build /absolute/build/glfw-opengl \
  --font /absolute/path/NotoSansSC-Regular.otf > paired-cff.json
```

The script uses that build's exact compiler/link argv, compiles baseline/current
headers against one core, then alternates processes without overlapping builds.
It only uses the Python standard library, Ninja and `/usr/bin/time`; these are
measurement tools, not new product dependencies. Appending still reparses and
lays out the changed paragraph: these results do not establish 60 FPS or GPU frame
time, and do not cover arbitrary documents, hardware GPUs, Windows or compositor input.

Independent local Release builds with fixtures enabled, apps/user apps/tray disabled:

| Backend | Full CTest | Installed SDK consumer | Viewer |
| --- | --- | --- | --- |
| GLFW/OpenGL | 38 passed | 1 passed | mapped, wheel-scrolled, closed |
| SDL2/OpenGL | 38 passed | 1 passed | mapped, wheel-scrolled, closed |
| GLFW/Vulkan | 37 passed, 1 explicit skip | 1 passed | mapped, wheel-scrolled, closed |
| SDL2/Vulkan | 37 passed, 1 explicit skip | 1 passed | mapped, wheel-scrolled, closed |

OpenGL pixel probes compare exact RGBA bytes for twelve light/dark × 1/1.5 DPI ×
scroll combinations. The OpenGL readback probe returns skip 77 on Vulkan;
geometry/ID tests and viewer interaction still run there. No Vulkan pixel-equality
claim. SDL was rerun with `WAYLAND_DISPLAY` unset and `SDL_VIDEODRIVER=x11` so the
final checks used a private X server. All four final viewport unit reruns passed.
The unit was also independently compiled with ASAN (leak detection enabled) and
UBSAN, both passed without a report; these two instrument the changed header,
while linking the same uninstrumented Release core. Full sanitizer library coverage
is left to the consuming application's instrumented build.
