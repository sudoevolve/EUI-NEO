#pragma once
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

struct TabsStyle {
    TabsStyle() : TabsStyle(theme::dark()) {}

    explicit TabsStyle(const theme::ThemeColorTokens& tokens) {
        text = theme::withOpacity(tokens.text, 0.66f);
        hover = tokens.surfaceHover;
        selectedText = tokens.primary;
        indicator = tokens.primary;
        border = theme::withOpacity(tokens.border, 0.70f);
    }

    core::Color text;
    core::Color hover;
    core::Color selectedText;
    core::Color indicator;
    core::Color border;
};

class TabsBuilder {
public:
    TabsBuilder(core::dsl::Ui& ui, std::string id)
        : ui_(ui), id_(std::move(id)) {}

    TabsBuilder& size(float width, float height) { width_ = width; height_ = height; return *this; }
    TabsBuilder& items(std::vector<std::string> value) { items_ = std::move(value); return *this; }
    TabsBuilder& selected(int value) { selected_ = value; return *this; }
    TabsBuilder& bind(eui::Signal<int>& signal) {
        selected(signal.get());
        onChange([&signal](int value) { signal.set(value); });
        return *this;
    }
    TabsBuilder& fontSize(float value) { fontSize_ = std::max(1.0f, value); return *this; }
    TabsBuilder& style(const TabsStyle& value) { style_ = value; return *this; }
    TabsBuilder& theme(const theme::ThemeColorTokens& tokens) { style_ = TabsStyle(tokens); return *this; }
    TabsBuilder& transition(const core::Transition& value) { transition_ = value; return *this; }
    TabsBuilder& transition(float duration, core::Ease ease = core::Ease::OutCubic) {
        transition_ = core::Transition::make(duration, ease);
        return *this;
    }
    TabsBuilder& onChange(std::function<void(int)> callback) { onChange_ = std::move(callback); return *this; }

    void build() {
        const int count = static_cast<int>(items_.size());
        const int selected = count > 0 ? std::clamp(selected_, 0, count - 1) : 0;
        const float tabWidth = count > 0 ? width_ / static_cast<float>(count) : width_;
        const float labelLineHeight = fontSize_;
        const float labelY = std::max(0.0f, (height_ - labelLineHeight) * 0.5f) - 2.0f;
        const std::function<void(int)> onChange = onChange_;

        ui_.stack(id_)
            .size(width_, height_)
            .content([&] {
                ui_.rect(id_ + ".line")
                    .y(std::max(0.0f, height_ - 1.0f))
                    .size(width_, 1.0f)
                    .color(style_.border)
                    .build();

                for (int index = 0; index < count; ++index) {
                    const float x = static_cast<float>(index) * tabWidth;
                    const bool active = index == selected;
                    ui_.rect(id_ + ".hit." + std::to_string(index))
                        .x(x)
                        .size(tabWidth, height_)
                        .states(theme::color(0.0f, 0.0f, 0.0f, 0.0f),
                                theme::color(0.0f, 0.0f, 0.0f, 0.0f),
                                theme::color(0.0f, 0.0f, 0.0f, 0.0f))
                        .radius(8.0f)
                        .onClick([onChange, index] {
                            if (onChange) {
                                onChange(index);
                            }
                        })
                        .build();

                    ui_.text(id_ + ".label." + std::to_string(index))
                        .x(x)
                        .y(labelY)
                        .size(tabWidth, labelLineHeight)
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

                if (count > 0) {
                    ui_.rect(id_ + ".indicator")
                        .x(tabWidth * static_cast<float>(selected) + 10.0f)
                        .y(std::max(0.0f, height_ - 3.0f))
                        .size(std::max(0.0f, tabWidth - 20.0f), 3.0f)
                        .color(style_.indicator)
                        .radius(1.5f)
                        .transition(transition_)
                        .animate(core::AnimProperty::Frame | core::AnimProperty::Color)
                        .build();
                }
            })
            .build();
    }

private:
    core::dsl::Ui& ui_;
    std::string id_;
    std::vector<std::string> items_;
    TabsStyle style_;
    core::Transition transition_ = core::Transition::make(0.16f, core::Ease::OutCubic);
    std::function<void(int)> onChange_;
    int selected_ = 0;
    float width_ = 360.0f;
    float height_ = 42.0f;
    float fontSize_ = 17.0f;
};

inline TabsBuilder tabs(core::dsl::Ui& ui, const std::string& id) {
    return TabsBuilder(ui, id);
}

} // namespace components
