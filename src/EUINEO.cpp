#include "EUINEO.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <vector>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

namespace EUINEO {

Color Lerp(const Color& a, const Color& b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return Color(
        a.r + (b.r - a.r) * t,
        a.g + (b.g - a.g) * t,
        a.b + (b.b - a.b) * t,
        a.a + (b.a - a.a) * t
    );
}

float Lerp(float a, float b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return a + (b - a) * t;
}

RectTransform Lerp(const RectTransform& a, const RectTransform& b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    RectTransform out;
    out.translateX = Lerp(a.translateX, b.translateX, t);
    out.translateY = Lerp(a.translateY, b.translateY, t);
    out.scaleX = Lerp(a.scaleX, b.scaleX, t);
    out.scaleY = Lerp(a.scaleY, b.scaleY, t);
    out.rotationDegrees = Lerp(a.rotationDegrees, b.rotationDegrees, t);
    return out;
}

float ApplyEasing(Easing easing, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    switch (easing) {
        case Easing::EaseIn:
            return t * t * t;
        case Easing::EaseOut: {
            float inv = 1.0f - t;
            return 1.0f - inv * inv * inv;
        }
        case Easing::EaseInOut:
            if (t < 0.5f) {
                return 4.0f * t * t * t;
            }
            return 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) * 0.5f;
        case Easing::Linear:
        default:
            return t;
    }
}

RectGradient RectGradient::Solid(const Color& color) {
    RectGradient gradient;
    gradient.enabled = true;
    gradient.topLeft = color;
    gradient.topRight = color;
    gradient.bottomLeft = color;
    gradient.bottomRight = color;
    return gradient;
}

RectGradient RectGradient::Horizontal(const Color& left, const Color& right) {
    RectGradient gradient;
    gradient.enabled = true;
    gradient.topLeft = left;
    gradient.bottomLeft = left;
    gradient.topRight = right;
    gradient.bottomRight = right;
    return gradient;
}

RectGradient RectGradient::Vertical(const Color& top, const Color& bottom) {
    RectGradient gradient;
    gradient.enabled = true;
    gradient.topLeft = top;
    gradient.topRight = top;
    gradient.bottomLeft = bottom;
    gradient.bottomRight = bottom;
    return gradient;
}

RectGradient RectGradient::Corners(const Color& topLeft, const Color& topRight,
                                   const Color& bottomLeft, const Color& bottomRight) {
    RectGradient gradient;
    gradient.enabled = true;
    gradient.topLeft = topLeft;
    gradient.topRight = topRight;
    gradient.bottomLeft = bottomLeft;
    gradient.bottomRight = bottomRight;
    return gradient;
}

RectGradient Lerp(const RectGradient& a, const RectGradient& b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    RectGradient out;
    out.enabled = a.enabled || b.enabled;
    out.topLeft = Lerp(a.topLeft, b.topLeft, t);
    out.topRight = Lerp(a.topRight, b.topRight, t);
    out.bottomLeft = Lerp(a.bottomLeft, b.bottomLeft, t);
    out.bottomRight = Lerp(a.bottomRight, b.bottomRight, t);
    return out;
}

static RectGradient ResolveGradientForStyle(const RectStyle& style) {
    if (style.gradient.enabled) {
        return style.gradient;
    }

    Color solid = style.color;
    solid.a = 1.0f;
    return RectGradient::Solid(solid);
}

RectStyle Lerp(const RectStyle& a, const RectStyle& b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    RectStyle out;
    out.color = Lerp(a.color, b.color, t);
    out.gradient = Lerp(ResolveGradientForStyle(a), ResolveGradientForStyle(b), t);
    out.gradient.enabled = a.gradient.enabled || b.gradient.enabled;
    out.rounding = Lerp(a.rounding, b.rounding, t);
    out.blurAmount = Lerp(a.blurAmount, b.blurAmount, t);
    out.shadowBlur = Lerp(a.shadowBlur, b.shadowBlur, t);
    out.shadowOffsetX = Lerp(a.shadowOffsetX, b.shadowOffsetX, t);
    out.shadowOffsetY = Lerp(a.shadowOffsetY, b.shadowOffsetY, t);
    out.shadowColor = Lerp(a.shadowColor, b.shadowColor, t);
    out.transform = Lerp(a.transform, b.transform, t);
    return out;
}

Theme LightTheme = {
    Color(0.95f, 0.95f, 0.97f),
    Color(0.2f, 0.5f, 0.9f),
    Color(1.0f, 1.0f, 1.0f),
    Color(0.9f, 0.9f, 0.9f),
    Color(0.8f, 0.8f, 0.8f),
    Color(0.0f, 0.0f, 0.0f),
    Color(0.8f, 0.8f, 0.8f)
};

Theme DarkTheme = {
    Color(0.1f, 0.1f, 0.12f),
    Color(0.3f, 0.6f, 1.0f),
    Color(0.15f, 0.15f, 0.18f),
    Color(0.25f, 0.25f, 0.28f),
    Color(0.35f, 0.35f, 0.38f),
    Color(1.0f, 1.0f, 1.0f),
    Color(0.3f, 0.3f, 0.3f)
};

Theme* CurrentTheme = &DarkTheme;
UIState State;

static GLuint VAO = 0;
static GLuint VBO = 0;
static GLuint ShaderProgram = 0;
static GLint ProjLoc = -1;
static GLint ColorLoc = -1;
static GLint PosLoc = -1;
static GLint SizeLoc = -1;
static GLint RoundingLoc = -1;
static GLint BoxPosLoc = -1;
static GLint BoxSizeLoc = -1;
static GLint TranslateLoc = -1;
static GLint ScaleLoc = -1;
static GLint RotationLoc = -1;
static GLint TransformInvLoc = -1;
static GLint BlurAmountLoc = -1;
static GLint ShadowBlurLoc = -1;
static GLint ShadowOffsetLoc = -1;
static GLint ShadowColorLoc = -1;
static GLint GradientEnabledLoc = -1;
static GLint GradientTopLeftLoc = -1;
static GLint GradientTopRightLoc = -1;
static GLint GradientBottomLeftLoc = -1;
static GLint GradientBottomRightLoc = -1;
static GLint TimeLoc = -1;
static GLint ResolutionLoc = -1;
static GLint Channel0Loc = -1;
static GLuint BgTexture = 0;
static GLuint CachedBlurProgram = 0;
static GLint CachedBlurProjLoc = -1;
static GLint CachedBlurPosLoc = -1;
static GLint CachedBlurSizeLoc = -1;
static GLint CachedBlurTextureLoc = -1;
static GLuint CachedBlurTexture = 0;
static int CachedBlurTextureW = 0;
static int CachedBlurTextureH = 0;
static bool BlurCacheValid = false;
static float CachedBlurX = 0.0f;
static float CachedBlurY = 0.0f;
static float CachedBlurW = 0.0f;
static float CachedBlurH = 0.0f;
static float CachedBlurColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
static float CachedBlurShadowColor[4] = {0.0f, 0.0f, 0.0f, 0.0f};
static float CachedBlurRounding = 0.0f;
static float CachedBlurAmount = 0.0f;
static float CachedBlurShadowBlur = 0.0f;
static float CachedBlurShadowOffsetX = 0.0f;
static float CachedBlurShadowOffsetY = 0.0f;
static RectTransform CachedBlurTransform;
static RectGradient CachedBlurGradient;
static unsigned int BackdropVersion = 1;
static unsigned int CachedBackdropVersion = 0;
static bool ActiveRedrawClipEnabled = false;
static float ActiveRedrawX1 = 0.0f;
static float ActiveRedrawY1 = 0.0f;
static float ActiveRedrawX2 = 0.0f;
static float ActiveRedrawY2 = 0.0f;

static const char* vShaderStr = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
out vec2 vPos;
out vec2 vLocal;
out vec2 vUV;
uniform mat4 projection;
uniform vec2 uPos;
uniform vec2 uSize;
uniform vec2 uTranslate;
uniform vec2 uScale;
uniform float uRotation;
void main() {
    vec2 center = uPos + uSize * 0.5 + uTranslate;
    vec2 local = (aPos - vec2(0.5)) * uSize;
    vec2 scaled = local * uScale;
    float rotation = radians(uRotation);
    float c = cos(rotation);
    float s = sin(rotation);
    vec2 rotated = vec2(
        c * scaled.x - s * scaled.y,
        s * scaled.x + c * scaled.y
    );
    vec2 pos = center + rotated;
    vPos = pos;
    vLocal = local;
    vUV = aPos;
    gl_Position = projection * vec4(pos, 0.0, 1.0);
}
)";

static const char* fShaderStr = R"(
#version 330 core
in vec2 vPos;
in vec2 vLocal;
in vec2 vUV;
uniform vec4 uColor;
uniform vec2 uPos;
uniform vec2 uSize;
uniform vec2 uBoxPos;
uniform vec2 uBoxSize;
uniform vec2 uTranslate;
uniform vec2 uScale;
uniform float uRotation;
uniform mat2 uTransformInv;
uniform float uRounding;
uniform float uBlurAmount;
uniform float uShadowBlur;
uniform vec2 uShadowOffset;
uniform vec4 uShadowColor;
uniform int uGradientEnabled;
uniform vec4 uGradientTopLeft;
uniform vec4 uGradientTopRight;
uniform vec4 uGradientBottomLeft;
uniform vec4 uGradientBottomRight;
uniform float iTime;
uniform vec2 iResolution;
uniform sampler2D iChannel0;
out vec4 FragColor;

float roundedBoxSDF(vec2 centerPosition, vec2 size, float radius) {
    return length(max(abs(centerPosition) - size + radius, 0.0)) - radius;
}

vec3 draw(vec2 uv) {
    return texture(iChannel0, uv).rgb;
}

float rand(vec2 co) {
    return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

vec4 fillColorAt(vec2 uv) {
    if (uGradientEnabled == 0) {
        return uColor;
    }
    vec4 top = mix(uGradientTopLeft, uGradientTopRight, clamp(uv.x, 0.0, 1.0));
    vec4 bottom = mix(uGradientBottomLeft, uGradientBottomRight, clamp(uv.x, 0.0, 1.0));
    vec4 gradientColor = mix(top, bottom, clamp(uv.y, 0.0, 1.0));
    return vec4(gradientColor.rgb, gradientColor.a * uColor.a);
}

void main() {
    vec2 center = uBoxPos + uBoxSize * 0.5 + uTranslate;
    vec2 p = vLocal;
    float d = roundedBoxSDF(p, uBoxSize * 0.5, uRounding);

    float shadowAlpha = 0.0;
    if (uShadowBlur > 0.0) {
        vec2 shadowDelta = (vPos - center) - uShadowOffset;
        vec2 sp = uTransformInv * shadowDelta;
        float sd = roundedBoxSDF(sp, uBoxSize * 0.5, uRounding);
        shadowAlpha = 1.0 - smoothstep(-uShadowBlur, uShadowBlur, sd);
        shadowAlpha *= uShadowColor.a;
    }

    float alpha = 1.0 - smoothstep(-1.0, 1.0, d);
    vec4 fillColor = fillColorAt(vUV);
    vec4 finalColor = vec4(0.0);

    if (uBlurAmount > 0.0 && alpha > 0.0) {
        vec2 uv = gl_FragCoord.xy / iResolution.xy;
        float bluramount = uBlurAmount;
        vec3 blurredImage = vec3(0.0);
        const float repeats = 60.0;
        for (float i = 0.0; i < repeats; i += 1.0) {
            vec2 q = vec2(cos(degrees((i / repeats) * 360.0)), sin(degrees((i / repeats) * 360.0))) *
                     (rand(vec2(i, uv.x + uv.y)) + bluramount);
            vec2 uv2 = uv + (q * bluramount);
            blurredImage += draw(uv2) / 2.0;
            q = vec2(cos(degrees((i / repeats) * 360.0)), sin(degrees((i / repeats) * 360.0))) *
                (rand(vec2(i + 2.0, uv.x + uv.y + 24.0)) + bluramount);
            uv2 = uv + (q * bluramount);
            blurredImage += draw(uv2) / 2.0;
        }
        blurredImage /= repeats;
        vec3 mixColor = mix(blurredImage, fillColor.rgb, fillColor.a);
        finalColor = vec4(mixColor, fillColor.a * alpha);
    } else {
        finalColor = vec4(fillColor.rgb, fillColor.a * alpha);
    }

    if (shadowAlpha > 0.0 && alpha < 1.0) {
        vec3 outRgb = (finalColor.rgb * finalColor.a + uShadowColor.rgb * shadowAlpha * (1.0 - finalColor.a)) /
                      max(0.001, (finalColor.a + shadowAlpha * (1.0 - finalColor.a)));
        float outA = finalColor.a + shadowAlpha * (1.0 - finalColor.a);
        FragColor = vec4(outRgb, outA);
    } else {
        FragColor = finalColor;
    }
}
)";

static const char* cachedBlurVShaderStr = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
out vec2 vUV;
uniform mat4 projection;
uniform vec2 uPos;
uniform vec2 uSize;
void main() {
    vec2 pos = (aPos * uSize) + uPos;
    vUV = vec2(aPos.x, 1.0 - aPos.y);
    gl_Position = projection * vec4(pos, 0.0, 1.0);
}
)";

static const char* cachedBlurFShaderStr = R"(
#version 330 core
in vec2 vUV;
uniform sampler2D uTexture;
out vec4 FragColor;
void main() {
    FragColor = texture(uTexture, vUV);
}
)";

static GLuint CompileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    return shader;
}

static GLuint CreateProgram(const char* vertexSource, const char* fragmentSource) {
    GLuint vs = CompileShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fragmentSource);
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

static bool FloatEq(float a, float b, float epsilon = 0.0001f) {
    return std::abs(a - b) <= epsilon;
}

static bool ColorEq(const Color& a, const Color& b, float epsilon = 0.0001f) {
    return FloatEq(a.r, b.r, epsilon) &&
           FloatEq(a.g, b.g, epsilon) &&
           FloatEq(a.b, b.b, epsilon) &&
           FloatEq(a.a, b.a, epsilon);
}

static bool RectTransformEq(const RectTransform& a, const RectTransform& b, float epsilon = 0.0001f) {
    return FloatEq(a.translateX, b.translateX, epsilon) &&
           FloatEq(a.translateY, b.translateY, epsilon) &&
           FloatEq(a.scaleX, b.scaleX, epsilon) &&
           FloatEq(a.scaleY, b.scaleY, epsilon) &&
           FloatEq(a.rotationDegrees, b.rotationDegrees, epsilon);
}

static bool RectGradientEq(const RectGradient& a, const RectGradient& b, float epsilon = 0.0001f) {
    return a.enabled == b.enabled &&
           ColorEq(a.topLeft, b.topLeft, epsilon) &&
           ColorEq(a.topRight, b.topRight, epsilon) &&
           ColorEq(a.bottomLeft, b.bottomLeft, epsilon) &&
           ColorEq(a.bottomRight, b.bottomRight, epsilon);
}

static void BuildTransformInverse(const RectTransform& transform, float out[4]) {
    float scaleX = std::abs(transform.scaleX) < 0.0001f ? (transform.scaleX < 0.0f ? -0.0001f : 0.0001f)
                                                        : transform.scaleX;
    float scaleY = std::abs(transform.scaleY) < 0.0001f ? (transform.scaleY < 0.0f ? -0.0001f : 0.0001f)
                                                        : transform.scaleY;
    float rotation = transform.rotationDegrees * 0.017453292519943295f;
    float c = std::cos(rotation);
    float s = std::sin(rotation);

    out[0] = c / scaleX;
    out[1] = s / scaleX;
    out[2] = -s / scaleY;
    out[3] = c / scaleY;
}

static RectBounds ComputeRectBounds(float x, float y, float w, float h, const RectStyle& style) {
    float halfW = w * 0.5f;
    float halfH = h * 0.5f;
    float centerX = x + halfW + style.transform.translateX;
    float centerY = y + halfH + style.transform.translateY;
    float rotation = style.transform.rotationDegrees * 0.017453292519943295f;
    float c = std::cos(rotation);
    float s = std::sin(rotation);

    const float localCorners[4][2] = {
        {-halfW, -halfH},
        { halfW, -halfH},
        {-halfW,  halfH},
        { halfW,  halfH},
    };

    float minX = centerX;
    float minY = centerY;
    float maxX = centerX;
    float maxY = centerY;

    for (int i = 0; i < 4; ++i) {
        float lx = localCorners[i][0] * style.transform.scaleX;
        float ly = localCorners[i][1] * style.transform.scaleY;
        float rx = c * lx - s * ly;
        float ry = s * lx + c * ly;
        float px = centerX + rx;
        float py = centerY + ry;
        minX = std::min(minX, px);
        minY = std::min(minY, py);
        maxX = std::max(maxX, px);
        maxY = std::max(maxY, py);
    }

    RectBounds base;
    base.x = minX;
    base.y = minY;
    base.w = maxX - minX;
    base.h = maxY - minY;

    RectBounds shadow = base;
    shadow.x += style.shadowOffsetX;
    shadow.y += style.shadowOffsetY;

    float unionX1 = std::min(base.x, shadow.x);
    float unionY1 = std::min(base.y, shadow.y);
    float unionX2 = std::max(base.x + base.w, shadow.x + shadow.w);
    float unionY2 = std::max(base.y + base.h, shadow.y + shadow.h);

    float expand = style.shadowBlur * 2.0f;
    RectBounds out;
    out.x = unionX1 - expand;
    out.y = unionY1 - expand;
    out.w = (unionX2 - unionX1) + expand * 2.0f;
    out.h = (unionY2 - unionY1) + expand * 2.0f;
    return out;
}

static bool RectIntersectsActiveRedraw(float x, float y, float w, float h) {
    if (!ActiveRedrawClipEnabled) {
        return true;
    }
    return w > 0.0f && h > 0.0f &&
        x < ActiveRedrawX2 &&
        x + w > ActiveRedrawX1 &&
        y < ActiveRedrawY2 &&
        y + h > ActiveRedrawY1;
}

static bool RectFullyCoveredByActiveRedraw(float x, float y, float w, float h) {
    if (!ActiveRedrawClipEnabled) {
        return true;
    }
    return w > 0.0f && h > 0.0f &&
        x >= ActiveRedrawX1 &&
        y >= ActiveRedrawY1 &&
        x + w <= ActiveRedrawX2 &&
        y + h <= ActiveRedrawY2;
}

static void EnsureCachedBlurTexture(int width, int height) {
    width = std::max(width, 1);
    height = std::max(height, 1);
    if (CachedBlurTexture == 0) {
        glGenTextures(1, &CachedBlurTexture);
    }
    glBindTexture(GL_TEXTURE_2D, CachedBlurTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (CachedBlurTextureW != width || CachedBlurTextureH != height) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        CachedBlurTextureW = width;
        CachedBlurTextureH = height;
        BlurCacheValid = false;
    }
}

static bool CachedBlurMatches(float quadX, float quadY, float quadW, float quadH, const RectStyle& style) {
    return BlurCacheValid &&
        CachedBackdropVersion == BackdropVersion &&
        FloatEq(CachedBlurX, quadX) &&
        FloatEq(CachedBlurY, quadY) &&
        FloatEq(CachedBlurW, quadW) &&
        FloatEq(CachedBlurH, quadH) &&
        FloatEq(CachedBlurColor[0], style.color.r) &&
        FloatEq(CachedBlurColor[1], style.color.g) &&
        FloatEq(CachedBlurColor[2], style.color.b) &&
        FloatEq(CachedBlurColor[3], style.color.a) &&
        FloatEq(CachedBlurRounding, style.rounding) &&
        FloatEq(CachedBlurAmount, style.blurAmount) &&
        FloatEq(CachedBlurShadowBlur, style.shadowBlur) &&
        FloatEq(CachedBlurShadowOffsetX, style.shadowOffsetX) &&
        FloatEq(CachedBlurShadowOffsetY, style.shadowOffsetY) &&
        FloatEq(CachedBlurShadowColor[0], style.shadowColor.r) &&
        FloatEq(CachedBlurShadowColor[1], style.shadowColor.g) &&
        FloatEq(CachedBlurShadowColor[2], style.shadowColor.b) &&
        FloatEq(CachedBlurShadowColor[3], style.shadowColor.a) &&
        RectTransformEq(CachedBlurTransform, style.transform) &&
        RectGradientEq(CachedBlurGradient, style.gradient);
}

struct Character {
    GLuint TextureID;
    int Size[2];
    int Bearing[2];
    unsigned int Advance;
};

static std::unordered_map<unsigned int, Character> Characters;
static GLuint TextVAO = 0;
static GLuint TextVBO = 0;
static GLuint TextShaderProgram = 0;
static GLint TextProjLoc = -1;
static GLint TextColorLoc = -1;

static const char* textVShaderStr = R"(
#version 330 core
layout(location = 0) in vec4 vertex;
out vec2 TexCoords;
uniform mat4 projection;
void main() {
    gl_Position = projection * vec4(vertex.xy, 0.0, 1.0);
    TexCoords = vertex.zw;
}
)";

static const char* textFShaderStr = R"(
#version 330 core
in vec2 TexCoords;
out vec4 color;
uniform sampler2D text;
uniform vec4 textColor;
void main() {
    vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, TexCoords).r);
    color = textColor * sampled;
}
)";

void Renderer::Init() {
    GLuint vs = CompileShader(GL_VERTEX_SHADER, vShaderStr);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fShaderStr);
    ShaderProgram = glCreateProgram();
    glAttachShader(ShaderProgram, vs);
    glAttachShader(ShaderProgram, fs);
    glLinkProgram(ShaderProgram);
    glDeleteShader(vs);
    glDeleteShader(fs);
    CachedBlurProgram = CreateProgram(cachedBlurVShaderStr, cachedBlurFShaderStr);

    ProjLoc = glGetUniformLocation(ShaderProgram, "projection");
    ColorLoc = glGetUniformLocation(ShaderProgram, "uColor");
    PosLoc = glGetUniformLocation(ShaderProgram, "uPos");
    SizeLoc = glGetUniformLocation(ShaderProgram, "uSize");
    BoxPosLoc = glGetUniformLocation(ShaderProgram, "uBoxPos");
    BoxSizeLoc = glGetUniformLocation(ShaderProgram, "uBoxSize");
    TranslateLoc = glGetUniformLocation(ShaderProgram, "uTranslate");
    ScaleLoc = glGetUniformLocation(ShaderProgram, "uScale");
    RotationLoc = glGetUniformLocation(ShaderProgram, "uRotation");
    TransformInvLoc = glGetUniformLocation(ShaderProgram, "uTransformInv");
    RoundingLoc = glGetUniformLocation(ShaderProgram, "uRounding");
    BlurAmountLoc = glGetUniformLocation(ShaderProgram, "uBlurAmount");
    ShadowBlurLoc = glGetUniformLocation(ShaderProgram, "uShadowBlur");
    ShadowOffsetLoc = glGetUniformLocation(ShaderProgram, "uShadowOffset");
    ShadowColorLoc = glGetUniformLocation(ShaderProgram, "uShadowColor");
    GradientEnabledLoc = glGetUniformLocation(ShaderProgram, "uGradientEnabled");
    GradientTopLeftLoc = glGetUniformLocation(ShaderProgram, "uGradientTopLeft");
    GradientTopRightLoc = glGetUniformLocation(ShaderProgram, "uGradientTopRight");
    GradientBottomLeftLoc = glGetUniformLocation(ShaderProgram, "uGradientBottomLeft");
    GradientBottomRightLoc = glGetUniformLocation(ShaderProgram, "uGradientBottomRight");
    TimeLoc = glGetUniformLocation(ShaderProgram, "iTime");
    ResolutionLoc = glGetUniformLocation(ShaderProgram, "iResolution");
    Channel0Loc = glGetUniformLocation(ShaderProgram, "iChannel0");
    CachedBlurProjLoc = glGetUniformLocation(CachedBlurProgram, "projection");
    CachedBlurPosLoc = glGetUniformLocation(CachedBlurProgram, "uPos");
    CachedBlurSizeLoc = glGetUniformLocation(CachedBlurProgram, "uSize");
    CachedBlurTextureLoc = glGetUniformLocation(CachedBlurProgram, "uTexture");

    glGenTextures(1, &BgTexture);
    glBindTexture(GL_TEXTURE_2D, BgTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    float vertices[] = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f
    };
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    GLuint tvs = CompileShader(GL_VERTEX_SHADER, textVShaderStr);
    GLuint tfs = CompileShader(GL_FRAGMENT_SHADER, textFShaderStr);
    TextShaderProgram = glCreateProgram();
    glAttachShader(TextShaderProgram, tvs);
    glAttachShader(TextShaderProgram, tfs);
    glLinkProgram(TextShaderProgram);
    glDeleteShader(tvs);
    glDeleteShader(tfs);

    TextProjLoc = glGetUniformLocation(TextShaderProgram, "projection");
    TextColorLoc = glGetUniformLocation(TextShaderProgram, "textColor");

    glGenVertexArrays(1, &TextVAO);
    glGenBuffers(1, &TextVBO);
    glBindVertexArray(TextVAO);
    glBindBuffer(GL_ARRAY_BUFFER, TextVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void Renderer::Shutdown() {
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(ShaderProgram);
    glDeleteProgram(CachedBlurProgram);
    glDeleteTextures(1, &BgTexture);
    if (CachedBlurTexture) glDeleteTextures(1, &CachedBlurTexture);

    glDeleteVertexArrays(1, &TextVAO);
    glDeleteBuffers(1, &TextVBO);
    glDeleteProgram(TextShaderProgram);
}

static GLuint CurrentActiveProgram = 0;

void Renderer::BeginFrame() {
    float L = 0.0f;
    float R = State.screenW;
    float B = State.screenH;
    float T = 0.0f;
    float proj[16] = {
        2.0f / (R - L), 0, 0, 0,
        0, 2.0f / (T - B), 0, 0,
        0, 0, -1, 0,
        -(R + L) / (R - L), -(T + B) / (T - B), 0, 1
    };

    glUseProgram(ShaderProgram);
    glUniformMatrix4fv(ProjLoc, 1, GL_FALSE, proj);

    glUseProgram(CachedBlurProgram);
    glUniformMatrix4fv(CachedBlurProjLoc, 1, GL_FALSE, proj);

    glUseProgram(TextShaderProgram);
    glUniformMatrix4fv(TextProjLoc, 1, GL_FALSE, proj);

    CurrentActiveProgram = TextShaderProgram;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
}

void Renderer::SetFullRedraw() {
    ActiveRedrawClipEnabled = false;
    ActiveRedrawX1 = ActiveRedrawY1 = ActiveRedrawX2 = ActiveRedrawY2 = 0.0f;
}

void Renderer::SetPartialRedraw(float x1, float y1, float x2, float y2) {
    if (x2 <= x1 || y2 <= y1) {
        SetFullRedraw();
        return;
    }
    ActiveRedrawClipEnabled = true;
    ActiveRedrawX1 = x1;
    ActiveRedrawY1 = y1;
    ActiveRedrawX2 = x2;
    ActiveRedrawY2 = y2;
}

RectBounds Renderer::MeasureRectBounds(float x, float y, float w, float h, const RectStyle& style) {
    return ComputeRectBounds(x, y, w, h, style);
}

void Renderer::DrawRect(float x, float y, float w, float h, const RectStyle& style) {
    RectBounds bounds = ComputeRectBounds(x, y, w, h, style);
    if (!RectIntersectsActiveRedraw(bounds.x, bounds.y, bounds.w, bounds.h)) {
        return;
    }

    if (style.blurAmount > 0.0f && CachedBlurMatches(bounds.x, bounds.y, bounds.w, bounds.h, style)) {
        if (CurrentActiveProgram != CachedBlurProgram) {
            glUseProgram(CachedBlurProgram);
            CurrentActiveProgram = CachedBlurProgram;
        }
        glUniform2f(CachedBlurPosLoc, CachedBlurX, CachedBlurY);
        glUniform2f(CachedBlurSizeLoc, CachedBlurW, CachedBlurH);
        glUniform1i(CachedBlurTextureLoc, 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, CachedBlurTexture);
        glDisable(GL_BLEND);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        return;
    }

    if (CurrentActiveProgram != ShaderProgram) {
        glUseProgram(ShaderProgram);
        CurrentActiveProgram = ShaderProgram;
    }

    float transformInv[4] = {};
    BuildTransformInverse(style.transform, transformInv);

    glUniform2f(PosLoc, x, y);
    glUniform2f(SizeLoc, w, h);
    glUniform2f(BoxPosLoc, x, y);
    glUniform2f(BoxSizeLoc, w, h);
    glUniform2f(TranslateLoc, style.transform.translateX, style.transform.translateY);
    glUniform2f(ScaleLoc, style.transform.scaleX, style.transform.scaleY);
    glUniform1f(RotationLoc, style.transform.rotationDegrees);
    glUniformMatrix2fv(TransformInvLoc, 1, GL_FALSE, transformInv);
    glUniform4f(ColorLoc, style.color.r, style.color.g, style.color.b, style.color.a);
    glUniform1f(RoundingLoc, style.rounding);
    glUniform1f(BlurAmountLoc, style.blurAmount);
    glUniform1f(ShadowBlurLoc, style.shadowBlur);
    glUniform2f(ShadowOffsetLoc, style.shadowOffsetX, style.shadowOffsetY);
    glUniform4f(ShadowColorLoc, style.shadowColor.r, style.shadowColor.g, style.shadowColor.b, style.shadowColor.a);
    glUniform1i(GradientEnabledLoc, style.gradient.enabled ? 1 : 0);
    glUniform4f(GradientTopLeftLoc, style.gradient.topLeft.r, style.gradient.topLeft.g,
                style.gradient.topLeft.b, style.gradient.topLeft.a);
    glUniform4f(GradientTopRightLoc, style.gradient.topRight.r, style.gradient.topRight.g,
                style.gradient.topRight.b, style.gradient.topRight.a);
    glUniform4f(GradientBottomLeftLoc, style.gradient.bottomLeft.r, style.gradient.bottomLeft.g,
                style.gradient.bottomLeft.b, style.gradient.bottomLeft.a);
    glUniform4f(GradientBottomRightLoc, style.gradient.bottomRight.r, style.gradient.bottomRight.g,
                style.gradient.bottomRight.b, style.gradient.bottomRight.a);
    glUniform1f(TimeLoc, (float)glfwGetTime());
    glUniform2f(ResolutionLoc, State.screenW, State.screenH);

    static int lastBlurFrame = -1;
    if (style.blurAmount > 0.0f) {
        glBindTexture(GL_TEXTURE_2D, BgTexture);
        static int texW = 0;
        static int texH = 0;
        if (texW != (int)State.screenW || texH != (int)State.screenH) {
            texW = (int)State.screenW;
            texH = (int)State.screenH;
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, texW, texH, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
            lastBlurFrame = -1;
        }
        if (lastBlurFrame != State.frameCount) {
            glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, texW, texH);
            lastBlurFrame = State.frameCount;
        }
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, BgTexture);
        glUniform1i(Channel0Loc, 0);
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    if (style.blurAmount > 0.0f) {
        int copyX = std::clamp((int)std::floor(bounds.x), 0, (int)State.screenW);
        int copyYTop = std::clamp((int)std::floor(bounds.y), 0, (int)State.screenH);
        int copyRight = std::clamp((int)std::ceil(bounds.x + bounds.w), 0, (int)State.screenW);
        int copyBottom = std::clamp((int)std::ceil(bounds.y + bounds.h), 0, (int)State.screenH);
        int copyW = copyRight - copyX;
        int copyH = copyBottom - copyYTop;

        if (copyW > 0 && copyH > 0 &&
            RectFullyCoveredByActiveRedraw((float)copyX, (float)copyYTop, (float)copyW, (float)copyH)) {
            int copyY = std::max(0, (int)std::floor(State.screenH - (copyYTop + copyH)));

            EnsureCachedBlurTexture(copyW, copyH);
            glBindTexture(GL_TEXTURE_2D, CachedBlurTexture);
            glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, copyX, copyY, copyW, copyH);

            CachedBackdropVersion = BackdropVersion;
            CachedBlurX = (float)copyX;
            CachedBlurY = (float)copyYTop;
            CachedBlurW = (float)copyW;
            CachedBlurH = (float)copyH;
            CachedBlurColor[0] = style.color.r;
            CachedBlurColor[1] = style.color.g;
            CachedBlurColor[2] = style.color.b;
            CachedBlurColor[3] = style.color.a;
            CachedBlurShadowColor[0] = style.shadowColor.r;
            CachedBlurShadowColor[1] = style.shadowColor.g;
            CachedBlurShadowColor[2] = style.shadowColor.b;
            CachedBlurShadowColor[3] = style.shadowColor.a;
            CachedBlurRounding = style.rounding;
            CachedBlurAmount = style.blurAmount;
            CachedBlurShadowBlur = style.shadowBlur;
            CachedBlurShadowOffsetX = style.shadowOffsetX;
            CachedBlurShadowOffsetY = style.shadowOffsetY;
            CachedBlurTransform = style.transform;
            CachedBlurGradient = style.gradient;
            BlurCacheValid = true;
        }
    }
}

void Renderer::DrawRect(float x, float y, float w, float h, const Color& color, float rounding,
                        float blurAmount, float shadowBlur, float shadowOffsetX, float shadowOffsetY,
                        const Color& shadowColor) {
    RectStyle style;
    style.color = color;
    style.rounding = rounding;
    style.blurAmount = blurAmount;
    style.shadowBlur = shadowBlur;
    style.shadowOffsetX = shadowOffsetX;
    style.shadowOffsetY = shadowOffsetY;
    style.shadowColor = shadowColor;
    DrawRect(x, y, w, h, style);
}

bool Renderer::LoadFont(const std::string& fontPath, float fontSize, unsigned int startChar, unsigned int endChar) {
    std::ifstream file(fontPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<unsigned char> buffer(size);
    if (!file.read((char*)buffer.data(), size)) return false;

    stbtt_fontinfo font;
    if (!stbtt_InitFont(&font, buffer.data(), 0)) return false;

    float scale = stbtt_ScaleForPixelHeight(&font, fontSize);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    for (unsigned int c = startChar; c < endChar; ++c) {
        if (Characters.find(c) != Characters.end()) continue;
        if (stbtt_FindGlyphIndex(&font, c) == 0) continue;

        int width = 0;
        int height = 0;
        int xoff = 0;
        int yoff = 0;
        unsigned char* bitmap = stbtt_GetCodepointBitmap(&font, 0, scale, c, &width, &height, &xoff, &yoff);
        if (!bitmap) continue;

        int advance = 0;
        int lsb = 0;
        stbtt_GetCodepointHMetrics(&font, c, &advance, &lsb);

        GLuint texture = 0;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, bitmap);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        Characters[c] = {
            texture,
            {width, height},
            {xoff, yoff},
            (unsigned int)(advance * scale)
        };
        stbtt_FreeBitmap(bitmap, nullptr);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

void Renderer::DrawTextStr(const std::string& text, float x, float y, const Color& color, float scale) {
    if (text.empty()) {
        return;
    }

    float textWidth = std::max(MeasureTextWidth(text, scale), 24.0f * scale);
    float textHeight = 32.0f * scale;
    if (!RectIntersectsActiveRedraw(x, y - textHeight, textWidth, textHeight * 1.35f)) {
        return;
    }

    if (CurrentActiveProgram != TextShaderProgram) {
        glUseProgram(TextShaderProgram);
        CurrentActiveProgram = TextShaderProgram;
    }

    glUniform4f(TextColorLoc, color.r, color.g, color.b, color.a);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(TextVAO);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    size_t i = 0;
    while (i < text.length()) {
        unsigned int c = 0;
        unsigned char ch = text[i];
        if (ch <= 0x7F) { c = ch; i++; }
        else if ((ch & 0xE0) == 0xC0) { if (i + 1 < text.length()) { c = ((ch & 0x1F) << 6) | (text[i + 1] & 0x3F); i += 2; } else break; }
        else if ((ch & 0xF0) == 0xE0) { if (i + 2 < text.length()) { c = ((ch & 0x0F) << 12) | ((text[i + 1] & 0x3F) << 6) | (text[i + 2] & 0x3F); i += 3; } else break; }
        else if ((ch & 0xF8) == 0xF0) { if (i + 3 < text.length()) { c = ((ch & 0x07) << 18) | ((text[i + 1] & 0x3F) << 12) | ((text[i + 2] & 0x3F) << 6) | (text[i + 3] & 0x3F); i += 4; } else break; }
        else { i++; continue; }

        if (c == ' ') {
            x += 24.0f * 0.3f * scale;
            continue;
        }

        if (Characters.find(c) == Characters.end()) continue;
        Character charData = Characters[c];

        float xpos = x + charData.Bearing[0] * scale;
        float ypos = y + charData.Bearing[1] * scale;
        float w = charData.Size[0] * scale;
        float h = charData.Size[1] * scale;

        float vertices[6][4] = {
            { xpos,     ypos + h,   0.0f, 1.0f },
            { xpos,     ypos,       0.0f, 0.0f },
            { xpos + w, ypos,       1.0f, 0.0f },
            { xpos,     ypos + h,   0.0f, 1.0f },
            { xpos + w, ypos,       1.0f, 0.0f },
            { xpos + w, ypos + h,   1.0f, 1.0f }
        };

        glBindTexture(GL_TEXTURE_2D, charData.TextureID);
        glBindBuffer(GL_ARRAY_BUFFER, TextVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        x += charData.Advance * scale;
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

float Renderer::MeasureTextWidth(const std::string& text, float scale) {
    float width = 0.0f;
    size_t i = 0;
    while (i < text.length()) {
        unsigned int c = 0;
        unsigned char ch = text[i];
        if (ch <= 0x7F) { c = ch; i++; }
        else if ((ch & 0xE0) == 0xC0) { if (i + 1 < text.length()) { c = ((ch & 0x1F) << 6) | (text[i + 1] & 0x3F); i += 2; } else break; }
        else if ((ch & 0xF0) == 0xE0) { if (i + 2 < text.length()) { c = ((ch & 0x0F) << 12) | ((text[i + 1] & 0x3F) << 6) | (text[i + 2] & 0x3F); i += 3; } else break; }
        else if ((ch & 0xF8) == 0xF0) { if (i + 3 < text.length()) { c = ((ch & 0x07) << 18) | ((text[i + 1] & 0x3F) << 12) | ((text[i + 2] & 0x3F) << 6) | (text[i + 3] & 0x3F); i += 4; } else break; }
        else { i++; continue; }

        if (Characters.find(c) != Characters.end()) {
            width += Characters[c].Advance * scale;
        } else if (c == ' ') {
            width += 24.0f * 0.3f * scale;
        }
    }
    return width;
}

void Renderer::RequestRepaint(float duration) {
    State.needsRepaint = true;
    if (duration > State.animationTimeLeft) {
        State.animationTimeLeft = duration;
    }
}

void Renderer::InvalidateAll() {
    State.needsRepaint = true;
    State.fullScreenDirty = true;
}

void Renderer::InvalidateBackdrop() {
    ++BackdropVersion;
    BlurCacheValid = false;
    State.needsRepaint = true;
    State.fullScreenDirty = true;
}

void Renderer::AddDirtyRect(float x, float y, float w, float h) {
    if (State.fullScreenDirty) return;

    if (State.dirtyX1 == State.dirtyX2) {
        State.dirtyX1 = x;
        State.dirtyY1 = y;
        State.dirtyX2 = x + w;
        State.dirtyY2 = y + h;
    } else {
        State.dirtyX1 = std::min(State.dirtyX1, x);
        State.dirtyY1 = std::min(State.dirtyY1, y);
        State.dirtyX2 = std::max(State.dirtyX2, x + w);
        State.dirtyY2 = std::max(State.dirtyY2, y + h);
    }
}

void Renderer::ApplyScissor() {
    glDisable(GL_SCISSOR_TEST);
}

void Renderer::ClearDirtyRect() {
    State.fullScreenDirty = false;
    State.dirtyX1 = State.dirtyY1 = State.dirtyX2 = State.dirtyY2 = 0;
}

bool Renderer::ShouldRepaint() {
    if (State.needsRepaint) {
        State.needsRepaint = false;
        return true;
    }
    if (State.animationTimeLeft > 0) {
        State.animationTimeLeft -= State.deltaTime;
        return true;
    }
    return false;
}

void Widget::GetAbsoluteBounds(float& outX, float& outY) {
    outX = x;
    outY = y;
    switch (anchor) {
        case Anchor::TopCenter: outX += State.screenW / 2 - width / 2; break;
        case Anchor::TopRight: outX += State.screenW - width; break;
        case Anchor::CenterLeft: outY += State.screenH / 2 - height / 2; break;
        case Anchor::Center: outX += State.screenW / 2 - width / 2; outY += State.screenH / 2 - height / 2; break;
        case Anchor::CenterRight: outX += State.screenW - width; outY += State.screenH / 2 - height / 2; break;
        case Anchor::BottomLeft: outY += State.screenH - height; break;
        case Anchor::BottomCenter: outX += State.screenW / 2 - width / 2; outY += State.screenH - height; break;
        case Anchor::BottomRight: outX += State.screenW - width; outY += State.screenH - height; break;
        case Anchor::TopLeft: default: break;
    }
}

bool Widget::IsHovered() {
    float absX = 0.0f;
    float absY = 0.0f;
    GetAbsoluteBounds(absX, absY);
    return State.mouseX >= absX && State.mouseX <= absX + width &&
           State.mouseY >= absY && State.mouseY <= absY + height;
}

void Widget::MarkDirty(const RectStyle& style, float expand, float duration) {
    float absX = 0.0f;
    float absY = 0.0f;
    GetAbsoluteBounds(absX, absY);
    RectBounds bounds = Renderer::MeasureRectBounds(absX, absY, width, height, style);
    Renderer::AddDirtyRect(bounds.x - expand, bounds.y - expand, bounds.w + expand * 2.0f, bounds.h + expand * 2.0f);
    Renderer::RequestRepaint(duration);
}

void Widget::MarkDirty(const RectStyle& fromStyle, const RectStyle& toStyle, float expand, float duration) {
    float absX = 0.0f;
    float absY = 0.0f;
    GetAbsoluteBounds(absX, absY);

    RectBounds fromBounds = Renderer::MeasureRectBounds(absX, absY, width, height, fromStyle);
    RectBounds toBounds = Renderer::MeasureRectBounds(absX, absY, width, height, toStyle);

    float x1 = std::min(fromBounds.x, toBounds.x) - expand;
    float y1 = std::min(fromBounds.y, toBounds.y) - expand;
    float x2 = std::max(fromBounds.x + fromBounds.w, toBounds.x + toBounds.w) + expand;
    float y2 = std::max(fromBounds.y + fromBounds.h, toBounds.y + toBounds.h) + expand;

    Renderer::AddDirtyRect(x1, y1, x2 - x1, y2 - y1);
    Renderer::RequestRepaint(duration);
}

void Widget::MarkDirty(float expand, float duration) {
    float absX = 0.0f;
    float absY = 0.0f;
    GetAbsoluteBounds(absX, absY);
    Renderer::AddDirtyRect(absX - expand, absY - expand, width + expand * 2, height + expand * 2);
    Renderer::RequestRepaint(duration);
}

} // namespace EUINEO
