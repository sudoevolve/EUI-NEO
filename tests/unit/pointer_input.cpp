#include "core/input/input_state.h"
#include "core/input/touch_tracker.h"

#include <iostream>
#include <limits>
#include <vector>

namespace {

bool verifyPointerQueue() {
    int firstTag = 0;
    int secondTag = 0;
    const auto firstWindow = reinterpret_cast<core::window::Handle>(&firstTag);
    const auto secondWindow = reinterpret_cast<core::window::Handle>(&secondTag);

    core::KeyModifiers modifiers;
    modifiers.shift = true;
    core::queuePointerMotion(firstWindow, 10.0, 20.0, {}, modifiers);
    core::queuePointerButton(firstWindow, 10.0, 20.0,
                             core::PointerButton::Middle,
                             core::PointerAction::Press,
                             modifiers);
    core::queuePointerMotion(firstWindow, 14.0, 25.0,
                             core::PointerButton::Middle,
                             modifiers);
    core::queuePointerButton(firstWindow, 14.0, 25.0,
                             core::PointerButton::Right,
                             core::PointerAction::Press,
                             modifiers);
    core::queuePointerButton(firstWindow, 14.0, 25.0,
                             core::PointerButton::Right,
                             core::PointerAction::Release,
                             modifiers);
    core::queuePointerButton(firstWindow, 14.0, 25.0,
                             core::PointerButton::Middle,
                             core::PointerAction::Release,
                             modifiers);

    if (!core::hasPendingPointerInput(firstWindow) ||
        core::hasPendingPointerInput(secondWindow)) {
        return false;
    }

    const std::vector<core::PointerEvent> events = core::consumePointerEvents(firstWindow);
    core::releaseInputQueue(firstWindow);
    core::releaseInputQueue(secondWindow);
    return events.size() == 6 &&
        events[0].action == core::PointerAction::Move &&
        events[1].isPress(core::PointerButton::Middle) &&
        events[1].modifiers.shift &&
        events[2].action == core::PointerAction::Move &&
        events[2].isDown(core::PointerButton::Middle) &&
        events[3].isPress(core::PointerButton::Right) &&
        events[3].isDown(core::PointerButton::Middle) &&
        events[3].isDown(core::PointerButton::Right) &&
        events[4].isRelease(core::PointerButton::Right) &&
        events[4].isDown(core::PointerButton::Middle) &&
        events[5].isRelease(core::PointerButton::Middle) &&
        events[5].buttons.empty();
}

bool verifyPointerCancel() {
    int windowTag = 0;
    const auto window = reinterpret_cast<core::window::Handle>(&windowTag);
    core::queuePointerButton(window, 5.0, 7.0,
                             core::PointerButton::Right,
                             core::PointerAction::Press,
                             {});
    core::cancelPointerInput(window);
    const std::vector<core::PointerEvent> events = core::consumePointerEvents(window);
    core::releaseInputQueue(window);
    return events.size() == 2 &&
        events[0].isPress(core::PointerButton::Right) &&
        events[1].action == core::PointerAction::Cancel &&
        events[1].button == core::PointerButton::Right &&
        events[1].buttons.empty();
}

bool verifyTouchPointerCancel() {
    int windowTag = 0;
    const auto window = reinterpret_cast<core::window::Handle>(&windowTag);
    core::queuePointerButton(window, 5.0, 7.0,
                             core::PointerButton::Left,
                             core::PointerAction::Press,
                             {}, true);
    core::cancelTouchPointerInput(window);
    const std::vector<core::PointerEvent> touchEvents = core::consumePointerEvents(window);
    if (touchEvents.size() != 2 ||
        touchEvents[1].action != core::PointerAction::Cancel ||
        !touchEvents[1].touch ||
        !touchEvents[1].buttons.empty()) {
        return false;
    }

    core::queuePointerButton(window, 5.0, 7.0, core::PointerButton::Left,
                             core::PointerAction::Press, {}, true);
    core::cancelInput(window);
    const auto focusCanceled = core::consumePointerEvents(window);
    if (focusCanceled.size() != 2 || !focusCanceled[1].touch ||
        focusCanceled[1].action != core::PointerAction::Cancel) return false;

    core::queuePointerButton(window, 5.0, 7.0,
                             core::PointerButton::Left,
                             core::PointerAction::Press,
                             {});
    core::cancelTouchPointerInput(window);
    const std::vector<core::PointerEvent> mouseEvents = core::consumePointerEvents(window);
    core::cancelPointerInput(window);
    core::releaseInputQueue(window);
    return mouseEvents.size() == 1 &&
        mouseEvents[0].isPress(core::PointerButton::Left) &&
        mouseEvents[0].buttons.contains(core::PointerButton::Left);
}

bool verifySideButtons() {
    int windowTag = 0;
    const auto window = reinterpret_cast<core::window::Handle>(&windowTag);
    core::queuePointerButton(window, 5.0, 7.0,
                             core::PointerButton::X1,
                             core::PointerAction::Press,
                             {});
    core::queuePointerButton(window, 5.0, 7.0,
                             core::PointerButton::X2,
                             core::PointerAction::Press,
                             {});
    core::queuePointerButton(window, 5.0, 7.0,
                             core::PointerButton::X1,
                             core::PointerAction::Release,
                             {});
    core::queuePointerButton(window, 5.0, 7.0,
                             core::PointerButton::X2,
                             core::PointerAction::Release,
                             {});
    const std::vector<core::PointerEvent> events = core::consumePointerEvents(window);
    core::releaseInputQueue(window);
    return events.size() == 4 &&
        events[0].isPress(core::PointerButton::X1) &&
        events[1].isPress(core::PointerButton::X2) &&
        events[1].isDown(core::PointerButton::X1) &&
        events[2].isRelease(core::PointerButton::X1) &&
        events[2].isDown(core::PointerButton::X2) &&
        events[3].isRelease(core::PointerButton::X2) &&
        events[3].buttons.empty();
}

bool verifyComposingKeyOrder() {
    int windowTag = 0;
    const auto window = reinterpret_cast<core::window::Handle>(&windowTag);
    core::queueTextEditing(window, "composition");
    core::queueKeyInput(window, {
        core::InputKey::Backspace, core::KeyAction::Press, {}
    });
    core::queueKeyInput(window, {
        core::InputKey::Backspace, core::KeyAction::Release, {}
    });
    const std::vector<core::KeyEvent> events = core::consumeKeyEvents(window);
    core::releaseInputQueue(window);
    return events.size() == 2 &&
        events[0].key == core::InputKey::Backspace &&
        events[0].action == core::KeyAction::Press &&
        events[1].key == core::InputKey::Backspace &&
        events[1].action == core::KeyAction::Release;
}

bool verifyKeyboardCancel() {
    int windowTag = 0;
    const auto window = reinterpret_cast<core::window::Handle>(&windowTag);
    core::KeyModifiers modifiers;
    modifiers.shift = true;
    core::queueKeyInput(window, {
        core::InputKey::LeftShift, core::KeyAction::Press, modifiers
    });
    core::queueKeyInput(window, {
        core::InputKey::Unknown, core::KeyAction::Press, modifiers, 123
    });
    core::cancelKeyboardInput(window);
    const std::vector<core::KeyEvent> events = core::consumeKeyEvents(window);
    const core::KeyModifiers current = core::detail::currentModifiers(window);
    core::releaseInputQueue(window);
    return events.size() == 3 &&
        events[0].key == core::InputKey::LeftShift &&
        events[0].action == core::KeyAction::Press &&
        events[1].key == core::InputKey::Unknown &&
        events[1].scanCode == 123 &&
        events[2].key == core::InputKey::LeftShift &&
        events[2].action == core::KeyAction::Release &&
        !current.shift;
}

bool verifyPointerOutsideWithoutPosition() {
    int windowTag = 0;
    const auto window = reinterpret_cast<core::window::Handle>(&windowTag);
    const std::vector<core::PointerEvent> events = core::consumePointerEvents(window);
    if (events.size() != 1 || events[0].action != core::PointerAction::Move ||
        events[0].x >= -999999.0 || events[0].y >= -999999.0) {
        core::releaseInputQueue(window);
        return false;
    }
    core::queuePointerMotion(window, 40.0, 50.0, {}, {});
    const std::vector<core::PointerEvent> entered = core::consumePointerEvents(window);
    core::releaseInputQueue(window);
    return entered.size() == 1 && entered[0].deltaX == 0.0 && entered[0].deltaY == 0.0;
}

bool verifyTouchScrollQueue() {
    int windowTag = 0;
    const auto window = reinterpret_cast<core::window::Handle>(&windowTag);
    core::queueTouchScrollInput(window, 3.0, -24.0);
    const core::ScrollEvent scroll = core::consumeScrollInput(window);
    core::queueTouchScrollInput(window, 5.0, -12.0);
    core::clearTouchScrollInput(window);
    const core::ScrollEvent discarded = core::consumeScrollInput(window);
    core::releaseInputQueue(window);
    return scroll.x == 0.0 && scroll.y == 0.0 &&
           scroll.pixelX == 3.0 && scroll.pixelY == -24.0 && scroll.active() &&
           !discarded.active();
}

bool verifyGestureQueue() {
    int windowTag = 0;
    const auto window = reinterpret_cast<core::window::Handle>(&windowTag);
    core::GestureEvent begin;
    begin.phase = core::GesturePhase::Begin;
    begin.x = 12.0;
    begin.y = 18.0;
    begin.touchCount = 2;
    core::queueGestureInput(window, begin);

    core::GestureEvent update;
    update.phase = core::GesturePhase::Update;
    update.x = 14.0;
    update.y = 21.0;
    update.deltaX = 2.0;
    update.deltaY = 3.0;
    update.scale = 1.25;
    update.rotation = -15.0;
    update.touchCount = 2;
    core::queueGestureInput(window, update);

    const std::vector<core::GestureEvent> events = core::consumeGestureInput(window, 2.0f);
    if (events.size() != 2 ||
        events[0].phase != core::GesturePhase::Begin ||
        events[0].x != 24.0 || events[0].y != 36.0 ||
        events[1].phase != core::GesturePhase::Update ||
        events[1].deltaX != 4.0 || events[1].deltaY != 6.0 ||
        events[1].scale != 1.25 || events[1].rotation != -15.0 ||
        events[1].touchCount != 2) {
        return false;
    }

    core::queueGestureInput(window, begin);
    core::cancelInput(window);
    const std::vector<core::GestureEvent> canceled = core::consumeGestureInput(window);
    core::releaseInputQueue(window);
    return canceled.size() == 2 &&
        canceled[0].phase == core::GesturePhase::Begin &&
        canceled[1].phase == core::GesturePhase::Cancel;
}

bool verifyTouchTracker() {
    int windowTag = 0;
    const auto window = reinterpret_cast<core::window::Handle>(&windowTag);
    core::input::TouchTracker tracker(window);
    tracker.down(1, 1, 50.0, 50.0);
    tracker.down(1, 1, 50.0, 50.0);
    tracker.down(1, 2, 150.0, 50.0);
    const auto pointer = core::consumePointerEvents(window);
    auto gestures = core::consumeGestureInput(window);
    if (pointer.size() != 2 || !pointer[0].isPress(core::PointerButton::Left) ||
        pointer[1].action != core::PointerAction::Cancel || !pointer[1].touch ||
        gestures.size() != 1 || gestures[0].phase != core::GesturePhase::Begin ||
        gestures[0].x != 100.0 || gestures[0].touchCount != 2) {
        return false;
    }
    tracker.move(1, 2, 100.0, 100.0, 2.0f);
    gestures = core::consumeGestureInput(window, 2.0f);
    if (gestures.size() != 1 || std::fabs(gestures[0].scale - std::sqrt(0.5)) > 0.001 ||
        std::fabs(gestures[0].rotation - 45.0) > 0.001 ||
        gestures[0].deltaX != -50.0 || gestures[0].deltaY != 50.0) {
        return false;
    }
    tracker.down(1, 3, 180.0, 10.0);
    tracker.move(1, 3, 180.0, 10.0, 1.0f);
    gestures = core::consumeGestureInput(window);
    if (gestures.size() != 1 || gestures[0].deltaX != 0.0 || gestures[0].deltaY != 0.0 ||
        gestures[0].touchCount != 3) {
        return false;
    }
    tracker.up(1, 3, 180.0, 10.0);
    tracker.up(1, 2, 120.0, 100.0);
    tracker.up(1, 1, 50.0, 50.0);
    gestures = core::consumeGestureInput(window);
    tracker.reset();
    core::releaseInputQueue(window);
    return gestures.size() == 2 && gestures[0].phase == core::GesturePhase::Update &&
           gestures[0].deltaX == 10.0 && gestures[1].phase == core::GesturePhase::End;
}

bool verifyGestureMotionOrdering() {
    int windowTag = 0;
    const auto window = reinterpret_cast<core::window::Handle>(&windowTag);
    core::queuePointerMotion(window, 10.0, 10.0, {}, {});
    core::GestureEvent gesture;
    gesture.phase = core::GesturePhase::Begin;
    core::queueGestureInput(window, gesture);
    core::queuePointerMotion(window, 20.0, 20.0, {}, {});
    std::vector<std::uint64_t> pointerSequences;
    std::vector<std::uint64_t> gestureSequences;
    const auto pointer = core::consumePointerEvents(window, 1.0f, &pointerSequences);
    const auto gestures = core::consumeGestureInput(window, 1.0f, &gestureSequences);
    core::releaseInputQueue(window);
    return pointer.size() == 2 && gestures.size() == 1 &&
        pointerSequences.size() == 2 && gestureSequences.size() == 1 &&
        pointerSequences[0] < gestureSequences[0] &&
        gestureSequences[0] < pointerSequences[1];
}

bool verifyAllFingerGeometry() {
    int tag = 0;
    const auto window = reinterpret_cast<core::window::Handle>(&tag);
    core::input::TouchTracker tracker(window);
    tracker.down(1, 1, -100, 0);
    tracker.down(1, 2, 100, 0);
    tracker.down(1, 3, 0, 100);
    core::consumeGestureInput(window);
    tracker.move(1, 3, 60, 200, 1);
    auto events = core::consumeGestureInput(window);
    bool passed = events.size() == 1 && events[0].touchCount == 3 &&
        events[0].scale > 1.2 && events[0].rotation < -5 &&
        std::fabs(events[0].deltaX - 20) < 0.001;
    tracker.up(1, 1, -100, 0);
    tracker.move(1, 3, 60, 200, 1);
    events = core::consumeGestureInput(window);
    passed = passed && events.size() == 1 && events[0].scale == 1 &&
        events[0].rotation == 0 && events[0].deltaX == 0;
    tracker.down(2, 9, 1000, 1000);
    tracker.move(2, 9, 2000, 2000, 1);
    tracker.move(1, 3, std::numeric_limits<double>::quiet_NaN(), 0, 1);
    passed = passed && core::consumeGestureInput(window).empty();
    tracker.up(1, 3, std::numeric_limits<double>::infinity(), 0);
    events = core::consumeGestureInput(window);
    passed = passed && events.size() == 1 && events[0].phase == core::GesturePhase::End &&
        std::isfinite(events[0].x);
    tracker.up(1, 2, 100, 0);
    tracker.down(1, 1, 0, 0);
    tracker.down(1, 2, 0, 0);
    core::consumeGestureInput(window);
    tracker.move(1, 2, 100, 0, 1);
    events = core::consumeGestureInput(window);
    passed = passed && events.size() == 1 && events[0].scale == 1 && events[0].rotation == 0;
    tracker.move(1, 2, 200, 0, 1);
    events = core::consumeGestureInput(window);
    passed = passed && events.size() == 1 && events[0].scale == 2;
    tracker.up(1, 2, 200, 0);
    core::consumeGestureInput(window);
    tracker.move(1, 1, 50, 0, 1);
    tracker.up(1, 1, 50, 0);
    const auto pointer = core::consumePointerEvents(window);
    for (const auto& event : pointer) {
        passed = passed && event.action != core::PointerAction::Release;
    }
    passed = passed && !core::consumeScrollInput(window).active() && tracker.empty();
    core::releaseInputQueue(window);
    return passed;
}

bool verifyInteractionCapture() {
    const core::Rect bounds{0.0f, 0.0f, 100.0f, 100.0f};
    const core::PointerButtons accepted =
        core::PointerButton::Middle | core::PointerButton::Right;

    core::PointerEvent middlePress;
    middlePress.x = 20.0;
    middlePress.y = 30.0;
    middlePress.action = core::PointerAction::Press;
    middlePress.button = core::PointerButton::Middle;
    middlePress.buttons = core::PointerButton::Middle;

    core::InteractionState defaultInteraction;
    defaultInteraction.update(bounds, middlePress, true, core::PointerButton::Left);
    if (defaultInteraction.active || defaultInteraction.pressStarted) {
        return false;
    }

    core::InteractionState interaction;
    interaction.update(bounds, middlePress, true, accepted);
    if (!interaction.pressStarted || interaction.activeButton != core::PointerButton::Middle) {
        return false;
    }

    core::PointerEvent shortMove = middlePress;
    shortMove.x += 3.0;
    shortMove.action = core::PointerAction::Move;
    shortMove.button = core::PointerButton::None;
    interaction.update(bounds, shortMove, true, accepted, 5.0);
    if (interaction.drag) {
        return false;
    }

    core::PointerEvent dragMove = shortMove;
    dragMove.x += 3.0;
    interaction.update(bounds, dragMove, true, accepted, 5.0);
    if (!interaction.drag) {
        return false;
    }

    core::PointerEvent unrelatedRelease = middlePress;
    unrelatedRelease.action = core::PointerAction::Release;
    unrelatedRelease.button = core::PointerButton::Right;
    interaction.update(bounds, unrelatedRelease, true, accepted);
    if (!interaction.active || interaction.released) {
        return false;
    }

    core::PointerEvent middleRelease = middlePress;
    middleRelease.action = core::PointerAction::Release;
    middleRelease.buttons = {};
    interaction.update(bounds, middleRelease, true, accepted);
    return !interaction.active && interaction.released && interaction.clicked;
}

} // namespace

int main() {
    if (!verifyPointerQueue()) {
        std::cerr << "Pointer event ordering or window isolation failed\n";
        return 1;
    }
    if (!verifyPointerCancel()) {
        std::cerr << "Pointer cancellation did not release held buttons\n";
        return 1;
    }
    if (!verifyTouchPointerCancel()) {
        std::cerr << "Touch gesture cancellation affected the mouse pointer\n";
        return 1;
    }
    if (!verifySideButtons()) {
        std::cerr << "X1/X2 button state or event ordering failed\n";
        return 1;
    }
    if (!verifyPointerOutsideWithoutPosition()) {
        std::cerr << "Unknown pointer position produced a false hover position\n";
        return 1;
    }
    if (!verifyTouchScrollQueue()) {
        std::cerr << "Touch scroll deltas were not preserved\n";
        return 1;
    }
    if (!verifyGestureQueue()) {
        std::cerr << "Gesture phase, scaling, or cancellation was not preserved\n";
        return 1;
    }
    if (!verifyTouchTracker()) {
        std::cerr << "Multi-touch tracking or gesture geometry failed\n";
        return 1;
    }
    if (!verifyGestureMotionOrdering()) {
        std::cerr << "Pointer motion was coalesced across a gesture event\n";
        return 1;
    }
    if (!verifyAllFingerGeometry()) {
        std::cerr << "All-finger geometry, rebasing, or degenerate contact handling failed\n";
        return 1;
    }
    if (!verifyComposingKeyOrder()) {
        std::cerr << "Composition filtering broke raw key event ordering\n";
        return 1;
    }
    if (!verifyKeyboardCancel()) {
        std::cerr << "Keyboard cancellation did not release known held keys\n";
        return 1;
    }
    if (!verifyInteractionCapture()) {
        std::cerr << "Pointer button acceptance or gesture capture failed\n";
        return 1;
    }
    return 0;
}
