#include "core/platform/gesture_bridge.h"

#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commctrl.h>
#include <math.h>
#include <stdlib.h>

typedef struct EuiGestureState {
    EuiGestureCallback callback;
    EuiTouchCallback touchCallback;
    int touchRegistered;
    void* userData;
    DWORD activeGesture;
    double previousX;
    double previousY;
    double previousZoom;
    double previousAngle;
    DWORD gestureInputs;
} EuiGestureState;

// WM_TOUCH 使用屏幕百分之一像素，GLFW 使用窗口坐标；保留亚像素精度。
static void eui_touch_dispatch(GLFWwindow* window, EuiGestureState* state,
                                const TOUCHINPUT* inputs, UINT count) {
    HWND hwnd = glfwGetWin32Window(window);
    RECT rect;
    POINT origin = {0, 0};
    int width = 0, height = 0;
    glfwGetWindowSize(window, &width, &height);
    if (!GetClientRect(hwnd, &rect) || !ClientToScreen(hwnd, &origin) ||
        rect.right <= 0 || rect.bottom <= 0) return;
    for (UINT i = 0; i < count; ++i) {
        const TOUCHINPUT* input = &inputs[i];
        EuiGesturePhase phase;
        if (input->dwFlags & TOUCHEVENTF_DOWN) phase = EUI_GESTURE_PHASE_BEGIN;
        else if (input->dwFlags & TOUCHEVENTF_UP) phase = EUI_GESTURE_PHASE_END;
        else if (input->dwFlags & TOUCHEVENTF_MOVE) phase = EUI_GESTURE_PHASE_UPDATE;
        else continue;
        state->touchCallback(state->userData, phase,
            (long long)(INT_PTR)input->hSource, input->dwID,
            (input->x / 100.0 - origin.x) * width / rect.right,
            (input->y / 100.0 - origin.y) * height / rect.bottom);
    }
}

static void eui_gesture_dispatch(GLFWwindow* window,
                                EuiGestureState* state,
                                const GESTUREINFO* info) {
    const HWND hwnd = glfwGetWin32Window(window);
    POINT point = {info->ptsLocation.x, info->ptsLocation.y};
    ScreenToClient(hwnd, &point);

    RECT clientRect;
    int windowWidth = 0;
    int windowHeight = 0;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);
    if (!GetClientRect(hwnd, &clientRect) ||
        clientRect.right <= clientRect.left || clientRect.bottom <= clientRect.top) {
        return;
    }

    const double x = (double)(point.x - clientRect.left) * windowWidth /
                     (clientRect.right - clientRect.left);
    const double y = (double)(point.y - clientRect.top) * windowHeight /
                     (clientRect.bottom - clientRect.top);
    const int begins = (info->dwFlags & GF_BEGIN) != 0;
    const int ends = (info->dwFlags & GF_END) != 0;
    if (info->dwID == GID_BEGIN) {
        return;
    }
    if (info->dwID == GID_END) {
        if (state->activeGesture != 0 && state->callback != NULL) {
            state->callback(state->userData, EUI_GESTURE_PHASE_END,
                            x, y, 0.0, 0.0, 1.0, 0.0, 0);
        }
        state->activeGesture = 0;
        state->previousZoom = 0.0;
        state->gestureInputs = 0;
        return;
    }

    if (info->dwID != GID_PAN && info->dwID != GID_ZOOM && info->dwID != GID_ROTATE) return;
    const DWORD inputBit = 1u << info->dwID;
    const int firstGesture = state->activeGesture == 0;
    if (firstGesture && !begins) return;
    const int firstGestureInput = (state->gestureInputs & inputBit) == 0 || begins;
    state->gestureInputs |= inputBit;
    EuiGesturePhase phase = firstGesture ? EUI_GESTURE_PHASE_BEGIN
                                         : EUI_GESTURE_PHASE_UPDATE;
    double deltaX = 0.0;
    double deltaY = 0.0;
    double scale = 1.0;
    double rotation = 0.0;

    if (info->dwID == GID_PAN) {
        if (!firstGestureInput) {
            deltaX = x - state->previousX;
            deltaY = y - state->previousY;
        }
        state->previousX = x;
        state->previousY = y;
    } else if (info->dwID == GID_ZOOM) {
        const double distance = (double)info->ullArguments;
        if (!firstGestureInput && state->previousZoom > 0.0) {
            scale = distance / state->previousZoom;
        }
        state->previousZoom = distance;
    } else if (info->dwID == GID_ROTATE) {
        const double angle = GID_ROTATE_ANGLE_FROM_ARGUMENT(info->ullArguments);
        if (!firstGestureInput) {
            double delta = angle - state->previousAngle;
            while (delta > 3.14159265358979323846) delta -= 6.28318530717958647692;
            while (delta < -3.14159265358979323846) delta += 6.28318530717958647692;
            rotation = delta * (180.0 / 3.14159265358979323846);
        }
        state->previousAngle = angle;
    } else {
        return;
    }

    if (state->activeGesture == 0) {
        state->activeGesture = info->dwID;
    }
    if (state->callback != NULL) {
        state->callback(state->userData, phase, x, y, deltaX, deltaY, scale, rotation, 0);
    }
    if (ends) {
        state->gestureInputs &= ~inputBit;
    }
}

static LRESULT CALLBACK eui_gesture_subclass_proc(HWND hwnd,
                                                   UINT message,
                                                   WPARAM wParam,
                                                   LPARAM lParam,
                                                   UINT_PTR subclassId,
                                                   DWORD_PTR referenceData) {
    (void)wParam;
    EuiGestureState* state = (EuiGestureState*)referenceData;
    if (state != NULL && (message == WM_KILLFOCUS || message == WM_CANCELMODE)) {
        if (state->touchCallback != NULL) {
            state->touchCallback(state->userData, EUI_GESTURE_PHASE_CANCEL, 0, 0, 0.0, 0.0);
        }
    }
    if (state != NULL && state->activeGesture != 0 &&
        (message == WM_KILLFOCUS || message == WM_CANCELMODE)) {
        if (state->callback != NULL) {
            state->callback(state->userData, EUI_GESTURE_PHASE_CANCEL,
                            state->previousX, state->previousY, 0.0, 0.0, 1.0, 0.0, 0);
        }
        state->activeGesture = 0;
        state->previousZoom = 0.0;
        state->gestureInputs = 0;
    }
    if (state != NULL && (state->activeGesture != 0 || state->touchRegistered) &&
        (((ULONG_PTR)GetMessageExtraInfo() & 0xFFFFFF80u) == 0xFF515780u) &&
        (message == WM_MOUSEMOVE || message == WM_LBUTTONDOWN || message == WM_LBUTTONUP ||
         message == WM_RBUTTONDOWN || message == WM_RBUTTONUP ||
         message == WM_MBUTTONDOWN || message == WM_MBUTTONUP)) {
        return 0;
    }
    if (message == WM_TOUCH && state != NULL && state->touchRegistered) {
        const UINT count = LOWORD(wParam);
        TOUCHINPUT* inputs = (TOUCHINPUT*)calloc(count, sizeof(TOUCHINPUT));
        if (inputs == NULL) return DefSubclassProc(hwnd, message, wParam, lParam);
        if (!GetTouchInputInfo((HTOUCHINPUT)lParam, count, inputs, sizeof(TOUCHINPUT))) {
            free(inputs);
            return DefSubclassProc(hwnd, message, wParam, lParam);
        }
        GLFWwindow* window = (GLFWwindow*)GetPropW(hwnd, L"EuiNeoGestureWindow");
        if (window != NULL) eui_touch_dispatch(window, state, inputs, count);
        free(inputs);
        CloseTouchInputHandle((HTOUCHINPUT)lParam);
        return 0;
    }
    if (message == WM_GESTURE) {
        GESTUREINFO info;
        ZeroMemory(&info, sizeof(info));
        info.cbSize = sizeof(info);
        if (GetGestureInfo((HGESTUREINFO)lParam, &info)) {
            GLFWwindow* window = (GLFWwindow*)GetPropW(hwnd, L"EuiNeoGestureWindow");
            if (window != NULL && state != NULL) {
                eui_gesture_dispatch(window, state, &info);
            }
            if (info.dwID != GID_PAN && info.dwID != GID_ZOOM && info.dwID != GID_ROTATE) {
                return DefSubclassProc(hwnd, message, wParam, lParam);
            }
            CloseGestureInfoHandle((HGESTUREINFO)lParam);
            return 0;
        }
        return DefSubclassProc(hwnd, message, wParam, lParam);
    }
    if (message == WM_NCDESTROY) {
        RemoveWindowSubclass(hwnd, eui_gesture_subclass_proc, subclassId);
        RemovePropW(hwnd, L"EuiNeoGestureWindow");
        free(state);
    }
    return DefSubclassProc(hwnd, message, wParam, lParam);
}

void eui_gesture_install(GLFWwindow* window, EuiGestureCallback callback, void* userData) {
    if (window == NULL || callback == NULL) return;
    HWND hwnd = glfwGetWin32Window(window);
    if (hwnd == NULL) return;

    EuiGestureState* state = (EuiGestureState*)calloc(1, sizeof(EuiGestureState));
    if (state == NULL) return;
    state->callback = callback;
    state->userData = userData;

    GESTURECONFIG config[] = {
        {GID_PAN, GC_PAN, 0},
        {GID_ZOOM, GC_ZOOM, 0},
        {GID_ROTATE, GC_ROTATE, 0}
    };
    if (!SetGestureConfig(hwnd, 0, 3, config, sizeof(GESTURECONFIG))) {
        free(state);
        return;
    }
    SetPropW(hwnd, L"EuiNeoGestureWindow", window);
    if (!SetWindowSubclass(hwnd, eui_gesture_subclass_proc, (UINT_PTR)window, (DWORD_PTR)state)) {
        RemovePropW(hwnd, L"EuiNeoGestureWindow");
        free(state);
    }
}

void eui_gesture_uninstall(GLFWwindow* window) {
    if (window == NULL) return;
    HWND hwnd = glfwGetWin32Window(window);
    if (hwnd == NULL) return;
    DWORD_PTR referenceData = 0;
    if (GetWindowSubclass(hwnd, eui_gesture_subclass_proc, (UINT_PTR)window, &referenceData)) {
        EuiGestureState* state = (EuiGestureState*)referenceData;
        if (state->touchRegistered) UnregisterTouchWindow(hwnd);
        RemoveWindowSubclass(hwnd, eui_gesture_subclass_proc, (UINT_PTR)window);
        RemovePropW(hwnd, L"EuiNeoGestureWindow");
        free(state);
    }
}

void eui_touch_install(GLFWwindow* window, EuiTouchCallback callback) {
    DWORD_PTR data = 0;
    HWND hwnd = glfwGetWin32Window(window);
    if (callback != NULL && GetWindowSubclass(hwnd, eui_gesture_subclass_proc, (UINT_PTR)window, &data)) {
        EuiGestureState* state = (EuiGestureState*)data;
        state->touchCallback = callback;
        // 注册原始触摸后由共享 TouchTracker 识别触屏手势，避免系统识别延迟和触点丢失。
        state->touchRegistered = RegisterTouchWindow(hwnd, 0) != 0;
    }
}

void eui_gesture_reset(GLFWwindow* window) {
    DWORD_PTR data = 0;
    HWND hwnd = glfwGetWin32Window(window);
    if (GetWindowSubclass(hwnd, eui_gesture_subclass_proc, (UINT_PTR)window, &data)) {
        EuiGestureState* state = (EuiGestureState*)data;
        state->activeGesture = 0;
        state->gestureInputs = 0;
        state->previousZoom = 0.0;
    }
}

#if defined(EUI_GESTURE_TESTING)
void eui_test_touch_dispatch(GLFWwindow* window, const void* inputs, unsigned int count) {
    DWORD_PTR data = 0;
    if (GetWindowSubclass(glfwGetWin32Window(window), eui_gesture_subclass_proc, (UINT_PTR)window, &data)) {
        eui_touch_dispatch(window, (EuiGestureState*)data, (const TOUCHINPUT*)inputs, count);
    }
}
void eui_test_gesture_dispatch(GLFWwindow* window, const void* info) {
    DWORD_PTR data = 0;
    if (GetWindowSubclass(glfwGetWin32Window(window), eui_gesture_subclass_proc, (UINT_PTR)window, &data)) {
        eui_gesture_dispatch(window, (EuiGestureState*)data, (const GESTUREINFO*)info);
    }
}
#endif

#elif defined(__APPLE__)
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>
#import <Cocoa/Cocoa.h>
#include <math.h>
#include <stdlib.h>

typedef struct EuiGestureState {
    GLFWwindow* window;
    EuiGestureCallback callback;
    void* userData;
    int active;
    struct EuiGestureState* next;
} EuiGestureState;

static EuiGestureState* eui_gesture_states;
static id eui_gesture_monitor;

static NSEvent* eui_gesture_monitor_event(NSEvent* event) {
    const NSEventType type = [event type];
    if (type != NSEventTypeMagnify && type != NSEventTypeRotate) {
        return event;
    }
    NSWindow* eventWindow = [event window];
    if (eventWindow == nil) return event;

    EuiGestureState* state = eui_gesture_states;
    while (state != NULL && glfwGetCocoaWindow(state->window) != eventWindow) {
        state = state->next;
    }
    if (state == NULL || state->callback == NULL) return event;

    NSView* view = [eventWindow contentView];
    NSPoint location = [view convertPoint:[event locationInWindow] fromView:nil];
    const double x = location.x;
    const double y = [view bounds].size.height - location.y;
    const NSEventPhase phase = [event phase];
    EuiGesturePhase eventPhase = EUI_GESTURE_PHASE_UPDATE;
    if (phase & NSEventPhaseBegan) {
        eventPhase = EUI_GESTURE_PHASE_BEGIN;
        state->active = 1;
    } else if (phase & (NSEventPhaseEnded | NSEventPhaseCancelled)) {
        eventPhase = (phase & NSEventPhaseCancelled)
            ? EUI_GESTURE_PHASE_CANCEL
            : EUI_GESTURE_PHASE_END;
    } else if (!state->active) {
        eventPhase = EUI_GESTURE_PHASE_BEGIN;
        state->active = 1;
    }

    const double scale = type == NSEventTypeMagnify
        ? fmax(0.0001, 1.0 + [event magnification])
        : 1.0;
    const double rotation = type == NSEventTypeRotate ? [event rotation] : 0.0;
    state->callback(state->userData, eventPhase, x, y, 0.0, 0.0, scale, rotation, 0);
    if (eventPhase == EUI_GESTURE_PHASE_END || eventPhase == EUI_GESTURE_PHASE_CANCEL) {
        state->active = 0;
    }
    return event;
}

void eui_gesture_install(GLFWwindow* window, EuiGestureCallback callback, void* userData) {
    if (window == NULL || callback == NULL || glfwGetCocoaWindow(window) == nil) return;
    EuiGestureState* state = (EuiGestureState*)calloc(1, sizeof(EuiGestureState));
    if (state == NULL) return;
    state->window = window;
    state->callback = callback;
    state->userData = userData;
    state->next = eui_gesture_states;
    eui_gesture_states = state;
    if (eui_gesture_monitor == nil) {
        eui_gesture_monitor = [NSEvent addLocalMonitorForEventsMatchingMask:
            (NSEventMaskMagnify | NSEventMaskRotate)
            handler:^NSEvent*(NSEvent* event) {
                return eui_gesture_monitor_event(event);
            }];
    }
}

void eui_gesture_uninstall(GLFWwindow* window) {
    EuiGestureState** current = &eui_gesture_states;
    while (*current != NULL) {
        if ((*current)->window == window) {
            EuiGestureState* removed = *current;
            *current = removed->next;
            free(removed);
            break;
        }
        current = &(*current)->next;
    }
    if (eui_gesture_states == NULL && eui_gesture_monitor != nil) {
        [NSEvent removeMonitor:eui_gesture_monitor];
        eui_gesture_monitor = nil;
    }
}

#else

void eui_gesture_install(GLFWwindow* window, EuiGestureCallback callback, void* userData) {
    (void)window;
    (void)callback;
    (void)userData;
}

void eui_gesture_uninstall(GLFWwindow* window) {
    (void)window;
}

#endif
