#include "Button.h"

namespace EUINEO {

Button::Button(std::string t, float x, float y, float w, float h) {
    this->text = t; this->x = x; this->y = y; this->width = w; this->height = h;
}

void Button::Update() {
    bool hovered = IsHovered();
    // 动效：缓动插值
    float speed = 15.0f * State.deltaTime;
    hoverAnim = Lerp(hoverAnim, hovered ? 1.0f : 0.0f, speed);
    
    if (hovered && State.mouseDown) {
        clickAnim = Lerp(clickAnim, 1.0f, speed * 2.0f);
    } else {
        clickAnim = Lerp(clickAnim, 0.0f, speed);
    }

    if (hovered) {
        Renderer::RequestRepaint(0.1f); // 悬停动效持续刷新
    } else if (hoverAnim > 0.01f) {
        Renderer::RequestRepaint(0.1f); // 恢复动效持续刷新
    }
    if (clickAnim > 0.01f) Renderer::RequestRepaint(0.1f);

    if (hovered && State.mouseClicked && onClick) {
        onClick();
        Renderer::RequestRepaint(0.2f); // 点击动效持续刷新
    }
}

void Button::Draw() {
    float absX, absY;
    GetAbsoluteBounds(absX, absY);
    
    Color baseColor = (style == ButtonStyle::Primary) ? CurrentTheme->primary : CurrentTheme->surface;
    Color hoverColor = (style == ButtonStyle::Primary) ? Lerp(CurrentTheme->primary, Color(1,1,1,1), 0.2f) : CurrentTheme->surfaceHover;
    Color activeColor = (style == ButtonStyle::Primary) ? Lerp(CurrentTheme->primary, Color(0,0,0,1), 0.2f) : CurrentTheme->surfaceActive;

    Color drawColor = Lerp(baseColor, hoverColor, hoverAnim);
    drawColor = Lerp(drawColor, activeColor, clickAnim);
    
    float cornerRadius = 6.0f; // 现代化圆角
    
    // 边框(伪)
    if (style == ButtonStyle::Outline) {
        Renderer::DrawRect(absX, absY, width, height, CurrentTheme->border, cornerRadius);
        Renderer::DrawRect(absX+1, absY+1, width-2, height-2, CurrentTheme->background, cornerRadius - 1.0f);
    } else {
        Renderer::DrawRect(absX, absY, width, height, drawColor, cornerRadius);
    }

    // 绘制文本 (居中)
    float textScale = fontSize / 24.0f;
    float textWidth = Renderer::MeasureTextWidth(text, textScale);
    float textX = absX + (width - textWidth) / 2.0f;
    // OpenGL 中字体渲染的基础线 (baseline) 通常在字体高度的偏下方
    // 简单居中：将 y 坐标设置在组件中间偏下的位置，大约加上字体高度的 1/4 作为修正
    float textY = absY + (height / 2.0f) + (fontSize / 4.0f); 
    Color textColor = (style == ButtonStyle::Primary) ? Color(1,1,1,1) : CurrentTheme->text;
    Renderer::DrawTextStr(text, textX, textY, textColor, textScale);
}

} // namespace EUINEO