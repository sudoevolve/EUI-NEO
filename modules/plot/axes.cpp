#include "modules/plot/axes.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace modules::plot {
namespace {
bool validRange(Range range, Scale scale) {
    return std::isfinite(range.min) && std::isfinite(range.max) && range.min < range.max &&
           (scale == Scale::Linear || range.min > 0);
}
bool accepts(double value, Scale scale) {
    return std::isfinite(value) && (scale == Scale::Linear || value > 0);
}
bool validViewport(Viewport viewport) {
    return std::isfinite(viewport.x) && std::isfinite(viewport.y) && std::isfinite(viewport.width) &&
           std::isfinite(viewport.height) && viewport.width > 0 && viewport.height > 0;
}
double logDistance(double value, double origin) {
    // 相邻大数不能先各自取 log 再相减，否则会丢失整个坐标间距。
    const double relative = (value - origin) / origin;
    return std::isfinite(relative) && relative > -1 ? std::log1p(relative)
                                                    : std::log(value) - std::log(origin);
}
double interpolate(double a, double b, double t) {
    return std::signbit(a) == std::signbit(b) ? a + (b - a) * t : a * (1 - t) + b * t;
}
std::string formatNumber(double value) {
    std::ostringstream stream;
    stream.imbue(std::locale::classic());
    stream << std::setprecision(6) << value;
    return stream.str();
}
void include(std::optional<Range>& extent, double value) {
    if (!extent)
        extent = Range{value, value};
    else {
        extent->min = std::min(extent->min, value);
        extent->max = std::max(extent->max, value);
    }
}
} // namespace

void Axis::setScale(Scale scale) {
    if (scale != Scale::Linear && scale != Scale::Log10)
        throw std::invalid_argument("plot: unknown axis scale");
    if (!automatic_ && !validRange(range_, scale))
        throw std::invalid_argument("plot: range incompatible with scale");
    scale_ = scale;
    if (automatic_ && !validRange(range_, scale_))
        range_ = {1, 10};
}
Scale Axis::scale() const noexcept { return scale_; }
void Axis::setRange(Range range) {
    if (!validRange(range, scale_))
        throw std::invalid_argument("plot: invalid axis range");
    range_ = range;
    automatic_ = false;
}
Range Axis::range() const noexcept { return range_; }
void Axis::resetAuto() noexcept { automatic_ = true; }
bool Axis::automatic() const noexcept { return automatic_; }
void Axis::setReversed(bool reversed) noexcept { reversed_ = reversed; }
bool Axis::reversed() const noexcept { return reversed_; }
void Axis::fit(const std::optional<Range>& extent) {
    if (!automatic_)
        return;
    if (!extent) {
        range_ = scale_ == Scale::Linear ? Range{0, 1} : Range{1, 10};
        return;
    }
    auto range = *extent;
    if (!accepts(range.min, scale_) || !accepts(range.max, scale_) || range.min > range.max)
        throw std::invalid_argument("plot: invalid data extent");
    if (range.min == range.max) {
        const double value = range.min;
        if (scale_ == Scale::Log10)
            range = {value / 2, value * 2};
        else {
            const double padding = value == 0 ? 1 : std::abs(value) * 0.05;
            range = {value - padding, value + padding};
        }
        if (!accepts(range.min, scale_))
            range.min = value;
        if (!accepts(range.max, scale_))
            range.max = value;
        if (range.min == range.max) {
            const double lower = std::nextafter(value, -std::numeric_limits<double>::infinity());
            const double upper = std::nextafter(value, std::numeric_limits<double>::infinity());
            if (accepts(lower, scale_))
                range.min = lower;
            if (accepts(upper, scale_))
                range.max = upper;
        }
    }
    if (!validRange(range, scale_))
        throw std::invalid_argument("plot: unrepresentable range");
    range_ = range;
}
std::optional<double> Axis::normalize(double value) const {
    if (!accepts(value, scale_))
        return std::nullopt;
    double fraction;
    if (scale_ == Scale::Log10) {
        fraction = logDistance(value, range_.min) / logDistance(range_.max, range_.min);
    } else {
        const double span = range_.max - range_.min;
        if (std::isfinite(span))
            fraction = (value - range_.min) / span;
        else
            fraction = (value / 2 - range_.min / 2) / (range_.max / 2 - range_.min / 2);
    }
    if (reversed_)
        fraction = 1 - fraction;
    return std::isfinite(fraction) ? std::optional<double>{fraction} : std::nullopt;
}
std::optional<double> Axis::denormalize(double fraction) const {
    if (!std::isfinite(fraction))
        return std::nullopt;
    if (reversed_)
        fraction = 1 - fraction;
    double value;
    if (fraction == 0)
        value = range_.min;
    else if (fraction == 1)
        value = range_.max;
    else if (scale_ == Scale::Linear)
        value = interpolate(range_.min, range_.max, fraction);
    else {
        const double distance = logDistance(range_.max, range_.min);
        if (std::abs(distance * fraction) < 0.5)
            value = range_.min + range_.min * std::expm1(distance * fraction);
        else
            value = std::exp(std::log(range_.min) + distance * fraction);
    }
    return accepts(value, scale_) ? std::optional<double>{value} : std::nullopt;
}
std::vector<Tick> Axis::ticks(int targetCount) const {
    targetCount = std::clamp(targetCount, 2, 20);
    std::vector<Tick> result;
    const auto add = [&](double value, bool major) {
        if (value >= range_.min && value <= range_.max && accepts(value, scale_))
            result.push_back(
                {value, major, major ? (formatter ? formatter(value) : formatNumber(value)) : ""});
    };
    if (scale_ == Scale::Log10 && logDistance(range_.max, range_.min) >= std::log(10.0)) {
        const int first = static_cast<int>(std::floor(std::log10(range_.min)));
        const int last = static_cast<int>(std::ceil(std::log10(range_.max)));
        const int stride = std::max(1, (last - first + targetCount - 1) / targetCount);
        for (int exponent = first; exponent <= last; ++exponent) {
            const double base = std::pow(10.0, exponent);
            if ((exponent - first) % stride == 0)
                add(base, true);
            if (stride == 1)
                for (int i = 2; i < 10; ++i)
                    add(base * i, false);
        }
    } else {
        // 先除后减使跨越整个 double 范围的轴仍可计算刻度步长。
        const double span = range_.max - range_.min;
        const double raw =
            std::isfinite(span) ? span / targetCount : range_.max / targetCount - range_.min / targetCount;
        const double power = std::pow(10.0, std::floor(std::log10(raw)));
        if (raw > 0 && power > 0 && std::isfinite(power)) {
            const double ratio = raw / power;
            const double step = (ratio <= 1 ? 1 : ratio <= 2 ? 2 : ratio <= 5 ? 5 : 10) * power;
            const double first = std::ceil(range_.min / step) * step;
            if (std::isfinite(first) && std::isfinite(step)) {
                for (int i = 0; i < 128; ++i) {
                    const double value = first + i * step;
                    if (!std::isfinite(value) || value > range_.max)
                        break;
                    if (i > 0 && value <= first + (i - 1) * step)
                        break;
                    add(value, true);
                    for (int minor = 1; minor < 5; ++minor)
                        add(value + step * (minor / 5.0), false);
                }
            }
        }
    }
    if (result.empty()) {
        add(range_.min, true);
        add(range_.max, true);
    }
    std::sort(result.begin(), result.end(), [](const Tick& a, const Tick& b) { return a.value < b.value; });
    result.erase(std::unique(result.begin(), result.end(),
                             [](const Tick& a, const Tick& b) { return a.value == b.value; }),
                 result.end());
    return result;
}
void Axes::fit(const std::vector<Data>& data) {
    std::optional<Range> extentX, extentY;
    for (const auto& series : data) {
        for (std::size_t index = 0; index < series.size(); ++index) {
            const auto point = series.at(index);
            if (!accepts(point.x, x.scale()) || !accepts(point.y, y.scale()))
                continue;
            include(extentX, point.x);
            include(extentY, point.y);
        }
    }
    x.fit(extentX);
    y.fit(extentY);
}
std::optional<Point> Axes::toScreen(Point point, Viewport viewport) const {
    if (!validViewport(viewport))
        return std::nullopt;
    const auto horizontal = x.normalize(point.x), vertical = y.normalize(point.y);
    if (!horizontal || !vertical)
        return std::nullopt;
    Point result{viewport.x + *horizontal * viewport.width, viewport.y + (1 - *vertical) * viewport.height};
    return std::isfinite(result.x) && std::isfinite(result.y) ? std::optional<Point>{result} : std::nullopt;
}
std::optional<Point> Axes::toData(Point point, Viewport viewport) const {
    if (!validViewport(viewport) || !std::isfinite(point.x) || !std::isfinite(point.y))
        return std::nullopt;
    const auto horizontal = x.denormalize((point.x - viewport.x) / viewport.width);
    const auto vertical = y.denormalize(1 - (point.y - viewport.y) / viewport.height);
    if (!horizontal || !vertical)
        return std::nullopt;
    return Point{*horizontal, *vertical};
}
bool Axes::equalize(Viewport viewport) {
    if (!validViewport(viewport) || x.scale() != Scale::Linear || y.scale() != Scale::Linear)
        return false;
    const auto xr = x.range(), yr = y.range();
    const double horizontal = (xr.max - xr.min) / viewport.width,
                 vertical = (yr.max - yr.min) / viewport.height;
    if (!std::isfinite(horizontal) || !std::isfinite(vertical))
        return false;
    const double units = std::max(horizontal, vertical);
    const double cx = interpolate(xr.min, xr.max, 0.5), cy = interpolate(yr.min, yr.max, 0.5);
    const Range newX{cx - units * viewport.width / 2, cx + units * viewport.width / 2};
    const Range newY{cy - units * viewport.height / 2, cy + units * viewport.height / 2};
    if (!validRange(newX, Scale::Linear) || !validRange(newY, Scale::Linear))
        return false;
    x.setRange(newX);
    y.setRange(newY);
    return true;
}
} // namespace modules::plot
