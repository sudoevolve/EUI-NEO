#pragma once

#include <cstdint>

#include "eui/animation/easing.h"

namespace eui::animation {

enum class PropertyKind : std::uint8_t {
    opacity,
    color,
    position,
    size,
    radius,
    blur,
    shadow,
    transform_2d,
    transform_3d,
    custom_scalar,
};

struct ScalarTrack {
    float from{0.0f};
    float to{0.0f};
};

struct TimelineClip {
    const char* id{nullptr};
    PropertyKind property{PropertyKind::custom_scalar};
    ScalarTrack scalar{};
    float duration_seconds{0.2f};
    float delay_seconds{0.0f};
    CubicBezier easing{};
};

}  // namespace eui::animation
