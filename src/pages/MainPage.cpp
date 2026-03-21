#include "MainPage.h"

namespace EUINEO {

MainPage::MainPage() {
    // 标题标签配置
    titleLabel = Label("EUI-NEO", 0, -20);
    titleLabel.anchor = Anchor::TopCenter;
    titleLabel.fontSize = 32.0f;

    // 主要按钮配置
    btnPrimary = Button("Primary", -70, 70, 120, 40);
    btnPrimary.anchor = Anchor::TopCenter;
    btnPrimary.style = ButtonStyle::Primary;
    btnPrimary.fontSize = 20.0f;
    btnPrimary.onClick = [this]() {
        progBar.value += 0.1f;
        if (progBar.value > 1.0f) progBar.value = 0.0f;
        CurrentTheme = (CurrentTheme == &DarkTheme) ? &LightTheme : &DarkTheme;
    };

    // 轮廓按钮配置
    btnOutline = Button("Outline", 70, 70, 120, 40);
    btnOutline.anchor = Anchor::TopCenter;
    btnOutline.style = ButtonStyle::Outline;
    btnOutline.fontSize = 20.0f;
    btnOutline.onClick = [this]() {
        slider.value = 0.0f;
    };
    
    // 图标按钮配置 (使用多个空格作为占位，拉开文本和图标的间距)
    btnIcon = Button("Icon  \xEF\x80\x93", 0, 130, 120, 40);
    btnIcon.anchor = Anchor::TopCenter;
    btnIcon.style = ButtonStyle::Default;
    btnIcon.fontSize = 20.0f;

    // 进度条配置
    progBar = ProgressBar(0, -60, 300, 15);
    progBar.anchor = Anchor::Center;
    progBar.value = 0.3f; // 初始进度

    // 滑块配置
    slider = Slider(0, -10, 300, 20);
    slider.anchor = Anchor::Center;
    slider.onValueChanged = [this](float val) {
        progBar.value = val;
    };

    // 分段控制器配置
    segCtrl = SegmentedControl({"Apple", "Banana", "Cherry"}, 0, 40, 300, 35);
    segCtrl.anchor = Anchor::Center;
    segCtrl.fontSize = 20.0f;
    
    // 输入框配置
    inputBox = InputBox("Type something...", 0, 100, 300, 35);
    inputBox.anchor = Anchor::Center;
    inputBox.fontSize = 20.0f;
    
    // 下拉框配置
    comboBox = ComboBox("Select an option", 0, 160, 300, 35);
    comboBox.anchor = Anchor::Center;
    comboBox.fontSize = 20.0f;
    comboBox.AddItem("Item 1");
    comboBox.AddItem("Item 2");
    comboBox.AddItem("Item 3");
}

void MainPage::Update() {
    btnPrimary.Update();
    btnOutline.Update();
    btnIcon.Update();
    progBar.Update();
    slider.Update();
    segCtrl.Update();
    inputBox.Update();
    // Dropdown needs to be updated last to handle click outside
    comboBox.Update();
}

void MainPage::Draw() {
    titleLabel.Draw();
    btnPrimary.Draw();
    btnOutline.Draw();
    btnIcon.Draw();
    progBar.Draw();
    slider.Draw();
    segCtrl.Draw();
    inputBox.Draw();
    // Dropdown drawn last to overlap other components
    comboBox.Draw();
}

} // namespace EUINEO