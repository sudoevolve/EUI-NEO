#version 450

layout(location = 0) in vec2 vLocalPos;
layout(location = 1) in vec2 vUv;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D uTexture;
layout(set = 0, binding = 1) uniform sampler2D uTextureU;
layout(set = 0, binding = 2) uniform sampler2D uTextureV;

layout(push_constant) uniform PushConstants {
    vec4 windowSize;
    vec4 tint;
    vec4 rect;
    vec4 flags;
    vec4 yuvMatrix0;
    vec4 yuvMatrix1;
    vec4 yuvMatrix2;
    vec4 yuvOffset;
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
    if (pc.flags.z > 0.5) {
        vec2 chroma = texture(uTextureU, vUv).rg;
        if (pc.flags.z > 1.5) {
            chroma = vec2(texture(uTextureU, vUv).r, texture(uTextureV, vUv).r);
        }
        vec3 yuv = vec3(sampled.r, chroma) - pc.yuvOffset.xyz;
        sampled = vec4(dot(pc.yuvMatrix0.xyz, yuv),
                       dot(pc.yuvMatrix1.xyz, yuv),
                       dot(pc.yuvMatrix2.xyz, yuv),
                       1.0);
    }
    if (pc.flags.y > 0.01) {
        vec2 pixelStep = max(fwidth(vUv), 1.0 / vec2(textureSize(uTexture, 0)));
        vec2 blurStep = pixelStep * pc.flags.y * 0.5;
        sampled *= 4.0;
        sampled += texture(uTexture, clamp(vUv + vec2( blurStep.x, 0.0), 0.0, 1.0)) * 2.0;
        sampled += texture(uTexture, clamp(vUv + vec2(-blurStep.x, 0.0), 0.0, 1.0)) * 2.0;
        sampled += texture(uTexture, clamp(vUv + vec2(0.0,  blurStep.y), 0.0, 1.0)) * 2.0;
        sampled += texture(uTexture, clamp(vUv + vec2(0.0, -blurStep.y), 0.0, 1.0)) * 2.0;
        sampled += texture(uTexture, clamp(vUv + blurStep, 0.0, 1.0));
        sampled += texture(uTexture, clamp(vUv - blurStep, 0.0, 1.0));
        sampled += texture(uTexture, clamp(vUv + vec2( blurStep.x, -blurStep.y), 0.0, 1.0));
        sampled += texture(uTexture, clamp(vUv + vec2(-blurStep.x,  blurStep.y), 0.0, 1.0));
        sampled /= 16.0;
    }
    outColor = vec4(sampled.rgb * pc.tint.rgb, sampled.a * pc.tint.a * shapeAlpha);
}
