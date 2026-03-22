#include "UIPrimitive.h"
#include <algorithm>
#include <cmath>

namespace EUINEO {

namespace {

RectFrame ResolveFrame(float x, float y, float width, float height, Anchor anchor) {
    RectFrame frame;
    frame.x = x;
    frame.y = y;
    frame.width = width;
    frame.height = height;

    switch (anchor) {
        case Anchor::TopCenter:
            frame.x += State.screenW * 0.5f - width * 0.5f;
            break;
        case Anchor::TopRight:
            frame.x += State.screenW - width;
            break;
        case Anchor::CenterLeft:
            frame.y += State.screenH * 0.5f - height * 0.5f;
            break;
        case Anchor::Center:
            frame.x += State.screenW * 0.5f - width * 0.5f;
            frame.y += State.screenH * 0.5f - height * 0.5f;
            break;
        case Anchor::CenterRight:
            frame.x += State.screenW - width;
            frame.y += State.screenH * 0.5f - height * 0.5f;
            break;
        case Anchor::BottomLeft:
            frame.y += State.screenH - height;
            break;
        case Anchor::BottomCenter:
            frame.x += State.screenW * 0.5f - width * 0.5f;
            frame.y += State.screenH - height;
            break;
        case Anchor::BottomRight:
            frame.x += State.screenW - width;
            frame.y += State.screenH - height;
            break;
        case Anchor::TopLeft:
        default:
            break;
    }

    return frame;
}

bool TryMakeScissorRect(const UIPrimitive& primitive, GLint& outX, GLint& outY, GLint& outW, GLint& outH) {
    if (!primitive.hasClipRect || primitive.clipRect.width <= 0.0f || primitive.clipRect.height <= 0.0f) {
        return false;
    }
    const RectFrame bounds{
        primitive.clipRect.x,
        primitive.clipRect.y,
        primitive.clipRect.width,
        primitive.clipRect.height
    };
    return Renderer::MakeCurrentScissorRect(bounds, outX, outY, outW, outH);
}

} // namespace

Color ApplyOpacity(Color color, float opacity) {
    color.a *= std::clamp(opacity, 0.0f, 1.0f);
    return color;
}

RectTransform MakeTransform(const UIPrimitive& primitive) {
    RectTransform transform;
    transform.scaleX = primitive.scaleX;
    transform.scaleY = primitive.scaleY;
    transform.rotationDegrees = primitive.rotation;
    transform.translateX = primitive.translateX;
    transform.translateY = primitive.translateY;
    return transform;
}

RectStyle MakeStyle(const UIPrimitive& primitive) {
    RectStyle style;
    style.color = ApplyOpacity(primitive.background, primitive.opacity);
    style.gradient = primitive.gradient;
    if (style.gradient.enabled) {
        style.gradient.topLeft = ApplyOpacity(style.gradient.topLeft, primitive.opacity);
        style.gradient.topRight = ApplyOpacity(style.gradient.topRight, primitive.opacity);
        style.gradient.bottomLeft = ApplyOpacity(style.gradient.bottomLeft, primitive.opacity);
        style.gradient.bottomRight = ApplyOpacity(style.gradient.bottomRight, primitive.opacity);
    }
    style.rounding = primitive.rounding;
    style.blurAmount = primitive.blur;
    style.shadowBlur = primitive.shadow.blur;
    style.shadowOffsetX = primitive.shadow.offsetX;
    style.shadowOffsetY = primitive.shadow.offsetY;
    style.shadowColor = ApplyOpacity(primitive.shadow.color, primitive.opacity);
    style.transform = MakeTransform(primitive);
    return style;
}

RectFrame PrimitiveFrame(const UIPrimitive& primitive) {
    return ResolveFrame(primitive.x, primitive.y, primitive.width, primitive.height, primitive.anchor);
}

bool PrimitiveContains(const UIPrimitive& primitive, float x, float y) {
    const RectFrame frame = PrimitiveFrame(primitive);
    return x >= frame.x && x <= frame.x + frame.width &&
           y >= frame.y && y <= frame.y + frame.height;
}

void RequestPrimitiveRepaint(const UIPrimitive& primitive, const RectStyle& style, float expand, float duration) {
    (void)style;
    (void)expand;
    (void)primitive;
    Renderer::RequestRepaint(duration);
}

PrimitiveClipScope::PrimitiveClipScope(const UIPrimitive& primitive) {
    GLint x = 0;
    GLint y = 0;
    GLint w = 0;
    GLint h = 0;
    if (!TryMakeScissorRect(primitive, x, y, w, h)) {
        return;
    }

    restoreEnabled_ = glIsEnabled(GL_SCISSOR_TEST) == GL_TRUE;
    if (restoreEnabled_) {
        glGetIntegerv(GL_SCISSOR_BOX, restoreBox_);
    }

    glEnable(GL_SCISSOR_TEST);
    glScissor(x, y, w, h);
    active_ = true;
}

PrimitiveClipScope::~PrimitiveClipScope() {
    if (!active_) {
        return;
    }

    if (restoreEnabled_) {
        glScissor(restoreBox_[0], restoreBox_[1], restoreBox_[2], restoreBox_[3]);
    } else {
        glDisable(GL_SCISSOR_TEST);
    }
}

} // namespace EUINEO
