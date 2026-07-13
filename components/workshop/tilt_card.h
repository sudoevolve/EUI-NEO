#pragma once

#include "components/mousearea.h"
#include "components/theme.h"
#include "core/dsl.h"

#include <algorithm>
#include <string>
#include <utility>

namespace components::workshop {

struct TiltCardStyle {
    core::Color surface{0.12f, 0.13f, 0.16f, 1.0f};
    core::Color surfaceTop{0.22f, 0.24f, 0.30f, 1.0f};
    core::Color accent{1.0f, 0.36f, 0.58f, 1.0f};
    core::Color text{0.96f, 0.97f, 1.0f, 1.0f};
    core::Color muted{0.66f, 0.70f, 0.78f, 1.0f};
    core::Color border{1.0f, 1.0f, 1.0f, 0.15f};
    core::Color shadow{0.0f, 0.0f, 0.0f, 0.28f};
    float radius = 22.0f;
};

inline TiltCardStyle tiltCardStyle(const theme::ThemeColorTokens& tokens) {
    TiltCardStyle style;
    style.radius = tokens.metrics.radius.feature;
    if (tokens.dark) {
        style.surface = core::mixColor(tokens.surface, {0.0f, 0.0f, 0.0f, 1.0f}, 0.10f);
        style.surfaceTop = core::mixColor(tokens.surfaceHover, {1.0f, 1.0f, 1.0f, 1.0f}, 0.04f);
        style.text = theme::withAlpha(tokens.text, 0.96f);
        style.muted = theme::withAlpha(tokens.text, 0.68f);
        style.border = theme::withAlpha(tokens.border, 0.72f);
        style.shadow = {0.0f, 0.0f, 0.0f, 0.34f};
    } else {
        style.surface = core::mixColor(tokens.surface, tokens.surfaceHover, 0.12f);
        style.surfaceTop = core::mixColor(tokens.surfaceHover, {1.0f, 1.0f, 1.0f, 1.0f}, 0.35f);
        style.text = theme::withAlpha(tokens.text, 0.94f);
        style.muted = theme::withAlpha(tokens.text, 0.62f);
        style.border = theme::withAlpha(tokens.border, 0.66f);
        style.shadow = {0.28f, 0.32f, 0.40f, 0.20f};
    }
    return style;
}

class TiltCardBuilder {
public:
    TiltCardBuilder(core::dsl::Ui& ui, std::string id)
        : ui_(ui), id_(std::move(id)) {}

    TiltCardBuilder& size(float width, float height) {
        width_ = width;
        height_ = height;
        return *this;
    }

    TiltCardBuilder& title(std::string value) {
        title_ = std::move(value);
        return *this;
    }

    TiltCardBuilder& subtitle(std::string value) {
        subtitle_ = std::move(value);
        return *this;
    }

    TiltCardBuilder& style(const TiltCardStyle& value) {
        style_ = value;
        return *this;
    }

    TiltCardBuilder& theme(const theme::ThemeColorTokens& tokens) {
        style_ = tiltCardStyle(tokens);
        metrics_ = tokens.metrics;
        return *this;
    }

    TiltCardBuilder& maxTilt(float radians) {
        maxTilt_ = std::clamp(radians, 0.0f, 0.80f);
        return *this;
    }

    TiltCardBuilder& transition(const core::Transition& value) {
        transition_ = value;
        return *this;
    }

    TiltCardBuilder& transition(float duration, core::Ease ease = core::Ease::OutCubic) {
        transition_ = core::Transition::make(duration, ease);
        return *this;
    }

    void build() {
        const float w = std::max(1.0f, width_);
        const float h = std::max(1.0f, height_);
        const float radius = std::min(style_.radius, std::min(w, h) * 0.33f);
        const core::Transition noTransformTransition = core::Transition::none();

        ui_.stack(id_)
            .size(w, h)
            .content([&] {
                ui_.rect(id_ + ".shadow")
                    .x(10.0f)
                    .y(18.0f)
                    .size(std::max(1.0f, w - 20.0f), std::max(1.0f, h - 16.0f))
                    .color({0.0f, 0.0f, 0.0f, 0.0f})
                    .radius(radius)
                    .shadow(16.0f, 0.0f, 10.0f, style_.shadow)
                    .disabled(true)
                    .build();

                ui_.stack(id_ + ".card")
                    .size(w, h)
                    .perspective(640.0f)
                    .transformOrigin(0.5f, 0.5f)
                    .runtimePointerTiltFrom(id_ + ".hit", maxTilt_, 1.012f)
                    .transition(noTransformTransition)
                    .content([&] {
                        ui_.rect(id_ + ".surface")
                            .size(w, h)
                            .gradient(style_.surfaceTop, style_.surface, core::GradientDirection::Vertical)
                            .radius(radius)
                            .border(1.0f, style_.border)
                            .disabled(true)
                            .transition(transition_)
                            .build();

                        ui_.text(id_ + ".title")
                            .x(metrics_.control.indicator)
                            .y(metrics_.spacing.panel)
                            .size(std::max(1.0f, w - metrics_.control.large), metrics_.control.menuItem)
                            .text(title_)
                            .fontSize(metrics_.typography.heading)
                            .lineHeight(metrics_.typography.heading + metrics_.typography.lineGap)
                            .fontWeight(700)
                            .color(style_.text)
                            .build();

                        ui_.text(id_ + ".subtitle")
                            .x(metrics_.control.indicator)
                            .y(60.0f)
                            .size(std::max(1.0f, w - metrics_.control.large), metrics_.control.switchWidth)
                            .text(subtitle_)
                            .fontSize(metrics_.typography.label)
                            .lineHeight(metrics_.typography.label + metrics_.typography.lineGap)
                            .wrap(true)
                            .color(style_.muted)
                            .build();

                        ui_.rect(id_ + ".chip")
                            .x(metrics_.control.indicator)
                            .y(std::max(106.0f, h - 48.0f))
                            .size(116.0f, metrics_.control.compact)
                            .color(theme::withAlpha(style_.accent, 0.18f))
                            .radius(metrics_.radius.elevated)
                            .border(1.0f, theme::withAlpha(style_.accent, 0.30f))
                            .disabled(true)
                            .build();

                        ui_.text(id_ + ".chip.text")
                            .x(metrics_.control.indicator)
                            .y(std::max(106.0f, h - 48.0f))
                            .size(116.0f, metrics_.control.compact)
                            .text("pointer tilt")
                            .fontSize(metrics_.typography.caption)
                            .lineHeight(metrics_.typography.caption + metrics_.spacing.micro)
                            .fontWeight(600)
                            .color(theme::withAlpha(style_.text, 0.90f))
                            .horizontalAlign(core::HorizontalAlign::Center)
                            .verticalAlign(core::VerticalAlign::Center)
                            .build();
                    })
                    .build();

                components::mouseArea(ui_, id_ + ".hit")
                    .size(w, h)
                    .radius(radius)
                    .build();
            })
            .build();
    }

private:
    core::dsl::Ui& ui_;
    std::string id_;
    std::string title_ = "Tilt Card";
    std::string subtitle_ = "A workshop card that follows the pointer with perspective and soft light.";
    TiltCardStyle style_;
    theme::ThemeMetricTokens metrics_;
    core::Transition transition_ = core::Transition::make(0.18f, core::Ease::OutCubic);
    float width_ = 300.0f;
    float height_ = 176.0f;
    float maxTilt_ = 0.22f;
};

inline TiltCardBuilder tiltCard(core::dsl::Ui& ui, const std::string& id) {
    return TiltCardBuilder(ui, id);
}

} // namespace components::workshop
