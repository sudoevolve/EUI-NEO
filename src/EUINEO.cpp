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

RectFrame Lerp(const RectFrame& a, const RectFrame& b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    RectFrame out;
    out.x = Lerp(a.x, b.x, t);
    out.y = Lerp(a.y, b.y, t);
    out.width = Lerp(a.width, b.width, t);
    out.height = Lerp(a.height, b.height, t);
    return out;
}

PanelState Lerp(const PanelState& a, const PanelState& b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    PanelState out;
    out.frame = Lerp(a.frame, b.frame, t);
    out.style = Lerp(a.style, b.style, t);
    out.borderWidth = Lerp(a.borderWidth, b.borderWidth, t);
    out.borderColor = Lerp(a.borderColor, b.borderColor, t);
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
static GLint CachedBlurBoxPosLoc = -1;
static GLint CachedBlurBoxSizeLoc = -1;
static GLint CachedBlurTranslateLoc = -1;
static GLint CachedBlurTransformInvLoc = -1;
static GLint CachedBlurRoundingLoc = -1;
static GLint CachedBlurShadowBlurLoc = -1;
static GLint CachedBlurShadowOffsetLoc = -1;
static GLint CachedBlurShadowAlphaLoc = -1;
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
static int BgTextureW = 0;
static int BgTextureH = 0;
static bool BackdropCapturePending = true;

struct LayerCache {
    GLuint texture = 0;
    GLuint framebuffer = 0;
    int width = 0;
    int height = 0;
    bool dirty = true;
    bool ready = false;
    RectFrame bounds;
};

struct CachedSurface {
    GLuint texture = 0;
    GLuint framebuffer = 0;
    int width = 0;
    int height = 0;
    RectFrame bounds;
    bool ready = false;
};

static LayerCache LayerCaches[static_cast<int>(RenderLayer::Count)];
static std::unordered_map<std::string, CachedSurface> CachedSurfaces;
static GLuint CompositeProgram = 0;
static GLuint CompositeVAO = 0;
static GLuint CompositeVBO = 0;
static GLint CompositeProjLoc = -1;
static GLint CompositePosLoc = -1;
static GLint CompositeSizeLoc = -1;
static GLint CompositeTextureLoc = -1;
static GLint CompositeUVPosLoc = -1;
static GLint CompositeUVSizeLoc = -1;
static int ActiveLayerIndex = -1;
static bool ActiveCustomSurface = false;
static RectFrame ActiveCustomSurfaceBounds;
static int ActiveCustomSurfaceWidth = 0;
static int ActiveCustomSurfaceHeight = 0;

static const char* vShaderStr = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
out vec2 vPos;
uniform mat4 projection;
uniform vec2 uPos;
uniform vec2 uSize;
void main() {
    vec2 pos = uPos + aPos * uSize;
    vPos = pos;
    gl_Position = projection * vec4(pos, 0.0, 1.0);
}
)";

static const char* fShaderStr = R"(
#version 330 core
in vec2 vPos;
uniform vec4 uColor;
uniform vec2 uBoxPos;
uniform vec2 uBoxSize;
uniform vec2 uTranslate;
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
    vec2 p = uTransformInv * (vPos - center);
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
    vec2 safeBoxSize = max(uBoxSize, vec2(0.001));
    vec2 fillUV = clamp((p / safeBoxSize) + vec2(0.5), 0.0, 1.0);
    vec4 fillColor = fillColorAt(fillUV);
    vec4 finalColor = vec4(0.0);

    if (uBlurAmount > 0.0 && alpha > 0.0) {
        vec2 uv = gl_FragCoord.xy / iResolution.xy;
        float bluramount = uBlurAmount;
        vec2 pixelStep = 1.0 / iResolution.xy;
        float blurRadiusPx = bluramount * min(iResolution.x, iResolution.y);
        vec3 blurredImage = draw(uv);
        float repeats = mix(10.0, 28.0, clamp(bluramount / 0.15, 0.0, 1.0));
        const float tau = 6.28318530718;
        for (float i = 0.0; i < repeats; i += 1.0) {
            float angle = (i / repeats) * tau;
            vec2 dir = vec2(cos(angle), sin(angle));

            float radiusA = blurRadiusPx * (0.35 + 0.65 * rand(vec2(i, uv.x + uv.y)));
            vec2 uv2 = clamp(uv + dir * radiusA * pixelStep, pixelStep * 0.5, vec2(1.0) - pixelStep * 0.5);
            blurredImage += draw(uv2);

            float angleB = angle + (0.5 * tau / repeats);
            vec2 dirB = vec2(cos(angleB), sin(angleB));
            float radiusB = blurRadiusPx * (0.20 + 0.80 * rand(vec2(i + 2.0, uv.x + uv.y + 24.0)));
            uv2 = clamp(uv + dirB * radiusB * pixelStep, pixelStep * 0.5, vec2(1.0) - pixelStep * 0.5);
            blurredImage += draw(uv2);
        }
        blurredImage /= (repeats * 2.0 + 1.0);
        vec3 mixColor = mix(blurredImage, fillColor.rgb, fillColor.a);
        finalColor = vec4(mixColor, alpha);
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
out vec2 vPos;
uniform mat4 projection;
uniform vec2 uPos;
uniform vec2 uSize;
void main() {
    vec2 pos = (aPos * uSize) + uPos;
    vUV = vec2(aPos.x, 1.0 - aPos.y);
    vPos = pos;
    gl_Position = projection * vec4(pos, 0.0, 1.0);
}
)";

static const char* cachedBlurFShaderStr = R"(
#version 330 core
in vec2 vUV;
in vec2 vPos;
uniform sampler2D uTexture;
uniform vec2 uBoxPos;
uniform vec2 uBoxSize;
uniform vec2 uTranslate;
uniform mat2 uTransformInv;
uniform float uRounding;
uniform float uShadowBlur;
uniform vec2 uShadowOffset;
uniform float uShadowAlpha;
out vec4 FragColor;

float roundedBoxSDF(vec2 centerPosition, vec2 size, float radius) {
    return length(max(abs(centerPosition) - size + radius, 0.0)) - radius;
}

void main() {
    vec2 center = uBoxPos + uBoxSize * 0.5 + uTranslate;
    vec2 p = uTransformInv * (vPos - center);
    float d = roundedBoxSDF(p, uBoxSize * 0.5, uRounding);
    float alpha = 1.0 - smoothstep(-1.0, 1.0, d);

    float shadowAlpha = 0.0;
    if (uShadowBlur > 0.0 && uShadowAlpha > 0.0) {
        vec2 shadowDelta = (vPos - center) - uShadowOffset;
        vec2 sp = uTransformInv * shadowDelta;
        float sd = roundedBoxSDF(sp, uBoxSize * 0.5, uRounding);
        shadowAlpha = (1.0 - smoothstep(-uShadowBlur, uShadowBlur, sd)) * uShadowAlpha;
    }

    if (alpha <= 0.0 && shadowAlpha <= 0.0) {
        discard;
    }

    FragColor = texture(uTexture, vUV);
}
)";

static const char* compositeVShaderStr = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
out vec2 vUV;
uniform mat4 projection;
uniform vec2 uPos;
uniform vec2 uSize;
uniform vec2 uUVPos;
uniform vec2 uUVSize;
void main() {
    vec2 pos = uPos + aPos * uSize;
    vUV = uUVPos + aUV * uUVSize;
    gl_Position = projection * vec4(pos, 0.0, 1.0);
}
)";

static const char* compositeFShaderStr = R"(
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

static bool CanReuseBlurCache(const RectStyle& style) {
    (void)style;
    return false;
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

static void EnsureBackdropTexture(int width, int height) {
    width = std::max(width, 1);
    height = std::max(height, 1);
    glBindTexture(GL_TEXTURE_2D, BgTexture);
    if (BgTextureW != width || BgTextureH != height) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        BgTextureW = width;
        BgTextureH = height;
    }
}

static int LayerIndex(RenderLayer layer) {
    return static_cast<int>(layer);
}

static RenderLayer LayerFromIndex(int index) {
    return static_cast<RenderLayer>(index);
}

static bool UsesTightLayerSurface(RenderLayer layer) {
    return layer != RenderLayer::Backdrop;
}

static void MarkBackdropInputsDirty() {
    ++BackdropVersion;
    BlurCacheValid = false;
    BackdropCapturePending = true;
}

static LayerCache& CacheFor(RenderLayer layer) {
    return LayerCaches[LayerIndex(layer)];
}

static int TargetLayerWidth(RenderLayer layer, const RectFrame& bounds) {
    if (UsesTightLayerSurface(layer)) {
        return std::max(1, static_cast<int>(std::ceil(std::max(0.0f, bounds.width))));
    }
    return std::max(1, static_cast<int>(State.screenW));
}

static int TargetLayerHeight(RenderLayer layer, const RectFrame& bounds) {
    if (UsesTightLayerSurface(layer)) {
        return std::max(1, static_cast<int>(std::ceil(std::max(0.0f, bounds.height))));
    }
    return std::max(1, static_cast<int>(State.screenH));
}

static void EnsureLayerCacheStorage(RenderLayer layer, int width, int height) {
    LayerCache& cache = CacheFor(layer);
    width = std::max(width, 1);
    height = std::max(height, 1);

    if (cache.texture == 0) {
        glGenTextures(1, &cache.texture);
        glBindTexture(GL_TEXTURE_2D, cache.texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    } else {
        glBindTexture(GL_TEXTURE_2D, cache.texture);
    }

    if (cache.width != width || cache.height != height) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        cache.width = width;
        cache.height = height;
        cache.dirty = true;
        cache.ready = false;
        if (layer == RenderLayer::Backdrop) {
            MarkBackdropInputsDirty();
        }
    }

    if (cache.framebuffer == 0) {
        glGenFramebuffers(1, &cache.framebuffer);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, cache.framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, cache.texture, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

static void EnsureCachedSurfaceStorage(CachedSurface& cache, int width, int height) {
    width = std::max(width, 1);
    height = std::max(height, 1);

    if (cache.texture == 0) {
        glGenTextures(1, &cache.texture);
        glBindTexture(GL_TEXTURE_2D, cache.texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    } else {
        glBindTexture(GL_TEXTURE_2D, cache.texture);
    }

    if (cache.width != width || cache.height != height) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        cache.width = width;
        cache.height = height;
        cache.ready = false;
    }

    if (cache.framebuffer == 0) {
        glGenFramebuffers(1, &cache.framebuffer);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, cache.framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, cache.texture, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

static void CompositeTexture(const GLuint texture, const RectFrame& bounds,
                             float uvX, float uvY, float uvW, float uvH) {
    if (texture == 0 || bounds.width <= 0.0f || bounds.height <= 0.0f) {
        return;
    }

    const float L = 0.0f;
    const float R = State.screenW;
    const float B = State.screenH;
    const float T = 0.0f;
    const float proj[16] = {
        2.0f / (R - L), 0, 0, 0,
        0, 2.0f / (T - B), 0, 0,
        0, 0, -1, 0,
        -(R + L) / (R - L), -(T + B) / (T - B), 0, 1
    };

    glUseProgram(CompositeProgram);
    glUniformMatrix4fv(CompositeProjLoc, 1, GL_FALSE, proj);
    glUniform1i(CompositeTextureLoc, 0);
    glUniform2f(CompositePosLoc, bounds.x, bounds.y);
    glUniform2f(CompositeSizeLoc, bounds.width, bounds.height);
    glUniform2f(CompositeUVPosLoc, uvX, uvY);
    glUniform2f(CompositeUVSizeLoc, uvW, uvH);
    glBindVertexArray(CompositeVAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);
}

static bool MakeScreenScissorRect(const RectFrame& bounds, GLint& outX, GLint& outY, GLint& outW, GLint& outH) {
    if (bounds.width <= 0.0f || bounds.height <= 0.0f) {
        return false;
    }

    const float x1 = std::clamp(bounds.x, 0.0f, State.screenW);
    const float y1 = std::clamp(bounds.y, 0.0f, State.screenH);
    const float x2 = std::clamp(bounds.x + bounds.width, x1, State.screenW);
    const float y2 = std::clamp(bounds.y + bounds.height, y1, State.screenH);
    if (x2 <= x1 || y2 <= y1) {
        return false;
    }

    outX = static_cast<GLint>(std::floor(x1));
    outY = static_cast<GLint>(std::floor(State.screenH - y2));
    outW = std::max<GLint>(1, static_cast<GLint>(std::ceil(x2) - std::floor(x1)));
    outH = std::max<GLint>(1, static_cast<GLint>(std::ceil(y2) - std::floor(y1)));
    return true;
}

static bool MakeLocalScissorRect(const RectFrame& surfaceBounds, int surfaceHeight,
                                 const RectFrame& bounds, GLint& outX, GLint& outY,
                                 GLint& outW, GLint& outH) {
    if (bounds.width <= 0.0f || bounds.height <= 0.0f ||
        surfaceBounds.width <= 0.0f || surfaceBounds.height <= 0.0f ||
        surfaceHeight <= 0) {
        return false;
    }

    const float x1 = std::clamp(bounds.x - surfaceBounds.x, 0.0f, surfaceBounds.width);
    const float y1 = std::clamp(bounds.y - surfaceBounds.y, 0.0f, surfaceBounds.height);
    const float x2 = std::clamp(bounds.x + bounds.width - surfaceBounds.x, x1, surfaceBounds.width);
    const float y2 = std::clamp(bounds.y + bounds.height - surfaceBounds.y, y1, surfaceBounds.height);
    if (x2 <= x1 || y2 <= y1) {
        return false;
    }

    outX = static_cast<GLint>(std::floor(x1));
    outY = static_cast<GLint>(std::floor(static_cast<float>(surfaceHeight) - y2));
    outW = std::max<GLint>(1, static_cast<GLint>(std::ceil(x2) - std::floor(x1)));
    outH = std::max<GLint>(1, static_cast<GLint>(std::ceil(y2) - std::floor(y1)));
    return true;
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

struct TextWidthCacheKey {
    std::string text;
    int scaleKey = 0;

    bool operator==(const TextWidthCacheKey& other) const {
        return scaleKey == other.scaleKey && text == other.text;
    }
};

struct TextWidthCacheKeyHash {
    std::size_t operator()(const TextWidthCacheKey& key) const {
        return std::hash<std::string>{}(key.text) ^ (static_cast<std::size_t>(key.scaleKey) << 1);
    }
};

static std::unordered_map<unsigned int, Character> Characters;
static std::unordered_map<TextWidthCacheKey, float, TextWidthCacheKeyHash> TextWidthCache;
static GLuint TextVAO = 0;
static GLuint TextVBO = 0;
static GLuint TextShaderProgram = 0;
static GLint TextProjLoc = -1;
static GLint TextColorLoc = -1;

static int MakeTextScaleKey(float scale) {
    return static_cast<int>(std::lround(scale * 1024.0f));
}

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
    CompositeProgram = CreateProgram(compositeVShaderStr, compositeFShaderStr);

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
    CachedBlurBoxPosLoc = glGetUniformLocation(CachedBlurProgram, "uBoxPos");
    CachedBlurBoxSizeLoc = glGetUniformLocation(CachedBlurProgram, "uBoxSize");
    CachedBlurTranslateLoc = glGetUniformLocation(CachedBlurProgram, "uTranslate");
    CachedBlurTransformInvLoc = glGetUniformLocation(CachedBlurProgram, "uTransformInv");
    CachedBlurRoundingLoc = glGetUniformLocation(CachedBlurProgram, "uRounding");
    CachedBlurShadowBlurLoc = glGetUniformLocation(CachedBlurProgram, "uShadowBlur");
    CachedBlurShadowOffsetLoc = glGetUniformLocation(CachedBlurProgram, "uShadowOffset");
    CachedBlurShadowAlphaLoc = glGetUniformLocation(CachedBlurProgram, "uShadowAlpha");
    CompositeProjLoc = glGetUniformLocation(CompositeProgram, "projection");
    CompositePosLoc = glGetUniformLocation(CompositeProgram, "uPos");
    CompositeSizeLoc = glGetUniformLocation(CompositeProgram, "uSize");
    CompositeTextureLoc = glGetUniformLocation(CompositeProgram, "uTexture");
    CompositeUVPosLoc = glGetUniformLocation(CompositeProgram, "uUVPos");
    CompositeUVSizeLoc = glGetUniformLocation(CompositeProgram, "uUVSize");

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

    const float compositeVertices[] = {
        0.0f, 0.0f, 0.0f, 1.0f,
        1.0f, 0.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
        1.0f, 1.0f, 1.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
    };
    glGenVertexArrays(1, &CompositeVAO);
    glGenBuffers(1, &CompositeVBO);
    glBindVertexArray(CompositeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, CompositeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(compositeVertices), compositeVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void Renderer::Shutdown() {
    for (LayerCache& cache : LayerCaches) {
        if (cache.framebuffer) glDeleteFramebuffers(1, &cache.framebuffer);
        if (cache.texture) glDeleteTextures(1, &cache.texture);
    }
    for (auto& entry : CachedSurfaces) {
        if (entry.second.framebuffer) glDeleteFramebuffers(1, &entry.second.framebuffer);
        if (entry.second.texture) glDeleteTextures(1, &entry.second.texture);
    }
    CachedSurfaces.clear();

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(ShaderProgram);
    glDeleteProgram(CachedBlurProgram);
    glDeleteProgram(CompositeProgram);
    glDeleteVertexArrays(1, &CompositeVAO);
    glDeleteBuffers(1, &CompositeVBO);
    glDeleteTextures(1, &BgTexture);
    if (CachedBlurTexture) glDeleteTextures(1, &CachedBlurTexture);

    glDeleteVertexArrays(1, &TextVAO);
    glDeleteBuffers(1, &TextVBO);
    glDeleteProgram(TextShaderProgram);
}

static GLuint CurrentActiveProgram = 0;

bool Renderer::MakeCurrentScissorRect(const RectFrame& bounds, GLint& outX, GLint& outY, GLint& outW, GLint& outH) {
    if (ActiveCustomSurface) {
        return MakeLocalScissorRect(ActiveCustomSurfaceBounds, ActiveCustomSurfaceHeight, bounds,
                                    outX, outY, outW, outH);
    }
    if (ActiveLayerIndex >= 0) {
        const RenderLayer layer = LayerFromIndex(ActiveLayerIndex);
        const LayerCache& cache = LayerCaches[ActiveLayerIndex];
        if (UsesTightLayerSurface(layer)) {
            return MakeLocalScissorRect(cache.bounds, cache.height, bounds, outX, outY, outW, outH);
        }
    }
    return MakeScreenScissorRect(bounds, outX, outY, outW, outH);
}

void Renderer::SetLayerBounds(RenderLayer layer, const RectFrame& bounds) {
    LayerCache& cache = CacheFor(layer);
    if (!FloatEq(cache.bounds.x, bounds.x) ||
        !FloatEq(cache.bounds.y, bounds.y) ||
        !FloatEq(cache.bounds.width, bounds.width) ||
        !FloatEq(cache.bounds.height, bounds.height)) {
        cache.bounds = bounds;
        cache.dirty = true;
        if (layer == RenderLayer::Backdrop) {
            MarkBackdropInputsDirty();
        }
        State.needsRepaint = true;
    }
}

bool Renderer::NeedsLayerRedraw(RenderLayer layer) {
    const LayerCache& cache = CacheFor(layer);
    const int targetW = TargetLayerWidth(layer, cache.bounds);
    const int targetH = TargetLayerHeight(layer, cache.bounds);
    return cache.dirty || !cache.ready || cache.width != targetW || cache.height != targetH;
}

void Renderer::BeginLayer(RenderLayer layer) {
    LayerCache& cache = CacheFor(layer);
    const int targetW = TargetLayerWidth(layer, cache.bounds);
    const int targetH = TargetLayerHeight(layer, cache.bounds);
    EnsureLayerCacheStorage(layer, targetW, targetH);

    glBindFramebuffer(GL_FRAMEBUFFER, cache.framebuffer);
    glViewport(0, 0, targetW, targetH);
    const Color clear = layer == RenderLayer::Backdrop
        ? CurrentTheme->background
        : Color(0.0f, 0.0f, 0.0f, 0.0f);
    if (UsesTightLayerSurface(layer)) {
        glDisable(GL_SCISSOR_TEST);
        glClearColor(clear.r, clear.g, clear.b, clear.a);
        glClear(GL_COLOR_BUFFER_BIT);
    } else {
        GLint scissorX = 0;
        GLint scissorY = 0;
        GLint scissorW = targetW;
        GLint scissorH = targetH;
        const bool hasScissor = MakeScreenScissorRect(cache.bounds, scissorX, scissorY, scissorW, scissorH);
        if (hasScissor) {
            glEnable(GL_SCISSOR_TEST);
            glScissor(scissorX, scissorY, scissorW, scissorH);
        } else {
            glDisable(GL_SCISSOR_TEST);
        }
        glClearColor(clear.r, clear.g, clear.b, clear.a);
        glClear(GL_COLOR_BUFFER_BIT);
        glDisable(GL_SCISSOR_TEST);
    }

    CurrentActiveProgram = 0;
    ActiveLayerIndex = LayerIndex(layer);
}

void Renderer::EndLayer() {
    if (ActiveLayerIndex < 0) {
        return;
    }

    LayerCache& cache = LayerCaches[ActiveLayerIndex];
    cache.dirty = false;
    cache.ready = true;
    ActiveLayerIndex = -1;
    CurrentActiveProgram = 0;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, std::max(1, (int)State.screenW), std::max(1, (int)State.screenH));
}

void Renderer::CompositeLayers(const Color& background) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, std::max(1, (int)State.screenW), std::max(1, (int)State.screenH));
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glClearColor(background.r, background.g, background.b, background.a);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    const LayerCache& backdrop = CacheFor(RenderLayer::Backdrop);
    if (backdrop.ready && backdrop.texture != 0 && backdrop.bounds.width > 0.0f && backdrop.bounds.height > 0.0f) {
        const float uvX = backdrop.bounds.x / std::max(1.0f, State.screenW);
        const float uvBottom = 1.0f - ((backdrop.bounds.y + backdrop.bounds.height) / std::max(1.0f, State.screenH));
        const float uvW = backdrop.bounds.width / std::max(1.0f, State.screenW);
        const float uvH = backdrop.bounds.height / std::max(1.0f, State.screenH);
        CompositeTexture(backdrop.texture, backdrop.bounds, uvX, uvBottom, uvW, uvH);
    }
    CurrentActiveProgram = 0;
}

void Renderer::DrawCachedSurface(const std::string& key, const RectFrame& bounds, bool dirty,
                                 const std::function<void()>& painter) {
    if (bounds.width <= 0.0f || bounds.height <= 0.0f || !painter) {
        return;
    }

    CachedSurface& cache = CachedSurfaces[key];
    const int targetW = std::max(1, static_cast<int>(std::ceil(bounds.width)));
    const int targetH = std::max(1, static_cast<int>(std::ceil(bounds.height)));
    const bool boundsChanged =
        !FloatEq(cache.bounds.x, bounds.x) ||
        !FloatEq(cache.bounds.y, bounds.y) ||
        !FloatEq(cache.bounds.width, bounds.width) ||
        !FloatEq(cache.bounds.height, bounds.height);

    EnsureCachedSurfaceStorage(cache, targetW, targetH);
    if (dirty || !cache.ready || boundsChanged) {
        cache.bounds = bounds;
        glBindFramebuffer(GL_FRAMEBUFFER, cache.framebuffer);
        glViewport(0, 0, targetW, targetH);
        glDisable(GL_SCISSOR_TEST);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ActiveCustomSurface = true;
        ActiveCustomSurfaceBounds = bounds;
        ActiveCustomSurfaceWidth = targetW;
        ActiveCustomSurfaceHeight = targetH;
        CurrentActiveProgram = 0;
        BeginFrame();
        painter();
        ActiveCustomSurface = false;
        ActiveCustomSurfaceBounds = RectFrame{};
        ActiveCustomSurfaceWidth = 0;
        ActiveCustomSurfaceHeight = 0;

        cache.ready = true;
        CurrentActiveProgram = 0;
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, std::max(1, (int)State.screenW), std::max(1, (int)State.screenH));
    }

    CompositeTexture(cache.texture, cache.bounds, 0.0f, 0.0f, 1.0f, 1.0f);
    CurrentActiveProgram = 0;
}

void Renderer::ReleaseCachedSurface(const std::string& key) {
    auto it = CachedSurfaces.find(key);
    if (it == CachedSurfaces.end()) {
        return;
    }
    if (it->second.framebuffer) glDeleteFramebuffers(1, &it->second.framebuffer);
    if (it->second.texture) glDeleteTextures(1, &it->second.texture);
    CachedSurfaces.erase(it);
}

void Renderer::InvalidateLayer(RenderLayer layer) {
    LayerCache& cache = CacheFor(layer);
    cache.dirty = true;
    if (layer == RenderLayer::Backdrop) {
        MarkBackdropInputsDirty();
    }
    State.needsRepaint = true;
}

void Renderer::BeginFrame() {
    float L = 0.0f;
    float R = State.screenW;
    float B = State.screenH;
    float T = 0.0f;
    if (ActiveCustomSurface) {
        if (ActiveCustomSurfaceBounds.width > 0.0f && ActiveCustomSurfaceBounds.height > 0.0f) {
            L = ActiveCustomSurfaceBounds.x;
            R = ActiveCustomSurfaceBounds.x + ActiveCustomSurfaceBounds.width;
            T = ActiveCustomSurfaceBounds.y;
            B = ActiveCustomSurfaceBounds.y + ActiveCustomSurfaceBounds.height;
        }
    } else if (ActiveLayerIndex >= 0) {
        const RenderLayer layer = LayerFromIndex(ActiveLayerIndex);
        if (UsesTightLayerSurface(layer)) {
            const RectFrame& bounds = LayerCaches[ActiveLayerIndex].bounds;
            if (bounds.width > 0.0f && bounds.height > 0.0f) {
                L = bounds.x;
                R = bounds.x + bounds.width;
                T = bounds.y;
                B = bounds.y + bounds.height;
            }
        }
    }
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

RectBounds Renderer::MeasureRectBounds(float x, float y, float w, float h, const RectStyle& style) {
    return ComputeRectBounds(x, y, w, h, style);
}

void Renderer::DrawRect(float x, float y, float w, float h, const RectStyle& style) {
    RectBounds bounds = ComputeRectBounds(x, y, w, h, style);
    const bool canReuseBlurCache = CanReuseBlurCache(style);
    if (canReuseBlurCache && CachedBlurMatches(bounds.x, bounds.y, bounds.w, bounds.h, style)) {
        if (CurrentActiveProgram != CachedBlurProgram) {
            glUseProgram(CachedBlurProgram);
            CurrentActiveProgram = CachedBlurProgram;
        }
        float transformInv[4] = {};
        BuildTransformInverse(style.transform, transformInv);
        glUniform2f(CachedBlurPosLoc, CachedBlurX, CachedBlurY);
        glUniform2f(CachedBlurSizeLoc, CachedBlurW, CachedBlurH);
        glUniform1i(CachedBlurTextureLoc, 0);
        glUniform2f(CachedBlurBoxPosLoc, x, y);
        glUniform2f(CachedBlurBoxSizeLoc, w, h);
        glUniform2f(CachedBlurTranslateLoc, style.transform.translateX, style.transform.translateY);
        glUniformMatrix2fv(CachedBlurTransformInvLoc, 1, GL_FALSE, transformInv);
        glUniform1f(CachedBlurRoundingLoc, style.rounding);
        glUniform1f(CachedBlurShadowBlurLoc, style.shadowBlur);
        glUniform2f(CachedBlurShadowOffsetLoc, style.shadowOffsetX, style.shadowOffsetY);
        glUniform1f(CachedBlurShadowAlphaLoc, style.shadowColor.a);
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

    glUniform2f(PosLoc, bounds.x, bounds.y);
    glUniform2f(SizeLoc, bounds.w, bounds.h);
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
    glUniform4f(ShadowColorLoc, style.shadowColor.r, style.shadowColor.g,
                style.shadowColor.b, style.shadowColor.a);
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

    if (style.blurAmount > 0.0f) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, BgTexture);
        glUniform1i(Channel0Loc, 0);
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    if (canReuseBlurCache) {
        int copyX = std::clamp((int)std::floor(bounds.x), 0, (int)State.screenW);
        int copyYTop = std::clamp((int)std::floor(bounds.y), 0, (int)State.screenH);
        int copyRight = std::clamp((int)std::ceil(bounds.x + bounds.w), 0, (int)State.screenW);
        int copyBottom = std::clamp((int)std::ceil(bounds.y + bounds.h), 0, (int)State.screenH);
        int copyW = copyRight - copyX;
        int copyH = copyBottom - copyYTop;

        if (copyW > 0 && copyH > 0) {
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
    TextWidthCache.clear();
    return true;
}

void Renderer::DrawTextStr(const std::string& text, float x, float y, const Color& color, float scale,
                           float rotationDegrees, float pivotX, float pivotY, bool useCustomPivot) {
    if (text.empty()) {
        return;
    }

    float finalPivotX = pivotX;
    float finalPivotY = pivotY;
    const bool needsRotation = std::abs(rotationDegrees) > 0.001f;
    if (needsRotation || useCustomPivot) {
        if (!useCustomPivot) {
            const float textWidth = std::max(MeasureTextWidth(text, scale), 24.0f * scale);
            const float textHeight = 32.0f * scale;
            const float boundsX = x;
            const float boundsY = y - textHeight;
            const float boundsW = textWidth;
            const float boundsH = textHeight * 1.35f;
            finalPivotX = boundsX + boundsW * 0.5f;
            finalPivotY = boundsY + boundsH * 0.5f;
        }
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

        if (needsRotation) {
            const float radians = rotationDegrees * 0.017453292519943295f;
            const float c = std::cos(radians);
            const float s = std::sin(radians);
            for (int vertexIndex = 0; vertexIndex < 6; ++vertexIndex) {
                const float dx = vertices[vertexIndex][0] - finalPivotX;
                const float dy = vertices[vertexIndex][1] - finalPivotY;
                vertices[vertexIndex][0] = finalPivotX + dx * c - dy * s;
                vertices[vertexIndex][1] = finalPivotY + dx * s + dy * c;
            }
        }

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
    if (text.empty()) {
        return 0.0f;
    }

    const TextWidthCacheKey cacheKey{text, MakeTextScaleKey(scale)};
    auto cacheIt = TextWidthCache.find(cacheKey);
    if (cacheIt != TextWidthCache.end()) {
        return cacheIt->second;
    }

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

    if (TextWidthCache.size() > 4096) {
        TextWidthCache.clear();
    }
    TextWidthCache.emplace(cacheKey, width);
    return width;
}

void Renderer::RequestRepaint(float duration) {
    State.needsRepaint = true;
    if (duration > State.animationTimeLeft) {
        State.animationTimeLeft = duration;
    }
}

void Renderer::InvalidateAll() {
    for (LayerCache& cache : LayerCaches) {
        cache.dirty = true;
    }
    MarkBackdropInputsDirty();
    State.needsRepaint = true;
}

void Renderer::InvalidateBackdrop() {
    InvalidateLayer(RenderLayer::Backdrop);
}

void Renderer::CaptureBackdrop() {
    if (!BackdropCapturePending) {
        return;
    }

    const int texW = std::max(1, (int)State.screenW);
    const int texH = std::max(1, (int)State.screenH);
    EnsureBackdropTexture(texW, texH);
    glBindTexture(GL_TEXTURE_2D, BgTexture);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, texW, texH);
    BackdropCapturePending = false;
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

} // namespace EUINEO
