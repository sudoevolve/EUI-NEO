#pragma once
#include <glad/glad.h>
#include <string>
#include <vector>
#include <functional>

namespace EUINEO {

// --- 基础数学与颜色 ---
struct Color {
    float r, g, b, a;
    Color() : r(1), g(1), b(1), a(1) {}
    Color(float r, float g, float b, float a = 1.0f) : r(r), g(g), b(b), a(a) {}
};

Color Lerp(const Color& a, const Color& b, float t);
float Lerp(float a, float b, float t);

// --- Theme Tokens (支持明暗双色与主题色) ---
struct Theme {
    Color background;
    Color primary;
    Color surface;
    Color surfaceHover;
    Color surfaceActive;
    Color text;
    Color border;
};

extern Theme LightTheme;
extern Theme DarkTheme;
extern Theme* CurrentTheme;

// --- 全局状态管理 ---
struct UIState {
    float mouseX = 0, mouseY = 0;
    bool mouseDown = false;
    bool mouseClicked = false;
    bool mouseRightDown = false;
    bool mouseRightClicked = false;
    float deltaTime = 0;
    float screenW = 800, screenH = 600;
    
    // 键盘输入
    std::string textInput;
    bool keys[512] = {false};
    bool keysPressed[512] = {false};
    
    // 智能刷新控制
    bool needsRepaint = true;
    float animationTimeLeft = 0.0f; // 剩余动效时间
};
extern UIState State;

// --- 极简 OpenGL 2D 渲染器 ---
class Renderer {
public:
    static void Init();
    static void Shutdown();
    static void BeginFrame();
    
    // --- 渲染接口 ---
    static void DrawRect(float x, float y, float w, float h, const Color& color, float rounding = 0.0f);
    
    // 文本渲染相关
    static bool LoadFont(const std::string& fontPath, float fontSize = 24.0f, unsigned int startChar = 32, unsigned int endChar = 128);
    static void DrawTextStr(const std::string& text, float x, float y, const Color& color, float scale = 1.0f);
    static float MeasureTextWidth(const std::string& text, float scale = 1.0f);
    // 触发重绘
    static void RequestRepaint(float duration = 0.0f);
    static bool ShouldRepaint();
};

// --- 锚点布局系统 ---
enum class Anchor {
    TopLeft, TopCenter, TopRight,
    CenterLeft, Center, CenterRight,
    BottomLeft, BottomCenter, BottomRight
};

class Widget {
public:
    float x = 0, y = 0, width = 100, height = 30;
    Anchor anchor = Anchor::TopLeft;
    float fontSize = 24.0f; // 组件字体大小
    
    virtual ~Widget() = default;
    virtual void Update() {}
    virtual void Draw() {}
    
    void GetAbsoluteBounds(float& outX, float& outY);
    bool IsHovered();
};

} // namespace EUINEO