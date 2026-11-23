#pragma once

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum EuiGesturePhase {
    EUI_GESTURE_PHASE_BEGIN,
    EUI_GESTURE_PHASE_UPDATE,
    EUI_GESTURE_PHASE_END,
    EUI_GESTURE_PHASE_CANCEL
} EuiGesturePhase;

typedef void (*EuiGestureCallback)(void* userData,
                                   EuiGesturePhase phase,
                                   double x,
                                   double y,
                                   double deltaX,
                                   double deltaY,
                                   double scale,
                                   double rotation,
                                   unsigned int touchCount);

void eui_gesture_install(GLFWwindow* window, EuiGestureCallback callback, void* userData);
void eui_gesture_uninstall(GLFWwindow* window);

#if defined(_WIN32)
/** @brief Windows 原始触摸回调；phase 为按下、移动、抬起或取消，坐标为窗口坐标。 */
typedef void (*EuiTouchCallback)(void* userData, EuiGesturePhase phase,
                                 long long device, long long id, double x, double y);
void eui_touch_install(GLFWwindow* window, EuiTouchCallback callback);
void eui_gesture_reset(GLFWwindow* window);
#if defined(EUI_GESTURE_TESTING)
void eui_test_touch_dispatch(GLFWwindow* window, const void* inputs, unsigned int count);
void eui_test_gesture_dispatch(GLFWwindow* window, const void* info);
#endif
#endif

#ifdef __cplusplus
}
#endif
