#pragma once

#include "components/theme.h"
#include "core/dsl.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace components {

struct TooltipStyle {
    TooltipStyle() : TooltipStyle(theme::dark()) {}

    explicit TooltipStyle(const theme::ThemeColorTokens& tokens) {
        background = tokens.dark
            ? core::mixColor(tokens.surface, theme::color(0.0f, 0.0f, 0.0f), 0.18f)
            : theme::color(1.0f, 1.0f, 1.0f, 0.96f);
        text = tokens.text;
        border = theme::withOpacity(tokens.border, 0.76f);
        radius = tokens.metrics.radius.tooltip;
    }

    core::Color background;
    core::Color text;
    core::Color border;
    core::Shadow shadow = {true, {0.0f, 4.0f}, 12.0f, 0.0f, theme::color(0.0f, 0.0f, 0.0f, 0.16f)};
    float radius = 9.0f;
};

class TooltipBuilder {
public:
    TooltipBuilder(core::dsl::Ui& ui, std::string id)
        : ui_(ui), id_(std::move(id)) {}

    TooltipBuilder& source(const std::string& value) { sourceId_ = value; return *this; }
    TooltipBuilder& value(const std::string& value) { text_ = value; return *this; }
    TooltipBuilder& anchor(float x, float y) { anchorX_ = x; anchorY_ = y; return *this; }
    TooltipBuilder& bounds(float width, float height) { boundsWidth_ = width; boundsHeight_ = height; return *this; }
    TooltipBuilder& style(const TooltipStyle& value) { style_ = value; return *this; }
    TooltipBuilder& theme(const theme::ThemeColorTokens& tokens) {
        style_ = TooltipStyle(tokens);
        metrics_ = tokens.metrics;
        return *this;
    }
    TooltipBuilder& zIndex(int value) { zIndex_ = value; return *this; }

    void build() {
        const float tooltipWidth = std::min(maxWidth_, std::max(minWidth_, boundsWidth_ - metrics_.control.control));
        const float stackHeight = panelHeight_ + pointerHeight_;
        const float leftLimit = margin_;
        const float rightLimit = std::max(margin_, boundsWidth_ - tooltipWidth - margin_);
        const float tooltipX = std::clamp(anchorX_ - tooltipWidth * 0.5f, leftLimit, rightLimit);
        const bool belowAnchor = anchorY_ - stackHeight - gap_ < topLimit_;
        const float wantedY = belowAnchor ? anchorY_ + gap_ : anchorY_ - stackHeight - gap_;
        const float bottomLimit = std::max(topLimit_, boundsHeight_ - stackHeight - bottomMargin_);
        const float tooltipY = std::clamp(wantedY, topLimit_, bottomLimit);
        const float panelY = belowAnchor ? pointerHeight_ : 0.0f;
        const float pointerY = belowAnchor ? 0.0f : panelHeight_;
        const float pointerX = std::clamp(
            anchorX_ - tooltipX,
            pointerHalfWidth_ + metrics_.typography.lineGapRelaxed,
            tooltipWidth - pointerHalfWidth_ - metrics_.typography.lineGapRelaxed);

        ui_.stack(id_)
            .x(tooltipX)
            .y(tooltipY)
            .size(tooltipWidth, stackHeight)
            .zIndex(zIndex_)
            .hoverOpacityFrom(sourceId_)
            .content([&] {
                const std::vector<core::Vec2> pointerPoints = belowAnchor
                    ? std::vector<core::Vec2>{
                        {pointerX, 0.0f},
                        {pointerX + pointerHalfWidth_, pointerHeight_},
                        {pointerX - pointerHalfWidth_, pointerHeight_}
                    }
                    : std::vector<core::Vec2>{
                        {pointerX - pointerHalfWidth_, 0.0f},
                        {pointerX + pointerHalfWidth_, 0.0f},
                        {pointerX, pointerHeight_}
                    };

                ui_.polygon(id_ + ".pointer")
                    .y(pointerY)
                    .size(tooltipWidth, pointerHeight_)
                    .points(pointerPoints)
                    .color(style_.background)
                    .radius(pointerRadius_)
                    .build();

                ui_.rect(id_ + ".bg")
                    .y(panelY)
                    .size(tooltipWidth, panelHeight_)
                    .color(style_.background)
                    .radius(style_.radius)
                    .border(metrics_.spacing.hairline, style_.border)
                    .shadow(style_.shadow)
                    .build();

                ui_.text(id_ + ".text")
                    .x(textPaddingX_)
                    .y(panelY)
                    .size(std::max(0.0f, tooltipWidth - textPaddingX_ * 2.0f), panelHeight_)
                    .text(text_)
                    .fontSize(metrics_.typography.hint)
                    .lineHeight(metrics_.typography.hint + metrics_.typography.lineGapTight)
                    .color(style_.text)
                    .horizontalAlign(core::HorizontalAlign::Center)
                    .verticalAlign(core::VerticalAlign::Center)
                    .build();
            })
            .build();
    }

private:
    core::dsl::Ui& ui_;
    std::string id_;
    std::string sourceId_;
    std::string text_;
    TooltipStyle style_;
    theme::ThemeMetricTokens metrics_;
    float anchorX_ = 0.0f;
    float anchorY_ = 0.0f;
    float boundsWidth_ = 206.0f;
    float boundsHeight_ = 236.0f;
    float minWidth_ = 86.0f;
    float maxWidth_ = 112.0f;
    float panelHeight_ = 32.0f;
    float pointerHeight_ = 8.0f;
    float pointerHalfWidth_ = 7.0f;
    float pointerRadius_ = 2.8f;
    float textPaddingX_ = 10.0f;
    float margin_ = 12.0f;
    float bottomMargin_ = 14.0f;
    float topLimit_ = 44.0f;
    float gap_ = 0.0f;
    int zIndex_ = 10;
};

inline TooltipBuilder tooltip(core::dsl::Ui& ui, const std::string& id) {
    return TooltipBuilder(ui, id);
}

} // namespace components
