#include "eui_neo.h"

#include <algorithm>

namespace app {
namespace {

struct MarkdownViewerState {
    float scrollOffset = 0.0f;
};

MarkdownViewerState& state() {
    static MarkdownViewerState value;
    return value;
}

const char* markdownSample() {
    return R"(# Markdown Viewer

EUI-NEO renders Markdown through MD4C callbacks mapped onto DSL primitives.

## Headings

### H3 Heading
#### H4 renders with compact heading style
##### H5 renders with compact heading style
###### H6 renders with compact heading style

## Paragraphs

This paragraph is intentionally long enough to wrap across multiple lines. It should keep readable spacing on both sides of the card and should never touch the right edge, even when the scrollbar appears. Entity coverage: &amp; &lt; &gt; &quot; &#169;.

Soft
line
breaks are folded into readable text, while hard breaks remain visible:
first line\
second line after a Markdown hard break.

Inline source coverage: **strong text**, *emphasis*, ***strong emphasis***, __underline when extension is enabled__, `inline code`, [a link](https://example.com "title text"), <https://example.com/autolink>, www.example.com, ~~strikethrough~~, $a^2 + b^2 = c^2$, [[Wiki Target]], <span>inline HTML</span>, and an escaped asterisk \*.

![Image alt text](assets/eui-icon.png "image title")

## Lists

- Bullet item one
- Plus and star markers are parsed by MD4C too.
* Star bullet item.
+ Plus bullet item.
- Bullet item two with longer wrapped text so spacing and right padding can be checked inside the card.
  - Nested bullet item
  - Another nested item
- Bullet item after a nested list to verify indentation resets.

3. Ordered list can start from an arbitrary number.
4. The next item should keep the ordered counter.
5. This longer ordered item wraps and still leaves right-side breathing room.

- [x] Task list item checked
- [ ] Task list item unchecked

## Block Quote

> Components should compose `Column`, `Row`, `Stack`, `Rect`, and `Text`.
> The render backend does not need to know about Markdown.
> This line is intentionally longer so quote wrapping and quote right padding are visible.

> Nested quote level one
>> Nested quote level two keeps its own left inset.

## Code Block

```cpp
components::markdown(ui, "doc")
    .theme(components::theme::dark())
    .width(720.0f)
    .markdown(markdownText)
    .build();
```

```text
Plain fenced code keeps whitespace:
    indented line
    another indented line
```

    Indented code block is also parsed.
    It should render as a code block without a fence language.

## Table

| Feature | Centered Status | Right Aligned Edge Case |
| :--- | :---: | ---: |
| Headings | Parsed | H1 through H6 |
| Lists | Parsed | Bullet, ordered, nested, task |
| Inline spans | Styled | **strong**, *emphasis*, `code`, [link](https://example.com) |
| Tables | Parsed | Header, body rows, and alignment |
| Long cell text | Wrapped | This cell is deliberately long so table cell padding and wrapping can be checked against the right edge. |

## HTML

<div>Inline HTML is preserved as text in this MVP.</div>
<!-- HTML comment is also passed through MD4C text callbacks. -->

## Display Math

$$
E = mc^2
$$

---

End of Markdown sample. Scroll back to the top and bottom to verify the full MD4C callback surface.)";
}

} // namespace

const DslAppConfig& dslAppConfig() {
    static const DslAppConfig config = DslAppConfig{}
        .title("Markdown Viewer")
        .pageId("markdown_viewer")
        .clearColor({0.07f, 0.08f, 0.10f, 1.0f})
        .windowSize(960, 760)
        .showDebugStatsInTitle(false)
        .fps(0.0)
        .iconPath("");
    return config;
}

void compose(eui::Ui& ui, const eui::Screen& screen) {
    const components::theme::ThemeColorTokens tokens = components::theme::dark();
    MarkdownViewerState& viewer = state();
    const float cardMargin = 40.0f;
    const float cardInset = 28.0f;
    const float minCardWidth = std::min(420.0f, std::max(320.0f, screen.width - cardMargin * 2.0f));
    const float cardWidth = std::max(minCardWidth, std::min(920.0f, screen.width - cardMargin * 2.0f));
    const float cardHeight = std::max(360.0f, screen.height - 64.0f);
    const float viewportWidth = std::max(0.0f, cardWidth - cardInset * 2.0f);
    const float viewportHeight = std::max(0.0f, cardHeight - cardInset * 2.0f);
    const float x = std::max(0.0f, (screen.width - cardWidth) * 0.5f);
    const float y = std::max(0.0f, (screen.height - cardHeight) * 0.5f);

    ui.rect("background")
        .size(screen.width, screen.height)
        .gradient({0.07f, 0.08f, 0.10f, 1.0f}, {0.10f, 0.11f, 0.14f, 1.0f}, eui::GradientDirection::Vertical)
        .build();

    ui.stack("page")
        .x(x)
        .y(y)
        .width(cardWidth)
        .height(cardHeight)
        .content([&] {
            ui.rect("panel")
                .size(cardWidth, cardHeight)
                .color({0.12f, 0.13f, 0.16f, 1.0f})
                .radius(12.0f)
                .border(1.0f, {0.24f, 0.27f, 0.33f, 1.0f})
                .shadow(24.0f, 0.0f, 10.0f, {0.0f, 0.0f, 0.0f, 0.24f})
                .build();

            ui.stack("viewport")
                .position(cardInset, cardInset)
                .size(viewportWidth, viewportHeight)
                .content([&] {
                    components::scrollView(ui, "markdown.scroll")
                        .theme(tokens)
                        .size(viewportWidth, viewportHeight)
                        .offset(viewer.scrollOffset)
                        .contentKey("markdown.viewer")
                        .gap(0.0f)
                        .step(56.0f)
                        .scrollbarWidth(8.0f)
                        .scrollbarGap(20.0f)
                        .onChange([&viewer](float value) {
                            viewer.scrollOffset = value;
                        })
                        .content([&](eui::Ui& contentUi, float contentWidth, float) {
                            components::markdown(contentUi, "markdown")
                                .theme(tokens)
                                .width(contentWidth)
                                .wrapContentHeight()
                                .markdown(markdownSample())
                                .zIndex(1)
                                .build();
                        })
                        .build();
                })
                .build();
        })
        .build();
}

} // namespace app
