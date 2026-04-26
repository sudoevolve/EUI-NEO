#pragma once

#include "core/primitive.h"

namespace components {

struct PanelStyle {
    core::Color color = {0.08f, 0.10f, 0.13f, 1.0f};
    core::Gradient gradient;
    core::Border border;
    core::Shadow shadow;
    float radius = 12.0f;
    float opacity = 1.0f;
};

class Panel {
public:
    Panel(float x, float y, float width, float height)
        : primitive_(x, y, width, height) {}

    bool initialize() {
        applyStyle();
        return primitive_.initialize();
    }

    void render(int windowWidth, int windowHeight) {
        primitive_.render(windowWidth, windowHeight);
    }

    void destroy() {
        primitive_.destroy();
    }

    Panel& setFrame(float x, float y, float width, float height) {
        primitive_.setBounds(x, y, width, height);
        return *this;
    }

    Panel& setStyle(const PanelStyle& style) {
        style_ = style;
        applyStyle();
        return *this;
    }

    Panel& setColor(const core::Color& color) {
        style_.color = color;
        primitive_.setColor(color);
        return *this;
    }

    Panel& setRadius(float radius) {
        style_.radius = radius;
        primitive_.setCornerRadius(radius);
        return *this;
    }

    Panel& setOpacity(float opacity) {
        style_.opacity = opacity;
        primitive_.setOpacity(opacity);
        return *this;
    }

    Panel& setGradient(const core::Gradient& gradient) {
        style_.gradient = gradient;
        primitive_.setGradient(gradient);
        return *this;
    }

    Panel& setBorder(const core::Border& border) {
        style_.border = border;
        primitive_.setBorder(border);
        return *this;
    }

    Panel& setShadow(const core::Shadow& shadow) {
        style_.shadow = shadow;
        primitive_.setShadow(shadow);
        return *this;
    }

    Panel& setTranslate(float x, float y) {
        primitive_.setTranslate(x, y);
        return *this;
    }

    Panel& setScale(float x, float y) {
        primitive_.setScale(x, y);
        return *this;
    }

    Panel& setRotate(float radians) {
        primitive_.setRotate(radians);
        return *this;
    }

    Panel& setTransformOrigin(float x, float y) {
        primitive_.setTransformOrigin(x, y);
        return *this;
    }

private:
    void applyStyle() {
        primitive_.setColor(style_.color);
        primitive_.setGradient(style_.gradient);
        primitive_.setBorder(style_.border);
        primitive_.setShadow(style_.shadow);
        primitive_.setCornerRadius(style_.radius);
        primitive_.setOpacity(style_.opacity);
    }

    PanelStyle style_;
    core::RoundedRectPrimitive primitive_;
};

} // namespace components
