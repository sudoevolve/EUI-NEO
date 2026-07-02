struct GalleryStylePage {
    const char* markdownSample() const {
        return R"(# Markdown card

Common syntax rendered inside the Style page.

## Inline

Use **strong**, *emphasis*, ***strong emphasis***, __underline__, `inline code`, [links](https://example.com), <https://example.com/autolink>, www.example.com, ~~deleted text~~, $a^2 + b^2$, [[Wiki Target]], <span>inline HTML</span>, ![Image alt](assets/eui-icon.png), and entity text like &amp; &lt; &gt;.

## Lists

- Unordered item
- Wrapped item with a longer sentence to check line wrapping in a constrained card.
  - Nested item

1. Ordered item
2. Second ordered item

- [x] Checked task
- [ ] Open task

## Quote

> Markdown composes into normal EUI primitives.

## Code

```cpp
components::markdown(ui, "style.markdown")
    .theme(themeColors())
    .width(contentWidth)
    .markdown(source)
    .build();
```

## Table

| Syntax | Result |
| :--- | :--- |
| `# title` | Heading |
| `- item` | List |
| `` `code` `` | Inline code |
| `~~text~~` | Deleted |
| `$math$` | Math chip |
)";
    }

    void textSample(eui::Ui& ui, const std::string& id, const std::string& text, float size, float height, float width, const eui::Color& color) {
        ui.text(id)
            .size(width, height)
            .text(text)
            .fontSize(size)
            .lineHeight(height)
            .color(color)
            .transition(textTransition())
            .build();
    }

    void themeSwatch(eui::Ui& ui, const std::string& id, const std::string& name, const eui::Color& color, float width) {
        ui.stack(id)
            .size(width, 86.0f)
            .content([&] {
                ui.rect(id + ".bg")
                    .size(width, 86.0f)
                    .color(surface())
                    .radius(12.0f)
                    .border(1.0f, borderColor(0.72f))
                    .build();

                ui.rect(id + ".chip")
                    .x(12.0f)
                    .y(12.0f)
                    .size(std::max(0.0f, width - 24.0f), 26.0f)
                    .color(color)
                    .radius(8.0f)
                    .border(1.0f, withAlpha(textPrimary(), color.a < 0.55f ? 0.20f : 0.08f))
                    .build();

                ui.text(id + ".name")
                    .x(12.0f)
                    .y(44.0f)
                    .size(std::max(0.0f, width - 24.0f), 20.0f)
                    .text(name)
                    .fontSize(14.0f)
                    .lineHeight(18.0f)
                    .color(textPrimary())
                    .horizontalAlign(eui::HorizontalAlign::Center)
                    .build();

                ui.text(id + ".value")
                    .x(12.0f)
                    .y(64.0f)
                    .size(std::max(0.0f, width - 24.0f), 18.0f)
                    .text(colorHex(color))
                    .fontSize(12.0f)
                    .lineHeight(15.0f)
                    .color(textMuted())
                    .horizontalAlign(eui::HorizontalAlign::Center)
                    .build();
            })
            .build();
    }

    void markdownCard(eui::Ui& ui, float width) {
        const float cardWidth = std::max(280.0f, std::min(width, 820.0f));
        const float inset = 22.0f;
        const components::MarkdownStyle markdownStyle(themeColors());
        const std::string markdown = markdownSample();
        const float markdownWidth = std::max(0.0f, cardWidth - inset * 2.0f);
        const float markdownHeight = components::MarkdownBuilder::estimateHeight(markdown, markdownWidth, markdownStyle) + 8.0f;
        const float cardHeight = markdownHeight + inset * 2.0f + 42.0f;

        ui.stack("style.markdown.card")
            .size(cardWidth, cardHeight)
            .content([&] {
                ui.rect("style.markdown.card.bg")
                    .size(cardWidth, cardHeight)
                    .color(surface())
                    .radius(14.0f)
                    .border(1.0f, borderColor(0.72f))
                    .shadow(18.0f, 0.0f, 8.0f, shadowColor(0.18f, 0.08f))
                    .transition(pageTransition())
                    .build();

                ui.text("style.markdown.card.title")
                    .position(inset, inset)
                    .size(std::max(0.0f, cardWidth - inset * 2.0f), 28.0f)
                    .text("Markdown Preview")
                    .fontSize(22.0f)
                    .lineHeight(28.0f)
                    .fontWeight(760)
                    .color(textPrimary())
                    .transition(textTransition())
                    .build();

                components::markdown(ui, "style.markdown.preview")
                    .position(inset, inset + 42.0f)
                    .style(markdownStyle)
                    .width(markdownWidth)
                    .height(markdownHeight)
                    .markdown(markdown)
                    .zIndex(1)
                    .build();
            })
            .build();
    }

    void compose(eui::Ui& ui, float width, float height) {
        (void)height;
        const bool compact = width < 520.0f;
        const float textWidth = std::max(0.0f, std::min(width, 760.0f));
        const float iconGap = 20.0f;
        const int iconColumns = width < 360.0f ? 2 : 4;
        const float iconCardWidth = std::max(60.0f, std::min(120.0f, (width - iconGap * static_cast<float>(iconColumns - 1)) / static_cast<float>(iconColumns)));
        const float iconRowWidth = iconCardWidth * static_cast<float>(iconColumns) + iconGap * static_cast<float>(iconColumns - 1);
        const float swatchGap = 14.0f;
        const int swatchColumns = width < 340.0f ? 1 : (width < 560.0f ? 2 : 4);
        const float swatchWidth = std::max(94.0f, std::min(142.0f, (width - swatchGap * static_cast<float>(swatchColumns - 1)) / static_cast<float>(swatchColumns)));
        const float swatchRowWidth = swatchWidth * static_cast<float>(swatchColumns) + swatchGap * static_cast<float>(swatchColumns - 1);
        const components::theme::ThemeColorTokens tokens = themeColors();
        const components::theme::PageVisualTokens visuals = pageVisuals();

        ui.column("text.samples")
            .width(width)
            .height(eui::SizeValue::wrapContent())
            .gap(12.0f)
            .content([&] {
                textSample(ui, "txt.display", "Display 48 - Gallery Title", compact ? 38.0f : 48.0f, 58.0f, textWidth, textPrimary());
                textSample(ui, "txt.h1", "Heading 36 - Section Header", compact ? 30.0f : 36.0f, 46.0f, textWidth, withAlpha(textPrimary(), 0.92f));
                textSample(ui, "txt.h2", "Heading 28 - Component Name", compact ? 24.0f : 28.0f, 38.0f, textWidth, withAlpha(textPrimary(), 0.82f));
                textSample(ui, "txt.body", "Body 20 - Text can wrap, align and use custom colors.", 20.0f, 30.0f, textWidth, bodyText());
                textSample(ui, "txt.small", "Small 15 - Secondary metadata and compact labels.", 15.0f, 24.0f, textWidth, withAlpha(textPrimary(), 0.58f));

                ui.flow("text.icons")
                    .width(iconRowWidth)
                    .height(eui::SizeValue::wrapContent())
                    .gap(iconGap)
                    .lineGap(14.0f)
                    .content([&] {
                        const unsigned int icons[] = {0xF015, 0xF1FC, 0xF013, 0xF05A};
                        const char* names[] = {"Home", "Theme", "Settings", "Info"};
                        for (int i = 0; i < 4; ++i) {
                            ui.stack(std::string("text.icon.card.") + std::to_string(i))
                                .size(iconCardWidth, 72.0f)
                                .content([&, i] {
                                    ui.text(std::string("text.icon.") + std::to_string(i))
                                        .size(iconCardWidth, 36.0f)
                                        .icon(icons[i])
                                        .fontSize(28.0f)
                                        .lineHeight(34.0f)
                                        .color(accent())
                                        .horizontalAlign(eui::HorizontalAlign::Center)
                                        .transition(textTransition())
                                        .build();

                                    caption(ui, std::string("text.icon.label.") + std::to_string(i), names[i], iconCardWidth, 40.0f);
                                })
                                .build();
                        }
                    });
            });

        ui.text("style.theme.title")
            .size(width, 30.0f)
            .text("Theme Color Tokens")
            .fontSize(25.0f)
            .lineHeight(30.0f)
            .color(textPrimary())
            .build();

        ui.flow("style.theme.tokens.a")
            .width(swatchRowWidth)
            .height(eui::SizeValue::wrapContent())
            .gap(swatchGap)
            .lineGap(swatchGap)
            .content([&] {
                themeSwatch(ui, "style.color.background", "background", tokens.background, swatchWidth);
                themeSwatch(ui, "style.color.primary", "primary", tokens.primary, swatchWidth);
                themeSwatch(ui, "style.color.surface", "surface", tokens.surface, swatchWidth);
                themeSwatch(ui, "style.color.surfaceHover", "surfaceHover", tokens.surfaceHover, swatchWidth);
            });

        ui.flow("style.theme.tokens.b")
            .width(swatchRowWidth)
            .height(eui::SizeValue::wrapContent())
            .gap(swatchGap)
            .lineGap(swatchGap)
            .content([&] {
                themeSwatch(ui, "style.color.surfaceActive", "surfaceActive", tokens.surfaceActive, swatchWidth);
                themeSwatch(ui, "style.color.text", "text", tokens.text, swatchWidth);
                themeSwatch(ui, "style.color.border", "border", tokens.border, swatchWidth);
                themeSwatch(ui, "style.color.accent", "pickerAccent", accent(), swatchWidth);
            });

        ui.text("style.visual.title")
            .size(width, 30.0f)
            .text("Page Visual Colors")
            .fontSize(25.0f)
            .lineHeight(30.0f)
            .color(textPrimary())
            .build();

        ui.flow("style.theme.visuals")
            .width(swatchRowWidth)
            .height(eui::SizeValue::wrapContent())
            .gap(swatchGap)
            .lineGap(swatchGap)
            .content([&] {
                themeSwatch(ui, "style.color.title", "titleColor", visuals.titleColor, swatchWidth);
                themeSwatch(ui, "style.color.subtitle", "subtitleColor", visuals.subtitleColor, swatchWidth);
                themeSwatch(ui, "style.color.body", "bodyColor", visuals.bodyColor, swatchWidth);
                themeSwatch(ui, "style.color.softAccent", "softAccent", visuals.softAccentColor, swatchWidth);
            });

        ui.text("style.markdown.title")
            .size(width, 30.0f)
            .text("Markdown Component")
            .fontSize(25.0f)
            .lineHeight(30.0f)
            .color(textPrimary())
            .build();

        markdownCard(ui, width);
    }
};
