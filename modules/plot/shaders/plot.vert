#version 450

layout(location = 0) in vec2 inPosition;

layout(push_constant) uniform PlotPushConstants {
    vec2 targetSize;
    vec4 color;
} pc;

void main() {
    vec2 normalized = inPosition / pc.targetSize;
    vec2 clip = vec2(normalized.x * 2.0 - 1.0, 1.0 - normalized.y * 2.0);
    gl_Position = vec4(clip, 0.0, 1.0);
}
