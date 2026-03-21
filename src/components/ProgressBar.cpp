#include "ProgressBar.h"

namespace EUINEO {

ProgressBar::ProgressBar(float x, float y, float w, float h) {
    this->x = x; this->y = y; this->width = w; this->height = h;
}

void ProgressBar::Update() {
    // 首次初始化时，直接让动画值等于目标值，避免从 0 开始的动画
    if (animatedValue < 0.0f) {
        animatedValue = value;
    }
    // 进度条平滑动效
    if (std::abs(animatedValue - value) > 0.001f) {
        animatedValue = Lerp(animatedValue, value, 10.0f * State.deltaTime);
        Renderer::RequestRepaint();
    }
}

void ProgressBar::Draw() {
    float absX, absY;
    GetAbsoluteBounds(absX, absY);
    
    float cornerRadius = height / 2.0f; // 药丸形状圆角
    // 底色
    Renderer::DrawRect(absX, absY, width, height, CurrentTheme->surfaceHover, cornerRadius);
    // 进度色
    if (animatedValue > 0.01f) {
        Renderer::DrawRect(absX, absY, width * animatedValue, height, CurrentTheme->primary, cornerRadius);
    }
}

} // namespace EUINEO