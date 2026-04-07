#pragma once

#include "../EUINEO.h"
#include "../ui/UIBuilder.h"
#include "../ui/ThemeTokens.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace EUINEO {

class ComboBoxNode : public UINode {
public:
    class Builder : public UIBuilderBase<ComboBoxNode, Builder> {
    public:
        Builder(UIContext& context, ComboBoxNode& node) : UIBuilderBase<ComboBoxNode, Builder>(context, node) {}

        Builder& items(std::vector<std::string> value) {
            this->node_.trackComposeValue("items", value);
            this->node_.items_ = std::move(value);
            return *this;
        }

        Builder& placeholder(std::string value) {
            this->node_.trackComposeValue("placeholder", value);
            this->node_.placeholder_ = std::move(value);
            return *this;
        }

        Builder& selected(int index) {
            this->node_.trackComposeValue("selected", index);
            this->node_.selectedIndex_ = index;
            return *this;
        }

        Builder& fontSize(float value) {
            this->node_.trackComposeValue("fontSize", value);
            this->node_.fontSize_ = value;
            return *this;
        }

        Builder& maxVisibleItems(int value) {
            this->node_.trackComposeValue("maxVisibleItems", value);
            this->node_.maxVisibleItems_ = std::max(1, value);
            return *this;
        }

        Builder& startOpen(bool value) {
            this->node_.trackComposeValue("startOpen", value);
            if (!this->node_.openStateInitialized_) {
                this->node_.isOpen_ = value;
                this->node_.openAnim_ = value ? 1.0f : 0.0f;
                this->node_.popupPresentation_ = value;
                this->node_.openStateInitialized_ = true;
            }
            return *this;
        }

        Builder& onChange(std::function<void(int)> handler) {
            this->node_.onChange_ = std::move(handler);
            return *this;
        }
    };

    explicit ComboBoxNode(const std::string& key) : UINode(key) {
        resetDefaults();
    }

    static constexpr const char* StaticTypeName() {
        return "ComboBoxNode";
    }

    const char* typeName() const override {
        return StaticTypeName();
    }

    bool blocksUnderlyingInput() const override {
        return popupPresentation_ && (isOpen_ || openAnim_ > 0.001f);
    }

    bool wantsContinuousUpdate() const override {
        if (hoverAnim_ > 0.001f && hoverAnim_ < 0.999f) {
            return true;
        }
        if (openAnim_ > 0.001f && openAnim_ < 0.999f) {
            return true;
        }
        if (isScrollDragging_) {
            return true;
        }
        for (float hover : itemHoverAnims_) {
            if (hover > 0.001f && hover < 0.999f) {
                return true;
            }
        }
        return false;
    }

    RectFrame paintBounds() const override {
        const RectFrame frame = PrimitiveFrame(primitive_);
        RectFrame bounds = popupPresentation_ ? frame : clipPaintBounds(frame);
        const float visibleOpen = std::clamp(openAnim_, 0.0f, 1.0f);
        const float visibleListHeight = listVisibleHeight(frame.height, items_.size(), maxVisibleItems_, visibleOpen);
        if (visibleListHeight <= 0.0f) {
            return bounds;
        }

        const UIFieldVisualTokens visuals = CurrentFieldVisuals();
        const RectFrame popupFrame = PopupListFrame(frame, visibleListHeight, listOverlap(visibleListHeight));
        const RectFrame popupBounds = measurePaintBounds(
            popupFrame,
            MakePopupChromeStyle(primitive_, visuals.popupRounding),
            !popupPresentation_
        );
        return unionPaintBounds(bounds, popupBounds);
    }

    void update() override {
        ensureRuntimeState();
        const RectFrame frame = PrimitiveFrame(primitive_);
        const bool hoveredMain = hovered();
        const float listHeightMax = maxListHeight(frame.height, items_.size(), maxVisibleItems_);
        const float contentHeight = totalListHeight(frame.height, items_.size());
        const float maxScroll = maxScrollOffset(frame.height, items_.size(), maxVisibleItems_);
        scrollOffsetY_ = std::clamp(scrollOffsetY_, 0.0f, maxScroll);

        if (animateTowards(hoverAnim_, hoveredMain ? 1.0f : 0.0f, State.deltaTime * 15.0f)) {
            requestRepaint(openAnim_, openAnim_);
        }

        const float targetOpen = isOpen_ ? 1.0f : 0.0f;
        const float fromOpen = openAnim_;
        if (animateTowards(openAnim_, targetOpen, State.deltaTime * 20.0f)) {
            requestRepaint(fromOpen, openAnim_);
        }

        const bool wantsPopupPresentation = isOpen_ || openAnim_ > 0.001f;
        if (popupPresentation_ != wantsPopupPresentation) {
            popupPresentation_ = wantsPopupPresentation;
            if (popupPresentation_) {
                applyPopupPresentationDefaults(180);
            } else {
                primitive_.renderLayer = RenderLayer::Content;
                primitive_.clipToParent = true;
            }
            requestRepaint(openAnim_, openAnim_);
        }

        if (!State.mouseDown && isScrollDragging_) {
            isScrollDragging_ = false;
            requestRepaint(openAnim_, openAnim_);
        }

        if (isOpen_ || openAnim_ > 0.0f) {
            const float visibleOpen = std::clamp(openAnim_, 0.0f, 1.0f);
            const float visibleListHeight = listHeightMax * visibleOpen;
            const float overlap = listOverlap(visibleListHeight);
            const RectFrame popupFrame = PopupListFrame(frame, visibleListHeight, overlap);
            const bool hoveredList =
                State.mouseX >= popupFrame.x && State.mouseX <= popupFrame.x + popupFrame.width &&
                State.mouseY >= popupFrame.y && State.mouseY <= popupFrame.y + popupFrame.height;
            constexpr float kScrollTrackHitWidth = 14.0f;
            const bool hasScrollableList = maxScroll > 0.0f && popupFrame.height > 0.0f;
            const bool hoveredTrack = hasScrollableList && hoveredList &&
                State.mouseX >= popupFrame.x + popupFrame.width - kScrollTrackHitWidth;
            RectFrame thumb{};
            bool hoveredThumb = false;
            if (hasScrollableList) {
                thumb = popupThumbFrame(popupFrame, contentHeight, maxScroll, scrollOffsetY_);
                hoveredThumb =
                    State.mouseX >= thumb.x && State.mouseX <= thumb.x + thumb.width &&
                    State.mouseY >= thumb.y && State.mouseY <= thumb.y + thumb.height;
            }

            if (isScrollDragging_ && hasScrollableList) {
                State.scrollConsumed = true;
                const float travel = std::max(0.0f, popupFrame.height - thumb.height);
                float nextOffset = 0.0f;
                if (travel > 0.0f) {
                    const float localThumbY = std::clamp(State.mouseY - scrollDragGrabOffsetY_ - popupFrame.y, 0.0f, travel);
                    nextOffset = (localThumbY / travel) * maxScroll;
                }
                if (std::abs(nextOffset - scrollOffsetY_) > 0.01f) {
                    scrollOffsetY_ = nextOffset;
                    requestRepaint(openAnim_, openAnim_);
                }
            }

            if (hoveredList && !State.scrollConsumed && std::abs(State.scrollDeltaY) > 0.001f && maxScroll > 0.0f) {
                const float nextOffset = std::clamp(scrollOffsetY_ - State.scrollDeltaY * scrollStep_, 0.0f, maxScroll);
                State.scrollConsumed = true;
                if (std::abs(nextOffset - scrollOffsetY_) > 0.01f) {
                    scrollOffsetY_ = nextOffset;
                    requestRepaint(openAnim_, openAnim_);
                }
            }

            for (std::size_t index = 0; index < items_.size(); ++index) {
                const float itemY = popupFrame.y + static_cast<float>(index) * frame.height - scrollOffsetY_;
                const bool itemHovered = primitive_.enabled &&
                    State.mouseX >= popupFrame.x && State.mouseX <= popupFrame.x + popupFrame.width &&
                    State.mouseY >= itemY && State.mouseY <= itemY + frame.height &&
                    (!hasScrollableList || State.mouseX < popupFrame.x + popupFrame.width - kScrollTrackHitWidth) &&
                    itemY + frame.height > popupFrame.y && itemY < popupFrame.y + popupFrame.height;

                const float targetItemHover = (itemHovered && isOpen_) ? 1.0f : 0.0f;
                if (animateTowards(itemHoverAnims_[index], targetItemHover, State.deltaTime * 15.0f)) {
                    requestRepaint(openAnim_, openAnim_);
                }
            }
        }

        if (State.mouseClicked && primitive_.enabled) {
            if (isOpen_) {
                const float visibleOpen = std::clamp(openAnim_, 0.0f, 1.0f);
                const float visibleListHeight = listHeightMax * visibleOpen;
                const float overlap = listOverlap(visibleListHeight);
                const RectFrame popupFrame = PopupListFrame(frame, visibleListHeight, overlap);
                const float listY = popupFrame.y;
                const float listHeight = popupFrame.height;
                constexpr float kScrollTrackHitWidth = 14.0f;
                const bool hoveredList =
                    State.mouseX >= popupFrame.x && State.mouseX <= popupFrame.x + popupFrame.width &&
                    State.mouseY >= listY && State.mouseY <= listY + listHeight;
                const bool hoveredMainFrame =
                    State.mouseX >= frame.x && State.mouseX <= frame.x + frame.width &&
                    State.mouseY >= frame.y && State.mouseY <= frame.y + frame.height;
                const bool hasScrollableList = maxScroll > 0.0f && popupFrame.height > 0.0f;
                const bool hoveredTrack = hasScrollableList && hoveredList &&
                    State.mouseX >= popupFrame.x + popupFrame.width - kScrollTrackHitWidth;
                RectFrame thumb{};
                bool hoveredThumb = false;
                if (hasScrollableList) {
                    thumb = popupThumbFrame(popupFrame, contentHeight, maxScroll, scrollOffsetY_);
                    hoveredThumb =
                        State.mouseX >= thumb.x && State.mouseX <= thumb.x + thumb.width &&
                        State.mouseY >= thumb.y && State.mouseY <= thumb.y + thumb.height;
                }

                if (hoveredThumb || hoveredTrack) {
                    if (hasScrollableList && hoveredTrack && !hoveredThumb) {
                        const float travel = std::max(0.0f, popupFrame.height - thumb.height);
                        float nextOffset = 0.0f;
                        if (travel > 0.0f) {
                            const float localThumbY = std::clamp(State.mouseY - thumb.height * 0.5f - popupFrame.y, 0.0f, travel);
                            nextOffset = (localThumbY / travel) * maxScroll;
                        }
                        scrollOffsetY_ = std::clamp(nextOffset, 0.0f, maxScroll);
                    }
                    isScrollDragging_ = hasScrollableList;
                    scrollDragGrabOffsetY_ = hasScrollableList
                        ? std::clamp(State.mouseY - thumb.y, 0.0f, thumb.height)
                        : 0.0f;
                    requestRepaint(openAnim_, openAnim_);
                    State.mouseClicked = false;
                } else if (hoveredList && frame.height > 0.0f) {
                    const bool inItemRegion = !hasScrollableList || State.mouseX < popupFrame.x + popupFrame.width - kScrollTrackHitWidth;
                    if (inItemRegion) {
                        const int index = static_cast<int>((State.mouseY - listY + scrollOffsetY_) / frame.height);
                        if (index >= 0 && index < static_cast<int>(items_.size())) {
                            selectedIndex_ = index;
                            if (onChange_) {
                                onChange_(selectedIndex_);
                            }
                        }
                        const float fromOpen = openAnim_;
                        isOpen_ = false;
                        isScrollDragging_ = false;
                        requestRepaint(fromOpen, 0.0f);
                    }
                    State.mouseClicked = false;
                } else if (hoveredMainFrame) {
                    const float fromOpen = openAnim_;
                    isOpen_ = false;
                    isScrollDragging_ = false;
                    requestRepaint(fromOpen, 0.0f);
                    State.mouseClicked = false;
                } else {
                    const float fromOpen = openAnim_;
                    isOpen_ = false;
                    isScrollDragging_ = false;
                    requestRepaint(fromOpen, 0.0f);
                    State.mouseClicked = false;
                }
            } else if (hoveredMain) {
                const float fromOpen = openAnim_;
                isOpen_ = true;
                isScrollDragging_ = false;
                requestRepaint(fromOpen, 1.0f);
                State.mouseClicked = false;
            }
        }
    }

    void draw() override {
        const RectFrame frame = PrimitiveFrame(primitive_);
        const UIFieldVisualTokens visuals = CurrentFieldVisuals();
        const float textScale = fontSize_ / 24.0f;
        const float textX = frame.x + visuals.horizontalInset;
        const float textY = frame.y + frame.height * 0.5f + (fontSize_ / 4.0f);
        const float visibleOpen = std::clamp(openAnim_, 0.0f, 1.0f);
        const float listHeightMax = maxListHeight(frame.height, items_.size(), maxVisibleItems_);
        const float visibleListHeight = listHeightMax * visibleOpen;

        if (visibleListHeight > 1.0f) {
            const float overlap = listOverlap(visibleListHeight);
            const RectFrame popupFrame = PopupListFrame(frame, visibleListHeight, overlap);
            const float listY = popupFrame.y;
            const float listHeight = popupFrame.height;
            DrawPopupChrome(primitive_, popupFrame, visuals.popupRounding);

            for (std::size_t index = 0; index < items_.size(); ++index) {
                const float itemY = listY + static_cast<float>(index) * frame.height - scrollOffsetY_;
                const float currentListBottom = listY + listHeight;
                const float visibleHeight = std::clamp(currentListBottom - itemY, 0.0f, frame.height);

                if (visibleHeight <= 0.0f || itemY + frame.height < listY) {
                    continue;
                }

                const float itemAlpha = visibleHeight / frame.height;
                Color itemBg = Lerp(CurrentTheme->surface, CurrentTheme->surfaceHover, itemHoverAnims_[index]);
                if (itemHoverAnims_[index] > 0.01f) {
                    itemBg.a *= visibleOpen;
                    Renderer::DrawRect(
                        frame.x + 1.0f,
                        itemY,
                        frame.width - 2.0f,
                        visibleHeight,
                        ApplyOpacity(itemBg, primitive_.opacity),
                        visibleHeight < frame.height ? 0.0f : 4.0f
                    );
                }

                Color itemColor = index == static_cast<std::size_t>(selectedIndex_) ? CurrentTheme->primary : CurrentTheme->text;
                itemColor.a *= visibleOpen * itemAlpha;
                if (visibleHeight >= fontSize_ * 0.90f) {
                    Renderer::DrawTextStr(items_[index], textX, itemY + frame.height * 0.5f + (fontSize_ / 4.0f),
                                          ApplyOpacity(itemColor, primitive_.opacity), textScale);
                }
            }

            const float maxScroll = maxScrollOffset(frame.height, items_.size(), maxVisibleItems_);
            if (maxScroll > 0.0f && popupFrame.height > 0.0f) {
                const RectFrame thumb = popupThumbFrame(
                    popupFrame,
                    totalListHeight(frame.height, items_.size()),
                    maxScroll,
                    scrollOffsetY_
                );
                const Color thumbColor = Color(CurrentTheme->text.r, CurrentTheme->text.g, CurrentTheme->text.b, 0.32f);
                Renderer::DrawRect(
                    thumb.x,
                    thumb.y,
                    thumb.width,
                    thumb.height,
                    ApplyOpacity(thumbColor, primitive_.opacity),
                    thumb.width * 0.5f
                );
            }
        }

        {
            PrimitiveClipScope clip(primitive_);
            DrawFieldChrome(primitive_, hoverAnim_, openAnim_, primitive_.rounding);

            const std::string displayText =
                (selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(items_.size())) ? items_[selectedIndex_] : placeholder_;
            Color textColor = selectedIndex_ >= 0
                ? CurrentTheme->text
                : Color(CurrentTheme->text.r, CurrentTheme->text.g, CurrentTheme->text.b, 0.5f);
            Renderer::DrawTextStr(displayText, textX, textY, ApplyOpacity(textColor, primitive_.opacity), textScale);
            Renderer::DrawTextStr(isOpen_ ? "\xEF\x84\x86" : "\xEF\x84\x87",
                                  frame.x + frame.width - 25.0f, textY,
                                  ApplyOpacity(CurrentTheme->text, primitive_.opacity), textScale);
        }
    }

protected:
    void resetDefaults() override {
        primitive_ = UIPrimitive{};
        if (popupPresentation_) {
            applyPopupPresentationDefaults(180);
        }
        primitive_.width = 220.0f;
        primitive_.height = 36.0f;
        placeholder_.clear();
        items_.clear();
        selectedIndex_ = -1;
        fontSize_ = 20.0f;
        maxVisibleItems_ = 6;
        scrollStep_ = 48.0f;
        onChange_ = {};
    }

private:
    static float totalListHeight(float itemHeight, std::size_t itemCount) {
        return static_cast<float>(itemCount) * itemHeight;
    }

    static float maxListHeight(float itemHeight, std::size_t itemCount, int maxVisibleItems) {
        const int visibleCount = std::max(1, std::min(static_cast<int>(itemCount), maxVisibleItems));
        return static_cast<float>(visibleCount) * itemHeight;
    }

    static float listVisibleHeight(float itemHeight, std::size_t itemCount, int maxVisibleItems, float openFactor) {
        return maxListHeight(itemHeight, itemCount, maxVisibleItems) * std::clamp(openFactor, 0.0f, 1.0f);
    }

    static float maxScrollOffset(float itemHeight, std::size_t itemCount, int maxVisibleItems) {
        return std::max(0.0f, totalListHeight(itemHeight, itemCount) - maxListHeight(itemHeight, itemCount, maxVisibleItems));
    }

    static RectFrame popupThumbFrame(const RectFrame& popupFrame, float contentHeight, float maxScroll, float scrollOffsetY) {
        const float thumbWidth = 6.0f;
        const float thumbMargin = 4.0f;
        const float visibleRatio = std::clamp(popupFrame.height / std::max(contentHeight, popupFrame.height), 0.0f, 1.0f);
        const float thumbHeight = std::max(22.0f, popupFrame.height * visibleRatio);
        const float travel = std::max(0.0f, popupFrame.height - thumbHeight);
        const float thumbY = popupFrame.y + (maxScroll > 0.0f ? (scrollOffsetY / maxScroll) * travel : 0.0f);
        const float thumbX = popupFrame.x + popupFrame.width - thumbWidth - thumbMargin;
        return RectFrame{thumbX, thumbY, thumbWidth, thumbHeight};
    }

    static float listOverlap(float visibleListHeight) {
        return visibleListHeight > 6.0f ? 1.0f : 0.0f;
    }

    void ensureRuntimeState() {
        if (itemHoverAnims_.size() != items_.size()) {
            itemHoverAnims_.assign(items_.size(), 0.0f);
        }
    }

    void requestRepaint(float fromOpenFactor, float toOpenFactor, float duration = 0.0f) {
        const float delta = std::abs(toOpenFactor - fromOpenFactor);
        const float autoDuration = std::max(0.10f, delta * 0.20f);
        requestVisualRepaint(duration > 0.0f ? duration : autoDuration);
    }

    std::vector<std::string> items_;
    std::string placeholder_;
    int selectedIndex_ = -1;
    float fontSize_ = 20.0f;
    int maxVisibleItems_ = 6;
    float scrollStep_ = 48.0f;
    float scrollOffsetY_ = 0.0f;
    bool isScrollDragging_ = false;
    float scrollDragGrabOffsetY_ = 0.0f;
    std::function<void(int)> onChange_;
    bool isOpen_ = false;
    bool openStateInitialized_ = false;
    bool popupPresentation_ = false;
    float hoverAnim_ = 0.0f;
    float openAnim_ = 0.0f;
    std::vector<float> itemHoverAnims_;
};

} // namespace EUINEO
