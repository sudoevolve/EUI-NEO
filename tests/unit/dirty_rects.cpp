#include "core/runtime/runtime_dirty.h"

#include <iostream>
#include <vector>

namespace {

bool expectRectCount(const std::string& label, const std::vector<core::Rect>& rects, std::size_t expected) {
    if (rects.size() == expected) {
        return true;
    }
    std::cerr << label << ": expected " << expected << " rects, got " << rects.size() << "\n";
    return false;
}

bool distantRectsStaySeparate() {
    const std::vector<core::dsl::runtime::LogicalDirtyRect> dirtyRects{
        {0.0f, 0.0f, 20.0f, 20.0f},
        {760.0f, 560.0f, 20.0f, 20.0f}
    };
    const std::vector<core::Rect> resolved = core::dsl::resolveDirtyRects(dirtyRects, 800, 600, 1.0f);
    return expectRectCount("distant rects", resolved, 2);
}

bool overlappingRectsMerge() {
    const std::vector<core::dsl::runtime::LogicalDirtyRect> dirtyRects{
        {10.0f, 10.0f, 30.0f, 30.0f},
        {35.0f, 35.0f, 30.0f, 30.0f}
    };
    const std::vector<core::Rect> resolved = core::dsl::resolveDirtyRects(dirtyRects, 800, 600, 1.0f);
    return expectRectCount("overlapping rects", resolved, 1);
}

bool nearbySmallRectsUseOnePass() {
    const std::vector<core::dsl::runtime::LogicalDirtyRect> dirtyRects{
        {0.0f, 0.0f, 20.0f, 20.0f},
        {30.0f, 0.0f, 20.0f, 20.0f},
        {60.0f, 0.0f, 20.0f, 20.0f}
    };
    const std::vector<core::Rect> resolved = core::dsl::resolveDirtyRects(dirtyRects, 800, 600, 1.0f);
    return expectRectCount("nearby small rects", resolved, 1);
}

} // namespace

int main() {
    bool ok = true;
    ok = distantRectsStaySeparate() && ok;
    ok = overlappingRectsMerge() && ok;
    ok = nearbySmallRectsUseOnePass() && ok;
    return ok ? 0 : 1;
}
