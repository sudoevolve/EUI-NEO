#pragma once

#include "../EUINEO.h"

namespace EUINEO {

struct UIClipRect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

struct UIShadow {
    float blur = 0.0f;
    float offsetX = 0.0f;
    float offsetY = 0.0f;
    Color color = Color(0.0f, 0.0f, 0.0f, 0.0f);
};

struct UIPrimitive {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float minWidth = 0.0f;
    float minHeight = 0.0f;
    float maxWidth = 0.0f;
    float maxHeight = 0.0f;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    float rotation = 0.0f;
    float translateX = 0.0f;
    float translateY = 0.0f;
    Anchor anchor = Anchor::TopLeft;
    float rounding = 0.0f;
    Color background = Color(0.0f, 0.0f, 0.0f, 0.0f);
    RectGradient gradient;
    float borderWidth = 0.0f;
    Color borderColor = Color(0.0f, 0.0f, 0.0f, 0.0f);
    float blur = 0.0f;
    UIShadow shadow;
    float opacity = 1.0f;
    bool visible = true;
    bool enabled = true;
    RenderLayer renderLayer = RenderLayer::Content;
    int zIndex = 0;
    bool hasClipRect = false;
    UIClipRect clipRect;
};

Color ApplyOpacity(Color color, float opacity);
RectTransform MakeTransform(const UIPrimitive& primitive);
RectStyle MakeStyle(const UIPrimitive& primitive);
RectFrame PrimitiveFrame(const UIPrimitive& primitive);
bool PrimitiveContains(const UIPrimitive& primitive, float x, float y);
void RequestPrimitiveRepaint(const UIPrimitive& primitive, const RectStyle& style,
                             float expand = 0.0f, float duration = 0.0f);

class PrimitiveClipScope {
public:
    explicit PrimitiveClipScope(const UIPrimitive& primitive);
    ~PrimitiveClipScope();

    PrimitiveClipScope(const PrimitiveClipScope&) = delete;
    PrimitiveClipScope& operator=(const PrimitiveClipScope&) = delete;

private:
    bool active_ = false;
    bool restoreEnabled_ = false;
    GLint restoreBox_[4] = {0, 0, 0, 0};
};

} // namespace EUINEO
