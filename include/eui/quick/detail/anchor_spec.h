#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace eui::quick::detail {

enum class AnchorUnit : std::uint8_t {
    auto_value,
    pixels,
    percent,
};

struct AnchorValue {
    AnchorUnit unit{AnchorUnit::auto_value};
    float value{0.0f};

    static AnchorValue Auto() {
        return AnchorValue{};
    }

    static AnchorValue Px(float px) {
        return AnchorValue{AnchorUnit::pixels, px};
    }

    static AnchorValue Percent(float percent) {
        return AnchorValue{AnchorUnit::percent, percent};
    }
};

enum class AnchorReference : std::uint8_t {
    parent,
    last_item,
    named,
};

struct AnchorRect {
    AnchorValue left{};
    AnchorValue top{};
    AnchorValue right{};
    AnchorValue bottom{};
    AnchorValue center_x{};
    AnchorValue center_y{};
    AnchorValue width{};
    AnchorValue height{};
    AnchorReference reference{AnchorReference::parent};
    const char* reference_id{nullptr};
    int z_index{0};
};

struct ResolvedRect {
    float x{0.0f};
    float y{0.0f};
    float w{0.0f};
    float h{0.0f};
};

inline float normalize_percent(float value) {
    if (std::fabs(value) > 1.0f) {
        return value * 0.01f;
    }
    return value;
}

inline bool has_anchor_value(const AnchorValue& value) {
    return value.unit != AnchorUnit::auto_value;
}

inline float resolve_anchor_value_px(const AnchorValue& value, float reference_span) {
    switch (value.unit) {
        case AnchorUnit::pixels:
            return value.value;
        case AnchorUnit::percent:
            return normalize_percent(value.value) * reference_span;
        case AnchorUnit::auto_value:
        default:
            return 0.0f;
    }
}

inline ResolvedRect resolve_anchor_rect(const AnchorRect& anchor, const ResolvedRect& reference) {
    const bool has_left = has_anchor_value(anchor.left);
    const bool has_right = has_anchor_value(anchor.right);
    const bool has_top = has_anchor_value(anchor.top);
    const bool has_bottom = has_anchor_value(anchor.bottom);
    const bool has_width = has_anchor_value(anchor.width);
    const bool has_height = has_anchor_value(anchor.height);
    const bool has_center_x = has_anchor_value(anchor.center_x);
    const bool has_center_y = has_anchor_value(anchor.center_y);

    const float left = resolve_anchor_value_px(anchor.left, reference.w);
    const float right = resolve_anchor_value_px(anchor.right, reference.w);
    const float top = resolve_anchor_value_px(anchor.top, reference.h);
    const float bottom = resolve_anchor_value_px(anchor.bottom, reference.h);
    const float width = resolve_anchor_value_px(anchor.width, reference.w);
    const float height = resolve_anchor_value_px(anchor.height, reference.h);
    const float center_x = resolve_anchor_value_px(anchor.center_x, reference.w);
    const float center_y = resolve_anchor_value_px(anchor.center_y, reference.h);

    float resolved_w = reference.w;
    if (has_width) {
        resolved_w = std::max(0.0f, width);
    } else if (has_left && has_right) {
        resolved_w = std::max(0.0f, reference.w - left - right);
    }

    float resolved_h = reference.h;
    if (has_height) {
        resolved_h = std::max(0.0f, height);
    } else if (has_top && has_bottom) {
        resolved_h = std::max(0.0f, reference.h - top - bottom);
    }

    float resolved_x = reference.x;
    if (has_left) {
        resolved_x = reference.x + left;
    } else if (has_right) {
        resolved_x = reference.x + reference.w - right - resolved_w;
    } else if (has_center_x) {
        resolved_x = reference.x + reference.w * 0.5f + center_x - resolved_w * 0.5f;
    }

    float resolved_y = reference.y;
    if (has_top) {
        resolved_y = reference.y + top;
    } else if (has_bottom) {
        resolved_y = reference.y + reference.h - bottom - resolved_h;
    } else if (has_center_y) {
        resolved_y = reference.y + reference.h * 0.5f + center_y - resolved_h * 0.5f;
    }

    return ResolvedRect{
        resolved_x,
        resolved_y,
        std::max(0.0f, resolved_w),
        std::max(0.0f, resolved_h),
    };
}

}  // namespace eui::quick::detail
