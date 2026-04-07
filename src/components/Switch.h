#pragma once

#include "../EUINEO.h"
#include "../ui/UIBuilder.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <utility>

namespace EUINEO {

class SwitchNode : public UINode {
public:
    class Builder : public UIBuilderBase<SwitchNode, Builder> {
    public:
        Builder(UIContext& context, SwitchNode& node) : UIBuilderBase<SwitchNode, Builder>(context, node) {}

        Builder& checked(bool value) {
            this->node_.trackComposeValue("checked", value);
            this->node_.checked_ = value;
            return *this;
        }

        Builder& label(std::string value) {
            this->node_.trackComposeValue("label", value);
            this->node_.label_ = std::move(value);
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

    explicit SwitchNode(const std::string& key) : UINode(key) {
        resetDefaults();
    }

    static constexpr const char* StaticTypeName() {
        return "SwitchNode";
    }

    const char* typeName() const override {
        return StaticTypeName();
    }

    bool wantsContinuousUpdate() const override {
        return std::abs(knobAnim_ - (checked_ ? 1.0f : 0.0f)) > 0.001f ||
               std::abs(hoverAnim_ - (hovered() ? 1.0f : 0.0f)) > 0.001f;
    }

    RectFrame paintBounds() const override {
        const RectFrame frame = PrimitiveFrame(primitive_);
        RectFrame bounds = frame;
        if (!label_.empty()) {
            const float textScale = fontSize_ / 24.0f;
            const RectFrame textBounds = Renderer::MeasureTextBounds(label_, textScale);
            bounds.width += 10.0f + std::max(0.0f, textBounds.width);
            bounds.height = std::max(bounds.height, std::max(0.0f, textBounds.height));
        }
        return expandPaintBounds(bounds, 3.0f, 3.0f, 3.0f, 3.0f);
    }

    void update() override {
        const bool isHovered = hovered();
        if (primitive_.enabled && State.mouseClicked && isHovered) {
            checked_ = !checked_;
            if (onChange_) {
                onChange_(checked_);
            }
            State.mouseClicked = false;
            requestVisualRepaint(0.16f);
        }

        const float targetKnob = checked_ ? 1.0f : 0.0f;
        if (animateTowards(knobAnim_, targetKnob, State.deltaTime * 16.0f)) {
            requestVisualRepaint(0.16f);
        }
        if (animateTowards(hoverAnim_, isHovered ? 1.0f : 0.0f, State.deltaTime * 12.0f)) {
            requestVisualRepaint(0.16f);
        }
    }

    void draw() override {
        PrimitiveClipScope clip(primitive_);
        const RectFrame frame = PrimitiveFrame(primitive_);
        const float trackHeight = std::max(18.0f, frame.height);
        const float trackY = frame.y + (frame.height - trackHeight) * 0.5f;
        const float radius = trackHeight * 0.5f;

        const Color offColor = Lerp(CurrentTheme->surfaceHover, CurrentTheme->surfaceActive, 0.3f);
        const Color onColor = CurrentTheme->primary;
        Color trackColor = Lerp(offColor, onColor, knobAnim_);
        trackColor = Lerp(trackColor, Color(1.0f, 1.0f, 1.0f, trackColor.a), hoverAnim_ * 0.08f);
        trackColor = ApplyOpacity(trackColor, primitive_.opacity);
        Renderer::DrawRect(frame.x, trackY, frame.width, trackHeight, trackColor, radius);

        const float knobMargin = 3.0f;
        const float knobSize = std::max(0.0f, trackHeight - knobMargin * 2.0f);
        const float knobTravel = std::max(0.0f, frame.width - knobMargin * 2.0f - knobSize);
        const float knobX = frame.x + knobMargin + knobTravel * knobAnim_;
        const float knobY = trackY + knobMargin;
        const Color knobColor = ApplyOpacity(
            Lerp(CurrentTheme->surface, Color(1.0f, 1.0f, 1.0f, 1.0f), 0.75f),
            primitive_.opacity
        );
        Renderer::DrawRect(knobX, knobY, knobSize, knobSize, knobColor, knobSize * 0.5f);

        if (!label_.empty()) {
            const float textScale = fontSize_ / 24.0f;
            const float textX = frame.x + frame.width + 10.0f;
            const float textY = frame.y + frame.height * 0.5f + fontSize_ * 0.25f;
            Renderer::DrawTextStr(label_, textX, textY, ApplyOpacity(CurrentTheme->text, primitive_.opacity), textScale);
        }
    }

protected:
    void resetDefaults() override {
        primitive_ = UIPrimitive{};
        primitive_.width = 46.0f;
        primitive_.height = 24.0f;
        checked_ = false;
        label_.clear();
        fontSize_ = 18.0f;
        onChange_ = {};
    }

private:
    bool checked_ = false;
    std::string label_;
    float fontSize_ = 18.0f;
    std::function<void(bool)> onChange_;
    float knobAnim_ = 0.0f;
    float hoverAnim_ = 0.0f;
};

} // namespace EUINEO
