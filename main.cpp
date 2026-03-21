#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "src/EUINEO.h"
#include "src/pages/MainPage.h"

// 记录鼠标状态以防多重点击
bool lastLeftDown = false;
bool lastRightDown = false;

int main() {
    glfwInit();
    // 关键优化：禁用多重采样抗锯齿 (MSAA)，默认可能会开启从而占用大量显存和内存
    glfwWindowHint(GLFW_SAMPLES, 4); 
    // 请求单缓冲或尽量小的颜色/深度缓冲 (按需)
    glfwWindowHint(GLFW_RED_BITS, 8);
    glfwWindowHint(GLFW_GREEN_BITS, 8);
    glfwWindowHint(GLFW_BLUE_BITS, 8);
    glfwWindowHint(GLFW_ALPHA_BITS, 8);
    glfwWindowHint(GLFW_DEPTH_BITS, 16); // 降低深度缓冲位数
    glfwWindowHint(GLFW_STENCIL_BITS, 0); // 禁用模板缓冲

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "EUI-NEO", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, [](GLFWwindow*, int w, int h) { glViewport(0, 0, w, h); });

    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    // 注册输入回调
    glfwSetCursorPosCallback(window, [](GLFWwindow*, double x, double y) {
        EUINEO::State.mouseX = (float)x;
        EUINEO::State.mouseY = (float)y;
        EUINEO::Renderer::RequestRepaint(); // 鼠标移动触发重绘
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
        EUINEO::Renderer::RequestRepaint(); // 点击触发重绘
    });
    glfwSetCharCallback(window, [](GLFWwindow*, unsigned int codepoint) {
        // Convert codepoint to utf-8 and append
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
    glfwSetKeyCallback(window, [](GLFWwindow*, int key, int scancode, int action, int mods) {
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

    // 开启垂直同步 (VSync)，限制 CPU 的空转循环
    glfwSwapInterval(1);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // 初始化 EUINEO
    EUINEO::Renderer::Init();
    
    // 1. 加载主字体 (英文+数字等基础字符)
    bool fontLoaded = false;
    if (EUINEO::Renderer::LoadFont("../src/font/Medicinal Compound.otf", 24.0f, 32, 128)) {
        fontLoaded = true;
    }
    
    // 2. 加载 Font Awesome 7 图标字体 (只加载常用的部分，避免占用过多内存)
    // 0xF000 到 0xF2FF 包含了数百个最核心的 Font Awesome 图标 (如设置、用户、搜索等)
    EUINEO::Renderer::LoadFont("../src/font/Font Awesome 7 Free-Solid-900.otf", 24.0f, 0xF000, 0xF2FF);
    
    if (!fontLoaded) {
        if (EUINEO::Renderer::LoadFont("C:/Windows/Fonts/msyh.ttc", 24.0f, 32, 128)) { // 尝试微软雅黑
            EUINEO::Renderer::LoadFont("C:/Windows/Fonts/msyh.ttc", 24.0f, 0x4E00, 0x9FA5); // 中文基本区
        } else if (!EUINEO::Renderer::LoadFont("C:/Windows/Fonts/arial.ttf", 24.0f)) {
            printf("Failed to load fallback font!\n");
        }
    }

    // 实例化主页面
    EUINEO::MainPage mainPage;

    double lastTime = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        if(glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
            glfwSetWindowShouldClose(window, 1);

        // 1. 更新时间和输入状态
        double currentTime = glfwGetTime();
        EUINEO::State.deltaTime = (float)(currentTime - lastTime);
        lastTime = currentTime;

        int w, h;
        glfwGetWindowSize(window, &w, &h);
        EUINEO::State.screenW = (float)w;
        EUINEO::State.screenH = (float)h;

        double mx, my;
        glfwGetCursorPos(window, &mx, &my);
        EUINEO::State.mouseX = (float)mx;
        EUINEO::State.mouseY = (float)my;

        bool leftDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        bool rightDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
        
        EUINEO::State.mouseDown = leftDown;
        EUINEO::State.mouseRightDown = rightDown;
        EUINEO::State.mouseClicked = (leftDown && !lastLeftDown);
        EUINEO::State.mouseRightClicked = (rightDown && !lastRightDown);
        
        lastLeftDown = leftDown;
        lastRightDown = rightDown;

        // 2. 更新组件逻辑和动效
        mainPage.Update();

        // 3. 渲染清屏
        EUINEO::Color bg = EUINEO::CurrentTheme->background;
        glClearColor(bg.r, bg.g, bg.b, bg.a);
        glClear(GL_COLOR_BUFFER_BIT);

        // 4. GUI 渲染通道
        EUINEO::Renderer::BeginFrame();
        mainPage.Draw();

        glfwSwapBuffers(window);
        
        // 清理当前帧状态
        EUINEO::State.textInput.clear();
        memset(EUINEO::State.keysPressed, 0, sizeof(EUINEO::State.keysPressed));

        // 智能刷新控制：静止时等待事件，有动效时依赖 VSync 控制帧率和占用
        if (EUINEO::Renderer::ShouldRepaint()) {
            glfwPollEvents();
        } else {
            glfwWaitEventsTimeout(0.05); // 休眠 50ms，或者直到有输入事件
        }
    }

    EUINEO::Renderer::Shutdown();
    glfwTerminate();
    return 0;
}
