#pragma once

#include "../EUINEO.h"
#include "../ui/ThemeTokens.h"
#include "../ui/UIBuilder.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <utility>

namespace EUINEO {

class DialogNode : public UINode {
public:
    class Builder : public UIBuilderBase<DialogNode, Builder> {
    public:
        Builder(UIContext& context, DialogNode& node) : UIBuilderBase<DialogNode, Builder>(context, node) {}

        Builder& open(bool value) {
            this->node_.trackComposeValue("open", value);
            this->node_.open_ = value;
            return *this;
        }

        Builder& title(std::string value) {
            this->node_.trackComposeValue("title", value);
            this->node_.title_ = std::move(value);
            return *this;
        }

        Builder& message(std::string value) {
            this->node_.trackComposeValue("message", value);
            this->node_.message_ = std::move(value);
            return *this;
        }

        Builder& confirmText(std::string value) {
            this->node_.trackComposeValue("confirmText", value);
            this->node_.confirmText_ = std::move(value);
            return *this;
        }

        Builder& cancelText(std::string value) {
            this->node_.trackComposeValue("cancelText", value);
            this->node_.cancelText_ = std::move(value);
            return *this;
        }

        Builder& showCancel(bool value) {
            this->node_.trackComposeValue("showCancel", value);
            this->node_.showCancel_ = value;
            return *this;
        }

        Builder& closeOnBackdrop(bool value) {
            this->node_.trackComposeValue("closeOnBackdrop", value);
            this->node_.closeOnBackdrop_ = value;
            return *this;
        }

        Builder& onConfirm(std::function<void()> handler) {
            this->node_.onConfirm_ = std::move(handler);
            return *this;
        }

        Builder& onCancel(std::function<void()> handler) {
            this->node_.onCancel_ = std::move(handler);
            return *this;
        }

        Builder& onClose(std::function<void()> handler) {
            this->node_.onClose_ = std::move(handler);
            return *this;
        }
    };

    explicit DialogNode(const std::string& key) : UINode(key) {
        resetDefaults();
    }

    static constexpr const char* StaticTypeName() {
        return "DialogNode";
    }

    const char* typeName() const override {
        return StaticTypeName();
    }

    bool usesCachedSurface() const override {
        return false;
    }

    bool blocksUnderlyingInput() const override {
        return open_ || visibilityAnim_ > 0.001f;
    }

    bool wantsContinuousUpdate() const override {
        const float target = open_ ? 1.0f : 0.0f;
        return std::abs(visibilityAnim_ - target) > 0.001f ||
               (visibilityAnim_ > 0.001f &&
                (std::abs(confirmHoverAnim_ - confirmHoverTarget_) > 0.001f ||
                 std::abs(cancelHoverAnim_ - cancelHoverTarget_) > 0.001f));
    }

    RectFrame paintBounds() const override {
        if (visibilityAnim_ <= 0.001f && !open_) {
            return RectFrame{};
        }
        return RectFrame{0.0f, 0.0f, State.screenW, State.screenH};
    }

    void update() override {
        const float target = open_ ? 1.0f : 0.0f;
        if (animateTowards(visibilityAnim_, target, State.deltaTime * 14.0f)) {
            requestVisualRepaint(0.18f);
        }

        if (visibilityAnim_ <= 0.001f) {
            confirmHoverTarget_ = 0.0f;
            cancelHoverTarget_ = 0.0f;
            return;
        }

        const DialogLayout layout = makeLayout();
        const bool inConfirm = contains(layout.confirmButton, State.mouseX, State.mouseY);
        const bool inCancel = showCancel_ && contains(layout.cancelButton, State.mouseX, State.mouseY);
        confirmHoverTarget_ = inConfirm ? 1.0f : 0.0f;
        cancelHoverTarget_ = inCancel ? 1.0f : 0.0f;
        if (animateTowards(confirmHoverAnim_, confirmHoverTarget_, State.deltaTime * 18.0f) ||
            animateTowards(cancelHoverAnim_, cancelHoverTarget_, State.deltaTime * 18.0f)) {
            requestVisualRepaint(0.16f);
        }

        if (primitive_.enabled && State.mouseClicked) {
            const bool inPanel = contains(layout.panel, State.mouseX, State.mouseY);
            if (inConfirm) {
                if (onConfirm_) {
                    onConfirm_();
                }
                if (onClose_) {
                    onClose_();
                }
                State.mouseClicked = false;
                requestVisualRepaint(0.18f);
                return;
            }
            if (inCancel) {
                if (onCancel_) {
                    onCancel_();
                }
                if (onClose_) {
                    onClose_();
                }
                State.mouseClicked = false;
                requestVisualRepaint(0.18f);
                return;
            }
            if (!inPanel && closeOnBackdrop_) {
                if (onClose_) {
                    onClose_();
                }
                State.mouseClicked = false;
                requestVisualRepaint(0.18f);
            }
        }
    }

    void draw() override {
        if (visibilityAnim_ <= 0.001f) {
            return;
        }

        const DialogLayout layout = makeLayout();
        const float opacity = std::clamp(visibilityAnim_, 0.0f, 1.0f) * primitive_.opacity;

        Renderer::DrawRect(
            layout.overlay.x,
            layout.overlay.y,
            layout.overlay.width,
            layout.overlay.height,
            ApplyOpacity(Color(0.0f, 0.0f, 0.0f, 0.44f), opacity),
            0.0f
        );

        UIPrimitive panelPrimitive = primitive_;
        panelPrimitive.opacity = opacity;
        RectStyle panelStyle = MakePopupChromeStyle(panelPrimitive, primitive_.rounding > 0.0f ? primitive_.rounding : 14.0f);
        panelStyle.color = ApplyOpacity(panelStyle.color, opacity);
        panelStyle.shadowColor = ApplyOpacity(panelStyle.shadowColor, opacity);
        Renderer::DrawRect(layout.panel.x, layout.panel.y, layout.panel.width, layout.panel.height, panelStyle);

        const float titleScale = titleFontSize_ / 24.0f;
        const float bodyScale = messageFontSize_ / 24.0f;
        const float contentX = layout.panel.x + 20.0f;
        float textY = layout.panel.y + 34.0f;

        if (!title_.empty()) {
            Renderer::DrawTextStr(title_, contentX, textY, ApplyOpacity(CurrentTheme->text, opacity), titleScale);
            textY += 30.0f;
        }

        const Color bodyColor = ApplyOpacity(Color(CurrentTheme->text.r, CurrentTheme->text.g, CurrentTheme->text.b, 0.78f), opacity);
        std::size_t start = 0;
        while (start <= message_.size()) {
            const std::size_t end = message_.find('\n', start);
            const std::string line = end == std::string::npos ? message_.substr(start) : message_.substr(start, end - start);
            Renderer::DrawTextStr(line, contentX, textY, bodyColor, bodyScale);
            textY += messageFontSize_ + 8.0f;
            if (end == std::string::npos) {
                break;
            }
            start = end + 1;
        }

        drawButton(layout.confirmButton, confirmText_, true, confirmHoverAnim_, opacity);
        if (showCancel_) {
            drawButton(layout.cancelButton, cancelText_, false, cancelHoverAnim_, opacity);
        }
    }

protected:
    void resetDefaults() override {
        primitive_ = UIPrimitive{};
        primitive_.width = 420.0f;
        primitive_.height = 220.0f;
        primitive_.rounding = 14.0f;
        primitive_.renderLayer = RenderLayer::Popup;
        primitive_.clipToParent = false;
        primitive_.zIndex = 260;
        primitive_.hasExplicitZIndex = true;
        open_ = false;
        title_ = "Confirm";
        message_ = "Please confirm this action.";
        confirmText_ = "Confirm";
        cancelText_ = "Cancel";
        showCancel_ = true;
        closeOnBackdrop_ = true;
        onConfirm_ = {};
        onCancel_ = {};
        onClose_ = {};
    }

private:
    struct DialogLayout {
        RectFrame overlay;
        RectFrame panel;
        RectFrame confirmButton;
        RectFrame cancelButton;
    };

    static bool contains(const RectFrame& frame, float x, float y) {
        return x >= frame.x && x <= frame.x + frame.width &&
               y >= frame.y && y <= frame.y + frame.height;
    }

    DialogLayout makeLayout() const {
        DialogLayout layout;
        layout.overlay = RectFrame{0.0f, 0.0f, State.screenW, State.screenH};
        const float width = std::max(260.0f, std::min(primitive_.width, State.screenW - 24.0f));
        const float height = std::max(160.0f, std::min(primitive_.height, State.screenH - 24.0f));
        layout.panel = RectFrame{
            std::max(12.0f, (State.screenW - width) * 0.5f),
            std::max(12.0f, (State.screenH - height) * 0.5f),
            width,
            height
        };

        const float buttonHeight = 34.0f;
        const float buttonWidth = showCancel_
            ? (width - 20.0f * 2.0f - 10.0f) * 0.5f
            : std::max(120.0f, width - 20.0f * 2.0f);
        const float buttonY = layout.panel.y + height - 18.0f - buttonHeight;
        layout.confirmButton = RectFrame{
            showCancel_ ? layout.panel.x + 20.0f + buttonWidth + 10.0f : layout.panel.x + width - 20.0f - buttonWidth,
            buttonY,
            buttonWidth,
            buttonHeight
        };
        layout.cancelButton = RectFrame{
            layout.panel.x + 20.0f,
            buttonY,
            buttonWidth,
            buttonHeight
        };
        return layout;
    }

    void drawButton(const RectFrame& frame, const std::string& text, bool primary,
                    float hoverAmount, float opacity) const {
        Color bg = primary ? CurrentTheme->primary : CurrentTheme->surfaceHover;
        if (primary) {
            bg = Lerp(bg, Color(1.0f, 1.0f, 1.0f, 1.0f), hoverAmount * 0.12f);
        } else {
            bg = Lerp(bg, CurrentTheme->surfaceActive, hoverAmount * 0.8f);
        }
        Renderer::DrawRect(frame.x, frame.y, frame.width, frame.height, ApplyOpacity(bg, opacity), 9.0f);

        const float scale = 18.0f / 24.0f;
        const float textWidth = Renderer::MeasureTextWidth(text, scale);
        const float textX = frame.x + (frame.width - textWidth) * 0.5f;
        const float textY = frame.y + frame.height * 0.5f + 18.0f * 0.25f;
        const Color textColor = primary
            ? Color(1.0f, 1.0f, 1.0f, 1.0f)
            : CurrentTheme->text;
        Renderer::DrawTextStr(text, textX, textY, ApplyOpacity(textColor, opacity), scale);
    }

    bool open_ = false;
    std::string title_;
    std::string message_;
    std::string confirmText_;
    std::string cancelText_;
    bool showCancel_ = true;
    bool closeOnBackdrop_ = true;
    std::function<void()> onConfirm_;
    std::function<void()> onCancel_;
    std::function<void()> onClose_;
    float visibilityAnim_ = 0.0f;
    float confirmHoverAnim_ = 0.0f;
    float cancelHoverAnim_ = 0.0f;
    float confirmHoverTarget_ = 0.0f;
    float cancelHoverTarget_ = 0.0f;
    float titleFontSize_ = 22.0f;
    float messageFontSize_ = 17.0f;
};

} // namespace EUINEO
