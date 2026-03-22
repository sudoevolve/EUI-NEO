#pragma once

#include "../EUINEO.h"
#include "../ui/UIContext.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <string>

namespace EUINEO {

class AnimationCardNode : public UINode {
public:
    class Builder : public UIBuilderBase<AnimationCardNode, Builder> {
    public:
        Builder(UIContext& context, AnimationCardNode& node)
            : UIBuilderBase<AnimationCardNode, Builder>(context, node) {}

        Builder& specIndex(int value) {
            this->node_.trackComposeValue("specIndex", value);
            this->node_.specIndex_ = value;
            return *this;
        }
    };

    explicit AnimationCardNode(const std::string& key) : UINode(key) {
        hoverAnimation_.Bind(&hoverAmount_);
        burstAnimation_.Bind(&burstAmount_);
        resetDefaults();
    }

    static constexpr const char* StaticTypeName() {
        return "AnimationCardNode";
    }

    const char* typeName() const override {
        return StaticTypeName();
    }

    void setSpecIndex(int value) {
        specIndex_ = value;
    }

    RectFrame paintBounds() const override {
        RectFrame frame = PrimitiveFrame(primitive_);
        frame.x -= 12.0f;
        frame.y -= 14.0f;
        frame.width += 24.0f;
        frame.height += 24.0f;

        if (primitive_.hasClipRect) {
            const float x1 = std::max(frame.x, primitive_.clipRect.x);
            const float y1 = std::max(frame.y, primitive_.clipRect.y);
            const float x2 = std::min(frame.x + frame.width, primitive_.clipRect.x + primitive_.clipRect.width);
            const float y2 = std::min(frame.y + frame.height, primitive_.clipRect.y + primitive_.clipRect.height);
            frame.x = x1;
            frame.y = y1;
            frame.width = std::max(0.0f, x2 - x1);
            frame.height = std::max(0.0f, y2 - y1);
        }
        return frame;
    }

    void update() override {
        const RectFrame frame = PrimitiveFrame(primitive_);
        const bool hovered = primitive_.enabled &&
                             State.mouseX >= frame.x && State.mouseX <= frame.x + frame.width &&
                             State.mouseY >= frame.y && State.mouseY <= frame.y + frame.height;
        bool needsRepaint = false;

        if (hovered != hovered_) {
            hovered_ = hovered;
            hoverAnimation_.PlayTo(hovered ? 1.0f : 0.0f, 0.18f, hovered ? Easing::EaseOut : Easing::EaseInOut);
            needsRepaint = true;
        }

        if (hovered && State.mouseClicked) {
            burstAnimation_.PlayTo(1.0f, 0.10f, Easing::EaseOut);
            burstAnimation_.Queue(0.0f, 0.18f, Easing::EaseInOut);
            needsRepaint = true;
        }

        if (hoverAnimation_.Update(State.deltaTime)) {
            needsRepaint = true;
        }
        if (burstAnimation_.Update(State.deltaTime)) {
            needsRepaint = true;
        }

        if (needsRepaint) {
            requestVisualRepaint();
        }
    }

    void draw() override {
        PrimitiveClipScope clip(primitive_);
        const RectFrame panelFrame = PrimitiveFrame(primitive_);
        const CardSpec& spec = CardSpecs()[std::clamp(specIndex_, 0, static_cast<int>(CardSpecs().size()) - 1)];
        const bool dark = CurrentTheme == &DarkTheme;
        const float hover = std::clamp(hoverAmount_, 0.0f, 1.0f);
        const float burst = std::clamp(burstAmount_, 0.0f, 1.0f);
        const float panelLift = hover * -4.0f + burst * -3.0f;
        const RectFrame sampleFrame = SampleFrame(panelFrame);
        const float sampleLift = specIndex_ == 3 ? hover * -6.0f + burst * -4.0f : 0.0f;

        Renderer::DrawRect(
            panelFrame.x,
            panelFrame.y + panelLift,
            panelFrame.width,
            panelFrame.height,
            CurrentTheme->surface,
            16.0f
        );

        Renderer::DrawTextStr(
            spec.title,
            panelFrame.x + 24.0f,
            panelFrame.y + 40.0f + panelLift,
            Color(CurrentTheme->text.r, CurrentTheme->text.g, CurrentTheme->text.b, 0.96f),
            23.0f / 24.0f
        );

        Renderer::DrawTextStr(
            spec.detailLine1,
            panelFrame.x + 24.0f,
            panelFrame.y + 72.0f + panelLift,
            Color(CurrentTheme->text.r, CurrentTheme->text.g, CurrentTheme->text.b, 0.66f),
            15.0f / 24.0f
        );

        Renderer::DrawTextStr(
            spec.detailLine2,
            panelFrame.x + 24.0f,
            panelFrame.y + 90.0f + panelLift,
            Color(CurrentTheme->text.r, CurrentTheme->text.g, CurrentTheme->text.b, 0.66f),
            15.0f / 24.0f
        );

        const float badgeScale = 14.0f / 24.0f;
        const float badgeTextWidth = Renderer::MeasureTextWidth(spec.badge, badgeScale);
        const float badgeX = panelFrame.x + 24.0f;
        const float badgeY = panelFrame.y + panelFrame.height - 38.0f + panelLift;

        Renderer::DrawRect(
            badgeX,
            badgeY,
            badgeTextWidth + 24.0f,
            24.0f,
            CurrentTheme->surfaceHover,
            10.0f
        );

        Renderer::DrawTextStr(
            spec.badge,
            badgeX + 12.0f,
            badgeY + 16.5f,
            Color(CurrentTheme->text.r, CurrentTheme->text.g, CurrentTheme->text.b, 0.82f),
            badgeScale
        );

        const SampleVisual sample = MakeSampleVisual(specIndex_, hover, burst, dark);
        RectStyle sampleStyle;
        sampleStyle.color = sample.color;
        sampleStyle.gradient = sample.gradient;
        sampleStyle.rounding = 12.0f;
        sampleStyle.transform.translateY = panelLift + sampleLift;
        sampleStyle.transform.scaleX = sample.scaleX;
        sampleStyle.transform.scaleY = sample.scaleY;
        sampleStyle.transform.rotationDegrees = sample.rotation;
        Renderer::DrawRect(sampleFrame.x, sampleFrame.y, sampleFrame.width, sampleFrame.height, sampleStyle);
    }

protected:
    void resetDefaults() override {
        primitive_ = UIPrimitive{};
        primitive_.width = 240.0f;
        primitive_.height = 180.0f;
        specIndex_ = 0;
    }

private:
    struct CardSpec {
        const char* title;
        const char* detailLine1;
        const char* detailLine2;
        const char* badge;
    };

    struct SampleVisual {
        Color color = Color(1.0f, 1.0f, 1.0f, 1.0f);
        RectGradient gradient;
        float scaleX = 1.0f;
        float scaleY = 1.0f;
        float rotation = 0.0f;
    };

    static const std::array<CardSpec, 4>& CardSpecs() {
        static const std::array<CardSpec, 4> specs{{
            {"Fade Alpha", "Use color.a to control", "the fill opacity.", "Color.a"},
            {"Uniform Scale", "Scale both axes together", "for lift and focus.", "scaleX = scaleY"},
            {"Axis Stretch", "Animate scaleX / scaleY", "for directional stretch.", "scaleX / scaleY"},
            {"Queue + Combo", "Blend move, rotate, scale", "and gradient in queue.", "PlayTo + Queue"},
        }};
        return specs;
    }

    static RectFrame SampleFrame(const RectFrame& panelFrame) {
        RectFrame frame;
        frame.width = std::min(84.0f, panelFrame.width * 0.24f);
        frame.height = std::min(56.0f, panelFrame.height * 0.30f);
        frame.x = panelFrame.x + panelFrame.width - frame.width - 26.0f;

        const float desiredY = panelFrame.y + panelFrame.height * 0.58f - frame.height * 0.5f;
        const float minY = panelFrame.y + panelFrame.height * 0.38f;
        const float maxY = panelFrame.y + panelFrame.height - frame.height - std::max(28.0f, panelFrame.height * 0.18f);
        frame.y = maxY >= minY ? std::clamp(desiredY, minY, maxY) : desiredY;
        return frame;
    }

    static SampleVisual MakeSampleVisual(int index, float hover, float burst, bool dark) {
        SampleVisual visual;
        visual.color = CurrentTheme->primary;

        if (index == 0) {
            const float baseAlpha = dark ? 0.36f : 0.48f;
            visual.color.a = Lerp(baseAlpha, 1.0f, std::clamp(hover + burst * 0.6f, 0.0f, 1.0f));
        } else if (index == 1) {
            visual.scaleX = 1.0f + hover * 0.18f + burst * 0.08f;
            visual.scaleY = 1.0f + hover * 0.18f + burst * 0.08f;
        } else if (index == 2) {
            visual.scaleX = 1.0f + hover * 0.26f + burst * 0.06f;
            visual.scaleY = 1.0f - hover * 0.22f - burst * 0.06f;
        } else {
            const float combo = std::clamp(hover + burst * 0.8f, 0.0f, 1.0f);
            visual.scaleX = 1.0f + hover * 0.10f + burst * 0.08f;
            visual.scaleY = 1.0f + hover * 0.10f + burst * 0.08f;
            visual.rotation = hover * 10.0f + burst * 8.0f;
            visual.gradient = RectGradient::Vertical(
                Lerp(CurrentTheme->primary, Color(1.0f, 1.0f, 1.0f, 1.0f), dark ? 0.10f + combo * 0.14f : 0.20f + combo * 0.04f),
                Lerp(CurrentTheme->primary, Color(0.0f, 0.0f, 0.0f, 1.0f), dark ? 0.18f + combo * 0.06f : 0.10f + combo * 0.04f)
            );
        }

        return visual;
    }

    int specIndex_ = 0;
    bool hovered_ = false;
    float hoverAmount_ = 0.0f;
    float burstAmount_ = 0.0f;
    FloatAnimation hoverAnimation_;
    FloatAnimation burstAnimation_;
};

class AnimationPage {
public:
    struct Layout {
        float gap = 18.0f;
        float topOffset = 98.0f;
        int columns = 1;
        float cardWidth = 0.0f;
        float cardHeight = 0.0f;
    };

    static void Compose(UIContext& ui, const std::string& idPrefix, const RectFrame& bounds) {
        if (bounds.width <= 0.0f || bounds.height <= 0.0f) {
            return;
        }

        const Layout layout = MakeLayout(bounds);

        ui.label(idPrefix + ".title")
            .text("Animation Page")
            .position(bounds.x, bounds.y + 24.0f)
            .fontSize(31.0f)
            .color(Color(CurrentTheme->text.r, CurrentTheme->text.g, CurrentTheme->text.b, 0.98f))
            .build();

        ui.label(idPrefix + ".subtitle")
            .text("Hover cards to preview rect property tracks. Click for a queued burst.")
            .position(bounds.x, bounds.y + 54.0f)
            .fontSize(17.0f)
            .color(Color(CurrentTheme->text.r, CurrentTheme->text.g, CurrentTheme->text.b, 0.72f))
            .build();

        for (int index = 0; index < 4; ++index) {
            const RectFrame frame = CardFrame(bounds, layout, index);
            ui.node<AnimationCardNode>(idPrefix + ".card" + std::to_string(index))
                .position(frame.x, frame.y)
                .size(frame.width, frame.height)
                .call(&AnimationCardNode::setSpecIndex, index)
                .build();
        }
    }

private:
    static Layout MakeLayout(const RectFrame& bounds) {
        Layout layout;
        layout.columns = bounds.width >= 520.0f ? 2 : 1;
        layout.cardWidth = layout.columns == 2 ? (bounds.width - layout.gap) * 0.5f : bounds.width;

        const float availableHeight = std::max(240.0f, bounds.height - layout.topOffset - layout.gap);
        layout.cardHeight = layout.columns == 2
            ? std::clamp((availableHeight - layout.gap) * 0.5f, 170.0f, 210.0f)
            : std::clamp((availableHeight - layout.gap * 3.0f) * 0.25f, 116.0f, 152.0f);
        return layout;
    }

    static RectFrame CardFrame(const RectFrame& bounds, const Layout& layout, int index) {
        RectFrame frame;
        const int col = index % layout.columns;
        const int row = index / layout.columns;
        frame.x = bounds.x + col * (layout.cardWidth + layout.gap);
        frame.y = bounds.y + layout.topOffset + row * (layout.cardHeight + layout.gap);
        frame.width = layout.cardWidth;
        frame.height = layout.cardHeight;
        return frame;
    }
};

} // namespace EUINEO
