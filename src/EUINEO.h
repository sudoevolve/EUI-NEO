#pragma once

#include <algorithm>
#include <deque>
#include <functional>
#include <glad/glad.h>
#include <string>
#include <vector>

namespace EUINEO {

struct Color {
    float r, g, b, a;
    Color() : r(1), g(1), b(1), a(1) {}
    Color(float r, float g, float b, float a = 1.0f) : r(r), g(g), b(b), a(a) {}
};

Color Lerp(const Color& a, const Color& b, float t);
float Lerp(float a, float b, float t);

struct RectTransform {
    float translateX = 0.0f;
    float translateY = 0.0f;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    float rotationDegrees = 0.0f;
};

RectTransform Lerp(const RectTransform& a, const RectTransform& b, float t);

enum class Easing {
    Linear,
    EaseIn,
    EaseOut,
    EaseInOut
};

float ApplyEasing(Easing easing, float t);

struct RectGradient {
    bool enabled = false;
    Color topLeft = Color(1, 1, 1, 1);
    Color topRight = Color(1, 1, 1, 1);
    Color bottomLeft = Color(1, 1, 1, 1);
    Color bottomRight = Color(1, 1, 1, 1);

    static RectGradient Solid(const Color& color);
    static RectGradient Horizontal(const Color& left, const Color& right);
    static RectGradient Vertical(const Color& top, const Color& bottom);
    static RectGradient Corners(const Color& topLeft, const Color& topRight,
                                const Color& bottomLeft, const Color& bottomRight);
};

RectGradient Lerp(const RectGradient& a, const RectGradient& b, float t);

struct RectStyle {
    Color color = Color(1, 1, 1, 1);
    RectGradient gradient;
    float rounding = 0.0f;
    float blurAmount = 0.0f;
    float shadowBlur = 0.0f;
    float shadowOffsetX = 0.0f;
    float shadowOffsetY = 0.0f;
    Color shadowColor = Color(0, 0, 0, 0);
    RectTransform transform;
};

RectStyle Lerp(const RectStyle& a, const RectStyle& b, float t);

struct RectBounds {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

template <typename T>
class PropertyAnimation {
public:
    void Bind(T* target) {
        target_ = target;
        if (target_ != nullptr) {
            current_ = *target_;
            hasCurrent_ = true;
        }
    }

    void SetCurrent(const T& value) {
        current_ = value;
        hasCurrent_ = true;
        if (target_ != nullptr) {
            *target_ = value;
        }
    }

    void Play(const T& from, const T& to, float duration, Easing easing = Easing::EaseInOut) {
        queued_.clear();
        active_ = Segment{from, to, duration, easing};
        current_ = from;
        elapsed_ = 0.0f;
        running_ = true;
        hasCurrent_ = true;
        if (target_ != nullptr) {
            *target_ = current_;
        }
    }

    void PlayTo(const T& to, float duration, Easing easing = Easing::EaseInOut) {
        Play(ResolveCurrent(), to, duration, easing);
    }

    void Queue(const T& to, float duration, Easing easing = Easing::EaseInOut) {
        Segment segment;
        segment.from = ResolveQueueStart();
        segment.to = to;
        segment.duration = duration;
        segment.easing = easing;
        queued_.push_back(segment);
    }

    void Clear() {
        queued_.clear();
        elapsed_ = 0.0f;
        running_ = false;
    }

    bool Update(float dt) {
        if (!running_) {
            if (!queued_.empty()) {
                StartNextQueued();
            } else {
                return false;
            }
        }

        if (!running_) {
            return false;
        }

        elapsed_ += dt;
        if (active_.duration <= 0.0f) {
            current_ = active_.to;
        } else {
            float t = std::clamp(elapsed_ / active_.duration, 0.0f, 1.0f);
            current_ = Lerp(active_.from, active_.to, ApplyEasing(active_.easing, t));
        }

        if (target_ != nullptr) {
            *target_ = current_;
        }

        if (active_.duration <= 0.0f || elapsed_ >= active_.duration) {
            current_ = active_.to;
            if (target_ != nullptr) {
                *target_ = current_;
            }
            if (!queued_.empty()) {
                StartNextQueued();
            } else {
                running_ = false;
            }
        }
        return true;
    }

    bool IsActive() const {
        return running_ || !queued_.empty();
    }

    const T& Current() const {
        return current_;
    }

private:
    struct Segment {
        T from;
        T to;
        float duration = 0.0f;
        Easing easing = Easing::EaseInOut;
    };

    T ResolveCurrent() const {
        if (target_ != nullptr) {
            return *target_;
        }
        if (hasCurrent_) {
            return current_;
        }
        return T{};
    }

    T ResolveQueueStart() const {
        if (!queued_.empty()) {
            return queued_.back().to;
        }
        if (running_) {
            return active_.to;
        }
        return ResolveCurrent();
    }

    void StartNextQueued() {
        if (queued_.empty()) {
            running_ = false;
            return;
        }
        active_ = queued_.front();
        queued_.pop_front();
        elapsed_ = 0.0f;
        running_ = true;
        current_ = active_.from;
        hasCurrent_ = true;
        if (target_ != nullptr) {
            *target_ = current_;
        }
    }

    T* target_ = nullptr;
    T current_{};
    Segment active_{};
    std::deque<Segment> queued_;
    float elapsed_ = 0.0f;
    bool running_ = false;
    bool hasCurrent_ = false;
};

using FloatAnimation = PropertyAnimation<float>;
using ColorAnimation = PropertyAnimation<Color>;
using GradientAnimation = PropertyAnimation<RectGradient>;
using TransformAnimation = PropertyAnimation<RectTransform>;
using RectStyleAnimation = PropertyAnimation<RectStyle>;

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

struct UIState {
    float mouseX = 0.0f;
    float mouseY = 0.0f;
    bool mouseDown = false;
    bool mouseClicked = false;
    bool mouseRightDown = false;
    bool mouseRightClicked = false;
    float deltaTime = 0.0f;
    float screenW = 800.0f;
    float screenH = 600.0f;

    std::string textInput;
    bool keys[512] = {false};
    bool keysPressed[512] = {false};

    bool needsRepaint = true;
    float animationTimeLeft = 0.0f;
    int frameCount = 0;

    bool fullScreenDirty = true;
    float dirtyX1 = 0.0f;
    float dirtyY1 = 0.0f;
    float dirtyX2 = 0.0f;
    float dirtyY2 = 0.0f;
};

extern UIState State;

class Renderer {
public:
    static void Init();
    static void Shutdown();
    static void BeginFrame();
    static void SetFullRedraw();
    static void SetPartialRedraw(float x1, float y1, float x2, float y2);

    static void ApplyScissor();
    static void ClearDirtyRect();
    static void AddDirtyRect(float x, float y, float w, float h);
    static void InvalidateAll();
    static void InvalidateBackdrop();

    static RectBounds MeasureRectBounds(float x, float y, float w, float h, const RectStyle& style);
    static void DrawRect(float x, float y, float w, float h, const RectStyle& style);
    static void DrawRect(float x, float y, float w, float h, const Color& color, float rounding = 0.0f,
                         float blurAmount = 0.0f, float shadowBlur = 0.0f,
                         float shadowOffsetX = 0.0f, float shadowOffsetY = 0.0f,
                         const Color& shadowColor = Color(0, 0, 0, 0));

    static bool LoadFont(const std::string& fontPath, float fontSize = 24.0f,
                         unsigned int startChar = 32, unsigned int endChar = 128);
    static void DrawTextStr(const std::string& text, float x, float y, const Color& color, float scale = 1.0f);
    static float MeasureTextWidth(const std::string& text, float scale = 1.0f);
    static void RequestRepaint(float duration = 0.0f);
    static bool ShouldRepaint();
};

enum class Anchor {
    TopLeft, TopCenter, TopRight,
    CenterLeft, Center, CenterRight,
    BottomLeft, BottomCenter, BottomRight
};

class Widget {
public:
    float x = 0.0f;
    float y = 0.0f;
    float width = 100.0f;
    float height = 30.0f;
    Anchor anchor = Anchor::TopLeft;
    float fontSize = 24.0f;

    virtual ~Widget() = default;
    virtual void Update() {}
    virtual void Draw() {}

    void GetAbsoluteBounds(float& outX, float& outY);
    bool IsHovered();
    void MarkDirty(const RectStyle& style, float expand = 0.0f, float duration = 0.0f);
    void MarkDirty(const RectStyle& fromStyle, const RectStyle& toStyle, float expand = 0.0f,
                   float duration = 0.0f);
    void MarkDirty(float expand = 20.0f, float duration = 0.0f);
};

} // namespace EUINEO
