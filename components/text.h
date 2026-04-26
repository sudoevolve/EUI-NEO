#pragma once

#include "core/text.h"

#include <string>

namespace components {

using TextStyle = core::TextStyle;

class Text {
public:
    Text(float x, float y, const std::string& value = "")
        : primitive_(x, y) {
        style_.text = value;
    }

    bool initialize() {
        primitive_.setStyle(style_);
        return primitive_.initialize();
    }

    void render(int windowWidth, int windowHeight) {
        primitive_.render(windowWidth, windowHeight);
    }

    void destroy() {
        primitive_.destroy();
    }

    Text& setPosition(float x, float y) {
        primitive_.setPosition(x, y);
        return *this;
    }

    Text& setStyle(const TextStyle& style) {
        style_ = style;
        primitive_.setStyle(style);
        return *this;
    }

    Text& setText(const std::string& value) {
        style_.text = value;
        primitive_.setText(value);
        return *this;
    }

    Text& setFontFamily(const std::string& fontFamily) {
        style_.fontFamily = fontFamily;
        primitive_.setFontFamily(fontFamily);
        return *this;
    }

    Text& setFontSize(float fontSize) {
        style_.fontSize = fontSize;
        primitive_.setFontSize(fontSize);
        return *this;
    }

    Text& setFontWeight(int fontWeight) {
        style_.fontWeight = fontWeight;
        primitive_.setFontWeight(fontWeight);
        return *this;
    }

    Text& setColor(const core::Color& color) {
        style_.color = color;
        primitive_.setColor(color);
        return *this;
    }

    Text& setMaxWidth(float maxWidth) {
        style_.maxWidth = maxWidth;
        primitive_.setMaxWidth(maxWidth);
        return *this;
    }

    Text& setWrap(bool wrap) {
        style_.wrap = wrap;
        primitive_.setWrap(wrap);
        return *this;
    }

    Text& setHorizontalAlign(core::HorizontalAlign align) {
        style_.horizontalAlign = align;
        primitive_.setHorizontalAlign(align);
        return *this;
    }

    Text& setVerticalAlign(core::VerticalAlign align) {
        style_.verticalAlign = align;
        primitive_.setVerticalAlign(align);
        return *this;
    }

    Text& setLineHeight(float lineHeight) {
        style_.lineHeight = lineHeight;
        primitive_.setLineHeight(lineHeight);
        return *this;
    }

    core::Vec2 measuredSize() {
        return primitive_.measuredSize();
    }

private:
    TextStyle style_;
    core::TextPrimitive primitive_;
};

} // namespace components
