#version 450

layout(location = 0) in vec3 aScreenPos;
layout(location = 1) in vec2 aLocalPos;
layout(location = 2) in vec4 aFillColor;
layout(location = 3) in vec4 aGradientStart;
layout(location = 4) in vec4 aGradientEnd;
layout(location = 5) in vec4 aBorderColor;
layout(location = 6) in vec4 aRect;
layout(location = 7) in vec4 aShapeParams;
layout(location = 8) in vec4 aModeParams;
layout(location = 9) in vec3 aShadowParams;

layout(location = 0) out vec2 vLocalPos;
layout(location = 1) flat out vec4 vFillColor;
layout(location = 2) flat out vec4 vGradientStart;
layout(location = 3) flat out vec4 vGradientEnd;
layout(location = 4) flat out vec4 vBorderColor;
layout(location = 5) flat out vec4 vRect;
layout(location = 6) flat out vec4 vShapeParams;
layout(location = 7) flat out vec4 vModeParams;
layout(location = 8) flat out vec3 vShadowParams;

layout(push_constant) uniform PushConstants {
    vec2 windowSize;
} pc;

void main() {
    vLocalPos = aLocalPos;
    vFillColor = aFillColor;
    vGradientStart = aGradientStart;
    vGradientEnd = aGradientEnd;
    vBorderColor = aBorderColor;
    vRect = aRect;
    vShapeParams = aShapeParams;
    vModeParams = aModeParams;
    vShadowParams = aShadowParams;
    vec2 ndc = vec2((aScreenPos.x / pc.windowSize.x) * 2.0 - 1.0,
                    (aScreenPos.y / pc.windowSize.y) * 2.0 - 1.0);
    gl_Position = vec4(ndc * aScreenPos.z, 0.0, aScreenPos.z);
}
