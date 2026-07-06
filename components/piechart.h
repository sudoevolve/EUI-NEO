#pragma once

#include "components/theme.h"
#include "components/tooltip.h"
#include "core/dsl.h"

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace components {

struct PieChartStyle {
    PieChartStyle() : PieChartStyle(theme::dark()) {}

    explicit PieChartStyle(const theme::ThemeColorTokens& tokens) {
        background = tokens.surface;
        title = tokens.text;
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
    core::Color tooltipBackground;
    core::Color tooltipText;
    core::Color border;
    core::Shadow shadow;
    std::vector<core::Color> palette;
    float radius = 18.0f;
};

class PieChartBuilder {
    struct TooltipItem {
        std::string sourceId;
        std::string text;
        float x = 0.0f;
        float y = 0.0f;
    };

public:
    PieChartBuilder(core::dsl::Ui& ui, std::string id)
        : ui_(ui), id_(std::move(id)) {}

    PieChartBuilder& size(float width, float height) { width_ = width; height_ = height; return *this; }
    PieChartBuilder& title(const std::string& value) { title_ = value; return *this; }
    PieChartBuilder& values(std::vector<float> value) { values_ = std::move(value); return *this; }
    PieChartBuilder& labels(std::vector<std::string> value) { labels_ = std::move(value); return *this; }
    PieChartBuilder& colors(std::vector<core::Color> value) { style_.palette = std::move(value); return *this; }
    PieChartBuilder& style(const PieChartStyle& value) { style_ = value; return *this; }
    PieChartBuilder& theme(const theme::ThemeColorTokens& tokens) { style_ = PieChartStyle(tokens); return *this; }
    PieChartBuilder& transition(const core::Transition& value) { transition_ = value; return *this; }

    void build() {
        const float titleX = 20.0f;
        const float pieSize = std::max(96.0f, std::min(width_ - 48.0f, height_ - 82.0f));
        const float pieX = (width_ - pieSize) * 0.5f;
        const float pieY = 70.0f;
        if (values_.empty()) {
            values_ = {0.42f, 0.24f, 0.18f, 0.16f};
        }
        if (labels_.empty()) {
            labels_ = {"Blue", "Green", "Orange", "Pink"};
        }
        AnimState& anim = ui_.state<AnimState>(id_ + ".anim");
        syncAnimation(anim);
        const std::vector<float>& displayValues = anim.display;
        const float total = valueTotal(displayValues);
        float startAngle = -1.57079632679f;

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

                std::vector<TooltipItem> tooltips;
                tooltips.reserve(values_.size());
                for (int index = 0; index < static_cast<int>(displayValues.size()); ++index) {
                    const float amount = total > 0.0f ? std::max(0.0f, displayValues[index]) / total : 0.0f;
                    const float sweep = amount * 6.28318530718f;
                    const float endAngle = startAngle + sweep;
                    const core::Color color = sliceColor(index);
                    const std::string sliceId = id_ + ".slice." + std::to_string(index);

                    ui_.polygon(sliceId)
                        .x(pieX)
                        .y(pieY)
                        .size(pieSize, pieSize)
                        .points(slicePoints(pieSize, startAngle, endAngle))
                        .states(color,
                                core::mixColor(color, theme::color(1.0f, 1.0f, 1.0f), 0.18f),
                                core::mixColor(color, theme::color(0.0f, 0.0f, 0.0f), 0.12f))
                        .instantStates()
                        .transition(transition_)
                        .onClick([] {})
                        .build();

                    const core::Vec2 anchor = sliceAnchor(pieX, pieY, pieSize, (startAngle + endAngle) * 0.5f);
                    tooltips.push_back({sliceId, dataLabel(index) + "  " + percent(amount), anchor.x, anchor.y});
                    startAngle = endAngle;
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

                if (anim.animating) {
                    ui_.stack(id_ + ".animator")
                        .size(0.0f, 0.0f)
                        .onTimer(0.016f, [] {})
                        .build();
                }
            })
            .build();
    }

private:
    struct AnimState {
        std::vector<float> display;
        std::vector<float> target;
        bool animating = false;
    };

    static bool closeValues(const std::vector<float>& left, const std::vector<float>& right) {
        if (left.size() != right.size()) {
            return false;
        }
        for (std::size_t i = 0; i < left.size(); ++i) {
            if (std::fabs(left[i] - right[i]) > 0.001f) {
                return false;
            }
        }
        return true;
    }

    void syncAnimation(AnimState& anim) const {
        if (anim.display.size() != values_.size()) {
            anim.display = values_;
            anim.target = values_;
            anim.animating = false;
            return;
        }

        if (!closeValues(anim.target, values_)) {
            anim.target = values_;
            anim.animating = true;
        }

        if (!anim.animating) {
            return;
        }

        bool moving = false;
        for (std::size_t i = 0; i < anim.display.size(); ++i) {
            const float next = anim.display[i] + (anim.target[i] - anim.display[i]) * 0.20f;
            if (std::fabs(next - anim.target[i]) > 0.002f) {
                anim.display[i] = next;
                moving = true;
            } else {
                anim.display[i] = anim.target[i];
            }
        }
        anim.animating = moving;
    }

    static float valueTotal(const std::vector<float>& values) {
        float total = 0.0f;
        for (float value : values) {
            total += std::max(0.0f, value);
        }
        return total;
    }

    core::Color sliceColor(int index) const {
        if (style_.palette.empty()) {
            return theme::color(0.22f, 0.50f, 0.88f);
        }
        return style_.palette[index % static_cast<int>(style_.palette.size())];
    }

    static std::vector<core::Vec2> slicePoints(float size, float startAngle, float endAngle) {
        const float radius = size * 0.5f;
        const core::Vec2 center{radius, radius};
        const float sweep = std::max(0.0f, endAngle - startAngle);
        const int steps = std::max(2, static_cast<int>(std::ceil(sweep / 6.28318530718f * 56.0f)));
        std::vector<core::Vec2> points;
        points.reserve(static_cast<std::size_t>(steps) + 2u);
        points.push_back(center);
        for (int step = 0; step <= steps; ++step) {
            const float t = static_cast<float>(step) / static_cast<float>(steps);
            const float angle = startAngle + sweep * t;
            points.push_back({
                center.x + std::cos(angle) * radius,
                center.y + std::sin(angle) * radius
            });
        }
        return points;
    }

    static core::Vec2 sliceAnchor(float pieX, float pieY, float pieSize, float angle) {
        const float radius = pieSize * 0.33f;
        return {
            pieX + pieSize * 0.5f + std::cos(angle) * radius,
            pieY + pieSize * 0.5f + std::sin(angle) * radius
        };
    }

    std::string dataLabel(int index) const {
        if (index >= 0 && index < static_cast<int>(labels_.size())) {
            return labels_[index];
        }
        return "S" + std::to_string(index + 1);
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
    std::string title_ = "PieChart";
    std::vector<float> values_;
    std::vector<std::string> labels_;
    PieChartStyle style_;
    core::Transition transition_ = core::Transition::make(0.16f, core::Ease::OutCubic);
    float width_ = 206.0f;
    float height_ = 236.0f;
};

inline PieChartBuilder pieChart(core::dsl::Ui& ui, const std::string& id) {
    return PieChartBuilder(ui, id);
}

} // namespace components
