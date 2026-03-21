#pragma once
#include "../EUINEO.h"

namespace EUINEO {

class Panel : public Widget {
public:
    Color color;
    RectGradient gradient;
    RectTransform transform;
    float rounding = 0.0f;
    float blurAmount = 0.0f;
    float shadowBlur = 0.0f;
    float shadowOffsetX = 0.0f;
    float shadowOffsetY = 0.0f;
    Color shadowColor = Color(0, 0, 0, 0);

    Panel() = default;
    Panel(float x, float y, float w, float h);

    using Widget::MarkDirty;

    void Update() override;
    void Draw() override;
    RectStyle GetStyle() const;
    void SetStyle(const RectStyle& style);
    void MarkDirty(float expand = 0.0f, float duration = 0.0f);
};

} // namespace EUINEO
