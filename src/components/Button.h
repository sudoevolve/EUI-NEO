#pragma once

#include <algorithm>
#include "../EUINEO.h"
#include "../ui/UIBuilder.h"
#include <cmath>
#include <functional>
#include <string>
#include <utility>

namespace EUINEO {

enum class ButtonStyle { Default, Primary, Outline };
enum class ButtonIconPlacement { Leading, Trailing };

class ButtonNode : public UINode {
public:
    class Builder : public UIBuilderBase<ButtonNode, Builder> {
    public:
        Builder(UIContext& context, ButtonNode& node) : UIBuilderBase<ButtonNode, Builder>(context, node) {}

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

        Builder& icon(std::string value) {
            this->node_.trackComposeValue("icon", value);
            this->node_.icon_ = std::move(value);
            return *this;
        }

        Builder& iconPlacement(ButtonIconPlacement value) {
            this->node_.trackComposeValue("iconPlacement", value);
            this->node_.iconPlacement_ = value;
            return *this;
        }

        Builder& textColor(const Color& value) {
            this->node_.trackComposeValue("hasTextColorOverride", true);
            this->node_.trackComposeValue("textColorOverride", value);
            this->node_.hasTextColorOverride_ = true;
            this->node_.textColorOverride_ = value;
            return *this;
        }

        Builder& style(ButtonStyle value) {
            this->node_.trackComposeValue("style", value);
            this->node_.style_ = value;
            return *this;
        }

        Builder& hoverScale(float idle, float hover, float duration) {
            this->node_.trackComposeValue("hoverScaleIdle", idle);
            this->node_.trackComposeValue("hoverScaleHover", hover);
            this->node_.trackComposeValue("hoverScaleDuration", duration);
            this->node_.hoverScaleIdle_ = idle;
            this->node_.hoverScaleHover_ = hover;
            this->node_.hoverScaleDuration_ = std::max(0.0f, duration);
            return *this;
        }

        Builder& onClick(std::function<void()> handler) {
            this->node_.onClick_ = std::move(handler);
            return *this;
        }
    };

    explicit ButtonNode(const std::string& key) : UINode(key) {
        resetDefaults();
    }

    static constexpr const char* StaticTypeName() {
        return "ButtonNode";
    }

    const char* typeName() const override {
        return StaticTypeName();
    }

    bool wantsContinuousUpdate() const override {
        return hoverAnim_ > 0.001f && hoverAnim_ < 0.999f ||
               clickAnim_ > 0.001f && clickAnim_ < 0.999f;
    }

    void update() override {
        const bool hovered = primitive_.enabled && PrimitiveContains(primitive_, State.mouseX, State.mouseY);
        const float hoverSpeed = hoverScaleDuration_ > 0.0f
            ? std::clamp(State.deltaTime / hoverScaleDuration_, 0.0f, 1.0f)
            : 1.0f;
        const float clickSpeed = 25.0f * State.deltaTime;

        const float targetHover = hovered ? 1.0f : 0.0f;
        if (std::abs(hoverAnim_ - targetHover) > 0.001f) {
            hoverAnim_ = Lerp(hoverAnim_, targetHover, hoverSpeed);
            if (std::abs(hoverAnim_ - targetHover) < 0.001f) {
                hoverAnim_ = targetHover;
            }
            requestRepaint(6.0f);
        }

        const float targetClick = (hovered && State.mouseDown) ? 1.0f : 0.0f;
        if (std::abs(clickAnim_ - targetClick) > 0.001f) {
            clickAnim_ = Lerp(clickAnim_, targetClick, clickSpeed);
            if (std::abs(clickAnim_ - targetClick) < 0.001f) {
                clickAnim_ = targetClick;
            }
            requestRepaint(6.0f);
        }

        if (hovered && State.mouseClicked && onClick_) {
            onClick_();
            requestRepaint(6.0f, 0.2f);
        }
    }

    void draw() override {
        PrimitiveClipScope clip(primitive_);
        const RectFrame frame = PrimitiveFrame(primitive_);
        const float hoverScale = Lerp(hoverScaleIdle_, hoverScaleHover_, hoverAnim_);
        const float pressScale = 1.0f - clickAnim_ * 0.035f;
        const float animScale = std::max(0.9f, hoverScale * pressScale);
        const float scaledWidth = frame.width * animScale;
        const float scaledHeight = frame.height * animScale;
        const RectFrame drawFrame{
            frame.x + (frame.width - scaledWidth) * 0.5f,
            frame.y + (frame.height - scaledHeight) * 0.5f,
            scaledWidth,
            scaledHeight
        };
        const float cornerRadius = primitive_.rounding > 0.0f ? primitive_.rounding : 6.0f;

        const Color baseColor = style_ == ButtonStyle::Primary ? CurrentTheme->primary : CurrentTheme->surface;
        const Color hoverColor = style_ == ButtonStyle::Primary
            ? Lerp(CurrentTheme->primary, Color(1.0f, 1.0f, 1.0f, 1.0f), 0.2f)
            : CurrentTheme->surfaceHover;
        const Color activeColor = style_ == ButtonStyle::Primary
            ? Lerp(CurrentTheme->primary, Color(0.0f, 0.0f, 0.0f, 1.0f), 0.2f)
            : CurrentTheme->surfaceActive;

        Color fillColor = Lerp(baseColor, hoverColor, hoverAnim_);
        fillColor = Lerp(fillColor, activeColor, clickAnim_);
        fillColor = ApplyOpacity(fillColor, primitive_.opacity);

        if (style_ == ButtonStyle::Outline) {
            RectStyle borderStyle = MakeStyle(primitive_);
            borderStyle.color = ApplyOpacity(
                primitive_.borderColor.a > 0.0f ? primitive_.borderColor : CurrentTheme->border,
                primitive_.opacity
            );
            borderStyle.gradient = RectGradient{};
            borderStyle.rounding = cornerRadius * animScale;
            Renderer::DrawRect(drawFrame.x, drawFrame.y, drawFrame.width, drawFrame.height, borderStyle);

            RectStyle innerStyle = MakeStyle(primitive_);
            innerStyle.color = ApplyOpacity(
                primitive_.background.a > 0.0f ? primitive_.background : CurrentTheme->background,
                primitive_.opacity
            );
            innerStyle.rounding = std::max(0.0f, cornerRadius * animScale - 1.0f);
            Renderer::DrawRect(drawFrame.x + 1.0f, drawFrame.y + 1.0f,
                               std::max(0.0f, drawFrame.width - 2.0f), std::max(0.0f, drawFrame.height - 2.0f), innerStyle);
        } else {
            RectStyle style = MakeStyle(primitive_);
            style.color = fillColor;
            style.rounding = cornerRadius * animScale;
            Renderer::DrawRect(drawFrame.x, drawFrame.y, drawFrame.width, drawFrame.height, style);
        }

        const float textScale = (fontSize_ / 24.0f) * animScale;
        const Color baseTextColor = hasTextColorOverride_
            ? textColorOverride_
            : (style_ == ButtonStyle::Primary ? Color(1.0f, 1.0f, 1.0f, 1.0f) : CurrentTheme->text);
        const Color textColor = ApplyOpacity(baseTextColor, primitive_.opacity);
        const float textY = drawFrame.y + drawFrame.height * 0.5f + (fontSize_ * animScale / 4.0f);

        if (icon_.empty()) {
            const float textWidth = Renderer::MeasureTextWidth(text_, textScale);
            const float textX = drawFrame.x + (drawFrame.width - textWidth) * 0.5f;
            Renderer::DrawTextStr(text_, textX, textY, textColor, textScale);
            return;
        }

        const float iconScale = textScale;
        const RectFrame iconBounds = Renderer::MeasureTextBounds(icon_, iconScale);
        const float iconBoxWidth = std::max(iconBounds.width, fontSize_ * 0.8f);
        const float textWidth = Renderer::MeasureTextWidth(text_, textScale);
        const float contentGap = text_.empty() ? 0.0f : std::max(8.0f * animScale, fontSize_ * animScale * 0.5f);
        const float groupWidth = iconBoxWidth + contentGap + textWidth;
        const float groupX = drawFrame.x + (drawFrame.width - groupWidth) * 0.5f;
        const float iconY = drawFrame.y + (drawFrame.height - std::max(iconBounds.height, fontSize_ * animScale * 0.8f)) * 0.5f - iconBounds.y;

        if (iconPlacement_ == ButtonIconPlacement::Leading) {
            const float iconX = groupX + (iconBoxWidth - iconBounds.width) * 0.5f - iconBounds.x;
            Renderer::DrawTextStr(icon_, iconX, iconY, textColor, iconScale);
            Renderer::DrawTextStr(text_, groupX + iconBoxWidth + contentGap, textY, textColor, textScale);
            return;
        }

        Renderer::DrawTextStr(text_, groupX, textY, textColor, textScale);
        Renderer::DrawTextStr(icon_,
                              groupX + textWidth + contentGap + (iconBoxWidth - iconBounds.width) * 0.5f - iconBounds.x,
                              iconY, textColor, iconScale);
    }

protected:
    void resetDefaults() override {
        primitive_ = UIPrimitive{};
        primitive_.width = 120.0f;
        primitive_.height = 40.0f;
        text_.clear();
        icon_.clear();
        fontSize_ = 20.0f;
        hasTextColorOverride_ = false;
        textColorOverride_ = Color(0.0f, 0.0f, 0.0f, 0.0f);
        style_ = ButtonStyle::Default;
        iconPlacement_ = ButtonIconPlacement::Leading;
        hoverScaleIdle_ = 1.0f;
        hoverScaleHover_ = 1.0f;
        hoverScaleDuration_ = 0.16f;
        onClick_ = {};
    }

private:
    void requestRepaint(float expand = 6.0f, float duration = 0.0f) {
        (void)expand;
        (void)duration;
        requestVisualRepaint();
    }

    std::string text_;
    std::string icon_;
    float fontSize_ = 20.0f;
    bool hasTextColorOverride_ = false;
    Color textColorOverride_ = Color(0.0f, 0.0f, 0.0f, 0.0f);
    ButtonStyle style_ = ButtonStyle::Default;
    ButtonIconPlacement iconPlacement_ = ButtonIconPlacement::Leading;
    float hoverScaleIdle_ = 1.0f;
    float hoverScaleHover_ = 1.0f;
    float hoverScaleDuration_ = 0.16f;
    std::function<void()> onClick_;
    float hoverAnim_ = 0.0f;
    float clickAnim_ = 0.0f;
};

} // namespace EUINEO
