#include "core/render/opengl/opengl_backend.h"

#include "core/window/window_backend.h"

#include <glad/glad.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace core::render::opengl {

namespace {

struct PrimitiveResources {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint shaderProgram = 0;
    GLint windowSizeLocation = -1;
    GLint fillColorLocation = -1;
    GLint gradientStartLocation = -1;
    GLint gradientEndLocation = -1;
    GLint borderColorLocation = -1;
    GLint shadowColorLocation = -1;
    GLint rectLocation = -1;
    GLint radiusLocation = -1;
    GLint borderWidthLocation = -1;
    GLint opacityLocation = -1;
    GLint shadowBlurLocation = -1;
    GLint blurAmountLocation = -1;
    GLint backdropRectLocation = -1;
    GLint useGradientLocation = -1;
    GLint gradientDirectionLocation = -1;
    GLint shadowPassLocation = -1;
    GLint insetShadowPassLocation = -1;
    GLint shadowOffsetLocation = -1;
    GLint shadowSpreadLocation = -1;
    GLint backdropLocation = -1;
    GLuint backdropTexture = 0;
    GLuint backdropFramebuffer = 0;
    int backdropX = 0;
    int backdropY = 0;
    int backdropWidth = 0;
    int backdropHeight = 0;
    int backdropTextureWidth = 0;
    int backdropTextureHeight = 0;
};

struct RoundedRectBatchResources {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint shaderProgram = 0;
    GLint windowSizeLocation = -1;
};

constexpr std::size_t kRoundedRectBatchVertexFloatCount = 36;
constexpr std::size_t kRoundedRectBatchMaxVertices = 6u * 2048u;

constexpr int kMaxPolygonEdges = 128;

struct PolygonResources {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint shaderProgram = 0;
    GLint windowSizeLocation = -1;
    GLint fillColorLocation = -1;
    GLint opacityLocation = -1;
    GLint edgeCountLocation = -1;
    GLint edgesLocation = -1;
};

PrimitiveResources& primitiveResources() {
    static std::unordered_map<window::ContextKey, PrimitiveResources> resourcesByContext;
    return resourcesByContext[window::currentContextKey()];
}

RoundedRectBatchResources& roundedRectBatchResources() {
    static std::unordered_map<window::ContextKey, RoundedRectBatchResources> resourcesByContext;
    return resourcesByContext[window::currentContextKey()];
}

PolygonResources& polygonResources() {
    static std::unordered_map<window::ContextKey, PolygonResources> resourcesByContext;
    return resourcesByContext[window::currentContextKey()];
}

GLuint compileShader(GLenum type, const char* source) {
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

void releaseResources(PrimitiveResources& resources) {
    if (resources.vbo) {
        glDeleteBuffers(1, &resources.vbo);
    }
    if (resources.vao) {
        glDeleteVertexArrays(1, &resources.vao);
    }
    if (resources.shaderProgram) {
        glDeleteProgram(resources.shaderProgram);
    }
    if (resources.backdropTexture) {
        glDeleteTextures(1, &resources.backdropTexture);
    }
    if (resources.backdropFramebuffer) {
        glDeleteFramebuffers(1, &resources.backdropFramebuffer);
    }
    resources = {};
}

void releaseResources(RoundedRectBatchResources& resources) {
    if (resources.vbo) {
        glDeleteBuffers(1, &resources.vbo);
    }
    if (resources.vao) {
        glDeleteVertexArrays(1, &resources.vao);
    }
    if (resources.shaderProgram) {
        glDeleteProgram(resources.shaderProgram);
    }
    resources = {};
}

void releaseResources(PolygonResources& resources) {
    if (resources.vbo) {
        glDeleteBuffers(1, &resources.vbo);
    }
    if (resources.vao) {
        glDeleteVertexArrays(1, &resources.vao);
    }
    if (resources.shaderProgram) {
        glDeleteProgram(resources.shaderProgram);
    }
    resources = {};
}

bool ensurePrimitiveResources() {
    PrimitiveResources& resources = primitiveResources();
    if (resources.shaderProgram != 0 && resources.vao != 0 && resources.vbo != 0) {
        return true;
    }

    const char* vertexSource =
        "#version 330 core\n"
        "layout(location = 0) in vec3 aScreenPos;\n"
        "layout(location = 1) in vec2 aLocalPos;\n"
        "uniform vec2 uWindowSize;\n"
        "out vec2 vLocalPos;\n"
        "void main() {\n"
        "    vLocalPos = aLocalPos;\n"
        "    vec2 ndc = vec2((aScreenPos.x / uWindowSize.x) * 2.0 - 1.0,\n"
        "                    1.0 - (aScreenPos.y / uWindowSize.y) * 2.0);\n"
        "    gl_Position = vec4(ndc * aScreenPos.z, 0.0, aScreenPos.z);\n"
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
        "uniform vec2 uWindowSize;\n"
        "uniform vec4 uRect;\n"
        "uniform float uRadius;\n"
        "uniform float uBorderWidth;\n"
        "uniform float uOpacity;\n"
        "uniform float uShadowBlur;\n"
        "uniform float uBlurAmount;\n"
        "uniform vec4 uBackdropRect;\n"
        "uniform int uUseGradient;\n"
        "uniform int uGradientDirection;\n"
        "uniform int uShadowPass;\n"
        "uniform int uInsetShadowPass;\n"
        "uniform vec2 uShadowOffset;\n"
        "uniform float uShadowSpread;\n"
        "uniform sampler2D uBackdrop;\n"
        "float rand(vec2 co) {\n"
        "    return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);\n"
        "}\n"
        "float roundedBoxDistance(vec2 point, vec2 halfSize, float radius) {\n"
        "    vec2 cornerVector = abs(point) - halfSize + vec2(radius);\n"
        "    return length(max(cornerVector, 0.0)) + min(max(cornerVector.x, cornerVector.y), 0.0) - radius;\n"
        "}\n"
        "vec3 backdropBlur(vec2 uv) {\n"
        "    vec2 pixelStep = 1.0 / max(uBackdropRect.zw, vec2(1.0));\n"
        "    float blurRadiusPx = uBlurAmount;\n"
        "    vec3 blurred = texture(uBackdrop, uv).rgb;\n"
        "    float repeats = mix(8.0, 24.0, clamp(blurRadiusPx / 36.0, 0.0, 1.0));\n"
        "    const float tau = 6.28318530718;\n"
        "    for (float i = 0.0; i < 24.0; i += 1.0) {\n"
        "        if (i >= repeats) break;\n"
        "        float angle = (i / repeats) * tau;\n"
        "        vec2 dir = vec2(cos(angle), sin(angle));\n"
        "        float radiusA = blurRadiusPx * (0.35 + 0.65 * rand(vec2(i, uv.x + uv.y)));\n"
        "        vec2 uvA = clamp(uv + dir * radiusA * pixelStep, pixelStep * 0.5, vec2(1.0) - pixelStep * 0.5);\n"
        "        blurred += texture(uBackdrop, uvA).rgb;\n"
        "        float angleB = angle + (0.5 * tau / repeats);\n"
        "        vec2 dirB = vec2(cos(angleB), sin(angleB));\n"
        "        float radiusB = blurRadiusPx * (0.20 + 0.80 * rand(vec2(i + 2.0, uv.x + uv.y + 24.0)));\n"
        "        vec2 uvB = clamp(uv + dirB * radiusB * pixelStep, pixelStep * 0.5, vec2(1.0) - pixelStep * 0.5);\n"
        "        blurred += texture(uBackdrop, uvB).rgb;\n"
        "    }\n"
        "    return blurred / (repeats * 2.0 + 1.0);\n"
        "}\n"
        "void main() {\n"
        "    vec2 center = uRect.xy + uRect.zw * 0.5;\n"
        "    float distanceToEdge = roundedBoxDistance(vLocalPos - center, uRect.zw * 0.5, uRadius);\n"
        "    float blur = max(uShadowBlur, 1.0);\n"
        "    if (uShadowPass == 1) {\n"
        "        if (uInsetShadowPass == 1) {\n"
        "            float edgeWidth = max(fwidth(distanceToEdge), 0.75);\n"
        "            float shapeAlpha = 1.0 - smoothstep(-edgeWidth, edgeWidth, distanceToEdge);\n"
        "            if (shapeAlpha <= 0.0) discard;\n"
        "            vec2 sideVector = dot(uShadowOffset, uShadowOffset) <= 0.0001 ? vec2(0.0, 1.0) : normalize(-uShadowOffset);\n"
        "            vec2 localUnit = (vLocalPos - center) / max(uRect.zw * 0.5, vec2(1.0));\n"
        "            float sideMask = clamp(0.34 + dot(localUnit, sideVector) * 0.66, 0.0, 1.0);\n"
        "            float spreadBias = max(uShadowSpread, 0.0);\n"
        "            float edgeFalloff = smoothstep(-blur - spreadBias, 0.0, distanceToEdge);\n"
        "            float innerAlpha = edgeFalloff * sideMask;\n"
        "            if (innerAlpha <= 0.0) discard;\n"
        "            FragColor = vec4(uShadowColor.rgb, uShadowColor.a * innerAlpha * shapeAlpha * uOpacity);\n"
        "            return;\n"
        "        }\n"
        "        float shadowAlpha = 1.0 - smoothstep(-blur, blur, distanceToEdge);\n"
        "        if (shadowAlpha <= 0.0) discard;\n"
        "        FragColor = vec4(uShadowColor.rgb, uShadowColor.a * shadowAlpha * uOpacity);\n"
        "        return;\n"
        "    }\n"
        "    float edgeWidth = max(fwidth(distanceToEdge), 0.75);\n"
        "    float shapeAlpha = 1.0 - smoothstep(-edgeWidth, edgeWidth, distanceToEdge);\n"
        "    if (shapeAlpha <= 0.0) discard;\n"
        "    float gradientAmount = uGradientDirection == 0 ?\n"
        "        clamp((vLocalPos.x - uRect.x) / max(uRect.z, 1.0), 0.0, 1.0) :\n"
        "        clamp((vLocalPos.y - uRect.y) / max(uRect.w, 1.0), 0.0, 1.0);\n"
        "    vec4 fill = uUseGradient == 1 ? mix(uGradientStart, uGradientEnd, gradientAmount) : uFillColor;\n"
        "    if (uBlurAmount > 0.0) {\n"
        "        vec2 backdropUv = (gl_FragCoord.xy - uBackdropRect.xy) / max(uBackdropRect.zw, vec2(1.0));\n"
        "        backdropUv = clamp(backdropUv, vec2(0.0), vec2(1.0));\n"
        "        vec3 blurred = backdropBlur(backdropUv);\n"
        "        fill = vec4(mix(blurred, fill.rgb, fill.a), 1.0);\n"
        "    }\n"
        "    float borderAlpha = uBorderWidth > 0.0 ? smoothstep(-uBorderWidth - edgeWidth, -uBorderWidth + edgeWidth, distanceToEdge) : 0.0;\n"
        "    vec4 color = mix(fill, uBorderColor, borderAlpha);\n"
        "    FragColor = vec4(color.rgb, color.a * shapeAlpha * uOpacity);\n"
        "}\n";

    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (!vertexShader || !fragmentShader) {
        if (vertexShader) {
            glDeleteShader(vertexShader);
        }
        if (fragmentShader) {
            glDeleteShader(fragmentShader);
        }
        return false;
    }

    resources.shaderProgram = glCreateProgram();
    glAttachShader(resources.shaderProgram, vertexShader);
    glAttachShader(resources.shaderProgram, fragmentShader);
    glLinkProgram(resources.shaderProgram);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLint linked = 0;
    glGetProgramiv(resources.shaderProgram, GL_LINK_STATUS, &linked);
    if (!linked) {
        releaseResources(resources);
        return false;
    }

    resources.windowSizeLocation = glGetUniformLocation(resources.shaderProgram, "uWindowSize");
    resources.fillColorLocation = glGetUniformLocation(resources.shaderProgram, "uFillColor");
    resources.gradientStartLocation = glGetUniformLocation(resources.shaderProgram, "uGradientStart");
    resources.gradientEndLocation = glGetUniformLocation(resources.shaderProgram, "uGradientEnd");
    resources.borderColorLocation = glGetUniformLocation(resources.shaderProgram, "uBorderColor");
    resources.shadowColorLocation = glGetUniformLocation(resources.shaderProgram, "uShadowColor");
    resources.rectLocation = glGetUniformLocation(resources.shaderProgram, "uRect");
    resources.radiusLocation = glGetUniformLocation(resources.shaderProgram, "uRadius");
    resources.borderWidthLocation = glGetUniformLocation(resources.shaderProgram, "uBorderWidth");
    resources.opacityLocation = glGetUniformLocation(resources.shaderProgram, "uOpacity");
    resources.shadowBlurLocation = glGetUniformLocation(resources.shaderProgram, "uShadowBlur");
    resources.blurAmountLocation = glGetUniformLocation(resources.shaderProgram, "uBlurAmount");
    resources.backdropRectLocation = glGetUniformLocation(resources.shaderProgram, "uBackdropRect");
    resources.useGradientLocation = glGetUniformLocation(resources.shaderProgram, "uUseGradient");
    resources.gradientDirectionLocation = glGetUniformLocation(resources.shaderProgram, "uGradientDirection");
    resources.shadowPassLocation = glGetUniformLocation(resources.shaderProgram, "uShadowPass");
    resources.insetShadowPassLocation = glGetUniformLocation(resources.shaderProgram, "uInsetShadowPass");
    resources.shadowOffsetLocation = glGetUniformLocation(resources.shaderProgram, "uShadowOffset");
    resources.shadowSpreadLocation = glGetUniformLocation(resources.shaderProgram, "uShadowSpread");
    resources.backdropLocation = glGetUniformLocation(resources.shaderProgram, "uBackdrop");

    glGenVertexArrays(1, &resources.vao);
    glGenBuffers(1, &resources.vbo);
    glBindVertexArray(resources.vao);
    glBindBuffer(GL_ARRAY_BUFFER, resources.vbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(PrimitiveGeometryVertex), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(PrimitiveGeometryVertex), reinterpret_cast<void*>(offsetof(PrimitiveGeometryVertex, local)));
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return resources.shaderProgram != 0 && resources.vao != 0 && resources.vbo != 0;
}

bool ensureRoundedRectBatchResources() {
    RoundedRectBatchResources& resources = roundedRectBatchResources();
    if (resources.shaderProgram != 0 && resources.vao != 0 && resources.vbo != 0) {
        return true;
    }

    const char* vertexSource =
        "#version 330 core\n"
        "layout(location = 0) in vec3 aScreenPos;\n"
        "layout(location = 1) in vec2 aLocalPos;\n"
        "layout(location = 2) in vec4 aFillColor;\n"
        "layout(location = 3) in vec4 aGradientStart;\n"
        "layout(location = 4) in vec4 aGradientEnd;\n"
        "layout(location = 5) in vec4 aBorderColor;\n"
        "layout(location = 6) in vec4 aRect;\n"
        "layout(location = 7) in vec4 aShapeParams;\n"
        "layout(location = 8) in vec4 aModeParams;\n"
        "layout(location = 9) in vec3 aShadowParams;\n"
        "uniform vec2 uWindowSize;\n"
        "out vec2 vLocalPos;\n"
        "flat out vec4 vFillColor;\n"
        "flat out vec4 vGradientStart;\n"
        "flat out vec4 vGradientEnd;\n"
        "flat out vec4 vBorderColor;\n"
        "flat out vec4 vRect;\n"
        "flat out vec4 vShapeParams;\n"
        "flat out vec4 vModeParams;\n"
        "flat out vec3 vShadowParams;\n"
        "void main() {\n"
        "    vLocalPos = aLocalPos;\n"
        "    vFillColor = aFillColor;\n"
        "    vGradientStart = aGradientStart;\n"
        "    vGradientEnd = aGradientEnd;\n"
        "    vBorderColor = aBorderColor;\n"
        "    vRect = aRect;\n"
        "    vShapeParams = aShapeParams;\n"
        "    vModeParams = aModeParams;\n"
        "    vShadowParams = aShadowParams;\n"
        "    vec2 ndc = vec2((aScreenPos.x / uWindowSize.x) * 2.0 - 1.0,\n"
        "                    1.0 - (aScreenPos.y / uWindowSize.y) * 2.0);\n"
        "    gl_Position = vec4(ndc * aScreenPos.z, 0.0, aScreenPos.z);\n"
        "}\n";

    const char* fragmentSource =
        "#version 330 core\n"
        "in vec2 vLocalPos;\n"
        "flat in vec4 vFillColor;\n"
        "flat in vec4 vGradientStart;\n"
        "flat in vec4 vGradientEnd;\n"
        "flat in vec4 vBorderColor;\n"
        "flat in vec4 vRect;\n"
        "flat in vec4 vShapeParams;\n"
        "flat in vec4 vModeParams;\n"
        "flat in vec3 vShadowParams;\n"
        "out vec4 FragColor;\n"
        "float roundedBoxDistance(vec2 point, vec2 halfSize, float radius) {\n"
        "    vec2 cornerVector = abs(point) - halfSize + vec2(radius);\n"
        "    return length(max(cornerVector, 0.0)) + min(max(cornerVector.x, cornerVector.y), 0.0) - radius;\n"
        "}\n"
        "void main() {\n"
        "    float radius = vShapeParams.x;\n"
        "    float borderWidth = vShapeParams.y;\n"
        "    float opacity = vShapeParams.z;\n"
        "    float shadowBlur = max(vShapeParams.w, 1.0);\n"
        "    bool useGradient = vModeParams.x > 0.5;\n"
        "    bool verticalGradient = vModeParams.y > 0.5;\n"
        "    bool shadowPass = vModeParams.z > 0.5;\n"
        "    bool insetShadowPass = vModeParams.w > 0.5;\n"
        "    vec2 center = vRect.xy + vRect.zw * 0.5;\n"
        "    float distanceToEdge = roundedBoxDistance(vLocalPos - center, vRect.zw * 0.5, radius);\n"
        "    if (shadowPass) {\n"
        "        if (insetShadowPass) {\n"
        "            float edgeWidth = max(fwidth(distanceToEdge), 0.75);\n"
        "            float shapeAlpha = 1.0 - smoothstep(-edgeWidth, edgeWidth, distanceToEdge);\n"
        "            if (shapeAlpha <= 0.0) discard;\n"
        "            vec2 offset = vShadowParams.xy;\n"
        "            vec2 sideVector = dot(offset, offset) <= 0.0001 ? vec2(0.0, 1.0) : normalize(-offset);\n"
        "            vec2 localUnit = (vLocalPos - center) / max(vRect.zw * 0.5, vec2(1.0));\n"
        "            float sideMask = clamp(0.34 + dot(localUnit, sideVector) * 0.66, 0.0, 1.0);\n"
        "            float spreadBias = max(vShadowParams.z, 0.0);\n"
        "            float edgeFalloff = smoothstep(-shadowBlur - spreadBias, 0.0, distanceToEdge);\n"
        "            float innerAlpha = edgeFalloff * sideMask;\n"
        "            if (innerAlpha <= 0.0) discard;\n"
        "            FragColor = vec4(vFillColor.rgb, vFillColor.a * innerAlpha * shapeAlpha * opacity);\n"
        "            return;\n"
        "        }\n"
        "        float shadowAlpha = 1.0 - smoothstep(-shadowBlur, shadowBlur, distanceToEdge);\n"
        "        if (shadowAlpha <= 0.0) discard;\n"
        "        FragColor = vec4(vFillColor.rgb, vFillColor.a * shadowAlpha * opacity);\n"
        "        return;\n"
        "    }\n"
        "    float edgeWidth = max(fwidth(distanceToEdge), 0.75);\n"
        "    float shapeAlpha = 1.0 - smoothstep(-edgeWidth, edgeWidth, distanceToEdge);\n"
        "    if (shapeAlpha <= 0.0) discard;\n"
        "    float gradientAmount = verticalGradient ?\n"
        "        clamp((vLocalPos.y - vRect.y) / max(vRect.w, 1.0), 0.0, 1.0) :\n"
        "        clamp((vLocalPos.x - vRect.x) / max(vRect.z, 1.0), 0.0, 1.0);\n"
        "    vec4 fill = useGradient ? mix(vGradientStart, vGradientEnd, gradientAmount) : vFillColor;\n"
        "    float borderAlpha = borderWidth > 0.0 ? smoothstep(-borderWidth - edgeWidth, -borderWidth + edgeWidth, distanceToEdge) : 0.0;\n"
        "    vec4 color = mix(fill, vBorderColor, borderAlpha);\n"
        "    FragColor = vec4(color.rgb, color.a * shapeAlpha * opacity);\n"
        "}\n";

    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (!vertexShader || !fragmentShader) {
        if (vertexShader) glDeleteShader(vertexShader);
        if (fragmentShader) glDeleteShader(fragmentShader);
        return false;
    }

    resources.shaderProgram = glCreateProgram();
    glAttachShader(resources.shaderProgram, vertexShader);
    glAttachShader(resources.shaderProgram, fragmentShader);
    glLinkProgram(resources.shaderProgram);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLint linked = 0;
    glGetProgramiv(resources.shaderProgram, GL_LINK_STATUS, &linked);
    if (!linked) {
        releaseResources(resources);
        return false;
    }

    resources.windowSizeLocation = glGetUniformLocation(resources.shaderProgram, "uWindowSize");
    glGenVertexArrays(1, &resources.vao);
    glGenBuffers(1, &resources.vbo);
    glBindVertexArray(resources.vao);
    glBindBuffer(GL_ARRAY_BUFFER, resources.vbo);
    const GLsizei stride = static_cast<GLsizei>(sizeof(float) * kRoundedRectBatchVertexFloatCount);
    const auto attribute = [stride](GLuint location, GLint size, std::size_t offset) {
        glVertexAttribPointer(location,
                              size,
                              GL_FLOAT,
                              GL_FALSE,
                              stride,
                              reinterpret_cast<void*>(sizeof(float) * offset));
        glEnableVertexAttribArray(location);
    };
    attribute(0, 3, 0);
    attribute(1, 2, 3);
    attribute(2, 4, 5);
    attribute(3, 4, 9);
    attribute(4, 4, 13);
    attribute(5, 4, 17);
    attribute(6, 4, 21);
    attribute(7, 4, 25);
    attribute(8, 4, 29);
    attribute(9, 3, 33);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    return resources.shaderProgram != 0 && resources.vao != 0 && resources.vbo != 0;
}

bool ensurePolygonResources() {
    PolygonResources& resources = polygonResources();
    if (resources.shaderProgram != 0 && resources.vao != 0 && resources.vbo != 0) {
        return true;
    }

    const char* vertexSource =
        "#version 330 core\n"
        "layout(location = 0) in vec3 aScreenPos;\n"
        "layout(location = 1) in vec2 aLocalPos;\n"
        "uniform vec2 uWindowSize;\n"
        "out vec2 vLocalPos;\n"
        "void main() {\n"
        "    vLocalPos = aLocalPos;\n"
        "    vec2 ndc = vec2((aScreenPos.x / uWindowSize.x) * 2.0 - 1.0,\n"
        "                    1.0 - (aScreenPos.y / uWindowSize.y) * 2.0);\n"
        "    gl_Position = vec4(ndc * aScreenPos.z, 0.0, aScreenPos.z);\n"
        "}\n";

    const char* fragmentSource =
        "#version 330 core\n"
        "#define MAX_POLYGON_EDGES 128\n"
        "in vec2 vLocalPos;\n"
        "out vec4 FragColor;\n"
        "uniform vec4 uFillColor;\n"
        "uniform float uOpacity;\n"
        "uniform int uEdgeCount;\n"
        "uniform vec4 uEdges[MAX_POLYGON_EDGES];\n"
        "float edgeDistance(vec2 p, vec2 a, vec2 b) {\n"
        "    vec2 ab = b - a;\n"
        "    float t = clamp(dot(p - a, ab) / max(dot(ab, ab), 0.0001), 0.0, 1.0);\n"
        "    return length(p - (a + ab * t));\n"
        "}\n"
        "bool polygonContains(vec2 p) {\n"
        "    bool inside = false;\n"
        "    for (int i = 0; i < MAX_POLYGON_EDGES; ++i) {\n"
        "        if (i >= uEdgeCount) break;\n"
        "        vec4 edge = uEdges[i];\n"
        "        vec2 a = edge.xy;\n"
        "        vec2 b = edge.zw;\n"
        "        bool crosses = ((a.y > p.y) != (b.y > p.y));\n"
        "        if (crosses) {\n"
        "            float xAtY = (b.x - a.x) * (p.y - a.y) / (b.y - a.y) + a.x;\n"
        "            if (p.x < xAtY) {\n"
        "                inside = !inside;\n"
        "            }\n"
        "        }\n"
        "    }\n"
        "    return inside;\n"
        "}\n"
        "void main() {\n"
        "    bool inside = polygonContains(vLocalPos);\n"
        "    float minDistance = 1000000.0;\n"
        "    for (int i = 0; i < MAX_POLYGON_EDGES; ++i) {\n"
        "        if (i >= uEdgeCount) break;\n"
        "        vec4 edge = uEdges[i];\n"
        "        minDistance = min(minDistance, edgeDistance(vLocalPos, edge.xy, edge.zw));\n"
        "    }\n"
        "    float signedDistance = inside ? -minDistance : minDistance;\n"
        "    float edgeWidth = max(fwidth(signedDistance), 0.75);\n"
        "    float shapeAlpha = 1.0 - smoothstep(-edgeWidth, edgeWidth, signedDistance);\n"
        "    if (shapeAlpha <= 0.0) discard;\n"
        "    FragColor = vec4(uFillColor.rgb, uFillColor.a * shapeAlpha * uOpacity);\n"
        "}\n";

    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (!vertexShader || !fragmentShader) {
        if (vertexShader) {
            glDeleteShader(vertexShader);
        }
        if (fragmentShader) {
            glDeleteShader(fragmentShader);
        }
        return false;
    }

    resources.shaderProgram = glCreateProgram();
    glAttachShader(resources.shaderProgram, vertexShader);
    glAttachShader(resources.shaderProgram, fragmentShader);
    glLinkProgram(resources.shaderProgram);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLint linked = 0;
    glGetProgramiv(resources.shaderProgram, GL_LINK_STATUS, &linked);
    if (!linked) {
        releaseResources(resources);
        return false;
    }

    resources.windowSizeLocation = glGetUniformLocation(resources.shaderProgram, "uWindowSize");
    resources.fillColorLocation = glGetUniformLocation(resources.shaderProgram, "uFillColor");
    resources.opacityLocation = glGetUniformLocation(resources.shaderProgram, "uOpacity");
    resources.edgeCountLocation = glGetUniformLocation(resources.shaderProgram, "uEdgeCount");
    resources.edgesLocation = glGetUniformLocation(resources.shaderProgram, "uEdges[0]");

    glGenVertexArrays(1, &resources.vao);
    glGenBuffers(1, &resources.vbo);
    glBindVertexArray(resources.vao);
    glBindBuffer(GL_ARRAY_BUFFER, resources.vbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(PrimitiveGeometryVertex), nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(PrimitiveGeometryVertex), reinterpret_cast<void*>(offsetof(PrimitiveGeometryVertex, local)));
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return resources.shaderProgram != 0 && resources.vao != 0 && resources.vbo != 0;
}

void ensureBackdropTexture(int width, int height) {
    PrimitiveResources& resources = primitiveResources();
    width = std::max(1, width);
    height = std::max(1, height);
    if (resources.backdropTexture != 0 &&
        resources.backdropTextureWidth == width &&
        resources.backdropTextureHeight == height) {
        return;
    }

    if (resources.backdropTexture == 0) {
        glGenTextures(1, &resources.backdropTexture);
    }
    resources.backdropTextureWidth = width;
    resources.backdropTextureHeight = height;
    glBindTexture(GL_TEXTURE_2D, resources.backdropTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
}

} // namespace

void OpenGLRenderBackend::prepareBackdropBlur(const core::Rect& bounds, float blur, int windowWidth, int windowHeight) {
    flushRoundedRectBatch();
    if (blur <= 0.0f || windowWidth <= 0 || windowHeight <= 0) {
        return;
    }

    const int safeWindowWidth = std::max(1, windowWidth);
    const int safeWindowHeight = std::max(1, windowHeight);
    const int left = std::clamp(static_cast<int>(std::floor(bounds.x - blur)), 0, safeWindowWidth - 1);
    const int top = std::clamp(static_cast<int>(std::floor(bounds.y - blur)), 0, safeWindowHeight - 1);
    const int right = std::clamp(static_cast<int>(std::ceil(bounds.x + bounds.width + blur)), left + 1, safeWindowWidth);
    const int bottom = std::clamp(static_cast<int>(std::ceil(bounds.y + bounds.height + blur)), top + 1, safeWindowHeight);
    const int captureWidth = right - left;
    const int captureHeight = bottom - top;
    const int sourceY = safeWindowHeight - bottom;
    constexpr float backdropScale = 0.5f;
    const int textureWidth = std::max(1, static_cast<int>(std::ceil(static_cast<float>(captureWidth) * backdropScale)));
    const int textureHeight = std::max(1, static_cast<int>(std::ceil(static_cast<float>(captureHeight) * backdropScale)));

    ensureBackdropTexture(textureWidth, textureHeight);
    PrimitiveResources& resources = primitiveResources();
    resources.backdropX = left;
    resources.backdropY = sourceY;
    resources.backdropWidth = captureWidth;
    resources.backdropHeight = captureHeight;

    if (resources.backdropFramebuffer == 0) {
        glGenFramebuffers(1, &resources.backdropFramebuffer);
    }

    GLint previousReadFramebuffer = 0;
    GLint previousDrawFramebuffer = 0;
    GLint previousTexture = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousReadFramebuffer);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousDrawFramebuffer);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
    const GLboolean scissorEnabled = glIsEnabled(GL_SCISSOR_TEST);

    glBindTexture(GL_TEXTURE_2D, resources.backdropTexture);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, previousDrawFramebuffer);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resources.backdropFramebuffer);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, resources.backdropTexture, 0);

    if (glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
        if (scissorEnabled) {
            glDisable(GL_SCISSOR_TEST);
        }
        glBlitFramebuffer(left, sourceY, right, sourceY + captureHeight,
                          0, 0, textureWidth, textureHeight,
                          GL_COLOR_BUFFER_BIT, GL_LINEAR);
        if (scissorEnabled) {
            glEnable(GL_SCISSOR_TEST);
        }
    }

    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(previousReadFramebuffer));
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(previousDrawFramebuffer));
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));
    resetStateCache();
}

void OpenGLRenderBackend::flushRoundedRectBatch() {
    if (roundedRectBatchVertices_.empty()) {
        return;
    }
    if (roundedRectBatchWindowWidth_ <= 0 ||
        roundedRectBatchWindowHeight_ <= 0 ||
        !ensureRoundedRectBatchResources()) {
        roundedRectBatchVertices_.clear();
        roundedRectBatchWindowWidth_ = 0;
        roundedRectBatchWindowHeight_ = 0;
        return;
    }

    RoundedRectBatchResources& resources = roundedRectBatchResources();
    setStandardAlphaBlend();
    useProgram(resources.shaderProgram);
    glUniform2f(resources.windowSizeLocation,
                static_cast<float>(roundedRectBatchWindowWidth_),
                static_cast<float>(roundedRectBatchWindowHeight_));
    bindVertexArray(resources.vao);
    bindArrayBuffer(resources.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(roundedRectBatchVertices_.size() * sizeof(float)),
                 roundedRectBatchVertices_.data(),
                 GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES,
                 0,
                 static_cast<GLsizei>(roundedRectBatchVertices_.size() /
                                      kRoundedRectBatchVertexFloatCount));
    roundedRectBatchVertices_.clear();
    roundedRectBatchWindowWidth_ = 0;
    roundedRectBatchWindowHeight_ = 0;
}

void OpenGLRenderBackend::drawRoundedRect(const RoundedRectDrawCommand& command, int windowWidth, int windowHeight) {
    if (command.vertices.empty() || windowWidth <= 0 || windowHeight <= 0 ||
        !roundedRectHasVisibleContent(command)) {
        return;
    }

    if (command.backdropBlur <= 0.001f) {
        if (!roundedRectBatchVertices_.empty() &&
            (roundedRectBatchWindowWidth_ != windowWidth ||
             roundedRectBatchWindowHeight_ != windowHeight)) {
            flushRoundedRectBatch();
        }
        RoundedRectBatchResources& batchResources = roundedRectBatchResources();
        const bool hadBatchResources =
            batchResources.shaderProgram != 0 &&
            batchResources.vao != 0 &&
            batchResources.vbo != 0;
        if (ensureRoundedRectBatchResources()) {
            if (!hadBatchResources) {
                resetStateCache();
            }
            const std::size_t currentVertexCount =
                roundedRectBatchVertices_.size() / kRoundedRectBatchVertexFloatCount;
            if (currentVertexCount + command.vertices.size() > kRoundedRectBatchMaxVertices) {
                flushRoundedRectBatch();
            }

            roundedRectBatchWindowWidth_ = windowWidth;
            roundedRectBatchWindowHeight_ = windowHeight;
            roundedRectBatchVertices_.reserve(
                roundedRectBatchVertices_.size() +
                command.vertices.size() * kRoundedRectBatchVertexFloatCount);
            const auto appendColor = [&](const core::Color& color) {
                roundedRectBatchVertices_.push_back(color.r);
                roundedRectBatchVertices_.push_back(color.g);
                roundedRectBatchVertices_.push_back(color.b);
                roundedRectBatchVertices_.push_back(color.a);
            };
            for (const PrimitiveGeometryVertex& vertex : command.vertices) {
                roundedRectBatchVertices_.push_back(vertex.screen.x);
                roundedRectBatchVertices_.push_back(vertex.screen.y);
                roundedRectBatchVertices_.push_back(vertex.screen.z);
                roundedRectBatchVertices_.push_back(vertex.local.x);
                roundedRectBatchVertices_.push_back(vertex.local.y);
                appendColor(command.fillColor);
                appendColor(command.gradient.start);
                appendColor(command.gradient.end);
                appendColor(command.border.color);
                roundedRectBatchVertices_.push_back(command.rect.x);
                roundedRectBatchVertices_.push_back(command.rect.y);
                roundedRectBatchVertices_.push_back(command.rect.width);
                roundedRectBatchVertices_.push_back(command.rect.height);
                roundedRectBatchVertices_.push_back(command.radius);
                roundedRectBatchVertices_.push_back(command.shadowPass ? 0.0f : command.border.width);
                roundedRectBatchVertices_.push_back(command.opacity);
                roundedRectBatchVertices_.push_back(command.shadowBlur);
                roundedRectBatchVertices_.push_back(
                    command.gradient.enabled && !command.shadowPass ? 1.0f : 0.0f);
                roundedRectBatchVertices_.push_back(static_cast<float>(command.gradient.direction));
                roundedRectBatchVertices_.push_back(command.shadowPass ? 1.0f : 0.0f);
                roundedRectBatchVertices_.push_back(command.insetShadowPass ? 1.0f : 0.0f);
                roundedRectBatchVertices_.push_back(command.shadowOffset.x);
                roundedRectBatchVertices_.push_back(command.shadowOffset.y);
                roundedRectBatchVertices_.push_back(command.shadowSpread);
            }
            return;
        }
    } else {
        flushRoundedRectBatch();
    }

    PrimitiveResources& resources = primitiveResources();
    const bool hadResources = resources.shaderProgram != 0 && resources.vao != 0 && resources.vbo != 0;
    if (!ensurePrimitiveResources()) {
        return;
    }
    if (!hadResources) {
        resetStateCache();
    }
    setStandardAlphaBlend();

    useProgram(resources.shaderProgram);
    glUniform2f(resources.windowSizeLocation, static_cast<float>(windowWidth), static_cast<float>(windowHeight));
    glUniform4f(resources.fillColorLocation, command.fillColor.r, command.fillColor.g, command.fillColor.b, command.fillColor.a);
    glUniform4f(resources.gradientStartLocation, command.gradient.start.r, command.gradient.start.g, command.gradient.start.b, command.gradient.start.a);
    glUniform4f(resources.gradientEndLocation, command.gradient.end.r, command.gradient.end.g, command.gradient.end.b, command.gradient.end.a);
    glUniform4f(resources.borderColorLocation, command.border.color.r, command.border.color.g, command.border.color.b, command.border.color.a);
    glUniform4f(resources.shadowColorLocation, command.fillColor.r, command.fillColor.g, command.fillColor.b, command.fillColor.a);
    glUniform4f(resources.rectLocation, command.rect.x, command.rect.y, command.rect.width, command.rect.height);
    glUniform1f(resources.radiusLocation, command.radius);
    glUniform1f(resources.borderWidthLocation, command.shadowPass ? 0.0f : command.border.width);
    glUniform1f(resources.opacityLocation, command.opacity);
    glUniform1f(resources.shadowBlurLocation, command.shadowBlur);
    glUniform1f(resources.blurAmountLocation, command.shadowPass ? 0.0f : command.backdropBlur);
    glUniform4f(resources.backdropRectLocation,
                static_cast<float>(resources.backdropX),
                static_cast<float>(resources.backdropY),
                static_cast<float>(std::max(1, resources.backdropWidth)),
                static_cast<float>(std::max(1, resources.backdropHeight)));
    glUniform1i(resources.useGradientLocation, command.gradient.enabled && !command.shadowPass ? 1 : 0);
    glUniform1i(resources.gradientDirectionLocation, static_cast<int>(command.gradient.direction));
    glUniform1i(resources.shadowPassLocation, command.shadowPass ? 1 : 0);
    glUniform1i(resources.insetShadowPassLocation, command.insetShadowPass ? 1 : 0);
    glUniform2f(resources.shadowOffsetLocation,
                command.shadowPass ? command.shadowOffset.x : command.backdropOffset.x,
                command.shadowPass ? command.shadowOffset.y : command.backdropOffset.y);
    glUniform1f(resources.shadowSpreadLocation, command.shadowSpread);
    glUniform1i(resources.backdropLocation, 0);

    if (resources.backdropTexture == 0) {
        ensureBackdropTexture(1, 1);
        resetStateCache();
    }
    activeTextureUnit(0);
    bindTexture2D(resources.backdropTexture);
    bindVertexArray(resources.vao);
    bindArrayBuffer(resources.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(command.vertices.size() * sizeof(PrimitiveGeometryVertex)),
                 command.vertices.data(),
                 GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(command.vertices.size()));
}

void OpenGLRenderBackend::drawPolygon(const PolygonDrawCommand& command, int windowWidth, int windowHeight) {
    if (command.vertices.empty() || command.edges.size() < 3 || windowWidth <= 0 || windowHeight <= 0 ||
        command.opacity <= 0.001f || command.fillColor.a <= 0.001f) {
        return;
    }
    flushRoundedRectBatch();

    PolygonResources& resources = polygonResources();
    const bool hadResources = resources.shaderProgram != 0 && resources.vao != 0 && resources.vbo != 0;
    if (!ensurePolygonResources()) {
        return;
    }
    if (!hadResources) {
        resetStateCache();
    }
    setStandardAlphaBlend();

    std::vector<float> edges;
    const int edgeCount = std::min(static_cast<int>(command.edges.size()), kMaxPolygonEdges);
    edges.reserve(static_cast<std::size_t>(edgeCount) * 4u);
    for (int index = 0; index < edgeCount; ++index) {
        const PolygonEdgeData& edge = command.edges[static_cast<std::size_t>(index)];
        edges.push_back(edge.from.x);
        edges.push_back(edge.from.y);
        edges.push_back(edge.to.x);
        edges.push_back(edge.to.y);
    }

    useProgram(resources.shaderProgram);
    glUniform2f(resources.windowSizeLocation, static_cast<float>(windowWidth), static_cast<float>(windowHeight));
    glUniform4f(resources.fillColorLocation, command.fillColor.r, command.fillColor.g, command.fillColor.b, command.fillColor.a);
    glUniform1f(resources.opacityLocation, command.opacity);
    glUniform1i(resources.edgeCountLocation, edgeCount);
    glUniform4fv(resources.edgesLocation, edgeCount, edges.data());

    bindVertexArray(resources.vao);
    bindArrayBuffer(resources.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(command.vertices.size() * sizeof(PrimitiveGeometryVertex)),
                 command.vertices.data(),
                 GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(command.vertices.size()));
}

void OpenGLRenderBackend::releasePrimitiveResources() {
    flushRoundedRectBatch();
    releaseResources(primitiveResources());
    releaseResources(roundedRectBatchResources());
    resetStateCache();
}

void OpenGLRenderBackend::releasePolygonResources() {
    releaseResources(polygonResources());
    resetStateCache();
}

} // namespace core::render::opengl
