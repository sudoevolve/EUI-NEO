#pragma once
#include "../EUINEO.h"
#include <string>
#include <vector>
#include <functional>

namespace EUINEO {

class SegmentedControl : public Widget {
public:
    std::vector<std::string> options;
    int selectedIndex = 0;
    float indicatorAnim = 0.0f;
    std::function<void(int)> onSelectionChanged;

    SegmentedControl() = default;
    SegmentedControl(std::vector<std::string> opts, float x, float y, float w = 300, float h = 35);
    void Update() override;
    void Draw() override;
};

} // namespace EUINEO