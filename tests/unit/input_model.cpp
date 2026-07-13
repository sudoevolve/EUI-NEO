#include "components/input_model.h"

#include <array>
#include <cmath>
#include <iostream>
#include <string>

int main() {
    using Model = components::input_detail::InputModel;

    Model::InputState state;
    state.text = "first line\nsecond line";
    state.textRevision = 1;

    constexpr float inset = 12.0f;
    constexpr float fontSize = 17.0f;
    constexpr float width = 360.0f;
    constexpr float height = 116.0f;
    const float viewportWidth = width - inset * 2.0f;
    const float viewportHeight = height - inset * 2.0f;
    const Model::InputLayout layout = Model::InputLayout::build(
        state,
        viewportWidth,
        viewportHeight,
        width,
        inset,
        inset,
        fontSize,
        "monospace",
        fontSize,
        true);

    const core::Rect bounds{100.0f, 200.0f, width, height};
    const int firstLineCursor = layout.cursorFromPointer(
        bounds.x + inset,
        bounds.y + inset + fontSize * 0.5f,
        bounds,
        width,
        inset);
    const int secondLineCursor = layout.cursorFromPointer(
        bounds.x + inset,
        bounds.y + inset + fontSize * 1.5f,
        bounds,
        width,
        inset);

    if (firstLineCursor >= 10) {
        std::cerr << "First line click resolved past newline: " << firstLineCursor << "\n";
        return 1;
    }
    if (secondLineCursor < 11) {
        std::cerr << "Second line click did not resolve to second line: " << secondLineCursor << "\n";
        return 1;
    }

    std::string longChineseText;
    for (int index = 0; index < 40; ++index) {
        longChineseText += "啊";
    }
    const auto logicalMetrics = Model::measureMetrics(longChineseText, "monospace", fontSize);
    for (const float dpiScale : std::array{1.25f, 1.5f}) {
        const auto pixelMetrics = Model::measureMetrics(longChineseText, "monospace", fontSize * dpiScale);
        if (logicalMetrics.caretX.size() != pixelMetrics.caretX.size()) {
            std::cerr << "DPI metrics produced different caret counts at scale " << dpiScale << "\n";
            return 1;
        }
        float maximumCaretDrift = 0.0f;
        for (std::size_t index = 0; index < logicalMetrics.caretX.size(); ++index) {
            maximumCaretDrift = std::max(
                maximumCaretDrift,
                std::fabs(logicalMetrics.caretX[index] * dpiScale - pixelMetrics.caretX[index]));
        }
        if (maximumCaretDrift > 0.5f) {
            std::cerr << "Long Chinese caret drifted by " << maximumCaretDrift
                      << " pixels at DPI scale " << dpiScale << "\n";
            return 1;
        }
    }

    return 0;
}
