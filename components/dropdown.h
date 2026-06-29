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

struct DropdownStyle {
    DropdownStyle() : DropdownStyle(theme::dark()) {}

    explicit DropdownStyle(const theme::ThemeColorTokens& tokens) {
        field = tokens.surface;
        fieldHover = tokens.surfaceHover;
        fieldPressed = tokens.surfaceActive;
        popup = tokens.dark
            ? core::mixColor(tokens.surface, theme::color(0.0f, 0.0f, 0.0f), 0.12f)
            : tokens.surface;
        optionHover = tokens.surfaceHover;
        optionPressed = tokens.surfaceActive;
        selected = theme::withAlpha(tokens.primary, tokens.dark ? 0.24f : 0.14f);
        text = tokens.text;
        mutedText = theme::withOpacity(tokens.text, 0.60f);
        accent = tokens.primary;
        border = theme::withOpacity(tokens.border, 0.78f);
        shadow = theme::popupShadow(tokens);
    }

    core::Color field;
    core::Color fieldHover;
    core::Color fieldPressed;
    core::Color popup;
    core::Color optionHover;
    core::Color optionPressed;
    core::Color selected;
    core::Color text;
    core::Color mutedText;
    core::Color accent;
    core::Color border;
    core::Shadow shadow;
    float radius = 12.0f;
};

class DropdownBuilder {
public:
    DropdownBuilder(core::dsl::Ui& ui, std::string id)
        : ui_(ui), id_(std::move(id)) {}

    DropdownBuilder& size(float width, float height) { width_ = width; height_ = height; return *this; }
    DropdownBuilder& items(std::vector<std::string> value) { items_ = std::move(value); return *this; }
    DropdownBuilder& selected(int value) { selected_ = value; return *this; }
    DropdownBuilder& bind(eui::Signal<int>& signal) {
        selected(signal.get());
        onChange([&signal](int value) { signal.set(value); });
        return *this;
    }
    DropdownBuilder& placeholder(const std::string& value) { placeholder_ = value; return *this; }
    DropdownBuilder& open(bool value = true) { open_ = value; return *this; }
    DropdownBuilder& bindOpen(eui::Signal<bool>& signal) {
        open(signal.get());
        onOpenChange([&signal](bool value) { signal.set(value); });
        return *this;
    }
    DropdownBuilder& itemHeight(float value) { itemHeight_ = std::max(24.0f, value); return *this; }
    DropdownBuilder& style(const DropdownStyle& value) { style_ = value; return *this; }
    DropdownBuilder& theme(const theme::ThemeColorTokens& tokens) { style_ = DropdownStyle(tokens); return *this; }
    DropdownBuilder& transition(const core::Transition& value) { transition_ = value; return *this; }
    DropdownBuilder& zIndex(int value) { zIndex_ = value; return *this; }
    DropdownBuilder& onChange(std::function<void(int)> callback) { onChange_ = std::move(callback); return *this; }
    DropdownBuilder& onOpenChange(std::function<void(bool)> callback) { onOpenChange_ = std::move(callback); return *this; }

    void build() {
        const int count = static_cast<int>(items_.size());
        const int selected = count > 0 ? std::clamp(selected_, 0, count - 1) : -1;
        const std::string label = selected >= 0 ? items_[selected] : placeholder_;
        const float popupGap = 8.0f;
        const float popupPadding = 6.0f;
        const float popupHeight = itemHeight_ * static_cast<float>(std::max(1, count)) + popupPadding * 2.0f;
        const float rootHeight = height_ + popupGap + popupHeight;
        const float visible = open_ ? 1.0f : 0.0f;
        const float popupOffsetY = open_ ? 0.0f : -6.0f;
        const float popupScale = open_ ? 1.0f : 0.96f;
        const std::function<void(int)> onChange = onChange_;
        const std::function<void(bool)> onOpenChange = onOpenChange_;

        ui_.stack(id_)
            .size(width_, rootHeight)
            .zIndex(zIndex_)
            .content([&] {
                ui_.rect(id_ + ".field")
                    .size(width_, height_)
                    .states(style_.field, style_.fieldHover, style_.fieldPressed)
                    .radius(style_.radius)
                    .border(1.0f, style_.border)
                    .transition(transition_)
                    .onClick([onOpenChange, open = open_] {
                        if (onOpenChange) {
                            onOpenChange(!open);
                        }
                    })
                    .build();

                ui_.text(id_ + ".label")
                    .x(14.0f)
                    .size(std::max(0.0f, width_ - 48.0f), height_)
                    .text(label)
                    .fontSize(16.0f)
                    .lineHeight(20.0f)
                    .color(selected >= 0 ? style_.text : style_.mutedText)
                    .verticalAlign(core::VerticalAlign::Center)
                    .build();

                ui_.text(id_ + ".chevron")
                    .x(std::max(0.0f, width_ - 34.0f))
                    .size(20.0f, height_)
                    .icon(open_ ? 0xF077 : 0xF078)
                    .fontSize(13.0f)
                    .lineHeight(18.0f)
                    .color(style_.accent)
                    .horizontalAlign(core::HorizontalAlign::Center)
                    .verticalAlign(core::VerticalAlign::Center)
                    .transition(transition_)
                    .animate(core::AnimProperty::TextColor)
                    .build();

                ui_.stack(id_ + ".popup")
                    .y(height_ + popupGap)
                    .size(width_, popupHeight)
                    .opacity(visible)
                    .translateY(popupOffsetY)
                    .scale(popupScale)
                    .transformOrigin(0.5f, 0.0f)
                    .transition(transition_)
                    .animate(core::AnimProperty::Opacity | core::AnimProperty::Transform)
                    .content([&] {
                        ui_.rect(id_ + ".popup.bg")
                            .size(width_, popupHeight)
                            .color(style_.popup)
                            .radius(style_.radius)
                            .border(1.0f, style_.border)
                            .shadow(style_.shadow)
                            .build();

                        ui_.rect(id_ + ".popup.hit")
                            .size(width_, popupHeight)
                            .states(theme::color(0.0f, 0.0f, 0.0f, 0.0f),
                                    theme::color(0.0f, 0.0f, 0.0f, 0.0f),
                                    theme::color(0.0f, 0.0f, 0.0f, 0.0f))
                            .disabled(!open_)
                            .onClick([] {})
                            .build();

                        for (int index = 0; index < count; ++index) {
                            const bool active = index == selected;
                            const float itemY = popupPadding + static_cast<float>(index) * itemHeight_;
                            ui_.rect(id_ + ".item." + std::to_string(index))
                                .x(popupPadding)
                                .y(itemY)
                                .size(std::max(0.0f, width_ - popupPadding * 2.0f), itemHeight_)
                                .states(theme::color(0.0f, 0.0f, 0.0f, 0.0f), style_.optionHover, style_.optionPressed)
                                .radius(std::max(4.0f, style_.radius - 4.0f))
                                .instantStates()
                                .disabled(!open_)
                                .onClick([onChange, onOpenChange, index] {
                                    if (onChange) {
                                        onChange(index);
                                    }
                                    if (onOpenChange) {
                                        onOpenChange(false);
                                    }
                                })
                                .build();

                            ui_.text(id_ + ".item.label." + std::to_string(index))
                                .x(popupPadding + 12.0f)
                                .y(itemY + std::max(0.0f, (itemHeight_ - 18.0f) * 0.5f))
                                .size(std::max(0.0f, width_ - popupPadding * 2.0f - 24.0f), 20.0f)
                                .text(items_[index])
                                .fontSize(15.0f)
                                .lineHeight(18.0f)
                                .color(active ? style_.accent : style_.text)
                                .transition(transition_)
                                .animate(core::AnimProperty::TextColor)
                                .build();
                        }
                    })
                    .build();
            })
            .build();
    }

private:
    core::dsl::Ui& ui_;
    std::string id_;
    std::vector<std::string> items_;
    DropdownStyle style_;
    core::Transition transition_ = core::Transition::make(0.16f, core::Ease::OutCubic);
    std::function<void(int)> onChange_;
    std::function<void(bool)> onOpenChange_;
    std::string placeholder_ = "Select";
    int selected_ = -1;
    bool open_ = false;
    float width_ = 260.0f;
    float height_ = 42.0f;
    float itemHeight_ = 34.0f;
    int zIndex_ = 20;
};

inline DropdownBuilder dropdown(core::dsl::Ui& ui, const std::string& id) {
    return DropdownBuilder(ui, id);
}

} // namespace components
