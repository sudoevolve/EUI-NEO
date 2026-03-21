#include "Slider.h"
#include <algorithm>
#include <cmath>

namespace EUINEO {

Slider::Slider(float x, float y, float w, float h) {
    this->x = x; this->y = y; this->width = w; this->height = h;
}

void Slider::Update() {
    float absX, absY;
    GetAbsoluteBounds(absX, absY);

    float handleW = 10.0f;
    float handleX = absX + value * width - handleW / 2.0f;
    float handleY = absY + (height - 16.0f) / 2.0f;

    bool isHovered = (State.mouseX >= handleX && State.mouseX <= handleX + handleW &&
                      State.mouseY >= handleY && State.mouseY <= handleY + 16.0f);

    if (isHovered || isDragging) {
        hoverAnim = Lerp(hoverAnim, 1.0f, State.deltaTime * 15.0f);
        Renderer::RequestRepaint(0.1f);
    } else {
        hoverAnim = Lerp(hoverAnim, 0.0f, State.deltaTime * 10.0f);
        if (hoverAnim > 0.01f) Renderer::RequestRepaint(0.1f);
    }

    if (isHovered && State.mouseDown && !State.mouseClicked) {
        isDragging = true;
    }
    if (!State.mouseDown) {
        isDragging = false;
    }

    if (isDragging) {
        float relX = State.mouseX - absX;
        value = std::clamp(relX / width, 0.0f, 1.0f);
        if (onValueChanged) onValueChanged(value);
        Renderer::RequestRepaint(0.1f);
    }
}

void Slider::Draw() {
    float absX, absY;
    GetAbsoluteBounds(absX, absY);
    
    // 轨道
    float trackH = 4.0f; // 细一点的轨道更现代
    float trackY = absY + (height - trackH) / 2.0f;
    Renderer::DrawRect(absX, trackY, width, trackH, CurrentTheme->surface, trackH / 2.0f);
    
    // 激活的轨道
    if (value > 0.01f) {
        Renderer::DrawRect(absX, trackY, width * value, trackH, CurrentTheme->primary, trackH / 2.0f);
    }
    
    // 滑块
    float handleRadius = 8.0f; // 圆形滑块
    float handleX = absX + value * width - handleRadius;
    float handleY = absY + (height - handleRadius * 2.0f) / 2.0f;
    
    Color handleColor = Lerp(CurrentTheme->text, CurrentTheme->primary, hoverAnim);
    Renderer::DrawRect(handleX, handleY, handleRadius * 2.0f, handleRadius * 2.0f, handleColor, handleRadius);
}

} // namespace EUINEO