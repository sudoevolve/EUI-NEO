#pragma once
#include "../EUINEO.h"
#include <string>

namespace EUINEO {

class Label : public Widget {
public:
    std::string text;
    Color color;
    bool useThemeColor = true;
    
    Label() = default;
    Label(std::string t, float x, float y);
    Label(std::string t, float x, float y, const Color& c);
    
    void Update() override;
    void Draw() override;
};

} // namespace EUINEO