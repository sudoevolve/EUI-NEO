#pragma once

#include "core/event.h"
#include "core/dsl.h"
#include "core/primitive.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <utility>

namespace components {

struct ButtonStyle {
    core::Color normal = {0.13f, 0.47f, 0.86f, 1.0f};
    core::Color hover = {0.20f, 0.62f, 0.95f, 1.0f};
    core::Color pressed = {0.08f, 0.30f, 0.62f, 1.0f};
    core::Gradient gradient;
    core::Border border;
    core::Shadow shadow;
    float radius = 16.0f;
    float opacity = 1.0f;
};

class Button {
public:
    Button(float x, float y, float width, float height)
        : bounds_{x, y, width, height}, background_(x, y, width, height) {}

    bool initialize() {
        applyStaticStyle();
        return background_.initialize();
    }

    bool update(GLFWwindow* window, float deltaSeconds, float dpiScale = 1.0f) {
        interaction_.update(bounds_, core::readPointerEvent(window, dpiScale));

        const float oldHoverBlend = hoverBlend_;
        const float oldPressBlend = pressBlend_;
        hoverBlend_ = animateToward(hoverBlend_, interaction_.hover ? 1.0f : 0.0f, 9.0f, deltaSeconds);
        pressBlend_ = animateToward(pressBlend_, interaction_.pressed ? 1.0f : 0.0f, 16.0f, deltaSeconds);
        visualChanged_ = std::fabs(oldHoverBlend - hoverBlend_) > 0.0001f ||
                         std::fabs(oldPressBlend - pressBlend_) > 0.0001f ||
                         interaction_.changed;

        return interaction_.clicked;
    }

    void render(int windowWidth, int windowHeight) {
        const core::Color hoverColor = core::mixColor(style_.normal, style_.hover, hoverBlend_);
        const core::Color currentColor = core::mixColor(hoverColor, style_.pressed, pressBlend_);

        background_.setBounds(bounds_.x, bounds_.y, bounds_.width, bounds_.height);
        background_.setCornerRadius(style_.radius);
        background_.setColor(currentColor);
        background_.setOpacity(style_.opacity);
        const float pressScale = 1.0f - (1.0f - 0.965f) * pressBlend_;
        background_.setTransformOrigin(0.5f, 0.5f);
        background_.setScale(pressScale, pressScale);
        background_.render(windowWidth, windowHeight);
    }

    void destroy() {
        background_.destroy();
    }

    bool isHovered() const { return interaction_.hover; }
    bool isPressed() const { return interaction_.pressed; }
    bool isDragging() const { return interaction_.drag; }
    bool wasClicked() const { return interaction_.clicked; }
    bool hasVisualChange() const { return visualChanged_; }
    bool needsRender() const { return visualChanged_; }
    bool isAnimating() const {
        return std::fabs(hoverBlend_ - (interaction_.hover ? 1.0f : 0.0f)) > 0.0001f ||
               std::fabs(pressBlend_ - (interaction_.pressed ? 1.0f : 0.0f)) > 0.0001f;
    }

    Button& setFrame(float x, float y, float width, float height) {
        bounds_ = {x, y, width, height};
        background_.setBounds(x, y, width, height);
        return *this;
    }

    Button& setStyle(const ButtonStyle& style) {
        style_ = style;
        applyStaticStyle();
        return *this;
    }

    Button& setOpacity(float opacity) {
        style_.opacity = opacity;
        background_.setOpacity(opacity);
        return *this;
    }

    Button& setCornerRadius(float radius) {
        style_.radius = radius;
        background_.setCornerRadius(radius);
        return *this;
    }

    Button& setColors(const core::Color& normal, const core::Color& hover, const core::Color& pressed) {
        style_.normal = normal;
        style_.hover = hover;
        style_.pressed = pressed;
        background_.setColor(normal);
        return *this;
    }

    Button& setGradient(const core::Gradient& gradient) {
        style_.gradient = gradient;
        background_.setGradient(gradient);
        return *this;
    }

    Button& setBorder(const core::Border& border) {
        style_.border = border;
        background_.setBorder(border);
        return *this;
    }

    Button& setShadow(const core::Shadow& shadow) {
        style_.shadow = shadow;
        background_.setShadow(shadow);
        return *this;
    }

    Button& setTranslate(float x, float y) {
        background_.setTranslate(x, y);
        return *this;
    }

    Button& setScale(float x, float y) {
        background_.setScale(x, y);
        return *this;
    }

    Button& setRotate(float radians) {
        background_.setRotate(radians);
        return *this;
    }

    Button& setTransformOrigin(float x, float y) {
        background_.setTransformOrigin(x, y);
        return *this;
    }

private:
    void applyStaticStyle() {
        background_.setCornerRadius(style_.radius);
        background_.setColor(style_.normal);
        background_.setOpacity(style_.opacity);
        background_.setGradient(style_.gradient);
        background_.setBorder(style_.border);
        background_.setShadow(style_.shadow);
    }

    static float animateToward(float current, float target, float speed, float deltaSeconds) {
        if (deltaSeconds <= 0.0f) {
            return current;
        }

        const float next = target + (current - target) * std::exp(-speed * deltaSeconds);
        return std::fabs(next - target) < 0.001f ? target : next;
    }

    core::Rect bounds_;
    core::RoundedRectPrimitive background_;
    core::InteractionState interaction_;
    ButtonStyle style_;
    bool visualChanged_ = false;
    float hoverBlend_ = 0.0f;
    float pressBlend_ = 0.0f;
};

class ButtonBuilder {
public:
    ButtonBuilder(core::dsl::Ui& ui, std::string id)
        : ui_(ui), id_(std::move(id)) {}

    ButtonBuilder& size(float width, float height) {
        width_ = width;
        height_ = height;
        return *this;
    }

    ButtonBuilder& scale(float value) {
        scaleX_ = value;
        scaleY_ = value;
        return *this;
    }

    ButtonBuilder& scale(float x, float y) {
        scaleX_ = x;
        scaleY_ = y;
        return *this;
    }

    ButtonBuilder& pressScale(float value) {
        pressScale_ = std::clamp(value, 0.80f, 1.0f);
        return *this;
    }

    ButtonBuilder& text(const std::string& value) {
        text_ = value;
        return *this;
    }

    ButtonBuilder& icon(unsigned int codepoint) {
        icon_ = core::dsl::utf8(codepoint);
        hasIcon_ = true;
        return *this;
    }

    ButtonBuilder& icon(const std::string& value) {
        icon_ = value;
        hasIcon_ = true;
        return *this;
    }

    ButtonBuilder& iconSize(float value) {
        iconSize_ = value;
        hasIconSize_ = true;
        return *this;
    }

    ButtonBuilder& iconColor(const core::Color& value) {
        iconColor_ = value;
        return *this;
    }

    ButtonBuilder& fontSize(float value) {
        fontSize_ = value;
        hasFontSize_ = true;
        return *this;
    }

    ButtonBuilder& textColor(const core::Color& value) {
        textColor_ = value;
        return *this;
    }

    ButtonBuilder& colors(const core::Color& normal, const core::Color& hover, const core::Color& pressed) {
        normal_ = normal;
        hover_ = hover;
        pressed_ = pressed;
        return *this;
    }

    ButtonBuilder& rounding(float value) {
        radius_ = value;
        return *this;
    }

    ButtonBuilder& radius(float value) {
        radius_ = value;
        return *this;
    }

    ButtonBuilder& border(float width, const core::Color& color) {
        border_ = {width, color};
        return *this;
    }

    ButtonBuilder& shadow(float blur, float offsetX, float offsetY, const core::Color& color) {
        shadow_ = {true, {offsetX, offsetY}, blur, 0.0f, color};
        return *this;
    }

    ButtonBuilder& onClick(std::function<void()> callback) {
        onClick_ = std::move(callback);
        return *this;
    }

    void build() {
        const float scaledWidth = width_ * scaleX_;
        const float scaledHeight = height_ * scaleY_;
        const float averageScale = (scaleX_ + scaleY_) * 0.5f;
        const float resolvedFontSize = hasFontSize_ ? fontSize_ * averageScale : scaledHeight * 0.46f;
        const float resolvedLineHeight = resolvedFontSize * 1.18f;
        const float resolvedIconSize = hasIconSize_ ? iconSize_ : resolvedFontSize * 0.92f;
        const float iconBoxWidth = hasIcon_ ? resolvedIconSize * 1.15f : 0.0f;
        const float contentGap = hasIcon_ ? std::max(6.0f * averageScale, scaledHeight * 0.12f) : 0.0f;
        const float labelWidth = hasIcon_ ? std::max(0.0f, scaledWidth - iconBoxWidth - contentGap - 32.0f * averageScale) : scaledWidth;
        core::Border scaledBorder = border_;
        scaledBorder.width *= averageScale;
        core::Shadow scaledShadow = shadow_;
        scaledShadow.offset.x *= averageScale;
        scaledShadow.offset.y *= averageScale;
        scaledShadow.blur *= averageScale;
        scaledShadow.spread *= averageScale;

        ui_.stack(id_)
            .size(scaledWidth, scaledHeight)
            .visualStateFrom(id_ + ".bg", pressScale_)
            .content([&] {
                ui_.rect(id_ + ".bg")
                    .size(scaledWidth, scaledHeight)
                    .states(normal_, hover_, pressed_)
                    .radius(radius_ * averageScale)
                    .border(scaledBorder)
                    .shadow(scaledShadow)
                    .onClick(onClick_)
                    .build();

                ui_.row(id_ + ".content")
                    .size(scaledWidth, scaledHeight)
                    .gap(contentGap)
                    .justifyContent(core::Align::CENTER)
                    .alignItems(core::Align::CENTER)
                    .content([&] {
                        if (hasIcon_) {
                            ui_.text(id_ + ".icon")
                                .size(iconBoxWidth, scaledHeight)
                                .icon(icon_)
                                .fontSize(resolvedIconSize)
                                .lineHeight(resolvedIconSize * 1.18f)
                                .color(iconColor_)
                                .horizontalAlign(core::HorizontalAlign::Center)
                                .verticalAlign(core::VerticalAlign::Center)
                                .build();
                        }

                        ui_.text(id_ + ".text")
                            .size(labelWidth, scaledHeight)
                            .text(text_)
                            .fontSize(resolvedFontSize)
                            .lineHeight(resolvedLineHeight)
                            .color(textColor_)
                            .horizontalAlign(hasIcon_ ? core::HorizontalAlign::Left : core::HorizontalAlign::Center)
                            .verticalAlign(core::VerticalAlign::Center)
                            .build();
                    });
            })
            .build();
    }

private:
    core::dsl::Ui& ui_;
    std::string id_;
    std::string text_ = "Button";
    std::string icon_;
    float width_ = 240.0f;
    float height_ = 70.0f;
    float scaleX_ = 1.0f;
    float scaleY_ = 1.0f;
    float radius_ = 16.0f;
    float fontSize_ = 0.0f;
    float iconSize_ = 0.0f;
    float pressScale_ = 0.965f;
    bool hasFontSize_ = false;
    bool hasIcon_ = false;
    bool hasIconSize_ = false;
    core::Color textColor_ = {0.94f, 0.97f, 1.0f, 1.0f};
    core::Color iconColor_ = {0.94f, 0.97f, 1.0f, 1.0f};
    core::Color normal_ = {0.13f, 0.47f, 0.86f, 1.0f};
    core::Color hover_ = {0.20f, 0.62f, 0.95f, 1.0f};
    core::Color pressed_ = {0.08f, 0.30f, 0.62f, 1.0f};
    core::Border border_ = {1.0f, {0.55f, 0.78f, 1.0f, 0.40f}};
    core::Shadow shadow_ = {true, {0.0f, 4.0f}, 14.0f, 0.0f, {0.0f, 0.0f, 0.0f, 0.22f}};
    std::function<void()> onClick_;
};

inline ButtonBuilder button(core::dsl::Ui& ui, const std::string& id) {
    return ButtonBuilder(ui, id);
}

} // namespace components
