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

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#if defined(EUI_RENDER_BACKEND_VULKAN)
#include <SDL3/SDL_vulkan.h>
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
#include <memory>
#include <thread>
#include <vector>

namespace {

struct WindowState : app::AppRunner {
    bool running = true;
    core::render::RenderBackend* renderBackend = nullptr;
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
    SDL_GetWindowSizeInPixels(window, &width, &height);
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
    const SDL_DisplayID display = SDL_GetDisplayForWindow(window);
    const SDL_DisplayMode* mode = display != 0 ? SDL_GetCurrentDisplayMode(display) : nullptr;
    if (mode != nullptr && mode->refresh_rate > 0.0f) {
        return static_cast<double>(mode->refresh_rate);
    }
    return 60.0;
}

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
    case SDLK_A: mapped = core::InputKey::A; return true;
    case SDLK_C: mapped = core::InputKey::C; return true;
    case SDLK_V: mapped = core::InputKey::V; return true;
    case SDLK_X: mapped = core::InputKey::X; return true;
    case SDLK_Y: mapped = core::InputKey::Y; return true;
    case SDLK_Z: mapped = core::InputKey::Z; return true;
    default: return false;
    }
}

bool isWindowEvent(const SDL_Event& event) {
    return event.type >= SDL_EVENT_WINDOW_FIRST && event.type <= SDL_EVENT_WINDOW_LAST;
}

bool isInputEvent(const SDL_Event& event) {
    return isWindowEvent(event) ||
        event.type == SDL_EVENT_KEY_DOWN ||
        event.type == SDL_EVENT_TEXT_INPUT ||
        event.type == SDL_EVENT_TEXT_EDITING ||
        event.type == SDL_EVENT_MOUSE_WHEEL;
}

SDL_WindowID eventWindowId(const SDL_Event& event) {
    if (isWindowEvent(event)) {
        return event.window.windowID;
    }
    switch (event.type) {
    case SDL_EVENT_KEY_DOWN:
        return event.key.windowID;
    case SDL_EVENT_TEXT_INPUT:
        return event.text.windowID;
    case SDL_EVENT_TEXT_EDITING:
        return event.edit.windowID;
    case SDL_EVENT_MOUSE_WHEEL:
        return event.wheel.windowID;
    default:
        return 0;
    }
}

bool isPaintWindowEvent(const SDL_Event& event) {
    switch (event.type) {
    case SDL_EVENT_WINDOW_EXPOSED:
    case SDL_EVENT_WINDOW_RESIZED:
    case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
    case SDL_EVENT_WINDOW_SHOWN:
    case SDL_EVENT_WINDOW_RESTORED:
        return true;
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
    if (event.type == SDL_EVENT_QUIT) {
        requestClose(window, state);
        return;
    }
    if (event.type == SDL_EVENT_TEXT_INPUT) {
        if (!inputEnabled) {
            return;
        }
        core::queueTextInput(window, event.text.text);
        state.paintRequested = true;
        return;
    }
    if (event.type == SDL_EVENT_TEXT_EDITING) {
        if (!inputEnabled) {
            return;
        }
        core::queueTextEditing(window, event.edit.text);
        state.paintRequested = true;
        return;
    }
    if (event.type == SDL_EVENT_MOUSE_WHEEL) {
        if (!inputEnabled) {
            return;
        }
        core::queueScrollInput(window, event.wheel.x, event.wheel.y);
        state.paintRequested = true;
        return;
    }
    if (event.type == SDL_EVENT_KEY_DOWN) {
        if (!inputEnabled) {
            return;
        }
        const bool ctrl = (event.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0;
        const bool shift = (event.key.mod & SDL_KMOD_SHIFT) != 0;
        core::InputKey key;
        if (mapKey(event.key.key, key)) {
            core::queueKeyInput(window, key, ctrl, shift);
            state.paintRequested = true;
        }
        if (event.key.key == SDLK_ESCAPE && !state.trayAvailable) {
            state.running = false;
        } else if (event.key.key == SDLK_ESCAPE) {
            hideToTray(window, state);
        }
        return;
    }
    if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
        requestClose(window, state);
        return;
    }
    if (isPaintWindowEvent(event)) {
        state.paintRequested = true;
        app::detail::requestFullPaint();
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
        SDL_SetWindowParent(window, parentWindow);
        SDL_SetWindowModal(window, true);
        attachNativeChildWindow(parentWindow, window);
        SDL_RaiseWindow(window);
    }
    SDL_StartTextInput(window);
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
        SDL_StopTextInput(managed->window);
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

ManagedWindow* findWindow(app::DslWindowManager<ManagedWindow>& windows, SDL_WindowID windowId) {
    return windows.find([&](const ManagedWindow& managed) {
        return managed.window != nullptr && SDL_GetWindowID(managed.window) == windowId;
    });
}

ManagedWindow* findModalWindow(app::DslWindowManager<ManagedWindow>& windows) {
    return windows.modalWindow(isManagedWindowClosed);
}

void processManagedEvent(ManagedWindow& managed, const SDL_Event& event) {
    if (event.type == SDL_EVENT_TEXT_INPUT) {
        core::queueTextInput(managed.window, event.text.text);
        managed.content.requestPaint();
        return;
    }
    if (event.type == SDL_EVENT_TEXT_EDITING) {
        core::queueTextEditing(managed.window, event.edit.text);
        managed.content.requestPaint();
        return;
    }
    if (event.type == SDL_EVENT_MOUSE_WHEEL) {
        core::queueScrollInput(managed.window, event.wheel.x, event.wheel.y);
        managed.content.requestPaint();
        return;
    }
    if (event.type == SDL_EVENT_KEY_DOWN) {
        const bool ctrl = (event.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0;
        const bool shift = (event.key.mod & SDL_KMOD_SHIFT) != 0;
        core::InputKey key;
        if (mapKey(event.key.key, key)) {
            core::queueKeyInput(managed.window, key, ctrl, shift);
            managed.content.requestPaint();
        }
        if (event.key.key == SDLK_ESCAPE) {
            managed.closeRequested = true;
        }
        return;
    }
    if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
        managed.closeRequested = true;
    } else if (isPaintWindowEvent(event)) {
        managed.content.requestFullPaint();
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
#ifdef _WIN32
    SDL_SetHint(SDL_HINT_WINDOWS_DPI_AWARENESS, "permonitorv2");
    SDL_SetHint(SDL_HINT_IME_IMPLEMENTED_UI, "composition");
#endif
    if (!SDL_Init(SDL_INIT_VIDEO)) {
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
    SDL_StartTextInput(window);

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
                if (isInputEvent(event)) {
                    if (ManagedWindow* managed = findWindow(childWindows, eventWindowId(event))) {
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
            if (isInputEvent(event)) {
                if (ManagedWindow* managed = findWindow(childWindows, eventWindowId(event))) {
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
    SDL_StopTextInput(window);
    core::window::destroyWindow(window);
    SDL_Quit();
    return 0;
}
