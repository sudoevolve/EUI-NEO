#pragma once

#include "components/theme.h"
#include "core/dsl.h"

#include <algorithm>
#include <string>
#include <utility>

namespace components {

struct ProgressStyle {
    ProgressStyle() : ProgressStyle(theme::dark()) {}

    explicit ProgressStyle(const theme::ThemeColorTokens& tokens)
        : track(tokens.surfaceHover), fill(tokens.primary) {}

    core::Color track;
    core::Color fill;
};

class ProgressBuilder {
public:
    ProgressBuilder(core::dsl::Ui& ui, std::string id)
        : ui_(ui), id_(std::move(id)) {}

    ProgressBuilder& size(float width, float height) { width_ = width; height_ = height; return *this; }
    ProgressBuilder& value(float value) { value_ = std::clamp(value, 0.0f, 1.0f); return *this; }
    ProgressBuilder& style(const ProgressStyle& value) { style_ = value; return *this; }
    ProgressBuilder& theme(const theme::ThemeColorTokens& tokens) {
        style_ = ProgressStyle(tokens);
        metrics_ = tokens.metrics;
        return *this;
    }
    ProgressBuilder& transition(const core::Transition& value) { transition_ = value; return *this; }
    ProgressBuilder& transition(float duration, core::Ease ease = core::Ease::OutCubic) {
        transition_ = core::Transition::make(duration, ease);
        return *this;
    }

    void build() {
        const float height = height_ >= 0.0f ? height_ : metrics_.control.progress;
        ui_.stack(id_)
            .size(width_, height)
            .content([&] {
                ui_.rect(id_ + ".track")
                    .size(width_, height)
                    .color(style_.track)
                    .radius(height * 0.5f)
                    .build();

                ui_.rect(id_ + ".fill")
                    .size(width_ * value_, height)
                    .color(style_.fill)
                    .radius(height * 0.5f)
                    .transition(transition_)
                    .animate(core::AnimProperty::Frame | core::AnimProperty::Color)
                    .build();
            })
            .build();
    }

private:
    core::dsl::Ui& ui_;
    std::string id_;
    ProgressStyle style_;
    theme::ThemeMetricTokens metrics_;
    core::Transition transition_ = core::Transition::make(0.18f, core::Ease::OutCubic);
    float width_ = 300.0f;
    float height_ = -1.0f;
    float value_ = 0.0f;
};

inline ProgressBuilder progress(core::dsl::Ui& ui, const std::string& id) {
    return ProgressBuilder(ui, id);
}

} // namespace components
