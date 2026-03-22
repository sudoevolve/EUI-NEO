#pragma once

#include "../EUINEO.h"
#include "../ui/UIBuilder.h"
#include <algorithm>
#include <cmath>
#include <string>
#include <utility>

namespace EUINEO {

// Copy this file, rename the node, then use:
// ui.node<TemplateCardNode>("demo.card")
//     .position(120.0f, 80.0f)
//     .size(220.0f, 96.0f)
//     .call(&TemplateCardNode::setTitle, std::string("CPU"))
//     .call(&TemplateCardNode::setValue, std::string("42%"))
//     .call(&TemplateCardNode::setAccent, Color(0.30f, 0.65f, 1.0f, 1.0f))
//     .build();
//
// If you later want ui.templateCard("..."), add:
// EUI_UI_COMPONENT(templateCard, TemplateCardNode)
// to src/ui/UIComponents.def

class TemplateCardNode : public UINode {
public:
    using Builder = DefaultNodeBuilder<TemplateCardNode>;

    explicit TemplateCardNode(const std::string& key) : UINode(key) {
        resetDefaults();
        liftAnimation_.Bind(&lift_);
    }

    static constexpr const char* StaticTypeName() {
        return "TemplateCardNode";
    }

    const char* typeName() const override {
        return StaticTypeName();
    }

    void setTitle(std::string value) {
        title_ = std::move(value);
    }

    void setValue(std::string value) {
        value_ = std::move(value);
    }

    void setAccent(const Color& value) {
        accent_ = value;
    }

    void update() override {
        const bool hovered = primitive_.enabled && PrimitiveContains(primitive_, State.mouseX, State.mouseY);
        if (hovered != hovered_) {
            hovered_ = hovered;
            liftAnimation_.PlayTo(hovered ? 1.0f : 0.0f, 0.16f, hovered ? Easing::EaseOut : Easing::EaseInOut);
            requestRepaint(12.0f, 0.18f);
        }

        if (liftAnimation_.Update(State.deltaTime)) {
            requestRepaint(12.0f, 0.18f);
        }
    }

    void draw() override {
        PrimitiveClipScope clip(primitive_);
        const RectFrame frame = PrimitiveFrame(primitive_);

        RectStyle panelStyle = MakeStyle(primitive_);
        panelStyle.color = ApplyOpacity(
            Lerp(CurrentTheme->surface, CurrentTheme->surfaceHover, lift_ * 0.55f),
            primitive_.opacity
        );
        panelStyle.transform.translateY += -4.0f * lift_;
        Renderer::DrawRect(frame.x, frame.y, frame.width, frame.height, panelStyle);

        Renderer::DrawRect(
            frame.x + 14.0f,
            frame.y + 16.0f - 4.0f * lift_,
            10.0f,
            frame.height - 32.0f,
            ApplyOpacity(accent_, primitive_.opacity),
            5.0f
        );

        Renderer::DrawTextStr(
            title_,
            frame.x + 36.0f,
            frame.y + 34.0f - 4.0f * lift_,
            ApplyOpacity(CurrentTheme->text, primitive_.opacity),
            16.0f / 24.0f
        );

        Renderer::DrawTextStr(
            value_,
            frame.x + 36.0f,
            frame.y + 68.0f - 4.0f * lift_,
            ApplyOpacity(accent_, primitive_.opacity),
            28.0f / 24.0f
        );
    }

protected:
    void resetDefaults() override {
        primitive_ = UIPrimitive{};
        primitive_.width = 220.0f;
        primitive_.height = 96.0f;
        primitive_.background = CurrentTheme->surface;
        primitive_.rounding = 18.0f;
        primitive_.shadow.blur = 18.0f;
        primitive_.shadow.offsetY = 8.0f;
        primitive_.shadow.color = Color(0.0f, 0.0f, 0.0f, 0.18f);
        title_ = "Title";
        value_ = "Value";
        accent_ = CurrentTheme->primary;
    }

private:
    void requestRepaint(float expand = 12.0f, float duration = 0.0f) {
        (void)expand;
        requestVisualRepaint(duration);
    }

    std::string title_;
    std::string value_;
    Color accent_ = Color(1.0f, 1.0f, 1.0f, 1.0f);
    bool hovered_ = false;
    float lift_ = 0.0f;
    FloatAnimation liftAnimation_;
};

} // namespace EUINEO
