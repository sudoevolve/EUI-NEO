#include "MainPage.h"
#include <algorithm>
#include <cmath>

namespace EUINEO {

static bool FloatEq(float a, float b, float epsilon = 0.0001f) {
    return std::abs(a - b) <= epsilon;
}

static bool ColorEq(const Color& a, const Color& b, float epsilon = 0.0001f) {
    return FloatEq(a.r, b.r, epsilon) &&
           FloatEq(a.g, b.g, epsilon) &&
           FloatEq(a.b, b.b, epsilon) &&
           FloatEq(a.a, b.a, epsilon);
}

static void MarkPanelDirty(Panel& panel) {
    panel.MarkDirty();
}

MainPage::MainPage() {
    bgCircle1 = Panel(-100, -100, 100, 100);
    bgCircle1.anchor = Anchor::Center;
    bgCircle1.color = Color(1.0f, 0.2f, 0.2f, 1.0f);
    bgCircle1.rounding = 50.0f;

    bgCircle2 = Panel(100, -100, 100, 100);
    bgCircle2.anchor = Anchor::Center;
    bgCircle2.color = Color(0.2f, 1.0f, 0.2f, 1.0f);
    bgCircle2.rounding = 50.0f;

    bgCircle3 = Panel(0, 0, 100, 100);
    bgCircle3.anchor = Anchor::Center;
    bgCircle3.color = Color(0.2f, 0.2f, 1.0f, 1.0f);
    bgCircle3.rounding = 50.0f;

    glassCard = Panel(0, 50, 340, 360);
    glassCard.anchor = Anchor::Center;
    glassCard.color = CurrentTheme->surface;
    glassCard.color.a = 0.6f;
    glassCard.rounding = 16.0f;
    glassCard.shadowBlur = 20.0f;
    glassCard.shadowOffsetY = 10.0f;
    glassCard.shadowColor = Color(0.0f, 0.0f, 0.0f, 0.3f);

    titleLabel = Label("EUI-NEO", 0, 30);
    titleLabel.anchor = Anchor::TopCenter;
    titleLabel.fontSize = 32.0f;

    btnPrimary = Button("Primary", -70, 50, 120, 40);
    btnPrimary.anchor = Anchor::TopCenter;
    btnPrimary.style = ButtonStyle::Primary;
    btnPrimary.fontSize = 20.0f;
    btnPrimary.onClick = [this]() {
        progBar.value += 0.1f;
        if (progBar.value > 1.0f) progBar.value = 0.0f;
        CurrentTheme = (CurrentTheme == &DarkTheme) ? &LightTheme : &DarkTheme;
        Renderer::InvalidateBackdrop();
    };

    btnOutline = Button("Outline", 70, 50, 120, 40);
    btnOutline.anchor = Anchor::TopCenter;
    btnOutline.style = ButtonStyle::Outline;
    btnOutline.fontSize = 20.0f;
    btnOutline.onClick = [this]() {
        slider.value = 0.0f;
    };

    btnIcon = Button("Icon  \xEF\x80\x93", 0, 110, 120, 40);
    btnIcon.anchor = Anchor::TopCenter;
    btnIcon.style = ButtonStyle::Default;
    btnIcon.fontSize = 20.0f;

    progBar = ProgressBar(0, -60, 300, 15);
    progBar.anchor = Anchor::Center;
    progBar.value = 0.3f;

    slider = Slider(0, -10, 300, 20);
    slider.anchor = Anchor::Center;
    slider.onValueChanged = [this](float val) {
        progBar.value = val;
    };

    segCtrl = SegmentedControl({"Apple", "Banana", "Cherry"}, 0, 40, 300, 35);
    segCtrl.anchor = Anchor::Center;
    segCtrl.fontSize = 20.0f;

    inputBox = InputBox("Type something...", 0, 100, 300, 35);
    inputBox.anchor = Anchor::Center;
    inputBox.fontSize = 20.0f;

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
    comboBox.Update();

    glassCard.color = CurrentTheme->surface;
    glassCard.color.a = 0.6f;
    glassCard.blurAmount = slider.value * 0.15f;

    if (!hasPreviousGlassState ||
        !FloatEq(previousGlassBlurAmount, glassCard.blurAmount) ||
        !ColorEq(previousGlassColor, glassCard.color)) {
        MarkPanelDirty(glassCard);
        previousGlassBlurAmount = glassCard.blurAmount;
        previousGlassColor = glassCard.color;
        hasPreviousGlassState = true;
    }
}

void MainPage::Draw() {
    bgCircle1.Draw();
    bgCircle2.Draw();
    bgCircle3.Draw();

    glassCard.Draw();

    titleLabel.Draw();
    btnPrimary.Draw();
    btnOutline.Draw();
    btnIcon.Draw();
    progBar.Draw();
    slider.Draw();
    segCtrl.Draw();
    inputBox.Draw();
    comboBox.Draw();
}

} // namespace EUINEO
