#include "eui_neo.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <functional>
#include <string>

namespace app {

namespace {

constexpr eui::Color kTransparent{0.0f, 0.0f, 0.0f, 0.0f};

int selectedPage = 0;
bool optionGlass = false;
bool optionMotion = true;
bool optionUnlockFps = false;
bool optionNight = true;
float optionAnimationSpeed = 1.0f;
eui::Color sampleColor = components::theme::defaultPrimary();
int accentChoice = 0;
float pageScroll[8] = {};

eui::Transition pageTransition() {
    if (!optionMotion) {
        return eui::Transition::none();
    }
    return eui::Transition::make(0.22f, eui::Ease::OutCubic);
}

eui::Transition textTransition() {
    eui::Transition transition = pageTransition();
    if (transition.enabled) {
        transition.animate(eui::AnimProperty::TextColor | eui::AnimProperty::Opacity);
    }
    return transition;
}

eui::Transition motionTransition() {
    if (!optionMotion) {
        return eui::Transition::none();
    }
    return eui::Transition::make(0.36f, eui::Ease::OutBack);
}

double galleryFrameRateLimit() {
    return optionUnlockFps ? 0.0 : 90.0;
}

float animationSpeedFromSlider(float value) {
    return 0.25f + std::clamp(value, 0.0f, 1.0f) * 2.25f;
}

float animationSpeedSliderValue() {
    return std::clamp((optionAnimationSpeed - 0.25f) / 2.25f, 0.0f, 1.0f);
}

std::string animationSpeedText() {
    char buffer[16] = {};
    std::snprintf(buffer, sizeof(buffer), "%.2fx", optionAnimationSpeed);
    return buffer;
}

void applyGlobalAnimationSpeed() {
    core::setGlobalTransitionDurationScale(1.0f / std::max(0.05f, optionAnimationSpeed));
}

components::theme::ThemeColorTokens themeColors() {
    auto tokens = optionNight ? components::theme::dark() : components::theme::light();
    tokens.primary = sampleColor;
    return tokens;
}

components::theme::PageVisualTokens pageVisuals() {
    return components::theme::pageVisuals(themeColors());
}

eui::Color withAlpha(eui::Color color, float alpha) {
    return components::theme::withAlpha(color, alpha);
}

eui::Color appBg() {
    return themeColors().background;
}

eui::Color surface() {
    return themeColors().surface;
}

eui::Color surfaceSoft() {
    return themeColors().surfaceHover;
}

eui::Color surfaceActive() {
    return themeColors().surfaceActive;
}

eui::Color textPrimary() {
    return pageVisuals().titleColor;
}

eui::Color textMuted() {
    return pageVisuals().subtitleColor;
}

eui::Color bodyText() {
    return pageVisuals().bodyColor;
}

eui::Color borderColor(float alpha = 1.0f) {
    return components::theme::withOpacity(themeColors().border, alpha);
}

eui::Color shadowColor(float darkAlpha = 0.28f, float lightAlpha = 0.12f) {
    return optionNight
        ? eui::Color{0.0f, 0.0f, 0.0f, darkAlpha}
        : eui::Color{0.10f, 0.14f, 0.22f, lightAlpha};
}

eui::Color buttonHover(const eui::Color& base) {
    return eui::mixColor(
        base,
        optionNight ? eui::Color{1.0f, 1.0f, 1.0f, base.a} : themeColors().primary,
        optionNight ? 0.16f : 0.10f);
}

eui::Color buttonPressed(const eui::Color& base) {
    return eui::mixColor(
        base,
        optionNight ? eui::Color{0.0f, 0.0f, 0.0f, base.a} : themeColors().surfaceActive,
        optionNight ? 0.34f : 0.22f);
}

eui::Color accent() {
    return themeColors().primary;
}

std::string colorHex(eui::Color color) {
    const int r = static_cast<int>(std::clamp(color.r, 0.0f, 1.0f) * 255.0f + 0.5f);
    const int g = static_cast<int>(std::clamp(color.g, 0.0f, 1.0f) * 255.0f + 0.5f);
    const int b = static_cast<int>(std::clamp(color.b, 0.0f, 1.0f) * 255.0f + 0.5f);
    char result[8] = {};
    std::snprintf(result, sizeof(result), "#%02X%02X%02X", r, g, b);
    return result;
}

void caption(eui::Ui& ui, const std::string& id, const std::string& text,
             float width, float y) {
    ui.text(id)
        .position(0.0f, y)
        .size(width, 24.0f)
        .text(text)
        .fontSize(16.0f)
        .lineHeight(22.0f)
        .color(textMuted())
        .horizontalAlign(eui::HorizontalAlign::Center)
        .build();
}

const char* pageTitle() {
    switch (selectedPage) {
    case 1: return "Style";
    case 2: return "Motion";
    case 3: return "Layout";
    case 4: return "Bing";
    case 5: return "Settings";
    case 6: return "About";
    case 7: return "More";
    default: return "Controls";
    }
}

const char* pageSubtitle() {
    switch (selectedPage) {
    case 1: return "Typography, icons and theme tokens";
    case 2: return "Transitions and transform states";
    case 3: return "Rows, columns, fill and wrapping";
    case 4: return "Network text and image carousel";
    case 5: return "Gallery appearance and behavior";
    case 6: return "Runtime and project information";
    case 7: return "More gallery destinations";
    default: return "Inputs, choices, feedback and charts";
    }
}

#include "examples/pages/gallery_controls.h"
#include "examples/pages/gallery_style.h"
#include "examples/pages/gallery_animation.h"
#include "examples/pages/gallery_layout.h"
#include "examples/pages/gallery_bing.h"
#include "examples/pages/gallery_about.h"

GalleryControlsPage controlsPage;
GalleryStylePage stylePage;
GalleryAnimationPage animationPage;
GalleryLayoutPage layoutPage;
GalleryBingPage bingPage;
GalleryAboutPage aboutPage;

void settingsRow(eui::Ui& ui, const std::string& id, const std::string& label,
                 bool value, float width, const std::function<void(bool)>& onChange) {
    ui.stack(id)
        .size(width, 42.0f)
        .content([&] {
            components::toggleSwitch(ui, id + ".switch")
                .theme(themeColors())
                .size(width, 36.0f)
                .checked(value)
                .text(label)
                .onChange(onChange)
                .transition(pageTransition())
                .build();
        })
        .build();
}

void composeSettings(eui::Ui& ui, float width) {
    ui.column("settings.mobile")
        .width(width)
        .height(eui::SizeValue::wrapContent())
        .gap(14.0f)
        .content([&] {
            settingsRow(ui, "settings.dark", "Dark theme", optionNight, width,
                        [](bool value) { optionNight = value; });
            settingsRow(ui, "settings.motion", "Animated transitions", optionMotion, width,
                        [](bool value) { optionMotion = value; });
            settingsRow(ui, "settings.glass", "Glass surface samples", optionGlass, width,
                        [](bool value) { optionGlass = value; });
            settingsRow(ui, "settings.fps", "Use display refresh rate", optionUnlockFps, width,
                        [](bool value) { optionUnlockFps = value; });

            ui.text("settings.speed.label")
                .size(width, 24.0f)
                .text("Animation speed  " + animationSpeedText())
                .fontSize(15.0f)
                .lineHeight(21.0f)
                .fontWeight(720)
                .color(textPrimary())
                .build();
            components::slider(ui, "settings.speed")
                .theme(themeColors())
                .size(width, 32.0f)
                .value(animationSpeedSliderValue())
                .onChange([](float value) {
                    optionAnimationSpeed = animationSpeedFromSlider(value);
                })
                .transition(pageTransition())
                .build();

            ui.text("settings.accent.label")
                .size(width, 24.0f)
                .text("Accent color")
                .fontSize(15.0f)
                .lineHeight(21.0f)
                .fontWeight(720)
                .color(textPrimary())
                .build();
            components::segmented(ui, "settings.accent")
                .theme(themeColors())
                .size(width, 38.0f)
                .items({"Blue", "Mint", "Orange"})
                .selected(accentChoice)
                .onChange([](int value) {
                    accentChoice = value;
                    sampleColor = value == 1
                        ? eui::Color{0.12f, 0.72f, 0.62f, 1.0f}
                        : value == 2
                            ? eui::Color{0.94f, 0.48f, 0.22f, 1.0f}
                            : components::theme::defaultPrimary();
                })
                .transition(pageTransition())
                .build();
        })
        .build();
}

void moreButton(eui::Ui& ui, const std::string& id, const std::string& label,
                int icon, int page, float width) {
    components::button(ui, id)
        .theme(themeColors(), false)
        .size(width, 48.0f)
        .icon(icon)
        .text(label)
        .radius(8.0f)
        .onClick([page] { selectedPage = page; })
        .transition(pageTransition())
        .build();
}

void composeMore(eui::Ui& ui, float width) {
    ui.column("more.menu")
        .width(width)
        .height(eui::SizeValue::wrapContent())
        .gap(12.0f)
        .content([&] {
            moreButton(ui, "more.bing", "Bing media", 0xF1C5, 4, width);
            moreButton(ui, "more.settings", "Settings", 0xF013, 5, width);
            moreButton(ui, "more.about", "About EUI-NEO", 0xF05A, 6, width);

            ui.stack("more.runtime")
                .size(width, 108.0f)
                .content([&] {
                    ui.rect("more.runtime.background")
                        .size(width, 108.0f)
                        .color(surface())
                        .radius(8.0f)
                        .border(1.0f, borderColor(0.72f))
                        .build();
                    ui.text("more.runtime.title")
                        .position(16.0f, 15.0f)
                        .size(width - 32.0f, 26.0f)
                        .text("Android runtime")
                        .fontSize(18.0f)
                        .lineHeight(24.0f)
                        .fontWeight(760)
                        .color(textPrimary())
                        .build();
                    ui.text("more.runtime.detail")
                        .position(16.0f, 49.0f)
                        .size(width - 32.0f, 42.0f)
                        .text("SDL2 window  |  Vulkan renderer\nGlobal UI scale 1.6x")
                        .fontSize(13.0f)
                        .lineHeight(20.0f)
                        .color(textMuted())
                        .build();
                })
                .build();
        })
        .build();
}

void composePage(eui::Ui& ui, float width, float height) {
    switch (selectedPage) {
    case 1:
        stylePage.compose(ui, width, height);
        break;
    case 2:
        animationPage.compose(ui, width, height);
        break;
    case 3:
        layoutPage.compose(ui, width, height);
        break;
    case 4:
        bingPage.compose(ui, width, height);
        break;
    case 5:
        composeSettings(ui, width);
        break;
    case 6:
        aboutPage.compose(ui, width, height);
        break;
    case 7:
        composeMore(ui, width);
        break;
    default:
        controlsPage.compose(ui, width, height);
        break;
    }
}

struct DockItem {
    int page;
    const char* id;
    const char* label;
    unsigned int icon;
};

constexpr std::array<DockItem, 5> kDockItems{{
    {0, "controls", "Controls", 0xF1B2},
    {1, "style", "Style", 0xF1FC},
    {2, "motion", "Motion", 0xF2F1},
    {3, "layout", "Layout", 0xF0DB},
    {7, "more", "More", 0xF141},
}};

int selectedDockPage() {
    return selectedPage >= 4 ? 7 : selectedPage;
}

void composeDock(eui::Ui& ui, float y, float width) {
    constexpr float height = 56.0f;
    const float itemWidth = width / static_cast<float>(kDockItems.size());
    const int activePage = selectedDockPage();

    ui.stack("dock")
        .position(0.0f, y)
        .size(width, height)
        .zIndex(80)
        .content([&] {
            ui.rect("dock.background")
                .size(width, height)
                .color(surface())
                .transition(pageTransition())
                .animate(eui::AnimProperty::Color)
                .build();

            for (std::size_t index = 0; index < kDockItems.size(); ++index) {
                const DockItem& item = kDockItems[index];
                const bool active = activePage == item.page;
                const float x = static_cast<float>(index) * itemWidth;

                ui.rect(std::string("dock.hit.") + item.id)
                    .position(x, 0.0f)
                    .size(itemWidth, height)
                    .states({0.0f, 0.0f, 0.0f, 0.0f},
                            {0.0f, 0.0f, 0.0f, 0.0f},
                            {0.0f, 0.0f, 0.0f, 0.0f})
                    .onPress([page = item.page](const core::PointerEvent&, const core::Rect&) {
                        selectedPage = page;
                    })
                    .build();

                ui.text(std::string("dock.icon.") + item.id)
                    .position(x, 10.0f)
                    .size(itemWidth, 16.0f)
                    .icon(item.icon)
                    .fontSize(15.0f)
                    .lineHeight(16.0f)
                    .color(active ? accent() : textMuted())
                    .horizontalAlign(eui::HorizontalAlign::Center)
                    .transition(textTransition())
                    .build();

                ui.text(std::string("dock.label.") + item.id)
                    .position(x, 34.0f)
                    .size(itemWidth, 16.0f)
                    .text(item.label)
                    .fontSize(9.0f)
                    .lineHeight(13.0f)
                    .fontWeight(active ? 760 : 560)
                    .color(active ? accent() : textMuted())
                    .horizontalAlign(eui::HorizontalAlign::Center)
                    .transition(textTransition())
                    .build();
            }
        })
        .build();
}

} // namespace

const DslAppConfig& dslAppConfig() {
    static DslAppConfig config = DslAppConfig{}
        .title("EUI-NEO Android Gallery")
        .pageId("android_gallery")
        .clearColor({0.10f, 0.10f, 0.12f, 1.0f})
        .windowSize(1080, 1920)
        .uiScale(1.6f)
        .fps(galleryFrameRateLimit())
        .iconFont("assets/Font Awesome 7 Free-Solid-900.otf")
        .showDebugStatsInTitle(false);
    config.fps(galleryFrameRateLimit());
    return config;
}

void compose(eui::Ui& ui, const eui::Screen& screen) {
    applyGlobalAnimationSpeed();
    constexpr float side = 14.0f;
    constexpr float headerHeight = 62.0f;
    constexpr float dockHeight = 56.0f;
    const float contentWidth = std::max(0.0f, screen.width - side * 2.0f);
    const float scrollTop = side + headerHeight;
    const float dockY = std::max(scrollTop, screen.height - dockHeight);
    const float scrollHeight = std::max(0.0f, dockY - scrollTop);
    const int page = std::clamp(selectedPage, 0, 7);

    ui.stack("root")
        .size(screen.width, screen.height)
        .content([&] {
            ui.rect("root.background")
                .size(screen.width, screen.height)
                .color(appBg())
                .transition(pageTransition())
                .animate(eui::AnimProperty::Color)
                .build();

            ui.text("page.title")
                .position(side, side)
                .size(contentWidth, 28.0f)
                .text(pageTitle())
                .fontSize(24.0f)
                .lineHeight(28.0f)
                .fontWeight(820)
                .color(textPrimary())
                .transition(textTransition())
                .build();
            ui.text("page.subtitle")
                .position(side, side + 31.0f)
                .size(contentWidth, 20.0f)
                .text(pageSubtitle())
                .fontSize(11.0f)
                .lineHeight(16.0f)
                .color(textMuted())
                .transition(textTransition())
                .build();

            components::scrollView(ui, "page.scroll." + std::to_string(page))
                .theme(themeColors())
                .position(side, scrollTop)
                .size(contentWidth, scrollHeight)
                .offset(pageScroll[page])
                .gap(12.0f)
                .step(42.0f)
                .scrollbarWidth(6.0f)
                .scrollbarGap(4.0f)
                .contentKey(page == 1 ? (optionNight ? "style.dark" : "style.light") : "")
                .onChange([page](float value) { pageScroll[page] = value; })
                .content([&](eui::Ui& contentUi, float width, float viewportHeight) {
                    composePage(contentUi, width, viewportHeight);
                })
                .build();

            composeDock(ui, dockY, screen.width);
        })
        .build();

    controlsPage.composeOverlays(ui, screen);
}

} // namespace app
