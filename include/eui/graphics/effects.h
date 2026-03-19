#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace eui::graphics {

struct Color {
    float r{0.0f};
    float g{0.0f};
    float b{0.0f};
    float a{1.0f};
};

struct Point {
    float x{0.0f};
    float y{0.0f};
};

struct ColorStop {
    float position{0.0f};
    Color color{};
};

enum class BrushKind : std::uint8_t {
    none,
    solid,
    linear_gradient,
    radial_gradient,
};

struct LinearGradient {
    Point start{};
    Point end{};
    std::array<ColorStop, 4> stops{};
    std::size_t stop_count{0};
};

struct RadialGradient {
    Point center{};
    float radius{0.0f};
    std::array<ColorStop, 4> stops{};
    std::size_t stop_count{0};
};

struct Brush {
    BrushKind kind{BrushKind::none};
    Color solid{};
    LinearGradient linear{};
    RadialGradient radial{};
};

struct Stroke {
    float width{0.0f};
    Brush brush{};
};

struct Shadow {
    float offset_x{0.0f};
    float offset_y{0.0f};
    float blur_radius{0.0f};
    float spread{0.0f};
    Color color{};
};

struct Blur {
    float radius{0.0f};
    float backdrop_radius{0.0f};
};

}  // namespace eui::graphics
