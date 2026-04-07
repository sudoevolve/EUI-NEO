#pragma once

#include "AnimationPage.h"
#include "AboutPage.h"
#include "HomePage.h"
#include "LayoutPage.h"
#include "TypographyPage.h"
#include "../ui/UIContext.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace EUINEO {

enum class MainPageView {
    Home = 0,
    Animation = 1,
    Layout = 2,
    Typography = 3,
    About = 4,
};

class MainPage {
public:
    MainPage() = default;

    void Update() {
        if (pageReveal_ < 1.0f) {
            const float previous = pageReveal_;
            pageReveal_ = Lerp(pageReveal_, 1.0f, State.deltaTime * 11.0f);
            if (std::abs(1.0f - pageReveal_) < 0.01f) {
                pageReveal_ = 1.0f;
            }
            if (std::abs(previous - pageReveal_) > 0.0001f) {
                ui_.requestVisualRefresh(0.18f);
            }
        }

        const std::uint64_t versionBeforeUpdate = stateVersion_;
        Compose();
        ui_.update();
        if (hasPendingViewSwitch_) {
            const MainPageView target = pendingView_;
            hasPendingViewSwitch_ = false;
            SwitchView(target);
        }
        if (stateVersion_ != versionBeforeUpdate || ui_.consumeRecomposeRequest()) {
            Compose();
        }
    }

    void Draw() {
        State.imeCursorActive = false;
        ui_.render();
    }

    bool WantsContinuousUpdate() const {
        return ui_.wantsContinuousUpdate();
    }

private:
    void Compose() {
        const Layout layout = MakeLayout();

        ui_.begin("main");

        ui_.sidebar("sidebar")
            .position(layout.sidebarX, layout.sidebarY)
            .size(sidebarWidth_, layout.sidebarH)
            .width(60.0f, sidebarWidth_)
            .layer(RenderLayer::Chrome)
            .brand("EUI", "NEO")
            .selectedIndex(static_cast<int>(currentView_))
            .item("\xEF\x80\x95", "Home", [this] { QueueViewSwitch(MainPageView::Home); })
            .item("\xEF\x81\x8B", "Animation", [this] { QueueViewSwitch(MainPageView::Animation); })
            .item("\xEF\x80\x89", "Layout", [this] { QueueViewSwitch(MainPageView::Layout); })
            .item("\xEF\x80\xB1", "Typography", [this] { QueueViewSwitch(MainPageView::Typography); })
            .item("\xEF\x81\x9A", "About", [this] { QueueViewSwitch(MainPageView::About); })
            .themeToggle([this] { ToggleTheme(); })
            .build();

        ui_.glassPanel("content")
            .position(layout.contentX, layout.contentY)
            .size(layout.contentW, layout.contentH)
            .rounding(16.0f)
            .blur(progressValue_ * 0.15f)
            .layer(RenderLayer::Backdrop)
            .zIndex(-2)
            .build();

        ui_.panel("bg.red")
            .position(layout.contentX + layout.contentW * 0.10f - 84.0f, layout.contentY + 58.0f)
            .size(196.0f, 196.0f)
            .background(0.98f, 0.36f, 0.36f, 0.92f)
            .layer(RenderLayer::Backdrop)
            .rounding(98.0f)
            .zIndex(-3)
            .build();

        ui_.panel("bg.green")
            .position(layout.contentX + layout.contentW * 0.58f, layout.contentY + 86.0f)
            .size(164.0f, 164.0f)
            .background(0.30f, 0.92f, 0.58f, 0.88f)
            .layer(RenderLayer::Backdrop)
            .rounding(82.0f)
            .zIndex(-3)
            .build();

        ui_.panel("bg.blue")
            .position(layout.contentX + layout.contentW * 0.30f, layout.contentY + layout.contentH * 0.44f)
            .size(246.0f, 246.0f)
            .background(0.34f, 0.52f, 1.0f, 0.90f)
            .layer(RenderLayer::Backdrop)
            .rounding(123.0f)
            .zIndex(-3)
            .build();

        ui_.pushClip(layout.contentX, layout.contentY, layout.contentW, layout.contentH);
        ComposeCurrentPage(PageBounds());
        ui_.popClip();
        ui_.end();
    }

    void ToggleTheme() {
        CurrentTheme = CurrentTheme == &DarkTheme ? &LightTheme : &DarkTheme;
        ++stateVersion_;
        ui_.requestThemeRefresh(0.18f);
    }

    void SwitchView(MainPageView view) {
        if (view == currentView_) {
            return;
        }
        const int previousIndex = static_cast<int>(currentView_);
        const int nextIndex = static_cast<int>(view);
        currentView_ = view;
        pageReveal_ = 0.0f;
        pageRevealDirection_ = nextIndex >= previousIndex ? 1 : -1;
        ++stateVersion_;
        ui_.requestVisualRefresh(0.18f);
    }

    void QueueViewSwitch(MainPageView view) {
        if (hasPendingViewSwitch_ && pendingView_ == view) {
            return;
        }
        pendingView_ = view;
        hasPendingViewSwitch_ = true;
    }

    struct Layout {
        float sidebarX = 0.0f;
        float sidebarY = 0.0f;
        float sidebarH = 0.0f;
        float contentX = 0.0f;
        float contentY = 0.0f;
        float contentW = 0.0f;
        float contentH = 0.0f;
        float currentContentOffsetX = 0.0f;
    };

    Layout MakeLayout() const {
        Layout layout;
        layout.sidebarX = shellPadding_;
        layout.sidebarY = shellPadding_;
        layout.sidebarH = std::max(240.0f, State.screenH - shellPadding_ * 2.0f);
        layout.contentX = layout.sidebarX + sidebarWidth_ + 24.0f;
        layout.contentY = shellPadding_;
        layout.contentW = std::max(280.0f, State.screenW - layout.contentX - shellPadding_);
        layout.contentH = std::max(240.0f, State.screenH - shellPadding_ * 2.0f);
        layout.currentContentOffsetX = (1.0f - pageReveal_) * 28.0f * static_cast<float>(pageRevealDirection_);
        return layout;
    }

    RectFrame PageBounds() const {
        const Layout layout = MakeLayout();
        RectFrame frame;
        frame.x = layout.contentX + contentInset_ + layout.currentContentOffsetX;
        frame.y = layout.contentY + 18.0f;
        frame.width = layout.contentW - contentInset_ * 2.0f;
        frame.height = layout.contentH - 36.0f;
        return frame;
    }

    void ComposeCurrentPage(const RectFrame& bounds) {
        switch (currentView_) {
        case MainPageView::Home: {
            HomePage::Actions actions;
            actions.onRandomizeThemeColor = [this] { RandomizeThemeColor(); };
            actions.onToggleIconAccent = [this] { ToggleHomeIconAccent(); };
            actions.onProgressChange = [this](float value) { SetProgressValue(value); };
            actions.onSegmentedChange = [this](int index) { SetSegmentedIndex(index); };
            actions.onInputChange = [this](const std::string& text) { SetInputText(text); };
            actions.onTextAreaChange = [this](const std::string& text) { SetTextAreaText(text); };
            actions.onComboChange = [this](int index) { SetComboSelection(index); };

            HomePage::Compose(
                ui_,
                "home.page",
                bounds,
                homeIconAccentEnabled_,
                progressValue_,
                segmentedIndex_,
                inputText_,
                textAreaText_,
                comboSelection_,
                actions
            );
            break;
        }
        case MainPageView::Animation:
            AnimationPage::Compose(ui_, "animation.page", bounds);
            break;
        case MainPageView::Layout: {
            LayoutPage::Actions actions;
            actions.onSplitChange = [this](float value) { SetLayoutSplit(value); };
            LayoutPage::Compose(ui_, "layout.page", bounds, layoutSplit_, actions);
            break;
        }
        case MainPageView::Typography:
            TypographyPage::Compose(ui_, "typography.page", bounds);
            break;
        case MainPageView::About:
            AboutPage::Compose(ui_, "about.page", bounds);
            break;
        }
    }

    void SetProgressValue(float value) {
        const float clamped = std::clamp(value, 0.0f, 1.0f);
        if (std::abs(progressValue_ - clamped) < 0.0001f) {
            return;
        }
        progressValue_ = clamped;
        ++stateVersion_;
        ui_.requestVisualRefresh(0.18f);
    }

    void SetSegmentedIndex(int index) {
        if (segmentedIndex_ == index) {
            return;
        }
        segmentedIndex_ = index;
        ++stateVersion_;
    }

    void SetInputText(const std::string& text) {
        if (inputText_ == text) {
            return;
        }
        inputText_ = text;
        ++stateVersion_;
    }

    void SetComboSelection(int index) {
        if (comboSelection_ == index) {
            return;
        }
        comboSelection_ = index;
        ++stateVersion_;
    }

    void SetTextAreaText(const std::string& text) {
        if (textAreaText_ == text) {
            return;
        }
        textAreaText_ = text;
        ++stateVersion_;
    }

    void ToggleHomeIconAccent() {
        homeIconAccentEnabled_ = !homeIconAccentEnabled_;
        ++stateVersion_;
    }

    void RandomizeThemeColor() {
        static const std::array<Color, 10> accentPalette{{
            Color(0.20f, 0.50f, 0.90f),
            Color(0.12f, 0.72f, 0.78f),
            Color(0.15f, 0.78f, 0.48f),
            Color(0.88f, 0.42f, 0.18f),
            Color(0.92f, 0.28f, 0.46f),
            Color(0.56f, 0.36f, 0.96f),
            Color(0.96f, 0.68f, 0.18f),
            Color(0.78f, 0.22f, 0.78f),
            Color(0.32f, 0.64f, 0.24f),
            Color(0.88f, 0.18f, 0.24f),
        }};

        randomSeed_ = randomSeed_ * 1664525u + 1013904223u;
        int nextIndex = static_cast<int>(randomSeed_ % accentPalette.size());
        if (nextIndex == accentIndex_) {
            nextIndex = (nextIndex + 1) % static_cast<int>(accentPalette.size());
        }

        accentIndex_ = nextIndex;
        const Color accent = accentPalette[accentIndex_];
        LightTheme.primary = accent;
        DarkTheme.primary = accent;

        ++stateVersion_;
        ui_.requestThemeRefresh(0.18f);
    }

    void SetLayoutSplit(float value) {
        const float clamped = std::clamp(value, 0.28f, 0.72f);
        if (std::abs(layoutSplit_ - clamped) < 0.0001f) {
            return;
        }
        layoutSplit_ = clamped;
        ++stateVersion_;
    }

    UIContext ui_;
    MainPageView currentView_ = MainPageView::Home;
    float pageReveal_ = 1.0f;
    int pageRevealDirection_ = 1;
    float shellPadding_ = 22.0f;
    float sidebarWidth_ = 86.0f;
    float contentInset_ = 34.0f;
    float progressValue_ = 0.30f;
    bool homeIconAccentEnabled_ = false;
    int segmentedIndex_ = 0;
    std::string inputText_;
    std::string textAreaText_;
    int comboSelection_ = -1;
    float layoutSplit_ = 0.42f;
    std::uint32_t randomSeed_ = 0xC0FFEE11u;
    int accentIndex_ = 0;
    std::uint64_t stateVersion_ = 0;
    MainPageView pendingView_ = MainPageView::Home;
    bool hasPendingViewSwitch_ = false;
};

} // namespace EUINEO
