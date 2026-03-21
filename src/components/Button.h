#pragma once
#include "../EUINEO.h"
#include <string>
#include <functional>

namespace EUINEO {

enum class ButtonStyle { Default, Primary, Outline };

class Button : public Widget {
public:
    std::string text;
    ButtonStyle style = ButtonStyle::Default;
    std::function<void()> onClick;
    
    float hoverAnim = 0.0f; // 0.0 -> 1.0
    float clickAnim = 0.0f; // 0.0 -> 1.0

    Button() = default;
    Button(std::string t, float x, float y, float w = 100, float h = 35);
    void Update() override;
    void Draw() override;
};

} // namespace EUINEO