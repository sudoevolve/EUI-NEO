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
    return ok ? 0 : 1;
}
