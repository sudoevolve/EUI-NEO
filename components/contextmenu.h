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

struct ContextMenuStyle {
    ContextMenuStyle() : ContextMenuStyle(theme::dark()) {}

    explicit ContextMenuStyle(const theme::ThemeColorTokens& tokens) {
        background = tokens.dark
            ? core::mixColor(tokens.surface, theme::color(0.0f, 0.0f, 0.0f), 0.16f)
            : tokens.surface;
        hover = tokens.surfaceHover;
        pressed = tokens.surfaceActive;
        text = tokens.text;
        mutedText = theme::withOpacity(tokens.text, 0.54f);
        border = theme::withOpacity(tokens.border, 0.82f);
        shadow = theme::popupShadow(tokens);
    }

    core::Color background;
    core::Color hover;
    core::Color pressed;
    core::Color text;
    core::Color mutedText;
    core::Color border;
    core::Shadow shadow;
    float radius = 12.0f;
};

class ContextMenuBuilder {
public:
    ContextMenuBuilder(core::dsl::Ui& ui, std::string id)
        : ui_(ui), id_(std::move(id)) {}

    ContextMenuBuilder& open(bool value = true) { open_ = value; return *this; }
    ContextMenuBuilder& bindOpen(eui::Signal<bool>& signal) {
        open(signal.get());
        onOpenChange([&signal](bool value) { signal.set(value); });
        return *this;
    }
    ContextMenuBuilder& screen(float width, float height) { screenWidth_ = width; screenHeight_ = height; return *this; }
    ContextMenuBuilder& position(float x, float y) { x_ = x; y_ = y; return *this; }
    ContextMenuBuilder& size(float width, float itemHeight) { width_ = width; itemHeight_ = itemHeight; return *this; }
    ContextMenuBuilder& items(std::vector<std::string> value) { items_ = std::move(value); return *this; }
    ContextMenuBuilder& style(const ContextMenuStyle& value) { style_ = value; return *this; }
    ContextMenuBuilder& theme(const theme::ThemeColorTokens& tokens) { style_ = ContextMenuStyle(tokens); return *this; }
    ContextMenuBuilder& transition(const core::Transition& value) { transition_ = value; return *this; }
    ContextMenuBuilder& zIndex(int value) { zIndex_ = value; return *this; }
    ContextMenuBuilder& onSelect(std::function<void(int)> callback) { onSelect_ = std::move(callback); return *this; }
    ContextMenuBuilder& onOpenChange(std::function<void(bool)> callback) { onOpenChange_ = std::move(callback); return *this; }

    void build() {
        if (items_.empty()) {
            return;
        }

        const float inset = 6.0f;
        const float width = std::min(width_, std::max(0.0f, screenWidth_ - 16.0f));
        const float height = itemHeight_ * static_cast<float>(items_.size()) + inset * 2.0f;
        const float x = std::clamp(x_, 8.0f, std::max(8.0f, screenWidth_ - width - 8.0f));
        const float y = std::clamp(y_, 8.0f, std::max(8.0f, screenHeight_ - height - 8.0f));
        const float visible = open_ ? 1.0f : 0.0f;
        const float menuScale = open_ ? 1.0f : 0.94f;
        const float menuOffsetY = open_ ? 0.0f : -4.0f;
        const std::function<void()> requestDismiss = dismissCallback();
        const std::function<void(int)> onSelect = onSelect_;

        ui_.stack(id_)
            .size(screenWidth_, screenHeight_)
            .zIndex(zIndex_)
            .content([&] {
                if (open_) {
                    ui_.rect(id_ + ".dismiss")
                        .size(screenWidth_, screenHeight_)
                        .states(theme::color(0.0f, 0.0f, 0.0f, 0.0f),
                                theme::color(0.0f, 0.0f, 0.0f, 0.0f),
                                theme::color(0.0f, 0.0f, 0.0f, 0.0f))
                        .onClick(requestDismiss)
                        .onScroll([](const core::ScrollEvent&) {})
                        .build();
                }

                ui_.stack(id_ + ".menu")
                    .x(x)
                    .y(y)
                    .size(width, height)
                    .opacity(visible)
                    .translateY(menuOffsetY)
                    .scale(menuScale)
                    .transformOrigin(0.0f, 0.0f)
                    .transition(transition_)
                    .animate(core::AnimProperty::Opacity | core::AnimProperty::Transform)
                    .content([&] {
                        if (!open_) {
                            return;
                        }

                        ui_.rect(id_ + ".bg")
                            .size(width, height)
                            .color(style_.background)
                            .radius(style_.radius)
                            .border(1.0f, style_.border)
                            .shadow(style_.shadow)
                            .build();

                        ui_.rect(id_ + ".hit")
                            .size(width, height)
                            .states(theme::color(0.0f, 0.0f, 0.0f, 0.0f),
                                    theme::color(0.0f, 0.0f, 0.0f, 0.0f),
                                    theme::color(0.0f, 0.0f, 0.0f, 0.0f))
                            .disabled(!open_)
                            .onClick([] {})
                            .build();

                        for (int index = 0; index < static_cast<int>(items_.size()); ++index) {
                            const float itemY = inset + static_cast<float>(index) * itemHeight_;
                            ui_.rect(id_ + ".item." + std::to_string(index))
                                .x(inset)
                                .y(itemY)
                                .size(std::max(0.0f, width - inset * 2.0f), itemHeight_)
                                .states(theme::color(0.0f, 0.0f, 0.0f, 0.0f), style_.hover, style_.pressed)
                                .radius(std::max(4.0f, style_.radius - 4.0f))
                                .instantStates()
                                .disabled(!open_)
                                .onClick([onSelect, index] {
                                    if (onSelect) {
                                        onSelect(index);
                                    }
                                })
                                .build();

                            ui_.text(id_ + ".label." + std::to_string(index))
                                .x(inset + 12.0f)
                                .y(itemY + std::max(0.0f, (itemHeight_ - 18.0f) * 0.5f))
                                .size(std::max(0.0f, width - inset * 2.0f - 24.0f), 20.0f)
                                .text(items_[index])
                                .fontSize(15.0f)
                                .lineHeight(18.0f)
                                .color(index == static_cast<int>(items_.size()) - 1 ? style_.mutedText : style_.text)
                                .build();
                        }
                    })
                    .build();
            })
            .build();
    }

private:
    std::function<void()> dismissCallback() const {
        const std::function<void(bool)> onOpenChange = onOpenChange_;
        return [onOpenChange] {
            if (onOpenChange) {
                onOpenChange(false);
            }
        };
    }

    core::dsl::Ui& ui_;
    std::string id_;
    std::vector<std::string> items_;
    ContextMenuStyle style_;
    core::Transition transition_ = core::Transition::make(0.12f, core::Ease::OutCubic);
    std::function<void(int)> onSelect_;
    std::function<void(bool)> onOpenChange_;
    bool open_ = false;
    float screenWidth_ = 800.0f;
    float screenHeight_ = 600.0f;
    float x_ = 0.0f;
    float y_ = 0.0f;
    float width_ = 190.0f;
    float itemHeight_ = 36.0f;
    int zIndex_ = 1050;
};

inline ContextMenuBuilder contextMenu(core::dsl::Ui& ui, const std::string& id) {
    return ContextMenuBuilder(ui, id);
}

} // namespace components
