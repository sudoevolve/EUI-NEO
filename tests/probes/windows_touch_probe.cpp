#if defined(_WIN32) && !defined(EUI_WINDOW_BACKEND_SDL2)
#define EUI_GESTURE_TESTING 1
#define GLFW_EXPOSE_NATIVE_WIN32
#include "core/input/input_state.h"
#include "core/platform/gesture_bridge.h"
#include <GLFW/glfw3native.h>
#include <cmath>
#include <cstdio>

int main() {
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(200, 100, "Windows touch probe", nullptr, nullptr);
    if (!window) { glfwTerminate(); return 1; }
    core::window::installInputCallbacks(window);
    POINT origin{};
    ClientToScreen(glfwGetWin32Window(window), &origin);
    const auto touch = [&](DWORD flags, DWORD id, double x, double y) {
        TOUCHINPUT input{};
        input.dwFlags = flags;
        input.dwID = id;
        input.hSource = reinterpret_cast<HANDLE>(1);
        input.x = static_cast<LONG>((origin.x + x) * 100);
        input.y = static_cast<LONG>((origin.y + y) * 100);
        eui_test_touch_dispatch(window, &input, 1);
    };
    touch(TOUCHEVENTF_DOWN, 1, 50, 50);
    touch(TOUCHEVENTF_DOWN, 2, 150, 50);
    touch(TOUCHEVENTF_MOVE, 2, 100, 100);
    touch(TOUCHEVENTF_UP, 2, 100, 100);
    touch(TOUCHEVENTF_UP, 1, 50, 50);
    const auto pointer = core::consumePointerEvents(window);
    auto events = core::consumeGestureInput(window);
    bool passed = IsTouchWindow(glfwGetWin32Window(window), nullptr) &&
        pointer.size() == 2 && pointer[0].touch &&
        pointer[1].action == core::PointerAction::Cancel && events.size() == 3 &&
        events[0].phase == core::GesturePhase::Begin &&
        std::fabs(events[1].scale - std::sqrt(0.5)) < 0.001 &&
        std::fabs(events[1].rotation - 45.0) < 0.001 &&
        events[2].phase == core::GesturePhase::End;
    touch(TOUCHEVENTF_DOWN, 3, 50, 50);
    touch(TOUCHEVENTF_DOWN, 4, 150, 50);
    core::consumeGestureInput(window);
    SendMessageW(glfwGetWin32Window(window), WM_CANCELMODE, 0, 0);
    events = core::consumeGestureInput(window);
    passed = passed && events.size() == 1 && events[0].phase == core::GesturePhase::Cancel;
    touch(TOUCHEVENTF_MOVE, 4, 170, 50);
    touch(TOUCHEVENTF_UP, 4, 170, 50);
    passed = passed && core::consumeGestureInput(window).empty();
    GESTUREINFO info{};
    info.ptsLocation.x = static_cast<SHORT>(origin.x + 50);
    info.ptsLocation.y = static_cast<SHORT>(origin.y + 50);
    const auto gesture = [&](DWORD type, DWORD flags, ULONGLONG argument) {
        info.dwID = type; info.dwFlags = flags; info.ullArguments = argument;
        eui_test_gesture_dispatch(window, &info);
    };
    gesture(GID_PAN, GF_BEGIN, 0);
    gesture(GID_ZOOM, GF_BEGIN, 100);
    gesture(GID_ZOOM, 0, 150);
    info.ptsLocation.x += 10;
    gesture(GID_PAN, 0, 0);
    gesture(GID_ZOOM, GF_END, 150);
    gesture(GID_PAN, GF_END, 0);
    gesture(GID_END, 0, 0);
    events = core::consumeGestureInput(window);
    passed = passed && events.size() == 7 && events[0].phase == core::GesturePhase::Begin &&
        events[2].scale == 1.5 && events[3].deltaX == 10 &&
        events[4].phase == core::GesturePhase::Update &&
        events[5].phase == core::GesturePhase::Update && events[6].phase == core::GesturePhase::End;
    gesture(GID_ROTATE, GF_BEGIN, GID_ROTATE_ANGLE_TO_ARGUMENT(0));
    gesture(GID_ROTATE, 0, GID_ROTATE_ANGLE_TO_ARGUMENT(3.14159265358979323846 / 4));
    events = core::consumeGestureInput(window);
    passed = passed && events.size() == 2 && std::fabs(events[1].rotation - 45) < 0.02;
    core::cancelInput(window);
    core::consumeGestureInput(window);
    gesture(GID_ROTATE, 0, GID_ROTATE_ANGLE_TO_ARGUMENT(1));
    gesture(GID_END, 0, 0);
    passed = passed && core::consumeGestureInput(window).empty();
    core::releaseInputQueue(window);
    glfwDestroyWindow(window);
    glfwTerminate();
    if (!passed) std::fprintf(stderr, "Windows native touch/gesture translation failed\n");
    return passed ? 0 : 1;
}
#else
int main() { return 77; }
#endif
