#include <cmath>
#include <cstdio>
#include <cstring>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "src/EUINEO.h"
#include "src/pages/MainPage.h"

bool lastLeftDown = false;
bool lastRightDown = false;

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

    GLFWwindow* window = glfwCreateWindow(800, 600, "EUI-NEO", nullptr, nullptr);
    glfwMakeContextCurrent(window);

    glfwSetFramebufferSizeCallback(window, [](GLFWwindow*, int w, int h) {
        glViewport(0, 0, w, h);
        EUINEO::State.screenW = (float)w;
        EUINEO::State.screenH = (float)h;
        EUINEO::Renderer::InvalidateBackdrop();
        EUINEO::Renderer::InvalidateAll();
    });

    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    int initialFbW = 0;
    int initialFbH = 0;
    glfwGetFramebufferSize(window, &initialFbW, &initialFbH);
    glViewport(0, 0, initialFbW, initialFbH);
    EUINEO::State.screenW = (float)initialFbW;
    EUINEO::State.screenH = (float)initialFbH;

    glfwSetCursorPosCallback(window, [](GLFWwindow*, double x, double y) {
        const float nextX = (float)x;
        const float nextY = (float)y;
        if (std::abs(EUINEO::State.mouseX - nextX) > 0.01f ||
            std::abs(EUINEO::State.mouseY - nextY) > 0.01f) {
            EUINEO::State.mouseX = nextX;
            EUINEO::State.mouseY = nextY;
            EUINEO::Renderer::RequestRepaint();
            return;
        }
        EUINEO::State.mouseX = nextX;
        EUINEO::State.mouseY = nextY;
    });

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

    bool fontLoaded = false;
    if (EUINEO::Renderer::LoadFont("src/font/Mountain and Nature.ttf", 24.0f, 32, 128) ||
        EUINEO::Renderer::LoadFont("../src/font/Mountain and Nature.ttf", 24.0f, 32, 128) ||
        EUINEO::Renderer::LoadFont("../../src/font/Mountain and Nature.ttf", 24.0f, 32, 128)) {
        fontLoaded = true;
    }

    if (!EUINEO::Renderer::LoadFont("font/Font Awesome 7 Free-Solid-900.otf", 24.0f, 0xF000, 0xF2FF)) {
        EUINEO::Renderer::LoadFont("../src/font/Font Awesome 7 Free-Solid-900.otf", 24.0f, 0xF000, 0xF2FF);
    }

    if (!fontLoaded) {
        if (EUINEO::Renderer::LoadFont("C:/Windows/Fonts/msyh.ttc", 24.0f, 32, 128)) {
            fontLoaded = true;
        } else if (EUINEO::Renderer::LoadFont("C:/Windows/Fonts/arial.ttf", 24.0f)) {
            fontLoaded = true;
        } else {
            printf("Failed to load fallback font!\n");
        }
    }

    EUINEO::Renderer::LoadFont("C:/Windows/Fonts/msyh.ttc", 24.0f, 0x4E00, 0xA000);

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

        int w = 0;
        int h = 0;
        glfwGetFramebufferSize(window, &w, &h);
        EUINEO::State.screenW = (float)w;
        EUINEO::State.screenH = (float)h;

        double mx = 0.0;
        double my = 0.0;
        glfwGetCursorPos(window, &mx, &my);
        int winW = 0;
        int winH = 0;
        glfwGetWindowSize(window, &winW, &winH);
        float mouseScaleX = (winW > 0) ? ((float)w / (float)winW) : 1.0f;
        float mouseScaleY = (winH > 0) ? ((float)h / (float)winH) : 1.0f;
        EUINEO::State.mouseX = (float)mx * mouseScaleX;
        EUINEO::State.mouseY = (float)my * mouseScaleY;

        bool leftDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        bool rightDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

        EUINEO::State.mouseDown = leftDown;
        EUINEO::State.mouseRightDown = rightDown;
        EUINEO::State.mouseClicked = (leftDown && !lastLeftDown);
        EUINEO::State.mouseRightClicked = (rightDown && !lastRightDown);

        lastLeftDown = leftDown;
        lastRightDown = rightDown;

        const bool frameRequestedBeforeUpdate =
            EUINEO::State.needsRepaint ||
            EUINEO::State.animationTimeLeft > 0.0f ||
            mainPage.WantsContinuousUpdate();
        if (frameRequestedBeforeUpdate) {
            mainPage.Update();
        }

        bool shouldDraw = EUINEO::Renderer::ShouldRepaint();
        if (shouldDraw) {
            EUINEO::State.frameCount++;
            EUINEO::Renderer::SetLayerBounds(EUINEO::RenderLayer::Backdrop, mainPage.LayerBounds(EUINEO::RenderLayer::Backdrop));
            if (EUINEO::Renderer::NeedsLayerRedraw(EUINEO::RenderLayer::Backdrop)) {
                EUINEO::Renderer::BeginLayer(EUINEO::RenderLayer::Backdrop);
                EUINEO::Renderer::BeginFrame();
                mainPage.DrawLayer(EUINEO::RenderLayer::Backdrop);
                EUINEO::Renderer::EndLayer();
            }

            EUINEO::Renderer::CompositeLayers(EUINEO::CurrentTheme->background);
            EUINEO::Renderer::BeginFrame();
            mainPage.Draw();
            glfwSwapBuffers(window);
        }

        EUINEO::State.textInput.clear();
        memset(EUINEO::State.keysPressed, 0, sizeof(EUINEO::State.keysPressed));

        if (shouldDraw || EUINEO::State.animationTimeLeft > 0) {
            glfwPollEvents();
        } else if (mainPage.WantsContinuousUpdate()) {
            glfwWaitEventsTimeout(1.0 / 30.0);
        } else {
            glfwWaitEvents();
        }
    }

    EUINEO::Renderer::Shutdown();
    glfwTerminate();
    return 0;
}
