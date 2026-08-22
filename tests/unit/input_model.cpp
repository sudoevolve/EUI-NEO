#include "components/input.h"
#include "components/input_model.h"

#include <array>
#include <cmath>
#include <iostream>
#include <string>

int main() {
    using Model = components::input_detail::InputModel;

    const core::KeyEvent left{
        core::InputKey::Left, core::KeyAction::Repeat, {false, true}
    };
    core::KeyModifiers shortcutModifiers;
    shortcutModifiers.shift = true;
#if defined(__APPLE__)
    shortcutModifiers.super = true;
#else
    shortcutModifiers.control = true;
#endif
    const core::KeyEvent shiftedShortcut{
        core::InputKey::Z, core::KeyAction::Press, shortcutModifiers
    };
    const core::KeyEvent shiftRelease{
        core::InputKey::LeftShift, core::KeyAction::Release, {}
    };
    if (!left.isDown() || !left.modifiers.shift ||
        !shiftedShortcut.isDown() || !shiftedShortcut.modifiers.shortcut() ||
        !shiftedShortcut.modifiers.shift || shiftRelease.isDown()) {
        std::cerr << "Generic keyboard events lost key action or modifiers\n";
        return 1;
    }

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

    Model::InputState scrolledState;
    scrolledState.text = "one\ntwo\nthree\nfour\nfive\nsix";
    scrolledState.textRevision = 1;
    scrolledState.cursor = static_cast<int>(scrolledState.text.size());
    scrolledState.verticalScroll = 0.0f;
    scrolledState.followCaret = false;
    Model::InputLayout::build(
        scrolledState, viewportWidth, fontSize * 2.0f, width, inset, inset,
        fontSize, "monospace", fontSize, true);
    if (scrolledState.verticalScroll != 0.0f) {
        std::cerr << "Manual multiline scroll was overridden by caret following\n";
        return 1;
    }

    core::dsl::Ui ui;
    ui.begin("input.viewport");
    components::input(ui, "field")
        .size(120.0f, 42.0f)
        .value("A very long value that must scroll inside the field")
        .build();
    ui.end();
    ui.layout(120.0f, 42.0f);
    const core::dsl::Element* textViewport = ui.find("field.textViewport");
    if (textViewport == nullptr || !textViewport->clip || textViewport->frame.x <= 0.0f || textViewport->frame.width >= 120.0f) {
        std::cerr << "Input text viewport did not preserve the horizontal inset\n";
        return 1;
    }

    ui.begin("input.multiline");
    components::input(ui, "multiline")
        .size(180.0f, 100.0f)
        .fontSize(20.0f)
        .multiline()
        .value("first\nsecond")
        .build();
    ui.end();
    ui.layout(180.0f, 100.0f);
    const core::dsl::Element* firstTextLine = ui.find("multiline.text.0");
    const core::dsl::Element* secondTextLine = ui.find("multiline.text.1");
    if (firstTextLine == nullptr || secondTextLine == nullptr ||
        std::fabs(secondTextLine->frame.y - firstTextLine->frame.y - 24.0f) > 0.01f) {
        std::cerr << "Multiline input did not reserve a full text line height\n";
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
