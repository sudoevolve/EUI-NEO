#if __has_include("eui_neo.h")
#include "eui_neo.h"
#else
#include "../include/eui_neo.h"
#endif

#include <algorithm>
#include <cstdio>
#include <functional>
#include <vector>
#include <string>

namespace app {

namespace {

constexpr eui::Color kTransparent{0.0f, 0.0f, 0.0f, 0.0f};

int selectedPage = 0;
bool optionDense = false;
bool optionGlass = false;
bool optionMotion = true;
bool optionUnlockFps = false;
bool optionNight = true;
float optionAnimationSpeed = 1.0f;
eui::Color sampleColor = components::theme::defaultPrimary();
bool workshopOpen = false;
bool workshopHeartLiked = false;
float pageScroll[7] = {};

constexpr float kSidebarWidth = 272.0f;
constexpr float kCompactSidebarWidth = 96.0f;

eui::Transition pageTransition() {
    if (!optionMotion) {
        return eui::Transition::none();
    }
    return eui::Transition::make(0.28f, eui::Ease::OutCubic);
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
    return eui::Transition::make(0.42f, eui::Ease::OutBack);
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
    components::theme::ThemeColorTokens tokens = optionNight ? components::theme::dark() : components::theme::light();
    tokens.primary = sampleColor;
    return tokens;
}

components::theme::PageVisualTokens pageVisuals() {
    return components::theme::pageVisuals(themeColors());
}

eui::Color withAlpha(eui::Color color, float alpha) {
    return components::theme::withAlpha(color, alpha);
}

eui::Color mixTheme(eui::Color from, eui::Color to, float amount) {
    return eui::mixColor(from, to, amount);
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
    return mixTheme(base, optionNight ? eui::Color{1.0f, 1.0f, 1.0f, base.a} : themeColors().primary, optionNight ? 0.16f : 0.10f);
}

eui::Color buttonPressed(const eui::Color& base) {
    return mixTheme(base, optionNight ? eui::Color{0.0f, 0.0f, 0.0f, base.a} : themeColors().surfaceActive, optionNight ? 0.34f : 0.22f);
}

eui::Color accent() {
    return themeColors().primary;
}

const char* pageTitle() {
    if (selectedPage == 1) {
        return "Style";
    }
    if (selectedPage == 2) {
        return "Animation";
    }
    if (selectedPage == 3) {
        return "Settings";
    }
    if (selectedPage == 4) {
        return "Bing";
    }
    if (selectedPage == 5) {
        return "About";
    }
    if (selectedPage == 6) {
        return "Layout";
    }
    return "Controls";
}

const char* pageSubtitle() {
    if (selectedPage == 1) {
        return "Text scales, icon text and theme color tokens for developers.";
    }
    if (selectedPage == 2) {
        return "Click and hover samples driven by DSL transitions.";
    }
    if (selectedPage == 3) {
        return "Interactive settings built with the same rect and text primitives.";
    }
    if (selectedPage == 4) {
        return "Bing daily images and API text requests in one composed page.";
    }
    if (selectedPage == 5) {
        return "A lightweight and elegant C++ GUI framework.";
    }
    if (selectedPage == 6) {
        return "Rows, columns, stacks, padding, fill, wrapping, scrolling and loader modes.";
    }
    return "Basic controls, states and visual properties in one surface.";
}

void caption(eui::Ui& ui, const std::string& id, const std::string& text, float width, float y) {
    ui.text(id)
        .y(y)
        .size(width, 24.0f)
        .text(text)
        .fontSize(16.0f)
        .lineHeight(22.0f)
        .color(textMuted())
        .horizontalAlign(eui::HorizontalAlign::Center)
        .build();
}

void composeNavbar(eui::Ui& ui, float width, float height, bool compact) {
    components::navbar(ui, "gallery.navbar")
        .theme(themeColors())
        .size(width, height)
        .compact(compact)
        .brand("EUI Gallery", 0xF5FD)
        .selected(selectedPage)
        .items({
            {"controls", "Controls", 0xF1B2, 0},
            {"style", "Style", 0xF1FC, 1},
            {"animation", "Animation", 0xF2F1, 2},
            {"layout", "Layout", 0xF0DB, 6},
            {"bing", "Bing", 0xF1C5, 4},
            {"settings", "Settings", 0xF013, 3},
            {"about", "About", 0xF05A, 5},
        })
        .footer(optionNight ? "Light Mode" : "Night Mode", optionNight ? 0xF185 : 0xF186, [] {
            optionNight = !optionNight;
        })
        .transition(pageTransition())
        .onChange([](int page) {
            selectedPage = page;
        })
        .build();
}

std::string colorHex(eui::Color color) {
    const int r = static_cast<int>(std::clamp(color.r, 0.0f, 1.0f) * 255.0f + 0.5f);
    const int g = static_cast<int>(std::clamp(color.g, 0.0f, 1.0f) * 255.0f + 0.5f);
    const int b = static_cast<int>(std::clamp(color.b, 0.0f, 1.0f) * 255.0f + 0.5f);
    char result[8] = {};
    std::snprintf(result, sizeof(result), "#%02X%02X%02X", r, g, b);
    return result;
}

#include "pages/gallery_controls.h"
#include "pages/gallery_style.h"
#include "pages/gallery_animation.h"
#include "pages/gallery_layout.h"
#include "pages/gallery_settings.h"
#include "pages/gallery_bing.h"
#include "pages/gallery_about.h"

GalleryControlsPage controlsPage;
GalleryStylePage stylePage;
GalleryAnimationPage animationPage;
GalleryLayoutPage layoutPage;
GallerySettingsPage settingsPage;
GalleryBingPage bingPage;
GalleryAboutPage aboutPage;

void composePageBody(eui::Ui& ui, float width, float height) {
    ui.loader("pages.controls")
        .active(selectedPage == 0)
        .keepAlive()
        .content([&] {
            controlsPage.compose(ui, width, height);
        })
        .build();

    ui.loader("pages.style")
        .active(selectedPage == 1)
        .keepAlive()
        .content([&] {
            stylePage.compose(ui, width, height);
        })
        .build();

    ui.loader("pages.animation")
        .active(selectedPage == 2)
        .keepAlive()
        .content([&] {
            animationPage.compose(ui, width, height);
        })
        .build();

    ui.loader("pages.settings")
        .active(selectedPage == 3)
        .keepAlive()
        .content([&] {
            settingsPage.compose(ui, width, height);
        })
        .build();

    ui.loader("pages.bing")
        .active(selectedPage == 4)
        .keepAlive()
        .content([&] {
            bingPage.compose(ui, width, height);
        })
        .build();

    ui.loader("pages.about")
        .active(selectedPage == 5)
        .keepAlive()
        .content([&] {
            aboutPage.compose(ui, width, height);
        })
        .build();

    ui.loader("pages.layout")
        .active(selectedPage == 6)
        .keepAlive()
        .content([&] {
            layoutPage.compose(ui, width, height);
        })
        .build();
}

void composeContent(eui::Ui& ui, float width, float height) {
    const bool compact = width < 760.0f;
    const float shellMargin = compact ? 18.0f : 36.0f;
    const float contentInset = compact ? 24.0f : 32.0f;
    const float shellWidth = std::max(0.0f, width - shellMargin * 2.0f);
    const float innerWidth = std::max(0.0f, shellWidth - contentInset * 2.0f);
    const float shellHeight = std::max(0.0f, height - shellMargin * 2.0f);
    const float innerHeight = std::max(0.0f, shellHeight - contentInset * 2.0f);
    const float headerGap = optionDense ? 18.0f : 26.0f;
    const float bodyHeight = std::max(0.0f, innerHeight - 46.0f - 30.0f - headerGap * 2.0f);
    const int page = std::clamp(selectedPage, 0, 6);

    ui.stack("content.area")
        .size(width, height)
        .content([&] {
            ui.rect("content.bg")
                .size(width, height)
                .color(appBg())
                .build();

            ui.rect("page.shell")
                .size(shellWidth, shellHeight)
                .margin(shellMargin)
                .color(surface())
                .radius(compact ? 18.0f : 26.0f)
                .border(1.0f, borderColor())
                .shadow(30.0f, 0.0f, 16.0f, shadowColor(0.28f, 0.14f))
                .transition(pageTransition())
                .build();

            ui.column("page.content")
                .size(innerWidth, innerHeight)
                .margin(shellMargin + contentInset)
                .padding(0.0f)
                .gap(headerGap)
                .content([&] {
                    ui.row("page.title.row")
                        .size(innerWidth, 46.0f)
                        .alignItems(eui::Align::CENTER)
                        .content([&] {
                            const bool showWorkshopLink = selectedPage == 0;
                            const float moreWidth = showWorkshopLink ? 64.0f : 0.0f;
                            ui.text("page.title")
                                .size(std::max(0.0f, innerWidth - moreWidth), 46.0f)
                                .text(pageTitle())
                                .fontSize(compact ? 30.0f : 38.0f)
                                .lineHeight(44.0f)
                                .color(accent())
                                .transition(textTransition())
                                .build();

                            if (showWorkshopLink) {
                                ui.text("page.title.more")
                                    .size(moreWidth, 28.0f)
                                    .text("more")
                                    .fontSize(16.0f)
                                    .lineHeight(20.0f)
                                    .fontWeight(760)
                                    .color(accent())
                                    .horizontalAlign(eui::HorizontalAlign::Left)
                                    .verticalAlign(eui::VerticalAlign::Center)
                                    .transition(textTransition())
                                    .onClick([] {
                                        workshopOpen = true;
                                    })
                                    .build();
                            }
                        })
                        .build();

                    ui.text("page.subtitle")
                        .size(innerWidth, 30.0f)
                        .text(pageSubtitle())
                        .fontSize(compact ? 17.0f : 20.0f)
                        .lineHeight(28.0f)
                        .color(textMuted())
                        .transition(textTransition())
                        .build();

                    ui.stack("page.body")
                        .size(innerWidth, bodyHeight)
                        .content([&] {
                            const std::string scrollId = "page.body.scrollview." + std::to_string(page);
                            const std::string scrollContentKey = page == 1
                                ? std::string("style.") + (optionNight ? "dark" : "light") + "." + (optionDense ? "dense" : "regular")
                                : "";
                            components::scrollView(ui, scrollId)
                                .theme(themeColors())
                                .size(innerWidth, bodyHeight)
                                .offset(pageScroll[page])
                                .gap(headerGap)
                                .step(48.0f)
                                .contentKey(scrollContentKey)
                                .onChange([page](float value) {
                                    pageScroll[page] = value;
                                })
                                .content([&](eui::Ui& contentUi, float contentWidth, float viewportHeight) {
                                    composePageBody(contentUi, contentWidth, viewportHeight);
                                })
                                .build();
                        })
                        .build();
                });

            components::sidebar(ui, "workshop.sidebar")
                .theme(themeColors())
                .size(width, height)
                .drawerWidth(430.0f)
                .open(workshopOpen)
                .zIndex(80)
                .transition(pageTransition())
                .onOpenChange([](bool open) {
                    workshopOpen = open;
                })
                .content([&](eui::Ui& panelUi, float panelWidth, float) {
                    panelUi.column("workshop.components")
                        .size(panelWidth, 382.0f)
                        .gap(22.0f)
                        .alignItems(eui::Align::CENTER)
                        .content([&] {
                            components::workshop::neumorphicButton(panelUi, "workshop.neumorphic.button").theme(themeColors()).size(std::min(310.0f, panelWidth), 92.0f).fontSize(32.0f).text("Click me").transition(pageTransition()).build();

                            components::workshop::heartSwitch(panelUi, "workshop.heart.switch").theme(themeColors()).size(64.0f, 64.0f).checked(workshopHeartLiked).transition(pageTransition()).onChange([](bool value) {
                                workshopHeartLiked = value;
                            }).build();

                            components::workshop::tiltCard(panelUi, "workshop.tilt.card").theme(themeColors()).size(std::min(318.0f, panelWidth), 178.0f).transition(pageTransition()).build();
                        })
                        .build();
                })
                .build();
        });
}

} // namespace

const DslAppConfig& dslAppConfig() {
    static DslAppConfig config = DslAppConfig{}
        .title("EUI Gallery")
        .pageId("gallery")
        .clearColor({0.07f, 0.08f, 0.10f, 1.0f})
        .windowSize(1600, 1100)
        .fps(galleryFrameRateLimit());
    config.fps(galleryFrameRateLimit());
    return config;
}

void compose(eui::Ui& ui, const eui::Screen& screen) {
    applyGlobalAnimationSpeed();
    const bool compactShell = screen.width < 980.0f;
    const float sidebarWidth = compactShell ? kCompactSidebarWidth : kSidebarWidth;
    const float contentWidth = std::max(0.0f, screen.width - sidebarWidth);

    ui.row("root")
        .size(screen.width, screen.height)
        .content([&] {
            composeNavbar(ui, sidebarWidth, screen.height, compactShell);
            composeContent(ui, contentWidth, screen.height);
        });

    controlsPage.composeOverlays(ui, screen);
}

} // namespace app
