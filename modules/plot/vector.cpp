#include "modules/plot/vector.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace modules::plot {
namespace {
bool validPositive(double value) { return std::isfinite(value) && value > 0; }
Point add(Point a, Point b, double factor) { return {a.x + b.x * factor, a.y + b.y * factor}; }
double magnitude(Point value) { return std::hypot(value.x, value.y); }
void appendSegment(std::vector<Vertex>& output, Point a, Point b, const Axes& axes, Viewport viewport, double width) {
    const auto mappedA = axes.toScreen(a, viewport);
    const auto mappedB = axes.toScreen(b, viewport);
    if (!mappedA || !mappedB)
        return;
    const double dx = mappedB->x - mappedA->x, dy = mappedB->y - mappedA->y;
    const double length = std::hypot(dx, dy);
    if (length == 0)
        return;
    const double nx = -dy / length * width / 2, ny = dx / length * width / 2;
    const Vertex points[] = {{float(mappedA->x - viewport.x + nx), float(mappedA->y - viewport.y + ny)},
                             {float(mappedB->x - viewport.x + nx), float(mappedB->y - viewport.y + ny)},
                             {float(mappedB->x - viewport.x - nx), float(mappedB->y - viewport.y - ny)},
                             {float(mappedA->x - viewport.x - nx), float(mappedA->y - viewport.y - ny)}};
    for (const int index : {0, 1, 2, 0, 2, 3})
        output.push_back(points[index]);
}
} // namespace

VectorField::VectorField(RectilinearField x, RectilinearField y) : x_(std::move(x)), y_(std::move(y)) {
    if (x_.x() != y_.x() || x_.y() != y_.y())
        throw std::invalid_argument("plot: vector components require identical grids");
}
const RectilinearField& VectorField::xComponent() const noexcept { return x_; }
const RectilinearField& VectorField::yComponent() const noexcept { return y_; }
std::optional<Point> VectorField::sample(Point position) const {
    const auto x = x_.interpolate(position), y = y_.interpolate(position);
    if (!x || !y || !std::isfinite(*x) || !std::isfinite(*y))
        return std::nullopt;
    return Point{*x, *y};
}
std::vector<VectorArrow> vectorArrows(const VectorField& field, double scale, double minimumMagnitude) {
    if (!validPositive(scale) || !std::isfinite(minimumMagnitude) || minimumMagnitude < 0)
        throw std::invalid_argument("plot: invalid vector arrow parameters");
    std::vector<VectorArrow> result;
    for (std::size_t row = 0; row < field.xComponent().rows(); ++row)
        for (std::size_t column = 0; column < field.xComponent().columns(); ++column) {
            const Point origin{field.xComponent().x()[column], field.xComponent().y()[row]};
            const auto value = field.sample(origin);
            if (!value || magnitude(*value) < minimumMagnitude)
                continue;
            result.push_back({origin, add(origin, *value, scale), row * field.xComponent().columns() + column});
        }
    return result;
}
std::vector<Streamline> streamlines(const VectorField& field, const std::vector<Point>& seeds, double step,
                                    std::size_t maxSteps, double minimumMagnitude, bool bothDirections) {
    if (!validPositive(step) || maxSteps == 0 || !std::isfinite(minimumMagnitude) || minimumMagnitude < 0)
        throw std::invalid_argument("plot: invalid streamline parameters");
    std::vector<Streamline> result;
    const auto integrate = [&](Point seed, double direction, std::size_t sourceIndex) {
        Streamline line{sourceIndex, {seed}};
        Point current = seed;
        for (std::size_t index = 0; index < maxSteps; ++index) {
            const auto initial = field.sample(current);
            if (!initial || magnitude(*initial) < minimumMagnitude)
                break;
            const auto k1 = *initial;
            const auto k2 = field.sample(add(current, k1, direction * step / 2));
            const auto k3 = k2 ? field.sample(add(current, *k2, direction * step / 2)) : std::nullopt;
            const auto k4 = k3 ? field.sample(add(current, *k3, direction * step)) : std::nullopt;
            if (!k2 || !k3 || !k4)
                break;
            current.x += direction * step * (k1.x + 2 * k2->x + 2 * k3->x + k4->x) / 6;
            current.y += direction * step * (k1.y + 2 * k2->y + 2 * k3->y + k4->y) / 6;
            if (!field.sample(current))
                break;
            line.points.push_back(current);
        }
        return line;
    };
    for (std::size_t index = 0; index < seeds.size(); ++index) {
        if (!field.sample(seeds[index]))
            continue;
        if (bothDirections) {
            auto reverse = integrate(seeds[index], -1, index);
            std::reverse(reverse.points.begin(), reverse.points.end());
            reverse.points.pop_back();
            auto forward = integrate(seeds[index], 1, index);
            reverse.points.insert(reverse.points.end(), forward.points.begin(), forward.points.end());
            result.push_back(std::move(reverse));
        } else {
            result.push_back(integrate(seeds[index], 1, index));
        }
    }
    return result;
}
std::vector<Vertex> arrowGeometry(const std::vector<VectorArrow>& arrows, const Axes& axes, Viewport viewport,
                                  double width, double headLength, double headWidth) {
    if (!validPositive(width) || !validPositive(headLength) || !validPositive(headWidth))
        throw std::invalid_argument("plot: invalid arrow geometry parameters");
    std::vector<Vertex> result;
    for (const auto& arrow : arrows) {
        appendSegment(result, arrow.from, arrow.to, axes, viewport, width);
        const double dx = arrow.to.x - arrow.from.x, dy = arrow.to.y - arrow.from.y;
        const double length = std::hypot(dx, dy);
        if (length == 0)
            continue;
        const Point normal{-dy / length * headWidth, dx / length * headWidth};
        const Point base{arrow.to.x - dx / length * headLength, arrow.to.y - dy / length * headLength};
        appendSegment(result, arrow.to, add(base, normal, 1), axes, viewport, width);
        appendSegment(result, arrow.to, add(base, normal, -1), axes, viewport, width);
    }
    return result;
}
std::vector<Vertex> streamlineGeometry(const Streamline& line, const Axes& axes, Viewport viewport, double width) {
    if (!validPositive(width))
        throw std::invalid_argument("plot: invalid streamline width");
    std::vector<Vertex> result;
    for (std::size_t index = 1; index < line.points.size(); ++index)
        appendSegment(result, line.points[index - 1], line.points[index], axes, viewport, width);
    return result;
}
} // namespace modules::plot
