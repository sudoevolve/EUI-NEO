#pragma once

#include "../EUINEO.h"
#include "../ui/UIBuilder.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <utility>

namespace EUINEO {

class InputBoxNode : public UINode {
public:
    class Builder : public UIBuilderBase<InputBoxNode, Builder> {
    public:
        Builder(UIContext& context, InputBoxNode& node) : UIBuilderBase<InputBoxNode, Builder>(context, node) {}

        Builder& placeholder(std::string value) {
            this->node_.trackComposeValue("placeholder", value);
            this->node_.placeholder_ = std::move(value);
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

        Builder& onChange(std::function<void(const std::string&)> handler) {
            this->node_.onChange_ = std::move(handler);
            return *this;
        }

        Builder& onEnter(std::function<void()> handler) {
            this->node_.onEnter_ = std::move(handler);
            return *this;
        }
    };

    explicit InputBoxNode(const std::string& key) : UINode(key) {
        resetDefaults();
    }

    static constexpr const char* StaticTypeName() {
        return "InputBoxNode";
    }

    const char* typeName() const override {
        return StaticTypeName();
    }

    bool wantsContinuousUpdate() const override {
        return isFocused_;
    }

    void update() override {
        cursorPosition_ = std::min(cursorPosition_, static_cast<int>(text_.size()));
        const bool hovered = primitive_.enabled && PrimitiveContains(primitive_, State.mouseX, State.mouseY);

        if (!primitive_.enabled && isFocused_) {
            isFocused_ = false;
            requestRepaint(4.0f);
        }

        const float targetHover = hovered ? 1.0f : 0.0f;
        if (std::abs(hoverAnim_ - targetHover) > 0.001f) {
            hoverAnim_ = Lerp(hoverAnim_, targetHover, State.deltaTime * 15.0f);
            if (std::abs(hoverAnim_ - targetHover) < 0.01f) {
                hoverAnim_ = targetHover;
            }
            requestRepaint(4.0f);
        }

        const float targetFocus = isFocused_ ? 1.0f : 0.0f;
        if (std::abs(focusAnim_ - targetFocus) > 0.001f) {
            focusAnim_ = Lerp(focusAnim_, targetFocus, State.deltaTime * 15.0f);
            if (std::abs(focusAnim_ - targetFocus) < 0.01f) {
                focusAnim_ = targetFocus;
            }
            requestRepaint(4.0f);
        }

        if (State.mouseClicked) {
            const bool nextFocus = primitive_.enabled && hovered;
            if (isFocused_ != nextFocus) {
                isFocused_ = nextFocus;
                cursorBlinkTime_ = 0.0f;
                cursorVisible_ = true;
                requestRepaint(4.0f);
            }
        }

        if (isFocused_) {
            cursorBlinkTime_ += State.deltaTime;
            if (cursorBlinkTime_ >= 1.0f) {
                cursorBlinkTime_ = std::fmod(cursorBlinkTime_, 1.0f);
            }

            const bool nextCursorVisible = cursorBlinkTime_ < 0.5f;
            if (cursorVisible_ != nextCursorVisible) {
                cursorVisible_ = nextCursorVisible;
                requestRepaint(4.0f);
            }

            if (!State.textInput.empty()) {
                text_ += State.textInput;
                cursorPosition_ = static_cast<int>(text_.size());
                cursorBlinkTime_ = 0.0f;
                cursorVisible_ = true;
                if (onChange_) {
                    onChange_(text_);
                }
                requestRepaint(4.0f);
            }

            if (State.keysPressed[GLFW_KEY_BACKSPACE] && !text_.empty()) {
                while (!text_.empty() && (static_cast<unsigned char>(text_.back()) & 0xC0U) == 0x80U) {
                    text_.pop_back();
                }
                if (!text_.empty()) {
                    text_.pop_back();
                }
                cursorPosition_ = static_cast<int>(text_.size());
                cursorBlinkTime_ = 0.0f;
                cursorVisible_ = true;
                if (onChange_) {
                    onChange_(text_);
                }
                requestRepaint(4.0f);
            }

            if (State.keysPressed[GLFW_KEY_ENTER] || State.keysPressed[GLFW_KEY_KP_ENTER]) {
                if (onEnter_) {
                    onEnter_();
                }
                requestRepaint(4.0f);
            }
        } else {
            cursorBlinkTime_ = 0.0f;
            cursorVisible_ = true;
        }
    }

    void draw() override {
        PrimitiveClipScope clip(primitive_);
        const RectFrame frame = PrimitiveFrame(primitive_);

        Color baseColor = Lerp(CurrentTheme->surface, CurrentTheme->surfaceActive, focusAnim_);
        Color hoverColor = Lerp(CurrentTheme->surfaceHover, CurrentTheme->surfaceActive, focusAnim_);
        const Color background = ApplyOpacity(Lerp(baseColor, hoverColor, hoverAnim_), primitive_.opacity);
        Renderer::DrawRect(frame.x, frame.y, frame.width, frame.height, background, 6.0f);

        if (focusAnim_ > 0.01f) {
            Renderer::DrawRect(frame.x, frame.y, frame.width, 1.0f, ApplyOpacity(CurrentTheme->border, primitive_.opacity), 0.0f);
            Color focusColor = CurrentTheme->primary;
            focusColor.a = focusAnim_;
            Renderer::DrawRect(frame.x, frame.y, frame.width, 2.0f, ApplyOpacity(focusColor, primitive_.opacity), 0.0f);
        } else {
            Renderer::DrawRect(frame.x, frame.y, frame.width, 1.0f, ApplyOpacity(CurrentTheme->border, primitive_.opacity), 0.0f);
        }

        const float textScale = fontSize_ / 24.0f;
        const float textX = frame.x + 10.0f;
        const float textY = frame.y + frame.height * 0.5f + (fontSize_ / 4.0f);

        if (text_.empty()) {
            if (!placeholder_.empty()) {
                Color placeholderColor = CurrentTheme->text;
                placeholderColor.a = 0.5f;
                Renderer::DrawTextStr(placeholder_, textX, textY, ApplyOpacity(placeholderColor, primitive_.opacity), textScale);
            }
        } else {
            Renderer::DrawTextStr(text_, textX, textY, ApplyOpacity(CurrentTheme->text, primitive_.opacity), textScale);
        }

        if (isFocused_ && cursorVisible_) {
            const float cursorX = textX + (text_.empty() ? 0.0f : Renderer::MeasureTextWidth(text_, textScale));
            Renderer::DrawRect(
                cursorX,
                frame.y + frame.height * 0.5f - 10.0f,
                2.0f,
                20.0f,
                ApplyOpacity(CurrentTheme->primary, primitive_.opacity),
                1.0f
            );
        }
    }

protected:
    void resetDefaults() override {
        primitive_ = UIPrimitive{};
        primitive_.width = 220.0f;
        primitive_.height = 36.0f;
        placeholder_.clear();
        text_.clear();
        fontSize_ = 20.0f;
        onChange_ = {};
        onEnter_ = {};
    }

private:
    void requestRepaint(float expand = 4.0f, float duration = 0.0f) {
        (void)expand;
        requestVisualRepaint(duration);
    }

    std::string placeholder_;
    std::string text_;
    float fontSize_ = 20.0f;
    std::function<void(const std::string&)> onChange_;
    std::function<void()> onEnter_;
    bool isFocused_ = false;
    float cursorBlinkTime_ = 0.0f;
    int cursorPosition_ = 0;
    bool cursorVisible_ = true;
    float hoverAnim_ = 0.0f;
    float focusAnim_ = 0.0f;
};

} // namespace EUINEO
