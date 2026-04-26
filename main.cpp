#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "app/app.h"

#include <chrono>
#include <thread>

struct WindowState {
    bool needsRender = true;
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

double getDisplayFrameInterval(GLFWwindow* window) {
    double frameInterval = 1.0 / 60.0;

    GLFWmonitor* monitor = glfwGetWindowMonitor(window);
    if (!monitor) {
        monitor = glfwGetPrimaryMonitor();
    }

    const GLFWvidmode* videoMode = monitor ? glfwGetVideoMode(monitor) : nullptr;
    if (videoMode && videoMode->refreshRate > 0) {
        frameInterval = 1.0 / static_cast<double>(videoMode->refreshRate);
    }

    return frameInterval;
}

int main() {
    glfwInit();

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
    glfwSwapInterval(1);

    WindowState windowState;
    glfwSetWindowUserPointer(window, &windowState);
    glfwSetFramebufferSizeCallback(window, [](GLFWwindow* currentWindow, int w, int h) {
        glViewport(0, 0, w, h);
        static_cast<WindowState*>(glfwGetWindowUserPointer(currentWindow))->needsRender = true;
    });
    glfwSetWindowRefreshCallback(window, [](GLFWwindow* currentWindow) {
        static_cast<WindowState*>(glfwGetWindowUserPointer(currentWindow))->needsRender = true;
    });
    glfwSetWindowContentScaleCallback(window, [](GLFWwindow* currentWindow, float, float) {
        static_cast<WindowState*>(glfwGetWindowUserPointer(currentWindow))->needsRender = true;
    });

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        glfwTerminate();
        return -1;
    }

    if (!app::initialize(window)) {
        glfwTerminate();
        return -1;
    }

    const double animationFrameInterval = getDisplayFrameInterval(window);
    double lastFrameTime = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        const double currentFrameTime = glfwGetTime();
        const float deltaSeconds = static_cast<float>(currentFrameTime - lastFrameTime);
        lastFrameTime = currentFrameTime;

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, 1);
            break;
        }

        int framebufferWidth = 0;
        int framebufferHeight = 0;
        glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
        const float dpiScale = getDpiScale(window);
        const float pointerScale = getPointerScale(window);

        if (app::update(window, deltaSeconds, framebufferWidth, framebufferHeight, dpiScale, pointerScale)) {
            windowState.needsRender = true;
        }

        if (windowState.needsRender) {
            app::render(framebufferWidth, framebufferHeight, dpiScale);
            glfwSwapBuffers(window);
            windowState.needsRender = false;
        }

        if (app::isAnimating()) {
            const double frameElapsed = glfwGetTime() - currentFrameTime;
            const double sleepSeconds = animationFrameInterval - frameElapsed;
            if (sleepSeconds > 0.0) {
                std::this_thread::sleep_for(std::chrono::duration<double>(sleepSeconds));
            }
            glfwPollEvents();
        } else {
            glfwWaitEvents();
        }
    }

    app::shutdown();
    glfwTerminate();
    return 0;
}
