#pragma once

#include "UINode.h"
#include <algorithm>
#include <functional>
#include <utility>

namespace EUINEO {

class UIContext;

struct LayoutBuildInfo {
    bool hasX = false;
    bool hasY = false;
    bool hasWidth = false;
    bool hasHeight = false;
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float flex = 0.0f;
    float marginLeft = 0.0f;
    float marginTop = 0.0f;
    float marginRight = 0.0f;
    float marginBottom = 0.0f;
};

void FinalizeUIBuild(UIContext& context, UINode& node, const LayoutBuildInfo& info);

template <typename NodeT, typename Derived>
class UIBuilderBase {
public:
    Derived& x(float value) {
        node_.primitive().x = value;
        layoutBuild_.hasX = true;
        layoutBuild_.x = value;
        return self();
    }

    Derived& y(float value) {
        node_.primitive().y = value;
        layoutBuild_.hasY = true;
        layoutBuild_.y = value;
        return self();
    }

    Derived& position(float xValue, float yValue) {
        node_.primitive().x = xValue;
        node_.primitive().y = yValue;
        layoutBuild_.hasX = true;
        layoutBuild_.hasY = true;
        layoutBuild_.x = xValue;
        layoutBuild_.y = yValue;
        return self();
    }

    Derived& width(float value) {
        node_.primitive().width = value;
        layoutBuild_.hasWidth = true;
        layoutBuild_.width = value;
        return self();
    }

    Derived& height(float value) {
        node_.primitive().height = value;
        layoutBuild_.hasHeight = true;
        layoutBuild_.height = value;
        return self();
    }

    Derived& size(float widthValue, float heightValue) {
        node_.primitive().width = widthValue;
        node_.primitive().height = heightValue;
        layoutBuild_.hasWidth = true;
        layoutBuild_.hasHeight = true;
        layoutBuild_.width = widthValue;
        layoutBuild_.height = heightValue;
        return self();
    }

    Derived& minWidth(float value) {
        node_.primitive().minWidth = value;
        return self();
    }

    Derived& minHeight(float value) {
        node_.primitive().minHeight = value;
        return self();
    }

    Derived& maxWidth(float value) {
        node_.primitive().maxWidth = value;
        return self();
    }

    Derived& maxHeight(float value) {
        node_.primitive().maxHeight = value;
        return self();
    }

    Derived& anchor(Anchor value) {
        node_.primitive().anchor = value;
        return self();
    }

    Derived& scale(float value) {
        node_.primitive().scaleX = value;
        node_.primitive().scaleY = value;
        return self();
    }

    Derived& scaleX(float value) {
        node_.primitive().scaleX = value;
        return self();
    }

    Derived& scaleY(float value) {
        node_.primitive().scaleY = value;
        return self();
    }

    Derived& rotation(float value) {
        node_.primitive().rotation = value;
        return self();
    }

    Derived& translateX(float value) {
        node_.primitive().translateX = value;
        return self();
    }

    Derived& translateY(float value) {
        node_.primitive().translateY = value;
        return self();
    }

    Derived& opacity(float value) {
        node_.primitive().opacity = std::clamp(value, 0.0f, 1.0f);
        return self();
    }

    Derived& flex(float value) {
        layoutBuild_.flex = std::max(0.0f, value);
        return self();
    }

    Derived& margin(float value) {
        const float clamped = std::max(0.0f, value);
        layoutBuild_.marginLeft = clamped;
        layoutBuild_.marginTop = clamped;
        layoutBuild_.marginRight = clamped;
        layoutBuild_.marginBottom = clamped;
        return self();
    }

    Derived& margin(float horizontal, float vertical) {
        layoutBuild_.marginLeft = std::max(0.0f, horizontal);
        layoutBuild_.marginRight = std::max(0.0f, horizontal);
        layoutBuild_.marginTop = std::max(0.0f, vertical);
        layoutBuild_.marginBottom = std::max(0.0f, vertical);
        return self();
    }

    Derived& margin(float left, float top, float right, float bottom) {
        layoutBuild_.marginLeft = std::max(0.0f, left);
        layoutBuild_.marginTop = std::max(0.0f, top);
        layoutBuild_.marginRight = std::max(0.0f, right);
        layoutBuild_.marginBottom = std::max(0.0f, bottom);
        return self();
    }

    Derived& clipRect(float xValue, float yValue, float widthValue, float heightValue) {
        node_.primitive().hasClipRect = widthValue > 0.0f && heightValue > 0.0f;
        node_.primitive().clipRect = UIClipRect{xValue, yValue, widthValue, heightValue};
        return self();
    }

    Derived& clearClipRect() {
        node_.primitive().hasClipRect = false;
        node_.primitive().clipRect = UIClipRect{};
        return self();
    }

    Derived& clipToParent(bool value) {
        node_.primitive().clipToParent = value;
        return self();
    }

    Derived& escapeClip() {
        node_.primitive().clipToParent = false;
        node_.primitive().hasClipRect = false;
        node_.primitive().clipRect = UIClipRect{};
        return self();
    }

    Derived& background(const Color& value) {
        node_.primitive().background = value;
        return self();
    }

    Derived& background(float r, float g, float b, float a = 1.0f) {
        node_.primitive().background = Color(r, g, b, a);
        return self();
    }

    Derived& gradient(const RectGradient& value) {
        node_.primitive().gradient = value;
        return self();
    }

    Derived& rounding(float value) {
        node_.primitive().rounding = value;
        return self();
    }

    Derived& border(float widthValue, const Color& colorValue) {
        node_.primitive().borderWidth = widthValue;
        node_.primitive().borderColor = colorValue;
        return self();
    }

    Derived& blur(float value) {
        node_.primitive().blur = value;
        return self();
    }

    Derived& shadow(float blurValue, float offsetY, const Color& colorValue) {
        node_.primitive().shadow.blur = blurValue;
        node_.primitive().shadow.offsetY = offsetY;
        node_.primitive().shadow.color = colorValue;
        return self();
    }

    Derived& shadow(float blurValue, float offsetX, float offsetY, const Color& colorValue) {
        node_.primitive().shadow.blur = blurValue;
        node_.primitive().shadow.offsetX = offsetX;
        node_.primitive().shadow.offsetY = offsetY;
        node_.primitive().shadow.color = colorValue;
        return self();
    }

    Derived& visible(bool value) {
        node_.primitive().visible = value;
        return self();
    }

    Derived& enabled(bool value) {
        node_.primitive().enabled = value;
        return self();
    }

    Derived& layer(RenderLayer value) {
        node_.primitive().renderLayer = value;
        return self();
    }

    Derived& popupLayer() {
        node_.primitive().renderLayer = RenderLayer::Popup;
        return escapeClip();
    }

    Derived& zIndex(int value) {
        node_.primitive().zIndex = value;
        node_.primitive().hasExplicitZIndex = true;
        return self();
    }

    template <typename ValueT, typename AssignedT>
    Derived& prop(ValueT NodeT::* member, AssignedT&& value) {
        if (!node_.trackComposeValue("prop", value)) {
            node_.forceComposeDirty();
        }
        node_.*member = std::forward<AssignedT>(value);
        return self();
    }

    template <typename MethodT, typename... Args>
    Derived& call(MethodT method, Args&&... args) {
        node_.trackComposeMarker("call");
        (trackArgument(args), ...);
        std::invoke(method, node_, std::forward<Args>(args)...);
        return self();
    }

    template <typename Fn>
    Derived& configure(Fn&& fn) {
        std::forward<Fn>(fn)(node_);
        node_.forceComposeDirty();
        return self();
    }

    void build() {
        FinalizeUIBuild(context_, node_, layoutBuild_);
    }

protected:
    UIBuilderBase(UIContext& context, NodeT& node) : context_(context), node_(node) {}

    Derived& self() {
        return static_cast<Derived&>(*this);
    }

    template <typename ArgT>
    void trackArgument(const ArgT& value) {
        if (!node_.trackComposeValue("arg", value)) {
            node_.forceComposeDirty();
        }
    }

    UIContext& context_;
    NodeT& node_;
    LayoutBuildInfo layoutBuild_;
};

template <typename NodeT>
class GenericNodeBuilder : public UIBuilderBase<NodeT, GenericNodeBuilder<NodeT>> {
public:
    GenericNodeBuilder(UIContext& context, NodeT& node)
        : UIBuilderBase<NodeT, GenericNodeBuilder<NodeT>>(context, node) {}
};

template <typename NodeT>
using DefaultNodeBuilder = GenericNodeBuilder<NodeT>;

} // namespace EUINEO
