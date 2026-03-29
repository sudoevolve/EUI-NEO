#pragma once

#include "../EUINEO.h"
#include "../ui/UIContext.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>

namespace EUINEO {

struct DslAppConfig {
    std::string title = "EUI-NEO App";
    int width = 960;
    int height = 640;
    std::string pageId = "app";
    int fps = 0;
};

using DslComposeFn = std::function<void(UIContext&, const RectFrame&)>;

struct DslWindowState {
    GLFWwindow* window = nullptr;
    bool fullscreen = false;
    int windowedX = 100;
    int windowedY = 100;
    int windowedW = 960;
    int windowedH = 640;
};

inline DslWindowState& ActiveDslWindowState() {
    static DslWindowState state;
    return state;
}

inline bool IsDslWindowFullscreen() {
    return ActiveDslWindowState().fullscreen;
}

inline void SetDslWindowFullscreen(bool fullscreen) {
    DslWindowState& state = ActiveDslWindowState();
    GLFWwindow* window = state.window;
    if (window == nullptr) {
        return;
    }
    const bool currentFullscreen = glfwGetWindowMonitor(window) != nullptr;
    if (currentFullscreen == fullscreen) {
        state.fullscreen = currentFullscreen;
        return;
    }

    if (fullscreen) {
        glfwGetWindowPos(window, &state.windowedX, &state.windowedY);
        glfwGetWindowSize(window, &state.windowedW, &state.windowedH);
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        if (monitor == nullptr) {
            return;
        }
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        if (mode == nullptr) {
            return;
        }
        glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
    } else {
        const int targetW = std::max(320, state.windowedW);
        const int targetH = std::max(240, state.windowedH);
        glfwSetWindowMonitor(window, nullptr, state.windowedX, state.windowedY, targetW, targetH, 0);
    }
    state.fullscreen = glfwGetWindowMonitor(window) != nullptr;
    Renderer::InvalidateAll();
    Renderer::RequestRepaint(0.18f);
}

inline void ToggleDslWindowFullscreen() {
    SetDslWindowFullscreen(!IsDslWindowFullscreen());
}

inline int RunDslApp(const DslAppConfig& config, const DslComposeFn& compose) {
    if (!compose) {
        return -1;
    }

    struct WindowRuntimeState {
        int framebufferW = 0;
        int framebufferH = 0;
        int windowW = 0;
        int windowH = 0;
        float contentScaleX = 1.0f;
        float contentScaleY = 1.0f;
    } runtime;

    const auto syncSurfaceMetrics = [&]() {
        const float fallbackScaleX = runtime.windowW > 0 ? static_cast<float>(runtime.framebufferW) / static_cast<float>(std::max(1, runtime.windowW)) : 1.0f;
        const float fallbackScaleY = runtime.windowH > 0 ? static_cast<float>(runtime.framebufferH) / static_cast<float>(std::max(1, runtime.windowH)) : 1.0f;
        const float scaleX = std::max(0.5f, std::max(runtime.contentScaleX, fallbackScaleX));
        const float scaleY = std::max(0.5f, std::max(runtime.contentScaleY, fallbackScaleY));
        State.screenW = static_cast<float>(std::max(1, runtime.framebufferW)) / scaleX;
        State.screenH = static_cast<float>(std::max(1, runtime.framebufferH)) / scaleY;
        State.framebufferW = static_cast<float>(std::max(1, runtime.framebufferW));
        State.framebufferH = static_cast<float>(std::max(1, runtime.framebufferH));
        State.dpiScaleX = scaleX;
        State.dpiScaleY = scaleY;
    };

    const auto updateMousePosition = [&](double x, double y) {
        const float rawX = static_cast<float>(x);
        const float rawY = static_cast<float>(y);
        const float normalizedX = rawX <= static_cast<float>(std::max(1, runtime.windowW)) + 0.5f
            ? rawX / static_cast<float>(std::max(1, runtime.windowW))
            : rawX / static_cast<float>(std::max(1, runtime.framebufferW));
        const float normalizedY = rawY <= static_cast<float>(std::max(1, runtime.windowH)) + 0.5f
            ? rawY / static_cast<float>(std::max(1, runtime.windowH))
            : rawY / static_cast<float>(std::max(1, runtime.framebufferH));
        const float nextX = std::clamp(normalizedX, 0.0f, 1.0f) * State.screenW;
        const float nextY = std::clamp(normalizedY, 0.0f, 1.0f) * State.screenH;
        if (std::abs(State.mouseX - nextX) > 0.01f || std::abs(State.mouseY - nextY) > 0.01f) {
            State.pointerMoved = true;
        }
        State.mouseX = nextX;
        State.mouseY = nextY;
    };

    if (!glfwInit()) {
        return -1;
    }

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
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(config.width, config.height, config.title.c_str(), nullptr, nullptr);
    if (window == nullptr) {
        glfwTerminate();
        return -1;
    }
    {
        DslWindowState& state = ActiveDslWindowState();
        state.window = window;
        state.fullscreen = false;
        state.windowedW = config.width;
        state.windowedH = config.height;
        glfwGetWindowPos(window, &state.windowedX, &state.windowedY);
    }

    glfwMakeContextCurrent(window);
    const int targetFps = std::max(0, config.fps);
    const bool useVsync = targetFps <= 0;
    const double targetFrameSeconds = targetFps > 0 ? 1.0 / static_cast<double>(targetFps) : 0.0;
    glfwSwapInterval(useVsync ? 1 : 0);
    glfwSetWindowUserPointer(window, &runtime);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    glfwGetFramebufferSize(window, &runtime.framebufferW, &runtime.framebufferH);
    glViewport(0, 0, runtime.framebufferW, runtime.framebufferH);
    glfwGetWindowSize(window, &runtime.windowW, &runtime.windowH);
    glfwGetWindowContentScale(window, &runtime.contentScaleX, &runtime.contentScaleY);
    syncSurfaceMetrics();

    glfwSetFramebufferSizeCallback(window, [](GLFWwindow* win, int w, int h) {
        auto* rt = static_cast<WindowRuntimeState*>(glfwGetWindowUserPointer(win));
        if (rt == nullptr) {
            return;
        }
        glViewport(0, 0, w, h);
        rt->framebufferW = w;
        rt->framebufferH = h;
        glfwGetWindowSize(win, &rt->windowW, &rt->windowH);
        const float fallbackScaleX = rt->windowW > 0 ? static_cast<float>(rt->framebufferW) / static_cast<float>(std::max(1, rt->windowW)) : 1.0f;
        const float fallbackScaleY = rt->windowH > 0 ? static_cast<float>(rt->framebufferH) / static_cast<float>(std::max(1, rt->windowH)) : 1.0f;
        const float scaleX = std::max(0.5f, std::max(rt->contentScaleX, fallbackScaleX));
        const float scaleY = std::max(0.5f, std::max(rt->contentScaleY, fallbackScaleY));
        State.screenW = static_cast<float>(std::max(1, rt->framebufferW)) / scaleX;
        State.screenH = static_cast<float>(std::max(1, rt->framebufferH)) / scaleY;
        State.framebufferW = static_cast<float>(std::max(1, rt->framebufferW));
        State.framebufferH = static_cast<float>(std::max(1, rt->framebufferH));
        State.dpiScaleX = scaleX;
        State.dpiScaleY = scaleY;
        Renderer::InvalidateAll();
    });

    glfwSetWindowSizeCallback(window, [](GLFWwindow* win, int w, int h) {
        auto* rt = static_cast<WindowRuntimeState*>(glfwGetWindowUserPointer(win));
        if (rt == nullptr) {
            return;
        }
        rt->windowW = w;
        rt->windowH = h;
        glfwGetWindowContentScale(win, &rt->contentScaleX, &rt->contentScaleY);
        const float fallbackScaleX = rt->windowW > 0 ? static_cast<float>(rt->framebufferW) / static_cast<float>(std::max(1, rt->windowW)) : 1.0f;
        const float fallbackScaleY = rt->windowH > 0 ? static_cast<float>(rt->framebufferH) / static_cast<float>(std::max(1, rt->windowH)) : 1.0f;
        const float scaleX = std::max(0.5f, std::max(rt->contentScaleX, fallbackScaleX));
        const float scaleY = std::max(0.5f, std::max(rt->contentScaleY, fallbackScaleY));
        State.screenW = static_cast<float>(std::max(1, rt->framebufferW)) / scaleX;
        State.screenH = static_cast<float>(std::max(1, rt->framebufferH)) / scaleY;
        State.framebufferW = static_cast<float>(std::max(1, rt->framebufferW));
        State.framebufferH = static_cast<float>(std::max(1, rt->framebufferH));
        State.dpiScaleX = scaleX;
        State.dpiScaleY = scaleY;
        DslWindowState& state = ActiveDslWindowState();
        state.fullscreen = glfwGetWindowMonitor(win) != nullptr;
        if (!state.fullscreen) {
            state.windowedW = w;
            state.windowedH = h;
            glfwGetWindowPos(win, &state.windowedX, &state.windowedY);
        }
        Renderer::InvalidateAll();
    });

    glfwSetWindowContentScaleCallback(window, [](GLFWwindow* win, float xscale, float yscale) {
        auto* rt = static_cast<WindowRuntimeState*>(glfwGetWindowUserPointer(win));
        if (rt == nullptr) {
            return;
        }
        rt->contentScaleX = xscale;
        rt->contentScaleY = yscale;
        glfwGetWindowSize(win, &rt->windowW, &rt->windowH);
        glfwGetFramebufferSize(win, &rt->framebufferW, &rt->framebufferH);
        const float fallbackScaleX = rt->windowW > 0 ? static_cast<float>(rt->framebufferW) / static_cast<float>(std::max(1, rt->windowW)) : 1.0f;
        const float fallbackScaleY = rt->windowH > 0 ? static_cast<float>(rt->framebufferH) / static_cast<float>(std::max(1, rt->windowH)) : 1.0f;
        const float scaleX = std::max(0.5f, std::max(rt->contentScaleX, fallbackScaleX));
        const float scaleY = std::max(0.5f, std::max(rt->contentScaleY, fallbackScaleY));
        State.screenW = static_cast<float>(std::max(1, rt->framebufferW)) / scaleX;
        State.screenH = static_cast<float>(std::max(1, rt->framebufferH)) / scaleY;
        State.framebufferW = static_cast<float>(std::max(1, rt->framebufferW));
        State.framebufferH = static_cast<float>(std::max(1, rt->framebufferH));
        State.dpiScaleX = scaleX;
        State.dpiScaleY = scaleY;
        Renderer::InvalidateAll();
    });

    glfwSetCursorPosCallback(window, [](GLFWwindow* win, double x, double y) {
        auto* rt = static_cast<WindowRuntimeState*>(glfwGetWindowUserPointer(win));
        if (rt == nullptr) {
            return;
        }
        const float rawX = static_cast<float>(x);
        const float rawY = static_cast<float>(y);
        const float normalizedX = rawX <= static_cast<float>(std::max(1, rt->windowW)) + 0.5f
            ? rawX / static_cast<float>(std::max(1, rt->windowW))
            : rawX / static_cast<float>(std::max(1, rt->framebufferW));
        const float normalizedY = rawY <= static_cast<float>(std::max(1, rt->windowH)) + 0.5f
            ? rawY / static_cast<float>(std::max(1, rt->windowH))
            : rawY / static_cast<float>(std::max(1, rt->framebufferH));
        const float nextX = std::clamp(normalizedX, 0.0f, 1.0f) * State.screenW;
        const float nextY = std::clamp(normalizedY, 0.0f, 1.0f) * State.screenH;
        if (std::abs(State.mouseX - nextX) > 0.01f || std::abs(State.mouseY - nextY) > 0.01f) {
            State.pointerMoved = true;
        }
        State.mouseX = nextX;
        State.mouseY = nextY;
    });
    double initialMouseX = 0.0;
    double initialMouseY = 0.0;
    glfwGetCursorPos(window, &initialMouseX, &initialMouseY);
    updateMousePosition(initialMouseX, initialMouseY);

    glfwSetMouseButtonCallback(window, [](GLFWwindow*, int button, int action, int) {
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            if (action == GLFW_PRESS) {
                State.mouseDown = true;
                State.mouseClicked = true;
            } else if (action == GLFW_RELEASE) {
                State.mouseDown = false;
            }
        } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            if (action == GLFW_PRESS) {
                State.mouseRightDown = true;
                State.mouseRightClicked = true;
            } else if (action == GLFW_RELEASE) {
                State.mouseRightDown = false;
            }
        }
        Renderer::RequestRepaint();
    });

    glfwSetCharCallback(window, [](GLFWwindow* win, unsigned int codepoint) {
        const int leftCtrl = glfwGetKey(win, GLFW_KEY_LEFT_CONTROL);
        const int rightCtrl = glfwGetKey(win, GLFW_KEY_RIGHT_CONTROL);
        if (leftCtrl == GLFW_PRESS || rightCtrl == GLFW_PRESS) {
            return;
        }
        if (codepoint <= 0x7f) {
            State.textInput += static_cast<char>(codepoint);
        } else if (codepoint <= 0x7ff) {
            State.textInput += static_cast<char>(0xc0 | ((codepoint >> 6) & 0x1f));
            State.textInput += static_cast<char>(0x80 | (codepoint & 0x3f));
        } else if (codepoint <= 0xffff) {
            State.textInput += static_cast<char>(0xe0 | ((codepoint >> 12) & 0x0f));
            State.textInput += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f));
            State.textInput += static_cast<char>(0x80 | (codepoint & 0x3f));
        } else if (codepoint <= 0x10ffff) {
            State.textInput += static_cast<char>(0xf0 | ((codepoint >> 18) & 0x07));
            State.textInput += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f));
            State.textInput += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f));
            State.textInput += static_cast<char>(0x80 | (codepoint & 0x3f));
        }
        Renderer::RequestRepaint();
    });

    glfwSetScrollCallback(window, [](GLFWwindow*, double xoffset, double yoffset) {
        State.scrollDeltaX += static_cast<float>(xoffset);
        State.scrollDeltaY += static_cast<float>(yoffset);
        Renderer::RequestRepaint();
    });

    glfwSetKeyCallback(window, [](GLFWwindow* win, int key, int, int action, int) {
        if (key >= 0 && key < 512) {
            if (action == GLFW_PRESS) {
                State.keys[key] = true;
                State.keysPressed[key] = true;
                Renderer::RequestRepaint();
            } else if (action == GLFW_RELEASE) {
                State.keys[key] = false;
                Renderer::RequestRepaint();
            } else if (action == GLFW_REPEAT) {
                State.keysPressed[key] = true;
                Renderer::RequestRepaint();
            }
        }
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
            if (glfwGetWindowMonitor(win) != nullptr) {
                SetDslWindowFullscreen(false);
            } else {
                glfwSetWindowShouldClose(win, 1);
            }
        }
    });

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_MULTISAMPLE);
    Renderer::Init();

    bool fontLoaded = false;
    if (Renderer::RegisterFontSource("font/YouSheBiaoTiHei-2.ttf", 72.0f, true)) {
        fontLoaded = true;
    } else if (Renderer::RegisterFontSource("src/font/YouSheBiaoTiHei-2.ttf", 72.0f, true)) {
        fontLoaded = true;
    }
    Renderer::RegisterFontSource("font/Font Awesome 7 Free-Solid-900.otf", 96.0f, false);
    Renderer::RegisterFontSource("src/font/Font Awesome 7 Free-Solid-900.otf", 96.0f, false);
    if (!fontLoaded) {
        if (!Renderer::RegisterFontSource("C:/Windows/Fonts/msyh.ttc", 72.0f, true)) {
            Renderer::RegisterFontSource("C:/Windows/Fonts/arial.ttf", 72.0f, true);
        }
    }
    Renderer::RegisterFontSource("C:/Windows/Fonts/msyh.ttc", 72.0f);

    UIContext ui;
    const std::string pageId = config.pageId.empty() ? std::string("app") : config.pageId;
    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        const double currentTime = glfwGetTime();
        float dt = static_cast<float>(currentTime - lastTime);
        State.deltaTime = dt > 0.05f ? 0.05f : dt;
        lastTime = currentTime;

        const bool frameRequestedBeforeUpdate =
            State.needsRepaint ||
            State.animationTimeLeft > 0.0f ||
            State.pointerMoved;
        if (frameRequestedBeforeUpdate) {
            ui.begin(pageId);
            compose(ui, RectFrame{0.0f, 0.0f, State.screenW, State.screenH});
            ui.end();
            ui.update();
            if (ui.wantsContinuousUpdate()) {
                ui.requestVisualRefresh(0.18f);
            }
            if (ui.consumeRecomposeRequest()) {
                ui.begin(pageId);
                compose(ui, RectFrame{0.0f, 0.0f, State.screenW, State.screenH});
                ui.end();
            }
        }

        bool shouldDraw = Renderer::ShouldRepaint();
        if (shouldDraw) {
            State.frameCount++;
            ui.render();
            glfwSwapBuffers(window);
        }

        State.textInput.clear();
        State.scrollDeltaX = 0.0f;
        State.scrollDeltaY = 0.0f;
        State.scrollConsumed = false;
        State.mouseClicked = false;
        State.mouseRightClicked = false;
        State.pointerMoved = false;
        std::memset(State.keysPressed, 0, sizeof(State.keysPressed));

        if (useVsync) {
            if (shouldDraw || State.animationTimeLeft > 0.0f) {
                glfwPollEvents();
            } else {
                glfwWaitEvents();
            }
        } else {
            const double frameElapsed = glfwGetTime() - currentTime;
            const double waitSeconds = targetFrameSeconds - frameElapsed;
            if (waitSeconds > 0.0) {
                glfwWaitEventsTimeout(waitSeconds);
            } else {
                glfwPollEvents();
            }
        }
    }

    Renderer::Shutdown();
    ActiveDslWindowState().window = nullptr;
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

} // namespace EUINEO
