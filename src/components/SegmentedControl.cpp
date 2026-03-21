#include "SegmentedControl.h"
#include <algorithm>
#include <cmath>

namespace EUINEO {

SegmentedControl::SegmentedControl(std::vector<std::string> opts, float x, float y, float w, float h) {
    this->options = opts; this->x = x; this->y = y; this->width = w; this->height = h;
}

void SegmentedControl::Update() {
    float absX, absY;
    GetAbsoluteBounds(absX, absY);

    bool isHovered = (State.mouseX >= absX && State.mouseX <= absX + width &&
                      State.mouseY >= absY && State.mouseY <= absY + height);

    if (isHovered && State.mouseClicked) {
        float relX = State.mouseX - absX;
        float segW = width / options.size();
        selectedIndex = std::clamp((int)(relX / segW), 0, (int)options.size() - 1);
        if (onSelectionChanged) onSelectionChanged(selectedIndex);
        Renderer::RequestRepaint(0.5f); // 切换动效较长
    }

    float targetAnim = (float)selectedIndex;
    indicatorAnim = Lerp(indicatorAnim, targetAnim, State.deltaTime * 15.0f);
    if (std::abs(indicatorAnim - targetAnim) > 0.01f) {
        Renderer::RequestRepaint(0.1f);
    }
}

void SegmentedControl::Draw() {
    float absX, absY;
    GetAbsoluteBounds(absX, absY);
    
    float cornerRadius = 6.0f;
    // 大底框
    Renderer::DrawRect(absX, absY, width, height, CurrentTheme->surface, cornerRadius);
    
    // 激活的滑块背景 (留出 2px 边距)
    float segW = width / options.size();
    float indX = absX + indicatorAnim * segW;
    Renderer::DrawRect(indX + 2, absY + 2, segW - 4, height - 4, CurrentTheme->primary, cornerRadius - 1.0f);

    // 绘制文本
    float textScale = fontSize / 24.0f;
    for (size_t i = 0; i < options.size(); ++i) {
        float textWidth = Renderer::MeasureTextWidth(options[i], textScale);
        float textX = absX + i * segW + (segW - textWidth) / 2.0f;
        float textY = absY + (height / 2.0f) + (fontSize / 4.0f);
        Color textColor = (i == std::round(indicatorAnim)) ? Color(1,1,1,1) : CurrentTheme->text;
        Renderer::DrawTextStr(options[i], textX, textY, textColor, textScale);
    }
}

} // namespace EUINEO