#include "InputBox.h"
#include <GLFW/glfw3.h> // For key codes

namespace EUINEO {

InputBox::InputBox(std::string placeholderText, float x, float y, float w, float h) 
    : placeholder(placeholderText) {
    this->x = x;
    this->y = y;
    this->width = w;
    this->height = h;
}

void InputBox::Update() {
    bool hovered = IsHovered();
    
    // 悬停动效
    float targetHover = hovered ? 1.0f : 0.0f;
    if (hoverAnim != targetHover) {
        hoverAnim = Lerp(hoverAnim, targetHover, State.deltaTime * 15.0f);
        if (std::abs(hoverAnim - targetHover) < 0.01f) hoverAnim = targetHover;
        Renderer::RequestRepaint();
    }
    
    // 聚焦动效
    float targetFocus = isFocused ? 1.0f : 0.0f;
    if (focusAnim != targetFocus) {
        focusAnim = Lerp(focusAnim, targetFocus, State.deltaTime * 15.0f);
        if (std::abs(focusAnim - targetFocus) < 0.01f) focusAnim = targetFocus;
        Renderer::RequestRepaint();
    }
    
    if (State.mouseClicked) {
        isFocused = hovered;
        if (isFocused) {
            cursorBlinkTime = 0.0f;
            Renderer::RequestRepaint();
        }
    }
    
    if (isFocused) {
        cursorBlinkTime += State.deltaTime;
        if (cursorBlinkTime >= 1.0f) {
            cursorBlinkTime -= 1.0f;
        }
        Renderer::RequestRepaint(1.0f); // keep repainting for cursor blink
        
        // Handle text input
        if (!State.textInput.empty()) {
            // Very simple insert at the end for now. Proper UTF-8 insert would need to respect cursor position.
            text += State.textInput;
            cursorPosition = (int)text.length(); // Simplified
            cursorBlinkTime = 0.0f; // reset blink
            if (onTextChanged) onTextChanged(text);
            Renderer::RequestRepaint();
        }
        
        // Handle backspace
        if (State.keysPressed[GLFW_KEY_BACKSPACE]) {
            if (!text.empty()) {
                // Simplistic UTF-8 backspace (remove last codepoint)
                while (!text.empty() && (text.back() & 0xC0) == 0x80) {
                    text.pop_back();
                }
                if (!text.empty()) text.pop_back();
                cursorPosition = (int)text.length();
                cursorBlinkTime = 0.0f;
                if (onTextChanged) onTextChanged(text);
                Renderer::RequestRepaint();
            }
        }
        
        // Handle enter
        if (State.keysPressed[GLFW_KEY_ENTER] || State.keysPressed[GLFW_KEY_KP_ENTER]) {
            if (onEnter) onEnter();
            Renderer::RequestRepaint();
        }
    }
}

void InputBox::Draw() {
    float absX, absY;
    GetAbsoluteBounds(absX, absY);

    // Background
    Color baseColor = Lerp(CurrentTheme->surface, CurrentTheme->surfaceActive, focusAnim);
    Color hoverColor = Lerp(CurrentTheme->surfaceHover, CurrentTheme->surfaceActive, focusAnim);
    Color bgColor = Lerp(baseColor, hoverColor, hoverAnim);
    
    Renderer::DrawRect(absX, absY, width, height, bgColor, 6.0f);
    
    // Border
    if (focusAnim > 0.01f) {
        // base border
        Renderer::DrawRect(absX, absY, width, 1.0f, CurrentTheme->border, 0.0f); 
        // focus highlight
        Color focusColor = CurrentTheme->primary;
        focusColor.a = focusAnim;
        Renderer::DrawRect(absX, absY, width, 2.0f, focusColor, 0.0f); 
    } else {
        Renderer::DrawRect(absX, absY, width, 1.0f, CurrentTheme->border, 0.0f); // bottom border
    }
    
    // Text
    float textScale = fontSize / 24.0f;
    // 以 baseline 作为渲染基准线，将其置于偏下位置以实现垂直居中
    float textY = absY + height / 2.0f + (fontSize / 4.0f); 
    float textX = absX + 10.0f;
    
    if (text.empty()) {
        if (!placeholder.empty()) {
            Color phColor = CurrentTheme->text;
            phColor.a = 0.5f;
            Renderer::DrawTextStr(placeholder, textX, textY, phColor, textScale);
        }
    } else {
        Renderer::DrawTextStr(text, textX, textY, CurrentTheme->text, textScale);
    }
    
    // Cursor
    if (isFocused && cursorBlinkTime < 0.5f) {
        float cursorX = textX + (text.empty() ? 0 : Renderer::MeasureTextWidth(text, textScale));
        Renderer::DrawRect(cursorX, absY + height / 2.0f - 10.0f, 2.0f, 20.0f, CurrentTheme->primary, 1.0f);
    }
}

} // namespace EUINEO