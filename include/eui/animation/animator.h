#pragma once

#include <algorithm>
#include <cstdint>

#include "eui/animation/timeline.h"
#include "eui/graphics/transforms.h"

namespace eui::animation {

struct TransformOrigin2D {
    float x{0.0f};
    float y{0.0f};
};

struct TransformOrigin3D {
    float x{0.0f};
    float y{0.0f};
    float z{0.0f};
};

struct TransformAnimation2D {
    eui::graphics::Transform2D from{};
    eui::graphics::Transform2D to{};
    TransformOrigin2D origin{};
    TimelineClip clip{};
};

struct TransformAnimation3D {
    eui::graphics::Transform3D from{};
    eui::graphics::Transform3D to{};
    TransformOrigin3D origin{};
    TimelineClip clip{};
};

struct AnimatorState {
    std::uint64_t frame_index{0};
    double now_seconds{0.0};
    double delta_seconds{0.0};
};

inline float lerp_scalar(float from, float to, float t) {
    return from + (to - from) * std::clamp(t, 0.0f, 1.0f);
}

inline float evaluate_timeline_progress(const TimelineClip& clip, float elapsed_seconds) {
    const float delayed = std::max(0.0f, elapsed_seconds - std::max(0.0f, clip.delay_seconds));
    const float duration = std::max(1e-5f, clip.duration_seconds);
    return ease(clip.easing, delayed / duration);
}

inline float animate_scalar(const TimelineClip& clip, float elapsed_seconds) {
    return lerp_scalar(clip.scalar.from, clip.scalar.to, evaluate_timeline_progress(clip, elapsed_seconds));
}

inline eui::graphics::Transform2D interpolate_transform(const eui::graphics::Transform2D& from,
                                                        const eui::graphics::Transform2D& to, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return eui::graphics::Transform2D{
        lerp_scalar(from.translation_x, to.translation_x, t),
        lerp_scalar(from.translation_y, to.translation_y, t),
        lerp_scalar(from.scale_x, to.scale_x, t),
        lerp_scalar(from.scale_y, to.scale_y, t),
        lerp_scalar(from.rotation_deg, to.rotation_deg, t),
        lerp_scalar(from.origin_x, to.origin_x, t),
        lerp_scalar(from.origin_y, to.origin_y, t),
    };
}

inline eui::graphics::Transform3D interpolate_transform(const eui::graphics::Transform3D& from,
                                                        const eui::graphics::Transform3D& to, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return eui::graphics::Transform3D{
        lerp_scalar(from.translation_x, to.translation_x, t),
        lerp_scalar(from.translation_y, to.translation_y, t),
        lerp_scalar(from.translation_z, to.translation_z, t),
        lerp_scalar(from.scale_x, to.scale_x, t),
        lerp_scalar(from.scale_y, to.scale_y, t),
        lerp_scalar(from.scale_z, to.scale_z, t),
        lerp_scalar(from.rotation_x_deg, to.rotation_x_deg, t),
        lerp_scalar(from.rotation_y_deg, to.rotation_y_deg, t),
        lerp_scalar(from.rotation_z_deg, to.rotation_z_deg, t),
        lerp_scalar(from.origin_x, to.origin_x, t),
        lerp_scalar(from.origin_y, to.origin_y, t),
        lerp_scalar(from.origin_z, to.origin_z, t),
        lerp_scalar(from.perspective, to.perspective, t),
    };
}

inline eui::graphics::Transform2D animate_transform(const TransformAnimation2D& animation, float elapsed_seconds) {
    return interpolate_transform(animation.from, animation.to, evaluate_timeline_progress(animation.clip, elapsed_seconds));
}

inline eui::graphics::Transform3D animate_transform(const TransformAnimation3D& animation, float elapsed_seconds) {
    return interpolate_transform(animation.from, animation.to, evaluate_timeline_progress(animation.clip, elapsed_seconds));
}

}  // namespace eui::animation
