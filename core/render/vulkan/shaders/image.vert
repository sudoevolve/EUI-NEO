#version 450

layout(location = 0) in vec3 aScreenPos;
layout(location = 1) in vec2 aLocalPos;
layout(location = 2) in vec2 aUv;

layout(location = 0) out vec2 vLocalPos;
layout(location = 1) out vec2 vUv;

layout(push_constant) uniform PushConstants {
    vec4 windowSize;
    vec4 tint;
    vec4 rect;
    vec4 flags;
} pc;

void main() {
    vLocalPos = aLocalPos;
    vUv = aUv;
    vec2 ndc = vec2((aScreenPos.x / pc.windowSize.x) * 2.0 - 1.0,
                    (aScreenPos.y / pc.windowSize.y) * 2.0 - 1.0);
    gl_Position = vec4(ndc * aScreenPos.z, 0.0, aScreenPos.z);
}
