#include "Panel.h"

namespace EUINEO {

Panel::Panel(float x, float y, float w, float h) {
    this->x = x;
    this->y = y;
    this->width = w;
    this->height = h;
    this->color = CurrentTheme ? CurrentTheme->surface : Color(1, 1, 1, 1);
}

void Panel::Update() {
}

RectStyle Panel::GetStyle() const {
    RectStyle style;
    style.color = color;
    style.gradient = gradient;
    style.rounding = rounding;
    style.blurAmount = blurAmount;
    style.shadowBlur = shadowBlur;
    style.shadowOffsetX = shadowOffsetX;
    style.shadowOffsetY = shadowOffsetY;
    style.shadowColor = shadowColor;
    style.transform = transform;
    return style;
}

void Panel::SetStyle(const RectStyle& style) {
    color = style.color;
    gradient = style.gradient;
    rounding = style.rounding;
    blurAmount = style.blurAmount;
    shadowBlur = style.shadowBlur;
    shadowOffsetX = style.shadowOffsetX;
    shadowOffsetY = style.shadowOffsetY;
    shadowColor = style.shadowColor;
    transform = style.transform;
}

void Panel::Draw() {
    float absX = 0.0f;
    float absY = 0.0f;
    GetAbsoluteBounds(absX, absY);
    Renderer::DrawRect(absX, absY, width, height, GetStyle());
}

void Panel::MarkDirty(float expand, float duration) {
    Widget::MarkDirty(GetStyle(), expand, duration);
}

} // namespace EUINEO
