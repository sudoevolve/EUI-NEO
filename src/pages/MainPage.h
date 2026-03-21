#pragma once
#include "../EUINEO.h"
#include "../components/Button.h"
#include "../components/ProgressBar.h"
#include "../components/Slider.h"
#include "../components/SegmentedControl.h"
#include "../components/Label.h"
#include "../components/InputBox.h"
#include "../components/ComboBox.h"

namespace EUINEO {

class MainPage {
public:
    Label titleLabel;
    Button btnPrimary;
    Button btnOutline;
    Button btnIcon;
    ProgressBar progBar;
    Slider slider;
    SegmentedControl segCtrl;
    InputBox inputBox;
    ComboBox comboBox;
    
    float testProgress = 0.0f;
    float testSliderVal = 50.0f;

    MainPage();
    void Update();
    void Draw();
};

} // namespace EUINEO