#include "modules/plot/contour.h"
#include "modules/plot/numeric.h"

#include <algorithm>
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
    return {detail::interpolateFinite(x.min, x.max, horizontal),
            detail::interpolateFinite(y.min, y.max,
                                      field.origin() == FieldOrigin::LowerLeft ? vertical : 1 - vertical)};
}
Point intersection(Point from, double fromValue, Point to, double toValue, double level) {
    const double fraction = detail::intervalFraction(level, fromValue, toValue);
    return {detail::interpolateFinite(from.x, to.x, fraction),
            detail::interpolateFinite(from.y, to.y, fraction)};
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
            output.push_back(
                {intersection(previous.point, previous.value, current.point, current.value, limit), limit});
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
                    // 分别保留尾数和指数，避免共同缩放使极小角值下溢，错误改变鞍点拓扑。
                    std::array<double, 4> mantissa;
                    std::array<int, 4> exponent;
                    for (int i = 0; i < 4; ++i) {
                        double distance = values[i] - level;
                        const bool overflow = !std::isfinite(distance);
                        if (overflow)
                            distance = values[i] / 2 - level / 2;
                        mantissa[i] = std::frexp(std::abs(distance), &exponent[i]);
                        exponent[i] += overflow ? 1 : 0;
                    }
                    int shiftA = 0, shiftB = 0;
                    const double productA = std::frexp(mantissa[0] * mantissa[2], &shiftA);
                    const double productB = std::frexp(mantissa[1] * mantissa[3], &shiftB);
                    const int exponentA = exponent[0] + exponent[2] + shiftA;
                    const int exponentB = exponent[1] + exponent[3] + shiftB;
                    const double residualA =
                        std::ldexp(std::fma(mantissa[0], mantissa[2], -mantissa[0] * mantissa[2]), -shiftA);
                    const double residualB =
                        std::ldexp(std::fma(mantissa[1], mantissa[3], -mantissa[1] * mantissa[3]), -shiftB);
                    if (productB == 0 ||
                        (productA != 0 &&
                         (exponentA > exponentB ||
                          (exponentA == exponentB &&
                           (productA > productB || (productA == productB && residualA >= residualB)))))) {
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
    for (std::size_t index = 1; index <= count; ++index) {
        const double value = detail::interpolateFinite(range->min, range->max, double(index) / (count + 1));
        if (value > range->min && value < range->max && (result.empty() || value > result.back()))
            result.push_back(value);
    }
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
            result.push_back({line.level,
                              {detail::interpolateFinite(segment.from.x, segment.to.x, .5),
                               detail::interpolateFinite(segment.from.y, segment.to.y, .5)}});
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
