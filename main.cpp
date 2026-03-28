#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <string>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "src/EUINEO.h"
#include "src/pages/MainPage.h"

namespace {

int gFramebufferW = 0;
int gFramebufferH = 0;
int gWindowW = 0;
int gWindowH = 0;
float gContentScaleX = 1.0f;
float gContentScaleY = 1.0f;

void SyncSurfaceMetrics() {
    const float fallbackScaleX = gWindowW > 0 ? static_cast<float>(gFramebufferW) / static_cast<float>(std::max(1, gWindowW)) : 1.0f;
    const float fallbackScaleY = gWindowH > 0 ? static_cast<float>(gFramebufferH) / static_cast<float>(std::max(1, gWindowH)) : 1.0f;
    const float scaleX = std::max(0.5f, std::max(gContentScaleX, fallbackScaleX));
    const float scaleY = std::max(0.5f, std::max(gContentScaleY, fallbackScaleY));
    const float logicalW = static_cast<float>(std::max(1, gFramebufferW)) / scaleX;
    const float logicalH = static_cast<float>(std::max(1, gFramebufferH)) / scaleY;
    const float framebufferW = static_cast<float>(std::max(1, gFramebufferW));
    const float framebufferH = static_cast<float>(std::max(1, gFramebufferH));
    EUINEO::State.screenW = logicalW;
    EUINEO::State.screenH = logicalH;
    EUINEO::State.framebufferW = framebufferW;
    EUINEO::State.framebufferH = framebufferH;
    EUINEO::State.dpiScaleX = scaleX;
    EUINEO::State.dpiScaleY = scaleY;
}

void UpdateMousePosition(double x, double y) {
    const float rawX = static_cast<float>(x);
    const float rawY = static_cast<float>(y);
    const float windowW = static_cast<float>(std::max(1, gWindowW));
    const float windowH = static_cast<float>(std::max(1, gWindowH));
    const float framebufferW = static_cast<float>(std::max(1, gFramebufferW));
    const float framebufferH = static_cast<float>(std::max(1, gFramebufferH));

    const bool cursorLooksLikeWindowSpaceX = rawX <= windowW + 0.5f;
    const bool cursorLooksLikeWindowSpaceY = rawY <= windowH + 0.5f;
    const float normalizedX = cursorLooksLikeWindowSpaceX ? (rawX / windowW) : (rawX / framebufferW);
    const float normalizedY = cursorLooksLikeWindowSpaceY ? (rawY / windowH) : (rawY / framebufferH);
    const float nextX = std::clamp(normalizedX, 0.0f, 1.0f) * EUINEO::State.screenW;
    const float nextY = std::clamp(normalizedY, 0.0f, 1.0f) * EUINEO::State.screenH;
    if (std::abs(EUINEO::State.mouseX - nextX) > 0.01f ||
        std::abs(EUINEO::State.mouseY - nextY) > 0.01f) {
        EUINEO::State.pointerMoved = true;
    }
    EUINEO::State.mouseX = nextX;
    EUINEO::State.mouseY = nextY;
}

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
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "EUI-NEO", nullptr, nullptr);
    glfwMakeContextCurrent(window);

    glfwSetFramebufferSizeCallback(window, [](GLFWwindow* win, int w, int h) {
        glViewport(0, 0, w, h);
        gFramebufferW = w;
        gFramebufferH = h;
        glfwGetWindowSize(win, &gWindowW, &gWindowH);
        SyncSurfaceMetrics();
        EUINEO::Renderer::InvalidateAll();
    });

    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    int initialFbW = 0;
    int initialFbH = 0;
    glfwGetFramebufferSize(window, &initialFbW, &initialFbH);
    gFramebufferW = initialFbW;
    gFramebufferH = initialFbH;
    glViewport(0, 0, initialFbW, initialFbH);
    glfwGetWindowSize(window, &gWindowW, &gWindowH);
    glfwGetWindowContentScale(window, &gContentScaleX, &gContentScaleY);
    SyncSurfaceMetrics();

    glfwSetWindowSizeCallback(window, [](GLFWwindow* win, int w, int h) {
        gWindowW = w;
        gWindowH = h;
        glfwGetWindowContentScale(win, &gContentScaleX, &gContentScaleY);
        SyncSurfaceMetrics();
        EUINEO::Renderer::InvalidateAll();
    });

    glfwSetWindowContentScaleCallback(window, [](GLFWwindow* win, float xscale, float yscale) {
        gContentScaleX = xscale;
        gContentScaleY = yscale;
        glfwGetWindowSize(win, &gWindowW, &gWindowH);
        glfwGetFramebufferSize(win, &gFramebufferW, &gFramebufferH);
        SyncSurfaceMetrics();
        EUINEO::Renderer::InvalidateAll();
    });

    glfwSetCursorPosCallback(window, [](GLFWwindow*, double x, double y) {
        UpdateMousePosition(x, y);
    });
    double initialMouseX = 0.0;
    double initialMouseY = 0.0;
    glfwGetCursorPos(window, &initialMouseX, &initialMouseY);
    UpdateMousePosition(initialMouseX, initialMouseY);

    glfwSetMouseButtonCallback(window, [](GLFWwindow*, int button, int action, int mods) {
        if (button == GLFW_MOUSE_BUTTON_LEFT) {
            if (action == GLFW_PRESS) {
                EUINEO::State.mouseDown = true;
                EUINEO::State.mouseClicked = true;
            } else if (action == GLFW_RELEASE) {
                EUINEO::State.mouseDown = false;
            }
        } else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
            if (action == GLFW_PRESS) {
                EUINEO::State.mouseRightDown = true;
                EUINEO::State.mouseRightClicked = true;
            } else if (action == GLFW_RELEASE) {
                EUINEO::State.mouseRightDown = false;
            }
        }
        EUINEO::Renderer::RequestRepaint();
    });

    glfwSetCharCallback(window, [](GLFWwindow*, unsigned int codepoint) {
        if (codepoint <= 0x7f) {
            EUINEO::State.textInput += (char)codepoint;
        } else if (codepoint <= 0x7ff) {
            EUINEO::State.textInput += (char)(0xc0 | ((codepoint >> 6) & 0x1f));
            EUINEO::State.textInput += (char)(0x80 | (codepoint & 0x3f));
        } else if (codepoint <= 0xffff) {
            EUINEO::State.textInput += (char)(0xe0 | ((codepoint >> 12) & 0x0f));
            EUINEO::State.textInput += (char)(0x80 | ((codepoint >> 6) & 0x3f));
            EUINEO::State.textInput += (char)(0x80 | (codepoint & 0x3f));
        } else if (codepoint <= 0x10ffff) {
            EUINEO::State.textInput += (char)(0xf0 | ((codepoint >> 18) & 0x07));
            EUINEO::State.textInput += (char)(0x80 | ((codepoint >> 12) & 0x3f));
            EUINEO::State.textInput += (char)(0x80 | ((codepoint >> 6) & 0x3f));
            EUINEO::State.textInput += (char)(0x80 | (codepoint & 0x3f));
        }
        EUINEO::Renderer::RequestRepaint();
    });

    glfwSetScrollCallback(window, [](GLFWwindow*, double xoffset, double yoffset) {
        EUINEO::State.scrollDeltaX += static_cast<float>(xoffset);
        EUINEO::State.scrollDeltaY += static_cast<float>(yoffset);
        EUINEO::Renderer::RequestRepaint();
    });

    glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
        if (key >= 0 && key < 512) {
            if (action == GLFW_PRESS) {
                EUINEO::State.keys[key] = true;
                EUINEO::State.keysPressed[key] = true;
                EUINEO::Renderer::RequestRepaint();
            } else if (action == GLFW_RELEASE) {
                EUINEO::State.keys[key] = false;
                EUINEO::Renderer::RequestRepaint();
            } else if (action == GLFW_REPEAT) {
                EUINEO::State.keysPressed[key] = true;
                EUINEO::Renderer::RequestRepaint();
            }
        }
    });

    glfwSwapInterval(1);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_MULTISAMPLE);

    EUINEO::Renderer::Init();

    constexpr const char* kUIFontFile = "YouSheBiaoTiHei-2.ttf";
    constexpr const char* kIconFontFile = "Font Awesome 7 Free-Solid-900.otf";
    constexpr float kUiSdfLoadSize = 72.0f;
    constexpr float kIconSdfLoadSize = 96.0f;
    constexpr float kCjkSdfLoadSize = 72.0f;

    const auto loadProjectFont = [](const char* fileName,
                                    float fontSize,
                                    unsigned int startChar,
                                    unsigned int endChar,
                                    bool useSdf = true) {
        static const char* kFontDirs[] = {
            "font/",
            "src/font/"
        };
        for (const char* dir : kFontDirs) {
            const std::string path = std::string(dir) + fileName;
            if (EUINEO::Renderer::LoadFont(path, fontSize, startChar, endChar, useSdf)) {
                return true;
            }
        }
        return false;
    };

    const auto loadProjectIcon = [&](unsigned int codepoint) {
        return loadProjectFont(kIconFontFile, kIconSdfLoadSize, codepoint, codepoint + 1, false);
    };

    bool fontLoaded = false;
    if (loadProjectFont(kUIFontFile, kUiSdfLoadSize, 32, 128)) {
        fontLoaded = true;
    }

    loadProjectIcon(0xF009); // grid
    loadProjectIcon(0xF013); // gear
    loadProjectIcon(0xF015); // home
    loadProjectIcon(0xF031); // font
    loadProjectIcon(0xF04B); // play
    loadProjectIcon(0xF106); // chevron-up
    loadProjectIcon(0xF107); // chevron-down
    loadProjectIcon(0xF185); // sun
    loadProjectIcon(0xF186); // moon

    if (!fontLoaded) {
        if (EUINEO::Renderer::LoadFont("C:/Windows/Fonts/msyh.ttc", kUiSdfLoadSize, 32, 128)) {
            fontLoaded = true;
        } else if (EUINEO::Renderer::LoadFont("C:/Windows/Fonts/arial.ttf", kUiSdfLoadSize)) {
            fontLoaded = true;
        } else {
            printf("Failed to load fallback font!\n");
        }
    }

    EUINEO::Renderer::RegisterFontSource("C:/Windows/Fonts/msyh.ttc", kCjkSdfLoadSize); // Deferred fallback for missing glyphs.

    EUINEO::MainPage mainPage{}; // Force recompilation when header-only pages change.
    double lastTime = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, 1);
        }

        double currentTime = glfwGetTime();
        float dt = (float)(currentTime - lastTime);
        EUINEO::State.deltaTime = dt > 0.05f ? 0.05f : dt;
        lastTime = currentTime;

        const bool frameRequestedBeforeUpdate =
            EUINEO::State.needsRepaint ||
            EUINEO::State.animationTimeLeft > 0.0f ||
            EUINEO::State.pointerMoved;
        if (frameRequestedBeforeUpdate) {
            mainPage.Update();
        }

        bool shouldDraw = EUINEO::Renderer::ShouldRepaint();
        if (shouldDraw) {
            EUINEO::State.frameCount++;
            mainPage.Draw();
            glfwSwapBuffers(window);
        }

        EUINEO::State.textInput.clear();
        EUINEO::State.scrollDeltaX = 0.0f;
        EUINEO::State.scrollDeltaY = 0.0f;
        EUINEO::State.scrollConsumed = false;
        EUINEO::State.mouseClicked = false;
        EUINEO::State.mouseRightClicked = false;
        EUINEO::State.pointerMoved = false;
        memset(EUINEO::State.keysPressed, 0, sizeof(EUINEO::State.keysPressed));

        if (shouldDraw || EUINEO::State.animationTimeLeft > 0) {
            glfwPollEvents();
        } else {
            glfwWaitEvents();
        }
    }

    EUINEO::Renderer::Shutdown();
    glfwTerminate();
    return 0;
}
