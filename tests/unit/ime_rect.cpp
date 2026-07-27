#include "core/runtime/runtime_geometry.h"

#include <cmath>
#include <iostream>
#include <string>

namespace {

bool expectClose(const std::string& label, float actual, float expected) {
    if (std::fabs(actual - expected) <= 0.01f) {
        return true;
    }
    std::cerr << label << ": expected " << expected << ", got " << actual << "\n";
    return false;
}

bool imeRectFollowsScrollTransform() {
    core::dsl::Element element;
    element.frame = {100.0f, 240.0f, 320.0f, 44.0f};
    element.imeRect = {72.0f, 12.0f, 1.5f, 20.0f};

    core::Transform scrollTransform;
    scrollTransform.translate.y = -80.0f;
    const core::TransformMatrix transform = core::dsl::matrixForTransform(
        core::dsl::toPixelRect(element.frame, 2.0f),
        core::dsl::scaleTransform(scrollTransform, 2.0f));
    const core::Rect rect = core::dsl::imeCursorPixelRect(element, 2.0f, transform);

    return expectClose("IME x", rect.x, 344.0f) &&
           expectClose("IME y", rect.y, 344.0f) &&
           expectClose("IME width", rect.width, 3.0f) &&
           expectClose("IME height", rect.height, 40.0f);
}

} // namespace

int main() {
    return imeRectFollowsScrollTransform() ? 0 : 1;
}
