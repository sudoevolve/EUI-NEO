#pragma once

#include "core/dsl.h"
#include "core/render/primitive_geometry.h"

#include <cstddef>
#include <vector>

namespace core::dsl {

inline std::vector<Vec2> hitTestPolygonPoints(const Element& element) {
    if (element.radius <= 0.001f) {
        return element.polygonPoints;
    }
    return core::render::roundedPolygonPoints(element.polygonPoints, element.radius);
}

inline bool polygonContains(const Element& element, double pointX, double pointY, float dpiScale, const Rect& bounds) {
    if (element.strokeWidth > 0) {
        const double x = (pointX - bounds.x) / dpiScale, y = (pointY - bounds.y) / dpiScale;
        const double radius = element.strokeWidth * .5;
        for (std::size_t i = 1; i < element.polygonPoints.size(); ++i) {
            const auto a = element.polygonPoints[i - 1], b = element.polygonPoints[i];
            if (!std::isfinite(a.x) || !std::isfinite(a.y) || !std::isfinite(b.x) || !std::isfinite(b.y)) continue;
            const double dx = b.x - a.x, dy = b.y - a.y, length = dx * dx + dy * dy;
            if (length <= .00000001) continue;
            const double t = std::clamp(((x - a.x) * dx + (y - a.y) * dy) / length, 0., 1.);
            const double ex = x - a.x - t * dx, ey = y - a.y - t * dy;
            if (ex * ex + ey * ey <= radius * radius) return true;
        }
        return false;
    }
    const std::vector<Vec2> points = hitTestPolygonPoints(element);
    if (points.size() < 3 || !bounds.contains(pointX, pointY)) {
        return false;
    }

    bool inside = false;
    const double localX = pointX - bounds.x;
    const double localY = pointY - bounds.y;
    std::size_t previous = points.size() - 1;
    for (std::size_t current = 0; current < points.size(); ++current) {
        const Vec2& a = points[current];
        const Vec2& b = points[previous];
        const double ax = static_cast<double>(a.x) * dpiScale;
        const double ay = static_cast<double>(a.y) * dpiScale;
        const double bx = static_cast<double>(b.x) * dpiScale;
        const double by = static_cast<double>(b.y) * dpiScale;
        const double denominator = by - ay;
        const bool crosses = ((ay > localY) != (by > localY)) &&
            (localX < (bx - ax) * (localY - ay) / denominator + ax);
        if (crosses) {
            inside = !inside;
        }
        previous = current;
    }
    return inside;
}

} // namespace core::dsl
