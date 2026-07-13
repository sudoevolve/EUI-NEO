#pragma once

#include "core/runtime/runtime_geometry.h"
#include "core/runtime/runtime_instances.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>

namespace core::dsl {

inline bool ownsScrollState(const Element& element) {
    return !element.scrollStateId.empty() && element.id == element.scrollStateId;
}

inline bool ownsSliderState(const Element& element) {
    return !element.sliderStateId.empty() && element.id == element.sliderStateId;
}

inline constexpr float scrollTransformMinActiveOffset() {
    return 0.01f;
}

inline constexpr float scrollInertiaFriction() {
    return 12.0f;
}

inline constexpr float scrollInertiaStopVelocity() {
    return 2.0f;
}

inline constexpr float scrollInertiaMaxVelocity() {
    return 7200.0f;
}

inline constexpr float scrollInertiaMaxDeltaSeconds() {
    return 1.0f / 30.0f;
}

inline bool scrollMotionActive(float velocity) {
    return std::fabs(velocity) > scrollInertiaStopVelocity();
}

inline float addScrollImpulse(float velocity, float distance) {
    if (velocity * distance < 0.0f) {
        velocity = 0.0f;
    }
    const float impulse = distance * scrollInertiaFriction();
    return std::clamp(velocity + impulse, -scrollInertiaMaxVelocity(), scrollInertiaMaxVelocity());
}

struct ScrollMotionStep {
    float offset = 0.0f;
    float velocity = 0.0f;
    bool active = false;
};

inline ScrollMotionStep advanceScrollMotion(float offset,
                                            float maxOffset,
                                            float velocity,
                                            float deltaSeconds) {
    maxOffset = std::max(0.0f, maxOffset);
    offset = std::clamp(offset, 0.0f, maxOffset);
    if (maxOffset <= 0.0f || !scrollMotionActive(velocity)) {
        return {offset, 0.0f, false};
    }

    const float dt = std::clamp(deltaSeconds, 0.0f, scrollInertiaMaxDeltaSeconds());
    if (dt <= 0.0f) {
        return {offset, velocity, true};
    }

    const float friction = scrollInertiaFriction();
    const float decay = std::exp(-friction * dt);
    const float distance = velocity * (1.0f - decay) / friction;
    const float unclampedOffset = offset + distance;
    const float nextOffset = std::clamp(unclampedOffset, 0.0f, maxOffset);
    const bool hitBoundary = unclampedOffset < 0.0f || unclampedOffset > maxOffset;
    float nextVelocity = hitBoundary ? 0.0f : velocity * decay;
    if (!scrollMotionActive(nextVelocity)) {
        nextVelocity = 0.0f;
    }
    return {nextOffset, nextVelocity, scrollMotionActive(nextVelocity)};
}

inline void syncOwnedScrollState(const Element& element, runtime::ScrollStateInstance& instance) {
    instance.maxOffset = std::max(0.0f, element.scrollMaxOffset);
    instance.step = std::max(1.0f, element.scrollStep);
    instance.dirtyRect = {element.frame.x, element.frame.y, element.frame.width, element.frame.height};
    instance.hasDirtyRect = true;
    if (!instance.initialized) {
        instance.offset = std::clamp(element.scrollOffset, 0.0f, instance.maxOffset);
        instance.initialized = true;
    } else {
        instance.offset = std::clamp(instance.offset, 0.0f, instance.maxOffset);
    }
    if (instance.maxOffset <= 0.0f ||
        (instance.offset <= 0.0f && instance.velocity < 0.0f) ||
        (instance.offset >= instance.maxOffset && instance.velocity > 0.0f)) {
        instance.velocity = 0.0f;
    }
}

inline void syncOwnedSliderState(const Element& element, runtime::SliderStateInstance& instance) {
    instance.width = std::max(0.0f, element.sliderWidth);
    instance.knobSize = std::max(0.0f, element.sliderKnobSize);
    instance.dirtyRect = {element.frame.x, element.frame.y, element.frame.width, element.frame.height};
    instance.hasDirtyRect = true;
    if (!instance.initialized || !instance.dragging) {
        instance.value = std::clamp(element.sliderValue, 0.0f, 1.0f);
        instance.initialized = true;
    }
}

inline float sliderValueFromPointer(const Element& owner, double pointerX, float dpiScale) {
    const Rect bounds = toPixelRect(owner.frame, dpiScale);
    const float localX = static_cast<float>(pointerX - bounds.x);
    return std::clamp(localX / std::max(1.0f, bounds.width), 0.0f, 1.0f);
}

inline Transform pointerRuntimeTransformForElement(const Element& element,
                                                   const Element* source,
                                                   double pointerX,
                                                   double pointerY,
                                                   float dpiScale,
                                                   const std::string& hoverTargetId) {
    Transform result = element.transform;
    if (element.pointerRuntimeSourceId.empty() ||
        element.pointerRuntimeAmount <= 0.0f ||
        source == nullptr ||
        source->disabled ||
        hoverTargetId != source->id) {
        return result;
    }

    const Rect bounds = toPixelRect(source->frame, dpiScale);
    const float localX = static_cast<float>(pointerX) - bounds.x;
    const float localY = static_cast<float>(pointerY) - bounds.y;
    const float nx = std::clamp(localX / std::max(1.0f, bounds.width), 0.0f, 1.0f) - 0.5f;
    const float ny = std::clamp(localY / std::max(1.0f, bounds.height), 0.0f, 1.0f) - 0.5f;
    result.rotateY += nx * element.pointerRuntimeMaxRotateY;
    result.rotateX += -ny * element.pointerRuntimeMaxRotateX;
    result.translate.x += nx * element.pointerRuntimeTranslate.x;
    result.translate.y += ny * element.pointerRuntimeTranslate.y;
    result.scale = {
        result.scale.x * element.pointerRuntimeHoverScale,
        result.scale.y * element.pointerRuntimeHoverScale
    };
    return result;
}

inline Transform scrollTransformForElement(const Element& element,
                                           const std::unordered_map<std::string, runtime::ScrollStateInstance>& scrollStates,
                                           Transform transform = {}) {
    if (!element.scrollContentSourceId.empty()) {
        const auto state = scrollStates.find(element.scrollContentSourceId);
        if (state != scrollStates.end() && state->second.maxOffset > 0.0f) {
            float offset = state->second.offset;
            if (std::fabs(offset) <= scrollTransformMinActiveOffset()) {
                offset = scrollTransformMinActiveOffset();
            }
            transform.translate.y -= offset;
        }
    }
    if (!element.scrollThumbSourceId.empty()) {
        const auto state = scrollStates.find(element.scrollThumbSourceId);
        if (state != scrollStates.end() && state->second.maxOffset > 0.0f) {
            const float normalized = std::clamp(state->second.offset / state->second.maxOffset, 0.0f, 1.0f);
            transform.translate.y += element.scrollThumbTravel * normalized;
        }
    }
    return transform;
}

inline Transform sliderTransformForElement(const Element& element,
                                           const std::unordered_map<std::string, runtime::SliderStateInstance>& sliderStates,
                                           Transform transform = {}) {
    if (!element.sliderKnobSourceId.empty()) {
        const auto state = sliderStates.find(element.sliderKnobSourceId);
        if (state != sliderStates.end()) {
            const float travel = std::max(0.0f, state->second.width - state->second.knobSize);
            const float centered = std::clamp(state->second.width * state->second.value - state->second.knobSize * 0.5f,
                                              0.0f,
                                              travel);
            transform.translate.x += centered;
        }
    }
    return transform;
}

inline Transform runtimeTransformForElement(const Element& element,
                                            const std::unordered_map<std::string, runtime::ScrollStateInstance>& scrollStates,
                                            const std::unordered_map<std::string, runtime::SliderStateInstance>& sliderStates,
                                            const Transform& base) {
    return sliderTransformForElement(element, sliderStates, scrollTransformForElement(element, scrollStates, base));
}

} // namespace core::dsl
