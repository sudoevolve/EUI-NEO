#version 450

layout(location = 0) in vec2 vLocalPos;
layout(location = 1) flat in vec4 vFillColor;
layout(location = 2) flat in vec4 vGradientStart;
layout(location = 3) flat in vec4 vGradientEnd;
layout(location = 4) flat in vec4 vBorderColor;
layout(location = 5) flat in vec4 vRect;
layout(location = 6) flat in vec4 vShapeParams;
layout(location = 7) flat in vec4 vModeParams;
layout(location = 8) flat in vec3 vShadowParams;

layout(location = 0) out vec4 outColor;

float roundedBoxDistance(vec2 point, vec2 halfSize, float radius) {
    vec2 cornerVector = abs(point) - halfSize + vec2(radius);
    return length(max(cornerVector, 0.0)) + min(max(cornerVector.x, cornerVector.y), 0.0) - radius;
}

void main() {
    float radius = vShapeParams.x;
    float borderWidth = vShapeParams.y;
    float opacity = vShapeParams.z;
    float shadowBlur = max(vShapeParams.w, 1.0);
    bool useGradient = vModeParams.x > 0.5;
    bool verticalGradient = vModeParams.y > 0.5;
    bool shadowPass = vModeParams.z > 0.5;
    bool insetShadowPass = vModeParams.w > 0.5;
    vec2 center = vRect.xy + vRect.zw * 0.5;
    float distanceToEdge = roundedBoxDistance(vLocalPos - center, vRect.zw * 0.5, radius);

    if (shadowPass) {
        if (insetShadowPass) {
            float edgeWidth = max(fwidth(distanceToEdge), 0.75);
            float shapeAlpha = 1.0 - smoothstep(-edgeWidth, edgeWidth, distanceToEdge);
            if (shapeAlpha <= 0.0) {
                discard;
            }
            vec2 offset = vShadowParams.xy;
            vec2 sideVector = dot(offset, offset) <= 0.0001 ? vec2(0.0, 1.0) : normalize(-offset);
            vec2 localUnit = (vLocalPos - center) / max(vRect.zw * 0.5, vec2(1.0));
            float sideMask = clamp(0.34 + dot(localUnit, sideVector) * 0.66, 0.0, 1.0);
            float spreadBias = max(vShadowParams.z, 0.0);
            float edgeFalloff = smoothstep(-shadowBlur - spreadBias, 0.0, distanceToEdge);
            float innerAlpha = edgeFalloff * sideMask;
            if (innerAlpha <= 0.0) {
                discard;
            }
            outColor = vec4(vFillColor.rgb, vFillColor.a * innerAlpha * shapeAlpha * opacity);
            return;
        }

        float shadowAlpha = 1.0 - smoothstep(-shadowBlur, shadowBlur, distanceToEdge);
        if (shadowAlpha <= 0.0) {
            discard;
        }
        outColor = vec4(vFillColor.rgb, vFillColor.a * shadowAlpha * opacity);
        return;
    }

    float edgeWidth = max(fwidth(distanceToEdge), 0.75);
    float shapeAlpha = 1.0 - smoothstep(-edgeWidth, edgeWidth, distanceToEdge);
    if (shapeAlpha <= 0.0) {
        discard;
    }
    float gradientAmount = verticalGradient
        ? clamp((vLocalPos.y - vRect.y) / max(vRect.w, 1.0), 0.0, 1.0)
        : clamp((vLocalPos.x - vRect.x) / max(vRect.z, 1.0), 0.0, 1.0);
    vec4 fill = useGradient ? mix(vGradientStart, vGradientEnd, gradientAmount) : vFillColor;
    float borderAlpha = borderWidth > 0.0
        ? smoothstep(-borderWidth - edgeWidth, -borderWidth + edgeWidth, distanceToEdge)
        : 0.0;
    vec4 color = mix(fill, vBorderColor, borderAlpha);
    outColor = vec4(color.rgb, color.a * shapeAlpha * opacity);
}
