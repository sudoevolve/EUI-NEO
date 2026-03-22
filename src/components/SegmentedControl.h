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

class SegmentedControlNode : public UINode {
public:
    class Builder : public UIBuilderBase<SegmentedControlNode, Builder> {
    public:
        Builder(UIContext& context, SegmentedControlNode& node) : UIBuilderBase<SegmentedControlNode, Builder>(context, node) {}

        Builder& items(std::vector<std::string> value) {
            this->node_.trackComposeValue("items", value);
            this->node_.items_ = std::move(value);
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

    explicit SegmentedControlNode(const std::string& key) : UINode(key) {
        resetDefaults();
    }

    static constexpr const char* StaticTypeName() {
        return "SegmentedControlNode";
    }

    const char* typeName() const override {
        return StaticTypeName();
    }

    void update() override {
        const int clampedSelected = items_.empty()
            ? 0
            : std::clamp(selectedIndex_, 0, static_cast<int>(items_.size()) - 1);

        if (!indicatorReady_ || lastItemCount_ != items_.size()) {
            indicatorAnim_ = static_cast<float>(clampedSelected);
            indicatorReady_ = true;
            lastItemCount_ = items_.size();
        }

        if (items_.empty()) {
            return;
        }

        const RectFrame frame = PrimitiveFrame(primitive_);
        const bool hovered = primitive_.enabled && PrimitiveContains(primitive_, State.mouseX, State.mouseY);
        if (hovered && State.mouseClicked && frame.width > 0.0f) {
            const float segmentWidth = frame.width / static_cast<float>(items_.size());
            const float relativeX = State.mouseX - frame.x;
            selectedIndex_ = std::clamp(static_cast<int>(relativeX / segmentWidth), 0, static_cast<int>(items_.size()) - 1);
            if (onChange_) {
                onChange_(selectedIndex_);
            }
            requestRepaint(4.0f);
        }

        const float targetAnim = static_cast<float>(clampedSelected);
        if (std::abs(indicatorAnim_ - targetAnim) > 0.001f) {
            indicatorAnim_ = Lerp(indicatorAnim_, targetAnim, State.deltaTime * 15.0f);
            if (std::abs(indicatorAnim_ - targetAnim) < 0.001f) {
                indicatorAnim_ = targetAnim;
            }
            requestRepaint(4.0f);
        }
    }

    void draw() override {
        PrimitiveClipScope clip(primitive_);
        const RectFrame frame = PrimitiveFrame(primitive_);
        const float cornerRadius = primitive_.rounding > 0.0f ? primitive_.rounding : 6.0f;
        Renderer::DrawRect(frame.x, frame.y, frame.width, frame.height,
                           ApplyOpacity(CurrentTheme->surface, primitive_.opacity), cornerRadius);

        if (items_.empty()) {
            return;
        }

        const float segmentWidth = frame.width / static_cast<float>(items_.size());
        const float indicatorX = frame.x + indicatorAnim_ * segmentWidth;
        Renderer::DrawRect(indicatorX + 2.0f, frame.y + 2.0f, segmentWidth - 4.0f, frame.height - 4.0f,
                           ApplyOpacity(CurrentTheme->primary, primitive_.opacity), std::max(0.0f, cornerRadius - 1.0f));

        const float textScale = fontSize_ / 24.0f;
        for (std::size_t index = 0; index < items_.size(); ++index) {
            const float textWidth = Renderer::MeasureTextWidth(items_[index], textScale);
            const float textX = frame.x + static_cast<float>(index) * segmentWidth + (segmentWidth - textWidth) * 0.5f;
            const float textY = frame.y + frame.height * 0.5f + (fontSize_ / 4.0f);
            const Color textColor = index == static_cast<std::size_t>(std::round(indicatorAnim_))
                ? Color(1.0f, 1.0f, 1.0f, 1.0f)
                : CurrentTheme->text;
            Renderer::DrawTextStr(items_[index], textX, textY, ApplyOpacity(textColor, primitive_.opacity), textScale);
        }
    }

protected:
    void resetDefaults() override {
        primitive_ = UIPrimitive{};
        primitive_.width = 300.0f;
        primitive_.height = 35.0f;
        items_.clear();
        selectedIndex_ = 0;
        fontSize_ = 20.0f;
        onChange_ = {};
    }

private:
    void requestRepaint(float expand = 4.0f, float duration = 0.0f) {
        (void)expand;
        requestVisualRepaint(duration);
    }

    std::vector<std::string> items_;
    int selectedIndex_ = 0;
    float fontSize_ = 20.0f;
    std::function<void(int)> onChange_;
    float indicatorAnim_ = 0.0f;
    bool indicatorReady_ = false;
    std::size_t lastItemCount_ = 0;
};

} // namespace EUINEO
