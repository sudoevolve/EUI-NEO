#pragma once

#include "components/theme.h"
#include "core/dsl.h"
#include "eui/signal.h"

#include <algorithm>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace components {

struct SegmentedStyle {
    SegmentedStyle() : SegmentedStyle(theme::dark()) {}

    explicit SegmentedStyle(const theme::ThemeColorTokens& tokens) {
        background = tokens.surface;
        hover = tokens.surfaceHover;
        selected = tokens.primary;
        text = tokens.text;
        selectedText = tokens.dark ? theme::color(0.96f, 0.98f, 1.0f, 1.0f) : theme::color(1.0f, 1.0f, 1.0f, 1.0f);
        border = theme::withOpacity(tokens.border, 0.70f);
    }

    core::Color background;
    core::Color hover;
    core::Color selected;
    core::Color text;
    core::Color selectedText;
    core::Color border;
};

class SegmentedBuilder {
public:
    SegmentedBuilder(core::dsl::Ui& ui, std::string id)
        : ui_(ui), id_(std::move(id)) {}

    SegmentedBuilder& size(float width, float height) { width_ = width; height_ = height; return *this; }
    SegmentedBuilder& items(std::vector<std::string> value) { items_ = std::move(value); return *this; }
    SegmentedBuilder& selected(int value) { selected_ = value; return *this; }
    SegmentedBuilder& bind(eui::Signal<int>& signal) {
        selected(signal.get());
        onChange([&signal](int value) { signal.set(value); });
        return *this;
    }
    SegmentedBuilder& fontSize(float value) { fontSize_ = std::max(1.0f, value); return *this; }
    SegmentedBuilder& style(const SegmentedStyle& value) { style_ = value; return *this; }
    SegmentedBuilder& theme(const theme::ThemeColorTokens& tokens) { style_ = SegmentedStyle(tokens); return *this; }
    SegmentedBuilder& transition(const core::Transition& value) { transition_ = value; return *this; }
    SegmentedBuilder& transition(float duration, core::Ease ease = core::Ease::OutCubic) {
        transition_ = core::Transition::make(duration, ease);
        return *this;
    }
    SegmentedBuilder& onChange(std::function<void(int)> callback) { onChange_ = std::move(callback); return *this; }

    void build() {
        const int count = static_cast<int>(items_.size());
        const int selected = count > 0 ? std::clamp(selected_, 0, count - 1) : 0;
        const float segmentWidth = count > 0 ? width_ / static_cast<float>(count) : width_;
        const float innerInset = 3.0f;
        const float labelLineHeight = fontSize_;
        const float labelY = std::max(0.0f, (height_ - labelLineHeight) * 0.5f);
        const std::function<void(int)> onChange = onChange_;

        ui_.stack(id_)
            .size(width_, height_)
            .clip()
            .content([&] {
                ui_.rect(id_ + ".bg")
                    .size(width_, height_)
                    .color(style_.background)
                    .radius(radius_)
                    .border(1.0f, style_.border)
                    .build();

                if (count > 0) {
                    ui_.rect(id_ + ".indicator")
                        .x(segmentWidth * static_cast<float>(selected) + innerInset)
                        .y(innerInset)
                        .size(std::max(0.0f, segmentWidth - innerInset * 2.0f), std::max(0.0f, height_ - innerInset * 2.0f))
                        .color(style_.selected)
                        .radius(std::max(0.0f, radius_ - 2.0f))
                        .transition(transition_)
                        .animate(core::AnimProperty::Frame | core::AnimProperty::Color)
                        .build();
                }

                for (int index = 0; index < count; ++index) {
                    const float x = static_cast<float>(index) * segmentWidth;
                    const bool active = index == selected;
                    ui_.rect(id_ + ".hit." + std::to_string(index))
                        .x(x)
                        .size(segmentWidth, height_)
                        .states(theme::color(0.0f, 0.0f, 0.0f, 0.0f),
                                theme::color(0.0f, 0.0f, 0.0f, 0.0f),
                                theme::color(0.0f, 0.0f, 0.0f, 0.0f))
                        .radius(radius_)
                        .onClick([onChange, index] {
                            if (onChange) {
                                onChange(index);
                            }
                        })
                        .build();

                    ui_.text(id_ + ".label." + std::to_string(index))
                        .x(x)
                        .y(labelY)
                        .size(segmentWidth, labelLineHeight)
                        .text(items_[index])
                        .fontSize(fontSize_)
                        .lineHeight(labelLineHeight)
                        .color(active ? style_.selectedText : style_.text)
                        .horizontalAlign(core::HorizontalAlign::Center)
                        .verticalAlign(core::VerticalAlign::Top)
                        .transition(transition_)
                        .animate(core::AnimProperty::TextColor)
                        .build();
                }
            })
            .build();
    }

private:
    core::dsl::Ui& ui_;
    std::string id_;
    std::vector<std::string> items_;
    SegmentedStyle style_;
    core::Transition transition_ = core::Transition::make(0.18f, core::Ease::OutCubic);
    std::function<void(int)> onChange_;
    int selected_ = 0;
    float width_ = 300.0f;
    float height_ = 36.0f;
    float fontSize_ = 16.0f;
    float radius_ = 9.0f;
};

inline SegmentedBuilder segmented(core::dsl::Ui& ui, const std::string& id) {
    return SegmentedBuilder(ui, id);
}

} // namespace components
