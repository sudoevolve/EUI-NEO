#pragma once
#include "../EUINEO.h"
#include <functional>

namespace EUINEO {

class Slider : public Widget {
public:
    float value = 0.5f;
    bool isDragging = false;
    bool isEditing = false;
    float hoverAnim = 0.0f;
    std::function<void(float)> onValueChanged;

    Slider() = default;
    Slider(float x, float y, float w, float h);
    void Update() override;
    void Draw() override;
};

} // namespace EUINEO