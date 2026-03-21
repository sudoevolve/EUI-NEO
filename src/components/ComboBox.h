#pragma once
#include "../EUINEO.h"
#include <string>
#include <vector>
#include <functional>

namespace EUINEO {

class ComboBox : public Widget {
public:
    std::vector<std::string> items;
    int selectedIndex = -1;
    bool isOpen = false;
    std::string placeholder;
    
    float hoverAnim = 0.0f; // 悬停动效
    float openAnim = 0.0f;  // 展开动效 (0.0 -> 1.0)
    std::vector<float> itemHoverAnims; // 列表项的悬停动效
    
    // Callbacks
    std::function<void(int)> onSelectionChanged;

    ComboBox(std::string placeholderText = "", float x = 0, float y = 0, float w = 150, float h = 35);

    void AddItem(const std::string& item);
    
    void Update() override;
    void Draw() override;
};

} // namespace EUINEO