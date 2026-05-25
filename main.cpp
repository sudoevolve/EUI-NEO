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

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "app/app.h"
#include "core/async.h"
#include "core/dsl_runtime.h"
#include "core/network.h"
#include "core/platform.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <memory>
#include <thread>
#include <vector>

struct WindowState {
    bool needsRender = true;
    bool trayAvailable = false;
    bool hiddenToTray = false;
    bool hideToTrayRequested = false;
    bool forceClose = false;
    GLFWwindow* modalChildWindow = nullptr;
    int renderedFrames = 0;
    double lastTitleUpdate = 0.0;
    double nextFrameTime = 0.0;
    double frameInterval = 1.0 / 60.0;
    double lastFrameRateLimit = 0.0;
    double lastRefreshRateUpdate = 0.0;
};

struct ManagedWindow {
    GLFWwindow* window = nullptr;
    WindowState state;
    core::dsl::Runtime runtime;
    app::DslWindowRequest request;
    bool composed = false;
    float logicalWidth = 0.0f;
    float logicalHeight = 0.0f;
    double lastFrameTime = 0.0;
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

float getDpiScale(GLFWwindow* window) {
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    glfwGetWindowContentScale(window, &scaleX, &scaleY);
    return (scaleX + scaleY) * 0.5f;
}

float getPointerScale(GLFWwindow* window) {
    int windowWidth = 0;
    int windowHeight = 0;
    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetWindowSize(window, &windowWidth, &windowHeight);
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);

    if (windowWidth <= 0 || windowHeight <= 0) {
        return 1.0f;
    }

    const float scaleX = static_cast<float>(framebufferWidth) / static_cast<float>(windowWidth);
    const float scaleY = static_cast<float>(framebufferHeight) / static_cast<float>(windowHeight);
    return (scaleX + scaleY) * 0.5f;
}

GLFWmonitor* getWindowMonitor(GLFWwindow* window) {
    if (GLFWmonitor* monitor = glfwGetWindowMonitor(window)) {
        return monitor;
    }

    int windowX = 0;
    int windowY = 0;
    int windowWidth = 0;
    int windowHeight = 0;
    glfwGetWindowPos(window, &windowX, &windowY);
    glfwGetWindowSize(window, &windowWidth, &windowHeight);

    int monitorCount = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&monitorCount);
    GLFWmonitor* bestMonitor = glfwGetPrimaryMonitor();
    int bestArea = 0;

    for (int i = 0; i < monitorCount; ++i) {
        GLFWmonitor* monitor = monitors[i];
        int monitorX = 0;
        int monitorY = 0;
        glfwGetMonitorPos(monitor, &monitorX, &monitorY);
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        if (!mode) {
            continue;
        }

        const int overlapLeft = std::max(windowX, monitorX);
        const int overlapTop = std::max(windowY, monitorY);
        const int overlapRight = std::min(windowX + windowWidth, monitorX + mode->width);
        const int overlapBottom = std::min(windowY + windowHeight, monitorY + mode->height);
        const int overlapWidth = std::max(0, overlapRight - overlapLeft);
        const int overlapHeight = std::max(0, overlapBottom - overlapTop);
        const int overlapArea = overlapWidth * overlapHeight;
        if (overlapArea > bestArea) {
            bestArea = overlapArea;
            bestMonitor = monitor;
        }
    }

    return bestMonitor;
}

double getWindowRefreshRate(GLFWwindow* window) {
    GLFWmonitor* monitor = getWindowMonitor(window);
    const GLFWvidmode* mode = monitor ? glfwGetVideoMode(monitor) : nullptr;
    if (mode && mode->refreshRate > 0) {
        return static_cast<double>(mode->refreshRate);
    }
    return 60.0;
}

void updateFrameInterval(GLFWwindow* window, WindowState& windowState, double now, bool force = false) {
    const double limit = app::frameRateLimit();
    if (!force && limit == windowState.lastFrameRateLimit && now - windowState.lastRefreshRateUpdate < 0.5) {
        return;
    }

    double refreshRate = std::clamp(getWindowRefreshRate(window), 30.0, 500.0);
    if (limit > 0.0) {
        refreshRate = std::min(refreshRate, limit);
    }
    windowState.frameInterval = 1.0 / std::max(1.0, refreshRate);
    windowState.lastFrameRateLimit = limit;
    windowState.lastRefreshRateUpdate = now;
}

void waitForNextFrame(GLFWwindow* window, const WindowState& windowState) {
    while (!glfwWindowShouldClose(window)) {
        const double remaining = windowState.nextFrameTime - glfwGetTime();
        if (remaining <= 0.0) {
            break;
        }

        if (remaining > 0.002) {
            glfwWaitEventsTimeout(remaining - 0.001);
        } else {
            std::this_thread::sleep_for(std::chrono::duration<double>(remaining * 0.5));
        }
    }
}

void hideWindowToTray(GLFWwindow* window, WindowState& windowState) {
    if (!windowState.trayAvailable || windowState.hiddenToTray) {
        return;
    }

    app::releaseGraphicsResources();
    glfwHideWindow(window);
    windowState.hiddenToTray = true;
    windowState.hideToTrayRequested = false;
    windowState.needsRender = false;
    windowState.renderedFrames = 0;
    windowState.nextFrameTime = glfwGetTime();
}

void restoreWindowFromTray(GLFWwindow* window, WindowState& windowState) {
    if (!windowState.hiddenToTray) {
        return;
    }

    glfwRestoreWindow(window);
    glfwShowWindow(window);
    glfwFocusWindow(window);
    windowState.hiddenToTray = false;
    windowState.hideToTrayRequested = false;
    windowState.needsRender = true;
    windowState.nextFrameTime = glfwGetTime();
}

void installWindowCallbacks(GLFWwindow* window, WindowState& windowState) {
    glfwSetWindowUserPointer(window, &windowState);
    glfwSetFramebufferSizeCallback(window, [](GLFWwindow* currentWindow, int w, int h) {
        glViewport(0, 0, w, h);
        const float dpiScale = getDpiScale(currentWindow);
        app::render(w, h, dpiScale);
		glfwSwapBuffers(currentWindow);
        static_cast<WindowState*>(glfwGetWindowUserPointer(currentWindow))->needsRender = false;
        ++static_cast<WindowState*>(glfwGetWindowUserPointer(currentWindow))->renderedFrames;
    });
    glfwSetWindowRefreshCallback(window, [](GLFWwindow* currentWindow) {
        static_cast<WindowState*>(glfwGetWindowUserPointer(currentWindow))->needsRender = true;
    });
    glfwSetWindowContentScaleCallback(window, [](GLFWwindow* currentWindow, float, float) {
        static_cast<WindowState*>(glfwGetWindowUserPointer(currentWindow))->needsRender = true;
    });
    glfwSetWindowFocusCallback(window, [](GLFWwindow* currentWindow, int focused) {
        WindowState* state = static_cast<WindowState*>(glfwGetWindowUserPointer(currentWindow));
        if (!state) {
            return;
        }
        state->needsRender = true;
        if (focused && state->modalChildWindow != nullptr && !glfwWindowShouldClose(state->modalChildWindow)) {
            glfwFocusWindow(state->modalChildWindow);
        }
    });
}

std::unique_ptr<ManagedWindow> createManagedWindow(const app::DslWindowRequest& request, GLFWwindow* shareWindow) {
    GLFWwindow* childWindow = glfwCreateWindow(request.width, request.height, request.title.c_str(), nullptr, shareWindow);
    if (!childWindow) {
        return {};
    }

    auto managed = std::make_unique<ManagedWindow>();
    managed->window = childWindow;
    managed->request = request;
    managed->state.lastTitleUpdate = glfwGetTime();
    managed->state.nextFrameTime = managed->state.lastTitleUpdate;
    managed->lastFrameTime = managed->state.lastTitleUpdate;
    installWindowCallbacks(childWindow, managed->state);

    glfwMakeContextCurrent(childWindow);
    glfwSwapInterval(0);
    if (!managed->runtime.initialize(childWindow)) {
        core::releaseInputQueue(childWindow);
        glfwDestroyWindow(childWindow);
        return {};
    }

    managed->state.needsRender = true;
    if (managed->request.modal) {
        glfwFocusWindow(childWindow);
    }
    return managed;
}

void destroyManagedWindow(std::unique_ptr<ManagedWindow>& managed) {
    if (!managed || managed->window == nullptr) {
        managed.reset();
        return;
    }

    GLFWwindow* previousContext = glfwGetCurrentContext();
    GLFWwindow* windowToDestroy = managed->window;
    glfwMakeContextCurrent(windowToDestroy);
    core::releaseInputQueue(windowToDestroy);
    managed->runtime.shutdown(false);
    glfwDestroyWindow(windowToDestroy);
    if (previousContext != windowToDestroy) {
        glfwMakeContextCurrent(previousContext);
    } else {
        glfwMakeContextCurrent(nullptr);
    }
    managed.reset();
}

bool updateManagedWindow(ManagedWindow& managed, float deltaSeconds, bool externalReady) {
    if (managed.window == nullptr || glfwWindowShouldClose(managed.window)) {
        return false;
    }

    glfwMakeContextCurrent(managed.window);

    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetFramebufferSize(managed.window, &framebufferWidth, &framebufferHeight);
    if (framebufferWidth <= 0 || framebufferHeight <= 0) {
        managed.state.needsRender = true;
        return true;
    }

    const float dpiScale = getDpiScale(managed.window);
    const float pointerScale = getPointerScale(managed.window);
    const float logicalWidth = static_cast<float>(framebufferWidth) / dpiScale;
    const float logicalHeight = static_cast<float>(framebufferHeight) / dpiScale;

    const auto composeFrame = [&] {
        managed.runtime.compose(managed.request.pageId, logicalWidth, logicalHeight,
            [&](core::dsl::Ui& ui, const core::dsl::Screen& screen) {
                managed.request.compose(ui, screen);
            });
        managed.composed = true;
        managed.logicalWidth = logicalWidth;
        managed.logicalHeight = logicalHeight;
    };

    if (!managed.composed || managed.logicalWidth != logicalWidth || managed.logicalHeight != logicalHeight || externalReady) {
        composeFrame();
        managed.runtime.markFullRedraw();
        managed.state.needsRender = true;
    }

    if (managed.runtime.update(managed.window, deltaSeconds, pointerScale, dpiScale)) {
        managed.state.needsRender = true;
    }

    if (managed.state.needsRender) {
        managed.runtime.render(framebufferWidth, framebufferHeight, dpiScale, managed.request.clearColor);
        glfwSwapBuffers(managed.window);
        managed.state.needsRender = false;
        ++managed.state.renderedFrames;
    }

    managed.lastFrameTime = glfwGetTime();
    return true;
}

void pruneClosedWindows(std::vector<std::unique_ptr<ManagedWindow>>& windows) {
    windows.erase(std::remove_if(windows.begin(), windows.end(), [](std::unique_ptr<ManagedWindow>& managed) {
        if (!managed) {
            return true;
        }
        if (managed->window == nullptr || !glfwWindowShouldClose(managed->window)) {
            return false;
        }
        destroyManagedWindow(managed);
        return true;
    }), windows.end());
}

void createRequestedWindows(std::vector<std::unique_ptr<ManagedWindow>>& windows,
                            GLFWwindow* shareWindow,
                            const std::vector<app::DslWindowRequest>& requests) {
    GLFWwindow* previousContext = glfwGetCurrentContext();
    for (const app::DslWindowRequest& request : requests) {
        if (!request.compose) {
            continue;
        }
        if (std::unique_ptr<ManagedWindow> managed = createManagedWindow(request, shareWindow)) {
            windows.push_back(std::move(managed));
        }
    }
    glfwMakeContextCurrent(previousContext);
}

GLFWwindow* findModalChildWindow(const std::vector<std::unique_ptr<ManagedWindow>>& windows) {
    for (auto it = windows.rbegin(); it != windows.rend(); ++it) {
        const std::unique_ptr<ManagedWindow>& managed = *it;
        if (managed && managed->request.modal && managed->window != nullptr && !glfwWindowShouldClose(managed->window)) {
            return managed->window;
        }
    }
    return nullptr;
}

int main() {
    glfwInit();
    TimerResolutionGuard timerResolution;

    glfwWindowHint(GLFW_SAMPLES, 0);
    glfwWindowHint(GLFW_RED_BITS, 8);
    glfwWindowHint(GLFW_GREEN_BITS, 8);
    glfwWindowHint(GLFW_BLUE_BITS, 8);
    glfwWindowHint(GLFW_ALPHA_BITS, 8);
    glfwWindowHint(GLFW_DEPTH_BITS, 16);
    glfwWindowHint(GLFW_STENCIL_BITS, 0);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(app::initialWindowWidth(), app::initialWindowHeight(), app::windowTitle(), nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(0);

    WindowState windowState;
    windowState.lastTitleUpdate = glfwGetTime();
    windowState.nextFrameTime = windowState.lastTitleUpdate;
    updateFrameInterval(window, windowState, windowState.lastTitleUpdate, true);
    if (app::showFrameCountInTitle()) {
        char title[128];
        std::snprintf(title, sizeof(title), "%s - 0 FPS", app::windowTitle());
        glfwSetWindowTitle(window, title);
    }
    installWindowCallbacks(window, windowState);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        glfwTerminate();
        return -1;
    }

    if (!app::initialize(window)) {
        glfwTerminate();
        return -1;
    }
    if (app::trayEnabled()) {
        windowState.trayAvailable = core::platform::initializeTray({
            app::trayTitle(),
            app::trayIconPath()
        });
    }
    glfwSetWindowCloseCallback(window, [](GLFWwindow* currentWindow) {
        WindowState* state = static_cast<WindowState*>(glfwGetWindowUserPointer(currentWindow));
        if (state && state->modalChildWindow != nullptr && !glfwWindowShouldClose(state->modalChildWindow)) {
            glfwFocusWindow(state->modalChildWindow);
            glfwSetWindowShouldClose(currentWindow, GLFW_FALSE);
            return;
        }
        if (state && state->trayAvailable && !state->forceClose) {
            state->hideToTrayRequested = true;
            glfwSetWindowShouldClose(currentWindow, GLFW_FALSE);
        }
    });
    glfwSetWindowIconifyCallback(window, [](GLFWwindow* currentWindow, int iconified) {
        WindowState* state = static_cast<WindowState*>(glfwGetWindowUserPointer(currentWindow));
        if (state && state->trayAvailable && iconified && !state->forceClose) {
            state->hideToTrayRequested = true;
        }
    });

    double lastFrameTime = glfwGetTime();
    std::vector<std::unique_ptr<ManagedWindow>> childWindows;

    while (!glfwWindowShouldClose(window)) {
        glfwMakeContextCurrent(window);
        core::platform::pollTray(false);
        if (core::platform::consumeTrayExitRequested()) {
            windowState.forceClose = true;
            glfwSetWindowShouldClose(window, GLFW_TRUE);
            break;
        }
        if (core::platform::consumeTrayShowRequested()) {
            restoreWindowFromTray(window, windowState);
        }
        pruneClosedWindows(childWindows);
        windowState.modalChildWindow = findModalChildWindow(childWindows);
        if (windowState.hideToTrayRequested && !childWindows.empty()) {
            windowState.hideToTrayRequested = false;
        }
        if (windowState.hideToTrayRequested) {
            hideWindowToTray(window, windowState);
        }
        if (windowState.hiddenToTray) {
            glfwWaitEventsTimeout(0.10);
            lastFrameTime = glfwGetTime();
            windowState.nextFrameTime = lastFrameTime;
            continue;
        }

        const bool mainAnimatingAtFrameStart = app::isAnimating();
        const bool childAnimatingAtFrameStart = std::any_of(childWindows.begin(), childWindows.end(), [](const std::unique_ptr<ManagedWindow>& managed) {
            return managed != nullptr && managed->runtime.isAnimating();
        });
        if (mainAnimatingAtFrameStart || childAnimatingAtFrameStart) {
            waitForNextFrame(window, windowState);
        }

        const double currentFrameTime = glfwGetTime();
        updateFrameInterval(window, windowState, currentFrameTime);
        const float deltaSeconds = static_cast<float>(currentFrameTime - lastFrameTime);
        lastFrameTime = currentFrameTime;

        if (windowState.modalChildWindow == nullptr && glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            if (windowState.trayAvailable) {
                windowState.hideToTrayRequested = true;
            } else {
                glfwSetWindowShouldClose(window, 1);
                break;
            }
        }

        int framebufferWidth = 0;
        int framebufferHeight = 0;
        glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
        if (framebufferWidth <= 0 || framebufferHeight <= 0) {
            windowState.needsRender = true;
            glfwWaitEvents();
            windowState.nextFrameTime = glfwGetTime();
            lastFrameTime = windowState.nextFrameTime;
            continue;
        }

        const float dpiScale = getDpiScale(window);
        const float pointerScale = getPointerScale(window);
        const bool asyncReady = core::async::dispatchReady();
        const bool externalReady = core::network::consumeAnyTextReady() || asyncReady;
        const bool mainInputEnabled = windowState.modalChildWindow == nullptr;

        if (app::update(window, deltaSeconds, framebufferWidth, framebufferHeight, dpiScale, pointerScale, externalReady, mainInputEnabled)) {
            windowState.needsRender = true;
        }

        createRequestedWindows(childWindows, window, app::consumeWindowRequests());
        pruneClosedWindows(childWindows);
        windowState.modalChildWindow = findModalChildWindow(childWindows);

        if (windowState.needsRender) {
            app::render(framebufferWidth, framebufferHeight, dpiScale);
            glfwSwapBuffers(window);
            windowState.needsRender = false;
            ++windowState.renderedFrames;
        }

        for (std::unique_ptr<ManagedWindow>& managed : childWindows) {
            if (managed && updateManagedWindow(*managed, deltaSeconds, externalReady)) {
                // child window rendered inside updateManagedWindow
            }
        }

        createRequestedWindows(childWindows, window, app::consumeWindowRequests());
        pruneClosedWindows(childWindows);
        windowState.modalChildWindow = findModalChildWindow(childWindows);

        if (app::showFrameCountInTitle()) {
            const double titleElapsed = glfwGetTime() - windowState.lastTitleUpdate;
            if (titleElapsed >= 1.0) {
                const double fps = static_cast<double>(windowState.renderedFrames) / titleElapsed;
                char title[128];
                std::snprintf(title, sizeof(title), "%s - %.0f FPS", app::windowTitle(), fps);
                glfwSetWindowTitle(window, title);
                windowState.renderedFrames = 0;
                windowState.lastTitleUpdate = glfwGetTime();
            }
        }

        const bool anyAnimating = app::isAnimating() || std::any_of(childWindows.begin(), childWindows.end(), [](const std::unique_ptr<ManagedWindow>& managed) {
            return managed != nullptr && managed->runtime.isAnimating();
        });
        if (anyAnimating) {
            const double now = glfwGetTime();
            windowState.nextFrameTime += windowState.frameInterval;
            if (windowState.nextFrameTime <= now || windowState.nextFrameTime > now + windowState.frameInterval * 2.0) {
                windowState.nextFrameTime = now + windowState.frameInterval;
            }
            glfwPollEvents();
        } else {
            glfwWaitEvents();
            windowState.nextFrameTime = glfwGetTime();
        }
    }

    for (std::unique_ptr<ManagedWindow>& managed : childWindows) {
        destroyManagedWindow(managed);
    }
    core::releaseInputQueue(window);
    glfwMakeContextCurrent(window);
    core::platform::shutdownTray();
    app::shutdown();
    glfwTerminate();
    return 0;
}
