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
        const theme::ThemeMetricTokens& metrics = tokens_.metrics;
        const float compactWidth = metrics.control.navigation + metrics.spacing.micro;
        const float navWidth = compact_ ? compactWidth : std::max(compactWidth, width_ - metrics.spacing.header * 2.0f);
        const float sideInset = compact_ ? std::max(0.0f, (width_ - navWidth) * 0.5f) : metrics.spacing.header;
        const float titleHeight = compact_ ? 0.0f : metrics.control.segmented;
        const float subtitleHeight = compact_ || subtitle_.empty() ? 0.0f : metrics.control.switchHeight;
        const float brandGap = metrics.typography.label;
        const float navHeight = metrics.control.navigation;
        const float navGap = metrics.typography.label;
        const float titleGap = compact_ ? 0.0f : brandGap;
        const float subtitleGap = subtitleHeight > 0.0f ? brandGap : 0.0f;
        const float navTop = metrics.spacing.header + metrics.control.menuItem + titleGap +
                             titleHeight + subtitleGap + subtitleHeight + brandGap;
        const float activeY = navTop + static_cast<float>(activeVisualIndex()) * (navHeight + navGap);

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
                        .size(metrics.spacing.tiny, navHeight)
                        .color(tokens_.primary)
                        .radius(metrics.radius.micro)
                        .transition(transition_)
                        .animate(core::AnimProperty::Frame | core::AnimProperty::Color)
                        .build();
                }

                ui_.column(id_ + ".content")
                    .size(width_, std::max(0.0f, height_ - metrics.control.control))
                    .margin(0.0f, metrics.spacing.header, 0.0f, 0.0f)
                    .gap(brandGap)
                    .alignItems(core::Align::CENTER)
                    .content([&] {
                        ui_.text(id_ + ".brand.icon")
                            .size(navWidth, metrics.control.menuItem)
                            .icon(icon_)
                            .fontSize(metrics.typography.headline)
                            .lineHeight(metrics.typography.headline + metrics.typography.lineGapRelaxed)
                            .color(tokens_.primary)
                            .horizontalAlign(core::HorizontalAlign::Center)
                            .transition(transition_)
                            .build();

                        if (!compact_) {
                            ui_.text(id_ + ".brand.title")
                                .size(navWidth, titleHeight)
                                .text(title_)
                                .fontSize(metrics.typography.displayCompact)
                                .lineHeight(metrics.typography.displayCompact + metrics.typography.lineGap)
                                .fontWeight(820)
                                .color(textColor())
                                .horizontalAlign(core::HorizontalAlign::Center)
                                .transition(transition_)
                                .build();

                            if (!subtitle_.empty()) {
                                ui_.text(id_ + ".brand.subtitle")
                                    .size(navWidth, subtitleHeight)
                                    .text(subtitle_)
                                    .fontSize(metrics.typography.label)
                                    .lineHeight(metrics.typography.label + metrics.typography.lineGapLoose)
                                    .color(mutedTextColor())
                                    .horizontalAlign(core::HorizontalAlign::Center)
                                    .transition(transition_)
                                    .build();
                            }
                        }

                        for (const NavbarItem& item : items_) {
                            navItem(item, navWidth, navHeight);
                        }
                    })
                    .build();

                if (footerClick_) {
                    footerButton(sideInset, navWidth, navHeight);
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

    void navItem(const NavbarItem& item, float navWidth, float navHeight) {
        const theme::ThemeMetricTokens& metrics = tokens_.metrics;
        const bool active = item.value == selected_;
        const core::Color base = active ? tokens_.primary : tokens_.surface;
        const std::function<void(int)> onChange = onChange_;
        components::button(ui_, id_ + ".nav." + item.id)
            .theme(tokens_, active)
            .size(navWidth, navHeight)
            .icon(item.icon)
            .iconSize(compact_ ? metrics.spacing.large : metrics.spacing.section)
            .fontSize(metrics.typography.input)
            .text(compact_ ? "" : item.label)
            .colors(base, buttonHoverColor(base), buttonPressedColor(base))
            .textColor(active || tokens_.dark ? core::Color{0.94f, 0.97f, 1.0f, 1.0f} : textColor())
            .iconColor(active ? core::Color{0.94f, 0.97f, 1.0f, 1.0f} : (compact_ ? textColor() : tokens_.primary))
            .radius(metrics.radius.card)
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

    void footerButton(float sideInset, float navWidth, float navHeight) {
        const theme::ThemeMetricTokens& metrics = tokens_.metrics;
        ui_.stack(id_ + ".footer")
            .x(sideInset)
            .y(std::max(0.0f, height_ - metrics.spacing.overlay - metrics.control.menuItem))
            .size(navWidth, navHeight)
            .content([&] {
                components::button(ui_, id_ + ".footer.button")
                    .theme(tokens_, false)
                    .size(navWidth, navHeight)
                    .icon(footerIcon_)
                    .iconSize(compact_ ? metrics.spacing.large : metrics.spacing.section)
                    .fontSize(metrics.typography.input)
                    .text(compact_ ? "" : footerText_)
                    .colors(tokens_.surface, tokens_.surfaceHover, tokens_.surfaceActive)
                    .textColor(textColor())
                    .iconColor(tokens_.primary)
                    .radius(metrics.radius.card)
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
    int selected_ = 0;
    bool compact_ = false;
};

inline NavbarBuilder navbar(core::dsl::Ui& ui, const std::string& id) {
    return NavbarBuilder(ui, id);
}

} // namespace components
