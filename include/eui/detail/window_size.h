#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace app::detail {

/** @brief 将 compose 逻辑尺寸转换为原生窗口单位，兼容 framebuffer 与鼠标坐标比例不同的平台。 */
inline std::pair<int, int> nativeWindowSize(int width, int height, float dpiScale, float pointerScale, float uiScale) {
    const auto validScale = [](float value) { return std::isfinite(value) && value > 0 ? static_cast<double>(value) : 1.0; };
    const double factor = validScale(dpiScale) * validScale(uiScale) / validScale(pointerScale);
    const auto dimension = [factor](int value) {
        return static_cast<int>(std::round(std::clamp(std::max(1, value) * factor,
            1.0, static_cast<double>(std::numeric_limits<int>::max() / 4))));
    };
    return {dimension(width), dimension(height)};
}

} // namespace app::detail
