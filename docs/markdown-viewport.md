# Markdown viewport construction

`MarkdownBuilder::viewport(top, height, overscan = 32)` is an opt-in construction
limit for scrolling read-only documents. Coordinates are logical units relative
to the document content origin, after its outer margin; they are not window or
framebuffer coordinates. The caller still clips its scroll viewport.

```cpp
auto doc = components::markdown(ui, "document")
    .width(contentWidth)
    .markdown(source)
    .theme(tokens);
doc.viewport(documentScrollOffset, viewportHeight).build();
```

Omit `viewport()` for the original complete tree. A zero/negative height builds
an empty tree with the full document bounds. Negative top values are valid;
non-finite arguments fall back to complete construction. Overscan is clamped to
zero or greater. The default includes adjacent lines for glyph overhang; increase
it when custom fonts or effects extend beyond their logical line boxes.

The component preserves full height and the original block/table/segment IDs.
Only nearby blocks, table rows and inline lines become DSL controls, including
when one paragraph contains thousands of segments. Added placement containers
are implementation details. Fixed component height and outer margins retain
their existing meaning. Rendering colors come from the current style, so color
transitions do not invalidate geometry.

For `ScrollView`, keep its height measurement in complete mode and use the actual
scroll root's offset for the real tree (see `tests/fixtures/markdown_viewer.cpp`).
Request recomposition when scrolling changes the offset; moving a previously
culled tree alone cannot reveal new content. Width, font configuration, source
replacement and metric style changes rebuild the layout automatically. Source
comparison is exact, not a hash used as an identity.

A shared UI-thread-local LRU holds at most eight viewport plans with an 8 MiB
accounted budget and a 4 MiB maximum entry. It retains parsed documents, inline
geometry and table row offsets; accounting includes source keys and retained
storage. Oversized plans remain usable for that build but are not retained.
Existing parser/inline caches keep their own bounds. No worker, timer, dependency,
font resource or scheduling facility is added. `TextPrimitive::fontConfigurationRevision()`
allows metric caches to notice a change to `setDefaultFontFiles()`; setting the
same paths is a no-op. Render segment metadata no longer duplicates source
strings for every glyph, avoiding quadratic storage for a single long run.

A viewport tree intentionally omits offscreen text controls. Consumers that derive
selection/copy information by walking controls must request a complete build on
selection start, or keep their own source-based selection mapping. Do not treat
the culled tree as the complete document. This option does not add text selection,
link activation, or an asynchronous parser.

## Validation and reproduction

`markdown_viewport` compares full versus viewport leaf text, stable IDs and frames
at fractional offsets, two widths, font sizes and themes, with CJK/Latin/emoji,
headings, quotes, lists, code, tables, a long single paragraph and a long table.
It checks edits, replacement, font configuration and cache churn. The optional
`--benchmark` reports build/layout/teardown time and node count for the same
18 KiB paragraph, five warmups and 60 samples. It excludes the geometry oracle's
map-copying work and does not report GPU frame time.

`markdown_viewport_probe` compares actual RGBA framebuffer bytes in OpenGL for
twelve theme/DPI/scroll combinations. It returns 77 on Vulkan, where the portable
geometry test and the Markdown viewer still run; the readback probe does not
claim Vulkan pixel equivalence. Run each backend's CTest suite and launch the
viewer in a real window or an isolated X server. Installed SDK consumers must
also compile the new public API.

Exact environment, paired results, four-backend checks and known limits are
recorded in [the paired measurement](benchmarks/markdown-viewport-20261023.md). Software rendering and component construction
measurements do not guarantee 60 FPS for arbitrary fonts, documents or devices.
