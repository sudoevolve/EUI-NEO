#if defined(__linux__) && !defined(__ANDROID__) && !defined(EUI_WINDOW_BACKEND_SDL2)

#define EUI_LINUX_TOUCH_TESTING 1
#include "core/platform/linux_touch.h"
#include "core/input/input_state.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

int main() {
#if !defined(GLFW_PLATFORM_X11) || !defined(GLFW_PLATFORM_WAYLAND)
    return 77;
#else
    const char* backend = std::getenv("EUI_TEST_GLFW_PLATFORM");
    if (backend && std::strcmp(backend, "x11") == 0) {
        glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
    } else if (backend && std::strcmp(backend, "wayland") == 0) {
        glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);
    }
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(200, 100, "Linux touch probe", nullptr, nullptr);
    GLFWwindow* second = glfwCreateWindow(200, 100, "Linux touch isolation probe", nullptr, nullptr);
    if (!window || !second) {
        if (window) glfwDestroyWindow(window);
        if (second) glfwDestroyWindow(second);
        glfwTerminate();
        return 2;
    }
    core::window::installInputCallbacks(window);
    core::window::installInputCallbacks(second);
    bool passed = core::platform::linuxTouchReady(window) &&
                  core::platform::linuxTouchReady(second) &&
                  core::platform::injectLinuxTouchTestEvents(window);
    core::window::pollTouchInput(window, 1.0f);
    core::window::pollTouchInput(second, 1.0f);
    const auto pointer = core::consumePointerEvents(window);
    const auto gestures = core::consumeGestureInput(window);
    passed = passed && core::consumeGestureInput(second).empty() &&
             !core::hasPendingPointerInput(second);
    passed = passed && pointer.size() == 2 &&
             pointer[0].isPress(core::PointerButton::Left) &&
             pointer[1].action == core::PointerAction::Cancel &&
             gestures.size() >= 3 &&
             gestures[0].phase == core::GesturePhase::Begin &&
             gestures[1].phase == core::GesturePhase::Update &&
             std::fabs(gestures[1].scale - std::sqrt(0.5)) < 0.001 &&
             std::fabs(gestures[1].rotation - 45.0) < 0.001 &&
             gestures[2].phase == core::GesturePhase::End;
    if (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND || gestures.size() == 10) {
        passed = passed && gestures.size() == 10 &&
                 gestures[3].phase == core::GesturePhase::Begin &&
                 gestures[4].phase == core::GesturePhase::Update &&
                 gestures[4].scale == 1.5 && gestures[4].rotation == 10.0 &&
                 gestures[4].deltaX == 4.0 && gestures[4].deltaY == 5.0 &&
                 gestures[5].scale == 2.0 && gestures[5].rotation == -5.0 &&
                 gestures[6].phase == core::GesturePhase::Cancel &&
                 gestures[7].phase == core::GesturePhase::Begin &&
                 gestures[7].touchCount == 3 &&
                 gestures[8].phase == core::GesturePhase::Update &&
                 gestures[8].deltaX == -3.0 && gestures[8].deltaY == 7.0 &&
                 gestures[9].phase == core::GesturePhase::End;
    }
    core::releaseInputQueue(window);
    glfwDestroyWindow(window);
    passed = passed && core::platform::linuxTouchReady(second) &&
             core::platform::injectLinuxTouchTestEvents(second);
    core::window::pollTouchInput(second, 1.0f);
    passed = passed && !core::consumeGestureInput(second).empty();
    core::releaseInputQueue(second);
    glfwDestroyWindow(second);
    glfwTerminate();
    if (!passed) std::fprintf(stderr, "Linux touch backend probe failed\n");
    return passed ? 0 : 1;
#endif
}
#else
int main() { return 77; }
#endif
