#pragma once

#include <glad/glad.h>

#include <algorithm>
#include <cmath>

namespace core {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct Color {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
};

struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    bool contains(double pointX, double pointY) const {
        return pointX >= x && pointX <= x + width &&
               pointY >= y && pointY <= y + height;
    }
};

enum class GradientDirection {
    Horizontal = 0,
    Vertical = 1
};

struct Gradient {
    bool enabled = false;
    Color start = {1.0f, 1.0f, 1.0f, 1.0f};
    Color end = {1.0f, 1.0f, 1.0f, 1.0f};
    GradientDirection direction = GradientDirection::Vertical;
};

struct Border {
    float width = 0.0f;
    Color color = {1.0f, 1.0f, 1.0f, 1.0f};
};

struct Shadow {
    bool enabled = false;
    Vec2 offset = {0.0f, 4.0f};
    float blur = 8.0f;
    float spread = 0.0f;
    Color color = {0.0f, 0.0f, 0.0f, 0.28f};
};

struct Transform {
    Vec2 translate = {0.0f, 0.0f};
    Vec2 scale = {1.0f, 1.0f};
    float rotate = 0.0f;
    Vec2 origin = {0.5f, 0.5f};
};

class RoundedRectPrimitive {
public:
    RoundedRectPrimitive() = default;

    RoundedRectPrimitive(float x, float y, float width, float height)
        : bounds_{x, y, width, height} {}

    bool initialize() {
        const char* vertexSource =
            "#version 330 core\n"
            "layout(location = 0) in vec2 aScreenPos;\n"
            "layout(location = 1) in vec2 aLocalPos;\n"
            "uniform vec2 uWindowSize;\n"
            "out vec2 vLocalPos;\n"
            "void main() {\n"
            "    vLocalPos = aLocalPos;\n"
            "    vec2 ndc = vec2((aScreenPos.x / uWindowSize.x) * 2.0 - 1.0,\n"
            "                    1.0 - (aScreenPos.y / uWindowSize.y) * 2.0);\n"
            "    gl_Position = vec4(ndc, 0.0, 1.0);\n"
            "}\n";

        const char* fragmentSource =
            "#version 330 core\n"
            "in vec2 vLocalPos;\n"
            "out vec4 FragColor;\n"
            "uniform vec4 uFillColor;\n"
            "uniform vec4 uGradientStart;\n"
            "uniform vec4 uGradientEnd;\n"
            "uniform vec4 uBorderColor;\n"
            "uniform vec4 uShadowColor;\n"
            "uniform vec4 uRect;\n"
            "uniform float uRadius;\n"
            "uniform float uBorderWidth;\n"
            "uniform float uOpacity;\n"
            "uniform float uShadowBlur;\n"
            "uniform int uUseGradient;\n"
            "uniform int uGradientDirection;\n"
            "uniform int uShadowPass;\n"
            "float roundedBoxDistance(vec2 point, vec2 halfSize, float radius) {\n"
            "    vec2 cornerVector = abs(point) - halfSize + vec2(radius);\n"
            "    return length(max(cornerVector, 0.0)) + min(max(cornerVector.x, cornerVector.y), 0.0) - radius;\n"
            "}\n"
            "void main() {\n"
            "    vec2 center = uRect.xy + uRect.zw * 0.5;\n"
            "    float distanceToEdge = roundedBoxDistance(vLocalPos - center, uRect.zw * 0.5, uRadius);\n"
            "    float blur = max(uShadowBlur, 1.0);\n"
            "    if (uShadowPass == 1) {\n"
            "        float shadowAlpha = 1.0 - smoothstep(-blur, blur, distanceToEdge);\n"
            "        if (shadowAlpha <= 0.0) discard;\n"
            "        FragColor = vec4(uShadowColor.rgb, uShadowColor.a * shadowAlpha * uOpacity);\n"
            "        return;\n"
            "    }\n"
            "    float shapeAlpha = 1.0 - smoothstep(0.0, 1.0, distanceToEdge);\n"
            "    if (shapeAlpha <= 0.0) discard;\n"
            "    float gradientAmount = uGradientDirection == 0 ?\n"
            "        clamp((vLocalPos.x - uRect.x) / max(uRect.z, 1.0), 0.0, 1.0) :\n"
            "        clamp((vLocalPos.y - uRect.y) / max(uRect.w, 1.0), 0.0, 1.0);\n"
            "    vec4 fill = uUseGradient == 1 ? mix(uGradientStart, uGradientEnd, gradientAmount) : uFillColor;\n"
            "    vec4 color = fill;\n"
            "    if (uBorderWidth > 0.0 && distanceToEdge > -uBorderWidth) {\n"
            "        color = uBorderColor;\n"
            "    }\n"
            "    FragColor = vec4(color.rgb, color.a * shapeAlpha * uOpacity);\n"
            "}\n";

        GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
        GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
        if (!vertexShader || !fragmentShader) {
            return false;
        }

        shaderProgram_ = glCreateProgram();
        glAttachShader(shaderProgram_, vertexShader);
        glAttachShader(shaderProgram_, fragmentShader);
        glLinkProgram(shaderProgram_);
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        GLint linked = 0;
        glGetProgramiv(shaderProgram_, GL_LINK_STATUS, &linked);
        if (!linked) {
            glDeleteProgram(shaderProgram_);
            shaderProgram_ = 0;
            return false;
        }

        windowSizeLocation_ = glGetUniformLocation(shaderProgram_, "uWindowSize");
        fillColorLocation_ = glGetUniformLocation(shaderProgram_, "uFillColor");
        gradientStartLocation_ = glGetUniformLocation(shaderProgram_, "uGradientStart");
        gradientEndLocation_ = glGetUniformLocation(shaderProgram_, "uGradientEnd");
        borderColorLocation_ = glGetUniformLocation(shaderProgram_, "uBorderColor");
        shadowColorLocation_ = glGetUniformLocation(shaderProgram_, "uShadowColor");
        rectLocation_ = glGetUniformLocation(shaderProgram_, "uRect");
        radiusLocation_ = glGetUniformLocation(shaderProgram_, "uRadius");
        borderWidthLocation_ = glGetUniformLocation(shaderProgram_, "uBorderWidth");
        opacityLocation_ = glGetUniformLocation(shaderProgram_, "uOpacity");
        shadowBlurLocation_ = glGetUniformLocation(shaderProgram_, "uShadowBlur");
        useGradientLocation_ = glGetUniformLocation(shaderProgram_, "uUseGradient");
        gradientDirectionLocation_ = glGetUniformLocation(shaderProgram_, "uGradientDirection");
        shadowPassLocation_ = glGetUniformLocation(shaderProgram_, "uShadowPass");

        glGenVertexArrays(1, &vao_);
        glGenBuffers(1, &vbo_);
        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 24, nullptr, GL_DYNAMIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, nullptr);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 4, reinterpret_cast<void*>(sizeof(float) * 2));
        glEnableVertexAttribArray(1);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);

        return true;
    }

    void destroy() {
        if (vbo_) {
            glDeleteBuffers(1, &vbo_);
            vbo_ = 0;
        }
        if (vao_) {
            glDeleteVertexArrays(1, &vao_);
            vao_ = 0;
        }
        if (shaderProgram_) {
            glDeleteProgram(shaderProgram_);
            shaderProgram_ = 0;
        }
    }

    void setBounds(float x, float y, float width, float height) { bounds_ = {x, y, width, height}; }
    void setColor(const Color& color) { color_ = color; }
    void setGradient(const Gradient& gradient) { gradient_ = gradient; }
    void setCornerRadius(float radius) { cornerRadius_ = radius; }
    void setOpacity(float opacity) { opacity_ = std::clamp(opacity, 0.0f, 1.0f); }
    void setBorder(const Border& border) { border_ = border; }
    void setShadow(const Shadow& shadow) { shadow_ = shadow; }
    void setTranslate(float x, float y) { transform_.translate = {x, y}; }
    void setScale(float x, float y) { transform_.scale = {x, y}; }
    void setRotate(float radians) { transform_.rotate = radians; }
    void setTransformOrigin(float x, float y) { transform_.origin = {x, y}; }
    void setTransform(const Transform& transform) { transform_ = transform; }

    const Rect& bounds() const { return bounds_; }
    const Color& color() const { return color_; }
    const Gradient& gradient() const { return gradient_; }
    const Border& border() const { return border_; }
    const Shadow& shadow() const { return shadow_; }
    const Transform& transform() const { return transform_; }
    float cornerRadius() const { return cornerRadius_; }
    float opacity() const { return opacity_; }

    void render(int windowWidth, int windowHeight) const {
        if (!shaderProgram_ || !vao_ || !vbo_) {
            return;
        }

        const GLboolean blendEnabled = glIsEnabled(GL_BLEND);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        if (shadow_.enabled) {
            drawShadow(windowWidth, windowHeight);
        }

        drawLayer(windowWidth, windowHeight, bounds_, bounds_, false, color_, shadow_.blur);

        if (!blendEnabled) {
            glDisable(GL_BLEND);
        }
    }

private:
    static GLuint compileShader(GLenum type, const char* source) {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);

        GLint compiled = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            glDeleteShader(shader);
            return 0;
        }

        return shader;
    }

    Vec2 transformPoint(float x, float y) const {
        const Vec2 origin = {
            bounds_.x + bounds_.width * transform_.origin.x,
            bounds_.y + bounds_.height * transform_.origin.y
        };

        const float scaledX = (x - origin.x) * transform_.scale.x;
        const float scaledY = (y - origin.y) * transform_.scale.y;
        const float cosine = std::cos(transform_.rotate);
        const float sine = std::sin(transform_.rotate);

        return {
            origin.x + scaledX * cosine - scaledY * sine + transform_.translate.x,
            origin.y + scaledX * sine + scaledY * cosine + transform_.translate.y
        };
    }

    static Rect expandRect(const Rect& rect, float amount) {
        return {
            rect.x - amount,
            rect.y - amount,
            rect.width + amount * 2.0f,
            rect.height + amount * 2.0f
        };
    }

    static Color withAlpha(Color color, float alphaScale) {
        color.a *= alphaScale;
        return color;
    }

    void drawShadow(int windowWidth, int windowHeight) const {
        Rect shadowShape = bounds_;
        shadowShape.x += shadow_.offset.x - shadow_.spread;
        shadowShape.y += shadow_.offset.y - shadow_.spread;
        shadowShape.width += shadow_.spread * 2.0f;
        shadowShape.height += shadow_.spread * 2.0f;

        const float blur = std::max(shadow_.blur, 1.0f);

        Rect ambientShape = shadowShape;
        ambientShape.x += shadow_.offset.x * 0.15f;
        ambientShape.y += shadow_.offset.y * 0.15f;
        drawLayer(windowWidth, windowHeight, expandRect(ambientShape, blur * 1.4f), ambientShape,
                  true, withAlpha(shadow_.color, 0.22f), blur * 1.4f);

        Rect midShape = shadowShape;
        midShape.x += shadow_.offset.x * 0.65f;
        midShape.y += shadow_.offset.y * 0.65f;
        drawLayer(windowWidth, windowHeight, expandRect(midShape, blur * 0.85f), midShape,
                  true, withAlpha(shadow_.color, 0.34f), blur * 0.85f);

        drawLayer(windowWidth, windowHeight, expandRect(shadowShape, blur * 0.38f), shadowShape,
                  true, withAlpha(shadow_.color, 0.26f), blur * 0.38f);
    }

    void drawLayer(int windowWidth,
                   int windowHeight,
                   const Rect& geometryBounds,
                   const Rect& sdfBounds,
                   bool shadowPass,
                   const Color& layerColor,
                   float blur) const {
        const float left = geometryBounds.x;
        const float top = geometryBounds.y;
        const float right = geometryBounds.x + geometryBounds.width;
        const float bottom = geometryBounds.y + geometryBounds.height;

        const Vec2 p0 = transformPoint(left, top);
        const Vec2 p1 = transformPoint(right, top);
        const Vec2 p2 = transformPoint(right, bottom);
        const Vec2 p3 = transformPoint(left, bottom);

        const float vertices[] = {
            p0.x, p0.y, left, top,
            p1.x, p1.y, right, top,
            p2.x, p2.y, right, bottom,
            p0.x, p0.y, left, top,
            p2.x, p2.y, right, bottom,
            p3.x, p3.y, left, bottom
        };

        const float radius = std::clamp(cornerRadius_, 0.0f, std::min(sdfBounds.width, sdfBounds.height) * 0.5f);
        const float borderWidth = shadowPass ? 0.0f : std::clamp(border_.width, 0.0f, std::min(sdfBounds.width, sdfBounds.height) * 0.5f);

        glUseProgram(shaderProgram_);
        glUniform2f(windowSizeLocation_, static_cast<float>(windowWidth), static_cast<float>(windowHeight));
        glUniform4f(fillColorLocation_, color_.r, color_.g, color_.b, color_.a);
        glUniform4f(gradientStartLocation_, gradient_.start.r, gradient_.start.g, gradient_.start.b, gradient_.start.a);
        glUniform4f(gradientEndLocation_, gradient_.end.r, gradient_.end.g, gradient_.end.b, gradient_.end.a);
        glUniform4f(borderColorLocation_, border_.color.r, border_.color.g, border_.color.b, border_.color.a);
        glUniform4f(shadowColorLocation_, layerColor.r, layerColor.g, layerColor.b, layerColor.a);
        glUniform4f(rectLocation_, sdfBounds.x, sdfBounds.y, sdfBounds.width, sdfBounds.height);
        glUniform1f(radiusLocation_, radius);
        glUniform1f(borderWidthLocation_, borderWidth);
        glUniform1f(opacityLocation_, opacity_);
        glUniform1f(shadowBlurLocation_, blur);
        glUniform1i(useGradientLocation_, gradient_.enabled && !shadowPass ? 1 : 0);
        glUniform1i(gradientDirectionLocation_, static_cast<int>(gradient_.direction));
        glUniform1i(shadowPassLocation_, shadowPass ? 1 : 0);

        glBindVertexArray(vao_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    Rect bounds_;
    Color color_ = {1.0f, 1.0f, 1.0f, 1.0f};
    Gradient gradient_;
    Border border_;
    Shadow shadow_;
    Transform transform_;
    float cornerRadius_ = 0.0f;
    float opacity_ = 1.0f;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint shaderProgram_ = 0;
    GLint windowSizeLocation_ = -1;
    GLint fillColorLocation_ = -1;
    GLint gradientStartLocation_ = -1;
    GLint gradientEndLocation_ = -1;
    GLint borderColorLocation_ = -1;
    GLint shadowColorLocation_ = -1;
    GLint rectLocation_ = -1;
    GLint radiusLocation_ = -1;
    GLint borderWidthLocation_ = -1;
    GLint opacityLocation_ = -1;
    GLint shadowBlurLocation_ = -1;
    GLint useGradientLocation_ = -1;
    GLint gradientDirectionLocation_ = -1;
    GLint shadowPassLocation_ = -1;
};

inline Color mixColor(const Color& from, const Color& to, float amount) {
    const float clampedAmount = std::clamp(amount, 0.0f, 1.0f);
    const float inverse = 1.0f - clampedAmount;
    return {
        from.r * inverse + to.r * clampedAmount,
        from.g * inverse + to.g * clampedAmount,
        from.b * inverse + to.b * clampedAmount,
        from.a * inverse + to.a * clampedAmount
    };
}

} // namespace core
