#pragma once

#include "../EUINEO.h"
#include "../ui/UIBuilder.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <utility>

namespace EUINEO {

class RadioNode : public UINode {
public:
    class Builder : public UIBuilderBase<RadioNode, Builder> {
    public:
        Builder(UIContext& context, RadioNode& node) : UIBuilderBase<RadioNode, Builder>(context, node) {}

        Builder& selected(bool value) {
            this->node_.trackComposeValue("selected", value);
            this->node_.selected_ = value;
            return *this;
        }

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

        Builder& onChange(std::function<void(bool)> handler) {
            this->node_.onChange_ = std::move(handler);
            return *this;
        }
    };

    explicit RadioNode(const std::string& key) : UINode(key) {
        resetDefaults();
    }

    static constexpr const char* StaticTypeName() {
        return "RadioNode";
    }

    const char* typeName() const override {
        return StaticTypeName();
    }

    bool wantsContinuousUpdate() const override {
        const float targetSelected = selected_ ? 1.0f : 0.0f;
        const float targetHover = hovered() ? 1.0f : 0.0f;
        return std::abs(selectAnim_ - targetSelected) > 0.001f ||
               std::abs(hoverAnim_ - targetHover) > 0.001f;
    }

    RectFrame paintBounds() const override {
        const RectFrame frame = PrimitiveFrame(primitive_);
        RectFrame bounds = frame;
        if (!text_.empty()) {
            const RectFrame textBounds = Renderer::MeasureTextBounds(text_, fontSize_ / 24.0f);
            bounds.width += 10.0f + std::max(0.0f, textBounds.width);
            bounds.height = std::max(bounds.height, std::max(0.0f, textBounds.height));
        }
        return expandPaintBounds(bounds, 3.0f, 3.0f, 3.0f, 3.0f);
    }

    void update() override {
        const bool isHovered = hovered();
        if (primitive_.enabled && State.mouseClicked && isHovered) {
            if (!selected_) {
                selected_ = true;
                if (onChange_) {
                    onChange_(true);
                }
            } else if (onChange_) {
                onChange_(true);
            }
            State.mouseClicked = false;
            requestVisualRepaint(0.16f);
        }

        if (animateTowards(selectAnim_, selected_ ? 1.0f : 0.0f, State.deltaTime * 18.0f)) {
            requestVisualRepaint(0.16f);
        }
        if (animateTowards(hoverAnim_, isHovered ? 1.0f : 0.0f, State.deltaTime * 12.0f)) {
            requestVisualRepaint(0.16f);
        }
    }

    void draw() override {
        PrimitiveClipScope clip(primitive_);
        const RectFrame frame = PrimitiveFrame(primitive_);
        const float diameter = std::min(frame.width, frame.height);
        const float y = frame.y + (frame.height - diameter) * 0.5f;
        const float radius = diameter * 0.5f;

        const Color ringColor = ApplyOpacity(Lerp(CurrentTheme->border, CurrentTheme->primary, std::max(selectAnim_, hoverAnim_)), primitive_.opacity);
        Renderer::DrawRect(frame.x, y, diameter, diameter, ringColor, radius);
        Renderer::DrawRect(frame.x + 2.0f, y + 2.0f, std::max(0.0f, diameter - 4.0f), std::max(0.0f, diameter - 4.0f),
                           ApplyOpacity(CurrentTheme->background, primitive_.opacity), std::max(0.0f, radius - 2.0f));

        const float innerSize = std::max(0.0f, (diameter - 10.0f) * selectAnim_);
        if (innerSize > 0.0f) {
            const float innerX = frame.x + (diameter - innerSize) * 0.5f;
            const float innerY = y + (diameter - innerSize) * 0.5f;
            Renderer::DrawRect(innerX, innerY, innerSize, innerSize, ApplyOpacity(CurrentTheme->primary, primitive_.opacity), innerSize * 0.5f);
        }

        if (!text_.empty()) {
            const float textScale = fontSize_ / 24.0f;
            const float textX = frame.x + diameter + 10.0f;
            const float textY = frame.y + frame.height * 0.5f + fontSize_ * 0.25f;
            Renderer::DrawTextStr(text_, textX, textY, ApplyOpacity(CurrentTheme->text, primitive_.opacity), textScale);
        }
    }

protected:
    void resetDefaults() override {
        primitive_ = UIPrimitive{};
        primitive_.width = 22.0f;
        primitive_.height = 22.0f;
        selected_ = false;
        text_.clear();
        fontSize_ = 18.0f;
        onChange_ = {};
    }

private:
    bool selected_ = false;
    std::string text_;
    float fontSize_ = 18.0f;
    std::function<void(bool)> onChange_;
    float selectAnim_ = 0.0f;
    float hoverAnim_ = 0.0f;
};

} // namespace EUINEO
