#pragma once

#include "../EUINEO.h"
#include "../ui/UIBuilder.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>

namespace EUINEO {

class SliderNode : public UINode {
public:
    class Builder : public UIBuilderBase<SliderNode, Builder> {
    public:
        Builder(UIContext& context, SliderNode& node) : UIBuilderBase<SliderNode, Builder>(context, node) {}

        Builder& value(float nextValue) {
            this->node_.trackComposeValue("value", nextValue);
            this->node_.value_ = nextValue;
            return *this;
        }

        Builder& onChange(std::function<void(float)> handler) {
            this->node_.onChange_ = std::move(handler);
            return *this;
        }
    };

    explicit SliderNode(const std::string& key) : UINode(key) {
        resetDefaults();
    }

    static constexpr const char* StaticTypeName() {
        return "SliderNode";
    }

    const char* typeName() const override {
        return StaticTypeName();
    }

    RectFrame paintBounds() const override {
        const RectFrame frame = PrimitiveFrame(primitive_);
        const float handleRadius = 8.0f;
        RectFrame bounds{
            frame.x - handleRadius,
            frame.y,
            frame.width + handleRadius * 2.0f,
            frame.height
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

    void update() override {
        const RectFrame frame = PrimitiveFrame(primitive_);
        const float clampedValue = std::clamp(value_, 0.0f, 1.0f);
        const float handleWidth = 10.0f;
        const float handleX = frame.x + clampedValue * frame.width - handleWidth * 0.5f;
        const float handleY = frame.y + (frame.height - 16.0f) * 0.5f;

        const bool hovered = primitive_.enabled &&
            State.mouseX >= handleX && State.mouseX <= handleX + handleWidth &&
            State.mouseY >= handleY && State.mouseY <= handleY + 16.0f;

        const float targetHover = (hovered || isDragging_) ? 1.0f : 0.0f;
        if (std::abs(hoverAnim_ - targetHover) > 0.001f) {
            hoverAnim_ = Lerp(hoverAnim_, targetHover, State.deltaTime * ((hovered || isDragging_) ? 15.0f : 10.0f));
            if (std::abs(hoverAnim_ - targetHover) < 0.001f) {
                hoverAnim_ = targetHover;
            }
            requestRepaint(12.0f);
        }

        if (hovered && State.mouseDown && !State.mouseClicked) {
            isDragging_ = true;
        }
        if (!State.mouseDown) {
            isDragging_ = false;
        }

        if (isDragging_ && frame.width > 0.0f) {
            const float relativeX = State.mouseX - frame.x;
            const float nextValue = std::clamp(relativeX / frame.width, 0.0f, 1.0f);
            if (std::abs(value_ - nextValue) > 0.0001f) {
                value_ = nextValue;
                if (onChange_) {
                    onChange_(value_);
                }
                requestRepaint(12.0f);
            }
        }
    }

    void draw() override {
        PrimitiveClipScope clip(primitive_);
        const RectFrame frame = PrimitiveFrame(primitive_);
        const float clampedValue = std::clamp(value_, 0.0f, 1.0f);

        const float trackHeight = 4.0f;
        const float trackY = frame.y + (frame.height - trackHeight) * 0.5f;
        Renderer::DrawRect(
            frame.x,
            trackY,
            frame.width,
            trackHeight,
            ApplyOpacity(CurrentTheme->surface, primitive_.opacity),
            trackHeight * 0.5f
        );

        if (clampedValue > 0.01f) {
            Renderer::DrawRect(
                frame.x,
                trackY,
                frame.width * clampedValue,
                trackHeight,
                ApplyOpacity(CurrentTheme->primary, primitive_.opacity),
                trackHeight * 0.5f
            );
        }

        const float handleRadius = 8.0f;
        const float handleX = frame.x + clampedValue * frame.width - handleRadius;
        const float handleY = frame.y + (frame.height - handleRadius * 2.0f) * 0.5f;
        const Color handleColor = ApplyOpacity(
            Lerp(CurrentTheme->text, CurrentTheme->primary, hoverAnim_),
            primitive_.opacity
        );
        Renderer::DrawRect(handleX, handleY, handleRadius * 2.0f, handleRadius * 2.0f, handleColor, handleRadius);
    }

protected:
    void resetDefaults() override {
        primitive_ = UIPrimitive{};
        primitive_.width = 300.0f;
        primitive_.height = 20.0f;
        value_ = 0.0f;
        onChange_ = {};
    }

private:
    void requestRepaint(float expand = 12.0f, float duration = 0.0f) {
        (void)expand;
        requestVisualRepaint(duration);
    }

    float value_ = 0.0f;
    std::function<void(float)> onChange_;
    bool isDragging_ = false;
    float hoverAnim_ = 0.0f;
};

} // namespace EUINEO
