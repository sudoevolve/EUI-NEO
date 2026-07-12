#pragma once

#include "core/runtime/runtime_instances.h"

#include <cstddef>
#include <vector>

namespace core::dsl {

inline bool isRectAnimating(const Element& element, const runtime::RectInstance& instance) {
    const bool interactive = element.interactive && !element.disabled;
    const bool stateColorsVisible = element.hasStateColors &&
        (!closeEnough(element.color, element.hoverColor) || !closeEnough(element.color, element.pressedColor));
    return instance.hoverBlend.isMovingTo(interactive && stateColorsVisible && instance.interaction.hover ? 1.0f : 0.0f) ||
           instance.pressBlend.isMovingTo(interactive && stateColorsVisible && instance.interaction.pressed ? 1.0f : 0.0f) ||
           instance.frame.isActive() ||
           instance.color.isActive() ||
           instance.radius.isActive() ||
           instance.blur.isActive() ||
           instance.opacity.isActive() ||
           instance.border.isActive() ||
           instance.shadow.isActive() ||
           instance.transform.isActive();
}

inline bool isPolygonAnimating(const Element& element, const runtime::PolygonInstance& instance) {
    const bool interactive = element.interactive && !element.disabled;
    const bool stateColorsVisible = element.hasStateColors &&
        (!closeEnough(element.color, element.hoverColor) || !closeEnough(element.color, element.pressedColor));
    return instance.hoverBlend.isMovingTo(interactive && stateColorsVisible && instance.interaction.hover ? 1.0f : 0.0f) ||
           instance.pressBlend.isMovingTo(interactive && stateColorsVisible && instance.interaction.pressed ? 1.0f : 0.0f) ||
           instance.frame.isActive() ||
           instance.color.isActive() ||
           instance.radius.isActive() ||
           instance.opacity.isActive() ||
           instance.transform.isActive();
}

inline bool isTextAnimating(const runtime::TextInstance& instance) {
    return instance.frame.isActive() ||
           instance.color.isActive() ||
           instance.opacity.isActive() ||
           instance.transform.isActive();
}

inline bool isImageAnimating(const runtime::ImageInstance& instance) {
    return instance.frame.isActive() ||
           instance.tint.isActive() ||
           instance.radius.isActive() ||
           instance.opacity.isActive() ||
           instance.transform.isActive() ||
           instance.primitive->isAnimating() ||
           instance.primitive->hasPendingLoad();
}

inline bool isVideoAnimating(const runtime::VideoInstance& instance) {
    return instance.frame.isActive() ||
           instance.tint.isActive() ||
           instance.radius.isActive() ||
           instance.opacity.isActive() ||
           instance.transform.isActive();
}

inline bool isLayoutAnimating(const runtime::LayoutInstance& instance) {
    return instance.transform.isActive() ||
           instance.opacity.isActive();
}

inline bool shouldAnimate(const Element& element, AnimProperty property) {
    return element.transition.enabled && hasAnimProperty(element.transition.properties, property);
}

inline bool shouldAnimateFrame(const Element& element) {
    return element.transition.enabled &&
           hasAnimProperty(element.transition.properties, AnimProperty::Frame) &&
           element.explicitFrameAnimation;
}

inline bool samePoints(const std::vector<Vec2>& left, const std::vector<Vec2>& right) {
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.size(); ++index) {
        if (!closeEnough(left[index], right[index])) {
            return false;
        }
    }
    return true;
}

} // namespace core::dsl
