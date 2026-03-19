#pragma once

#include <cstdint>
#include <string_view>

#include "eui/graphics/effects.h"
#include "eui/graphics/transforms.h"

namespace eui::graphics {

struct Size {
    float w{0.0f};
    float h{0.0f};
};

struct Rect {
    float x{0.0f};
    float y{0.0f};
    float w{0.0f};
    float h{0.0f};
};

struct CornerRadius {
    float top_left{0.0f};
    float top_right{0.0f};
    float bottom_right{0.0f};
    float bottom_left{0.0f};
};

enum class ClipMode : std::uint8_t {
    none,
    bounds,
};

struct ClipRect {
    Rect rect{};
    ClipMode mode{ClipMode::none};
};

struct RectanglePrimitive {
    Rect rect{};
    CornerRadius radius{};
    Brush fill{};
    Stroke stroke{};
    Shadow shadow{};
    Blur blur{};
    float opacity{1.0f};
    ClipRect clip{};
    Transform2D transform_2d{};
    Transform3D transform_3d{};
};

enum class ImageFit : std::uint8_t {
    fill,
    contain,
    cover,
    stretch,
    center,
};

struct ImagePrimitive {
    Rect rect{};
    std::string_view source{};
    CornerRadius radius{};
    float opacity{1.0f};
    ImageFit fit{ImageFit::cover};
    ClipRect clip{{}, ClipMode::bounds};
    Transform2D transform_2d{};
    Transform3D transform_3d{};
};

struct IconPrimitive {
    Rect rect{};
    std::uint32_t glyph{0};
    std::string_view font_family{};
    Brush tint{};
    float opacity{1.0f};
    ClipRect clip{};
    Transform2D transform_2d{};
    Transform3D transform_3d{};
};

}  // namespace eui::graphics
