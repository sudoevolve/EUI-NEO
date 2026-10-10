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

bool controlledScrollOffsetsPreserveUserMotion() {
    core::dsl::Element owner;
    owner.scrollMaxOffset = 100.0f;
    core::dsl::runtime::ScrollStateInstance state;
    core::dsl::syncOwnedScrollState(owner, state);
    state.offset = 30.0f;
    state.velocity = 50.0f;
    core::dsl::syncOwnedScrollState(owner, state);
    if (state.offset != 30.0f || state.velocity != 50.0f) {
        std::cerr << "unchanged page offset reset user scrolling\n";
        return false;
    }
    owner.scrollOffset = 30.0f;
    core::dsl::syncOwnedScrollState(owner, state);
    if (state.velocity != 50.0f) {
        std::cerr << "onChange echo stopped wheel inertia\n";
        return false;
    }
    owner.scrollOffset = 100.0f;
    core::dsl::syncOwnedScrollState(owner, state);
    if (state.offset != 100.0f || state.velocity != 0.0f) {
        std::cerr << "programmatic jump was ignored\n";
        return false;
    }
    owner.scrollMaxOffset = 150.0f;
    owner.scrollOffset = 150.0f;
    core::dsl::syncOwnedScrollState(owner, state);
    if (state.offset != 150.0f) {
        std::cerr << "new content did not follow requested bottom\n";
        return false;
    }
    owner.scrollMaxOffset = 20.0f;
    core::dsl::syncOwnedScrollState(owner, state);
    return state.offset == 20.0f && state.velocity == 0.0f;
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
        {"button", 54.0f},
        {"input", 44.0f},
        {"dropdown.field", 44.0f},
        {"checkbox", 30.0f},
        {"radio", 30.0f},
        {"switch", 32.0f},
        {"progress", 14.0f},
        {"slider", 32.0f},
        {"segmented", 38.0f},
        {"tabs", 42.0f},
        {"stepper", 40.0f},
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

bool dropdownRetainsUncontrolledOpenState() {
    core::dsl::Ui ui;
    const auto compose = [&] {
        ui.begin("dropdown.internal-state");
        components::dropdown(ui, "menu").items({"A", "B"}).build();
        ui.end();
    };

    compose();
    const core::dsl::Element* field = ui.find("menu.field");
    if (field == nullptr || !field->onClick) {
        std::cerr << "dropdown field click handler missing\n";
        return false;
    }
    field->onClick();
    compose();
    const core::dsl::Element* option = ui.find("menu.item.0");
    if (option == nullptr || option->disabled) {
        std::cerr << "uncontrolled dropdown did not open after click\n";
        return false;
    }
    option->onClick();
    compose();
    option = ui.find("menu.item.0");
    if (option == nullptr || !option->disabled) {
        std::cerr << "uncontrolled dropdown did not close after selection\n";
        return false;
    }
    return true;
}

bool dropdownOpenCallbackRemainsControlled() {
    core::dsl::Ui ui;
    bool open = false;
    const auto compose = [&] {
        ui.begin("dropdown.controlled-state");
        components::dropdown(ui, "menu")
            .items({"A"})
            .open(open)
            .onOpenChange([&](bool value) { open = value; })
            .build();
        ui.end();
    };

    compose();
    const core::dsl::Element* field = ui.find("menu.field");
    if (field == nullptr || !field->onClick) return false;
    field->onClick();
    if (!open) {
        std::cerr << "controlled dropdown callback did not receive open state\n";
        return false;
    }
    compose();
    const core::dsl::Element* option = ui.find("menu.item.0");
    if (option == nullptr || option->disabled) {
        std::cerr << "controlled dropdown ignored external open state\n";
        return false;
    }
    return true;
}

bool nestedHighZChildPromotesItsContainer() {
    core::dsl::Ui ui;
    ui.begin("zindex.escape");
    ui.stack("viewport")
        .size(400, 300)
        .content([&] {
            ui.stack("card")
                .size(300, 120)
                .content([&] { ui.rect("popup").size(120, 80).zIndex(100).build(); })
                .build();
            ui.rect("sibling").size(300, 120).zIndex(0).build();
        })
        .build();
    ui.end();
    ui.layout(400, 300);

    const auto* viewport = ui.find("viewport");
    if (viewport == nullptr || viewport->orderedChildren.size() != 2 ||
        viewport->orderedChildren.front()->id != "zindex.escape.sibling" ||
        viewport->orderedChildren.back()->id != "zindex.escape.card") {
        std::cerr << "container with high-z descendant was not promoted";
        if (viewport != nullptr) {
            std::cerr << " children=" << viewport->orderedChildren.size();
            for (const auto* child : viewport->orderedChildren)
                std::cerr << " [" << child->id << ", z=" << child->zIndex
                          << ", subtree=" << child->subtreeMaxZIndex << "]";
        }
        std::cerr << '\n';
        return false;
    }
    return true;
}

bool zeroZDescendantsPreserveInsertionOrder() {
    core::dsl::Ui ui;
    ui.begin("zindex.stable");
    ui.stack("root")
        .size(200, 100)
        .content([&] {
            ui.rect("first").size(20, 20).build();
            ui.rect("second").size(20, 20).build();
        })
        .build();
    ui.end();
    ui.layout(200, 100);
    const auto* root = ui.find("root");
    return root != nullptr && root->orderedChildren.size() == 2 &&
            root->orderedChildren[0]->id == "zindex.stable.first" &&
            root->orderedChildren[1]->id == "zindex.stable.second";
}

bool dropdownCanOpenUpWithoutChangingDefaultDirection() {
    const auto popupRelativeToField = [](bool openUp) -> std::optional<std::pair<float, float>> {
        core::dsl::Ui ui;
        ui.begin("dropdown.direction");
        ui.stack("anchor")
            .position(20, 160)
            .size(280, 100)
            .content([&] {
                components::dropdown(ui, "menu").items({"A", "B"}).open(true).openUp(openUp).build();
            })
            .build();
        ui.end();
        ui.layout(360, 240);
        const auto* field = ui.find("menu.field");
        const auto* popup = ui.find("menu.popup");
        if (!field || !popup) return std::nullopt;
        return std::pair<float, float>{field->frame.y, popup->frame.y};
    };

    const auto upward = popupRelativeToField(true);
    const auto downward = popupRelativeToField(false);
    if (!upward || upward->second >= upward->first) {
        std::cerr << "openUp popup was not positioned above its field\n";
        return false;
    }
    if (!downward || downward->second <= downward->first) {
        std::cerr << "default dropdown direction no longer opens downward\n";
        return false;
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
    ok = controlledScrollOffsetsPreserveUserMotion() && ok;
    ok = blockPointerUsesArrowCursor() && ok;
    ok = textWrapContentUsesIntrinsicSize() && ok;
    ok = textSizeMeasurementMatchesLineLayout() && ok;
    ok = componentDefaultsMatchGallery() && ok;
    ok = dropdownRetainsUncontrolledOpenState() && ok;
    ok = dropdownOpenCallbackRemainsControlled() && ok;
    ok = nestedHighZChildPromotesItsContainer() && ok;
    ok = zeroZDescendantsPreserveInsertionOrder() && ok;
    ok = dropdownCanOpenUpWithoutChangingDefaultDirection() && ok;
    return ok ? 0 : 1;
}
