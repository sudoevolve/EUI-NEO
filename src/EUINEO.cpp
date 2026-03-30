#include "EUINEO.h"
#include "components/Image.h"
#include "ui/ThemeTokens.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <mutex>
#include <atomic>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <vector>
#ifdef EUI_HAS_CURL
#include <curl/curl.h>
#elif defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <urlmon.h>
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

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

Theme LightTheme = MakeTheme(LightThemeColors());

Theme DarkTheme = MakeTheme(DarkThemeColors());

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
static constexpr float CachedSurfaceSupersampleScale = 2.0f;
static GLuint CompositeProgram = 0;
static GLuint CompositeVAO = 0;
static GLuint CompositeVBO = 0;
static GLint CompositeProjLoc = -1;
static GLint CompositePosLoc = -1;
static GLint CompositeSizeLoc = -1;
static GLint CompositeTextureLoc = -1;
static GLint CompositeUVPosLoc = -1;
static GLint CompositeUVSizeLoc = -1;
static GLuint ImageProgram = 0;
static GLint ImageProjLoc = -1;
static GLint ImagePosLoc = -1;
static GLint ImageSizeLoc = -1;
static GLint ImageTextureLoc = -1;
static GLint ImageBoxPosLoc = -1;
static GLint ImageBoxSizeLoc = -1;
static GLint ImageRoundingLoc = -1;
static GLint ImageTintLoc = -1;
static GLuint PolygonProgram = 0;
static GLuint PolygonVAO = 0;
static GLuint PolygonVBO = 0;
static GLsizeiptr PolygonVBOCapacity = 0;
static GLint PolygonProjLoc = -1;
static GLint PolygonColorLoc = -1;
static GLint PolygonGradientEnabledLoc = -1;
static GLint PolygonGradientTopLeftLoc = -1;
static GLint PolygonGradientTopRightLoc = -1;
static GLint PolygonGradientBottomLeftLoc = -1;
static GLint PolygonGradientBottomRightLoc = -1;
static int ActiveLayerIndex = -1;
static GLuint CurrentActiveProgram = 0;
static bool ActiveCustomSurface = false;
static RectFrame ActiveCustomSurfaceBounds;
static int ActiveCustomSurfaceWidth = 0;
static int ActiveCustomSurfaceHeight = 0;
static std::unordered_map<std::string, GLuint> ImageTextureCache;
static std::unordered_map<std::string, std::string> DownloadedImagePathCache;
static std::unordered_map<std::string, bool> RemoteImageDownloadInFlight;
static std::unordered_map<std::string, long long> RemoteImageRetryAfterMs;
static std::mutex RemoteImageMutex;
static std::atomic<bool> RemoteImageReadyFlag = false;

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

static const char* imageVShaderStr = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
out vec2 vUV;
out vec2 vPos;
uniform mat4 projection;
uniform vec2 uPos;
uniform vec2 uSize;
void main() {
    vec2 pos = uPos + aPos * uSize;
    vUV = aUV;
    vPos = pos;
    gl_Position = projection * vec4(pos, 0.0, 1.0);
}
)";

static const char* imageFShaderStr = R"(
#version 330 core
in vec2 vUV;
in vec2 vPos;
uniform sampler2D uTexture;
uniform vec2 uBoxPos;
uniform vec2 uBoxSize;
uniform float uRounding;
uniform vec4 uTint;
out vec4 FragColor;

float roundedBoxSDF(vec2 centerPosition, vec2 size, float radius) {
    return length(max(abs(centerPosition) - size + radius, 0.0)) - radius;
}

void main() {
    vec2 center = uBoxPos + uBoxSize * 0.5;
    vec2 p = vPos - center;
    float d = roundedBoxSDF(p, uBoxSize * 0.5, uRounding);
    float alpha = 1.0 - smoothstep(-1.0, 1.0, d);
    if (alpha <= 0.0) {
        discard;
    }
    vec4 sampled = texture(uTexture, vUV);
    FragColor = vec4(sampled.rgb * uTint.rgb, sampled.a * uTint.a * alpha);
}
)";

static const char* polygonVShaderStr = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
uniform mat4 projection;
out vec2 vUV;
void main() {
    vUV = aUV;
    gl_Position = projection * vec4(aPos, 0.0, 1.0);
}
)";

static const char* polygonFShaderStr = R"(
#version 330 core
uniform vec4 uColor;
uniform int uGradientEnabled;
uniform vec4 uGradientTopLeft;
uniform vec4 uGradientTopRight;
uniform vec4 uGradientBottomLeft;
uniform vec4 uGradientBottomRight;
in vec2 vUV;
out vec4 FragColor;
vec4 fillColorAt(vec2 uv) {
    vec4 top = mix(uGradientTopLeft, uGradientTopRight, clamp(uv.x, 0.0, 1.0));
    vec4 bottom = mix(uGradientBottomLeft, uGradientBottomRight, clamp(uv.x, 0.0, 1.0));
    return mix(top, bottom, clamp(uv.y, 0.0, 1.0));
}
void main() {
    FragColor = (uGradientEnabled == 1) ? fillColorAt(vUV) : uColor;
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

static bool IsHttpUrl(const std::string& path) {
    return path.rfind("http://", 0) == 0 || path.rfind("https://", 0) == 0;
}

static std::string BuildDownloadedImageFilePath(const std::string& url) {
    const char* tempDir = std::getenv("TMPDIR");
    if (tempDir == nullptr || tempDir[0] == '\0') {
        tempDir = std::getenv("TEMP");
    }
    if (tempDir == nullptr || tempDir[0] == '\0') {
        tempDir = std::getenv("TMP");
    }
    if (tempDir == nullptr || tempDir[0] == '\0') {
#ifdef _WIN32
        tempDir = ".";
#else
        tempDir = "/tmp";
#endif
    }
    const size_t hashValue = std::hash<std::string>{}(url);
    std::ostringstream oss;
    oss << tempDir;
    const char last = oss.str().empty() ? '\0' : oss.str().back();
    if (last != '/' && last != '\\') {
#ifdef _WIN32
        oss << "\\";
#else
        oss << "/";
#endif
    }
    oss << "eui_neo_img_" << hashValue << ".cache";
    return oss.str();
}

#ifdef EUI_HAS_CURL
static size_t CurlWriteToFile(void* data, size_t size, size_t nmemb, void* userdata) {
    const size_t bytes = size * nmemb;
    std::ofstream* output = reinterpret_cast<std::ofstream*>(userdata);
    output->write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(bytes));
    return bytes;
}

static size_t CurlWriteToString(void* data, size_t size, size_t nmemb, void* userdata) {
    const size_t bytes = size * nmemb;
    std::string* output = reinterpret_cast<std::string*>(userdata);
    output->append(reinterpret_cast<const char*>(data), bytes);
    return bytes;
}

static bool EnsureCurlReady() {
    static bool initialized = false;
    static bool ready = false;
    if (!initialized) {
        ready = curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
        initialized = true;
    }
    return ready;
}

static bool DownloadUrlToString(const std::string& url, std::string& output) {
    if (!EnsureCurlReady()) {
        return false;
    }
    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        return false;
    }
    output.clear();
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteToString);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &output);
    CURLcode code = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);
    return code == CURLE_OK && status >= 200 && status < 300;
}

static bool DownloadUrlToFile(const std::string& url, const std::string& localFilePath) {
    if (!EnsureCurlReady()) {
        return false;
    }
    std::ofstream output(localFilePath, std::ios::binary | std::ios::trunc);
    if (!output.good()) {
        return false;
    }
    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        return false;
    }
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteToFile);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &output);
    CURLcode code = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);
    output.close();
    return code == CURLE_OK && status >= 200 && status < 300;
}
#elif defined(_WIN32)
static bool DownloadUrlToString(const std::string& url, std::string& output) {
    const std::string tempFilePath = BuildDownloadedImageFilePath(url + "__meta");
    if (tempFilePath.empty()) {
        return false;
    }
    HRESULT hr = URLDownloadToFileA(nullptr, url.c_str(), tempFilePath.c_str(), 0, nullptr);
    if (FAILED(hr)) {
        return false;
    }
    std::ifstream input(tempFilePath, std::ios::binary);
    if (!input.good()) {
        std::remove(tempFilePath.c_str());
        return false;
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    output = buffer.str();
    input.close();
    std::remove(tempFilePath.c_str());
    return !output.empty();
}

static bool DownloadUrlToFile(const std::string& url, const std::string& localFilePath) {
    HRESULT hr = URLDownloadToFileA(nullptr, url.c_str(), localFilePath.c_str(), 0, nullptr);
    return SUCCEEDED(hr);
}
#else
static bool DownloadUrlToString(const std::string& url, std::string& output) {
    output.clear();
    return false;
}

static bool DownloadUrlToFile(const std::string& url, const std::string& localFilePath) {
    return false;
}
#endif

static std::string ResolveBingDailyImageUrl(const std::string& uri) {
#if defined(EUI_HAS_CURL) || defined(_WIN32)
    const std::string api = ImageSource::BuildBingDailyApiUrl(uri);
    std::string payload;
    if (!DownloadUrlToString(api, payload)) {
        return {};
    }
    return ImageSource::ExtractBingImageUrlFromJson(payload);
#else
    return {};
#endif
}

static std::string DownloadUrlImageToFile(const std::string& url) {
    std::lock_guard<std::mutex> lock(RemoteImageMutex);
    auto cacheIt = DownloadedImagePathCache.find(url);
    if (cacheIt == DownloadedImagePathCache.end()) {
        return {};
    }
    std::ifstream cached(cacheIt->second, std::ios::binary);
    if (!cached.good()) {
        return {};
    }
    return cacheIt->second;
}

static long long CurrentTimeMs() {
    return static_cast<long long>(glfwGetTime() * 1000.0);
}

static bool ShouldStartRemoteFetch(const std::string& key) {
    std::lock_guard<std::mutex> lock(RemoteImageMutex);
    auto readyIt = DownloadedImagePathCache.find(key);
    if (readyIt != DownloadedImagePathCache.end()) {
        std::ifstream file(readyIt->second, std::ios::binary);
        if (file.good()) {
            return false;
        }
    }
    auto inflightIt = RemoteImageDownloadInFlight.find(key);
    if (inflightIt != RemoteImageDownloadInFlight.end() && inflightIt->second) {
        return false;
    }
    const long long nowMs = CurrentTimeMs();
    auto retryIt = RemoteImageRetryAfterMs.find(key);
    if (retryIt != RemoteImageRetryAfterMs.end() && nowMs < retryIt->second) {
        return false;
    }
    RemoteImageDownloadInFlight[key] = true;
    return true;
}

static void CompleteRemoteFetch(const std::string& key, const std::string& localPath) {
    std::lock_guard<std::mutex> lock(RemoteImageMutex);
    RemoteImageDownloadInFlight[key] = false;
    if (!localPath.empty()) {
        DownloadedImagePathCache[key] = localPath;
        RemoteImageRetryAfterMs.erase(key);
        RemoteImageReadyFlag.store(true);
        glfwPostEmptyEvent();
    } else {
        RemoteImageRetryAfterMs[key] = CurrentTimeMs() + 5000;
    }
}

static void StartRemoteFetchAsync(const std::string& key) {
    if (!ShouldStartRemoteFetch(key)) {
        return;
    }
    std::thread([key]() {
        std::string localPath;
        std::string url = key;
        if (ImageSource::IsBingDailyScheme(key)) {
            url = ResolveBingDailyImageUrl(key);
        }
        if (!url.empty()) {
            const std::string targetFile = BuildDownloadedImageFilePath(url);
            if (!targetFile.empty() && DownloadUrlToFile(url, targetFile)) {
                localPath = targetFile;
            }
        }
        CompleteRemoteFetch(key, localPath);
    }).detach();
}

static std::string ResolveUrlImagePath(const std::string& path) {
    const std::string cached = DownloadUrlImageToFile(path);
    if (!cached.empty()) {
        return cached;
    }
    StartRemoteFetchAsync(path);
    return {};
}

static std::string ResolveImagePath(const std::string& path) {
    if (path.empty()) {
        return {};
    }
    if (IsHttpUrl(path) || ImageSource::IsBingDailyScheme(path)) {
        const std::string resolvedUrlPath = ResolveUrlImagePath(path);
        if (!resolvedUrlPath.empty()) {
            return resolvedUrlPath;
        }
#if !defined(EUI_HAS_CURL) && !defined(_WIN32)
        return {};
#endif
    }
    std::ifstream direct(path, std::ios::binary);
    if (direct.good()) {
        return path;
    }
    static const char* prefixes[] = {"./", "../", "../../", "../../../", "../../../../"};
    for (const char* prefix : prefixes) {
        const std::string candidate = std::string(prefix) + path;
        std::ifstream file(candidate, std::ios::binary);
        if (file.good()) {
            return candidate;
        }
    }
    return {};
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

static RenderLayer LayerFromIndex(int index);

static bool CanReuseBlurCache(const RectStyle& style) {
    // The cached blur texture copies pixels from the current full-screen source.
    // Restrict reuse to the default framebuffer and the full-size backdrop layer
    // so the copy coordinates remain valid without changing the output.
    return style.blurAmount > 0.0f &&
           !ActiveCustomSurface &&
           (ActiveLayerIndex < 0 || LayerFromIndex(ActiveLayerIndex) == RenderLayer::Backdrop);
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

static RectBounds ComputePolygonBounds(const std::vector<Point2>& points, float strokeWidth) {
    RectBounds bounds;
    if (points.empty()) {
        return bounds;
    }

    float minX = points[0].x;
    float minY = points[0].y;
    float maxX = points[0].x;
    float maxY = points[0].y;
    for (const Point2& point : points) {
        minX = std::min(minX, point.x);
        minY = std::min(minY, point.y);
        maxX = std::max(maxX, point.x);
        maxY = std::max(maxY, point.y);
    }

    const float expand = std::max(0.0f, strokeWidth) * 0.5f;
    bounds.x = minX - expand;
    bounds.y = minY - expand;
    bounds.w = (maxX - minX) + expand * 2.0f;
    bounds.h = (maxY - minY) + expand * 2.0f;
    return bounds;
}

static float SignedPolygonArea(const std::vector<Point2>& points) {
    if (points.size() < 3) {
        return 0.0f;
    }

    float area = 0.0f;
    for (size_t index = 0; index < points.size(); ++index) {
        const Point2& current = points[index];
        const Point2& next = points[(index + 1) % points.size()];
        area += current.x * next.y - next.x * current.y;
    }
    return area * 0.5f;
}

static float TriangleCross(const Point2& a, const Point2& b, const Point2& c) {
    return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
}

static bool PointInTriangle(const Point2& point, const Point2& a, const Point2& b, const Point2& c) {
    const float area1 = TriangleCross(point, a, b);
    const float area2 = TriangleCross(point, b, c);
    const float area3 = TriangleCross(point, c, a);
    const bool hasNegative = area1 < 0.0f || area2 < 0.0f || area3 < 0.0f;
    const bool hasPositive = area1 > 0.0f || area2 > 0.0f || area3 > 0.0f;
    return !(hasNegative && hasPositive);
}

static std::vector<Point2> TriangulatePolygon(const std::vector<Point2>& points) {
    std::vector<Point2> triangles;
    if (points.size() < 3) {
        return triangles;
    }

    if (points.size() == 3) {
        return points;
    }

    std::vector<int> indices(points.size());
    for (size_t index = 0; index < points.size(); ++index) {
        indices[index] = static_cast<int>(index);
    }

    const bool ccw = SignedPolygonArea(points) >= 0.0f;
    int guard = static_cast<int>(points.size()) * static_cast<int>(points.size());
    while (indices.size() > 3 && guard-- > 0) {
        bool clippedEar = false;
        for (size_t index = 0; index < indices.size(); ++index) {
            const size_t prevIndex = (index + indices.size() - 1) % indices.size();
            const size_t nextIndex = (index + 1) % indices.size();
            const Point2& a = points[indices[prevIndex]];
            const Point2& b = points[indices[index]];
            const Point2& c = points[indices[nextIndex]];

            const float cross = TriangleCross(a, b, c);
            if ((ccw && cross <= 0.0f) || (!ccw && cross >= 0.0f)) {
                continue;
            }

            bool containsOtherPoint = false;
            for (size_t testIndex = 0; testIndex < indices.size(); ++testIndex) {
                if (testIndex == prevIndex || testIndex == index || testIndex == nextIndex) {
                    continue;
                }
                if (PointInTriangle(points[indices[testIndex]], a, b, c)) {
                    containsOtherPoint = true;
                    break;
                }
            }
            if (containsOtherPoint) {
                continue;
            }

            triangles.push_back(a);
            triangles.push_back(b);
            triangles.push_back(c);
            indices.erase(indices.begin() + static_cast<std::ptrdiff_t>(index));
            clippedEar = true;
            break;
        }

        if (!clippedEar) {
            break;
        }
    }

    if (indices.size() == 3) {
        triangles.push_back(points[indices[0]]);
        triangles.push_back(points[indices[1]]);
        triangles.push_back(points[indices[2]]);
    }

    if (indices.size() != 3) {
        triangles.clear();
    }

    if (triangles.empty()) {
        for (size_t index = 1; index + 1 < points.size(); ++index) {
            triangles.push_back(points[0]);
            triangles.push_back(points[index]);
            triangles.push_back(points[index + 1]);
        }
    }

    return triangles;
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
        const float scaleX = std::max(State.dpiScaleX, 0.0001f);
        return std::max(1, static_cast<int>(std::ceil(std::max(0.0f, bounds.width * scaleX))));
    }
    return std::max(1, static_cast<int>(std::ceil(State.framebufferW)));
}

static int TargetLayerHeight(RenderLayer layer, const RectFrame& bounds) {
    if (UsesTightLayerSurface(layer)) {
        const float scaleY = std::max(State.dpiScaleY, 0.0001f);
        return std::max(1, static_cast<int>(std::ceil(std::max(0.0f, bounds.height * scaleY))));
    }
    return std::max(1, static_cast<int>(std::ceil(State.framebufferH)));
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

static void CurrentProjectionBounds(float& outL, float& outR, float& outT, float& outB) {
    outL = 0.0f;
    outR = State.screenW;
    outB = State.screenH;
    outT = 0.0f;

    if (ActiveCustomSurface) {
        if (ActiveCustomSurfaceBounds.width > 0.0f && ActiveCustomSurfaceBounds.height > 0.0f) {
            outL = ActiveCustomSurfaceBounds.x;
            outR = ActiveCustomSurfaceBounds.x + ActiveCustomSurfaceBounds.width;
            outT = ActiveCustomSurfaceBounds.y;
            outB = ActiveCustomSurfaceBounds.y + ActiveCustomSurfaceBounds.height;
        }
        return;
    }

    if (ActiveLayerIndex >= 0) {
        const RenderLayer layer = LayerFromIndex(ActiveLayerIndex);
        if (UsesTightLayerSurface(layer)) {
            const RectFrame& bounds = LayerCaches[ActiveLayerIndex].bounds;
            if (bounds.width > 0.0f && bounds.height > 0.0f) {
                outL = bounds.x;
                outR = bounds.x + bounds.width;
                outT = bounds.y;
                outB = bounds.y + bounds.height;
            }
        }
    }
}

static void CompositeTexture(const GLuint texture, const RectFrame& bounds,
                             float uvX, float uvY, float uvW, float uvH) {
    if (texture == 0 || bounds.width <= 0.0f || bounds.height <= 0.0f) {
        return;
    }

    float L = 0.0f;
    float R = State.screenW;
    float B = State.screenH;
    float T = 0.0f;
    CurrentProjectionBounds(L, R, T, B);
    const float proj[16] = {
        2.0f / (R - L), 0, 0, 0,
        0, 2.0f / (T - B), 0, 0,
        0, 0, -1, 0,
        -(R + L) / (R - L), -(T + B) / (T - B), 0, 1
    };

    if (CurrentActiveProgram != CompositeProgram) {
        glUseProgram(CompositeProgram);
        CurrentActiveProgram = CompositeProgram;
    }
    glUniformMatrix4fv(CompositeProjLoc, 1, GL_FALSE, proj);
    glUniform2f(CompositePosLoc, bounds.x, bounds.y);
    glUniform2f(CompositeSizeLoc, bounds.width, bounds.height);
    glUniform2f(CompositeUVPosLoc, uvX, uvY);
    glUniform2f(CompositeUVSizeLoc, uvW, uvH);
    glBindVertexArray(CompositeVAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

static bool MakeScreenScissorRect(const RectFrame& bounds, GLint& outX, GLint& outY, GLint& outW, GLint& outH) {
    if (bounds.width <= 0.0f || bounds.height <= 0.0f) {
        return false;
    }

    const float scaleX = std::max(State.dpiScaleX, 0.0001f);
    const float scaleY = std::max(State.dpiScaleY, 0.0001f);
    const float framebufferW = std::max(State.framebufferW, 1.0f);
    const float framebufferH = std::max(State.framebufferH, 1.0f);
    const float x1 = std::clamp(bounds.x * scaleX, 0.0f, framebufferW);
    const float y1 = std::clamp(bounds.y * scaleY, 0.0f, framebufferH);
    const float x2 = std::clamp((bounds.x + bounds.width) * scaleX, x1, framebufferW);
    const float y2 = std::clamp((bounds.y + bounds.height) * scaleY, y1, framebufferH);
    if (x2 <= x1 || y2 <= y1) {
        return false;
    }

    outX = static_cast<GLint>(std::floor(x1));
    outY = static_cast<GLint>(std::floor(framebufferH - y2));
    outW = std::max<GLint>(1, static_cast<GLint>(std::ceil(x2) - std::floor(x1)));
    outH = std::max<GLint>(1, static_cast<GLint>(std::ceil(y2) - std::floor(y1)));
    return true;
}

static bool MakeLocalScissorRect(const RectFrame& surfaceBounds, int surfaceWidth, int surfaceHeight,
                                 const RectFrame& bounds, GLint& outX, GLint& outY,
                                 GLint& outW, GLint& outH) {
    if (bounds.width <= 0.0f || bounds.height <= 0.0f ||
        surfaceBounds.width <= 0.0f || surfaceBounds.height <= 0.0f ||
        surfaceWidth <= 0 || surfaceHeight <= 0) {
        return false;
    }

    const float scaleX = static_cast<float>(surfaceWidth) / surfaceBounds.width;
    const float scaleY = static_cast<float>(surfaceHeight) / surfaceBounds.height;
    const float x1 = std::clamp((bounds.x - surfaceBounds.x) * scaleX, 0.0f, static_cast<float>(surfaceWidth));
    const float y1 = std::clamp((bounds.y - surfaceBounds.y) * scaleY, 0.0f, static_cast<float>(surfaceHeight));
    const float x2 = std::clamp((bounds.x + bounds.width - surfaceBounds.x) * scaleX, x1, static_cast<float>(surfaceWidth));
    const float y2 = std::clamp((bounds.y + bounds.height - surfaceBounds.y) * scaleY, y1, static_cast<float>(surfaceHeight));
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
    float RenderSize[2];
    float RenderBearing[2];
    float VisibleSize[2];
    float VisibleBearing[2];
    float Advance;
    float BasePixelSize;
    bool IsSdf;
};

struct FontSource {
    std::string Path;
    std::vector<unsigned char> Buffer;
    stbtt_fontinfo Info{};
    float PixelSize = 24.0f;
    bool UseSdf = true;
    bool Loaded = false;
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
static std::unordered_map<TextWidthCacheKey, RectFrame, TextWidthCacheKeyHash> TextBoundsCache;
static std::vector<FontSource> FontSources;
static GLuint TextVAO = 0;
static GLuint TextVBO = 0;
static GLuint TextShaderProgram = 0;
static GLint TextProjLoc = -1;
static GLint TextColorLoc = -1;
static GLint TextModeLoc = -1;
static GLint TextSdfEdgeLoc = -1;
static GLint TextSdfPixelRangeLoc = -1;
static GLint TextSdfSoftnessLoc = -1;
static GLint TextSdfBiasLoc = -1;
static GLint TextSdfMinWidthLoc = -1;
static GLint TextSdfDiscardThresholdLoc = -1;
static GLint TextSdfSmallTextBlendLoc = -1;
static constexpr int kTextSdfPadding = 16;
static constexpr unsigned char kTextSdfOnEdgeValue = 120;
static constexpr float kTextSdfPixelDistScale =
    static_cast<float>(kTextSdfOnEdgeValue) / static_cast<float>(kTextSdfPadding);
static constexpr float kTextSdfSoftness = 1.0f;
static constexpr float kTextSdfBias = 0.0f;
static constexpr float kTextSdfMinWidth = 0.35f;
static constexpr float kTextSdfDiscardThreshold = 0.005f;
static constexpr float kTextSdfSmallTextStartPx = 64.0f;
static constexpr float kTextSdfSmallTextEndPx = 16.0f;
static constexpr float kTextSdfSmallSoftness = 1.25f;
static constexpr float kTextSdfSmallBias = 0.015f;
static constexpr float kTextSdfSmallMinWidth = 0.55f;
static constexpr float kTextSdfSmallDiscardThreshold = 0.001f;

static int MakeTextScaleKey(float scale) {
    return static_cast<int>(std::lround(scale * 1024.0f));
}

static float ResolveRequestedTextPixelSize(float normalizedScale) {
    return 24.0f * normalizedScale;
}

static float ResolveCharacterScale(const Character& character, float normalizedScale) {
    const float basePixelSize = std::max(character.BasePixelSize, 1.0f);
    return ResolveRequestedTextPixelSize(normalizedScale) / basePixelSize;
}

static float ResolveSmallTextBlend(float requestedPixelSize) {
    if (requestedPixelSize <= kTextSdfSmallTextEndPx) {
        return 1.0f;
    }
    if (requestedPixelSize >= kTextSdfSmallTextStartPx) {
        return 0.0f;
    }
    const float range = std::max(kTextSdfSmallTextStartPx - kTextSdfSmallTextEndPx, 0.001f);
    return (kTextSdfSmallTextStartPx - requestedPixelSize) / range;
}

static FontSource* FindFontSource(const std::string& fontPath, float fontSize, bool useSdf) {
    for (FontSource& source : FontSources) {
        if (source.Path == fontPath &&
            std::abs(source.PixelSize - fontSize) < 0.001f &&
            source.UseSdf == useSdf) {
            return &source;
        }
    }
    return nullptr;
}

static bool EnsureFontSourceLoaded(FontSource& source) {
    if (source.Loaded) {
        return true;
    }

    std::ifstream file(source.Path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return false;
    }
    const std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    source.Buffer.resize(static_cast<std::size_t>(size));
    if (!file.read(reinterpret_cast<char*>(source.Buffer.data()), size)) {
        source.Buffer.clear();
        return false;
    }

    const int fontOffset = std::max(0, stbtt_GetFontOffsetForIndex(source.Buffer.data(), 0));
    if (!stbtt_InitFont(&source.Info, source.Buffer.data(), fontOffset)) {
        source.Buffer.clear();
        return false;
    }

    source.Loaded = true;
    return true;
}

static FontSource* RegisterFontSourceInternal(const std::string& fontPath, float fontSize, bool useSdf,
                                              bool loadImmediately) {
    if (FontSource* existing = FindFontSource(fontPath, fontSize, useSdf)) {
        return (!loadImmediately || EnsureFontSourceLoaded(*existing)) ? existing : nullptr;
    }

    if (!loadImmediately) {
        std::ifstream file(fontPath, std::ios::binary);
        if (!file.is_open()) {
            return nullptr;
        }
    }

    FontSource source;
    source.Path = fontPath;
    source.PixelSize = fontSize;
    source.UseSdf = useSdf;
    if (loadImmediately && !EnsureFontSourceLoaded(source)) {
        return nullptr;
    }

    FontSources.push_back(std::move(source));
    return &FontSources.back();
}

static bool LoadCharacterFromSource(FontSource& source, unsigned int codepoint) {
    if (Characters.find(codepoint) != Characters.end()) {
        return true;
    }
    if (!EnsureFontSourceLoaded(source)) {
        return false;
    }
    if (stbtt_FindGlyphIndex(&source.Info, codepoint) == 0) {
        return false;
    }

    const float scale = stbtt_ScaleForPixelHeight(&source.Info, source.PixelSize);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    int renderWidth = 0;
    int renderHeight = 0;
    int renderXoff = 0;
    int renderYoff = 0;
    unsigned char* bitmap = nullptr;
    if (source.UseSdf) {
        bitmap = stbtt_GetCodepointSDF(&source.Info, scale, codepoint, kTextSdfPadding,
                                       kTextSdfOnEdgeValue, kTextSdfPixelDistScale,
                                       &renderWidth, &renderHeight, &renderXoff, &renderYoff);
    } else {
        bitmap = stbtt_GetCodepointBitmap(&source.Info, scale, scale, codepoint,
                                          &renderWidth, &renderHeight, &renderXoff, &renderYoff);
    }
    if (!bitmap) {
        return false;
    }

    int visibleX0 = 0;
    int visibleY0 = 0;
    int visibleX1 = 0;
    int visibleY1 = 0;
    stbtt_GetCodepointBitmapBox(&source.Info, codepoint, scale, scale,
                                &visibleX0, &visibleY0, &visibleX1, &visibleY1);

    int advance = 0;
    int lsb = 0;
    stbtt_GetCodepointHMetrics(&source.Info, codepoint, &advance, &lsb);

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, renderWidth, renderHeight, 0, GL_RED, GL_UNSIGNED_BYTE, bitmap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    const bool useMipmaps = !source.UseSdf && source.PixelSize >= 48.0f;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, useMipmaps ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    if (useMipmaps) {
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    glBindTexture(GL_TEXTURE_2D, 0);

    Characters[codepoint] = {
        texture,
        {static_cast<float>(renderWidth), static_cast<float>(renderHeight)},
        {static_cast<float>(renderXoff), static_cast<float>(renderYoff)},
        {static_cast<float>(std::max(0, visibleX1 - visibleX0)), static_cast<float>(std::max(0, visibleY1 - visibleY0))},
        {static_cast<float>(visibleX0), static_cast<float>(visibleY0)},
        advance * scale,
        source.PixelSize,
        source.UseSdf
    };

    if (source.UseSdf) {
        stbtt_FreeSDF(bitmap, nullptr);
    } else {
        stbtt_FreeBitmap(bitmap, nullptr);
    }

    TextWidthCache.clear();
    TextBoundsCache.clear();
    return true;
}

static bool EnsureCharacterLoaded(unsigned int codepoint) {
    if (codepoint == ' ' || Characters.find(codepoint) != Characters.end()) {
        return true;
    }
    for (FontSource& source : FontSources) {
        if (LoadCharacterFromSource(source, codepoint)) {
            return true;
        }
    }
    return false;
}

static bool DecodeUtf8Codepoint(const std::string& text, size_t& index, unsigned int& outCodepoint) {
    if (index >= text.length()) {
        return false;
    }

    const unsigned char ch = static_cast<unsigned char>(text[index]);
    if (ch <= 0x7F) {
        outCodepoint = ch;
        ++index;
        return true;
    }
    if ((ch & 0xE0) == 0xC0) {
        if (index + 1 >= text.length()) {
            ++index;
            return false;
        }
        outCodepoint = ((ch & 0x1F) << 6) | (static_cast<unsigned char>(text[index + 1]) & 0x3F);
        index += 2;
        return true;
    }
    if ((ch & 0xF0) == 0xE0) {
        if (index + 2 >= text.length()) {
            ++index;
            return false;
        }
        outCodepoint = ((ch & 0x0F) << 12) |
                       ((static_cast<unsigned char>(text[index + 1]) & 0x3F) << 6) |
                       (static_cast<unsigned char>(text[index + 2]) & 0x3F);
        index += 3;
        return true;
    }
    if ((ch & 0xF8) == 0xF0) {
        if (index + 3 >= text.length()) {
            ++index;
            return false;
        }
        outCodepoint = ((ch & 0x07) << 18) |
                       ((static_cast<unsigned char>(text[index + 1]) & 0x3F) << 12) |
                       ((static_cast<unsigned char>(text[index + 2]) & 0x3F) << 6) |
                       (static_cast<unsigned char>(text[index + 3]) & 0x3F);
        index += 4;
        return true;
    }

    ++index;
    return false;
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
uniform int textMode;
uniform float sdfEdgeValue;
uniform float sdfPxRange;
uniform float sdfSoftness;
uniform float sdfBias;
uniform float sdfMinWidth;
uniform float sdfDiscardThreshold;
uniform float sdfSmallTextBlend;
void main() {
    float sampleValue = texture(text, TexCoords).r;
    float alpha = sampleValue;
    if (textMode == 1) {
        if (sdfSmallTextBlend > 0.001) {
            vec2 texelSize = 1.0 / vec2(textureSize(text, 0));
            vec2 sampleOffset = texelSize * (0.60 * sdfSmallTextBlend);
            float filteredSample = (sampleValue * 2.0
                + texture(text, TexCoords + vec2(sampleOffset.x, 0.0)).r
                + texture(text, TexCoords - vec2(sampleOffset.x, 0.0)).r
                + texture(text, TexCoords + vec2(0.0, sampleOffset.y)).r
                + texture(text, TexCoords - vec2(0.0, sampleOffset.y)).r) / 6.0;
            sampleValue = mix(sampleValue, filteredSample, sdfSmallTextBlend);
        }
        float texelDistance = (sampleValue - sdfEdgeValue) * (sdfPxRange / max(sdfEdgeValue, 0.0001));
        float aaWidth = max(fwidth(texelDistance) * sdfSoftness, sdfMinWidth);
        alpha = smoothstep(-aaWidth, aaWidth, texelDistance + sdfBias);
        if (alpha < sdfDiscardThreshold) {
            discard;
        }
    }
    color = vec4(textColor.rgb, textColor.a * alpha);
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
    ImageProgram = CreateProgram(imageVShaderStr, imageFShaderStr);
    PolygonProgram = CreateProgram(polygonVShaderStr, polygonFShaderStr);

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
    ImageProjLoc = glGetUniformLocation(ImageProgram, "projection");
    ImagePosLoc = glGetUniformLocation(ImageProgram, "uPos");
    ImageSizeLoc = glGetUniformLocation(ImageProgram, "uSize");
    ImageTextureLoc = glGetUniformLocation(ImageProgram, "uTexture");
    ImageBoxPosLoc = glGetUniformLocation(ImageProgram, "uBoxPos");
    ImageBoxSizeLoc = glGetUniformLocation(ImageProgram, "uBoxSize");
    ImageRoundingLoc = glGetUniformLocation(ImageProgram, "uRounding");
    ImageTintLoc = glGetUniformLocation(ImageProgram, "uTint");
    PolygonProjLoc = glGetUniformLocation(PolygonProgram, "projection");
    PolygonColorLoc = glGetUniformLocation(PolygonProgram, "uColor");
    PolygonGradientEnabledLoc = glGetUniformLocation(PolygonProgram, "uGradientEnabled");
    PolygonGradientTopLeftLoc = glGetUniformLocation(PolygonProgram, "uGradientTopLeft");
    PolygonGradientTopRightLoc = glGetUniformLocation(PolygonProgram, "uGradientTopRight");
    PolygonGradientBottomLeftLoc = glGetUniformLocation(PolygonProgram, "uGradientBottomLeft");
    PolygonGradientBottomRightLoc = glGetUniformLocation(PolygonProgram, "uGradientBottomRight");
    glUseProgram(ShaderProgram);
    glUniform1i(Channel0Loc, 0);
    glUseProgram(CachedBlurProgram);
    glUniform1i(CachedBlurTextureLoc, 0);
    glUseProgram(CompositeProgram);
    glUniform1i(CompositeTextureLoc, 0);
    glUseProgram(ImageProgram);
    glUniform1i(ImageTextureLoc, 0);

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
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glGenVertexArrays(1, &PolygonVAO);
    glGenBuffers(1, &PolygonVBO);
    glBindVertexArray(PolygonVAO);
    glBindBuffer(GL_ARRAY_BUFFER, PolygonVBO);
    PolygonVBOCapacity = sizeof(float) * 4 * 3;
    glBufferData(GL_ARRAY_BUFFER, PolygonVBOCapacity, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

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
    TextModeLoc = glGetUniformLocation(TextShaderProgram, "textMode");
    TextSdfEdgeLoc = glGetUniformLocation(TextShaderProgram, "sdfEdgeValue");
    TextSdfPixelRangeLoc = glGetUniformLocation(TextShaderProgram, "sdfPxRange");
    TextSdfSoftnessLoc = glGetUniformLocation(TextShaderProgram, "sdfSoftness");
    TextSdfBiasLoc = glGetUniformLocation(TextShaderProgram, "sdfBias");
    TextSdfMinWidthLoc = glGetUniformLocation(TextShaderProgram, "sdfMinWidth");
    TextSdfDiscardThresholdLoc = glGetUniformLocation(TextShaderProgram, "sdfDiscardThreshold");
    TextSdfSmallTextBlendLoc = glGetUniformLocation(TextShaderProgram, "sdfSmallTextBlend");
    glUseProgram(TextShaderProgram);
    glUniform1i(glGetUniformLocation(TextShaderProgram, "text"), 0);
    glUniform1i(TextModeLoc, 1);
    glUniform1f(TextSdfEdgeLoc, static_cast<float>(kTextSdfOnEdgeValue) / 255.0f);
    glUniform1f(TextSdfPixelRangeLoc, static_cast<float>(kTextSdfPadding));
    glUniform1f(TextSdfSoftnessLoc, kTextSdfSoftness);
    glUniform1f(TextSdfBiasLoc, kTextSdfBias);
    glUniform1f(TextSdfMinWidthLoc, kTextSdfMinWidth);
    glUniform1f(TextSdfDiscardThresholdLoc, kTextSdfDiscardThreshold);
    glUniform1f(TextSdfSmallTextBlendLoc, 0.0f);

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
    for (auto& entry : Characters) {
        if (entry.second.TextureID) {
            glDeleteTextures(1, &entry.second.TextureID);
        }
    }
    Characters.clear();
    TextWidthCache.clear();
    TextBoundsCache.clear();
    FontSources.clear();
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
    glDeleteProgram(ImageProgram);
    glDeleteProgram(PolygonProgram);
    glDeleteVertexArrays(1, &CompositeVAO);
    glDeleteBuffers(1, &CompositeVBO);
    glDeleteVertexArrays(1, &PolygonVAO);
    glDeleteBuffers(1, &PolygonVBO);
    glDeleteTextures(1, &BgTexture);
    if (CachedBlurTexture) glDeleteTextures(1, &CachedBlurTexture);
    for (auto& entry : ImageTextureCache) {
        if (entry.second) {
            glDeleteTextures(1, &entry.second);
        }
    }
    ImageTextureCache.clear();
    {
        std::lock_guard<std::mutex> lock(RemoteImageMutex);
        for (const auto& entry : DownloadedImagePathCache) {
            if (!entry.second.empty()) {
                std::remove(entry.second.c_str());
            }
        }
        DownloadedImagePathCache.clear();
        RemoteImageDownloadInFlight.clear();
        RemoteImageRetryAfterMs.clear();
    }
#ifdef EUI_HAS_CURL
    curl_global_cleanup();
#endif

    glDeleteVertexArrays(1, &TextVAO);
    glDeleteBuffers(1, &TextVBO);
    glDeleteProgram(TextShaderProgram);
}

bool Renderer::MakeCurrentScissorRect(const RectFrame& bounds, GLint& outX, GLint& outY, GLint& outW, GLint& outH) {
    if (ActiveCustomSurface) {
        return MakeLocalScissorRect(ActiveCustomSurfaceBounds, ActiveCustomSurfaceWidth, ActiveCustomSurfaceHeight, bounds,
                                    outX, outY, outW, outH);
    }
    if (ActiveLayerIndex >= 0) {
        const RenderLayer layer = LayerFromIndex(ActiveLayerIndex);
        const LayerCache& cache = LayerCaches[ActiveLayerIndex];
        if (UsesTightLayerSurface(layer)) {
            return MakeLocalScissorRect(cache.bounds, cache.width, cache.height, bounds, outX, outY, outW, outH);
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
    glViewport(0, 0, std::max(1, (int)State.framebufferW), std::max(1, (int)State.framebufferH));
}

void Renderer::CompositeLayers(const Color& background) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, std::max(1, (int)State.framebufferW), std::max(1, (int)State.framebufferH));
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glClearColor(background.r, background.g, background.b, background.a);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    CurrentActiveProgram = 0;
}

void Renderer::DrawCachedSurface(const std::string& key, const RectFrame& bounds, bool dirty,
                                 const std::function<void()>& painter) {
    if (bounds.width <= 0.0f || bounds.height <= 0.0f || !painter) {
        return;
    }

    const int previousLayerIndex = ActiveLayerIndex;
    const bool previousCustomSurface = ActiveCustomSurface;
    const RectFrame previousCustomSurfaceBounds = ActiveCustomSurfaceBounds;
    const int previousCustomSurfaceWidth = ActiveCustomSurfaceWidth;
    const int previousCustomSurfaceHeight = ActiveCustomSurfaceHeight;

    CachedSurface& cache = CachedSurfaces[key];
    const int targetW = std::max(1, static_cast<int>(std::ceil(
        bounds.width * CachedSurfaceSupersampleScale * std::max(State.dpiScaleX, 0.0001f))));
    const int targetH = std::max(1, static_cast<int>(std::ceil(
        bounds.height * CachedSurfaceSupersampleScale * std::max(State.dpiScaleY, 0.0001f))));
    const bool sizeChanged =
        !FloatEq(cache.bounds.width, bounds.width) ||
        !FloatEq(cache.bounds.height, bounds.height);

    EnsureCachedSurfaceStorage(cache, targetW, targetH);
    cache.bounds = bounds;
    if (dirty || !cache.ready || sizeChanged) {
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
        ActiveCustomSurface = previousCustomSurface;
        ActiveCustomSurfaceBounds = previousCustomSurfaceBounds;
        ActiveCustomSurfaceWidth = previousCustomSurfaceWidth;
        ActiveCustomSurfaceHeight = previousCustomSurfaceHeight;

        if (previousLayerIndex >= 0) {
            glBindFramebuffer(GL_FRAMEBUFFER, LayerCaches[previousLayerIndex].framebuffer);
            glViewport(0, 0,
                       std::max(1, LayerCaches[previousLayerIndex].width),
                       std::max(1, LayerCaches[previousLayerIndex].height));
        } else {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, std::max(1, (int)State.framebufferW), std::max(1, (int)State.framebufferH));
        }
        BeginFrame();
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
    glDisable(GL_SCISSOR_TEST);
    float L = 0.0f;
    float R = State.screenW;
    float B = State.screenH;
    float T = 0.0f;
    CurrentProjectionBounds(L, R, T, B);
    float proj[16] = {
        2.0f / (R - L), 0, 0, 0,
        0, 2.0f / (T - B), 0, 0,
        0, 0, -1, 0,
        -(R + L) / (R - L), -(T + B) / (T - B), 0, 1
    };

    glUseProgram(ShaderProgram);
    glUniformMatrix4fv(ProjLoc, 1, GL_FALSE, proj);
    glUniform1f(TimeLoc, static_cast<float>(glfwGetTime()));
    glUniform2f(ResolutionLoc, State.framebufferW, State.framebufferH);

    glUseProgram(CachedBlurProgram);
    glUniformMatrix4fv(CachedBlurProjLoc, 1, GL_FALSE, proj);

    glUseProgram(PolygonProgram);
    glUniformMatrix4fv(PolygonProjLoc, 1, GL_FALSE, proj);

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

    if (style.blurAmount > 0.0f) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, BgTexture);
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    if (canReuseBlurCache) {
        const float scaleX = std::max(State.dpiScaleX, 0.0001f);
        const float scaleY = std::max(State.dpiScaleY, 0.0001f);
        const int framebufferW = std::max(1, (int)State.framebufferW);
        const int framebufferH = std::max(1, (int)State.framebufferH);
        int copyX = std::clamp((int)std::floor(bounds.x * scaleX), 0, framebufferW);
        int copyYTop = std::clamp((int)std::floor(bounds.y * scaleY), 0, framebufferH);
        int copyRight = std::clamp((int)std::ceil((bounds.x + bounds.w) * scaleX), 0, framebufferW);
        int copyBottom = std::clamp((int)std::ceil((bounds.y + bounds.h) * scaleY), 0, framebufferH);
        int copyW = copyRight - copyX;
        int copyH = copyBottom - copyYTop;

        if (copyW > 0 && copyH > 0) {
            int copyY = std::max(0, (int)std::floor(State.framebufferH - (copyYTop + copyH)));

            EnsureCachedBlurTexture(copyW, copyH);
            glBindTexture(GL_TEXTURE_2D, CachedBlurTexture);
            glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, copyX, copyY, copyW, copyH);

            CachedBackdropVersion = BackdropVersion;
            CachedBlurX = static_cast<float>(copyX) / scaleX;
            CachedBlurY = static_cast<float>(copyYTop) / scaleY;
            CachedBlurW = static_cast<float>(copyW) / scaleX;
            CachedBlurH = static_cast<float>(copyH) / scaleY;
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

GLuint Renderer::LoadImageTexture(const std::string& imagePath, bool flipVertically) {
    const std::string resolvedPath = ResolveImagePath(imagePath);
    if (resolvedPath.empty()) {
        return 0;
    }
    auto cacheIt = ImageTextureCache.find(resolvedPath);
    if (cacheIt != ImageTextureCache.end()) {
        return cacheIt->second;
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_set_flip_vertically_on_load(flipVertically ? 1 : 0);
    unsigned char* pixels = stbi_load(resolvedPath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (pixels == nullptr || width <= 0 || height <= 0) {
        if (pixels != nullptr) {
            stbi_image_free(pixels);
        }
        return 0;
    }

    GLuint textureId = 0;
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(pixels);

    if (textureId != 0) {
        ImageTextureCache[resolvedPath] = textureId;
    }
    return textureId;
}

void Renderer::ReleaseImageTexture(GLuint textureId) {
    if (textureId == 0) {
        return;
    }
    for (auto it = ImageTextureCache.begin(); it != ImageTextureCache.end(); ++it) {
        if (it->second == textureId) {
            glDeleteTextures(1, &textureId);
            ImageTextureCache.erase(it);
            return;
        }
    }
    glDeleteTextures(1, &textureId);
}

void Renderer::DrawImage(GLuint textureId, float x, float y, float w, float h, float rounding, const Color& tint) {
    if (textureId == 0 || w <= 0.0f || h <= 0.0f) {
        return;
    }

    float L = 0.0f;
    float R = State.screenW;
    float B = State.screenH;
    float T = 0.0f;
    CurrentProjectionBounds(L, R, T, B);
    const float proj[16] = {
        2.0f / (R - L), 0, 0, 0,
        0, 2.0f / (T - B), 0, 0,
        0, 0, -1, 0,
        -(R + L) / (R - L), -(T + B) / (T - B), 0, 1
    };

    if (CurrentActiveProgram != ImageProgram) {
        glUseProgram(ImageProgram);
        CurrentActiveProgram = ImageProgram;
    }
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUniformMatrix4fv(ImageProjLoc, 1, GL_FALSE, proj);
    glUniform2f(ImagePosLoc, x, y);
    glUniform2f(ImageSizeLoc, w, h);
    glUniform2f(ImageBoxPosLoc, x, y);
    glUniform2f(ImageBoxSizeLoc, w, h);
    glUniform1f(ImageRoundingLoc, std::max(0.0f, rounding));
    glUniform4f(ImageTintLoc, tint.r, tint.g, tint.b, tint.a);
    glBindVertexArray(CompositeVAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureId);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}

RectBounds Renderer::MeasurePolygonBounds(const std::vector<Point2>& points, float strokeWidth) {
    return ComputePolygonBounds(points, strokeWidth);
}

void Renderer::DrawPolygon(const std::vector<Point2>& points, const Color& fillColor,
                           float strokeWidth, const Color& strokeColor) {
    DrawPolygon(points, fillColor, RectGradient{}, strokeWidth, strokeColor);
}

void Renderer::DrawPolygon(const std::vector<Point2>& points, const Color& fillColor, const RectGradient& gradient,
                           float strokeWidth, const Color& strokeColor) {
    if (points.size() < 3) {
        return;
    }

    const std::vector<Point2> triangles = TriangulatePolygon(points);
    if (triangles.empty()) {
        return;
    }

    RectBounds fillBounds = ComputePolygonBounds(points, 0.0f);
    const float invW = fillBounds.w > 0.0001f ? (1.0f / fillBounds.w) : 0.0f;
    const float invH = fillBounds.h > 0.0001f ? (1.0f / fillBounds.h) : 0.0f;
    std::vector<float> triangleVertices;
    triangleVertices.reserve(triangles.size() * 4);
    for (const Point2& point : triangles) {
        triangleVertices.push_back(point.x);
        triangleVertices.push_back(point.y);
        triangleVertices.push_back((point.x - fillBounds.x) * invW);
        triangleVertices.push_back((point.y - fillBounds.y) * invH);
    }

    if (CurrentActiveProgram != PolygonProgram) {
        glUseProgram(PolygonProgram);
        CurrentActiveProgram = PolygonProgram;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUniform4f(PolygonColorLoc, fillColor.r, fillColor.g, fillColor.b, fillColor.a);
    glUniform1i(PolygonGradientEnabledLoc, gradient.enabled ? 1 : 0);
    glUniform4f(PolygonGradientTopLeftLoc, gradient.topLeft.r, gradient.topLeft.g, gradient.topLeft.b, gradient.topLeft.a);
    glUniform4f(PolygonGradientTopRightLoc, gradient.topRight.r, gradient.topRight.g, gradient.topRight.b, gradient.topRight.a);
    glUniform4f(PolygonGradientBottomLeftLoc, gradient.bottomLeft.r, gradient.bottomLeft.g, gradient.bottomLeft.b, gradient.bottomLeft.a);
    glUniform4f(PolygonGradientBottomRightLoc, gradient.bottomRight.r, gradient.bottomRight.g, gradient.bottomRight.b, gradient.bottomRight.a);
    glBindVertexArray(PolygonVAO);
    glBindBuffer(GL_ARRAY_BUFFER, PolygonVBO);
    const GLsizeiptr triangleBytes = static_cast<GLsizeiptr>(sizeof(float) * triangleVertices.size());
    if (triangleBytes > PolygonVBOCapacity) {
        PolygonVBOCapacity = triangleBytes;
        glBufferData(GL_ARRAY_BUFFER, PolygonVBOCapacity, nullptr, GL_DYNAMIC_DRAW);
    }
    glBufferSubData(GL_ARRAY_BUFFER, 0, triangleBytes, triangleVertices.data());
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(triangles.size()));

    if (strokeWidth > 0.0f && strokeColor.a > 0.0f) {
        std::vector<float> outlineVertices;
        outlineVertices.reserve(points.size() * 4);
        for (const Point2& point : points) {
            outlineVertices.push_back(point.x);
            outlineVertices.push_back(point.y);
            outlineVertices.push_back(0.0f);
            outlineVertices.push_back(0.0f);
        }

        glUniform4f(PolygonColorLoc, strokeColor.r, strokeColor.g, strokeColor.b, strokeColor.a);
        glUniform1i(PolygonGradientEnabledLoc, 0);
        const GLsizeiptr outlineBytes = static_cast<GLsizeiptr>(sizeof(float) * outlineVertices.size());
        if (outlineBytes > PolygonVBOCapacity) {
            PolygonVBOCapacity = outlineBytes;
            glBufferData(GL_ARRAY_BUFFER, PolygonVBOCapacity, nullptr, GL_DYNAMIC_DRAW);
        }
        glBufferSubData(GL_ARRAY_BUFFER, 0, outlineBytes, outlineVertices.data());
        glLineWidth(std::max(1.0f, strokeWidth));
        glDrawArrays(GL_LINE_LOOP, 0, static_cast<GLsizei>(points.size()));
        glLineWidth(1.0f);
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

bool Renderer::RegisterFontSource(const std::string& fontPath, float fontSize, bool useSdf) {
    return RegisterFontSourceInternal(fontPath, fontSize, useSdf, false) != nullptr;
}

bool Renderer::LoadFont(const std::string& fontPath, float fontSize, unsigned int startChar, unsigned int endChar,
                        bool useSdf) {
    FontSource* source = RegisterFontSourceInternal(fontPath, fontSize, useSdf, true);
    if (!source) {
        return false;
    }

    bool loadedAny = startChar >= endChar;
    for (unsigned int c = startChar; c < endChar; ++c) {
        loadedAny = LoadCharacterFromSource(*source, c) || loadedAny;
    }
    return loadedAny;
}

void Renderer::DrawTextStr(const std::string& text, float x, float y, const Color& color, float scale,
                           float rotationDegrees, float pivotX, float pivotY, bool useCustomPivot) {
    if (text.empty()) {
        return;
    }

    float finalPivotX = pivotX;
    float finalPivotY = pivotY;
    const bool needsRotation = std::abs(rotationDegrees) > 0.001f;
    if (!needsRotation) {
        x = std::round(x);
        y = std::round(y);
    }
    if (needsRotation || useCustomPivot) {
        if (!useCustomPivot) {
            const RectFrame bounds = MeasureTextBounds(text, scale);
            const float boundsX = x + bounds.x;
            const float boundsY = y + bounds.y;
            const float boundsW = std::max(bounds.width, 24.0f * scale);
            const float boundsH = std::max(bounds.height, 32.0f * scale);
            finalPivotX = boundsX + boundsW * 0.5f;
            finalPivotY = boundsY + boundsH * 0.5f;
        }
    }

    if (CurrentActiveProgram != TextShaderProgram) {
        glUseProgram(TextShaderProgram);
        CurrentActiveProgram = TextShaderProgram;
    }

    glUniform4f(TextColorLoc, color.r, color.g, color.b, color.a);
    const float requestedPixelSize = ResolveRequestedTextPixelSize(scale);
    const float smallTextBlend = ResolveSmallTextBlend(requestedPixelSize);
    const float dynamicSdfSoftness = kTextSdfSoftness + (kTextSdfSmallSoftness - kTextSdfSoftness) * smallTextBlend;
    const float dynamicSdfBias = kTextSdfBias + (kTextSdfSmallBias - kTextSdfBias) * smallTextBlend;
    const float dynamicSdfMinWidth = kTextSdfMinWidth + (kTextSdfSmallMinWidth - kTextSdfMinWidth) * smallTextBlend;
    const float dynamicSdfDiscardThreshold = kTextSdfDiscardThreshold + (kTextSdfSmallDiscardThreshold - kTextSdfDiscardThreshold) * smallTextBlend;
    glUniform1f(TextSdfSoftnessLoc, dynamicSdfSoftness);
    glUniform1f(TextSdfBiasLoc, dynamicSdfBias);
    glUniform1f(TextSdfMinWidthLoc, dynamicSdfMinWidth);
    glUniform1f(TextSdfDiscardThresholdLoc, dynamicSdfDiscardThreshold);
    glUniform1f(TextSdfSmallTextBlendLoc, smallTextBlend);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(TextVAO);
    glBindBuffer(GL_ARRAY_BUFFER, TextVBO);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    int currentTextMode = -1;
    GLuint currentTexture = 0;
    size_t i = 0;
    while (i < text.length()) {
        unsigned int c = 0;
        if (!DecodeUtf8Codepoint(text, i, c)) {
            continue;
        }

        if (c == ' ') {
            x += ResolveRequestedTextPixelSize(scale) * 0.3f;
            continue;
        }

        if (!EnsureCharacterLoaded(c)) continue;
        const Character& charData = Characters[c];
        const float glyphScale = ResolveCharacterScale(charData, scale);
        const int textMode = charData.IsSdf ? 1 : 0;
        if (textMode != currentTextMode) {
            glUniform1i(TextModeLoc, textMode);
            currentTextMode = textMode;
        }

        const float xpos = x + charData.RenderBearing[0] * glyphScale;
        const float ypos = y + charData.RenderBearing[1] * glyphScale;
        const float w = charData.RenderSize[0] * glyphScale;
        const float h = charData.RenderSize[1] * glyphScale;

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

        if (charData.TextureID != currentTexture) {
            glBindTexture(GL_TEXTURE_2D, charData.TextureID);
            currentTexture = charData.TextureID;
        }
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        x += charData.Advance * glyphScale;
    }
}

RectFrame Renderer::MeasureTextBounds(const std::string& text, float scale) {
    if (text.empty()) {
        return RectFrame{};
    }

    const TextWidthCacheKey cacheKey{text, MakeTextScaleKey(scale)};
    auto cacheIt = TextBoundsCache.find(cacheKey);
    if (cacheIt != TextBoundsCache.end()) {
        return cacheIt->second;
    }

    float penX = 0.0f;
    bool hasGlyphBounds = false;
    float minX = 0.0f;
    float minY = 0.0f;
    float maxX = 0.0f;
    float maxY = 0.0f;

    size_t index = 0;
    while (index < text.length()) {
        unsigned int c = 0;
        if (!DecodeUtf8Codepoint(text, index, c)) {
            continue;
        }

        if (c == ' ') {
            penX += ResolveRequestedTextPixelSize(scale) * 0.3f;
            continue;
        }

        if (!EnsureCharacterLoaded(c)) {
            continue;
        }

        const Character& charData = Characters[c];
        const float glyphScale = ResolveCharacterScale(charData, scale);
        const float glyphX = penX + charData.VisibleBearing[0] * glyphScale;
        const float glyphY = charData.VisibleBearing[1] * glyphScale;
        const float glyphW = charData.VisibleSize[0] * glyphScale;
        const float glyphH = charData.VisibleSize[1] * glyphScale;

        if (glyphW > 0.0f && glyphH > 0.0f) {
            if (!hasGlyphBounds) {
                minX = glyphX;
                minY = glyphY;
                maxX = glyphX + glyphW;
                maxY = glyphY + glyphH;
                hasGlyphBounds = true;
            } else {
                minX = std::min(minX, glyphX);
                minY = std::min(minY, glyphY);
                maxX = std::max(maxX, glyphX + glyphW);
                maxY = std::max(maxY, glyphY + glyphH);
            }
        }

        penX += charData.Advance * glyphScale;
    }

    if (!hasGlyphBounds) {
        const RectFrame result{0.0f, 0.0f, penX, 0.0f};
        if (TextBoundsCache.size() > 4096) {
            TextBoundsCache.clear();
        }
        TextBoundsCache.emplace(cacheKey, result);
        return result;
    }

    const RectFrame result{minX, minY, maxX - minX, maxY - minY};
    if (TextBoundsCache.size() > 4096) {
        TextBoundsCache.clear();
    }
    TextBoundsCache.emplace(cacheKey, result);
    return result;
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
        if (!DecodeUtf8Codepoint(text, i, c)) {
            continue;
        }

        if (EnsureCharacterLoaded(c)) {
            const Character& charData = Characters[c];
            width += charData.Advance * ResolveCharacterScale(charData, scale);
        } else if (c == ' ') {
            width += ResolveRequestedTextPixelSize(scale) * 0.3f;
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

void Renderer::CaptureBackdrop() {
    if (!BackdropCapturePending) {
        return;
    }

    const int texW = std::max(1, (int)State.framebufferW);
    const int texH = std::max(1, (int)State.framebufferH);
    EnsureBackdropTexture(texW, texH);
    glBindTexture(GL_TEXTURE_2D, BgTexture);
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, texW, texH);
    BackdropCapturePending = false;
}

bool Renderer::ShouldRepaint() {
    if (RemoteImageReadyFlag.exchange(false)) {
        State.needsRepaint = true;
        if (State.animationTimeLeft < 0.12f) {
            State.animationTimeLeft = 0.12f;
        }
    }
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
