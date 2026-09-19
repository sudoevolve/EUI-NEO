#include "modules/plot/field.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace modules::plot {
namespace {
bool validRange(Range range) {
    return std::isfinite(range.min) && std::isfinite(range.max) && range.min < range.max;
}
bool accepts(double value, ColorScaleMode mode) {
    return std::isfinite(value) && (mode == ColorScaleMode::Linear || value > 0);
}
bool validMap(ColorMap map) {
    return map == ColorMap::Solid || map == ColorMap::Grayscale || map == ColorMap::Viridis ||
           map == ColorMap::Plasma || map == ColorMap::Turbo;
}
std::array<float, 4> interpolate(std::array<float, 4> low, std::array<float, 4> high, double fraction) {
    std::array<float, 4> result;
    for (std::size_t i = 0; i < result.size(); ++i)
        result[i] = static_cast<float>(low[i] + (high[i] - low[i]) * fraction);
    return result;
}
std::array<float, 4> color(ColorMap map, double fraction) {
    fraction = std::clamp(fraction, 0.0, 1.0);
    if (map == ColorMap::Grayscale)
        return {static_cast<float>(fraction), static_cast<float>(fraction), static_cast<float>(fraction), 1};
    if (map == ColorMap::Solid)
        return {0.2f, 0.65f, 1, 1};
    if (map == ColorMap::Plasma)
        return interpolate({0.05f, 0.03f, 0.53f, 1}, {0.94f, 0.98f, 0.13f, 1}, fraction);
    if (map == ColorMap::Turbo)
        return interpolate({0.19f, 0.07f, 0.23f, 1}, {0.48f, 0.02f, 0.01f, 1}, fraction);
    return interpolate({0.27f, 0.0f, 0.33f, 1}, {0.99f, 0.91f, 0.14f, 1}, fraction);
}
FieldTile tile(double left, double top, double right, double bottom, Viewport viewport,
               std::array<float, 4> color) {
    const auto vertex = [&](double x, double y) {
        return Vertex{static_cast<float>(x - viewport.x), static_cast<float>(y - viewport.y)};
    };
    return {{vertex(left, top), vertex(right, top), vertex(right, bottom), vertex(left, top),
             vertex(right, bottom), vertex(left, bottom)},
            color};
}
bool strictlyIncreasing(const std::vector<double>& coordinates) {
    if (coordinates.size() < 2)
        return false;
    for (std::size_t index = 0; index < coordinates.size(); ++index)
        if (!std::isfinite(coordinates[index]) || (index && coordinates[index] <= coordinates[index - 1]))
            return false;
    return true;
}
std::optional<std::size_t> cell(const std::vector<double>& coordinates, double value) {
    if (!std::isfinite(value) || value < coordinates.front() || value > coordinates.back())
        return std::nullopt;
    if (value == coordinates.back())
        return coordinates.size() - 2;
    return static_cast<std::size_t>(std::upper_bound(coordinates.begin(), coordinates.end(), value) -
                                    coordinates.begin() - 1);
}
std::optional<Vertex> vertex(Point point, const Axes& axes, Viewport viewport) {
    const auto screen = axes.toScreen(point, viewport);
    if (!screen)
        return std::nullopt;
    return Vertex{static_cast<float>(screen->x - viewport.x), static_cast<float>(screen->y - viewport.y)};
}
FieldTile triangleTile(const std::array<Point, 3>& points, std::array<float, 4> color, const Axes& axes,
                       Viewport viewport) {
    FieldTile result;
    result.color = color;
    for (const auto point : points) {
        const auto mapped = vertex(point, axes, viewport);
        if (!mapped) {
            result.vertices.clear();
            return result;
        }
        result.vertices.push_back(*mapped);
    }
    return result;
}
} // namespace

ScalarField::ScalarField(std::size_t rows, std::size_t columns, std::vector<double> values, Range x, Range y,
                         FieldOrigin origin, FieldSampling sampling)
    : rows_(rows), columns_(columns), values_(std::move(values)), x_(x), y_(y), origin_(origin),
      sampling_(sampling) {
    if (rows == 0 || columns == 0 || rows > std::numeric_limits<std::size_t>::max() / columns ||
        values_.size() != rows * columns || !validRange(x_) || !validRange(y_))
        throw std::invalid_argument("plot: invalid scalar field dimensions or range");
    if (origin_ != FieldOrigin::LowerLeft && origin_ != FieldOrigin::UpperLeft)
        throw std::invalid_argument("plot: unknown scalar field origin");
    if (sampling_ != FieldSampling::GridPoints && sampling_ != FieldSampling::CellCenters)
        throw std::invalid_argument("plot: unknown scalar field sampling");
}
std::size_t ScalarField::rows() const noexcept { return rows_; }
std::size_t ScalarField::columns() const noexcept { return columns_; }
double ScalarField::value(std::size_t row, std::size_t column) const {
    if (row >= rows_ || column >= columns_)
        throw std::out_of_range("plot: scalar field index");
    return values_[row * columns_ + column];
}
const std::vector<double>& ScalarField::values() const noexcept { return values_; }
Range ScalarField::xRange() const noexcept { return x_; }
Range ScalarField::yRange() const noexcept { return y_; }
FieldOrigin ScalarField::origin() const noexcept { return origin_; }
FieldSampling ScalarField::sampling() const noexcept { return sampling_; }
std::optional<Range> ScalarField::finiteRange() const noexcept {
    std::optional<Range> result;
    for (const double value : values_)
        if (std::isfinite(value)) {
            if (!result)
                result = {value, value};
            else {
                result->min = std::min(result->min, value);
                result->max = std::max(result->max, value);
            }
        }
    return result;
}
RectilinearField::RectilinearField(std::vector<double> x, std::vector<double> y, std::vector<double> values)
    : x_(std::move(x)), y_(std::move(y)), values_(std::move(values)) {
    if (!strictlyIncreasing(x_) || !strictlyIncreasing(y_) || values_.size() != x_.size() * y_.size())
        throw std::invalid_argument("plot: invalid rectilinear field");
}
std::size_t RectilinearField::rows() const noexcept { return y_.size(); }
std::size_t RectilinearField::columns() const noexcept { return x_.size(); }
const std::vector<double>& RectilinearField::x() const noexcept { return x_; }
const std::vector<double>& RectilinearField::y() const noexcept { return y_; }
double RectilinearField::value(std::size_t row, std::size_t column) const {
    if (row >= rows() || column >= columns())
        throw std::out_of_range("plot: rectilinear field index");
    return values_[row * columns() + column];
}
std::optional<double> RectilinearField::interpolate(Point point) const {
    const auto column = cell(x_, point.x);
    const auto row = cell(y_, point.y);
    if (!column || !row)
        return std::nullopt;
    const double values[] = {value(*row, *column), value(*row, *column + 1), value(*row + 1, *column),
                             value(*row + 1, *column + 1)};
    for (const double value : values)
        if (!std::isfinite(value))
            return std::nullopt;
    const double horizontal = (point.x - x_[*column]) / (x_[*column + 1] - x_[*column]);
    const double vertical = (point.y - y_[*row]) / (y_[*row + 1] - y_[*row]);
    return values[0] * (1 - horizontal) * (1 - vertical) + values[1] * horizontal * (1 - vertical) +
           values[2] * (1 - horizontal) * vertical + values[3] * horizontal * vertical;
}
TriangulatedField::TriangulatedField(std::vector<Point> points, std::vector<double> values,
                                     std::vector<std::array<std::size_t, 3>> triangles)
    : points_(std::move(points)), values_(std::move(values)), triangles_(std::move(triangles)) {
    if (points_.empty() || values_.size() != points_.size() || triangles_.empty())
        throw std::invalid_argument("plot: invalid triangulated field");
    for (const auto point : points_)
        if (!std::isfinite(point.x) || !std::isfinite(point.y))
            throw std::invalid_argument("plot: nonfinite triangulated point");
    for (const auto triangle : triangles_) {
        if (triangle[0] >= points_.size() || triangle[1] >= points_.size() || triangle[2] >= points_.size() ||
            triangle[0] == triangle[1] || triangle[1] == triangle[2] || triangle[0] == triangle[2])
            throw std::invalid_argument("plot: invalid triangulated topology");
        const auto a = points_[triangle[0]], b = points_[triangle[1]], c = points_[triangle[2]];
        if ((b.x - a.x) * (c.y - a.y) == (b.y - a.y) * (c.x - a.x))
            throw std::invalid_argument("plot: degenerate triangulated cell");
    }
}
const std::vector<Point>& TriangulatedField::points() const noexcept { return points_; }
const std::vector<double>& TriangulatedField::values() const noexcept { return values_; }
const std::vector<std::array<std::size_t, 3>>& TriangulatedField::triangles() const noexcept {
    return triangles_;
}
std::optional<double> TriangulatedField::interpolate(Point point) const {
    for (const auto triangle : triangles_) {
        const auto a = points_[triangle[0]], b = points_[triangle[1]], c = points_[triangle[2]];
        const double determinant = (b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y);
        const double first = ((b.y - c.y) * (point.x - c.x) + (c.x - b.x) * (point.y - c.y)) / determinant;
        const double second = ((c.y - a.y) * (point.x - c.x) + (a.x - c.x) * (point.y - c.y)) / determinant;
        const double third = 1 - first - second;
        if (first < -1e-12 || second < -1e-12 || third < -1e-12)
            continue;
        const double values[] = {values_[triangle[0]], values_[triangle[1]], values_[triangle[2]]};
        if (!std::isfinite(values[0]) || !std::isfinite(values[1]) || !std::isfinite(values[2]))
            return std::nullopt;
        return first * values[0] + second * values[1] + third * values[2];
    }
    return std::nullopt;
}
void ColorScale::setColorMap(ColorMap map) {
    if (!validMap(map))
        throw std::invalid_argument("plot: unknown color map");
    map_ = map;
}
ColorMap ColorScale::colorMap() const noexcept { return map_; }
void ColorScale::setMode(ColorScaleMode mode) {
    if (mode != ColorScaleMode::Linear && mode != ColorScaleMode::Log10)
        throw std::invalid_argument("plot: unknown color scale mode");
    if (!automatic_ && mode == ColorScaleMode::Log10 && range_.min <= 0)
        throw std::invalid_argument("plot: logarithmic color range must be positive");
    mode_ = mode;
}
ColorScaleMode ColorScale::mode() const noexcept { return mode_; }
void ColorScale::setRange(Range range) {
    if (!validRange(range) || (mode_ == ColorScaleMode::Log10 && range.min <= 0))
        throw std::invalid_argument("plot: invalid color range");
    range_ = range;
    automatic_ = false;
}
Range ColorScale::range() const noexcept { return range_; }
void ColorScale::resetAuto() noexcept { automatic_ = true; }
bool ColorScale::automatic() const noexcept { return automatic_; }
void ColorScale::fit(const std::optional<Range>& range) {
    if (!automatic_)
        return;
    if (!range || !accepts(range->min, mode_) || !accepts(range->max, mode_) || range->min > range->max) {
        range_ = mode_ == ColorScaleMode::Log10 ? Range{1, 10} : Range{0, 1};
        return;
    }
    range_ = *range;
    if (range_.min == range_.max) {
        const double padding = mode_ == ColorScaleMode::Log10
                                   ? range_.min * 0.5
                                   : (range_.min == 0 ? 1 : std::abs(range_.min) * 0.05);
        range_.min -= padding;
        range_.max += padding;
        if (mode_ == ColorScaleMode::Log10)
            range_.min = std::max(range_.min, std::numeric_limits<double>::min());
    }
}
void ColorScale::fit(const ScalarField& field) {
    if (!automatic_)
        return;
    std::optional<Range> range;
    for (const double value : field.values())
        if (accepts(value, mode_)) {
            if (!range)
                range = {value, value};
            else {
                range->min = std::min(range->min, value);
                range->max = std::max(range->max, value);
            }
        }
    fit(range);
}
void ColorScale::setDiscreteLevels(std::size_t levels) {
    if (levels == 1 || levels > 256)
        throw std::invalid_argument("plot: discrete color levels must be zero or between 2 and 256");
    discreteLevels_ = levels;
}
std::size_t ColorScale::discreteLevels() const noexcept { return discreteLevels_; }
void ColorScale::setMissingColor(std::array<float, 4> color) {
    for (const auto component : color)
        if (!std::isfinite(component) || component < 0 || component > 1)
            throw std::invalid_argument("plot: invalid missing color");
    missingColor_ = color;
}
std::array<float, 4> ColorScale::missingColor() const noexcept { return missingColor_; }
std::array<float, 4> ColorScale::map(double value) const {
    if (!accepts(value, mode_))
        return missingColor_;
    double fraction;
    if (mode_ == ColorScaleMode::Log10)
        fraction = (std::log(value) - std::log(range_.min)) / (std::log(range_.max) - std::log(range_.min));
    else
        fraction = (value - range_.min) / (range_.max - range_.min);
    if (!std::isfinite(fraction))
        return missingColor_;
    fraction = std::clamp(fraction, 0.0, 1.0);
    if (discreteLevels_)
        fraction = std::round(fraction * (discreteLevels_ - 1)) / (discreteLevels_ - 1);
    return color(map_, fraction);
}
std::vector<FieldTile> heatmapTiles(const ScalarField& field, const ColorScale& scale, Viewport viewport) {
    Axes axes;
    axes.x.setRange(field.xRange());
    axes.y.setRange(field.yRange());
    return heatmapTiles(field, scale, axes, viewport);
}
std::optional<FieldPick> pickField(const ScalarField& field, Point p) {
    const auto xr = field.xRange(), yr = field.yRange();
    if (!std::isfinite(p.x) || !std::isfinite(p.y) || p.x < xr.min || p.x > xr.max || p.y < yr.min ||
        p.y > yr.max)
        return std::nullopt;
    Axis x, y;
    x.setRange(xr);
    y.setRange(yr);
    const double u = *x.normalize(p.x), v = *y.normalize(p.y);
    const double rowFraction = field.origin() == FieldOrigin::LowerLeft ? v : 1 - v;
    const bool centers = field.sampling() == FieldSampling::CellCenters;
    const auto index = [centers](double t, std::size_t count) {
        return std::min(count - 1, static_cast<std::size_t>(centers ? std::floor(t * count)
                                                                    : std::round(t * (count - 1))));
    };
    const auto row = index(rowFraction, field.rows()), column = index(u, field.columns());
    const auto fraction = [centers](std::size_t i, std::size_t count) {
        return centers ? (double(i) + .5) / count : (count == 1 ? .5 : double(i) / (count - 1));
    };
    const auto fx = fraction(column, field.columns()), fy = fraction(row, field.rows());
    return FieldPick{
        row,
        column,
        {*x.denormalize(fx), *y.denormalize(field.origin() == FieldOrigin::LowerLeft ? fy : 1 - fy)},
        field.value(row, column)};
}
std::vector<FieldTile> heatmapTiles(const ScalarField& field, const ColorScale& scale, const Axes& axes,
                                    Viewport viewport) {
    if (!std::isfinite(viewport.x) || !std::isfinite(viewport.y) || !std::isfinite(viewport.width) ||
        !std::isfinite(viewport.height) || viewport.width <= 0 || viewport.height <= 0)
        throw std::invalid_argument("plot: invalid heatmap viewport");
    const std::size_t rows = field.sampling() == FieldSampling::CellCenters ? field.rows() : field.rows() - 1;
    const std::size_t columns =
        field.sampling() == FieldSampling::CellCenters ? field.columns() : field.columns() - 1;
    std::vector<FieldTile> result;
    result.reserve(rows * columns);
    if (rows == 0 || columns == 0)
        return result;
    for (std::size_t row = 0; row < rows; ++row)
        for (std::size_t column = 0; column < columns; ++column) {
            double value = field.value(row, column);
            if (field.sampling() == FieldSampling::GridPoints) {
                const double samples[] = {field.value(row, column), field.value(row, column + 1),
                                          field.value(row + 1, column), field.value(row + 1, column + 1)};
                value = 0;
                for (const double sample : samples) {
                    if (!std::isfinite(sample)) {
                        value = std::numeric_limits<double>::quiet_NaN();
                        break;
                    }
                    value += sample / 4;
                }
            }
            const auto xr = field.xRange(), yr = field.yRange();
            const auto mix = [](Range r, double t) { return r.min * (1 - t) + r.max * t; };
            const auto visualRow = field.origin() == FieldOrigin::LowerLeft ? row : rows - row - 1;
            const double x0 = std::max(axes.x.range().min, mix(xr, double(column) / columns));
            const double x1 = std::min(axes.x.range().max, mix(xr, double(column + 1) / columns));
            const double y0 = std::max(axes.y.range().min, mix(yr, double(visualRow) / rows));
            const double y1 = std::min(axes.y.range().max, mix(yr, double(visualRow + 1) / rows));
            if (x0 >= x1 || y0 >= y1)
                continue;
            const auto a = axes.toScreen({x0, y0}, viewport), b = axes.toScreen({x1, y1}, viewport);
            if (a && b)
                result.push_back(tile(std::min(a->x, b->x), std::min(a->y, b->y), std::max(a->x, b->x),
                                      std::max(a->y, b->y), viewport, scale.map(value)));
        }
    return result;
}
std::vector<FieldTile> rectilinearTiles(const RectilinearField& field, const ColorScale& scale,
                                        const Axes& axes, Viewport viewport) {
    std::vector<FieldTile> result;
    result.reserve((field.rows() - 1) * (field.columns() - 1) * 2);
    for (std::size_t row = 0; row + 1 < field.rows(); ++row)
        for (std::size_t column = 0; column + 1 < field.columns(); ++column) {
            const double values[] = {field.value(row, column), field.value(row, column + 1),
                                     field.value(row + 1, column), field.value(row + 1, column + 1)};
            double average = 0;
            for (const double value : values)
                average += std::isfinite(value) ? value / 4 : std::numeric_limits<double>::quiet_NaN();
            const std::array<Point, 4> points{{{field.x()[column], field.y()[row]},
                                               {field.x()[column + 1], field.y()[row]},
                                               {field.x()[column + 1], field.y()[row + 1]},
                                               {field.x()[column], field.y()[row + 1]}}};
            const auto color = scale.map(average);
            for (const auto triangle : {std::array<Point, 3>{points[0], points[1], points[2]},
                                        std::array<Point, 3>{points[0], points[2], points[3]}}) {
                auto tile = triangleTile(triangle, color, axes, viewport);
                if (!tile.vertices.empty())
                    result.push_back(std::move(tile));
            }
        }
    return result;
}
std::vector<FieldTile> triangulatedTiles(const TriangulatedField& field, const ColorScale& scale,
                                         const Axes& axes, Viewport viewport) {
    std::vector<FieldTile> result;
    result.reserve(field.triangles().size());
    for (const auto indices : field.triangles()) {
        const double values[] = {field.values()[indices[0]], field.values()[indices[1]],
                                 field.values()[indices[2]]};
        const double average =
            std::isfinite(values[0]) && std::isfinite(values[1]) && std::isfinite(values[2])
                ? (values[0] + values[1] + values[2]) / 3
                : std::numeric_limits<double>::quiet_NaN();
        auto tile =
            triangleTile({field.points()[indices[0]], field.points()[indices[1]], field.points()[indices[2]]},
                         scale.map(average), axes, viewport);
        if (!tile.vertices.empty())
            result.push_back(std::move(tile));
    }
    return result;
}
std::vector<FieldTile> colorbarTiles(const ColorScale& scale, Viewport viewport, std::size_t segments) {
    if (!std::isfinite(viewport.x) || !std::isfinite(viewport.y) || !std::isfinite(viewport.width) ||
        !std::isfinite(viewport.height) || viewport.width <= 0 || viewport.height <= 0)
        throw std::invalid_argument("plot: invalid colorbar viewport");
    if (segments == 0 || segments > 4096)
        throw std::invalid_argument("plot: invalid colorbar segments");
    if (scale.discreteLevels())
        segments = scale.discreteLevels();
    std::vector<FieldTile> result;
    result.reserve(segments);
    const auto range = scale.range();
    for (std::size_t index = 0; index < segments; ++index) {
        const double low = double(index) / segments;
        const double high = double(index + 1) / segments;
        const double fraction = (low + high) / 2;
        const double value =
            scale.mode() == ColorScaleMode::Log10
                ? std::exp(std::log(range.min) + (std::log(range.max) - std::log(range.min)) * fraction)
                : range.min + (range.max - range.min) * fraction;
        const double top = viewport.y + viewport.height * (1 - high);
        const double bottom = viewport.y + viewport.height * (1 - low);
        result.push_back(
            tile(viewport.x, top, viewport.x + viewport.width, bottom, viewport, scale.map(value)));
    }
    return result;
}
} // namespace modules::plot