#pragma once
#include "../EUINEO.h"

namespace EUINEO {

class ProgressBar : public Widget {
public:
    float value = 0.0f; // 0.0 -> 1.0
    float animatedValue = -1.0f; // 缓动值，初始为 -1 表示还未初始化
    
    ProgressBar() = default;
    ProgressBar(float x, float y, float w = 200, float h = 10);
    void Update() override;
    void Draw() override;
};

} // namespace EUINEO