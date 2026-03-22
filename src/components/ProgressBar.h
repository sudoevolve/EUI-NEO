#pragma once

#include "../EUINEO.h"
#include "../ui/UIBuilder.h"
#include <algorithm>
#include <cmath>

namespace EUINEO {

class ProgressBarNode : public UINode {
public:
    class Builder : public UIBuilderBase<ProgressBarNode, Builder> {
    public:
        Builder(UIContext& context, ProgressBarNode& node) : UIBuilderBase<ProgressBarNode, Builder>(context, node) {}

        Builder& value(float nextValue) {
            this->node_.trackComposeValue("value", nextValue);
            this->node_.value_ = nextValue;
            return *this;
        }
    };

    explicit ProgressBarNode(const std::string& key) : UINode(key) {
        resetDefaults();
    }

    static constexpr const char* StaticTypeName() {
        return "ProgressBarNode";
    }

    const char* typeName() const override {
        return StaticTypeName();
    }

    void update() override {
        const float targetValue = std::clamp(value_, 0.0f, 1.0f);
        if (animatedValue_ < 0.0f) {
            animatedValue_ = targetValue;
        }
        if (std::abs(animatedValue_ - targetValue) > 0.001f) {
            animatedValue_ = Lerp(animatedValue_, targetValue, 10.0f * State.deltaTime);
            requestRepaint(2.0f);
        }
    }

    void draw() override {
        PrimitiveClipScope clip(primitive_);
        const RectFrame frame = PrimitiveFrame(primitive_);
        const float cornerRadius = frame.height * 0.5f;

        Renderer::DrawRect(
            frame.x,
            frame.y,
            frame.width,
            frame.height,
            ApplyOpacity(CurrentTheme->surfaceHover, primitive_.opacity),
            cornerRadius
        );

        if (animatedValue_ > 0.01f) {
            Renderer::DrawRect(
                frame.x,
                frame.y,
                frame.width * std::clamp(animatedValue_, 0.0f, 1.0f),
                frame.height,
                ApplyOpacity(CurrentTheme->primary, primitive_.opacity),
                cornerRadius
            );
        }
    }

protected:
    void resetDefaults() override {
        primitive_ = UIPrimitive{};
        primitive_.width = 300.0f;
        primitive_.height = 15.0f;
        value_ = 0.0f;
    }

private:
    void requestRepaint(float expand = 2.0f, float duration = 0.0f) {
        (void)expand;
        requestVisualRepaint(duration);
    }

    float value_ = 0.0f;
    float animatedValue_ = -1.0f;
};

} // namespace EUINEO
