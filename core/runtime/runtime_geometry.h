#pragma once

#include "core/dsl.h"

#include <algorithm>
#include <cmath>

namespace core::dsl {

struct RenderTransform {
    bool active = false;
    TransformMatrix matrix;
    float opacity = 1.0f;
};

inline float toPixels(float value, float dpiScale) {
    return value * dpiScale;
}

inline Rect toPixelRect(const LayoutRect& frame, float dpiScale) {
    return {
        toPixels(frame.x, dpiScale),
        toPixels(frame.y, dpiScale),
        toPixels(frame.width, dpiScale),
        toPixels(frame.height, dpiScale)
    };
}

inline Rect toPixelRect(const Rect& rect, float dpiScale) {
    return {
        toPixels(rect.x, dpiScale),
        toPixels(rect.y, dpiScale),
        toPixels(rect.width, dpiScale),
        toPixels(rect.height, dpiScale)
    };
}

inline Rect toLogicalRect(const Rect& rect, float dpiScale) {
    const float scale = dpiScale > 0.0f ? 1.0f / dpiScale : 1.0f;
    return {
        rect.x * scale,
        rect.y * scale,
        rect.width * scale,
        rect.height * scale
    };
}

inline bool intersects(const Rect& a, const Rect& b) {
    return a.x < b.x + b.width &&
           a.x + a.width > b.x &&
           a.y < b.y + b.height &&
           a.y + a.height > b.y;
}

inline Rect unionRect(const Rect& a, const Rect& b) {
    const float left = std::min(a.x, b.x);
    const float top = std::min(a.y, b.y);
    const float right = std::max(a.x + a.width, b.x + b.width);
    const float bottom = std::max(a.y + a.height, b.y + b.height);
    return {left, top, right - left, bottom - top};
}

inline bool intersectRect(const Rect& a, const Rect& b, Rect& out) {
    const float left = std::max(a.x, b.x);
    const float top = std::max(a.y, b.y);
    const float right = std::min(a.x + a.width, b.x + b.width);
    const float bottom = std::min(a.y + a.height, b.y + b.height);
    if (right <= left || bottom <= top) {
        out = {};
        return false;
    }
    out = {left, top, right - left, bottom - top};
    return true;
}

inline Rect inflateRect(Rect rect, float amount) {
    rect.x -= amount;
    rect.y -= amount;
    rect.width += amount * 2.0f;
    rect.height += amount * 2.0f;
    return rect;
}

inline constexpr float dependentVisualPadding() {
    return 4.0f;
}

inline bool isIdentityTransform(const Transform& transform) {
    return closeEnough(transform.translate, Vec2{}) &&
           closeEnough(transform.translateZ, 0.0f) &&
           closeEnough(transform.scale, Vec2{1.0f, 1.0f}) &&
           closeEnough(transform.rotate, 0.0f) &&
           closeEnough(transform.rotateX, 0.0f) &&
           closeEnough(transform.rotateY, 0.0f) &&
           closeEnough(transform.perspective, 0.0f);
}

inline bool isIdentityMatrix(const TransformMatrix& matrix) {
    return closeEnough(matrix.m00, 1.0f) &&
           closeEnough(matrix.m01, 0.0f) &&
           closeEnough(matrix.tx, 0.0f) &&
           closeEnough(matrix.m10, 0.0f) &&
           closeEnough(matrix.m11, 1.0f) &&
           closeEnough(matrix.ty, 0.0f) &&
           closeEnough(matrix.px, 0.0f) &&
           closeEnough(matrix.py, 0.0f) &&
           closeEnough(matrix.pw, 1.0f);
}

inline TransformMatrix multiplyMatrix(const TransformMatrix& outer, const TransformMatrix& inner) {
    return {
        outer.m00 * inner.m00 + outer.m01 * inner.m10 + outer.tx * inner.px,
        outer.m00 * inner.m01 + outer.m01 * inner.m11 + outer.tx * inner.py,
        outer.m00 * inner.tx + outer.m01 * inner.ty + outer.tx * inner.pw,
        outer.m10 * inner.m00 + outer.m11 * inner.m10 + outer.ty * inner.px,
        outer.m10 * inner.m01 + outer.m11 * inner.m11 + outer.ty * inner.py,
        outer.m10 * inner.tx + outer.m11 * inner.ty + outer.ty * inner.pw,
        outer.px * inner.m00 + outer.py * inner.m10 + outer.pw * inner.px,
        outer.px * inner.m01 + outer.py * inner.m11 + outer.pw * inner.py,
        outer.px * inner.tx + outer.py * inner.ty + outer.pw * inner.pw
    };
}

inline bool inverseMatrix(const TransformMatrix& matrix, TransformMatrix& inverse) {
    const float c00 = matrix.m11 * matrix.pw - matrix.ty * matrix.py;
    const float c01 = matrix.ty * matrix.px - matrix.m10 * matrix.pw;
    const float c02 = matrix.m10 * matrix.py - matrix.m11 * matrix.px;
    const float c10 = matrix.tx * matrix.py - matrix.m01 * matrix.pw;
    const float c11 = matrix.m00 * matrix.pw - matrix.tx * matrix.px;
    const float c12 = matrix.m01 * matrix.px - matrix.m00 * matrix.py;
    const float c20 = matrix.m01 * matrix.ty - matrix.tx * matrix.m11;
    const float c21 = matrix.tx * matrix.m10 - matrix.m00 * matrix.ty;
    const float c22 = matrix.m00 * matrix.m11 - matrix.m01 * matrix.m10;
    const float determinant = matrix.m00 * c00 + matrix.m01 * c01 + matrix.tx * c02;
    if (std::fabs(determinant) <= 0.000001f) {
        return false;
    }
    const float invDet = 1.0f / determinant;
    inverse = {
        c00 * invDet,
        c10 * invDet,
        c20 * invDet,
        c01 * invDet,
        c11 * invDet,
        c21 * invDet,
        c02 * invDet,
        c12 * invDet,
        c22 * invDet
    };
    return true;
}

inline TransformMatrix matrixForTransform(const Rect& frame, const Transform& transform) {
    const Vec2 origin = {
        frame.x + frame.width * transform.origin.x,
        frame.y + frame.height * transform.origin.y
    };
    const float cosX = std::cos(transform.rotateX);
    const float sinX = std::sin(transform.rotateX);
    const float cosY = std::cos(transform.rotateY);
    const float sinY = std::sin(transform.rotateY);
    const float cosZ = std::cos(transform.rotate);
    const float sinZ = std::sin(transform.rotate);
    const float scaleX = transform.scale.x;
    const float scaleY = transform.scale.y;

    const float xFromX = scaleX * cosY;
    const float xFromY = scaleY * sinX * sinY;
    const float yFromY = scaleY * cosX;
    const float zFromX = -scaleX * sinY;
    const float zFromY = scaleY * sinX * cosY;

    const float ax = xFromX * cosZ;
    const float bx = xFromY * cosZ - yFromY * sinZ;
    const float ay = xFromX * sinZ;
    const float by = xFromY * sinZ + yFromY * cosZ;
    const float az = zFromX;
    const float bz = zFromY;

    if (transform.perspective <= 0.0001f) {
        return {
            ax,
            bx,
            origin.x + transform.translate.x - ax * origin.x - bx * origin.y,
            ay,
            by,
            origin.y + transform.translate.y - ay * origin.x - by * origin.y,
            0.0f,
            0.0f,
            1.0f
        };
    }

    const float perspective = std::max(1.0f, transform.perspective);
    const float tz = transform.translateZ;
    const float nxDx = perspective * ax - origin.x * az;
    const float nxDy = perspective * bx - origin.x * bz;
    const float nyDx = perspective * ay - origin.y * az;
    const float nyDy = perspective * by - origin.y * bz;
    const float denominator = perspective - tz + az * origin.x + bz * origin.y;

    return {
        nxDx + transform.translate.x * -az,
        nxDy + transform.translate.x * -bz,
        perspective * origin.x - origin.x * tz - nxDx * origin.x - nxDy * origin.y + transform.translate.x * denominator,
        nyDx + transform.translate.y * -az,
        nyDy + transform.translate.y * -bz,
        perspective * origin.y - origin.y * tz - nyDx * origin.x - nyDy * origin.y + transform.translate.y * denominator,
        -az,
        -bz,
        denominator
    };
}

inline TransformMatrix matrixForScaleAround(const Rect& frame, float scale) {
    Transform transform;
    transform.scale = {scale, scale};
    transform.origin = {0.5f, 0.5f};
    return matrixForTransform(frame, transform);
}

inline RenderTransform appendRenderMatrix(RenderTransform transform, const TransformMatrix& matrix) {
    transform.matrix = multiplyMatrix(transform.matrix, matrix);
    transform.active = transform.active || !isIdentityMatrix(matrix);
    return transform;
}

inline Vec2 transformPoint(Vec2 point, const LayoutRect& frame, const Transform& transform) {
    return core::transformPoint(matrixForTransform({frame.x, frame.y, frame.width, frame.height}, transform), point.x, point.y);
}

inline float shadowVisualPadding(const Shadow& shadow) {
    const float blur = std::max(shadow.blur, 1.0f);
    const float offsetMagnitude = std::max(std::fabs(shadow.offset.x), std::fabs(shadow.offset.y));
    return blur * 1.18f * 1.08f + offsetMagnitude * 0.20f + 1.0f;
}

inline Rect transformRect(const Rect& rect, const LayoutRect& frame, const Transform& transform) {
    if (isIdentityTransform(transform)) {
        return rect;
    }

    const Vec2 p0 = transformPoint({rect.x, rect.y}, frame, transform);
    const Vec2 p1 = transformPoint({rect.x + rect.width, rect.y}, frame, transform);
    const Vec2 p2 = transformPoint({rect.x + rect.width, rect.y + rect.height}, frame, transform);
    const Vec2 p3 = transformPoint({rect.x, rect.y + rect.height}, frame, transform);
    const float left = std::min(std::min(p0.x, p1.x), std::min(p2.x, p3.x));
    const float top = std::min(std::min(p0.y, p1.y), std::min(p2.y, p3.y));
    const float right = std::max(std::max(p0.x, p1.x), std::max(p2.x, p3.x));
    const float bottom = std::max(std::max(p0.y, p1.y), std::max(p2.y, p3.y));
    return {left, top, right - left, bottom - top};
}

inline Rect visualRect(const LayoutRect& frame, const Shadow& shadow, float blur, const Transform& transform = {}) {
    Rect rect{frame.x, frame.y, frame.width, frame.height};
    if (shadow.enabled && !shadow.inset) {
        Rect shadowRect{
            frame.x + shadow.offset.x - shadow.spread,
            frame.y + shadow.offset.y - shadow.spread,
            frame.width + shadow.spread * 2.0f,
            frame.height + shadow.spread * 2.0f
        };
        shadowRect = inflateRect(shadowRect, shadowVisualPadding(shadow));
        rect = unionRect(rect, shadowRect);
    }
    if (blur > 0.0f) {
        rect = inflateRect(rect, blur + 2.0f);
    }
    return transformRect(rect, frame, transform);
}

inline Rect backdropCaptureRect(const LayoutRect& frame, float blur, const Transform& transform = {}) {
    return visualRect(frame, Shadow{}, blur, transform);
}

inline Rect imageVisualRect(const LayoutRect& frame, const Transform& transform = {}) {
    return transformRect({frame.x, frame.y, frame.width, frame.height}, frame, transform);
}

inline bool containsRect(const Rect& outer, const Rect& inner) {
    return inner.x >= outer.x &&
           inner.y >= outer.y &&
           inner.x + inner.width <= outer.x + outer.width &&
           inner.y + inner.height <= outer.y + outer.height;
}

inline bool sameGradient(const Gradient& left, const Gradient& right) {
    return left.enabled == right.enabled &&
           left.direction == right.direction &&
           closeEnough(left.start, right.start) &&
           closeEnough(left.end, right.end);
}

inline Vec2 applyRenderTransform(Vec2 point, const RenderTransform& transform) {
    if (!transform.active) {
        return point;
    }
    return core::transformPoint(transform.matrix, point.x, point.y);
}

inline Rect applyRenderTransform(const Rect& rect, const RenderTransform& transform) {
    if (!transform.active) {
        return rect;
    }

    const Vec2 p0 = applyRenderTransform(Vec2{rect.x, rect.y}, transform);
    const Vec2 p1 = applyRenderTransform(Vec2{rect.x + rect.width, rect.y}, transform);
    const Vec2 p2 = applyRenderTransform(Vec2{rect.x + rect.width, rect.y + rect.height}, transform);
    const Vec2 p3 = applyRenderTransform(Vec2{rect.x, rect.y + rect.height}, transform);
    const float left = std::min(std::min(p0.x, p1.x), std::min(p2.x, p3.x));
    const float top = std::min(std::min(p0.y, p1.y), std::min(p2.y, p3.y));
    const float right = std::max(std::max(p0.x, p1.x), std::max(p2.x, p3.x));
    const float bottom = std::max(std::max(p0.y, p1.y), std::max(p2.y, p3.y));
    return {left, top, right - left, bottom - top};
}

inline Rect applyTransformMatrix(const Rect& rect, const TransformMatrix& matrix) {
    const Vec2 p0 = core::transformPoint(matrix, rect.x, rect.y);
    const Vec2 p1 = core::transformPoint(matrix, rect.x + rect.width, rect.y);
    const Vec2 p2 = core::transformPoint(matrix, rect.x + rect.width, rect.y + rect.height);
    const Vec2 p3 = core::transformPoint(matrix, rect.x, rect.y + rect.height);
    const float left = std::min(std::min(p0.x, p1.x), std::min(p2.x, p3.x));
    const float top = std::min(std::min(p0.y, p1.y), std::min(p2.y, p3.y));
    const float right = std::max(std::max(p0.x, p1.x), std::max(p2.x, p3.x));
    const float bottom = std::max(std::max(p0.y, p1.y), std::max(p2.y, p3.y));
    return {left, top, right - left, bottom - top};
}

inline Rect applyRenderTransformToLogicalRect(const Rect& rect, float dpiScale, const RenderTransform& transform) {
    if (!transform.active) {
        return rect;
    }
    return toLogicalRect(applyRenderTransform(toPixelRect(rect, dpiScale), transform), dpiScale);
}

inline Rect imeCursorPixelRect(const Element& element, float dpiScale, const TransformMatrix& transform) {
    const Rect logicalRect{
        element.frame.x + element.imeRect.x,
        element.frame.y + element.imeRect.y,
        element.imeRect.width,
        element.imeRect.height
    };
    return applyTransformMatrix(toPixelRect(logicalRect, dpiScale), transform);
}

inline Border scaleBorder(Border border, float dpiScale) {
    border.width = toPixels(border.width, dpiScale);
    return border;
}

inline Shadow scaleShadow(Shadow shadow, float dpiScale) {
    shadow.offset.x = toPixels(shadow.offset.x, dpiScale);
    shadow.offset.y = toPixels(shadow.offset.y, dpiScale);
    shadow.blur = toPixels(shadow.blur, dpiScale);
    shadow.spread = toPixels(shadow.spread, dpiScale);
    return shadow;
}

inline Transform scaleTransform(Transform transform, float dpiScale) {
    transform.translate.x = toPixels(transform.translate.x, dpiScale);
    transform.translate.y = toPixels(transform.translate.y, dpiScale);
    transform.translateZ = toPixels(transform.translateZ, dpiScale);
    transform.perspective = toPixels(transform.perspective, dpiScale);
    return transform;
}

inline bool usesRuntimeTransform(const Element& element) {
    return !element.scrollContentSourceId.empty() ||
           !element.scrollThumbSourceId.empty() ||
           !element.sliderKnobSourceId.empty();
}

inline TransformMatrix combinedPrimitiveMatrix(const RenderTransform& renderTransform,
                                               const Rect& frame,
                                               const Transform& localTransform) {
    return multiplyMatrix(renderTransform.matrix, matrixForTransform(frame, localTransform));
}

} // namespace core::dsl
