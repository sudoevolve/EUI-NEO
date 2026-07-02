#pragma once

#include "components/button.h"
#include "components/theme.h"
#include "core/dsl.h"

#include <algorithm>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace components {

struct NavbarItem {
    std::string id;
    std::string label;
    unsigned int icon = 0;
    int value = 0;
};

class NavbarBuilder {
public:
    NavbarBuilder(core::dsl::Ui& ui, std::string id)
        : ui_(ui), id_(std::move(id)) {}

    NavbarBuilder& size(float width, float height) { width_ = width; height_ = height; return *this; }
    NavbarBuilder& compact(bool value = true) { compact_ = value; return *this; }
    NavbarBuilder& theme(const theme::ThemeColorTokens& tokens) { tokens_ = tokens; return *this; }
    NavbarBuilder& transition(const core::Transition& value) { transition_ = value; return *this; }
    NavbarBuilder& selected(int value) { selected_ = value; return *this; }
    NavbarBuilder& brand(std::string title, unsigned int icon) {
        title_ = std::move(title);
        icon_ = icon;
        return *this;
    }
    NavbarBuilder& subtitle(std::string value) {
        subtitle_ = std::move(value);
        return *this;
    }
    NavbarBuilder& items(std::vector<NavbarItem> value) {
        items_ = std::move(value);
        return *this;
    }
    NavbarBuilder& footer(std::string text, unsigned int icon, std::function<void()> onClick) {
        footerText_ = std::move(text);
        footerIcon_ = icon;
        footerClick_ = std::move(onClick);
        return *this;
    }
    NavbarBuilder& onChange(std::function<void(int)> callback) {
        onChange_ = std::move(callback);
        return *this;
    }

    void build() {
        const float navWidth = compact_ ? 52.0f : std::max(52.0f, width_ - 60.0f);
        const float sideInset = compact_ ? std::max(0.0f, (width_ - navWidth) * 0.5f) : 30.0f;
        const float titleHeight = compact_ ? 0.0f : 36.0f;
        const float subtitleHeight = compact_ || subtitle_.empty() ? 0.0f : 24.0f;
        const float brandGap = 14.0f;
        const float titleGap = compact_ ? 0.0f : brandGap;
        const float subtitleGap = subtitleHeight > 0.0f ? brandGap : 0.0f;
        const float navTop = 30.0f + 34.0f + titleGap + titleHeight + subtitleGap + subtitleHeight + brandGap;
        const float activeY = navTop + static_cast<float>(activeVisualIndex()) * (navHeight_ + navGap_);

        ui_.stack(id_)
            .size(width_, height_)
            .content([&] {
                ui_.rect(id_ + ".bg")
                    .size(width_, height_)
                    .color(navbarColor())
                    .transition(transition_)
                    .animate(core::AnimProperty::Color)
                    .build();

                if (!items_.empty()) {
                    ui_.rect(id_ + ".accent")
                        .x(0.0f)
                        .y(activeY)
                        .size(4.0f, navHeight_)
                        .color(tokens_.primary)
                        .radius(2.0f)
                        .transition(transition_)
                        .animate(core::AnimProperty::Frame | core::AnimProperty::Color)
                        .build();
                }

                ui_.column(id_ + ".content")
                    .size(width_, std::max(0.0f, height_ - 42.0f))
                    .margin(0.0f, 30.0f, 0.0f, 0.0f)
                    .gap(brandGap)
                    .alignItems(core::Align::CENTER)
                    .content([&] {
                        ui_.text(id_ + ".brand.icon")
                            .size(navWidth, 34.0f)
                            .icon(icon_)
                            .fontSize(27.0f)
                            .lineHeight(32.0f)
                            .color(tokens_.primary)
                            .horizontalAlign(core::HorizontalAlign::Center)
                            .transition(transition_)
                            .build();

                        if (!compact_) {
                            ui_.text(id_ + ".brand.title")
                                .size(navWidth, titleHeight)
                                .text(title_)
                                .fontSize(30.0f)
                                .lineHeight(34.0f)
                                .fontWeight(820)
                                .color(textColor())
                                .horizontalAlign(core::HorizontalAlign::Center)
                                .transition(transition_)
                                .build();

                            if (!subtitle_.empty()) {
                                ui_.text(id_ + ".brand.subtitle")
                                    .size(navWidth, subtitleHeight)
                                    .text(subtitle_)
                                    .fontSize(14.0f)
                                    .lineHeight(20.0f)
                                    .color(mutedTextColor())
                                    .horizontalAlign(core::HorizontalAlign::Center)
                                    .transition(transition_)
                                    .build();
                            }
                        }

                        for (const NavbarItem& item : items_) {
                            navItem(item, navWidth);
                        }
                    })
                    .build();

                if (footerClick_) {
                    footerButton(sideInset, navWidth);
                }
            })
            .build();
    }

private:
    int activeVisualIndex() const {
        for (int i = 0; i < static_cast<int>(items_.size()); ++i) {
            if (items_[i].value == selected_) {
                return i;
            }
        }
        return 0;
    }

    core::Color withOpacity(core::Color color, float opacity) const {
        color.a *= std::clamp(opacity, 0.0f, 1.0f);
        return color;
    }

    core::Color navbarColor() const {
        return tokens_.dark
            ? core::mixColor(tokens_.background, core::Color{0.0f, 0.0f, 0.0f, 1.0f}, 0.24f)
            : tokens_.surface;
    }

    core::Color textColor() const {
        return theme::pageVisuals(tokens_).titleColor;
    }

    core::Color mutedTextColor() const {
        return theme::pageVisuals(tokens_).subtitleColor;
    }

    core::Color borderColor(float opacity = 1.0f) const {
        return withOpacity(tokens_.border, opacity);
    }

    core::Color shadowColor() const {
        return tokens_.dark
            ? core::Color{0.0f, 0.0f, 0.0f, 0.18f}
            : core::Color{0.10f, 0.14f, 0.22f, 0.08f};
    }

    core::Color buttonHoverColor(const core::Color& base) const {
        return theme::buttonHover(tokens_, base);
    }

    core::Color buttonPressedColor(const core::Color& base) const {
        return theme::buttonPressed(tokens_, base);
    }

    void navItem(const NavbarItem& item, float navWidth) {
        const bool active = item.value == selected_;
        const core::Color base = active ? tokens_.primary : tokens_.surface;
        const std::function<void(int)> onChange = onChange_;
        components::button(ui_, id_ + ".nav." + item.id)
            .size(navWidth, navHeight_)
            .icon(item.icon)
            .iconSize(compact_ ? 20.0f : 16.0f)
            .fontSize(17.0f)
            .text(compact_ ? "" : item.label)
            .colors(base, buttonHoverColor(base), buttonPressedColor(base))
            .textColor(active || tokens_.dark ? core::Color{0.94f, 0.97f, 1.0f, 1.0f} : textColor())
            .iconColor(active ? core::Color{0.94f, 0.97f, 1.0f, 1.0f} : (compact_ ? textColor() : tokens_.primary))
            .radius(12.0f)
            .border(1.0f, active ? theme::withAlpha(tokens_.primary, 0.58f) : borderColor(0.70f))
            .shadow(12.0f, 0.0f, 4.0f, shadowColor())
            .transition(transition_)
            .onClick([onChange, value = item.value] {
                if (onChange) {
                    onChange(value);
                }
            })
            .build();
    }

    void footerButton(float sideInset, float navWidth) {
        ui_.stack(id_ + ".footer")
            .x(sideInset)
            .y(std::max(0.0f, height_ - 82.0f))
            .size(navWidth, navHeight_)
            .content([&] {
                components::button(ui_, id_ + ".footer.button")
                    .size(navWidth, navHeight_)
                    .icon(footerIcon_)
                    .iconSize(compact_ ? 20.0f : 16.0f)
                    .fontSize(17.0f)
                    .text(compact_ ? "" : footerText_)
                    .colors(tokens_.surface, tokens_.surfaceHover, tokens_.surfaceActive)
                    .textColor(textColor())
                    .iconColor(tokens_.primary)
                    .radius(12.0f)
                    .border(1.0f, borderColor(0.80f))
                    .shadow(12.0f, 0.0f, 4.0f, shadowColor())
                    .transition(transition_)
                    .onClick(footerClick_)
                    .build();
            })
            .build();
    }

    core::dsl::Ui& ui_;
    std::string id_;
    std::string title_ = "App";
    std::string subtitle_;
    std::string footerText_;
    unsigned int icon_ = 0xF5FD;
    unsigned int footerIcon_ = 0;
    theme::ThemeColorTokens tokens_ = theme::dark();
    core::Transition transition_ = core::Transition::make(0.20f, core::Ease::OutCubic);
    std::function<void(int)> onChange_;
    std::function<void()> footerClick_;
    std::vector<NavbarItem> items_;
    float width_ = 272.0f;
    float height_ = 640.0f;
    float navHeight_ = 50.0f;
    float navGap_ = 14.0f;
    int selected_ = 0;
    bool compact_ = false;
};

inline NavbarBuilder navbar(core::dsl::Ui& ui, const std::string& id) {
    return NavbarBuilder(ui, id);
}

} // namespace components
