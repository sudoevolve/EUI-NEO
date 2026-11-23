#pragma once
#include <algorithm>
#include <cmath>

namespace modules::plot::detail {
// 有限端点的凸组合避免先计算异号跨度；端点保持原值。
inline double interpolateFinite(double a, double b, double t) {
    if (t == 0)
        return a;
    if (t == 1)
        return b;
    const double result = std::signbit(a) == std::signbit(b) ? a + (b - a) * t : a * (1 - t) + b * t;
    return t >= 0 && t <= 1 ? std::clamp(result, std::min(a, b), std::max(a, b)) : result;
}
// 适用于递增或递减端点；只有差值溢出时才缩放，保留相邻小数的间距。
inline double intervalFraction(double value, double a, double b) {
    if (value == a)
        return 0;
    if (value == b)
        return 1;
    const double span = b - a;
    return std::isfinite(span) ? (value - a) / span : (value / 2 - a / 2) / (b / 2 - a / 2);
}
} // namespace modules::plot::detail
