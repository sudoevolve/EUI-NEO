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

struct RadioStyle {
    RadioStyle() : RadioStyle(theme::dark()) {}

    explicit RadioStyle(const theme::ThemeColorTokens& tokens) {
        outer = tokens.surface;
        outerHover = tokens.surfaceHover;
        selected = tokens.primary;
        border = tokens.border;
        text = tokens.text;
        rowHover = theme::withAlpha(tokens.text, tokens.dark ? 0.06f : 0.05f);
        rowPressed = theme::withAlpha(tokens.text, tokens.dark ? 0.10f : 0.08f);
    }

    core::Color outer;
    core::Color outerHover;
    core::Color selected;
    core::Color border;
    core::Color text;
    core::Color rowHover;
    core::Color rowPressed;
};

class RadioBuilder {
public:
    RadioBuilder(core::dsl::Ui& ui, std::string id)
        : ui_(ui), id_(std::move(id)) {}

    RadioBuilder& size(float width, float height) { width_ = width; height_ = height; return *this; }
    RadioBuilder& selected(bool value) { selected_ = value; return *this; }
    RadioBuilder& bind(eui::Signal<bool>& signal) {
        selected(signal.get());
        onChange([&signal](bool value) { signal.set(value); });
        return *this;
    }
    RadioBuilder& text(std::string value) { text_ = std::move(value); return *this; }
    RadioBuilder& fontSize(float value) { fontSize_ = std::max(1.0f, value); return *this; }
    RadioBuilder& dotSize(float value) { dotSize_ = std::max(10.0f, value); return *this; }
    RadioBuilder& style(const RadioStyle& value) { style_ = value; return *this; }
    RadioBuilder& theme(const theme::ThemeColorTokens& tokens) { style_ = RadioStyle(tokens); return *this; }
    RadioBuilder& transition(const core::Transition& value) { transition_ = value; return *this; }
    RadioBuilder& transition(float duration, core::Ease ease = core::Ease::OutCubic) {
        transition_ = core::Transition::make(duration, ease);
        return *this;
    }
    RadioBuilder& onChange(std::function<void(bool)> callback) { onChange_ = std::move(callback); return *this; }

    void build() {
        const float outer = std::min(dotSize_, height_);
        const float inner = outer * 0.48f;
        const float visibleInner = selected_ ? inner : 0.0f;
        const float outerY = (height_ - outer) * 0.5f;
        const float innerOffset = (outer - visibleInner) * 0.5f;
        const float labelX = outer + gap_;
        const float labelWidth = std::max(0.0f, width_ - labelX);
        const float labelLineHeight = fontSize_;
        const float labelY = std::max(0.0f, (height_ - labelLineHeight) * 0.5f);
        const float hitWidth = text_.empty()
            ? outer
            : std::min(width_, labelX + textWidth(text_, fontSize_));
        core::Transition dotTransition = transition_;
        dotTransition.durationSeconds = selected_ ? 0.16f : 0.10f;
        dotTransition.ease = core::Ease::OutCubic;
        const std::function<void(bool)> onChange = onChange_;

        ui_.stack(id_)
            .size(width_, height_)
            .content([&] {
                ui_.rect(id_ + ".hit")
                    .size(hitWidth, height_)
                    .states(theme::color(0.0f, 0.0f, 0.0f, 0.0f), style_.rowHover, style_.rowPressed)
                    .radius(std::max(6.0f, height_ * 0.20f))
                    .transition(transition_)
                    .onClick([onChange] {
                        if (onChange) {
                            onChange(true);
                        }
                    })
                    .build();

                ui_.rect(id_ + ".outer")
                    .y(outerY)
                    .size(outer, outer)
                    .color(selected_ ? theme::withAlpha(style_.selected, 0.18f) : style_.outer)
                    .radius(outer * 0.5f)
                    .border(1.5f, selected_ ? style_.selected : style_.border)
                    .transition(transition_)
                    .animate(core::AnimProperty::Color | core::AnimProperty::Border)
                    .build();

                ui_.rect(id_ + ".inner")
                    .x(innerOffset)
                    .y(outerY + innerOffset)
                    .size(visibleInner, visibleInner)
                    .color(style_.selected)
                    .radius(visibleInner * 0.5f)
                    .opacity(selected_ ? 1.0f : 0.0f)
                    .transition(dotTransition)
                    .animate(core::AnimProperty::Frame | core::AnimProperty::Radius | core::AnimProperty::Opacity)
                    .build();

                if (!text_.empty()) {
                    ui_.text(id_ + ".label")
                        .x(labelX)
                        .y(labelY)
                        .size(labelWidth, labelLineHeight)
                        .text(text_)
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
    RadioStyle style_;
    core::Transition transition_ = core::Transition::make(0.16f, core::Ease::OutCubic);
    std::function<void(bool)> onChange_;
    std::string text_;
    bool selected_ = false;
    float width_ = 180.0f;
    float height_ = 28.0f;
    float dotSize_ = 22.0f;
    float gap_ = 10.0f;
    float fontSize_ = 18.0f;
};

inline RadioBuilder radio(core::dsl::Ui& ui, const std::string& id) {
    return RadioBuilder(ui, id);
}

} // namespace components
