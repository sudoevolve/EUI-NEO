#include "modules/plot/contour.h"

#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace modules::plot {
namespace {
Point position(const ScalarField& field, std::size_t row, std::size_t column) {
    const auto x = field.xRange();
    const auto y = field.yRange();
    const double horizontal = field.sampling() == FieldSampling::GridPoints
                                  ? double(column) / (field.columns() - 1)
                                  : (double(column) + 0.5) / field.columns();
    const double vertical = field.sampling() == FieldSampling::GridPoints ? double(row) / (field.rows() - 1)
                                                                            : (double(row) + 0.5) / field.rows();
    return {x.min + (x.max - x.min) * horizontal,
            field.origin() == FieldOrigin::LowerLeft ? y.min + (y.max - y.min) * vertical
                                                      : y.max - (y.max - y.min) * vertical};
}
Point intersection(Point from, double fromValue, Point to, double toValue, double level) {
    const double fraction = (level - fromValue) / (toValue - fromValue);
    return {from.x + (to.x - from.x) * fraction, from.y + (to.y - from.y) * fraction};
}
void validateLevels(const std::vector<double>& levels) {
    for (std::size_t index = 0; index < levels.size(); ++index)
        if (!std::isfinite(levels[index]) || (index && levels[index] <= levels[index - 1]))
            throw std::invalid_argument("plot: contour levels must be finite and increasing");
}
struct ScalarPoint {
    Point point;
    double value = 0;
};
std::vector<ScalarPoint> clip(const std::vector<ScalarPoint>& input, double limit, bool keepAbove) {
    std::vector<ScalarPoint> output;
    if (input.empty())
        return output;
    auto previous = input.back();
    bool previousInside = keepAbove ? previous.value >= limit : previous.value <= limit;
    for (const auto current : input) {
        const bool currentInside = keepAbove ? current.value >= limit : current.value <= limit;
        if (currentInside != previousInside) {
            const double fraction = (limit - previous.value) / (current.value - previous.value);
            output.push_back({{previous.point.x + (current.point.x - previous.point.x) * fraction,
                               previous.point.y + (current.point.y - previous.point.y) * fraction},
                              limit});
        }
        if (currentInside)
            output.push_back(current);
        previous = current;
        previousInside = currentInside;
    }
    return output;
}
} // namespace

std::vector<ContourLines> marchingSquares(const ScalarField& field, const std::vector<double>& levels) {
    validateLevels(levels);
    std::vector<ContourLines> result;
    result.reserve(levels.size());
    for (const double level : levels) {
        ContourLines lines;
        lines.level = level;
        for (std::size_t row = 0; row + 1 < field.rows(); ++row)
            for (std::size_t column = 0; column + 1 < field.columns(); ++column) {
                const std::array<Point, 4> points{position(field, row, column), position(field, row, column + 1),
                                                  position(field, row + 1, column + 1), position(field, row + 1, column)};
                const std::array<double, 4> values{field.value(row, column), field.value(row, column + 1),
                                                   field.value(row + 1, column + 1), field.value(row + 1, column)};
                if (!std::isfinite(values[0]) || !std::isfinite(values[1]) || !std::isfinite(values[2]) ||
                    !std::isfinite(values[3]))
                    continue;
                std::array<std::optional<Point>, 4> edges;
                for (std::size_t edge = 0; edge < edges.size(); ++edge) {
                    const auto next = (edge + 1) % edges.size();
                    if ((values[edge] > level) != (values[next] > level))
                        edges[edge] = intersection(points[edge], values[edge], points[next], values[next], level);
                }
                std::vector<Point> crossings;
                for (const auto& edge : edges)
                    if (edge)
                        crossings.push_back(*edge);
                if (crossings.size() == 2)
                    lines.segments.push_back({crossings[0], crossings[1]});
                else if (crossings.size() == 4) {
                    const double center = (values[0] + values[1] + values[2] + values[3]) / 4;
                    if (center > level) {
                        lines.segments.push_back({crossings[0], crossings[1]});
                        lines.segments.push_back({crossings[2], crossings[3]});
                    } else {
                        lines.segments.push_back({crossings[0], crossings[3]});
                        lines.segments.push_back({crossings[1], crossings[2]});
                    }
                }
            }
        result.push_back(std::move(lines));
    }
    return result;
}
std::vector<double> automaticContourLevels(const ScalarField& field, std::size_t count) {
    if (count == 0 || count > 100)
        throw std::invalid_argument("plot: contour level count must be between 1 and 100");
    const auto range = field.finiteRange();
    if (!range || range->min == range->max)
        return {};
    std::vector<double> result;
    result.reserve(count);
    for (std::size_t index = 1; index <= count; ++index)
        result.push_back(range->min + (range->max - range->min) * index / (count + 1));
    return result;
}
std::vector<ContourLabel> contourLabels(const std::vector<ContourLines>& lines, std::size_t maxPerLevel) {
    if (maxPerLevel == 0)
        throw std::invalid_argument("plot: contour labels per level must be positive");
    std::vector<ContourLabel> result;
    for (const auto& line : lines) {
        const auto count = std::min(maxPerLevel, line.segments.size());
        for (std::size_t index = 0; index < count; ++index) {
            const auto& segment = line.segments[index * line.segments.size() / count];
            result.push_back({line.level, {(segment.from.x + segment.to.x) / 2, (segment.from.y + segment.to.y) / 2}});
        }
    }
    return result;
}
std::vector<ContourBand> filledContours(const ScalarField& field, const std::vector<double>& levels) {
    validateLevels(levels);
    std::vector<ContourBand> result(levels.size() + 1);
    for (std::size_t index = 0; index < result.size(); ++index) {
        result[index].lower = index ? levels[index - 1] : -std::numeric_limits<double>::infinity();
        result[index].upper = index < levels.size() ? levels[index] : std::numeric_limits<double>::infinity();
    }
    for (std::size_t row = 0; row + 1 < field.rows(); ++row)
        for (std::size_t column = 0; column + 1 < field.columns(); ++column) {
            const std::array<ScalarPoint, 4> corners{{{position(field, row, column), field.value(row, column)},
                                                        {position(field, row, column + 1), field.value(row, column + 1)},
                                                        {position(field, row + 1, column + 1), field.value(row + 1, column + 1)},
                                                        {position(field, row + 1, column), field.value(row + 1, column)}}};
            if (!std::isfinite(corners[0].value) || !std::isfinite(corners[1].value) ||
                !std::isfinite(corners[2].value) || !std::isfinite(corners[3].value))
                continue;
            for (const auto triangle : {std::array<ScalarPoint, 3>{corners[0], corners[1], corners[2]},
                                        std::array<ScalarPoint, 3>{corners[0], corners[2], corners[3]}})
                for (auto& band : result) {
                    std::vector<ScalarPoint> polygon(triangle.begin(), triangle.end());
                    if (std::isfinite(band.lower))
                        polygon = clip(polygon, band.lower, true);
                    if (std::isfinite(band.upper))
                        polygon = clip(polygon, band.upper, false);
                    for (std::size_t index = 1; index + 1 < polygon.size(); ++index)
                        band.triangles.push_back(
                            {polygon[0].point, polygon[index].point, polygon[index + 1].point});
                }
        }
    return result;
}
} // namespace modules::plot