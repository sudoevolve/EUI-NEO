#pragma once
#include "modules/plot/renderer.h"
#include <algorithm>
#include <cmath>

namespace modules::plot::detail {
inline std::vector<std::uint8_t> rasterize(const std::vector<Batch>& batches, int width, int height,
                                           double dpi, bool bottomUp = false) {
    const std::size_t pixels = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    std::vector<std::uint8_t> rgba(pixels * 4);
    for (std::size_t index = 0; index < pixels; ++index) {
        rgba[index * 4] = 10;
        rgba[index * 4 + 1] = 13;
        rgba[index * 4 + 2] = 18;
        rgba[index * 4 + 3] = 255;
    }
    const auto edge = [](double ax, double ay, double bx, double by, double x, double y) {
        return (x - ax) * (by - ay) - (y - ay) * (bx - ax);
    };
    for (const auto& batch : batches) {
        const double alpha = batch.color[3];
        if (alpha <= 0)
            continue;
        const double inverseAlpha = 1.0 - alpha;
        const double sourceRed = batch.color[0] * alpha * 255.0;
        const double sourceGreen = batch.color[1] * alpha * 255.0;
        const double sourceBlue = batch.color[2] * alpha * 255.0;
        for (std::size_t triangle = 0; triangle < batch.vertices.size(); triangle += 3) {
            const auto& a = batch.vertices[triangle];
            const auto& b = batch.vertices[triangle + 1];
            const auto& c = batch.vertices[triangle + 2];
            // 固定子像素网格使相邻三角形的反向边得到相同的边界判定。
            const auto snap = [dpi](float coordinate) {
                return std::round(double(coordinate) * dpi * 256) / 256;
            };
            const double ax = snap(a.x), ay = snap(a.y);
            double bx = snap(b.x), by = snap(b.y);
            double cx = snap(c.x), cy = snap(c.y);
            const double area = edge(ax, ay, bx, by, cx, cy);
            if (std::abs(area) < 1e-12)
                continue;
            if (area < 0) {
                std::swap(bx, cx);
                std::swap(by, cy);
            }
            const auto topLeft = [](double x0, double y0, double x1, double y1) {
                return y1 > y0 || (y1 == y0 && x1 < x0);
            };
            const bool edge0Inclusive = topLeft(bx, by, cx, cy);
            const bool edge1Inclusive = topLeft(cx, cy, ax, ay);
            const bool edge2Inclusive = topLeft(ax, ay, bx, by);
            const int left =
                static_cast<int>(std::clamp(std::floor(std::min({ax, bx, cx})), 0.0, double(width - 1)));
            const int right =
                static_cast<int>(std::clamp(std::ceil(std::max({ax, bx, cx})), 0.0, double(width)));
            const int top =
                static_cast<int>(std::clamp(std::floor(std::min({ay, by, cy})), 0.0, double(height - 1)));
            const int bottom =
                static_cast<int>(std::clamp(std::ceil(std::max({ay, by, cy})), 0.0, double(height)));
            const double startX = left + 0.5;
            const double startY = top + 0.5;
            double rowEdge0 = edge(bx, by, cx, cy, startX, startY);
            double rowEdge1 = edge(cx, cy, ax, ay, startX, startY);
            double rowEdge2 = edge(ax, ay, bx, by, startX, startY);
            const double edge0StepX = cy - by;
            const double edge1StepX = ay - cy;
            const double edge2StepX = by - ay;
            const double edge0StepY = bx - cx;
            const double edge1StepY = cx - ax;
            const double edge2StepY = ax - bx;
            for (int y = top; y < bottom; ++y) {
                double e0 = rowEdge0;
                double e1 = rowEdge1;
                double e2 = rowEdge2;
                for (int x = left; x < right; ++x) {
                    if ((e0 > 0 || (e0 == 0 && edge0Inclusive)) && (e1 > 0 || (e1 == 0 && edge1Inclusive)) &&
                        (e2 > 0 || (e2 == 0 && edge2Inclusive))) {
                        const auto row = bottomUp ? height - 1 - y : y;
                        const std::size_t offset = (static_cast<std::size_t>(row) * width + x) * 4;
                        rgba[offset] =
                            static_cast<std::uint8_t>(sourceRed + rgba[offset] * inverseAlpha + 0.5);
                        rgba[offset + 1] =
                            static_cast<std::uint8_t>(sourceGreen + rgba[offset + 1] * inverseAlpha + 0.5);
                        rgba[offset + 2] =
                            static_cast<std::uint8_t>(sourceBlue + rgba[offset + 2] * inverseAlpha + 0.5);
                    }
                    e0 += edge0StepX;
                    e1 += edge1StepX;
                    e2 += edge2StepX;
                }
                rowEdge0 += edge0StepY;
                rowEdge1 += edge1StepY;
                rowEdge2 += edge2StepY;
            }
        }
    }
    return rgba;
}
} // namespace modules::plot::detail
