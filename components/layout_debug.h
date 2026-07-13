#pragma once

#include "components/theme.h"
#include "core/dsl.h"

#include <algorithm>
#include <string>

namespace components {

struct LayoutDebugStyle {
    LayoutDebugStyle() = default;

    explicit LayoutDebugStyle(const theme::ThemeColorTokens& tokens) {
        stroke = tokens.metrics.spacing.hairline;
        labelFontSize = tokens.metrics.typography.caption;
        labelLineHeight = tokens.metrics.typography.caption + tokens.metrics.typography.lineGap;
        labelX = tokens.metrics.spacing.compact;
        labelY = tokens.metrics.spacing.small;
        labelHeight = tokens.metrics.typography.control;
    }

    core::Color frame = theme::color(0.96f, 0.32f, 0.38f, 0.64f);
    core::Color padding = theme::color(0.95f, 0.70f, 0.20f, 0.34f);
    core::Color content = theme::color(0.28f, 0.58f, 0.98f, 0.56f);
    core::Color label = theme::color(0.92f, 0.95f, 1.0f, 0.88f);
    float stroke = 1.0f;
    float labelFontSize = 12.0f;
    float labelLineHeight = 16.0f;
    float labelX = 8.0f;
    float labelY = 6.0f;
    float labelHeight = 18.0f;
};

inline void layoutDebugOverlay(core::dsl::Ui& ui,
                               const std::string& id,
                               float width,
                               float height,
                               float inset,
                               const std::string& label = "",
                               const LayoutDebugStyle& style = LayoutDebugStyle{}) {
    const float safeWidth = std::max(0.0f, width);
    const float safeHeight = std::max(0.0f, height);
    const float safeInset = std::min(std::max(0.0f, inset), std::min(safeWidth, safeHeight) * 0.5f);
    const float contentWidth = std::max(0.0f, safeWidth - safeInset * 2.0f);
    const float contentHeight = std::max(0.0f, safeHeight - safeInset * 2.0f);
    const float stroke = std::max(1.0f, style.stroke);

    ui.stack(id)
        .size(safeWidth, safeHeight)
        .content([&] {
            ui.rect(id + ".frame.top").size(safeWidth, stroke).color(style.frame).build();
            ui.rect(id + ".frame.left").size(stroke, safeHeight).color(style.frame).build();
            ui.rect(id + ".frame.right").position(std::max(0.0f, safeWidth - stroke), 0.0f).size(stroke, safeHeight).color(style.frame).build();
            ui.rect(id + ".frame.bottom").position(0.0f, std::max(0.0f, safeHeight - stroke)).size(safeWidth, stroke).color(style.frame).build();

            if (safeInset > 0.0f) {
                ui.rect(id + ".padding.top").size(safeWidth, safeInset).color(theme::withOpacity(style.padding, 0.07f)).build();
                ui.rect(id + ".padding.left").size(safeInset, safeHeight).color(theme::withOpacity(style.padding, 0.07f)).build();
                ui.rect(id + ".padding.right").position(safeWidth - safeInset, 0.0f).size(safeInset, safeHeight).color(theme::withOpacity(style.padding, 0.07f)).build();
                ui.rect(id + ".padding.bottom").position(0.0f, safeHeight - safeInset).size(safeWidth, safeInset).color(theme::withOpacity(style.padding, 0.07f)).build();

                ui.rect(id + ".content.top").position(safeInset, safeInset).size(contentWidth, stroke).color(style.content).build();
                ui.rect(id + ".content.left").position(safeInset, safeInset).size(stroke, contentHeight).color(style.content).build();
                ui.rect(id + ".content.right").position(safeInset + contentWidth - stroke, safeInset).size(stroke, contentHeight).color(style.content).build();
                ui.rect(id + ".content.bottom").position(safeInset, safeInset + contentHeight - stroke).size(contentWidth, stroke).color(style.content).build();
            }

            if (!label.empty()) {
                ui.text(id + ".label")
                    .position(style.labelX, style.labelY)
                    .size(std::max(0.0f, safeWidth - style.labelX * 2.0f), style.labelHeight)
                    .text(label)
                    .fontSize(style.labelFontSize)
                    .lineHeight(style.labelLineHeight)
                    .color(style.label)
                    .build();
            }
        })
        .build();
}

} // namespace components
