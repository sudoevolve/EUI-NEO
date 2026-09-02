#version 450

layout(location = 0) in vec2 vLocalPos;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D uBackdrop;

layout(push_constant) uniform PushConstants {
    vec4 windowAndShape;
    vec4 fillColor;
    vec4 gradientStart;
    vec4 gradientEnd;
    vec4 borderColor;
    vec4 rect;
    vec4 flags;
    vec4 flags2;
} pc;

float rand(vec2 co) {
    return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

float roundedBoxDistance(vec2 point, vec2 halfSize, float radius) {
    vec2 cornerVector = abs(point) - halfSize + vec2(radius);
    return length(max(cornerVector, 0.0)) + min(max(cornerVector.x, cornerVector.y), 0.0) - radius;
}

vec4 backdropRect(float blurRadiusPx) {
    vec2 windowSize = max(pc.windowAndShape.xy, vec2(1.0));
    vec2 captureOffset = vec2(pc.flags.y, pc.flags2.w);
    vec2 captureOrigin = pc.rect.xy + captureOffset;
    float left = clamp(floor(captureOrigin.x - blurRadiusPx), 0.0, max(windowSize.x - 1.0, 0.0));
    float top = clamp(floor(captureOrigin.y - blurRadiusPx), 0.0, max(windowSize.y - 1.0, 0.0));
    float right = clamp(ceil(captureOrigin.x + pc.rect.z + blurRadiusPx), left + 1.0, windowSize.x);
    float bottom = clamp(ceil(captureOrigin.y + pc.rect.w + blurRadiusPx), top + 1.0, windowSize.y);
    return vec4(left, top, right - left, bottom - top);
}

vec3 sampleBackdrop(vec2 captureUv, vec4 captureRect) {
    vec2 clampedUv = clamp(captureUv, vec2(0.0), vec2(1.0));
    return texture(uBackdrop, clampedUv).rgb;
}

vec3 backdropBlur(vec2 uv, vec4 captureRect) {
    vec2 pixelStep = 1.0 / max(captureRect.zw, vec2(1.0));
    float blurRadiusPx = pc.flags2.y;
    vec3 blurred = sampleBackdrop(uv, captureRect);
    float repeats = mix(8.0, 24.0, clamp(blurRadiusPx / 36.0, 0.0, 1.0));
    float stepAngle = 6.28318530718 / repeats;
    vec2 stepDirection = vec2(cos(stepAngle), sin(stepAngle));
    vec2 halfStepDirection = vec2(cos(stepAngle * 0.5), sin(stepAngle * 0.5));
    vec2 dir = vec2(1.0, 0.0);
    for (int i = 0; i < 24; ++i) {
        float sampleIndex = float(i);
        if (sampleIndex >= repeats) {
            break;
        }
        float radiusA = blurRadiusPx * (0.35 + 0.65 * rand(vec2(sampleIndex, uv.x + uv.y)));
        vec2 uvA = clamp(uv + dir * radiusA * pixelStep, pixelStep * 0.5, vec2(1.0) - pixelStep * 0.5);
        blurred += sampleBackdrop(uvA, captureRect);

        vec2 dirB = vec2(dir.x * halfStepDirection.x - dir.y * halfStepDirection.y,
                          dir.x * halfStepDirection.y + dir.y * halfStepDirection.x);
        float radiusB = blurRadiusPx * (0.20 + 0.80 * rand(vec2(sampleIndex + 2.0, uv.x + uv.y + 24.0)));
        vec2 uvB = clamp(uv + dirB * radiusB * pixelStep, pixelStep * 0.5, vec2(1.0) - pixelStep * 0.5);
        blurred += sampleBackdrop(uvB, captureRect);
        dir = vec2(dir.x * stepDirection.x - dir.y * stepDirection.y,
                   dir.x * stepDirection.y + dir.y * stepDirection.x);
    }
    return blurred / (repeats * 2.0 + 1.0);
}

void main() {
    float radius = pc.windowAndShape.z;
    float borderWidth = pc.windowAndShape.w;
    float opacity = pc.flags.x;
    float shadowBlur = pc.flags.y;
    bool useGradient = pc.flags.z > 0.5;
    int gradientDirection = int(pc.flags.w + 0.5);
    bool shadowPass = pc.flags2.x > 0.5;
    float blurAmount = pc.flags2.y;
    bool backdropReady = pc.flags2.z > 0.5;
    bool insetShadowPass = pc.flags2.w > 0.5;

    vec2 center = pc.rect.xy + pc.rect.zw * 0.5;
    float distanceToEdge = roundedBoxDistance(vLocalPos - center, pc.rect.zw * 0.5, radius);
    float blur = max(shadowBlur, 1.0);
    if (shadowPass) {
        if (insetShadowPass) {
            float edgeWidth = max(fwidth(distanceToEdge), 0.75);
            float shapeAlpha = 1.0 - smoothstep(-edgeWidth, edgeWidth, distanceToEdge);
            if (shapeAlpha <= 0.0) {
                discard;
            }

            vec2 shadowOffset = pc.borderColor.xy;
            float shadowSpread = pc.borderColor.z;
            vec2 sideVector = dot(shadowOffset, shadowOffset) <= 0.0001 ? vec2(0.0, 1.0) : normalize(-shadowOffset);
            vec2 localUnit = (vLocalPos - center) / max(pc.rect.zw * 0.5, vec2(1.0));
            float sideMask = clamp(0.34 + dot(localUnit, sideVector) * 0.66, 0.0, 1.0);
            float spreadBias = max(shadowSpread, 0.0);
            float edgeFalloff = smoothstep(-blur - spreadBias, 0.0, distanceToEdge);
            float innerAlpha = edgeFalloff * sideMask;
            if (innerAlpha <= 0.0) {
                discard;
            }
            outColor = vec4(pc.fillColor.rgb, pc.fillColor.a * innerAlpha * shapeAlpha * opacity);
            return;
        }

        float shadowAlpha = 1.0 - smoothstep(-blur, blur, distanceToEdge);
        if (shadowAlpha <= 0.0) {
            discard;
        }
        outColor = vec4(pc.fillColor.rgb, pc.fillColor.a * shadowAlpha * opacity);
        return;
    }

    float edgeWidth = max(fwidth(distanceToEdge), 0.75);
    float shapeAlpha = 1.0 - smoothstep(-edgeWidth, edgeWidth, distanceToEdge);
    if (shapeAlpha <= 0.0) {
        discard;
    }

    float gradientAmount = gradientDirection == 0
        ? clamp((vLocalPos.x - pc.rect.x) / max(pc.rect.z, 1.0), 0.0, 1.0)
        : clamp((vLocalPos.y - pc.rect.y) / max(pc.rect.w, 1.0), 0.0, 1.0);
    vec4 fill = useGradient ? mix(pc.gradientStart, pc.gradientEnd, gradientAmount) : pc.fillColor;
    if (blurAmount > 0.0 && backdropReady) {
        vec4 captureRect = backdropRect(blurAmount);
        vec2 backdropUv = (gl_FragCoord.xy - captureRect.xy) / max(captureRect.zw, vec2(1.0));
        vec3 blurred = backdropBlur(backdropUv, captureRect);
        fill = vec4(mix(blurred, fill.rgb, fill.a), 1.0);
    }

    float borderAlpha = borderWidth > 0.0
        ? smoothstep(-borderWidth - edgeWidth, -borderWidth + edgeWidth, distanceToEdge)
        : 0.0;
    vec4 color = mix(fill, pc.borderColor, borderAlpha);
    outColor = vec4(color.rgb, color.a * shapeAlpha * opacity);
}
