# Bounded glyph advance cache

Upstream report: [EUI-NEO #113](https://github.com/sudoevolve/EUI-NEO/issues/113).

Text measurement and editable text repeatedly query advances from the same fixed-size
FreeType faces. Previously every occurrence called `FT_Load_Glyph`, including repeated
characters in a paragraph or unchanged characters during an edit. CFF fonts interpret
outlines on that path. The rendered bitmap atlas does not supply these metrics.

Each `FontFace` now holds 512 direct-mapped entries containing the glyph index, raw
advance and height (6 KiB on the supported 32/64-bit targets). A collision replaces one
entry in constant time. Storage is fixed even for random Unicode text. Entries move
with their face and are destroyed with it; changing fonts or physical sizes creates a
separate face. Existing font-stack ownership and cache eviction remain unchanged.

Only successful loads are cached. Tabs and combining marks retain their existing early
returns. Colored fallback glyphs retain per-call height limiting: the cache stores raw
metrics before `glyphScale` and the primary face's height cap. The rendering path still
loads a glyph before reading/rasterizing its current FreeType slot. No public API,
font resource, shaping rule, task, thread or dependency was added.

## Reproduce

Configure the normal Release fixture build, then run:

```bash
cmake --build build-contrib-glfw-opengl --target text_metrics_cache
./build-contrib-glfw-opengl/text_metrics_cache
./build-contrib-glfw-opengl/text_metrics_cache --benchmark
EUI_TEXT_TEST_FONT=/absolute/path/to/NotoSansSC-Regular.otf \
  ./build-contrib-glfw-opengl/text_metrics_cache --benchmark
```

The default test uses the already shipped Chinese TTF. The optional font path supplies
a CFF comparison without adding a resource. The test checks public metrics against
independent FreeType loads over 3000 distinct CJK glyphs, reverse traversal, repeated
calls and 12/20.272/40px size changes. Benchmark cases include short Latin/mixed text,
emoji + combining marks + tabs, ~8 KiB mixed text, 3000 different CJK characters and
8 KiB Latin text, at 20.272 and 46.336 physical pixels. Every sample changes its suffix;
`measureTextMetrics` does not use the whole-string size cache.

Each case has five warmup edits and sixty timed edits, measured with `steady_clock`.
JSON lines report p50/p95 milliseconds plus a fingerprint over all caret coordinates
and UTF-8 byte indices, including warmup. Compare those fingerprints between builds.
Run old/new binaries alternately on one machine without parallel compilation; report
multiple paired runs rather than selecting a single fastest result. A full UI test is
still needed to cover layout, event dispatch, drawing and presentation.

## Validation

Baseline: upstream dev `ad7e9e7`; candidate changes only the advance cache and this test.
Ubuntu 24.04.5 x86_64, Linux 7.0.0-38, Intel Core Ultra 5 225H (14 cores), 30 GiB RAM,
GCC 13.3.0, CMake 3.28.3, bundled FreeType, Release `-O3`. UI/backend checks use private
Xvfb with Mesa 25.2.8 llvmpipe (LLVM 20.1.2), not a hardware-GPU frame-rate test.

Three paired runs alternate before/after, after/before, before/after without concurrent
builds. Below is the median of each run's p50, in milliseconds at 20.272 physical px:

| Case | CFF before | CFF after | Shipped TTF before | TTF after |
| --- | ---: | ---: | ---: | ---: |
| Short Latin | 0.0520 | 0.0026 | 0.0340 | 0.0032 |
| Short mixed | 0.0530 | 0.0024 | 0.0689 | 0.0027 |
| Emoji / combining / tab | 0.1606 | 0.0021 | 0.1555 | 0.0023 |
| Long mixed | 4.9617 | 0.1826 | 5.1829 | 0.2050 |
| 3000 distinct CJK | 9.8736 | 9.7912 | 16.4699 | 15.7673 |
| Long Latin | 8.4241 | 0.3652 | 4.1567 | 0.4136 |

The 46.336px cases show the same pattern: common text improves, while a working set
larger than the fixed table remains effectively unchanged. All 72 paired fingerprints
match bit for bit. The independent FreeType oracle passes for both fonts. The raw
p50/p95 values, size, fingerprints and per-invocation peak RSS are in
[data/text-metrics-cache-20261017.jsonl](data/text-metrics-cache-20261017.jsonl).
Peak RSS ranges: CFF 24588–24776 -> 24636–24896 KiB; TTF 23500–23556 -> 23660–23852 KiB.
These are whole-process samples, not proof of a global memory ceiling; storage added
by this cache is bounded per live face. There is no claim that caching removes the
cost of arbitrary unique glyphs or an application's entire page composition.

All four independent Release builds (GLFW/SDL2 x OpenGL/Vulkan) pass **36/36** unit and
backend probe tests, and each installed SDK's consumer passes **1/1**. Tests ran with
separate TMPDIR/XDG directories: an initial concurrent run shared the image-cache
fixture's deterministic temporary filenames, causing one `image_cache` failure; all
four isolated full reruns pass. The fixture was not changed in this patch.

A downstream native input suite passes Release 5/5, ASAN 4/4 and UBSAN 4/4. Four
normal/minimum, light/dark view captures are byte-identical to before. Those UI checks
include selection, scrolling, previews, input editing and physical font-size layout.
This CPU measurement does not establish a desktop frame-rate guarantee.
