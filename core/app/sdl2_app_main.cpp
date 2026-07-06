#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#endif

#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>
#if defined(__ANDROID__)
#include <SDL_system.h>
#include <jni.h>
#endif
#if defined(EUI_RENDER_BACKEND_VULKAN)
#include <SDL_vulkan.h>
#endif

#include "eui/app.h"
#include "core/app/app_runner.h"
#include "core/app/dsl_window_manager.h"
#include "core/app/dsl_window_runtime.h"
#include "core/app/main_window_runtime.h"
#include "core/input/input_state.h"
#include "core/platform/platform.h"
#include "core/platform/native_bridge.h"
#include "core/render/render_backend.h"
#include "core/window/window_backend.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <thread>
#include <unordered_map>
#include <vector>

namespace {

struct WindowState : app::AppRunner {
    bool running = true;
    core::render::RenderBackend* renderBackend = nullptr;
#ifdef __ANDROID__
    bool surfaceResetPending = false;
#endif
#if defined(EUI_RENDER_BACKEND_OPENGL) && (defined(_WIN32) || defined(__APPLE__))
    // SDL2 can expose the OpenGL window before the drawable/backbuffer settles.
    int startupFullPaintFrames = 4;
#endif
};

struct ManagedWindow {
    SDL_Window* window = nullptr;
    bool closeRequested = false;
    SDL_Window* parentWindow = nullptr;
    app::DslWindowRuntime content;
    std::unique_ptr<core::render::RenderBackend> renderBackend;
};

struct TouchScrollState {
    SDL_FingerID fingerId = 0;
    double lastX = 0.0;
    double lastY = 0.0;
    double totalX = 0.0;
    double totalY = 0.0;
    double downTime = 0.0;
    bool active = false;
    bool scrolling = false;
    bool contextQueued = false;
    bool contextReleaseQueued = false;
};

struct TimerResolutionGuard {
    TimerResolutionGuard() {
#ifdef _WIN32
        timeBeginPeriod(1);
#endif
    }

    ~TimerResolutionGuard() {
#ifdef _WIN32
        timeEndPeriod(1);
#endif
    }
};

void getDrawableSize(SDL_Window* window, int& width, int& height) {
#if defined(EUI_RENDER_BACKEND_VULKAN)
    SDL_Vulkan_GetDrawableSize(window, &width, &height);
#else
    SDL_GL_GetDrawableSize(window, &width, &height);
#endif
}

float pointerScale(SDL_Window* window) {
    int windowWidth = 0;
    int windowHeight = 0;
    int drawableWidth = 0;
    int drawableHeight = 0;
    SDL_GetWindowSize(window, &windowWidth, &windowHeight);
    getDrawableSize(window, drawableWidth, drawableHeight);
    if (windowWidth <= 0 || windowHeight <= 0) {
        return 1.0f;
    }
    const float scaleX = static_cast<float>(drawableWidth) / static_cast<float>(windowWidth);
    const float scaleY = static_cast<float>(drawableHeight) / static_cast<float>(windowHeight);
    return (scaleX + scaleY) * 0.5f;
}

double refreshRate(SDL_Window* window) {
    const int display = SDL_GetWindowDisplayIndex(window);
    SDL_DisplayMode mode{};
    if (display >= 0 && SDL_GetCurrentDisplayMode(display, &mode) == 0 && mode.refresh_rate > 0) {
        return static_cast<double>(mode.refresh_rate);
    }
    return 60.0;
}

#ifdef __ANDROID__
float androidUiScaleFromDensity(float density) {
    // The gallery and current EUI examples are authored closer to desktop logical
    // pixels than native Android dp. Full 3x+ phone density makes the UI cramped,
    // so cap Android's effective logical scale while preserving high-DPI rendering.
    return std::clamp(density, 1.0f, 2.0f);
}
#endif

float dpiScale(SDL_Window* window) {
#ifdef _WIN32
    HWND hwnd = static_cast<HWND>(core::window::nativeWindowInfo(window).platformWindow);
    if (hwnd != nullptr) {
        const UINT dpi = GetDpiForWindow(hwnd);
        if (dpi > 0) {
            return static_cast<float>(dpi) / 96.0f;
        }
    }
#endif
#ifdef __ANDROID__
    JNIEnv* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
    if (env != nullptr && activity != nullptr) {
        jclass cls = env->GetObjectClass(activity);
        if (cls != nullptr) {
            jmethodID method = env->GetMethodID(cls, "displayDensity", "()F");
            if (method != nullptr && !env->ExceptionCheck()) {
                const float density = env->CallFloatMethod(activity, method);
                if (!env->ExceptionCheck() && density > 0.0f) {
                    env->DeleteLocalRef(cls);
                    env->DeleteLocalRef(activity);
                    return androidUiScaleFromDensity(density);
                }
            }
            if (env->ExceptionCheck()) {
                env->ExceptionClear();
            }
            env->DeleteLocalRef(cls);
        }
        env->DeleteLocalRef(activity);
    }

    const int display = SDL_GetWindowDisplayIndex(window);
    float diagonalDpi = 0.0f;
    float horizontalDpi = 0.0f;
    float verticalDpi = 0.0f;
    if (display >= 0 &&
        SDL_GetDisplayDPI(display, &diagonalDpi, &horizontalDpi, &verticalDpi) == 0) {
        const float dpi = horizontalDpi > 0.0f && verticalDpi > 0.0f
            ? (horizontalDpi + verticalDpi) * 0.5f
            : diagonalDpi;
        if (dpi > 0.0f) {
            return androidUiScaleFromDensity(dpi / 160.0f);
        }
    }
#endif
    return pointerScale(window);
}

void attachNativeChildWindow(SDL_Window* parentWindow, SDL_Window* childWindow) {
    eui_set_native_child_window(core::window::nativeWindowInfo(parentWindow).platformWindow,
                                core::window::nativeWindowInfo(childWindow).platformWindow,
                                1);
}

void detachNativeChildWindow(SDL_Window* parentWindow, SDL_Window* childWindow) {
    eui_set_native_child_window(core::window::nativeWindowInfo(parentWindow).platformWindow,
                                core::window::nativeWindowInfo(childWindow).platformWindow,
                                0);
}

void updateFrameInterval(SDL_Window* window, WindowState& state) {
    state.updateFrameInterval(refreshRate(window), core::window::timeSeconds());
}

bool mapKey(SDL_Keycode key, core::InputKey& mapped) {
    switch (key) {
    case SDLK_BACKSPACE: mapped = core::InputKey::Backspace; return true;
    case SDLK_DELETE: mapped = core::InputKey::Delete; return true;
    case SDLK_RETURN:
    case SDLK_KP_ENTER: mapped = core::InputKey::Enter; return true;
    case SDLK_LEFT: mapped = core::InputKey::Left; return true;
    case SDLK_RIGHT: mapped = core::InputKey::Right; return true;
    case SDLK_UP: mapped = core::InputKey::Up; return true;
    case SDLK_DOWN: mapped = core::InputKey::Down; return true;
    case SDLK_HOME: mapped = core::InputKey::Home; return true;
    case SDLK_END: mapped = core::InputKey::End; return true;
    case SDLK_ESCAPE: mapped = core::InputKey::Escape; return true;
    case SDLK_a: mapped = core::InputKey::A; return true;
    case SDLK_c: mapped = core::InputKey::C; return true;
    case SDLK_v: mapped = core::InputKey::V; return true;
    case SDLK_x: mapped = core::InputKey::X; return true;
    case SDLK_y: mapped = core::InputKey::Y; return true;
    case SDLK_z: mapped = core::InputKey::Z; return true;
    default: return false;
    }
}

bool isFingerEvent(Uint32 type) {
    return type == SDL_FINGERDOWN || type == SDL_FINGERMOTION || type == SDL_FINGERUP;
}

bool isAndroidLifecycleEvent(Uint32 type) {
#ifdef __ANDROID__
    return type == SDL_APP_WILLENTERBACKGROUND ||
           type == SDL_APP_DIDENTERBACKGROUND ||
           type == SDL_APP_WILLENTERFOREGROUND ||
           type == SDL_APP_DIDENTERFOREGROUND ||
           type == SDL_APP_LOWMEMORY;
#else
    (void)type;
    return false;
#endif
}

bool isRoutedEvent(const SDL_Event& event) {
    return event.type == SDL_WINDOWEVENT ||
           event.type == SDL_KEYDOWN ||
           event.type == SDL_TEXTINPUT ||
           event.type == SDL_TEXTEDITING ||
           event.type == SDL_MOUSEWHEEL ||
           isFingerEvent(event.type);
}

Uint32 routedWindowId(const SDL_Event& event) {
    if (event.type == SDL_WINDOWEVENT) {
        return event.window.windowID;
    }
    if (event.type == SDL_KEYDOWN) {
        return event.key.windowID;
    }
    if (event.type == SDL_TEXTINPUT) {
        return event.text.windowID;
    }
    if (event.type == SDL_TEXTEDITING) {
        return event.edit.windowID;
    }
    if (event.type == SDL_MOUSEWHEEL) {
        return event.wheel.windowID;
    }
    if (isFingerEvent(event.type)) {
        return event.tfinger.windowID;
    }
    return 0;
}

void queueFingerPointer(SDL_Window* window, const SDL_TouchFingerEvent& finger, bool down) {
    int windowWidth = 0;
    int windowHeight = 0;
    SDL_GetWindowSize(window, &windowWidth, &windowHeight);
    if (windowWidth <= 0 || windowHeight <= 0) {
        return;
    }
    core::queuePointerInput(window,
                            static_cast<double>(finger.x) * static_cast<double>(windowWidth),
                            static_cast<double>(finger.y) * static_cast<double>(windowHeight),
                            down);
}

std::unordered_map<SDL_Window*, TouchScrollState>& touchScrollStates() {
    static std::unordered_map<SDL_Window*, TouchScrollState> states;
    return states;
}

bool fingerWindowPosition(SDL_Window* window, const SDL_TouchFingerEvent& finger, double& x, double& y) {
    int windowWidth = 0;
    int windowHeight = 0;
    SDL_GetWindowSize(window, &windowWidth, &windowHeight);
    if (windowWidth <= 0 || windowHeight <= 0) {
        return false;
    }
    x = static_cast<double>(finger.x) * static_cast<double>(windowWidth);
    y = static_cast<double>(finger.y) * static_cast<double>(windowHeight);
    return true;
}

void queueFingerInput(SDL_Window* window, const SDL_TouchFingerEvent& finger) {
    double x = 0.0;
    double y = 0.0;
    if (!fingerWindowPosition(window, finger, x, y)) {
        return;
    }

    TouchScrollState& state = touchScrollStates()[window];
    if (finger.type == SDL_FINGERDOWN) {
        state = {};
        state.fingerId = finger.fingerId;
        state.lastX = x;
        state.lastY = y;
        state.downTime = core::window::timeSeconds();
        state.active = true;
        core::queuePointerInput(window, x, y, true);
        return;
    }

    if (!state.active || state.fingerId != finger.fingerId) {
        state = {};
        state.fingerId = finger.fingerId;
        state.lastX = x;
        state.lastY = y;
        state.active = finger.type != SDL_FINGERUP;
    }

    const double dx = x - state.lastX;
    const double dy = y - state.lastY;
    state.lastX = x;
    state.lastY = y;
    state.totalX += dx;
    state.totalY += dy;

    if (finger.type == SDL_FINGERUP) {
        core::queuePointerInput(window, x, y, false);
        touchScrollStates().erase(window);
        return;
    }

    constexpr double scrollStartThreshold = 14.0;
    constexpr double verticalIntentRatio = 1.15;
    constexpr double logicalScrollStep = 96.0;
    if (!state.scrolling &&
        std::fabs(state.totalY) >= scrollStartThreshold &&
        std::fabs(state.totalY) >= std::fabs(state.totalX) * verticalIntentRatio) {
        state.scrolling = true;
    }

    if (state.scrolling) {
        core::queuePointerInput(window, x, y, false);
        if (dy != 0.0) {
            core::queueScrollInput(window, 0.0, dy / logicalScrollStep);
        }
    } else {
        core::queuePointerInput(window, x, y, true);
    }
}

void pollFingerLongPress(SDL_Window* window) {
    auto it = touchScrollStates().find(window);
    if (it == touchScrollStates().end()) {
        return;
    }
    TouchScrollState& state = it->second;
    if (!state.active || state.scrolling) {
        return;
    }
    if (state.contextReleaseQueued) {
        core::queuePointerInput(window, state.lastX, state.lastY, false, false);
        state.contextReleaseQueued = false;
        return;
    }
    constexpr double longPressSeconds = 0.58;
    constexpr double maxMovement = 18.0;
    if (!state.contextQueued &&
        core::window::timeSeconds() - state.downTime >= longPressSeconds &&
        std::fabs(state.totalX) <= maxMovement &&
        std::fabs(state.totalY) <= maxMovement) {
        core::queuePointerInput(window, state.lastX, state.lastY, false, true);
        state.contextQueued = true;
        state.contextReleaseQueued = true;
    }
}

void handleAndroidLifecycleEvent(SDL_Window* window, WindowState& state, const SDL_Event& event) {
#ifdef __ANDROID__
    if (!isAndroidLifecycleEvent(event.type)) {
        return;
    }
    if (event.type == SDL_APP_WILLENTERBACKGROUND ||
        event.type == SDL_APP_DIDENTERBACKGROUND ||
        event.type == SDL_APP_LOWMEMORY) {
        core::platform::setTextInputActive(window, false);
        touchScrollStates().erase(window);
        if (state.renderBackend != nullptr) {
            state.renderBackend->makeCurrent();
            state.renderBackend->releaseRenderCache();
        }
        state.surfaceResetPending = true;
        app::detail::requestFullPaint();
        state.paintRequested = false;
        return;
    }
    if (event.type == SDL_APP_WILLENTERFOREGROUND) {
        app::detail::requestFullPaint();
        state.paintRequested = true;
        core::platform::requestFrame();
        return;
    }
    if (event.type == SDL_APP_DIDENTERFOREGROUND) {
        if (state.renderBackend != nullptr) {
            state.renderBackend->makeCurrent();
            if (state.surfaceResetPending) {
                state.renderBackend->resetSurface();
                state.surfaceResetPending = false;
            }
            state.renderBackend->releaseRenderCache();
        }
        app::detail::requestFullPaint();
        state.paintRequested = true;
        core::platform::requestFrame();
    }
#else
    (void)window;
    (void)state;
    (void)event;
#endif
}

void hideToTray(SDL_Window* window, WindowState& state) {
    if (!state.trayAvailable || state.hiddenToTray) {
        return;
    }
    if (state.renderBackend != nullptr) {
        state.renderBackend->makeCurrent();
        state.renderBackend->releaseRenderCache();
        core::render::ScopedRenderBackend scopedRenderBackend(*state.renderBackend);
        app::releaseGraphicsResources();
    } else {
        app::releaseGraphicsResources();
    }
    SDL_HideWindow(window);
    state.hiddenToTray = true;
    state.paintRequested = false;
}

void restoreFromTray(SDL_Window* window, WindowState& state) {
    if (!state.hiddenToTray) {
        return;
    }
    SDL_ShowWindow(window);
    SDL_RaiseWindow(window);
    state.hiddenToTray = false;
    state.paintRequested = true;
    app::detail::requestFullPaint();
}

void requestClose(SDL_Window* window, WindowState& state) {
    if (state.trayAvailable) {
        hideToTray(window, state);
    } else {
        state.running = false;
    }
}

void processMainEvent(SDL_Window* window, WindowState& state, const SDL_Event& event, bool inputEnabled) {
    if (event.type == SDL_QUIT) {
        requestClose(window, state);
        return;
    }
    if (isAndroidLifecycleEvent(event.type)) {
        handleAndroidLifecycleEvent(window, state, event);
        return;
    }
    if (isFingerEvent(event.type)) {
        if (!inputEnabled) {
            return;
        }
        queueFingerInput(window, event.tfinger);
        state.paintRequested = true;
        return;
    }
    if (event.type == SDL_TEXTINPUT) {
        if (!inputEnabled) {
            return;
        }
        core::queueTextInput(window, event.text.text);
        state.paintRequested = true;
        return;
    }
    if (event.type == SDL_TEXTEDITING) {
        if (!inputEnabled) {
            return;
        }
        core::queueTextEditing(window, event.edit.text);
        state.paintRequested = true;
        return;
    }
    if (event.type == SDL_MOUSEWHEEL) {
        if (!inputEnabled) {
            return;
        }
        core::queueScrollInput(window, event.wheel.preciseX, event.wheel.preciseY);
        state.paintRequested = true;
        return;
    }
    if (event.type == SDL_KEYDOWN) {
        if (!inputEnabled) {
            return;
        }
        const bool ctrl = (event.key.keysym.mod & (KMOD_CTRL | KMOD_GUI)) != 0;
        const bool shift = (event.key.keysym.mod & KMOD_SHIFT) != 0;
        core::InputKey key;
        if (mapKey(event.key.keysym.sym, key)) {
            core::queueKeyInput(window, key, ctrl, shift);
            state.paintRequested = true;
        }
        return;
    }
    if (event.type == SDL_WINDOWEVENT) {
        switch (event.window.event) {
        case SDL_WINDOWEVENT_CLOSE:
            requestClose(window, state);
            break;
        case SDL_WINDOWEVENT_MINIMIZED:
            break;
        case SDL_WINDOWEVENT_EXPOSED:
        case SDL_WINDOWEVENT_RESIZED:
        case SDL_WINDOWEVENT_SIZE_CHANGED:
        case SDL_WINDOWEVENT_SHOWN:
        case SDL_WINDOWEVENT_RESTORED:
        case SDL_WINDOWEVENT_FOCUS_GAINED:
            state.paintRequested = true;
            app::detail::requestFullPaint();
            break;
        default:
            break;
        }
    }
}

std::unique_ptr<ManagedWindow> createManagedWindow(const app::DslWindowRequest& request,
                                                   SDL_Window* parentWindow,
                                                   core::render::RenderBackend& shareBackend) {
    core::window::WindowCreateRequest windowRequest;
    windowRequest.width = request.width;
    windowRequest.height = request.height;
    windowRequest.title = request.title.c_str();
    windowRequest.parent = parentWindow;
    windowRequest.renderApi = core::render::windowRenderApi();
    SDL_Window* window = static_cast<SDL_Window*>(core::window::createWindow(windowRequest));
    if (window == nullptr) {
        return {};
    }

    auto managed = std::make_unique<ManagedWindow>();
    managed->window = window;
    managed->parentWindow = parentWindow;
    managed->renderBackend = core::render::createRenderBackend(window, &shareBackend);
    if (!managed->renderBackend) {
        core::window::destroyWindow(window);
        return {};
    }
    if (!managed->renderBackend->initialize()) {
        core::window::destroyWindow(window);
        return {};
    }
    if (!managed->content.initialize(window, request)) {
        managed->renderBackend.reset();
        core::window::destroyWindow(window);
        return {};
    }
    if (request.modal) {
        SDL_SetWindowModalFor(window, parentWindow);
        attachNativeChildWindow(parentWindow, window);
        SDL_RaiseWindow(window);
    }
    return managed;
}

void destroyManagedWindow(std::unique_ptr<ManagedWindow>& managed) {
    if (!managed) {
        return;
    }
    if (managed->window != nullptr && managed->renderBackend != nullptr) {
        if (managed->content.request().modal && managed->parentWindow != nullptr) {
            detachNativeChildWindow(managed->parentWindow, managed->window);
        }
        managed->renderBackend->makeCurrent();
        if (managed->renderBackend) {
            managed->renderBackend->releaseRenderCache();
        }
        core::releaseInputQueue(managed->window);
        if (managed->renderBackend) {
            core::render::ScopedRenderBackend scopedRenderBackend(*managed->renderBackend);
            managed->content.shutdown(false);
        } else {
            managed->content.shutdown(false);
        }
        managed->renderBackend.reset();
        core::window::destroyWindow(managed->window);
    }
    managed.reset();
}

bool isManagedWindowClosed(const ManagedWindow& managed) {
    return managed.closeRequested || managed.window == nullptr || managed.renderBackend == nullptr;
}

void pruneClosedWindows(app::DslWindowManager<ManagedWindow>& windows) {
    windows.pruneClosed(isManagedWindowClosed, destroyManagedWindow);
}

void createRequestedWindows(app::DslWindowManager<ManagedWindow>& windows,
                            SDL_Window* mainWindow,
                            core::render::RenderBackend& mainBackend,
                            const std::vector<app::DslWindowRequest>& requests) {
    mainBackend.makeCurrent();
    windows.createPending(requests, [&](const app::DslWindowRequest& request) {
        std::unique_ptr<ManagedWindow> managed = createManagedWindow(request, mainWindow, mainBackend);
        mainBackend.makeCurrent();
        return managed;
    });
}

ManagedWindow* findWindow(app::DslWindowManager<ManagedWindow>& windows, Uint32 windowId) {
    return windows.find([&](const ManagedWindow& managed) {
        return managed.window != nullptr && SDL_GetWindowID(managed.window) == windowId;
    });
}

ManagedWindow* findModalWindow(app::DslWindowManager<ManagedWindow>& windows) {
    return windows.modalWindow(isManagedWindowClosed);
}

void processManagedEvent(ManagedWindow& managed, const SDL_Event& event) {
    if (isFingerEvent(event.type)) {
        queueFingerInput(managed.window, event.tfinger);
        managed.content.requestPaint();
        return;
    }
    if (event.type == SDL_TEXTINPUT) {
        core::queueTextInput(managed.window, event.text.text);
        managed.content.requestPaint();
        return;
    }
    if (event.type == SDL_TEXTEDITING) {
        core::queueTextEditing(managed.window, event.edit.text);
        managed.content.requestPaint();
        return;
    }
    if (event.type == SDL_MOUSEWHEEL) {
        core::queueScrollInput(managed.window, event.wheel.preciseX, event.wheel.preciseY);
        managed.content.requestPaint();
        return;
    }
    if (event.type == SDL_KEYDOWN) {
        const bool ctrl = (event.key.keysym.mod & (KMOD_CTRL | KMOD_GUI)) != 0;
        const bool shift = (event.key.keysym.mod & KMOD_SHIFT) != 0;
        core::InputKey key;
        if (mapKey(event.key.keysym.sym, key)) {
            core::queueKeyInput(managed.window, key, ctrl, shift);
            managed.content.requestPaint();
        }
        return;
    }
    if (event.type == SDL_WINDOWEVENT) {
        if (event.window.event == SDL_WINDOWEVENT_CLOSE) {
            managed.closeRequested = true;
        } else if (event.window.event == SDL_WINDOWEVENT_EXPOSED ||
                   event.window.event == SDL_WINDOWEVENT_RESIZED ||
                   event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
                   event.window.event == SDL_WINDOWEVENT_SHOWN ||
                   event.window.event == SDL_WINDOWEVENT_RESTORED ||
                   event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
            managed.content.requestFullPaint();
        }
    }
}

bool updateManagedWindow(ManagedWindow& managed, float deltaSeconds, bool updateRequested) {
    if (managed.closeRequested || managed.window == nullptr || managed.renderBackend == nullptr) {
        return false;
    }

    managed.renderBackend->makeCurrent();
    int drawableWidth = 0;
    int drawableHeight = 0;
    getDrawableSize(managed.window, drawableWidth, drawableHeight);
    if (drawableWidth <= 0 || drawableHeight <= 0) {
        managed.renderBackend->releaseRenderCache();
        managed.content.requestFullPaint();
        return true;
    }
    const float dpi = dpiScale(managed.window);
    const float pointer = pointerScale(managed.window);
    const float logicalWidth = static_cast<float>(drawableWidth) / dpi;
    const float logicalHeight = static_cast<float>(drawableHeight) / dpi;

    managed.content.update(managed.window, deltaSeconds, logicalWidth, logicalHeight, pointer, dpi, updateRequested);
    if (managed.content.paintRequested()) {
        managed.renderBackend->beginFrame({
            managed.window,
            core::window::nativeWindowInfo(managed.window),
            drawableWidth,
            drawableHeight,
            dpi
        });
        managed.content.render(*managed.renderBackend, drawableWidth, drawableHeight, dpi);
        managed.renderBackend->present();
    }
    return true;
}

} // namespace

int runSdlApp() {
    core::platform::repairCurrentWorkingDirectory();
    SDL_SetMainReady();
#ifdef _WIN32
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
    SDL_SetHint(SDL_HINT_IME_INTERNAL_EDITING, "1");
#endif
#ifdef __ANDROID__
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        return -1;
    }
    TimerResolutionGuard timerResolution;

    core::window::WindowCreateRequest windowRequest;
    windowRequest.width = app::initialWindowWidth();
    windowRequest.height = app::initialWindowHeight();
    windowRequest.title = app::windowTitle();
    windowRequest.renderApi = core::render::windowRenderApi();
    SDL_Window* window = static_cast<SDL_Window*>(core::window::createWindow(windowRequest));
    if (window == nullptr) {
        SDL_Quit();
        return -1;
    }

    auto renderBackend = core::render::createRenderBackend(window);
    if (!renderBackend) {
        core::window::destroyWindow(window);
        SDL_Quit();
        return -1;
    }
    if (!renderBackend->initialize()) {
        core::window::destroyWindow(window);
        SDL_Quit();
        return -1;
    }

    if (!app::initialize(window)) {
        app::shutdown();
        renderBackend.reset();
        core::window::destroyWindow(window);
        SDL_Quit();
        return -1;
    }
    core::platform::setTextInputActive(window, false);

    WindowState state;
    state.resetTiming(core::window::timeSeconds());
    updateFrameInterval(window, state);
    state.initializeTray();
    state.renderBackend = renderBackend.get();
    app::MainWindowRuntime mainWindowRuntime(state);

    app::DslWindowManager<ManagedWindow> childWindows;
    while (state.running) {
        state.pollTray(false);
        if (state.consumeTrayExitRequested()) {
            break;
        }
        if (state.consumeTrayShowRequested()) {
            restoreFromTray(window, state);
        }
        if (state.hiddenToTray) {
            SDL_Event event{};
            if (SDL_WaitEventTimeout(&event, 100)) {
                processMainEvent(window, state, event, true);
            }
            continue;
        }

        ManagedWindow* modalWindow = findModalWindow(childWindows);
        const bool animating = state.anyAnimating(childWindows.anyAnimating());
        if (!animating) {
            SDL_Event event{};
            if (SDL_WaitEventTimeout(&event, 100)) {
                if (isRoutedEvent(event)) {
                    const Uint32 eventWindowId = routedWindowId(event);
                    if (ManagedWindow* managed = findWindow(childWindows, eventWindowId)) {
                        processManagedEvent(*managed, event);
                    } else {
                        processMainEvent(window, state, event, modalWindow == nullptr);
                    }
                } else {
                    processMainEvent(window, state, event, modalWindow == nullptr);
                }
            }
        } else {
            const double remaining = state.nextFrameTime - core::window::timeSeconds();
            if (remaining > 0.001) {
                std::this_thread::sleep_for(std::chrono::duration<double>(remaining * 0.75));
            }
        }

        SDL_Event event{};
        while (SDL_PollEvent(&event)) {
            if (isRoutedEvent(event)) {
                const Uint32 eventWindowId = routedWindowId(event);
                if (ManagedWindow* managed = findWindow(childWindows, eventWindowId)) {
                    processManagedEvent(*managed, event);
                } else {
                    processMainEvent(window, state, event, findModalWindow(childWindows) == nullptr);
                }
            } else {
                processMainEvent(window, state, event, findModalWindow(childWindows) == nullptr);
            }
        }
        pruneClosedWindows(childWindows);
        if (!state.running || state.hiddenToTray) {
            continue;
        }
        pollFingerLongPress(window);

        const double now = core::window::timeSeconds();

        int drawableWidth = 0;
        int drawableHeight = 0;
        getDrawableSize(window, drawableWidth, drawableHeight);
        if (drawableWidth <= 0 || drawableHeight <= 0) {
            renderBackend->releaseRenderCache();
            app::detail::requestFullPaint();
            mainWindowRuntime.markUnavailableFrame(core::window::timeSeconds());
            continue;
        }
        const float dpi = dpiScale(window);
        const float pointer = pointerScale(window);
#if defined(EUI_RENDER_BACKEND_OPENGL) && (defined(_WIN32) || defined(__APPLE__))
        if (state.startupFullPaintFrames > 0) {
            state.paintRequested = true;
            app::detail::requestFullPaint();
            --state.startupFullPaintFrames;
        }
#endif
        mainWindowRuntime.runFrame(
            window,
            *renderBackend,
            {drawableWidth, drawableHeight, dpi, pointer},
            now,
            refreshRate(window),
            findModalWindow(childWindows) == nullptr,
            [&] {
                createRequestedWindows(childWindows, window, *renderBackend, app::consumeWindowRequests());
            },
            [&](float frameDelta, bool updateRequested) {
                childWindows.updateAll([&](ManagedWindow& managed) {
                    updateManagedWindow(managed, frameDelta, updateRequested);
                });
                createRequestedWindows(childWindows, window, *renderBackend, app::consumeWindowRequests());
            },
            [&](const char* title) {
                SDL_SetWindowTitle(window, title);
            },
            [&] {
                return childWindows.anyAnimating();
            });
    }

    childWindows.destroyAll(destroyManagedWindow);
    core::releaseInputQueue(window);
    touchScrollStates().erase(window);
    core::platform::shutdownTray();
    renderBackend->makeCurrent();
    renderBackend->releaseRenderCache();
    {
        core::render::ScopedRenderBackend scopedRenderBackend(*renderBackend);
        app::shutdown();
    }
    renderBackend.reset();
    core::platform::setTextInputActive(window, false);
    core::window::destroyWindow(window);
    SDL_Quit();
    return 0;
}

#if defined(__ANDROID__)
extern "C" int SDL_main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    return runSdlApp();
}
#else
int main() {
    return runSdlApp();
}
#endif
