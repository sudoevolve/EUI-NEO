#if defined(EUI_WINDOW_BACKEND_SDL2)

#include "core/input/input_state.h"
#include "eui/dsl_app.h"

#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>

#include <cmath>
#include <cstdio>

bool eui_test_sdl_input_event(SDL_Window* window, const SDL_Event& event, bool inputEnabled);

namespace app {
const DslAppConfig& dslAppConfig() {
    static const DslAppConfig config;
    return config;
}
void compose(eui::Ui&, const eui::Screen&) {}
}

namespace {
int failures = 0;
void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "sdl_touch_probe: %s\n", message);
        ++failures;
    }
}

void finger(SDL_Window* window, Uint32 type, SDL_FingerID id, float x, float y,
            bool inputEnabled = true, Uint32 windowId = 0) {
    SDL_Event event{};
    event.type = type;
    event.tfinger.touchId = 123;
    event.tfinger.fingerId = id;
    event.tfinger.x = x;
    event.tfinger.y = y;
    event.tfinger.windowID = windowId ? windowId : SDL_GetWindowID(window);
    eui_test_sdl_input_event(window, event, inputEnabled);
}

bool closeTo(double a, double b) { return std::fabs(a - b) < 0.001; }
}

int main() {
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window* window = SDL_CreateWindow("touch probe", SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED, 200, 100, SDL_WINDOW_HIDDEN);
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    finger(window, SDL_FINGERDOWN, 1, 0.25f, 0.5f);
    finger(window, SDL_FINGERDOWN, 1, 0.25f, 0.5f);
    finger(window, SDL_FINGERDOWN, 2, 0.75f, 0.5f);
    auto pointer = core::consumePointerEvents(window);
    auto gestures = core::consumeGestureInput(window);
    require(pointer.size() == 2 && pointer[0].isPress(core::PointerButton::Left) &&
            pointer[1].action == core::PointerAction::Cancel && pointer[1].touch,
            "second finger must cancel the first press, and duplicate down must be ignored");
    require(gestures.size() == 1 && gestures[0].phase == core::GesturePhase::Begin &&
            gestures[0].touchCount == 2 && closeTo(gestures[0].x, 100.0) &&
            closeTo(gestures[0].y, 50.0), "incorrect initial gesture");

    finger(window, SDL_FINGERMOTION, 2, 0.5f, 1.0f);
    gestures = core::consumeGestureInput(window);
    require(gestures.size() == 1 && gestures[0].phase == core::GesturePhase::Update &&
            closeTo(gestures[0].deltaX, -25.0) && closeTo(gestures[0].deltaY, 25.0) &&
            closeTo(gestures[0].scale, std::sqrt(0.5)) && closeTo(gestures[0].rotation, 45.0),
            "incorrect pinch, rotation, or pan delta");

    finger(window, SDL_FINGERDOWN, 3, 0.9f, 0.1f);
    require(core::consumeGestureInput(window).empty(), "adding a finger must not create a jump");
    finger(window, SDL_FINGERMOTION, 3, 0.9f, 0.1f);
    gestures = core::consumeGestureInput(window);
    require(gestures.size() == 1 && gestures[0].touchCount == 3 &&
            closeTo(gestures[0].deltaX, 0.0) && closeTo(gestures[0].deltaY, 0.0),
            "third-finger baseline was not reset");
    finger(window, SDL_FINGERUP, 3, 0.9f, 0.1f);
    finger(window, SDL_FINGERMOTION, 2, 0.5f, 1.0f);
    gestures = core::consumeGestureInput(window);
    require(gestures.size() == 1 && gestures[0].touchCount == 2 &&
            closeTo(gestures[0].deltaX, 0.0) && closeTo(gestures[0].deltaY, 0.0),
            "removing a finger must not create a jump");

    finger(window, SDL_FINGERUP, 2, 0.6f, 1.0f);
    finger(window, SDL_FINGERUP, 1, 0.25f, 0.5f);
    gestures = core::consumeGestureInput(window);
    require(gestures.size() == 2 && gestures[0].phase == core::GesturePhase::Update &&
            closeTo(gestures[0].deltaX, 10.0) && gestures[1].phase == core::GesturePhase::End,
            "final finger-up motion must precede exactly one gesture end");
    require(core::consumePointerEvents(window)[0].action != core::PointerAction::Release,
            "gesture must not synthesize a click");

    finger(window, SDL_FINGERDOWN, 4, 0.3f, 0.5f);
    finger(window, SDL_FINGERDOWN, 5, 0.7f, 0.5f);
    core::consumeGestureInput(window);
    SDL_Event lost{};
    lost.type = SDL_WINDOWEVENT;
    lost.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
    eui_test_sdl_input_event(window, lost, true);
    gestures = core::consumeGestureInput(window);
    require(gestures.size() == 1 && gestures[0].phase == core::GesturePhase::Cancel,
            "focus loss must cancel the gesture");
    finger(window, SDL_FINGERUP, 5, 0.7f, 0.5f);
    require(core::consumeGestureInput(window).empty(), "stale finger-up must not end a canceled gesture");

    core::consumePointerEvents(window);
    finger(window, SDL_FINGERDOWN, 6, 0.3f, 0.5f, true, SDL_GetWindowID(window) + 1);
    require(!core::hasPendingPointerInput(window),
            "touch for another window must be ignored");

    SDL_Event noWindow{};
    noWindow.type = SDL_FINGERDOWN;
    noWindow.tfinger.touchId = 123;
    noWindow.tfinger.fingerId = 6;
    noWindow.tfinger.x = 0.3f;
    noWindow.tfinger.y = 0.5f;
    eui_test_sdl_input_event(window, noWindow, true);
    require(!core::hasPendingPointerInput(window), "windowless touch must be ignored");

    SDL_Event synthetic{};
    synthetic.type = SDL_MOUSEBUTTONDOWN;
    synthetic.button.which = SDL_TOUCH_MOUSEID;
    synthetic.button.button = SDL_BUTTON_LEFT;
    eui_test_sdl_input_event(window, synthetic, true);
    require(!core::hasPendingPointerInput(window), "SDL touch-synthesized mouse must be ignored");
    synthetic = {};
    synthetic.type = SDL_FINGERDOWN;
    synthetic.tfinger.touchId = SDL_MOUSE_TOUCHID;
    synthetic.tfinger.windowID = SDL_GetWindowID(window);
    eui_test_sdl_input_event(window, synthetic, true);
    require(!core::hasPendingPointerInput(window), "SDL mouse-synthesized touch must be ignored");

    finger(window, SDL_FINGERDOWN, 6, 0.3f, 0.5f);
    finger(window, SDL_FINGERDOWN, 7, 0.7f, 0.5f);
    core::consumeGestureInput(window);
    finger(window, SDL_FINGERUP, 7, 0.7f, 0.5f, false);
    gestures = core::consumeGestureInput(window);
    require(gestures.size() == 1 && gestures[0].phase == core::GesturePhase::Cancel,
            "disabled input must cancel, not end, a gesture");

    core::consumePointerEvents(window);
    finger(window, SDL_FINGERDOWN, 8, 0.3f, 0.5f);
    core::cancelInput(window);
    core::consumePointerEvents(window);
    finger(window, SDL_FINGERMOTION, 8, 0.4f, 0.5f);
    finger(window, SDL_FINGERUP, 8, 0.4f, 0.5f);
    require(!core::hasPendingPointerInput(window),
            "touch events after an external cancellation must not reactivate the pointer");

    finger(window, SDL_FINGERDOWN, 9, 0.25f, 0.5f);
    finger(window, SDL_FINGERDOWN, 10, 0.75f, 0.5f);
    core::consumeGestureInput(window);
    SDL_SetWindowSize(window, 400, 200);
    finger(window, SDL_FINGERMOTION, 10, 0.75f, 0.5f);
    gestures = core::consumeGestureInput(window);
    require(gestures.size() == 1 && closeTo(gestures[0].scale, 1.0) &&
            closeTo(gestures[0].deltaX, 0.0) && closeTo(gestures[0].deltaY, 0.0),
            "window resize must not create artificial gesture motion");
    finger(window, SDL_FINGERUP, 10, 0.75f, 0.5f);
    finger(window, SDL_FINGERUP, 9, 0.25f, 0.5f);

    core::releaseInputQueue(window);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return failures == 0 ? 0 : 1;
}
#else
int main() { return 77; }
#endif
