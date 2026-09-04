#pragma once

#include "core/runtime/runtime_geometry.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace core::dsl {

namespace runtime {

struct LogicalDirtyRect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

} // namespace runtime

inline float rectArea(const Rect& rect) {
    return std::max(0.0f, rect.width) * std::max(0.0f, rect.height);
}

inline bool shouldMergeDirtyRects(const Rect& a, const Rect& b) {
    if (intersects(a, b)) {
        return true;
    }
    const Rect merged = unionRect(a, b);
    const float separateArea = rectArea(a) + rectArea(b);
    if (separateArea <= 0.0f) {
        return true;
    }
    return rectArea(merged) <= separateArea * 1.25f;
}

inline std::vector<Rect> resolveDirtyRects(const std::vector<runtime::LogicalDirtyRect>& dirtyRects,
                                           int windowWidth,
                                           int windowHeight,
                                           float dpiScale) {
    constexpr std::size_t kMaxDirtyRects = 8;
    constexpr float kMaxDirtyAreaRatio = 0.65f;
    constexpr float kDirtyRectCommandOverheadPixels = 4096.0f;
    std::vector<Rect> rects;
    for (const runtime::LogicalDirtyRect& dirty : dirtyRects) {
        Rect rect = toPixelRect(Rect{dirty.x, dirty.y, dirty.width, dirty.height}, dpiScale);
        const float left = std::clamp(std::floor(rect.x), 0.0f, static_cast<float>(windowWidth));
        const float top = std::clamp(std::floor(rect.y), 0.0f, static_cast<float>(windowHeight));
        const float right = std::clamp(std::ceil(rect.x + rect.width), 0.0f, static_cast<float>(windowWidth));
        const float bottom = std::clamp(std::ceil(rect.y + rect.height), 0.0f, static_cast<float>(windowHeight));
        if (right <= left || bottom <= top) {
            continue;
        }
        rect = {left, top, right - left, bottom - top};

        bool merged = false;
        for (Rect& existing : rects) {
            if (shouldMergeDirtyRects(existing, rect)) {
                existing = unionRect(existing, rect);
                merged = true;
                break;
            }
        }
        if (!merged) {
            rects.push_back(rect);
        }
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (std::size_t i = 0; i < rects.size() && !changed; ++i) {
            for (std::size_t j = i + 1; j < rects.size(); ++j) {
                if (shouldMergeDirtyRects(rects[i], rects[j])) {
                    rects[i] = unionRect(rects[i], rects[j]);
                    rects.erase(rects.begin() + static_cast<std::ptrdiff_t>(j));
                    changed = true;
                    break;
                }
            }
        }
    }

    const float windowArea = static_cast<float>(std::max(0, windowWidth)) *
                             static_cast<float>(std::max(0, windowHeight));
    float dirtyArea = 0.0f;
    Rect mergedRect{};
    bool hasMergedRect = false;
    for (const Rect& rect : rects) {
        dirtyArea += rectArea(rect);
        mergedRect = hasMergedRect ? unionRect(mergedRect, rect) : rect;
        hasMergedRect = true;
    }

    const float mergedArea = hasMergedRect ? rectArea(mergedRect) : 0.0f;
    const float mergeWaste = std::max(0.0f, mergedArea - dirtyArea);
    // A scissor/clear pass has a fixed CPU and GPU cost. Collapse fragmented
    // damage when the extra pixels are cheaper than recording another pass,
    // while retaining separate regions when their bounding box is expensive.
    const std::size_t savedPasses = rects.size() > 1 ? rects.size() - 1 : 0;
    const float estimatedPassOverhead =
        kDirtyRectCommandOverheadPixels * static_cast<float>(savedPasses);
    const bool areaDominates = windowArea > 0.0f && dirtyArea > windowArea * kMaxDirtyAreaRatio;
    const bool mergeIsCheaper = mergeWaste <= estimatedPassOverhead;
    if (rects.size() > kMaxDirtyRects || areaDominates || mergeIsCheaper) {
        rects.clear();
        if (hasMergedRect) {
            rects.push_back(mergedRect);
        }
    }
    return rects;
}

} // namespace core::dsl
