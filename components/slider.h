#pragma once

#include "components/theme.h"
#include "core/dsl.h"
#include "eui/signal.h"

#include <algorithm>
#include <functional>
#include <string>
#include <utility>

namespace components {

struct SliderStyle {
    SliderStyle() : SliderStyle(theme::dark()) {}

    explicit SliderStyle(const theme::ThemeColorTokens& tokens) {
        track = core::mixColor(tokens.surfaceHover, tokens.surfaceActive, tokens.dark ? 0.24f : 0.18f);
        fill = tokens.primary;
        knob = tokens.text;
    }

    core::Color track;
    core::Color fill;
    core::Color knob;
};

class SliderBuilder {
public:
    SliderBuilder(core::dsl::Ui& ui, std::string id)
        : ui_(ui), id_(std::move(id)) {}

    SliderBuilder& size(float width, float height) { width_ = width; height_ = height; return *this; }
    SliderBuilder& value(float value) { value_ = std::clamp(value, 0.0f, 1.0f); return *this; }
    SliderBuilder& bind(eui::Signal<float>& signal) {
        value(signal.get());
        onChange([&signal](float value) { signal.set(value); });
        return *this;
    }
    SliderBuilder& style(const SliderStyle& value) { style_ = value; return *this; }
    SliderBuilder& theme(const theme::ThemeColorTokens& tokens) { style_ = SliderStyle(tokens); return *this; }
    SliderBuilder& transition(const core::Transition& value) { transition_ = value; return *this; }
    SliderBuilder& transition(float duration, core::Ease ease = core::Ease::OutCubic) {
        transition_ = core::Transition::make(duration, ease);
        return *this;
    }
    SliderBuilder& onChange(std::function<void(float)> callback) { onChange_ = std::move(callback); return *this; }

    void build() {
        const float trackHeight = std::max(3.0f, height_ * 0.18f);
        const float trackY = (height_ - trackHeight) * 0.5f;
        const float knobSize = std::max(14.0f, height_ * 0.72f);
        const std::function<void(float)> onChange = onChange_;

        ui_.stack(id_)
            .size(width_, height_)
            .sliderState(id_, value_, width_, knobSize, onChange)
            .content([&] {
                ui_.rect(id_ + ".track")
                    .y(trackY)
                    .size(width_, trackHeight)
                    .color(style_.track)
                    .radius(trackHeight * 0.5f)
                    .build();

                ui_.rect(id_ + ".fill")
                    .y(trackY)
                    .size(width_ * value_, trackHeight)
                    .color(style_.fill)
                    .radius(trackHeight * 0.5f)
                    .transition(transition_)
                    .animate(core::AnimProperty::Color)
                    .sliderFillFrom(id_)
                    .build();

                ui_.rect(id_ + ".knob")
                    .y((height_ - knobSize) * 0.5f)
                    .size(knobSize, knobSize)
                    .color(style_.knob)
                    .radius(knobSize * 0.5f)
                    .shadow(12.0f, 0.0f, 4.0f, theme::withAlpha(style_.fill, 0.20f))
                    .transition(transition_)
                    .animate(core::AnimProperty::Color | core::AnimProperty::Shadow)
                    .sliderKnobFrom(id_)
                    .build();

                ui_.rect(id_ + ".hit")
                    .size(width_, height_)
                    .states(theme::color(0.0f, 0.0f, 0.0f, 0.0f),
                            theme::color(0.0f, 0.0f, 0.0f, 0.0f),
                            theme::color(0.0f, 0.0f, 0.0f, 0.0f))
                    .zIndex(10)
                    .interactive()
                    .sliderInputFrom(id_)
                    .build();
            })
            .build();
    }

private:
    core::dsl::Ui& ui_;
    std::string id_;
    SliderStyle style_;
    core::Transition transition_ = core::Transition::make(0.16f, core::Ease::OutCubic);
    std::function<void(float)> onChange_;
    float width_ = 300.0f;
    float height_ = 28.0f;
    float value_ = 0.0f;
};

inline SliderBuilder slider(core::dsl::Ui& ui, const std::string& id) {
    return SliderBuilder(ui, id);
}

} // namespace components
