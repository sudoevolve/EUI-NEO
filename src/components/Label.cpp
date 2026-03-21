#include "Label.h"

namespace EUINEO {

Label::Label(std::string t, float x, float y) {
    this->text = t; this->x = x; this->y = y; this->useThemeColor = true;
}

Label::Label(std::string t, float x, float y, const Color& c) {
    this->text = t; this->x = x; this->y = y; this->color = c; this->useThemeColor = false;
}

void Label::Update() {}

void Label::Draw() {
    float absX, absY;
    GetAbsoluteBounds(absX, absY);
    Color drawColor = useThemeColor ? CurrentTheme->text : color;
    float textScale = fontSize / 24.0f; // 基于 24.0f 的基础字号进行缩放
    Renderer::DrawTextStr(text, absX, absY, drawColor, textScale);
}

} // namespace EUINEO