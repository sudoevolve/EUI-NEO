#include "core/input/input_state.h"

#include <iostream>
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
    return events.size() == 2 &&
        events[0].isPress(core::PointerButton::Right) &&
        events[1].action == core::PointerAction::Cancel &&
        events[1].button == core::PointerButton::Right &&
        events[1].buttons.empty();
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
        return false;
    }
    core::queuePointerMotion(window, 40.0, 50.0, {}, {});
    const std::vector<core::PointerEvent> entered = core::consumePointerEvents(window);
    return entered.size() == 1 && entered[0].deltaX == 0.0 && entered[0].deltaY == 0.0;
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
    if (!verifySideButtons()) {
        std::cerr << "X1/X2 button state or event ordering failed\n";
        return 1;
    }
    if (!verifyPointerOutsideWithoutPosition()) {
        std::cerr << "Unknown pointer position produced a false hover position\n";
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
