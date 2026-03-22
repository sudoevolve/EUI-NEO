#pragma once

#include "../EUINEO.h"
#include "../ui/UIBuilder.h"
#include <algorithm>
#include <string>
#include <utility>

namespace EUINEO {

class LabelNode : public UINode {
public:
    class Builder : public UIBuilderBase<LabelNode, Builder> {
    public:
        Builder(UIContext& context, LabelNode& node) : UIBuilderBase<LabelNode, Builder>(context, node) {}

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

        Builder& color(const Color& value) {
            this->node_.trackComposeValue("color", value);
            this->node_.color_ = value;
            this->node_.useThemeColor_ = false;
            return *this;
        }
    };

    explicit LabelNode(const std::string& key) : UINode(key) {
        resetDefaults();
    }

    static constexpr const char* StaticTypeName() {
        return "LabelNode";
    }

    const char* typeName() const override {
        return StaticTypeName();
    }

    void update() override {}

    RectFrame paintBounds() const override {
        const RectFrame frame = PrimitiveFrame(primitive_);
        const float textScale = fontSize_ / 24.0f;
        const float textWidth = std::max(Renderer::MeasureTextWidth(text_, textScale), 1.0f);
        const float textHeight = std::max(32.0f * textScale * 1.35f, 1.0f);
        RectFrame bounds{
            frame.x + primitive_.translateX,
            frame.y + primitive_.translateY - textHeight,
            textWidth,
            textHeight
        };
        if (primitive_.hasClipRect) {
            const float x1 = std::max(bounds.x, primitive_.clipRect.x);
            const float y1 = std::max(bounds.y, primitive_.clipRect.y);
            const float x2 = std::min(bounds.x + bounds.width, primitive_.clipRect.x + primitive_.clipRect.width);
            const float y2 = std::min(bounds.y + bounds.height, primitive_.clipRect.y + primitive_.clipRect.height);
            bounds.x = x1;
            bounds.y = y1;
            bounds.width = std::max(0.0f, x2 - x1);
            bounds.height = std::max(0.0f, y2 - y1);
        }
        return bounds;
    }

    void draw() override {
        PrimitiveClipScope clip(primitive_);
        const RectFrame frame = PrimitiveFrame(primitive_);
        const float textScale = fontSize_ / 24.0f;
        const Color drawColor = useThemeColor_
            ? ApplyOpacity(CurrentTheme->text, primitive_.opacity)
            : ApplyOpacity(color_, primitive_.opacity);

        Renderer::DrawTextStr(
            text_,
            frame.x + primitive_.translateX,
            frame.y + primitive_.translateY,
            drawColor,
            textScale,
            primitive_.rotation
        );
    }

protected:
    void resetDefaults() override {
        primitive_ = UIPrimitive{};
        text_.clear();
        fontSize_ = 24.0f;
        color_ = CurrentTheme->text;
        useThemeColor_ = true;
    }

private:
    std::string text_;
    float fontSize_ = 24.0f;
    Color color_ = Color(1.0f, 1.0f, 1.0f, 1.0f);
    bool useThemeColor_ = true;
};

} // namespace EUINEO
