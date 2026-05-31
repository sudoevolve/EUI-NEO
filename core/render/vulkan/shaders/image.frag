#version 450

layout(location = 0) in vec2 vLocalPos;
layout(location = 1) in vec2 vUv;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D uTexture;

layout(push_constant) uniform PushConstants {
    vec4 windowSize;
    vec4 tint;
    vec4 rect;
    vec4 flags;
} pc;

float roundedBoxDistance(vec2 point, vec2 halfSize, float radius) {
    vec2 cornerVector = abs(point) - halfSize + vec2(radius);
    return length(max(cornerVector, 0.0)) + min(max(cornerVector.x, cornerVector.y), 0.0) - radius;
}

void main() {
    vec2 center = pc.rect.xy + pc.rect.zw * 0.5;
    float distanceToEdge = roundedBoxDistance(vLocalPos - center, pc.rect.zw * 0.5, pc.flags.x);
    float edgeWidth = max(fwidth(distanceToEdge), 0.75);
    float shapeAlpha = 1.0 - smoothstep(-edgeWidth, edgeWidth, distanceToEdge);
    if (shapeAlpha <= 0.0) {
        discard;
    }
    vec4 sampled = texture(uTexture, vUv);
    outColor = vec4(sampled.rgb * pc.tint.rgb, sampled.a * pc.tint.a * shapeAlpha);
}
