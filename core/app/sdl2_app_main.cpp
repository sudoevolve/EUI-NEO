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
#if defined(EUI_RENDER_BACKEND_VULKAN)
#include <SDL_vulkan.h>
#endif

#include "eui/app.h"
#include "eui/detail/dsl_app_impl.h"
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
#include <memory>
#include <thread>
#include <vector>

namespace {

struct WindowState : app::AppRunner {
    bool running = true;
    bool hideToTrayRequested = false;
    core::render::RenderBackend* renderBackend = nullptr;
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

float dpiScale(SDL_Window* window) {
#ifdef _WIN32
    HWND hwnd = static_cast<HWND>(core::window::nativeWindowInfo(window).platformWindow);
    if (hwnd != nullptr) {
        using GetDpiForWindowFunction = UINT(WINAPI*)(HWND);
        static const GetDpiForWindowFunction getDpiForWindow = [] {
            HMODULE user32 = GetModuleHandleW(L"user32.dll");
            return user32 != nullptr
                ? reinterpret_cast<GetDpiForWindowFunction>(
                      GetProcAddress(user32, "GetDpiForWindow"))
                : nullptr;
        }();
        if (getDpiForWindow != nullptr) {
            const UINT dpi = getDpiForWindow(hwnd);
            if (dpi > 0) {
                return static_cast<float>(dpi) / 96.0f;
            }
        }
    }
#endif
    const float drawableRatio = pointerScale(window);
    if (drawableRatio > 1.0f) {
        return drawableRatio;
    }
    // On Linux the drawable-to-window ratio does not reflect desktop scaling,
    // so fall back to the display DPI reported by SDL.
    return core::window::displayScaleEstimate(SDL_GetWindowDisplayIndex(window));
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

Uint32 windowIdForEvent(const SDL_Event& event) {
    switch (event.type) {
    case SDL_WINDOWEVENT: return event.window.windowID;
    case SDL_KEYDOWN: return event.key.windowID;
    case SDL_TEXTINPUT: return event.text.windowID;
    case SDL_TEXTEDITING: return event.edit.windowID;
    case SDL_MOUSEWHEEL: return event.wheel.windowID;
    case SDL_MOUSEMOTION: return event.motion.windowID;
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP: return event.button.windowID;
    default: return 0;
    }
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

bool processInputEvent(SDL_Window* window,
                       const SDL_Event& event,
                       bool inputEnabled,
                       bool& repaintRequested) {
    repaintRequested = false;
    switch (event.type) {
    case SDL_MOUSEMOTION:
        core::queuePointerMotion(window,
                                 event.motion.x,
                                 event.motion.y,
                                 (event.motion.state & SDL_BUTTON_LMASK) != 0,
                                 (event.motion.state & SDL_BUTTON_RMASK) != 0);
        repaintRequested = inputEnabled;
        return true;
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
        if (event.button.button == SDL_BUTTON_LEFT || event.button.button == SDL_BUTTON_RIGHT) {
            core::queuePointerButton(window,
                                     event.button.x,
                                     event.button.y,
                                     event.button.button == SDL_BUTTON_RIGHT ? 1 : 0,
                                     event.type == SDL_MOUSEBUTTONDOWN);
        }
        repaintRequested = inputEnabled;
        return true;
    case SDL_TEXTINPUT:
        if (inputEnabled) {
            core::queueTextInput(window, event.text.text);
            repaintRequested = true;
        }
        return true;
    case SDL_TEXTEDITING:
        if (inputEnabled) {
            core::queueTextEditing(window, event.edit.text);
            repaintRequested = true;
        }
        return true;
    case SDL_MOUSEWHEEL:
        if (inputEnabled) {
            core::queueScrollInput(window, event.wheel.preciseX, event.wheel.preciseY);
            repaintRequested = true;
        }
        return true;
    case SDL_KEYDOWN: {
        core::InputKey key;
        if (inputEnabled && mapKey(event.key.keysym.sym, key)) {
            const bool shortcut = (event.key.keysym.mod & (KMOD_CTRL | KMOD_GUI)) != 0;
            const bool shift = (event.key.keysym.mod & KMOD_SHIFT) != 0;
            const core::KeyAction action =
                event.key.repeat != 0 ? core::KeyAction::Repeat : core::KeyAction::Press;
            core::queueKeyInput(window, {key, action, {shortcut, shift}});
            repaintRequested = true;
        }
        return true;
    }
    case SDL_WINDOWEVENT:
        if (event.window.event == SDL_WINDOWEVENT_ENTER) {
            core::queuePointerPresence(window, true);
            repaintRequested = inputEnabled;
            return true;
        }
        if (event.window.event == SDL_WINDOWEVENT_LEAVE) {
            core::queuePointerPresence(window, false);
            repaintRequested = inputEnabled;
            return true;
        }
        if (event.window.event == SDL_WINDOWEVENT_HIDDEN ||
            event.window.event == SDL_WINDOWEVENT_MINIMIZED ||
            event.window.event == SDL_WINDOWEVENT_CLOSE) {
            core::clearPointerInput(window);
        }
        return false;
    default:
        return false;
    }
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
    state.hideToTrayRequested = false;
    state.paintRequested = false;
    state.resetTiming(core::window::timeSeconds());
}

void restoreFromTray(SDL_Window* window, WindowState& state) {
    if (!state.hiddenToTray) {
        return;
    }
    SDL_ShowWindow(window);
    SDL_RaiseWindow(window);
    state.hiddenToTray = false;
    state.hideToTrayRequested = false;
    state.paintRequested = true;
    app::detail::requestFullPaint();
    state.resetTiming(core::window::timeSeconds());
}

void requestClose(WindowState& state) {
    if (state.trayAvailable) {
        state.hideToTrayRequested = true;
    } else {
        state.running = false;
    }
}

void processMainEvent(SDL_Window* window, WindowState& state, const SDL_Event& event, bool inputEnabled) {
    if (event.type == SDL_QUIT) {
        requestClose(state);
        return;
    }
    bool repaintRequested = false;
    if (processInputEvent(window, event, inputEnabled, repaintRequested)) {
        if (repaintRequested) {
            state.paintRequested = true;
        }
        return;
    }
    if (event.type == SDL_WINDOWEVENT) {
        switch (event.window.event) {
        case SDL_WINDOWEVENT_CLOSE:
            if (inputEnabled) {
                requestClose(state);
            }
            break;
        case SDL_WINDOWEVENT_MINIMIZED:
            break;
        case SDL_WINDOWEVENT_EXPOSED:
        case SDL_WINDOWEVENT_RESIZED:
        case SDL_WINDOWEVENT_SIZE_CHANGED:
        case SDL_WINDOWEVENT_SHOWN:
        case SDL_WINDOWEVENT_RESTORED:
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
    bool repaintRequested = false;
    if (processInputEvent(managed.window, event, true, repaintRequested)) {
        if (repaintRequested) {
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
                   event.window.event == SDL_WINDOWEVENT_RESTORED) {
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

int main() {
    core::platform::repairCurrentWorkingDirectory();
    SDL_SetMainReady();
#ifdef _WIN32
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
    SDL_SetHint(SDL_HINT_IME_INTERNAL_EDITING, "1");
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
    SDL_StartTextInput();

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
            state.resetTiming(core::window::timeSeconds());
            continue;
        }

        ManagedWindow* modalWindow = findModalWindow(childWindows);
        const bool animating = state.anyAnimating(childWindows.anyAnimating());
        if (!animating) {
            SDL_Event event{};
            if (SDL_WaitEventTimeout(&event, 100)) {
                const Uint32 eventWindowId = windowIdForEvent(event);
                if (eventWindowId != 0) {
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
            const Uint32 eventWindowId = windowIdForEvent(event);
            if (eventWindowId != 0) {
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
        if (state.hideToTrayRequested && !childWindows.empty()) {
            state.hideToTrayRequested = false;
        }
        if (state.hideToTrayRequested) {
            hideToTray(window, state);
        }
        if (!state.running || state.hiddenToTray) {
            continue;
        }

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
    core::platform::shutdownTray();
    renderBackend->makeCurrent();
    renderBackend->releaseRenderCache();
    {
        core::render::ScopedRenderBackend scopedRenderBackend(*renderBackend);
        app::shutdown();
    }
    renderBackend.reset();
    SDL_StopTextInput();
    core::window::destroyWindow(window);
    SDL_Quit();
    return 0;
}
