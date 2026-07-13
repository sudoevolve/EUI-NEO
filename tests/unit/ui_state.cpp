#include "components/virtuallist.h"
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

bool virtualListScrollRecomposesOnlyAtRowBoundary() {
    core::dsl::Ui ui;
    eui::Signal<float> offset{16.0f};
    ui.begin("virtual.list.test");
    components::virtualList(ui, "list")
        .size(320.0f, 180.0f)
        .itemCount(100)
        .rowHeight(32.0f)
        .bind(offset)
        .row([](core::dsl::Ui& rowUi,
                const std::string& rowId,
                std::int64_t,
                float rowWidth,
                float rowHeight) {
            rowUi.rect(rowId + ".bg").size(rowWidth, rowHeight).build();
        })
        .build();
    ui.stack("all.offsets")
        .composeOnScrollOffsetChangeInterval(32.0f)
        .composeOnScrollOffsetChange()
        .build();
    ui.end();
    ui.layout(320.0f, 180.0f);

    const core::dsl::Element* root = ui.find("list");
    const core::dsl::Element* window = ui.find("list.window");
    const core::dsl::Element* allOffsets = ui.find("all.offsets");
    if (root == nullptr || window == nullptr || allOffsets == nullptr || !root->onScrollOffsetChanged) {
        std::cerr << "virtual list runtime scroll bindings were not composed\n";
        return false;
    }
    if (!root->composeOnScrollOffsetChange ||
        !closeEnough(root->scrollComposeInterval, 32.0f, 0.01f) ||
        window->scrollContentSourceId != root->scrollStateId ||
        !closeEnough(window->transform.translate.y, 16.0f, 0.01f)) {
        std::cerr << "virtual list did not bind its window to the runtime scroll transform\n";
        return false;
    }
    if (!allOffsets->composeOnScrollOffsetChange || allOffsets->scrollComposeInterval != 0.0f) {
        std::cerr << "unthrottled scroll compose retained a stale interval\n";
        return false;
    }

    root->onScrollOffsetChanged(24.0f);
    if (!closeEnough(offset.get(), 24.0f, 0.01f)) {
        std::cerr << "virtual list did not publish its runtime offset\n";
        return false;
    }
    if (core::dsl::scrollComposeBoundaryCrossed(16.0f, 24.0f, root->scrollComposeInterval)) {
        std::cerr << "virtual list compose interval changed within one row\n";
        return false;
    }
    if (!core::dsl::scrollComposeBoundaryCrossed(24.0f, 40.0f, root->scrollComposeInterval) ||
        !core::dsl::scrollComposeBoundaryCrossed(80.0f, 31.0f, root->scrollComposeInterval)) {
        std::cerr << "virtual list compose interval missed a row boundary\n";
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
    ok = virtualListScrollRecomposesOnlyAtRowBoundary() && ok;
    return ok ? 0 : 1;
}
