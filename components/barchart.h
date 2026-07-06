#pragma once

#include "components/theme.h"
#include "components/tooltip.h"
#include "core/dsl.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace components {

struct BarChartStyle {
    BarChartStyle() : BarChartStyle(theme::dark()) {}

    explicit BarChartStyle(const theme::ThemeColorTokens& tokens) {
        background = tokens.surface;
        title = tokens.text;
        label = theme::withOpacity(tokens.text, 0.56f);
        grid = theme::withOpacity(tokens.border, tokens.dark ? 0.38f : 0.36f);
        tooltipBackground = tokens.dark
            ? core::mixColor(tokens.surface, theme::color(0.0f, 0.0f, 0.0f), 0.18f)
            : theme::color(1.0f, 1.0f, 1.0f, 0.96f);
        tooltipText = tokens.text;
        border = theme::withOpacity(tokens.border, 0.76f);
        shadow = theme::shadow(tokens, 18.0f, 4.0f, 0.20f, 0.10f);
        palette = {
            theme::color(0.22f, 0.50f, 0.88f),
            theme::color(0.20f, 0.76f, 0.58f),
            theme::color(0.98f, 0.62f, 0.15f),
            theme::color(0.86f, 0.28f, 0.44f)
        };
    }

    core::Color background;
    core::Color title;
    core::Color label;
    core::Color grid;
    core::Color tooltipBackground;
    core::Color tooltipText;
    core::Color border;
    core::Shadow shadow;
    std::vector<core::Color> palette;
    float radius = 18.0f;
};

class BarChartBuilder {
    struct TooltipItem {
        std::string sourceId;
        std::string text;
        float x = 0.0f;
        float y = 0.0f;
    };

public:
    BarChartBuilder(core::dsl::Ui& ui, std::string id)
        : ui_(ui), id_(std::move(id)) {}

    BarChartBuilder& size(float width, float height) { width_ = width; height_ = height; return *this; }
    BarChartBuilder& title(const std::string& value) { title_ = value; return *this; }
    BarChartBuilder& values(std::vector<float> value) { values_ = std::move(value); return *this; }
    BarChartBuilder& labels(std::vector<std::string> value) { labels_ = std::move(value); return *this; }
    BarChartBuilder& colors(std::vector<core::Color> value) { style_.palette = std::move(value); return *this; }
    BarChartBuilder& style(const BarChartStyle& value) { style_ = value; return *this; }
    BarChartBuilder& theme(const theme::ThemeColorTokens& tokens) { style_ = BarChartStyle(tokens); return *this; }
    BarChartBuilder& transition(const core::Transition& value) { transition_ = value; return *this; }

    void build() {
        if (values_.empty()) {
            values_ = {0.92f, 0.36f, 0.68f, 0.52f};
        }
        if (labels_.empty()) {
            labels_ = {"D1", "D2", "D3", "D4"};
        }

        const float titleX = 20.0f;
        const float plotX = 32.0f;
        const float plotY = 70.0f;
        const float plotWidth = std::max(1.0f, width_ - 64.0f);
        const float plotHeight = std::max(1.0f, height_ - 112.0f);
        const float bottomY = plotY + plotHeight;
        const int count = static_cast<int>(values_.size());
        const float slotWidth = plotWidth / static_cast<float>(std::max(1, count));
        const float barWidth = std::min(32.0f, std::max(18.0f, slotWidth * 0.54f));

        ui_.stack(id_)
            .size(width_, height_)
            .content([&] {
                ui_.rect(id_ + ".bg")
                    .size(width_, height_)
                    .color(style_.background)
                    .radius(style_.radius)
                    .border(1.0f, style_.border)
                    .shadow(style_.shadow)
                    .build();

                ui_.text(id_ + ".title")
                    .x(titleX)
                    .y(18.0f)
                    .size(std::max(0.0f, width_ - titleX * 2.0f), 28.0f)
                    .text(title_)
                    .fontSize(22.0f)
                    .lineHeight(26.0f)
                    .color(style_.title)
                    .build();

                for (int line = 0; line < 4; ++line) {
                    const float y = plotY + static_cast<float>(line) * plotHeight / 3.0f;
                    ui_.rect(id_ + ".grid." + std::to_string(line))
                        .x(plotX)
                        .y(y)
                        .size(plotWidth, 1.0f)
                        .color(style_.grid)
                        .build();
                }

                std::vector<TooltipItem> tooltips;
                tooltips.reserve(values_.size());
                for (int index = 0; index < count; ++index) {
                    const float value = std::clamp(values_[index], 0.0f, 1.0f);
                    const float barHeight = std::max(8.0f, value * plotHeight);
                    const float x = plotX + static_cast<float>(index) * slotWidth + (slotWidth - barWidth) * 0.5f;
                    const float y = bottomY - barHeight;
                    const core::Color color = style_.palette.empty()
                        ? theme::color(0.22f, 0.50f, 0.88f)
                        : style_.palette[index % static_cast<int>(style_.palette.size())];
                    const std::string barId = id_ + ".bar." + std::to_string(index);

                    ui_.rect(barId)
                        .x(x)
                        .y(y)
                        .size(barWidth, barHeight)
                        .states(color,
                                core::mixColor(color, theme::color(1.0f, 1.0f, 1.0f), 0.18f),
                                core::mixColor(color, theme::color(0.0f, 0.0f, 0.0f), 0.12f))
                        .radius(std::min(10.0f, barWidth * 0.34f))
                        .instantStates()
                        .transition(transition_)
                        .animate(core::AnimProperty::Frame | core::AnimProperty::Color)
                        .onClick([] {})
                        .build();

                    tooltips.push_back({barId, dataLabel(index) + "  " + percent(value), x + barWidth * 0.5f, y});

                    ui_.text(id_ + ".label." + std::to_string(index))
                        .x(x - 8.0f)
                        .y(height_ - 34.0f)
                        .size(barWidth + 16.0f, 22.0f)
                        .text(index < static_cast<int>(labels_.size()) ? labels_[index] : "")
                        .fontSize(14.0f)
                        .lineHeight(18.0f)
                        .color(style_.label)
                        .horizontalAlign(core::HorizontalAlign::Center)
                        .build();
                }

                for (const TooltipItem& item : tooltips) {
                    components::tooltip(ui_, item.sourceId + ".tooltip")
                        .source(item.sourceId)
                        .value(item.text)
                        .anchor(item.x, item.y)
                        .bounds(width_, height_)
                        .style(tooltipStyle())
                        .build();
                }
            })
            .build();
    }

private:
    std::string dataLabel(int index) const {
        if (index >= 0 && index < static_cast<int>(labels_.size())) {
            return labels_[index];
        }
        return "D" + std::to_string(index + 1);
    }

    static std::string percent(float value) {
        return std::to_string(static_cast<int>(std::clamp(value, 0.0f, 1.0f) * 100.0f + 0.5f)) + "%";
    }

    TooltipStyle tooltipStyle() const {
        TooltipStyle result;
        result.background = style_.tooltipBackground;
        result.text = style_.tooltipText;
        result.border = style_.border;
        return result;
    }

    core::dsl::Ui& ui_;
    std::string id_;
    std::string title_ = "BarChart";
    std::vector<float> values_;
    std::vector<std::string> labels_;
    BarChartStyle style_;
    core::Transition transition_ = core::Transition::make(0.16f, core::Ease::OutCubic);
    float width_ = 206.0f;
    float height_ = 236.0f;
};

inline BarChartBuilder barChart(core::dsl::Ui& ui, const std::string& id) {
    return BarChartBuilder(ui, id);
}

} // namespace components
