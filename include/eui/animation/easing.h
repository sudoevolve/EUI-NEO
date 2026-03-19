#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace eui::animation {

struct CubicBezier {
    float x1{0.25f};
    float y1{0.10f};
    float x2{0.25f};
    float y2{1.00f};
};

enum class EasingPreset : std::uint8_t {
    linear,
    ease,
    ease_in,
    ease_out,
    ease_in_out,
    spring_soft,
};

inline CubicBezier preset_bezier(EasingPreset preset) {
    switch (preset) {
        case EasingPreset::linear:
            return CubicBezier{0.0f, 0.0f, 1.0f, 1.0f};
        case EasingPreset::ease:
            return CubicBezier{0.25f, 0.10f, 0.25f, 1.0f};
        case EasingPreset::ease_in:
            return CubicBezier{0.42f, 0.0f, 1.0f, 1.0f};
        case EasingPreset::ease_out:
            return CubicBezier{0.0f, 0.0f, 0.58f, 1.0f};
        case EasingPreset::ease_in_out:
            return CubicBezier{0.42f, 0.0f, 0.58f, 1.0f};
        case EasingPreset::spring_soft:
            return CubicBezier{0.20f, 0.85f, 0.25f, 1.10f};
    }
    return CubicBezier{};
}

inline float cubic_bezier_component(float p0, float p1, float p2, float p3, float t) {
    const float inv = 1.0f - t;
    return inv * inv * inv * p0 + 3.0f * inv * inv * t * p1 + 3.0f * inv * t * t * p2 + t * t * t * p3;
}

inline float cubic_bezier_derivative(float p0, float p1, float p2, float p3, float t) {
    const float inv = 1.0f - t;
    return 3.0f * inv * inv * (p1 - p0) + 6.0f * inv * t * (p2 - p1) + 3.0f * t * t * (p3 - p2);
}

inline float sample_bezier_y(const CubicBezier& bezier, float progress) {
    progress = std::clamp(progress, 0.0f, 1.0f);
    if (progress <= 0.0f || progress >= 1.0f) {
        return progress;
    }

    float t = progress;
    for (int i = 0; i < 6; ++i) {
        const float x = cubic_bezier_component(0.0f, bezier.x1, bezier.x2, 1.0f, t);
        const float dx = cubic_bezier_derivative(0.0f, bezier.x1, bezier.x2, 1.0f, t);
        if (std::fabs(dx) < 1e-5f) {
            break;
        }
        t -= (x - progress) / dx;
        t = std::clamp(t, 0.0f, 1.0f);
    }

    float low = 0.0f;
    float high = 1.0f;
    for (int i = 0; i < 8; ++i) {
        const float x = cubic_bezier_component(0.0f, bezier.x1, bezier.x2, 1.0f, t);
        if (std::fabs(x - progress) < 1e-4f) {
            break;
        }
        if (x < progress) {
            low = t;
        } else {
            high = t;
        }
        t = 0.5f * (low + high);
    }

    return std::clamp(cubic_bezier_component(0.0f, bezier.y1, bezier.y2, 1.0f, t), 0.0f, 1.0f);
}

inline float ease(const CubicBezier& bezier, float progress) {
    return sample_bezier_y(bezier, progress);
}

inline float ease(EasingPreset preset, float progress) {
    return sample_bezier_y(preset_bezier(preset), progress);
}

}  // namespace eui::animation
