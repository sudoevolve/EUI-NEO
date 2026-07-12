#pragma once

#include "components/theme.h"
#include "core/dsl.h"

#include <algorithm>
#include <functional>
#include <string>
#include <utility>

namespace components {

struct ButtonStyle {
    ButtonStyle() : ButtonStyle(theme::dark()) {}

    explicit ButtonStyle(const theme::ThemeColorTokens& tokens, bool primary = true) {
        const core::Color base = primary ? tokens.primary : tokens.surface;
        normal = base;
        hover = theme::buttonHover(tokens, base);
        pressed = theme::buttonPressed(tokens, base);
        text = primary || tokens.dark ? core::Color{0.94f, 0.97f, 1.0f, 1.0f} : tokens.text;
        icon = text;
        border = theme::buttonBorder(tokens, primary);
        shadow = theme::buttonShadow(tokens);
    }

    core::Color normal;
    core::Color hover;
    core::Color pressed;
    core::Color text;
    core::Color icon;
    core::Border border;
    core::Shadow shadow;
    float radius = 16.0f;
    float opacity = 1.0f;
    float pressScale = 0.965f;
};

class ButtonBuilder {
public:
    ButtonBuilder(core::dsl::Ui& ui, std::string id)
        : ui_(ui), id_(std::move(id)) {}

    ButtonBuilder& x(float value) { x_ = value; hasX_ = true; return *this; }
    ButtonBuilder& y(float value) { y_ = value; hasY_ = true; return *this; }
    ButtonBuilder& position(float xValue, float yValue) { return x(xValue).y(yValue); }
    ButtonBuilder& size(float width, float height) { width_ = width; height_ = height; return *this; }
    ButtonBuilder& scale(float value) { scale_ = value; return *this; }
    ButtonBuilder& text(const std::string& value) { text_ = value; return *this; }
    ButtonBuilder& icon(unsigned int codepoint) { icon_ = core::dsl::utf8(codepoint); return *this; }
    ButtonBuilder& icon(const std::string& value) { icon_ = value; return *this; }
    ButtonBuilder& fontSize(float value) { fontSize_ = value; return *this; }
    ButtonBuilder& iconSize(float value) { iconSize_ = value; return *this; }
    ButtonBuilder& textColor(const core::Color& value) { style_.text = value; return *this; }
    ButtonBuilder& iconColor(const core::Color& value) { style_.icon = value; return *this; }
    ButtonBuilder& style(const ButtonStyle& value) { style_ = value; return *this; }
    ButtonBuilder& theme(const theme::ThemeColorTokens& tokens, bool primary = true) {
        style_ = ButtonStyle(tokens, primary);
        return *this;
    }
    ButtonBuilder& radius(float value) { style_.radius = value; return *this; }
    ButtonBuilder& opacity(float value) { style_.opacity = std::clamp(value, 0.0f, 1.0f); return *this; }
    ButtonBuilder& disabled(bool value = true) { disabled_ = value; return *this; }
    ButtonBuilder& preserveFocusOnPress(bool value = true) { preserveFocusOnPress_ = value; return *this; }
    ButtonBuilder& translate(float x, float y) { translateX_ = x; translateY_ = y; return *this; }
    ButtonBuilder& translateX(float value) { translateX_ = value; return *this; }
    ButtonBuilder& translateY(float value) { translateY_ = value; return *this; }
    ButtonBuilder& pressScale(float value) { style_.pressScale = std::clamp(value, 0.80f, 1.0f); return *this; }
    ButtonBuilder& border(float width, const core::Color& color) { style_.border = {width, color}; return *this; }
    ButtonBuilder& shadow(float blur, float offsetX, float offsetY, const core::Color& color) {
        style_.shadow = {true, {offsetX, offsetY}, blur, 0.0f, color};
        return *this;
    }
    ButtonBuilder& colors(const core::Color& normal, const core::Color& hover, const core::Color& pressed) {
        style_.normal = normal;
        style_.hover = hover;
        style_.pressed = pressed;
        return *this;
    }
    ButtonBuilder& transition(const core::Transition& value) { transition_ = value; return *this; }
    ButtonBuilder& transition(float duration, core::Ease ease = core::Ease::OutCubic) {
        transition_ = core::Transition::make(duration, ease);
        return *this;
    }
    ButtonBuilder& onClick(std::function<void()> callback) { onClick_ = std::move(callback); return *this; }
    ButtonBuilder& onPress(std::function<void()> callback) { onPress_ = std::move(callback); return *this; }
    ButtonBuilder& onRelease(std::function<void()> callback) { onRelease_ = std::move(callback); return *this; }
    ButtonBuilder& onFrame(std::function<void(float)> callback) { onFrame_ = std::move(callback); return *this; }
    ButtonBuilder& onContextMenu(std::function<void(const core::PointerEvent&, const core::Rect&)> callback) {
        onContextMenu_ = std::move(callback);
        return *this;
    }

    void build() {
        const float w = width_ * scale_;
        const float h = height_ * scale_;
        const float font = fontSize_ > 0.0f ? fontSize_ * scale_ : h * 0.46f;
        const float iconFont = iconSize_ > 0.0f ? iconSize_ * scale_ : font * 0.92f;
        const bool hasIcon = !icon_.empty();
        const bool hasText = !text_.empty();
        const float iconWidth = hasIcon ? iconFont * 1.15f : 0.0f;
        const float gap = hasIcon && hasText ? std::max(6.0f * scale_, h * 0.12f) : 0.0f;
        const float labelWidth = hasIcon && hasText ? std::max(0.0f, w - iconWidth - gap - 32.0f * scale_) : w;
        core::Border border = style_.border;
        border.width *= scale_;

        core::Shadow shadow = style_.shadow;
        shadow.offset.x *= scale_;
        shadow.offset.y *= scale_;
        shadow.blur *= scale_;
        shadow.spread *= scale_;
        core::Color textColor = style_.text;
        core::Color iconColor = style_.icon;
        textColor.a *= style_.opacity;
        iconColor.a *= style_.opacity;
        const std::function<void()> onPress = onPress_;
        const std::function<void()> onRelease = onRelease_;

        auto root = ui_.stack(id_)
            .size(w, h)
            .visualStateFrom(id_ + ".bg", style_.pressScale);
        if (hasX_) {
            root.x(x_);
        }
        if (hasY_) {
            root.y(y_);
        }
        root.content([&] {
                auto bg = ui_.rect(id_ + ".bg")
                    .size(w, h)
                    .states(style_.normal, style_.hover, style_.pressed)
                    .radius(style_.radius * scale_)
                    .opacity(style_.opacity)
                    .border(border)
                    .shadow(shadow)
                    .translate(translateX_, translateY_)
                    .transition(transition_)
                    .disabled(disabled_)
                    .preserveFocusOnPress(preserveFocusOnPress_)
                    .onClick(onClick_)
                    .onContextMenu(onContextMenu_);
                if (onPress) {
                    bg.onPress([onPress](const core::PointerEvent&, const core::Rect&) { onPress(); });
                }
                if (onRelease) {
                    bg.onRelease([onRelease](const core::PointerEvent&, const core::Rect&) { onRelease(); });
                }
                if (onFrame_) {
                    bg.onFrame(onFrame_);
                }
                bg.build();

                ui_.row(id_ + ".content")
                    .size(w, h)
                    .gap(gap)
                    .justifyContent(core::Align::CENTER)
                    .alignItems(core::Align::CENTER)
                    .content([&] {
                        if (hasIcon) {
                            ui_.text(id_ + ".icon")
                                .size(iconWidth, h)
                                .icon(icon_)
                                .fontSize(iconFont)
                                .lineHeight(iconFont)
                                .color(iconColor)
                                .horizontalAlign(core::HorizontalAlign::Center)
                                .verticalAlign(core::VerticalAlign::Center)
                                .transition(transition_)
                                .build();
                        }

                        if (hasText) {
                            ui_.text(id_ + ".text")
                                .size(labelWidth, h)
                                .text(text_)
                                .fontSize(font)
                                .lineHeight(font)
                                .color(textColor)
                                .horizontalAlign(hasIcon ? core::HorizontalAlign::Left : core::HorizontalAlign::Center)
                                .verticalAlign(core::VerticalAlign::Center)
                                .transition(transition_)
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
    std::string text_ = "Button";
    std::string icon_;
    ButtonStyle style_;
    core::Transition transition_;
    std::function<void()> onClick_;
    std::function<void()> onPress_;
    std::function<void()> onRelease_;
    std::function<void(float)> onFrame_;
    std::function<void(const core::PointerEvent&, const core::Rect&)> onContextMenu_;
    float width_ = 240.0f;
    float height_ = 70.0f;
    float scale_ = 1.0f;
    float fontSize_ = 0.0f;
    float iconSize_ = 0.0f;
    float translateX_ = 0.0f;
    float translateY_ = 0.0f;
    float x_ = 0.0f;
    float y_ = 0.0f;
    bool disabled_ = false;
    bool preserveFocusOnPress_ = false;
    bool hasX_ = false;
    bool hasY_ = false;
};

inline ButtonBuilder button(core::dsl::Ui& ui, const std::string& id) {
    return ButtonBuilder(ui, id);
}

} // namespace components
