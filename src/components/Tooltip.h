#pragma once

#include "../EUINEO.h"
#include "../ui/ThemeTokens.h"
#include "../ui/UIBuilder.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace EUINEO {

class TooltipNode : public UINode {
public:
    class Builder : public UIBuilderBase<TooltipNode, Builder> {
    public:
        Builder(UIContext& context, TooltipNode& node) : UIBuilderBase<TooltipNode, Builder>(context, node) {}

        Builder& text(std::string value) {
            this->node_.trackComposeValue("text", value);
            this->node_.text_ = std::move(value);
            return *this;
        }

        Builder& fontSize(float value) {
            this->node_.trackComposeValue("fontSize", value);
            this->node_.fontSize_ = value;
            return *this;
        }

        Builder& padding(float value) {
            this->node_.trackComposeValue("padding", value);
            this->node_.padding_ = std::max(4.0f, value);
            return *this;
        }

        Builder& offset(float x, float y) {
            this->node_.trackComposeValue("offsetX", x);
            this->node_.trackComposeValue("offsetY", y);
            this->node_.offsetX_ = x;
            this->node_.offsetY_ = y;
            return *this;
        }

        Builder& followMouse(bool value) {
            this->node_.trackComposeValue("followMouse", value);
            this->node_.followMouse_ = value;
            return *this;
        }

        Builder& triggerOnHover(bool value) {
            this->node_.trackComposeValue("triggerOnHover", value);
            this->node_.triggerOnHover_ = value;
            return *this;
        }

        Builder& show(bool value) {
            this->node_.trackComposeValue("show", value);
            this->node_.manualShow_ = value;
            return *this;
        }
    };

    explicit TooltipNode(const std::string& key) : UINode(key) {
        resetDefaults();
    }

    static constexpr const char* StaticTypeName() {
        return "TooltipNode";
    }

    const char* typeName() const override {
        return StaticTypeName();
    }

    bool usesCachedSurface() const override {
        return false;
    }

    bool wantsContinuousUpdate() const override {
        const bool hoverVisible = triggerOnHover_ && primitive_.enabled && containsPoint(State.mouseX, State.mouseY);
        const float target = (!text_.empty() && (manualShow_ || hoverVisible)) ? 1.0f : 0.0f;
        return std::abs(alphaAnim_ - target) > 0.001f ||
               (followMouse_ && alphaAnim_ > 0.001f && State.pointerMoved);
    }

    RectFrame paintBounds() const override {
        if (alphaAnim_ <= 0.001f && !manualShow_) {
            return RectFrame{};
        }
        return bubbleFrame();
    }

    void update() override {
        const bool hoverVisible = triggerOnHover_ && primitive_.enabled && containsPoint(State.mouseX, State.mouseY);
        const float target = (!text_.empty() && (manualShow_ || hoverVisible)) ? 1.0f : 0.0f;
        if (followMouse_ && alphaAnim_ > 0.001f && State.pointerMoved) {
            requestVisualRepaint(0.0f);
        }
        if (animateTowards(alphaAnim_, target, State.deltaTime * 16.0f)) {
            requestVisualRepaint(0.16f);
        }
    }

    void draw() override {
        if (alphaAnim_ <= 0.001f || text_.empty()) {
            return;
        }

        const RectFrame frame = bubbleFrame();
        UIPrimitive popupPrimitive = primitive_;
        popupPrimitive.opacity = primitive_.opacity * alphaAnim_;
        RectStyle style = MakePopupChromeStyle(popupPrimitive, 8.0f);
        style.color = ApplyOpacity(style.color, popupPrimitive.opacity);
        style.shadowColor = ApplyOpacity(style.shadowColor, popupPrimitive.opacity);
        Renderer::DrawRect(frame.x, frame.y, frame.width, frame.height, style);

        const float textScale = fontSize_ / 24.0f;
        const RectFrame textBounds = Renderer::MeasureTextBounds(text_, textScale);
        const float textX = frame.x + (frame.width - textBounds.width) * 0.5f - textBounds.x;
        const float textY = frame.y + (frame.height - textBounds.height) * 0.5f - textBounds.y;
        Renderer::DrawTextStr(text_, textX, textY, ApplyOpacity(CurrentTheme->text, popupPrimitive.opacity), textScale);
    }

protected:
    void resetDefaults() override {
        primitive_ = UIPrimitive{};
        primitive_.width = 180.0f;
        primitive_.height = 24.0f;
        primitive_.renderLayer = RenderLayer::Popup;
        primitive_.clipToParent = false;
        primitive_.zIndex = 280;
        primitive_.hasExplicitZIndex = true;
        text_.clear();
        fontSize_ = 16.0f;
        padding_ = 8.0f;
        offsetX_ = 12.0f;
        offsetY_ = 14.0f;
        followMouse_ = true;
        triggerOnHover_ = true;
        manualShow_ = false;
    }

private:
    RectFrame bubbleFrame() const {
        const float textScale = fontSize_ / 24.0f;
        const RectFrame textBounds = Renderer::MeasureTextBounds(text_, textScale);
        const float width = std::max(16.0f, textBounds.width + padding_ * 2.0f);
        const float height = std::max(16.0f, textBounds.height + padding_ * 2.0f);

        const RectFrame anchor = PrimitiveFrame(primitive_);
        float x = followMouse_ ? State.mouseX + offsetX_ : anchor.x + offsetX_;
        float y = followMouse_ ? State.mouseY + offsetY_ : anchor.y + anchor.height + offsetY_;

        const float minX = 6.0f;
        const float minY = 6.0f;
        const float maxX = std::max(minX, State.screenW - width - 6.0f);
        const float maxY = std::max(minY, State.screenH - height - 6.0f);
        x = std::clamp(x, minX, maxX);
        y = std::clamp(y, minY, maxY);
        return RectFrame{x, y, width, height};
    }

    std::string text_;
    float fontSize_ = 16.0f;
    float padding_ = 8.0f;
    float offsetX_ = 12.0f;
    float offsetY_ = 14.0f;
    bool followMouse_ = true;
    bool triggerOnHover_ = true;
    bool manualShow_ = false;
    float alphaAnim_ = 0.0f;
};

} // namespace EUINEO
