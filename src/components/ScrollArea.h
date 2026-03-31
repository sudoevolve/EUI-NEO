#pragma once

#include "../EUINEO.h"
#include "../ui/UIBuilder.h"
#include <algorithm>
#include <cmath>

namespace EUINEO {

class ScrollAreaNode : public UINode {
public:
    class Builder : public UIBuilderBase<ScrollAreaNode, Builder> {
    public:
        Builder(UIContext& context, ScrollAreaNode& node) : UIBuilderBase<ScrollAreaNode, Builder>(context, node) {}

        Builder& contentHeight(float value) {
            this->node_.trackComposeValue("contentHeight", value);
            this->node_.setContentHeight(value);
            return *this;
        }

        Builder& scrollStep(float value) {
            this->node_.trackComposeValue("scrollStep", value);
            this->node_.setScrollStep(value);
            return *this;
        }
    };

    explicit ScrollAreaNode(const std::string& key) : UINode(key) {
        resetDefaults();
    }

    static constexpr const char* StaticTypeName() {
        return "ScrollAreaNode";
    }

    const char* typeName() const override {
        return StaticTypeName();
    }

    bool wantsContinuousUpdate() const override {
        return isDragging_ ||
               isContentDragging_ ||
               (accentAnim_ > 0.001f && accentAnim_ < 0.999f) ||
               accentPulse_ > 0.01f;
    }

    RectFrame paintBounds() const override {
        return clipPaintBounds(PrimitiveFrame(primitive_));
    }

    void setContentHeight(float value) {
        contentHeight_ = std::max(0.0f, value);
        clampScrollOffset();
    }

    void setScrollStep(float value) {
        scrollStep_ = std::max(8.0f, value);
    }

    float scrollOffsetY() const {
        return scrollOffsetY_;
    }

    void update() override {
        const RectFrame frame = PrimitiveFrame(primitive_);
        hovered_ = hovered();

        const float maxScroll = maxScrollOffset(frame);
        if (scrollOffsetY_ > maxScroll) {
            scrollOffsetY_ = maxScroll;
            requestVisualRepaint();
        }

        const RectFrame thumb = thumbFrame(frame, maxScroll);
        const float trackHitWidth = 16.0f;
        const bool hoveredThumb = hovered_ &&
            State.mouseX >= thumb.x && State.mouseX <= thumb.x + thumb.width &&
            State.mouseY >= thumb.y && State.mouseY <= thumb.y + thumb.height;
        const bool hoveredTrack = hovered_ &&
            State.mouseX >= frame.x + frame.width - trackHitWidth && State.mouseX <= frame.x + frame.width &&
            State.mouseY >= frame.y && State.mouseY <= frame.y + frame.height;

        if (State.mouseClicked && primitive_.enabled && hovered_ && maxScroll > 0.0f) {
            if (hoveredThumb) {
                isDragging_ = true;
                isContentDragging_ = false;
                dragGrabOffsetY_ = std::clamp(State.mouseY - thumb.y, 0.0f, thumb.height);
            } else if (hoveredTrack) {
                const float travel = std::max(0.0f, frame.height - thumb.height);
                float nextOffset = 0.0f;
                if (travel > 0.0f) {
                    const float localThumbY = std::clamp(State.mouseY - thumb.height * 0.5f - frame.y, 0.0f, travel);
                    nextOffset = (localThumbY / travel) * maxScroll;
                }
                scrollOffsetY_ = std::clamp(nextOffset, 0.0f, maxScroll);
                isDragging_ = true;
                isContentDragging_ = false;
                dragGrabOffsetY_ = thumb.height * 0.5f;
            } else {
                isDragging_ = false;
                isContentDragging_ = true;
                contentDragStartMouseY_ = State.mouseY;
                contentDragStartOffsetY_ = scrollOffsetY_;
            }
            requestVisualRepaint();
        }

        if (!State.mouseDown && (isDragging_ || isContentDragging_)) {
            isDragging_ = false;
            isContentDragging_ = false;
            requestVisualRepaint();
        }

        bool scrollChanged = false;

        if (isDragging_ && maxScroll > 0.0f) {
            State.scrollConsumed = true;
            const float travel = std::max(0.0f, frame.height - thumb.height);
            float nextOffset = 0.0f;
            if (travel > 0.0f) {
                const float localThumbY = std::clamp(State.mouseY - dragGrabOffsetY_ - frame.y, 0.0f, travel);
                nextOffset = (localThumbY / travel) * maxScroll;
            }
            if (std::abs(nextOffset - scrollOffsetY_) > 0.01f) {
                scrollOffsetY_ = nextOffset;
                scrollChanged = true;
                requestVisualRepaint();
            }
        }

        if (isContentDragging_ && maxScroll > 0.0f) {
            State.scrollConsumed = true;
            const float dragDeltaY = State.mouseY - contentDragStartMouseY_;
            const float nextOffset = std::clamp(contentDragStartOffsetY_ - dragDeltaY, 0.0f, maxScroll);
            if (std::abs(nextOffset - scrollOffsetY_) > 0.01f) {
                scrollOffsetY_ = nextOffset;
                scrollChanged = true;
                requestVisualRepaint();
            }
        }

        if (hovered_ && !State.scrollConsumed && std::abs(State.scrollDeltaY) > 0.001f && maxScroll > 0.0f) {
            const float nextOffset = std::clamp(scrollOffsetY_ - State.scrollDeltaY * scrollStep_, 0.0f, maxScroll);
            State.scrollConsumed = true;
            if (std::abs(nextOffset - scrollOffsetY_) > 0.01f) {
                scrollOffsetY_ = nextOffset;
                scrollChanged = true;
                requestVisualRepaint();
            }
        }

        if (scrollChanged) {
            accentPulse_ = 1.0f;
        }

        const float targetAccent = (hoveredThumb || isDragging_ || isContentDragging_ || accentPulse_ > 0.01f) ? 1.0f : 0.0f;
        if (std::abs(accentAnim_ - targetAccent) > 0.001f) {
            accentAnim_ = Lerp(accentAnim_, targetAccent, State.deltaTime * 14.0f);
            if (std::abs(accentAnim_ - targetAccent) < 0.01f) {
                accentAnim_ = targetAccent;
            }
            requestVisualRepaint();
        }

        if (accentPulse_ > 0.0f) {
            accentPulse_ = std::max(0.0f, accentPulse_ - State.deltaTime * 4.0f);
            requestVisualRepaint();
        }
    }

    void draw() override {
        const RectFrame frame = PrimitiveFrame(primitive_);
        const float maxScroll = maxScrollOffset(frame);
        if (maxScroll <= 0.0f || frame.width <= 0.0f || frame.height <= 0.0f) {
            return;
        }

        PrimitiveClipScope clip(primitive_);

        const RectFrame thumb = thumbFrame(frame, maxScroll);
        const float pulseMix = std::clamp(std::max(accentAnim_, accentPulse_), 0.0f, 1.0f);
        const Color idleColor = hovered_
            ? Color(CurrentTheme->text.r, CurrentTheme->text.g, CurrentTheme->text.b, 0.24f)
            : Color(CurrentTheme->text.r, CurrentTheme->text.g, CurrentTheme->text.b, 0.14f);
        const Color activeColor = Color(CurrentTheme->primary.r, CurrentTheme->primary.g, CurrentTheme->primary.b, 0.92f);
        const Color thumbColor = Lerp(idleColor, activeColor, pulseMix);

        Renderer::DrawRect(
            thumb.x,
            thumb.y,
            thumb.width,
            thumb.height,
            ApplyOpacity(thumbColor, primitive_.opacity),
            thumb.width * 0.5f
        );
    }

protected:
    void resetDefaults() override {
        primitive_ = UIPrimitive{};
        primitive_.zIndex = 48;
        primitive_.width = 200.0f;
        primitive_.height = 200.0f;
    }

private:
    RectFrame thumbFrame(const RectFrame& frame, float maxScroll) const {
        const float thumbWidth = (hovered_ || isDragging_ || accentAnim_ > 0.01f) ? 6.0f : 4.0f;
        const float thumbMargin = 4.0f;
        const float visibleRatio = std::clamp(frame.height / std::max(contentHeight_, frame.height), 0.0f, 1.0f);
        const float thumbHeight = std::max(28.0f, frame.height * visibleRatio);
        const float travel = std::max(0.0f, frame.height - thumbHeight);
        const float thumbY = frame.y + (maxScroll > 0.0f ? (scrollOffsetY_ / maxScroll) * travel : 0.0f);
        const float thumbX = frame.x + frame.width - thumbWidth - thumbMargin;
        return RectFrame{thumbX, thumbY, thumbWidth, thumbHeight};
    }

    float maxScrollOffset(const RectFrame& frame) const {
        return std::max(0.0f, contentHeight_ - frame.height);
    }

    void clampScrollOffset() {
        const RectFrame frame = PrimitiveFrame(primitive_);
        scrollOffsetY_ = std::clamp(scrollOffsetY_, 0.0f, maxScrollOffset(frame));
    }

    float contentHeight_ = 0.0f;
    float scrollStep_ = 48.0f;
    float scrollOffsetY_ = 0.0f;
    bool hovered_ = false;
    bool isDragging_ = false;
    bool isContentDragging_ = false;
    float dragGrabOffsetY_ = 0.0f;
    float contentDragStartMouseY_ = 0.0f;
    float contentDragStartOffsetY_ = 0.0f;
    float accentAnim_ = 0.0f;
    float accentPulse_ = 0.0f;
};

} // namespace EUINEO
