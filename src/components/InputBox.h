#pragma once
#include "../EUINEO.h"
#include <string>
#include <functional>

namespace EUINEO {

class InputBox : public Widget {
public:
    std::string text;
    std::string placeholder;
    bool isFocused = false;
    float cursorBlinkTime = 0.0f;
    int cursorPosition = 0;
    
    float hoverAnim = 0.0f; // 0.0 -> 1.0 悬停动效
    float focusAnim = 0.0f; // 0.0 -> 1.0 聚焦动效
    
    // Callbacks
    std::function<void(const std::string&)> onTextChanged;
    std::function<void()> onEnter;

    InputBox(std::string placeholderText = "", float x = 0, float y = 0, float w = 200, float h = 35);

    void Update() override;
    void Draw() override;
};

} // namespace EUINEO