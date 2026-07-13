#pragma once

#include "components/theme.h"
#include "core/dsl.h"

#include <algorithm>
#include <functional>
#include <string>
#include <utility>

namespace components {

struct CardStyle {
    CardStyle() : CardStyle(theme::dark()) {}

    explicit CardStyle(const theme::ThemeColorTokens& tokens) {
        color = theme::pageVisuals(tokens).cardColor;
        border = theme::border(tokens, 1.0f, 0.72f);
        shadow = theme::panelShadow(tokens);
        radius = theme::pageVisuals(tokens).sectionRounding;
        inset = tokens.metrics.spacing.large;
    }

    core::Color color;
    core::Gradient gradient;
    core::Border border;
    core::Shadow shadow;
    float radius = 14.0f;
    float opacity = 1.0f;
    float inset = 20.0f;
};

class CardBuilder {
public:
    CardBuilder(core::dsl::Ui& ui, std::string id)
        : ui_(ui), id_(std::move(id)) {}

    CardBuilder& x(float value) { x_ = value; hasX_ = true; return *this; }
    CardBuilder& y(float value) { y_ = value; hasY_ = true; return *this; }
    CardBuilder& position(float xValue, float yValue) {
        x_ = xValue;
        y_ = yValue;
        hasX_ = true;
        hasY_ = true;
        return *this;
    }
    CardBuilder& width(float value) { width_ = core::SizeValue::fixed(std::max(0.0f, value)); return *this; }
    CardBuilder& width(core::SizeValue value) { width_ = value; return *this; }
    CardBuilder& height(float value) { height_ = core::SizeValue::fixed(std::max(0.0f, value)); return *this; }
    CardBuilder& height(core::SizeValue value) { height_ = value; return *this; }
    CardBuilder& size(float width, float height) {
        width_ = core::SizeValue::fixed(std::max(0.0f, width));
        height_ = core::SizeValue::fixed(std::max(0.0f, height));
        return *this;
    }
    CardBuilder& wrapContentHeight() { height_ = core::SizeValue::wrapContent(); return *this; }
    CardBuilder& padding(float value) { style_.inset = std::max(0.0f, value); return *this; }
    CardBuilder& style(const CardStyle& value) { style_ = value; return *this; }
    CardBuilder& theme(const theme::ThemeColorTokens& tokens) { style_ = CardStyle(tokens); return *this; }
    CardBuilder& color(const core::Color& value) { style_.color = value; return *this; }
    CardBuilder& radius(float value) { style_.radius = std::max(0.0f, value); return *this; }
    CardBuilder& border(float width, const core::Color& color) { style_.border = {std::max(0.0f, width), color}; return *this; }
    CardBuilder& shadow(const core::Shadow& value) { style_.shadow = value; return *this; }
    CardBuilder& opacity(float value) { style_.opacity = std::clamp(value, 0.0f, 1.0f); return *this; }
    CardBuilder& zIndex(int value) { zIndex_ = value; return *this; }

    template <typename ComposeFn>
    CardBuilder& content(ComposeFn&& compose) {
        content_ = std::forward<ComposeFn>(compose);
        return *this;
    }

    void build() {
        const bool fixedWidth = width_.mode == core::SizeMode::Fixed;
        const bool fixedHeight = height_.mode == core::SizeMode::Fixed;
        const float contentWidth = fixedWidth ? std::max(0.0f, width_.value - style_.inset * 2.0f) : 0.0f;
        const float contentHeight = fixedHeight ? std::max(0.0f, height_.value - style_.inset * 2.0f) : 0.0f;
        const core::SizeValue backgroundWidth = fixedWidth ? core::SizeValue::fixed(width_.value) : core::SizeValue::fill();
        const core::SizeValue backgroundHeight = fixedHeight ? core::SizeValue::fixed(height_.value) : core::SizeValue::fill();

        auto root = ui_.stack(id_)
            .width(width_)
            .height(height_)
            .zIndex(zIndex_);
        if (hasX_) {
            root.x(x_);
        }
        if (hasY_) {
            root.y(y_);
        }
        root.content([&] {
                ui_.rect(id_ + ".bg")
                    .size(backgroundWidth, backgroundHeight)
                    .color(style_.color)
                    .gradient(style_.gradient)
                    .radius(style_.radius)
                    .border(style_.border)
                    .shadow(style_.shadow)
                    .opacity(style_.opacity)
                    .build();

                auto content = ui_.stack(id_ + ".content")
                    .position(style_.inset, style_.inset)
                    .width(fixedWidth ? core::SizeValue::fixed(contentWidth) : core::SizeValue::fill())
                    .height(fixedHeight ? core::SizeValue::fixed(contentHeight) : core::SizeValue::wrapContent())
                    .margin(0.0f, 0.0f, style_.inset, style_.inset);
                content.content([&] {
                    if (content_) {
                        content_();
                    }
                }).build();
            })
            .build();
    }

private:
    core::dsl::Ui& ui_;
    std::string id_;
    CardStyle style_;
    std::function<void()> content_;
    core::SizeValue width_ = core::SizeValue::fixed(320.0f);
    core::SizeValue height_ = core::SizeValue::wrapContent();
    float x_ = 0.0f;
    float y_ = 0.0f;
    bool hasX_ = false;
    bool hasY_ = false;
    int zIndex_ = 0;
};

inline CardBuilder card(core::dsl::Ui& ui, const std::string& id) {
    return CardBuilder(ui, id);
}

} // namespace components
