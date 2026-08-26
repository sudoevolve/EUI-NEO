#include "components/button.h"
#include "components/checkbox.h"
#include "components/dropdown.h"
#include "components/input.h"
#include "components/progress.h"
#include "components/radio.h"
#include "components/segmented.h"
#include "components/slider.h"
#include "components/stepper.h"
#include "components/switch.h"
#include "components/tabs.h"
#include "core/dsl_runtime.h"

#include <cmath>
#include <iostream>

namespace {

struct PageState {
    int selectedIndex = 0;
    bool autoPlay = true;
};

bool closeEnough(float left, float right, float tolerance = 0.25f) {
    return std::fabs(left - right) <= tolerance;
}

bool scrollImpulsePreservesStepDistance() {
    float offset = 0.0f;
    float velocity = core::dsl::addScrollImpulse(0.0f, 48.0f);
    for (int frame = 0; frame < 600 && core::dsl::scrollMotionActive(velocity); ++frame) {
        const core::dsl::ScrollMotionStep motion = core::dsl::advanceScrollMotion(
            offset, 1000.0f, velocity, 1.0f / 120.0f);
        offset = motion.offset;
        velocity = motion.velocity;
    }
    if (!closeEnough(offset, 48.0f)) {
        std::cerr << "scroll impulse distance changed: " << offset << "\n";
        return false;
    }
    return velocity == 0.0f;
}

bool scrollMotionClampsAtBoundary() {
    float offset = 90.0f;
    float velocity = core::dsl::addScrollImpulse(0.0f, 48.0f);
    for (int frame = 0; frame < 600 && core::dsl::scrollMotionActive(velocity); ++frame) {
        const core::dsl::ScrollMotionStep motion = core::dsl::advanceScrollMotion(
            offset, 100.0f, velocity, 1.0f / 120.0f);
        offset = motion.offset;
        velocity = motion.velocity;
    }
    if (!closeEnough(offset, 100.0f, 0.01f) || velocity != 0.0f) {
        std::cerr << "scroll motion did not stop at boundary\n";
        return false;
    }
    return true;
}

bool repeatedScrollImpulsesAccumulate() {
    const float first = core::dsl::addScrollImpulse(0.0f, 48.0f);
    const float second = core::dsl::addScrollImpulse(first, 48.0f);
    const float reversed = core::dsl::addScrollImpulse(second, -48.0f);
    if (second <= first || reversed >= 0.0f) {
        std::cerr << "scroll impulses did not accumulate or reverse responsively\n";
        return false;
    }
    return true;
}

bool scrollMotionCapsLongFrameDelta() {
    const float velocity = core::dsl::addScrollImpulse(0.0f, 48.0f);
    const core::dsl::ScrollMotionStep motion = core::dsl::advanceScrollMotion(
        0.0f, 1000.0f, velocity, 5.0f);
    if (motion.offset <= 0.0f || motion.offset >= 20.0f || !motion.active) {
        std::cerr << "long frame delta consumed scroll inertia immediately\n";
        return false;
    }
    return true;
}

bool blockPointerUsesArrowCursor() {
    core::dsl::Ui ui;
    ui.begin("block.pointer");
    ui.rect("surface").size(120.0f, 80.0f).blockPointer().build();
    ui.end();

    const core::dsl::Element* surface = ui.find("surface");
    if (surface == nullptr || !surface->interactive || surface->cursor != core::CursorShape::Arrow || surface->onClick) {
        std::cerr << "pointer blocker did not retain blocker semantics\n";
        return false;
    }
    return true;
}

bool textWrapContentUsesIntrinsicSize() {
    core::dsl::Ui ui;
    ui.begin("intrinsic.text");
    ui.column("root")
        .size(800.0f, 600.0f)
        .padding(32.0f)
        .content([&] {
            ui.text("title")
                .text("Hello EUI-NEO")
                .fontSize(32.0f)
                .build();
            ui.stack("button").size(240.0f, 70.0f).build();
        })
        .build();
    ui.end();
    ui.layout(800.0f, 600.0f);

    const core::dsl::Element* title = ui.find("title");
    const core::dsl::Element* button = ui.find("button");
    if (title == nullptr || button == nullptr || title->frame.height <= 0.0f ||
        button->frame.y <= 32.0f || button->frame.y + button->frame.height > 600.0f) {
        std::cerr << "intrinsic text size did not keep following content visible\n";
        return false;
    }
    return true;
}

bool textSizeMeasurementMatchesLineLayout() {
    core::TextStyle style;
    style.text = "first\nsecond";
    style.fontFamily = "monospace";
    style.fontSize = 20.0f;
    style.lineHeight = 26.0f;

    const core::Vec2 size = core::TextPrimitive::measureTextSize(style);
    const float expectedWidth = std::max(
        core::TextPrimitive::measureTextWidth("first", style.fontFamily, style.fontSize),
        core::TextPrimitive::measureTextWidth("second", style.fontFamily, style.fontSize));
    if (!closeEnough(size.x, expectedWidth) || !closeEnough(size.y, 52.0f)) {
        std::cerr << "text intrinsic measurement diverged from line layout\n";
        return false;
    }
    return true;
}

bool controlStatePriorityIsStable() {
    using components::theme::ControlState;
    using components::theme::controlState;
    if (controlState(false, false, false, false) != ControlState::Normal ||
        controlState(false, false, true, false) != ControlState::Hovered ||
        controlState(false, false, false, true) != ControlState::Focused ||
        controlState(false, true, true, true) != ControlState::Pressed ||
        controlState(true, true, true, true) != ControlState::Disabled) {
        std::cerr << "control state priority is inconsistent\n";
        return false;
    }
    return true;
}

bool interactionTokensAreComplete() {
    const auto light = components::theme::light();
    const auto dark = components::theme::dark();
    const auto valid = [](const components::theme::ThemeColorTokens& tokens) {
        return tokens.interaction.focusRingWidth > 0.0f &&
               tokens.interaction.focusRingOffset >= 0.0f &&
               tokens.interaction.disabledOpacity >= 0.0f &&
               tokens.interaction.disabledOpacity <= 1.0f &&
               tokens.interaction.focusRing.a > 0.0f;
    };
    if (!valid(light) || !valid(dark)) {
        std::cerr << "theme interaction tokens are incomplete\n";
        return false;
    }
    return true;
}

bool componentDefaultsMatchGallery() {
    core::dsl::Ui ui;
    ui.begin("component.defaults");
    components::button(ui, "button").build();
    components::input(ui, "input").build();
    components::dropdown(ui, "dropdown").build();
    components::checkbox(ui, "checkbox").build();
    components::radio(ui, "radio").build();
    components::toggleSwitch(ui, "switch").build();
    components::progress(ui, "progress").build();
    components::slider(ui, "slider").build();
    components::segmented(ui, "segmented").items({"A", "B"}).build();
    components::tabs(ui, "tabs").items({"A", "B"}).build();
    components::stepper(ui, "stepper").build();
    ui.end();
    ui.layout(1000.0f, 800.0f);

    const std::pair<const char*, float> expected[] = {
        {"button", 54.0f}, {"input", 44.0f}, {"dropdown.field", 44.0f},
        {"checkbox", 30.0f}, {"radio", 30.0f}, {"switch", 32.0f},
        {"progress", 14.0f}, {"slider", 32.0f}, {"segmented", 38.0f},
        {"tabs", 42.0f}, {"stepper", 40.0f},
    };
    for (const auto& entry : expected) {
        const core::dsl::Element* element = ui.find(entry.first);
        if (element == nullptr || !closeEnough(element->frame.height, entry.second)) {
            std::cerr << entry.first << " default height did not match gallery\n";
            return false;
        }
    }
    return true;
}

} // namespace

int main() {
    core::dsl::Ui ui;

    ui.begin("state.page");
    PageState* first = &ui.state<PageState>("page");
    first->selectedIndex = 3;
    first->autoPlay = false;
    ui.end();

    ui.begin("state.page");
    PageState* second = &ui.state<PageState>("page");
    ui.end();

    if (first != second) {
        std::cerr << "page state address changed across compose\n";
        return 1;
    }
    if (second->selectedIndex != 3 || second->autoPlay) {
        std::cerr << "page state values did not survive compose\n";
        return 1;
    }
    bool ok = true;
    ok = scrollImpulsePreservesStepDistance() && ok;
    ok = scrollMotionClampsAtBoundary() && ok;
    ok = repeatedScrollImpulsesAccumulate() && ok;
    ok = scrollMotionCapsLongFrameDelta() && ok;
    ok = blockPointerUsesArrowCursor() && ok;
    ok = textWrapContentUsesIntrinsicSize() && ok;
    ok = textSizeMeasurementMatchesLineLayout() && ok;
    ok = componentDefaultsMatchGallery() && ok;
    ok = controlStatePriorityIsStable() && ok;
    ok = interactionTokensAreComplete() && ok;
    return ok ? 0 : 1;
}
