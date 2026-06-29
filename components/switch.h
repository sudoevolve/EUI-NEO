#pragma once

#include "components/theme.h"
#include "core/dsl.h"
#include "core/render/text.h"
#include "eui/signal.h"

#include <algorithm>
#include <functional>
#include <string>
#include <utility>

namespace components {

struct SwitchStyle {
    SwitchStyle() : SwitchStyle(theme::dark()) {}

    explicit SwitchStyle(const theme::ThemeColorTokens& tokens) {
        off = core::mixColor(tokens.surfaceHover, tokens.surfaceActive, 0.30f);
        on = tokens.primary;
        knob = core::mixColor(tokens.surface, theme::color(1.0f, 1.0f, 1.0f, 1.0f), 0.75f);
        text = tokens.text;
        rowHover = theme::withAlpha(tokens.text, tokens.dark ? 0.06f : 0.05f);
        rowPressed = theme::withAlpha(tokens.text, tokens.dark ? 0.10f : 0.08f);
    }

    core::Color off;
    core::Color on;
    core::Color knob;
    core::Color text;
    core::Color rowHover;
    core::Color rowPressed;
};

class SwitchBuilder {
public:
    SwitchBuilder(core::dsl::Ui& ui, std::string id)
        : ui_(ui), id_(std::move(id)) {}

    SwitchBuilder& size(float width, float height) { width_ = width; height_ = height; return *this; }
    SwitchBuilder& checked(bool value) { checked_ = value; return *this; }
    SwitchBuilder& bind(eui::Signal<bool>& signal) {
        checked(signal.get());
        onChange([&signal](bool value) { signal.set(value); });
        return *this;
    }
    SwitchBuilder& text(std::string value) { label_ = std::move(value); return *this; }
    SwitchBuilder& fontSize(float value) { fontSize_ = std::max(1.0f, value); return *this; }
    SwitchBuilder& trackSize(float width, float height) {
        trackWidth_ = std::max(20.0f, width);
        trackHeight_ = std::max(12.0f, height);
        return *this;
    }
    SwitchBuilder& style(const SwitchStyle& value) { style_ = value; return *this; }
    SwitchBuilder& theme(const theme::ThemeColorTokens& tokens) { style_ = SwitchStyle(tokens); return *this; }
    SwitchBuilder& transition(const core::Transition& value) { transition_ = value; return *this; }
    SwitchBuilder& transition(float duration, core::Ease ease = core::Ease::OutCubic) {
        transition_ = core::Transition::make(duration, ease);
        return *this;
    }
    SwitchBuilder& onChange(std::function<void(bool)> callback) { onChange_ = std::move(callback); return *this; }

    void build() {
        const float trackY = (height_ - trackHeight_) * 0.5f;
        const float margin = std::max(2.0f, trackHeight_ * 0.125f);
        const float knobSize = std::max(0.0f, trackHeight_ - margin * 2.0f);
        const float knobTravel = std::max(0.0f, trackWidth_ - margin * 2.0f - knobSize);
        const float knobX = margin + (checked_ ? knobTravel : 0.0f);
        const float labelX = trackWidth_ + gap_;
        const float labelWidth = std::max(0.0f, width_ - labelX);
        const float labelLineHeight = fontSize_;
        const float labelY = std::max(0.0f, (height_ - labelLineHeight) * 0.5f);
        const float hitWidth = label_.empty()
            ? trackWidth_
            : std::min(width_, labelX + textWidth(label_, fontSize_));
        const bool nextChecked = !checked_;
        const std::function<void(bool)> onChange = onChange_;

        ui_.stack(id_)
            .size(width_, height_)
            .content([&] {
                ui_.rect(id_ + ".hit")
                    .size(hitWidth, height_)
                    .states(theme::color(0.0f, 0.0f, 0.0f, 0.0f), style_.rowHover, style_.rowPressed)
                    .radius(std::max(6.0f, height_ * 0.20f))
                    .transition(transition_)
                    .onClick([onChange, nextChecked] {
                        if (onChange) {
                            onChange(nextChecked);
                        }
                    })
                    .build();

                ui_.rect(id_ + ".track")
                    .y(trackY)
                    .size(trackWidth_, trackHeight_)
                    .color(checked_ ? style_.on : style_.off)
                    .radius(trackHeight_ * 0.5f)
                    .transition(transition_)
                    .animate(core::AnimProperty::Color)
                    .build();

                ui_.rect(id_ + ".knob")
                    .x(knobX)
                    .y(trackY + margin)
                    .size(knobSize, knobSize)
                    .color(style_.knob)
                    .radius(knobSize * 0.5f)
                    .transition(transition_)
                    .animate(core::AnimProperty::Frame | core::AnimProperty::Color)
                    .build();

                if (!label_.empty()) {
                    ui_.text(id_ + ".label")
                        .x(labelX)
                        .y(labelY)
                        .size(labelWidth, labelLineHeight)
                        .text(label_)
                        .fontSize(fontSize_)
                        .lineHeight(labelLineHeight)
                        .color(style_.text)
                        .verticalAlign(core::VerticalAlign::Top)
                        .build();
                }
            })
            .build();
    }

private:
    static float textWidth(const std::string& value, float fontSize) {
        return core::TextPrimitive::measureTextWidth(value, {}, fontSize, 400);
    }

    core::dsl::Ui& ui_;
    std::string id_;
    SwitchStyle style_;
    core::Transition transition_ = core::Transition::make(0.18f, core::Ease::OutCubic);
    std::function<void(bool)> onChange_;
    std::string label_;
    bool checked_ = false;
    float width_ = 180.0f;
    float height_ = 30.0f;
    float trackWidth_ = 46.0f;
    float trackHeight_ = 24.0f;
    float gap_ = 10.0f;
    float fontSize_ = 18.0f;
};

inline SwitchBuilder toggleSwitch(core::dsl::Ui& ui, const std::string& id) {
    return SwitchBuilder(ui, id);
}

} // namespace components
