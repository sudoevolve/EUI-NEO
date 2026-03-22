#pragma once

#include "UINode.h"
#include <algorithm>
#include <functional>
#include <utility>

namespace EUINEO {

class UIContext;

template <typename NodeT, typename Derived>
class UIBuilderBase {
public:
    Derived& x(float value) {
        node_.primitive().x = value;
        return self();
    }

    Derived& y(float value) {
        node_.primitive().y = value;
        return self();
    }

    Derived& position(float xValue, float yValue) {
        node_.primitive().x = xValue;
        node_.primitive().y = yValue;
        return self();
    }

    Derived& width(float value) {
        node_.primitive().width = value;
        return self();
    }

    Derived& height(float value) {
        node_.primitive().height = value;
        return self();
    }

    Derived& size(float widthValue, float heightValue) {
        node_.primitive().width = widthValue;
        node_.primitive().height = heightValue;
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

    Derived& zIndex(int value) {
        node_.primitive().zIndex = value;
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
        node_.finishCompose();
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
