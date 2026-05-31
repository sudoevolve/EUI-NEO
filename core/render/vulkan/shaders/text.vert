#version 450

layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUv;
layout(location = 2) in float aColored;

layout(location = 0) out vec2 vUv;
layout(location = 1) out float vColored;

layout(push_constant) uniform PushConstants {
    vec4 windowSize;
    vec4 color;
} pc;

void main() {
    vUv = aUv;
    vColored = aColored;
    vec2 ndc = vec2((aPos.x / pc.windowSize.x) * 2.0 - 1.0,
                    (aPos.y / pc.windowSize.y) * 2.0 - 1.0);
    gl_Position = vec4(ndc, 0.0, 1.0);
}
