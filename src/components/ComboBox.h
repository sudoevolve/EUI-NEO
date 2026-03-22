#pragma once

#include "../EUINEO.h"
#include "../ui/UIBuilder.h"
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

    RectFrame paintBounds() const override {
        const RectFrame frame = PrimitiveFrame(primitive_);
        RectFrame bounds = frame;
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

        const float visibleOpen = std::clamp(openAnim_, 0.0f, 1.0f);
        const float visibleListHeight = listVisibleHeight(frame.height, items_.size(), visibleOpen);
        if (visibleListHeight > 1.0f) {
            const float overlap = listOverlap(visibleListHeight);
            bounds = RectFrame{
                std::min(bounds.x, frame.x),
                std::min(bounds.y, frame.y + frame.height - overlap),
                std::max(bounds.x + bounds.width, frame.x + frame.width) - std::min(bounds.x, frame.x),
                std::max(bounds.y + bounds.height, frame.y + frame.height + visibleListHeight) -
                    std::min(bounds.y, frame.y + frame.height - overlap)
            };
        }
        return bounds;
    }

    void update() override {
        ensureRuntimeState();
        const RectFrame frame = PrimitiveFrame(primitive_);
        const bool hoveredMain = primitive_.enabled && PrimitiveContains(primitive_, State.mouseX, State.mouseY);

        const float targetHover = hoveredMain ? 1.0f : 0.0f;
        if (std::abs(hoverAnim_ - targetHover) > 0.001f) {
            hoverAnim_ = Lerp(hoverAnim_, targetHover, State.deltaTime * 15.0f);
            if (std::abs(hoverAnim_ - targetHover) < 0.01f) {
                hoverAnim_ = targetHover;
            }
            requestRepaint(openAnim_, openAnim_);
        }

        const float targetOpen = isOpen_ ? 1.0f : 0.0f;
        if (std::abs(openAnim_ - targetOpen) > 0.001f) {
            const float fromOpen = openAnim_;
            openAnim_ = Lerp(openAnim_, targetOpen, State.deltaTime * 20.0f);
            if (std::abs(openAnim_ - targetOpen) < 0.01f) {
                openAnim_ = targetOpen;
            }
            requestRepaint(fromOpen, openAnim_);
        }

        if (isOpen_ || openAnim_ > 0.0f) {
            const float listY = frame.y + frame.height;
            for (std::size_t index = 0; index < items_.size(); ++index) {
                const float itemY = listY + static_cast<float>(index) * frame.height;
                const bool itemHovered = primitive_.enabled &&
                    State.mouseX >= frame.x && State.mouseX <= frame.x + frame.width &&
                    State.mouseY >= itemY && State.mouseY <= itemY + frame.height;

                const float targetItemHover = (itemHovered && isOpen_) ? 1.0f : 0.0f;
                if (std::abs(itemHoverAnims_[index] - targetItemHover) > 0.001f) {
                    itemHoverAnims_[index] = Lerp(itemHoverAnims_[index], targetItemHover, State.deltaTime * 15.0f);
                    if (std::abs(itemHoverAnims_[index] - targetItemHover) < 0.01f) {
                        itemHoverAnims_[index] = targetItemHover;
                    }
                    requestRepaint(openAnim_, openAnim_);
                }
            }
        }

        if (State.mouseClicked && primitive_.enabled) {
            if (isOpen_) {
                const float listY = frame.y + frame.height;
                const float listHeight = static_cast<float>(items_.size()) * frame.height;
                const bool hoveredList =
                    State.mouseX >= frame.x && State.mouseX <= frame.x + frame.width &&
                    State.mouseY >= listY && State.mouseY <= listY + listHeight;

                if (hoveredList && frame.height > 0.0f) {
                    const int index = static_cast<int>((State.mouseY - listY) / frame.height);
                    if (index >= 0 && index < static_cast<int>(items_.size())) {
                        selectedIndex_ = index;
                        if (onChange_) {
                            onChange_(selectedIndex_);
                        }
                    }
                }

                const float fromOpen = openAnim_;
                isOpen_ = false;
                requestRepaint(fromOpen, 0.0f);
            } else if (hoveredMain) {
                const float fromOpen = openAnim_;
                isOpen_ = true;
                requestRepaint(fromOpen, 1.0f);
            }
        }
    }

    void draw() override {
        const RectFrame frame = PrimitiveFrame(primitive_);
        const float textScale = fontSize_ / 24.0f;
        const float textX = frame.x + 10.0f;
        const float textY = frame.y + frame.height * 0.5f + (fontSize_ / 4.0f);
        const float visibleOpen = std::clamp(openAnim_, 0.0f, 1.0f);
        const float visibleListHeight = listVisibleHeight(frame.height, items_.size(), visibleOpen);

        if (visibleListHeight > 1.0f) {
            const float overlap = listOverlap(visibleListHeight);
            const float listY = frame.y + frame.height - overlap;
            const float listHeight = visibleListHeight + overlap;
            Renderer::DrawRect(
                frame.x,
                listY,
                frame.width,
                listHeight,
                ApplyOpacity(CurrentTheme->surface, primitive_.opacity),
                6.0f
            );

            for (std::size_t index = 0; index < items_.size(); ++index) {
                const float itemY = frame.y + frame.height + static_cast<float>(index) * frame.height;
                const float currentListBottom = listY + listHeight;
                const float visibleHeight = std::clamp(currentListBottom - itemY, 0.0f, frame.height);

                if (itemY > currentListBottom || visibleHeight <= 0.0f) {
                    break;
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
        }

        {
            PrimitiveClipScope clip(primitive_);
            Color baseColor = Lerp(CurrentTheme->surface, CurrentTheme->surfaceActive, openAnim_);
            Color hoverColor = Lerp(CurrentTheme->surfaceHover, CurrentTheme->surfaceActive, openAnim_);
            const Color background = ApplyOpacity(Lerp(baseColor, hoverColor, hoverAnim_), primitive_.opacity);
            Renderer::DrawRect(frame.x, frame.y, frame.width, frame.height, background, 6.0f);

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
        primitive_.width = 220.0f;
        primitive_.height = 36.0f;
        placeholder_.clear();
        items_.clear();
        selectedIndex_ = -1;
        fontSize_ = 20.0f;
        onChange_ = {};
    }

private:
    static float listVisibleHeight(float itemHeight, std::size_t itemCount, float openFactor) {
        return static_cast<float>(itemCount) * itemHeight * std::clamp(openFactor, 0.0f, 1.0f);
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
        (void)fromOpenFactor;
        (void)toOpenFactor;
        requestVisualRepaint(duration);
    }

    std::vector<std::string> items_;
    std::string placeholder_;
    int selectedIndex_ = -1;
    float fontSize_ = 20.0f;
    std::function<void(int)> onChange_;
    bool isOpen_ = false;
    float hoverAnim_ = 0.0f;
    float openAnim_ = 0.0f;
    std::vector<float> itemHoverAnims_;
};

} // namespace EUINEO
