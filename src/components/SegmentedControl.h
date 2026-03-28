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

    bool wantsContinuousUpdate() const override {
        if (items_.empty()) {
            return false;
        }
        return !indicatorReady_ || indicatorProgress_ < 1.0f;
    }

    void update() override {
        int clampedSelected = items_.empty()
            ? 0
            : std::clamp(selectedIndex_, 0, static_cast<int>(items_.size()) - 1);

        if (!indicatorReady_ || lastItemCount_ != items_.size()) {
            indicatorAnim_ = static_cast<float>(clampedSelected);
            indicatorStart_ = indicatorAnim_;
            indicatorTarget_ = indicatorAnim_;
            indicatorProgress_ = 1.0f;
            indicatorDuration_ = 0.0f;
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
            clampedSelected = selectedIndex_;
            if (onChange_) {
                onChange_(selectedIndex_);
            }
            requestRepaint(4.0f, 0.30f);
        }

        const float targetAnim = static_cast<float>(clampedSelected);
        if (std::abs(indicatorTarget_ - targetAnim) > 0.001f) {
            indicatorStart_ = indicatorAnim_;
            indicatorTarget_ = targetAnim;
            indicatorProgress_ = 0.0f;
            const float distance = std::abs(indicatorTarget_ - indicatorStart_);
            indicatorDuration_ = std::clamp(0.18f + distance * 0.10f, 0.18f, 0.34f);
        }

        if (indicatorProgress_ < 1.0f) {
            float deltaSeconds = State.deltaTime;
            if (deltaSeconds > 1.0f) {
                deltaSeconds *= 0.001f;
            }
            if (deltaSeconds <= 0.0f) {
                deltaSeconds = 1.0f / 120.0f;
            }

            const float duration = std::max(0.001f, indicatorDuration_);
            indicatorProgress_ = std::min(1.0f, indicatorProgress_ + deltaSeconds / duration);
            const float eased = EaseInOutBezier(indicatorProgress_);
            indicatorAnim_ = Lerp(indicatorStart_, indicatorTarget_, eased);
            if (indicatorProgress_ >= 1.0f || std::abs(indicatorAnim_ - indicatorTarget_) < 0.001f) {
                indicatorProgress_ = 1.0f;
                indicatorAnim_ = indicatorTarget_;
            }
            requestRepaint(4.0f, 0.30f);
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
        const float primaryLuminance = CurrentTheme->primary.r * 0.2126f +
                                       CurrentTheme->primary.g * 0.7152f +
                                       CurrentTheme->primary.b * 0.0722f;
        const Color indicatorTextColor = primaryLuminance > 0.55f
            ? Color(0.04f, 0.04f, 0.04f, 1.0f)
            : Color(1.0f, 1.0f, 1.0f, 1.0f);

        const float textScale = fontSize_ / 24.0f;
        for (std::size_t index = 0; index < items_.size(); ++index) {
            const float textWidth = Renderer::MeasureTextWidth(items_[index], textScale);
            const float textX = frame.x + static_cast<float>(index) * segmentWidth + (segmentWidth - textWidth) * 0.5f;
            const float textY = frame.y + frame.height * 0.5f + (fontSize_ / 4.0f);
            const float distance = std::abs(indicatorAnim_ - static_cast<float>(index));
            const float proximity = std::clamp(1.0f - distance, 0.0f, 1.0f);
            const float blend = proximity * proximity * (3.0f - 2.0f * proximity);
            const Color textColor = Lerp(CurrentTheme->text, indicatorTextColor, blend);
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
    static float CubicBezierAt(float t, float p1, float p2) {
        const float oneMinusT = 1.0f - t;
        return 3.0f * oneMinusT * oneMinusT * t * p1 +
               3.0f * oneMinusT * t * t * p2 +
               t * t * t;
    }

    static float EaseInOutBezier(float progress) {
        const float x = std::clamp(progress, 0.0f, 1.0f);
        constexpr float p1x = 0.30f;
        constexpr float p1y = 0.00f;
        constexpr float p2x = 0.15f;
        constexpr float p2y = 1.00f;

        float lower = 0.0f;
        float upper = 1.0f;
        for (int i = 0; i < 12; ++i) {
            const float mid = (lower + upper) * 0.5f;
            const float sampleX = CubicBezierAt(mid, p1x, p2x);
            if (sampleX < x) {
                lower = mid;
            } else {
                upper = mid;
            }
        }
        const float t = (lower + upper) * 0.5f;
        return CubicBezierAt(t, p1y, p2y);
    }

    void requestRepaint(float expand = 4.0f, float duration = 0.0f) {
        (void)expand;
        requestVisualRepaint(duration);
    }

    std::vector<std::string> items_;
    int selectedIndex_ = 0;
    float fontSize_ = 20.0f;
    std::function<void(int)> onChange_;
    float indicatorAnim_ = 0.0f;
    float indicatorStart_ = 0.0f;
    float indicatorTarget_ = 0.0f;
    float indicatorProgress_ = 1.0f;
    float indicatorDuration_ = 0.22f;
    bool indicatorReady_ = false;
    std::size_t lastItemCount_ = 0;
};

} // namespace EUINEO
