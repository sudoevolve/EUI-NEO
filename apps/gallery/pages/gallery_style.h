struct GalleryStylePage {
    enum class MetricPreview {
        Typography,
        Spacing,
        Radius,
        Control
    };

    struct MetricEntry {
        const char* name;
        float value;
    };

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

    void metricToken(eui::Ui& ui,
                     const std::string& id,
                     const MetricEntry& entry,
                     float width,
                     MetricPreview preview) {
        const components::theme::ThemeMetricTokens& metrics = themeColors().metrics;
        const float cardHeight = 98.0f;
        const float nameWidth = std::max(1.0f, width - metrics.spacing.section);
        const float measuredNameWidth = core::TextPrimitive::measureTextWidth(
            entry.name, {}, metrics.typography.hint, 400);
        const float nameFontSize = measuredNameWidth > nameWidth
            ? std::max(7.0f, metrics.typography.hint * nameWidth / measuredNameWidth)
            : metrics.typography.hint;
        ui.stack(id)
            .size(width, cardHeight)
            .content([&] {
                ui.rect(id + ".bg")
                    .size(width, cardHeight)
                    .color(surface())
                    .radius(metrics.radius.card)
                    .border(metrics.spacing.hairline, borderColor(0.72f))
                    .build();

                if (preview == MetricPreview::Typography) {
                    ui.text(id + ".preview.text")
                        .position(metrics.spacing.content, metrics.spacing.tiny)
                        .size(std::max(0.0f, width - metrics.spacing.panel), 48.0f)
                        .text("Aa")
                        .fontSize(entry.value)
                        .lineHeight(entry.value + metrics.spacing.micro)
                        .color(accent())
                        .horizontalAlign(eui::HorizontalAlign::Center)
                        .verticalAlign(eui::VerticalAlign::Center)
                        .build();
                } else if (preview == MetricPreview::Radius) {
                    ui.rect(id + ".preview.radius")
                        .position(std::max(0.0f, (width - 48.0f) * 0.5f), metrics.spacing.content)
                        .size(48.0f, 30.0f)
                        .color(withAlpha(accent(), 0.72f))
                        .radius(entry.value)
                        .build();
                } else {
                    const float previewWidth = std::min(std::max(metrics.spacing.hairline, entry.value),
                                                        std::max(0.0f, width - metrics.spacing.panel));
                    const float previewHeight = preview == MetricPreview::Control
                        ? std::min(30.0f, std::max(metrics.spacing.tiny, entry.value))
                        : metrics.spacing.small;
                    ui.rect(id + ".preview.bar")
                        .position(std::max(0.0f, (width - previewWidth) * 0.5f),
                                  metrics.spacing.content + (30.0f - previewHeight) * 0.5f)
                        .size(previewWidth, previewHeight)
                        .color(accent())
                        .radius(metrics.radius.full)
                        .build();
                }

                ui.text(id + ".name")
                    .position(metrics.spacing.compact, 56.0f)
                    .size(nameWidth, metrics.spacing.large)
                    .text(entry.name)
                    .fontSize(nameFontSize)
                    .lineHeight(nameFontSize + metrics.spacing.micro)
                    .color(textPrimary())
                    .horizontalAlign(eui::HorizontalAlign::Center)
                    .build();

                ui.text(id + ".value")
                    .position(metrics.spacing.compact, 76.0f)
                    .size(std::max(0.0f, width - metrics.spacing.section), metrics.typography.control)
                    .text(std::to_string(static_cast<int>(std::round(entry.value))) + " px")
                    .fontSize(metrics.typography.caption)
                    .lineHeight(metrics.typography.caption + metrics.spacing.micro)
                    .color(textMuted())
                    .horizontalAlign(eui::HorizontalAlign::Center)
                    .build();
            })
            .build();
    }

    void metricSection(eui::Ui& ui,
                       const std::string& id,
                       const std::string& title,
                       const MetricEntry* entries,
                       int count,
                       float width,
                       float rowWidth,
                       float cardWidth,
                       float gap,
                       MetricPreview preview) {
        const components::theme::ThemeMetricTokens& metrics = themeColors().metrics;
        ui.text(id + ".title")
            .size(width, metrics.spacing.header)
            .text(title)
            .fontSize(metrics.typography.heading)
            .lineHeight(metrics.spacing.header)
            .color(textPrimary())
            .build();

        ui.flow(id + ".tokens")
            .width(rowWidth)
            .height(eui::SizeValue::wrapContent())
            .gap(gap)
            .lineGap(gap)
            .content([&] {
                for (int index = 0; index < count; ++index) {
                    metricToken(ui, id + ".token." + std::to_string(index), entries[index], cardWidth, preview);
                }
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
        const components::theme::FieldVisualTokens fieldVisuals = components::theme::fieldVisuals(tokens);
        const auto& metrics = tokens.metrics;
        const MetricEntry typographyTokens[] = {
            {"micro", metrics.typography.micro},
            {"caption", metrics.typography.caption},
            {"hint", metrics.typography.hint},
            {"label", metrics.typography.label},
            {"option", metrics.typography.option},
            {"body", metrics.typography.body},
            {"input", metrics.typography.input},
            {"control", metrics.typography.control},
            {"cardTitle", metrics.typography.cardTitle},
            {"subtitle", metrics.typography.subtitle},
            {"title", metrics.typography.title},
            {"heading", metrics.typography.heading},
            {"headline", metrics.typography.headline},
            {"displayCompact", metrics.typography.displayCompact},
            {"display", metrics.typography.display},
            {"hero", metrics.typography.hero}
        };
        const MetricEntry lineGapTokens[] = {
            {"lineGapTight", metrics.typography.lineGapTight},
            {"lineGap", metrics.typography.lineGap},
            {"lineGapRelaxed", metrics.typography.lineGapRelaxed},
            {"lineGapLoose", metrics.typography.lineGapLoose},
            {"lineGapComfortable", metrics.typography.lineGapComfortable},
            {"lineGapWide", metrics.typography.lineGapWide}
        };
        const MetricEntry spacingTokens[] = {
            {"hairline", metrics.spacing.hairline},
            {"micro", metrics.spacing.micro},
            {"tiny", metrics.spacing.tiny},
            {"small", metrics.spacing.small},
            {"compact", metrics.spacing.compact},
            {"control", metrics.spacing.control},
            {"content", metrics.spacing.content},
            {"section", metrics.spacing.section},
            {"large", metrics.spacing.large},
            {"panel", metrics.spacing.panel},
            {"header", metrics.spacing.header},
            {"page", metrics.spacing.page},
            {"overlay", metrics.spacing.overlay}
        };
        const MetricEntry radiusTokens[] = {
            {"micro", metrics.radius.micro},
            {"tiny", metrics.radius.tiny},
            {"small", metrics.radius.small},
            {"control", metrics.radius.control},
            {"tooltip", metrics.radius.tooltip},
            {"popup", metrics.radius.popup},
            {"card", metrics.radius.card},
            {"elevated", metrics.radius.elevated},
            {"overlay", metrics.radius.overlay},
            {"section", metrics.radius.section},
            {"feature", metrics.radius.feature},
            {"full", metrics.radius.full}
        };
        const MetricEntry controlTokens[] = {
            {"progress", metrics.control.progress},
            {"indicator", metrics.control.indicator},
            {"switchHeight", metrics.control.switchHeight},
            {"compact", metrics.control.compact},
            {"menuItem", metrics.control.menuItem},
            {"field", metrics.control.field},
            {"segmented", metrics.control.segmented},
            {"input", metrics.control.input},
            {"control", metrics.control.control},
            {"large", metrics.control.large},
            {"switchWidth", metrics.control.switchWidth},
            {"navigation", metrics.control.navigation},
            {"scrollbar", metrics.control.scrollbar}
        };
        const MetricEntry pageVisualMetrics[] = {
            {"headerTopInset", visuals.headerTopInset},
            {"headerTitleGap", visuals.headerTitleGap},
            {"headerContentGap", visuals.headerContentGap},
            {"headerTitleSize", visuals.headerTitleSize},
            {"headerSubtitleSize", visuals.headerSubtitleSize},
            {"sectionGap", visuals.sectionGap},
            {"sectionInset", visuals.sectionInset},
            {"sectionRounding", visuals.sectionRounding},
            {"labelSize", visuals.labelSize},
            {"fieldHeight", visuals.fieldHeight}
        };
        const MetricEntry fieldVisualMetrics[] = {
            {"rounding", fieldVisuals.rounding},
            {"horizontalInset", fieldVisuals.horizontalInset},
            {"focusLineHeight", fieldVisuals.focusLineHeight},
            {"borderLineHeight", fieldVisuals.borderLineHeight},
            {"popupRounding", fieldVisuals.popupRounding},
            {"popupOverlap", fieldVisuals.popupOverlap},
            {"popupShadowBlur", fieldVisuals.popupShadowBlur},
            {"popupShadowOffsetY", fieldVisuals.popupShadowOffsetY}
        };

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

        metricSection(ui, "style.metric.typography", "Typography Tokens",
                      typographyTokens, static_cast<int>(sizeof(typographyTokens) / sizeof(typographyTokens[0])),
                      width, swatchRowWidth, swatchWidth, swatchGap, MetricPreview::Typography);
        metricSection(ui, "style.metric.lineGap", "Line Gap Tokens",
                      lineGapTokens, static_cast<int>(sizeof(lineGapTokens) / sizeof(lineGapTokens[0])),
                      width, swatchRowWidth, swatchWidth, swatchGap, MetricPreview::Spacing);
        metricSection(ui, "style.metric.spacing", "Spacing Tokens",
                      spacingTokens, static_cast<int>(sizeof(spacingTokens) / sizeof(spacingTokens[0])),
                      width, swatchRowWidth, swatchWidth, swatchGap, MetricPreview::Spacing);
        metricSection(ui, "style.metric.radius", "Radius Tokens",
                      radiusTokens, static_cast<int>(sizeof(radiusTokens) / sizeof(radiusTokens[0])),
                      width, swatchRowWidth, swatchWidth, swatchGap, MetricPreview::Radius);
        metricSection(ui, "style.metric.control", "Control Size Tokens",
                      controlTokens, static_cast<int>(sizeof(controlTokens) / sizeof(controlTokens[0])),
                      width, swatchRowWidth, swatchWidth, swatchGap, MetricPreview::Control);

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
                themeSwatch(ui, "style.color.card", "cardColor", visuals.cardColor, swatchWidth);
                themeSwatch(ui, "style.color.mutedCard", "mutedCardColor", visuals.mutedCardColor, swatchWidth);
                themeSwatch(ui, "style.color.softAccent", "softAccent", visuals.softAccentColor, swatchWidth);
            });

        metricSection(ui, "style.visual.metrics", "Page Visual Metrics",
                      pageVisualMetrics, static_cast<int>(sizeof(pageVisualMetrics) / sizeof(pageVisualMetrics[0])),
                      width, swatchRowWidth, swatchWidth, swatchGap, MetricPreview::Spacing);

        ui.text("style.field.title")
            .size(width, metrics.spacing.header)
            .text("Field Visual Tokens")
            .fontSize(metrics.typography.heading)
            .lineHeight(metrics.spacing.header)
            .color(textPrimary())
            .build();

        ui.flow("style.field.colors")
            .width(swatchRowWidth)
            .height(eui::SizeValue::wrapContent())
            .gap(swatchGap)
            .lineGap(swatchGap)
            .content([&] {
                themeSwatch(ui, "style.field.popupShadowColor", "popupShadowColor",
                            fieldVisuals.popupShadowColor, swatchWidth);
            });

        metricSection(ui, "style.field.metrics", "Field Visual Metrics",
                      fieldVisualMetrics, static_cast<int>(sizeof(fieldVisualMetrics) / sizeof(fieldVisualMetrics[0])),
                      width, swatchRowWidth, swatchWidth, swatchGap, MetricPreview::Spacing);

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
