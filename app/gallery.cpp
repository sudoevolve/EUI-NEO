#include "app/dsl_app.h"

#include "components/components.h"
#include "core/network.h"
#include "core/platform.h"

#include <algorithm>
#include <cstdio>
#include <functional>
#include <string>

namespace app {

namespace {

constexpr core::Color kTransparent{0.0f, 0.0f, 0.0f, 0.0f};

int selectedPage = 0;
bool optionDense = false;
bool optionGlass = false;
bool optionMotion = true;
bool optionUnlockFps = false;
bool optionNight = true;
bool animationMoved = false;
bool animationRotated = false;
bool animationFaded = false;
bool animationScaled = false;
bool animationRounded = false;
bool animationGlowing = false;
bool sampleChecked = true;
bool sampleSwitch = true;
bool sampleRadioA = true;
std::string sampleInput = "EUI";
float sampleSlider = 0.44f;
int sampleSegment = 1;
int sampleTab = 0;
int sampleDropdown = 1;
bool sampleDropdownOpen = false;
int sampleYear = 2026;
int sampleMonth = 4;
int sampleDay = 28;
int sampleHour = 9;
int sampleMinute = 30;
core::Color sampleColor = components::theme::defaultPrimary();
bool sampleDateOpen = false;
bool sampleTimeOpen = false;
bool sampleColorOpen = false;
bool sampleDialogOpen = false;
bool sampleToastVisible = false;
bool sampleContextMenuOpen = false;
float sampleContextMenuX = 0.0f;
float sampleContextMenuY = 0.0f;
std::string sampleFeedback = "Ready";
float pageScroll[6] = {};

constexpr float kSidebarWidth = 272.0f;
constexpr float kNavTop = 128.0f;
constexpr float kNavHeight = 50.0f;
constexpr float kNavGap = 14.0f;

core::Transition pageTransition() {
    if (!optionMotion) {
        return core::Transition::none();
    }
    return core::Transition::make(0.28f, core::Ease::OutCubic);
}

core::Transition textTransition() {
    core::Transition transition = pageTransition();
    if (transition.enabled) {
        transition.animate(core::AnimProperty::TextColor | core::AnimProperty::Opacity);
    }
    return transition;
}

core::Transition motionTransition() {
    if (!optionMotion) {
        return core::Transition::none();
    }
    return core::Transition::make(0.42f, core::Ease::OutBack);
}

double galleryFrameRateLimit() {
    return optionUnlockFps ? 0.0 : 90.0;
}

components::theme::ThemeColorTokens themeColors() {
    components::theme::ThemeColorTokens tokens = optionNight ? components::theme::DarkThemeColors() : components::theme::LightThemeColors();
    tokens.primary = sampleColor;
    return tokens;
}

components::theme::PageVisualTokens pageVisuals() {
    return components::theme::pageVisuals(themeColors());
}

core::Color withAlpha(core::Color color, float alpha) {
    return components::theme::withAlpha(color, alpha);
}

core::Color mixTheme(core::Color from, core::Color to, float amount) {
    return core::mixColor(from, to, amount);
}

core::Color appBg() {
    return themeColors().background;
}

core::Color surface() {
    return themeColors().surface;
}

core::Color surfaceSoft() {
    return themeColors().surfaceHover;
}

core::Color surfaceActive() {
    return themeColors().surfaceActive;
}

core::Color textPrimary() {
    return pageVisuals().titleColor;
}

core::Color textMuted() {
    return pageVisuals().subtitleColor;
}

core::Color bodyText() {
    return pageVisuals().bodyColor;
}

core::Color borderColor(float alpha = 1.0f) {
    return components::theme::withOpacity(themeColors().border, alpha);
}

core::Color shadowColor(float darkAlpha = 0.28f, float lightAlpha = 0.12f) {
    return optionNight
        ? core::Color{0.0f, 0.0f, 0.0f, darkAlpha}
        : core::Color{0.10f, 0.14f, 0.22f, lightAlpha};
}

core::Color buttonHover(const core::Color& base) {
    return mixTheme(base, optionNight ? core::Color{1.0f, 1.0f, 1.0f, base.a} : themeColors().primary, optionNight ? 0.16f : 0.10f);
}

core::Color buttonPressed(const core::Color& base) {
    return mixTheme(base, optionNight ? core::Color{0.0f, 0.0f, 0.0f, base.a} : themeColors().surfaceActive, optionNight ? 0.34f : 0.22f);
}

core::Color accent() {
    return themeColors().primary;
}

int navOrderForPage(int page) {
    if (page == 4) {
        return 3;
    }
    if (page == 3) {
        return 4;
    }
    return std::clamp(page, 0, 5);
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
    return "Basic controls, states and visual properties in one surface.";
}

void caption(core::dsl::Ui& ui, const std::string& id, const std::string& text, float width, float y) {
    ui.text(id)
        .y(y)
        .size(width, 24.0f)
        .text(text)
        .fontSize(16.0f)
        .lineHeight(22.0f)
        .color(textMuted())
        .horizontalAlign(core::HorizontalAlign::Center)
        .build();
}

void navItem(core::dsl::Ui& ui, const std::string& id, const std::string& label, unsigned int icon, int page) {
    const bool active = selectedPage == page;
    const core::Color activeAccent = accent();
    const core::Color normal = active ? activeAccent : surface();
    const core::Color hover = active ? buttonHover(activeAccent) : surfaceSoft();
    const core::Color pressed = active ? buttonPressed(activeAccent) : surfaceActive();
    components::button(ui, id)
        .size(212.0f, 50.0f)
        .icon(icon)
        .iconSize(16.0f)
        .fontSize(17.0f)
        .text(label)
        .colors(normal, hover, pressed)
        .textColor(active || optionNight ? core::Color{0.94f, 0.97f, 1.0f, 1.0f} : textPrimary())
        .iconColor(active || optionNight ? core::Color{0.94f, 0.97f, 1.0f, 1.0f} : textPrimary())
        .radius(12.0f)
        .border(1.0f, active ? withAlpha(activeAccent, 0.58f) : borderColor(0.60f))
        .shadow(12.0f, 0.0f, 4.0f, shadowColor(0.18f, 0.08f))
        .transition(pageTransition())
        .onClick([page] {
            selectedPage = page;
        })
        .build();
}

void composeSidebar(core::dsl::Ui& ui, float height) {
    const core::Color sidebarBg = optionNight ? mixTheme(appBg(), core::Color{0.0f, 0.0f, 0.0f, 1.0f}, 0.24f) : surface();
    ui.stack("sidebar")
        .size(kSidebarWidth, height)
        .content([&] {
            ui.rect("sidebar.bg")
                .size(kSidebarWidth, height)
                .color(sidebarBg)
                .build();

            ui.rect("sidebar.accent")
                .x(0.0f)
                .y(kNavTop)
                .size(4.0f, 50.0f)
                .color(accent())
                .radius(2.0f)
                .translateY(static_cast<float>(navOrderForPage(selectedPage)) * (kNavHeight + kNavGap))
                .transition(pageTransition())
                .animate(core::AnimProperty::Transform | core::AnimProperty::Color)
                .build();

            ui.column("sidebar.content")
                .size(kSidebarWidth, std::max(0.0f, height - 42.0f))
                .margin(0.0f, 30.0f, 0.0f, 0.0f)
                .gap(14.0f)
                .alignItems(core::Align::CENTER)
                .content([&] {
                    ui.text("brand.icon")
                        .size(212.0f, 34.0f)
                        .icon(0xF5FD)
                        .fontSize(27.0f)
                        .lineHeight(32.0f)
                        .color(accent())
                        .transition(textTransition())
                        .horizontalAlign(core::HorizontalAlign::Center)
                        .build();

                    ui.text("brand.title")
                        .size(212.0f, 36.0f)
                        .text("EUI Gallery")
                        .fontSize(30.0f)
                        .lineHeight(34.0f)
                        .color(textPrimary())
                        .horizontalAlign(core::HorizontalAlign::Center)
                        .build();

                    navItem(ui, "nav.controls", "Controls", 0xF1B2, 0);
                    navItem(ui, "nav.text", "Style", 0xF1FC, 1);
                    navItem(ui, "nav.animation", "Animation", 0xF2F1, 2);
                    navItem(ui, "nav.bing", "Bing", 0xF1C5, 4);
                    navItem(ui, "nav.settings", "Settings", 0xF013, 3);
                    navItem(ui, "nav.about", "About", 0xF05A, 5);
                });

            ui.stack("sidebar.theme")
                .x(30.0f)
                .y(std::max(0.0f, height - 82.0f))
                .size(212.0f, 50.0f)
                .content([&] {
                    components::button(ui, "nav.theme")
                        .size(212.0f, 50.0f)
                        .icon(optionNight ? 0xF185 : 0xF186)
                        .iconSize(16.0f)
                        .fontSize(17.0f)
                        .text(optionNight ? "Light Mode" : "Night Mode")
                        .colors(surface(), surfaceSoft(), surfaceActive())
                        .textColor(textPrimary())
                        .iconColor(accent())
                        .radius(12.0f)
                        .border(1.0f, borderColor(0.80f))
                        .shadow(12.0f, 0.0f, 4.0f, shadowColor(0.18f, 0.08f))
                        .transition(pageTransition())
                        .onClick([] {
                            optionNight = !optionNight;
                        })
                        .build();
                })
                .build();
        });
}

void propertyCard(core::dsl::Ui& ui, const std::string& id, const std::string& title, const std::string& note,
                  const core::Color& color, const std::string& kind, float width) {
    ui.stack(id)
        .size(width, 144.0f)
        .visualStateFrom(id + ".bg", 0.95f)
        .content([&] {
            if (kind == "blur") {
                ui.rect(id + ".circle.primary")
                    .x(width * 0.14f)
                    .y(20.0f)
                    .size(64.0f, 64.0f)
                    .color(withAlpha(accent(), 0.95f))
                    .radius(32.0f)
                    .build();

                ui.rect(id + ".circle.warm")
                    .x(width * 0.48f)
                    .y(62.0f)
                    .size(58.0f, 58.0f)
                    .color({1.0f, 0.54f, 0.18f, 0.92f})
                    .radius(29.0f)
                    .build();

                ui.rect(id + ".circle.cool")
                    .x(width * 0.66f)
                    .y(16.0f)
                    .size(46.0f, 46.0f)
                    .color({0.16f, 0.82f, 0.72f, 0.90f})
                    .radius(23.0f)
                    .build();
            }

            auto rect = ui.rect(id + ".bg")
                .size(width, 144.0f)
                .states(color, buttonHover(color), buttonPressed(color))
                .radius(18.0f)
                .transition(pageTransition());

            if (kind == "border") {
                rect.border(3.0f, accent());
            } else if (kind == "shadow") {
                rect.shadow(28.0f, 0.0f, 12.0f, shadowColor(0.34f, 0.18f));
            } else if (kind == "blur") {
                rect.opacity(optionGlass ? 1.0f : 0.82f)
                    .blur(optionGlass ? 18.0f : 0.0f)
                    .border(1.0f, optionGlass ? withAlpha(textPrimary(), 0.35f) : borderColor(0.70f));
            } else if (kind == "rotate") {
                rect.rotate(0.08f).transformOrigin(0.5f, 0.5f);
            }
            rect.build();

            ui.text(id + ".title")
                .size(width, 32.0f)
                .margin(0.0f, 36.0f, 0.0f, 0.0f)
                .text(title)
                .fontSize(22.0f)
                .lineHeight(28.0f)
                .color(textPrimary())
                .horizontalAlign(core::HorizontalAlign::Center)
                .build();

            caption(ui, id + ".note", note, width, 90.0f);
        })
        .build();
}

void bingImageCard(core::dsl::Ui& ui, const std::string& id, const std::string& title, int index, const std::string& market,
                   float width, float height = 122.0f, float imageHeight = 72.0f) {
    const float labelY = std::max(14.0f, height - 30.0f);
    ui.stack(id)
        .size(width, height)
        .content([&] {
            ui.rect(id + ".bg")
                .size(width, height)
                .color(surface())
                .radius(18.0f)
                .border(1.0f, borderColor())
                .build();

            ui.image(id + ".image")
                .x(14.0f)
                .y(14.0f)
                .size(std::max(0.0f, width - 28.0f), imageHeight)
                .bingDaily(index, market)
                .radius(14.0f)
                .transition(pageTransition())
                .build();

            ui.text(id + ".label")
                .x(14.0f)
                .y(labelY)
                .size(std::max(0.0f, width - 28.0f), 22.0f)
                .text(title)
                .fontSize(16.0f)
                .lineHeight(20.0f)
                .color(textMuted())
                .horizontalAlign(core::HorizontalAlign::Center)
                .build();
        })
        .build();
}

std::string jsonStringValue(const std::string& json, const std::string& key) {
    const std::string token = "\"" + key + "\":\"";
    const size_t begin = json.find(token);
    if (begin == std::string::npos) {
        return {};
    }
    const size_t valueBegin = begin + token.size();
    std::string value;
    bool escaping = false;
    for (size_t i = valueBegin; i < json.size(); ++i) {
        const char ch = json[i];
        if (escaping) {
            value.push_back(ch == '/' ? '/' : ch);
            escaping = false;
            continue;
        }
        if (ch == '\\') {
            escaping = true;
            continue;
        }
        if (ch == '"') {
            break;
        }
        value.push_back(ch);
    }
    return value;
}

std::string bingApiText() {
    const std::string key = "gallery.bing.text";
    core::network::requestText(key, "https://www.bing.com/HPImageArchive.aspx?format=js&n=1&idx=0&mkt=zh-CN");
    const core::network::TextResult result = core::network::textResult(key);
    if (!result.ready) {
        return "Loading Bing API text...";
    }
    if (!result.ok) {
        return "Network text request failed.";
    }
    const std::string copyright = jsonStringValue(result.body, "copyright");
    return copyright.empty() ? "Bing API returned text data." : copyright;
}

std::string colorHex(core::Color color);

std::string sampleDateText() {
    char buffer[16] = {};
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", sampleYear, sampleMonth, sampleDay);
    return std::string(buffer);
}

std::string sampleTimeText() {
    char buffer[8] = {};
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d", sampleHour, sampleMinute);
    return std::string(buffer);
}

void composeControlsPage(core::dsl::Ui& ui, float width, float height) {
    const float cardGap = 18.0f;
    const float cardWidth = std::max(72.0f, std::min(204.0f, (width - cardGap * 2.0f) / 3.0f));
    const float rowWidth = cardWidth * 3.0f + cardGap * 2.0f;
    const float rowHeight = 144.0f;
    const float buttonWidth = std::max(72.0f, std::min(178.0f, (width - 36.0f) / 3.0f));
    const float fieldWidth = std::max(0.0f, std::min(width, 680.0f));
    const float componentCardWidth = std::max(120.0f, std::min(340.0f, (width - 20.0f) * 0.5f));
    const float componentRowWidth = componentCardWidth * 2.0f + 20.0f;
    const float feedbackWidth = std::max(112.0f, std::min(176.0f, (fieldWidth - 54.0f) / 4.0f));
    const float dataRowGap = 20.0f;
    const float dropdownWidth = std::max(180.0f, std::min(260.0f, fieldWidth * 0.36f));
    const float tableWidth = std::max(260.0f, fieldWidth - dropdownWidth - dataRowGap);
    const float dataRowHeight = 200.0f;
    const float pickerGap = 18.0f;
    const float pickerWidth = std::max(154.0f, std::min(210.0f, (fieldWidth - pickerGap * 2.0f) / 3.0f));
    const float pickerRowWidth = pickerWidth * 3.0f + pickerGap * 2.0f;
    const float chartGap = 18.0f;
    const float chartWidth = std::max(150.0f, std::min(206.0f, (fieldWidth - chartGap * 2.0f) / 3.0f));
    const float chartHeight = 236.0f;
    const float chartRowWidth = chartWidth * 3.0f + chartGap * 2.0f;

    ui.text("controls.components.title")
        .size(width, 30.0f)
        .text("Basic Components")
        .fontSize(26.0f)
        .lineHeight(30.0f)
        .color(textPrimary())
        .build();

    ui.row("controls.buttons")
        .size(buttonWidth * 3.0f + 36.0f, 68.0f)
        .gap(18.0f)
        .content([&] {
            components::button(ui, "control.primary")
                .size(buttonWidth, 54.0f)
                .icon(0xF00C)
                .text("Filled")
                .colors(themeColors().primary, buttonHover(themeColors().primary), buttonPressed(themeColors().primary))
                .border(1.0f, withAlpha(themeColors().primary, 0.58f))
                .shadow(14.0f, 0.0f, 5.0f, shadowColor(0.22f, 0.10f))
                .transition(pageTransition())
                .build();

            components::button(ui, "control.soft")
                .size(buttonWidth, 54.0f)
                .icon(0xF0C8)
                .text("Outline")
                .colors(kTransparent, withAlpha(themeColors().primary, 0.10f), withAlpha(themeColors().primary, 0.18f))
                .textColor(themeColors().primary)
                .iconColor(themeColors().primary)
                .border(1.0f, withAlpha(themeColors().primary, 0.78f))
                .shadow(0.0f, 0.0f, 0.0f, shadowColor(0.0f, 0.0f))
                .transition(pageTransition())
                .build();

            components::button(ui, "control.warn")
                .size(buttonWidth, 54.0f)
                .icon(0xF1FC)
                .text("Ghost")
                .colors(kTransparent, withAlpha(themeColors().primary, 0.08f), withAlpha(themeColors().primary, 0.14f))
                .textColor(themeColors().primary)
                .iconColor(themeColors().primary)
                .border(0.0f, kTransparent)
                .shadow(0.0f, 0.0f, 0.0f, shadowColor(0.0f, 0.0f))
                .transition(pageTransition())
                .build();
        });

    components::input(ui, "control.input")
        .theme(themeColors())
        .size(fieldWidth, 44.0f)
        .value(sampleInput)
        .placeholder("Type here")
        .onChange([](const std::string& value) {
            sampleInput = value;
        })
        .build();

    ui.row("controls.toggles")
        .size(componentRowWidth, 92.0f)
        .gap(20.0f)
        .content([&] {
            ui.column("controls.checks")
                .size(componentCardWidth, 92.0f)
                .gap(12.0f)
                .content([&] {
                    components::checkbox(ui, "control.checkbox")
                        .theme(themeColors())
                        .size(componentCardWidth, 30.0f)
                        .checked(sampleChecked)
                        .text("Checkbox")
                        .onChange([](bool value) { sampleChecked = value; })
                        .build();

                    components::toggleSwitch(ui, "control.switch")
                        .theme(themeColors())
                        .size(componentCardWidth, 32.0f)
                        .checked(sampleSwitch)
                        .label("Switch")
                        .onChange([](bool value) { sampleSwitch = value; })
                        .build();
                })
                .build();

            ui.column("controls.radios")
                .size(componentCardWidth, 92.0f)
                .gap(12.0f)
                .content([&] {
                    components::radio(ui, "control.radio.a")
                        .theme(themeColors())
                        .size(componentCardWidth, 30.0f)
                        .selected(sampleRadioA)
                        .text("Radio A")
                        .onSelect([] { sampleRadioA = true; })
                        .build();

                    components::radio(ui, "control.radio.b")
                        .theme(themeColors())
                        .size(componentCardWidth, 30.0f)
                        .selected(!sampleRadioA)
                        .text("Radio B")
                        .onSelect([] { sampleRadioA = false; })
                        .build();
                })
                .build();
        });

    components::progress(ui, "control.progress")
        .theme(themeColors())
        .size(fieldWidth, 14.0f)
        .value(sampleSlider)
        .transition(core::Transition::none())
        .build();

    components::slider(ui, "control.slider")
        .theme(themeColors())
        .size(fieldWidth, 32.0f)
        .value(sampleSlider)
        .transition(pageTransition())
        .onChange([](float value) {
            sampleSlider = value;
        })
        .build();

    ui.row("controls.choice")
        .size(fieldWidth, 46.0f)
        .gap(18.0f)
        .alignItems(core::Align::CENTER)
        .content([&] {
            components::segmented(ui, "control.segmented")
                .theme(themeColors())
                .size(std::max(180.0f, (fieldWidth - 18.0f) * 0.5f), 38.0f)
                .items({"Small", "Medium", "Large"})
                .selected(sampleSegment)
                .transition(pageTransition())
                .onChange([](int index) {
                    sampleSegment = index;
                })
                .build();

            components::tabs(ui, "control.tabs")
                .theme(themeColors())
                .size(std::max(180.0f, (fieldWidth - 18.0f) * 0.5f), 42.0f)
                .items({"Overview", "Details", "Logs"})
                .selected(sampleTab)
                .transition(pageTransition())
                .onChange([](int index) {
                    sampleTab = index;
                })
                .build();
        });

    ui.text("controls.feedback.title")
        .size(width, 30.0f)
        .text("Feedback Components")
        .fontSize(25.0f)
        .lineHeight(30.0f)
        .color(textPrimary())
        .build();

    ui.row("controls.feedback")
        .size(feedbackWidth * 4.0f + 54.0f, 82.0f)
        .gap(18.0f)
        .content([&] {
            components::button(ui, "control.dialog")
                .theme(themeColors(), false)
                .size(feedbackWidth, 54.0f)
                .icon(0xF2D0)
                .text("Dialog")
                .textColor(textPrimary())
                .iconColor(accent())
                .radius(12.0f)
                .border(1.0f, borderColor(0.70f))
                .shadow(10.0f, 0.0f, 3.0f, shadowColor(0.16f, 0.08f))
                .transition(pageTransition())
                .onClick([] {
                    sampleDialogOpen = true;
                    sampleFeedback = "Dialog opened";
                })
                .build();

            components::button(ui, "control.toast")
                .theme(themeColors(), false)
                .size(feedbackWidth, 54.0f)
                .icon(0xF0F3)
                .text("Toast")
                .textColor(textPrimary())
                .iconColor(accent())
                .radius(12.0f)
                .border(1.0f, borderColor(0.70f))
                .shadow(10.0f, 0.0f, 3.0f, shadowColor(0.16f, 0.08f))
                .transition(pageTransition())
                .onClick([] {
                    sampleToastVisible = true;
                    sampleFeedback = "Toast queued";
                })
                .build();

            components::button(ui, "control.context")
                .theme(themeColors(), false)
                .size(feedbackWidth, 54.0f)
                .icon(0xF0C9)
                .text("Right Click")
                .textColor(textPrimary())
                .iconColor(accent())
                .radius(12.0f)
                .border(1.0f, borderColor(0.70f))
                .shadow(10.0f, 0.0f, 3.0f, shadowColor(0.16f, 0.08f))
                .transition(pageTransition())
                .onContextMenu([](const core::PointerEvent& event, const core::Rect&) {
                    sampleContextMenuOpen = true;
                    sampleContextMenuX = static_cast<float>(event.x);
                    sampleContextMenuY = static_cast<float>(event.y);
                    sampleFeedback = "Context menu opened";
                })
                .build();

            components::button(ui, "control.window")
                .theme(themeColors(), false)
                .size(feedbackWidth, 54.0f)
                .icon(0xF24D)
                .text("Window")
                .textColor(textPrimary())
                .iconColor(accent())
                .radius(12.0f)
                .border(1.0f, borderColor(0.70f))
                .shadow(10.0f, 0.0f, 3.0f, shadowColor(0.16f, 0.08f))
                .transition(pageTransition())
                .onClick([] {
                    app::openWindow(
                        app::DslWindowConfig{}
                            .title("Inspector")
                            .pageId("inspector")
                            .windowSize(640, 420)
                            .modal(true)
                            .clearColor(appBg()),
                        [](core::dsl::Ui& ui, const core::dsl::Screen& screen) {
                            ui.stack("inspector.root")
                                .size(screen.width, screen.height)
                                .content([&] {
                                    ui.rect("inspector.bg")
                                        .size(screen.width, screen.height)
                                        .color(appBg())
                                        .build();

                                    ui.text("inspector.title")
                                        .x(28.0f)
                                        .y(24.0f)
                                        .size(std::max(0.0f, screen.width - 56.0f), 34.0f)
                                        .text("Inspector Window")
                                        .fontSize(28.0f)
                                        .lineHeight(34.0f)
                                        .color(textPrimary())
                                        .build();

                                    ui.text("inspector.note")
                                        .x(28.0f)
                                        .y(70.0f)
                                        .size(std::max(0.0f, screen.width - 56.0f), 24.0f)
                                        .text("This window was opened from a button callback.")
                                        .fontSize(16.0f)
                                        .lineHeight(22.0f)
                                        .color(textMuted())
                                        .build();
                                })
                                .build();
                        });
                    sampleFeedback = "Window opened";
                })
                .build();
        });

    ui.text("controls.feedback.state")
        .size(width, 22.0f)
        .text(sampleFeedback)
        .fontSize(15.0f)
        .lineHeight(20.0f)
        .color(textMuted())
        .build();

    ui.text("controls.data.title")
        .size(width, 30.0f)
        .text("Selection & Data")
        .fontSize(25.0f)
        .lineHeight(30.0f)
        .color(textPrimary())
        .build();

    ui.row("controls.pickers.row")
        .size(pickerRowWidth, 56.0f)
        .gap(pickerGap)
        .content([&] {
            components::button(ui, "control.datepicker.open")
                .theme(themeColors(), false)
                .size(pickerWidth, 44.0f)
                .icon(0xF073)
                .text(sampleDateText())
                .textColor(textPrimary())
                .iconColor(accent())
                .radius(12.0f)
                .border(1.0f, borderColor(0.70f))
                .shadow(10.0f, 0.0f, 3.0f, shadowColor(0.16f, 0.08f))
                .transition(pageTransition())
                .onClick([] {
                    sampleDateOpen = true;
                    sampleTimeOpen = false;
                    sampleColorOpen = false;
                    sampleDropdownOpen = false;
                    sampleFeedback = "Date picker opened";
                })
                .build();

            components::button(ui, "control.timepicker.open")
                .theme(themeColors(), false)
                .size(pickerWidth, 44.0f)
                .icon(0xF017)
                .text(sampleTimeText())
                .textColor(textPrimary())
                .iconColor(accent())
                .radius(12.0f)
                .border(1.0f, borderColor(0.70f))
                .shadow(10.0f, 0.0f, 3.0f, shadowColor(0.16f, 0.08f))
                .transition(pageTransition())
                .onClick([] {
                    sampleTimeOpen = true;
                    sampleDateOpen = false;
                    sampleColorOpen = false;
                    sampleDropdownOpen = false;
                    sampleFeedback = "Time picker opened";
                })
                .build();

            components::button(ui, "control.colorpicker.open")
                .theme(themeColors(), false)
                .size(pickerWidth, 44.0f)
                .icon(0xF53F)
                .text(colorHex(sampleColor))
                .textColor(textPrimary())
                .iconColor(sampleColor)
                .radius(12.0f)
                .border(1.0f, borderColor(0.70f))
                .shadow(10.0f, 0.0f, 3.0f, shadowColor(0.16f, 0.08f))
                .transition(pageTransition())
                .onClick([] {
                    sampleColorOpen = true;
                    sampleDateOpen = false;
                    sampleTimeOpen = false;
                    sampleDropdownOpen = false;
                    sampleFeedback = "Color picker opened";
                })
                .build();
        });

    ui.row("controls.data.row")
        .size(dropdownWidth + tableWidth + dataRowGap, dataRowHeight)
        .gap(dataRowGap)
        .content([&] {
            components::dropdown(ui, "control.dropdown")
                .theme(themeColors())
                .size(dropdownWidth, 44.0f)
                .items({"Draft", "Review", "Published", "Archived"})
                .selected(sampleDropdown)
                .open(sampleDropdownOpen)
                .transition(pageTransition())
                .onOpenChange([](bool open) {
                    sampleDropdownOpen = open;
                    if (open) {
                        sampleDateOpen = false;
                        sampleTimeOpen = false;
                        sampleColorOpen = false;
                    }
                })
                .onChange([](int index) {
                    sampleDropdown = index;
                    sampleFeedback = "Dropdown changed";
                })
                .build();

            components::dataTable(ui, "control.table")
                .theme(themeColors())
                .size(tableWidth, 174.0f)
                .columns({"Name", "Status", "Owner"})
                .rows({
                    {"EUI Core", "Active", "Sudo"},
                    {"Gallery", "Review", "Design"},
                    {"Docs", "Draft", "DevRel"},
                    {"Runtime", "Stable", "Engine"}
                })
                .transition(pageTransition())
                .build();
        });

    ui.text("controls.charts.title")
        .size(width, 30.0f)
        .text("Charts")
        .fontSize(25.0f)
        .lineHeight(30.0f)
        .color(textPrimary())
        .build();

    ui.row("controls.charts.row")
        .size(chartRowWidth, chartHeight)
        .gap(chartGap)
        .content([&] {
            components::linechart(ui, "control.chart.line")
                .theme(themeColors())
                .size(chartWidth, chartHeight)
                .title("LineChart")
                .values({0.22f, 0.30f, 0.20f, 0.55f, 0.42f, 0.86f})
                .labels({"Jan", "Feb", "Mar", "Apr", "May", "Jun"})
                .transition(pageTransition())
                .build();

            components::barchart(ui, "control.chart.bar")
                .theme(themeColors())
                .size(chartWidth, chartHeight)
                .title("BarChart")
                .values({0.92f, 0.36f, 0.68f, 0.52f})
                .labels({"D1", "D2", "D3", "D4"})
                .transition(pageTransition())
                .build();

            components::piechart(ui, "control.chart.pie")
                .theme(themeColors())
                .size(chartWidth, chartHeight)
                .title("PieChart")
                .values({0.42f, 0.24f, 0.18f, 0.16f})
                .labels({"Blue", "Green", "Orange", "Pink"})
                .transition(pageTransition())
                .build();
        });

    ui.text("controls.primitives.title")
        .size(width, 30.0f)
        .text("Primitive Properties")
        .fontSize(25.0f)
        .lineHeight(30.0f)
        .color(textPrimary())
        .build();

    ui.row("properties.a")
        .size(rowWidth, rowHeight)
        .gap(cardGap)
        .content([&] {
            propertyCard(ui, "prop.color", "Color", "hover + press", {0.22f, 0.48f, 0.82f, 1.0f}, "color", cardWidth);
            propertyCard(ui, "prop.border", "Border", "animated edge", surface(), "border", cardWidth);
            propertyCard(ui, "prop.shadow", "Shadow", "elevation", surfaceSoft(), "shadow", cardWidth);
        });

    ui.row("properties.b")
        .size(rowWidth, rowHeight)
        .gap(cardGap)
        .content([&] {
            propertyCard(ui, "prop.alpha", "Opacity", "transparent fill", {0.86f, 0.38f, 0.52f, 0.58f}, "color", cardWidth);
            propertyCard(ui, "prop.blur", "Blur", "glass card", {0.78f, 0.92f, 1.0f, 0.22f}, "blur", cardWidth);
            propertyCard(ui, "prop.rotate", "Rotate", "transform", {0.48f, 0.64f, 0.36f, 1.0f}, "rotate", cardWidth);
        });

}

void textSample(core::dsl::Ui& ui, const std::string& id, const std::string& text, float size, float height, float width, const core::Color& color) {
    ui.text(id)
        .size(width, height)
        .text(text)
        .fontSize(size)
        .lineHeight(height)
        .color(color)
        .transition(textTransition())
        .build();
}

std::string colorHex(core::Color color) {
    const int r = static_cast<int>(std::clamp(color.r, 0.0f, 1.0f) * 255.0f + 0.5f);
    const int g = static_cast<int>(std::clamp(color.g, 0.0f, 1.0f) * 255.0f + 0.5f);
    const int b = static_cast<int>(std::clamp(color.b, 0.0f, 1.0f) * 255.0f + 0.5f);
    char result[8] = {};
    std::snprintf(result, sizeof(result), "#%02X%02X%02X", r, g, b);
    return result;
}

void themeSwatch(core::dsl::Ui& ui, const std::string& id, const std::string& name, const core::Color& color, float width) {
    ui.stack(id)
        .size(width, 86.0f)
        .content([&] {
            ui.rect(id + ".bg")
                .size(width, 86.0f)
                .color(surface())
                .radius(12.0f)
                .border(1.0f, borderColor(0.72f))
                .build();

            ui.rect(id + ".chip")
                .x(12.0f)
                .y(12.0f)
                .size(std::max(0.0f, width - 24.0f), 26.0f)
                .color(color)
                .radius(8.0f)
                .border(1.0f, withAlpha(textPrimary(), color.a < 0.55f ? 0.20f : 0.08f))
                .build();

            ui.text(id + ".name")
                .x(12.0f)
                .y(44.0f)
                .size(std::max(0.0f, width - 24.0f), 20.0f)
                .text(name)
                .fontSize(14.0f)
                .lineHeight(18.0f)
                .color(textPrimary())
                .horizontalAlign(core::HorizontalAlign::Center)
                .build();

            ui.text(id + ".value")
                .x(12.0f)
                .y(64.0f)
                .size(std::max(0.0f, width - 24.0f), 18.0f)
                .text(colorHex(color))
                .fontSize(12.0f)
                .lineHeight(15.0f)
                .color(textMuted())
                .horizontalAlign(core::HorizontalAlign::Center)
                .build();
        })
        .build();
}

void composeStylePage(core::dsl::Ui& ui, float width, float height) {
    const float textWidth = std::max(240.0f, std::min(width, 760.0f));
    const float iconGap = 20.0f;
    const float iconCardWidth = std::max(60.0f, std::min(120.0f, (width - iconGap * 3.0f) / 4.0f));
    const float iconRowWidth = iconCardWidth * 4.0f + iconGap * 3.0f;
    const float swatchGap = 14.0f;
    const float swatchWidth = std::max(94.0f, std::min(142.0f, (width - swatchGap * 3.0f) / 4.0f));
    const float swatchRowWidth = swatchWidth * 4.0f + swatchGap * 3.0f;
    const components::theme::ThemeColorTokens tokens = themeColors();
    const components::theme::PageVisualTokens visuals = pageVisuals();

    ui.column("text.samples")
        .size(width, std::min(height, 380.0f))
        .gap(12.0f)
        .content([&] {
            textSample(ui, "txt.display", "Display 48 - Gallery Title", 48.0f, 58.0f, textWidth, textPrimary());
            textSample(ui, "txt.h1", "Heading 36 - Section Header", 36.0f, 46.0f, textWidth, withAlpha(textPrimary(), 0.92f));
            textSample(ui, "txt.h2", "Heading 28 - Component Name", 28.0f, 38.0f, textWidth, withAlpha(textPrimary(), 0.82f));
            textSample(ui, "txt.body", "Body 20 - Text can wrap, align and use custom colors.", 20.0f, 30.0f, textWidth, bodyText());
            textSample(ui, "txt.small", "Small 15 - Secondary metadata and compact labels.", 15.0f, 24.0f, textWidth, withAlpha(textPrimary(), 0.58f));

            ui.row("text.icons")
                .size(iconRowWidth, 74.0f)
                .gap(iconGap)
                .content([&] {
                    const unsigned int icons[] = {0xF015, 0xF1FC, 0xF013, 0xF05A};
                    const char* names[] = {"Home", "Theme", "Settings", "Info"};
                    for (int i = 0; i < 4; ++i) {
                        ui.stack(std::string("text.icon.card.") + std::to_string(i))
                            .size(iconCardWidth, 72.0f)
                            .content([&, i] {
                                ui.text(std::string("text.icon.") + std::to_string(i))
                                    .size(iconCardWidth, 36.0f)
                                    .icon(icons[i])
                                    .fontSize(28.0f)
                                    .lineHeight(34.0f)
                                    .color(accent())
                                    .horizontalAlign(core::HorizontalAlign::Center)
                                    .transition(textTransition())
                                    .build();

                                caption(ui, std::string("text.icon.label.") + std::to_string(i), names[i], iconCardWidth, 40.0f);
                            })
                            .build();
                    }
                });
        });

    ui.text("style.theme.title")
        .size(width, 30.0f)
        .text("Theme Color Tokens")
        .fontSize(25.0f)
        .lineHeight(30.0f)
        .color(textPrimary())
        .build();

    ui.row("style.theme.tokens.a")
        .size(swatchRowWidth, 88.0f)
        .gap(swatchGap)
        .content([&] {
            themeSwatch(ui, "style.color.background", "background", tokens.background, swatchWidth);
            themeSwatch(ui, "style.color.primary", "primary", tokens.primary, swatchWidth);
            themeSwatch(ui, "style.color.surface", "surface", tokens.surface, swatchWidth);
            themeSwatch(ui, "style.color.surfaceHover", "surfaceHover", tokens.surfaceHover, swatchWidth);
        });

    ui.row("style.theme.tokens.b")
        .size(swatchRowWidth, 88.0f)
        .gap(swatchGap)
        .content([&] {
            themeSwatch(ui, "style.color.surfaceActive", "surfaceActive", tokens.surfaceActive, swatchWidth);
            themeSwatch(ui, "style.color.text", "text", tokens.text, swatchWidth);
            themeSwatch(ui, "style.color.border", "border", tokens.border, swatchWidth);
            themeSwatch(ui, "style.color.accent", "pickerAccent", accent(), swatchWidth);
        });

    ui.text("style.visual.title")
        .size(width, 30.0f)
        .text("Page Visual Colors")
        .fontSize(25.0f)
        .lineHeight(30.0f)
        .color(textPrimary())
        .build();

    ui.row("style.theme.visuals")
        .size(swatchRowWidth, 88.0f)
        .gap(swatchGap)
        .content([&] {
            themeSwatch(ui, "style.color.title", "titleColor", visuals.titleColor, swatchWidth);
            themeSwatch(ui, "style.color.subtitle", "subtitleColor", visuals.subtitleColor, swatchWidth);
            themeSwatch(ui, "style.color.body", "bodyColor", visuals.bodyColor, swatchWidth);
            themeSwatch(ui, "style.color.softAccent", "softAccent", visuals.softAccentColor, swatchWidth);
        });
}

void composeAnimationPage(core::dsl::Ui& ui, float width, float height) {
    (void)height;
    const float stageWidth = std::max(280.0f, std::min(width, 860.0f));
    const float stageHeight = 268.0f;
    const float actorWidth = animationRotated ? 150.0f : 118.0f;
    const float actorHeight = animationRotated ? 96.0f : 72.0f;
    const float actorScale = animationScaled ? 1.18f : 1.0f;
    const float actorTravel = std::max(46.0f, stageWidth - actorWidth * actorScale - 58.0f);
    const float buttonWidth = std::max(92.0f, std::min(166.0f, (stageWidth - 36.0f) / 3.0f));
    const float buttonRowWidth = buttonWidth * 3.0f + 36.0f;
    const core::Color rotateColor{0.84f, 0.46f, 0.60f, 1.0f};
    const core::Color fadeColor{0.50f, 0.72f, 0.34f, 1.0f};
    const core::Color scaleColor{0.92f, 0.62f, 0.26f, 1.0f};
    const core::Color radiusColor{0.50f, 0.58f, 0.94f, 1.0f};
    const core::Color glowColor{0.28f, 0.76f, 0.72f, 1.0f};

    ui.row("animation.controls")
        .size(buttonRowWidth, 58.0f)
        .gap(18.0f)
        .content([&] {
            components::button(ui, "anim.move")
                .size(buttonWidth, 50.0f)
                .text("Move")
                .colors(animationMoved ? accent() : surfaceSoft(),
                        buttonHover(animationMoved ? accent() : surfaceSoft()),
                        buttonPressed(animationMoved ? accent() : surfaceSoft()))
                .textColor(animationMoved || optionNight ? core::Color{0.94f, 0.97f, 1.0f, 1.0f} : textPrimary())
                .iconColor(animationMoved || optionNight ? core::Color{0.94f, 0.97f, 1.0f, 1.0f} : textPrimary())
                .border(1.0f, animationMoved ? withAlpha(accent(), 0.58f) : borderColor(0.70f))
                .shadow(12.0f, 0.0f, 4.0f, shadowColor(0.18f, 0.08f))
                .onClick([] { animationMoved = !animationMoved; })
                .transition(pageTransition())
                .build();

            components::button(ui, "anim.rotate")
                .size(buttonWidth, 50.0f)
                .text("Rotate")
                .colors(animationRotated ? rotateColor : surfaceSoft(),
                        buttonHover(animationRotated ? rotateColor : surfaceSoft()),
                        buttonPressed(animationRotated ? rotateColor : surfaceSoft()))
                .textColor(animationRotated || optionNight ? core::Color{0.94f, 0.97f, 1.0f, 1.0f} : textPrimary())
                .iconColor(animationRotated || optionNight ? core::Color{0.94f, 0.97f, 1.0f, 1.0f} : textPrimary())
                .border(1.0f, animationRotated ? withAlpha(rotateColor, 0.58f) : borderColor(0.70f))
                .shadow(12.0f, 0.0f, 4.0f, shadowColor(0.18f, 0.08f))
                .onClick([] { animationRotated = !animationRotated; })
                .transition(pageTransition())
                .build();

            components::button(ui, "anim.fade")
                .size(buttonWidth, 50.0f)
                .text("Fade")
                .colors(animationFaded ? fadeColor : surfaceSoft(),
                        buttonHover(animationFaded ? fadeColor : surfaceSoft()),
                        buttonPressed(animationFaded ? fadeColor : surfaceSoft()))
                .textColor(animationFaded || optionNight ? core::Color{0.94f, 0.97f, 1.0f, 1.0f} : textPrimary())
                .iconColor(animationFaded || optionNight ? core::Color{0.94f, 0.97f, 1.0f, 1.0f} : textPrimary())
                .border(1.0f, animationFaded ? withAlpha(fadeColor, 0.58f) : borderColor(0.70f))
                .shadow(12.0f, 0.0f, 4.0f, shadowColor(0.18f, 0.08f))
                .onClick([] { animationFaded = !animationFaded; })
                .transition(pageTransition())
                .build();
        });

    ui.row("animation.controls.extra")
        .size(buttonRowWidth, 58.0f)
        .gap(18.0f)
        .content([&] {
            components::button(ui, "anim.scale")
                .size(buttonWidth, 50.0f)
                .text("Scale")
                .colors(animationScaled ? scaleColor : surfaceSoft(),
                        buttonHover(animationScaled ? scaleColor : surfaceSoft()),
                        buttonPressed(animationScaled ? scaleColor : surfaceSoft()))
                .textColor(animationScaled || optionNight ? core::Color{0.94f, 0.97f, 1.0f, 1.0f} : textPrimary())
                .iconColor(animationScaled || optionNight ? core::Color{0.94f, 0.97f, 1.0f, 1.0f} : textPrimary())
                .border(1.0f, animationScaled ? withAlpha(scaleColor, 0.58f) : borderColor(0.70f))
                .shadow(12.0f, 0.0f, 4.0f, shadowColor(0.18f, 0.08f))
                .onClick([] { animationScaled = !animationScaled; })
                .transition(pageTransition())
                .build();

            components::button(ui, "anim.radius")
                .size(buttonWidth, 50.0f)
                .text("Radius")
                .colors(animationRounded ? radiusColor : surfaceSoft(),
                        buttonHover(animationRounded ? radiusColor : surfaceSoft()),
                        buttonPressed(animationRounded ? radiusColor : surfaceSoft()))
                .textColor(animationRounded || optionNight ? core::Color{0.94f, 0.97f, 1.0f, 1.0f} : textPrimary())
                .iconColor(animationRounded || optionNight ? core::Color{0.94f, 0.97f, 1.0f, 1.0f} : textPrimary())
                .border(1.0f, animationRounded ? withAlpha(radiusColor, 0.58f) : borderColor(0.70f))
                .shadow(12.0f, 0.0f, 4.0f, shadowColor(0.18f, 0.08f))
                .onClick([] { animationRounded = !animationRounded; })
                .transition(pageTransition())
                .build();

            components::button(ui, "anim.glow")
                .size(buttonWidth, 50.0f)
                .text("Glow")
                .colors(animationGlowing ? glowColor : surfaceSoft(),
                        buttonHover(animationGlowing ? glowColor : surfaceSoft()),
                        buttonPressed(animationGlowing ? glowColor : surfaceSoft()))
                .textColor(animationGlowing || optionNight ? core::Color{0.94f, 0.97f, 1.0f, 1.0f} : textPrimary())
                .iconColor(animationGlowing || optionNight ? core::Color{0.94f, 0.97f, 1.0f, 1.0f} : textPrimary())
                .border(1.0f, animationGlowing ? withAlpha(glowColor, 0.58f) : borderColor(0.70f))
                .shadow(12.0f, 0.0f, 4.0f, shadowColor(0.18f, 0.08f))
                .onClick([] { animationGlowing = !animationGlowing; })
                .transition(pageTransition())
                .build();
        });

    ui.stack("animation.stage")
        .size(stageWidth, stageHeight)
        .content([&] {
            ui.rect("animation.stage.bg")
                .size(stageWidth, stageHeight)
                .color(surface())
                .radius(24.0f)
                .border(1.0f, borderColor())
                .build();

            ui.rect("animation.stage.track")
                .x(38.0f)
                .y(stageHeight - 54.0f)
                .size(std::max(0.0f, stageWidth - 76.0f), 2.0f)
                .color(borderColor(0.48f))
                .radius(1.0f)
                .build();

            ui.rect("animation.actor")
                .x(animationMoved ? actorTravel : 46.0f)
                .y(animationMoved ? 70.0f : 92.0f)
                .size(actorWidth, actorHeight)
                .color(animationMoved ? accent() : radiusColor)
                .radius(animationRounded ? actorHeight * 0.5f : (animationRotated ? 30.0f : 18.0f))
                .rotate(animationRotated ? 0.42f : 0.0f)
                .scale(actorScale)
                .transformOrigin(0.5f, 0.5f)
                .opacity(animationFaded ? 0.36f : 1.0f)
                .shadow(animationGlowing ? 44.0f : 26.0f, 0.0f, animationGlowing ? 18.0f : 12.0f,
                        animationGlowing ? withAlpha(glowColor, optionNight ? 0.42f : 0.26f) : shadowColor(0.32f, 0.16f))
                .transition(motionTransition())
                .animate(core::AnimProperty::Frame | core::AnimProperty::Color | core::AnimProperty::Opacity |
                         core::AnimProperty::Radius | core::AnimProperty::Shadow | core::AnimProperty::Transform)
                .build();
        });
}

void settingRow(core::dsl::Ui& ui, const std::string& id, const std::string& title, const std::string& note, bool enabled, float width, const std::function<void()>& onClick) {
    const float toggleX = std::max(0.0f, width - 80.0f);
    const float textWidth = std::max(0.0f, width - 132.0f);
    components::SwitchStyle switchStyle(themeColors());
    switchStyle.on = accent();
    switchStyle.knob = optionNight
        ? core::Color{0.96f, 0.98f, 1.0f, 1.0f}
        : core::Color{1.0f, 1.0f, 1.0f, 1.0f};

    ui.stack(id)
        .size(width, 72.0f)
        .content([&] {
            ui.rect(id + ".hit")
                .size(width, 72.0f)
                .states(surfaceSoft(), buttonHover(surfaceSoft()), buttonPressed(surfaceSoft()))
                .radius(16.0f)
                .transition(pageTransition())
                .onClick(onClick)
                .build();

            ui.text(id + ".title")
                .x(24.0f)
                .y(12.0f)
                .size(textWidth, 28.0f)
                .text(title)
                .fontSize(20.0f)
                .lineHeight(26.0f)
                .color(textPrimary())
                .build();

            ui.text(id + ".note")
                .x(24.0f)
                .y(42.0f)
                .size(textWidth, 22.0f)
                .text(note)
                .fontSize(15.0f)
                .lineHeight(20.0f)
                .color(textMuted())
                .build();

            ui.stack(id + ".switch.wrap")
                .x(toggleX)
                .y(22.0f)
                .size(46.0f, 26.0f)
                .content([&] {
                    components::toggleSwitch(ui, id + ".switch")
                        .size(46.0f, 26.0f)
                        .trackSize(46.0f, 26.0f)
                        .checked(enabled)
                        .style(switchStyle)
                        .transition(pageTransition())
                        .onChange([onClick](bool) {
                            if (onClick) {
                                onClick();
                            }
                        })
                        .build();
                })
                .build();
        })
        .build();
}

void composeSettingsPage(core::dsl::Ui& ui, float width, float height) {
    const float rowWidth = std::max(0.0f, std::min(width, 720.0f));

    ui.column("settings.list")
        .size(rowWidth, std::min(height, 430.0f))
        .gap(14.0f)
        .content([&] {
            settingRow(ui, "setting.dense", "Dense layout", "Use tighter spacing for gallery pages.", optionDense, rowWidth, [] { optionDense = !optionDense; });
            settingRow(ui, "setting.glass", "Glass surfaces", "Show transparent panel examples in controls.", optionGlass, rowWidth, [] { optionGlass = !optionGlass; });
            settingRow(ui, "setting.motion", "Animated transitions", "Keep page and property transitions enabled.", optionMotion, rowWidth, [] { optionMotion = !optionMotion; });
            settingRow(ui, "setting.unlockFps", "Unlock 90 FPS limit", "Let animation rendering use the display refresh rate.", optionUnlockFps, rowWidth, [] { optionUnlockFps = !optionUnlockFps; });
            settingRow(ui, "setting.night", "Night mode", "Switch gallery between light and dark theme tokens.", optionNight, rowWidth, [] { optionNight = !optionNight; });
        });
}

void composeBingPage(core::dsl::Ui& ui, float width, float height) {
    const float contentWidth = std::max(260.0f, std::min(width, 860.0f));
    const float cardGap = 20.0f;
    const float cardWidth = std::max(116.0f, std::min(400.0f, (contentWidth - cardGap) * 0.5f));
    const float rowWidth = cardWidth * 2.0f + cardGap;
    const float mediaHeight = 252.0f;
    const float mediaImageHeight = 198.0f;
    const float apiHeight = 138.0f;

    ui.column("bing.body")
        .size(width, height)
        .alignItems(core::Align::CENTER)
        .gap(22.0f)
        .content([&] {
            ui.row("bing.media")
                .size(rowWidth, mediaHeight)
                .gap(cardGap)
                .content([&] {
                    bingImageCard(ui, "bing.media.today", "Bing Today", 0, "zh-CN", cardWidth, mediaHeight, mediaImageHeight);
                    bingImageCard(ui, "bing.media.yesterday", "Bing Yesterday", 1, "zh-CN", cardWidth, mediaHeight, mediaImageHeight);
                });

            ui.stack("bing.api")
                .size(contentWidth, apiHeight)
                .content([&] {
                    ui.rect("bing.api.bg")
                        .size(contentWidth, apiHeight)
                        .color(surface())
                        .radius(18.0f)
                        .border(1.0f, borderColor())
                        .build();

                    ui.text("bing.api.title")
                        .x(22.0f)
                        .y(18.0f)
                        .size(std::max(0.0f, contentWidth - 44.0f), 30.0f)
                        .text("Bing API text")
                        .fontSize(22.0f)
                        .lineHeight(26.0f)
                        .color(textPrimary())
                        .build();

                    ui.text("bing.api.text")
                        .x(22.0f)
                        .y(54.0f)
                        .size(std::max(0.0f, contentWidth - 44.0f), std::max(0.0f, apiHeight - 68.0f))
                        .text(bingApiText())
                        .fontSize(16.0f)
                        .lineHeight(22.0f)
                        .maxWidth(std::max(0.0f, contentWidth - 44.0f))
                        .wrap(true)
                        .color(textMuted())
                        .build();
                })
                .build();
        });
}

void composeAboutPage(core::dsl::Ui& ui, float width, float height) {
    const float contentWidth = std::max(280.0f, std::min(width, 860.0f));
    const bool compact = contentWidth < 620.0f;
    const float logoSize = compact ? 112.0f : 126.0f;
    const float buttonGap = compact ? 16.0f : 14.0f;
    const float buttonWidth = compact
        ? std::max(124.0f, std::min(180.0f, (contentWidth - buttonGap) * 0.5f))
        : 162.0f;
    const float buttonRowWidth = buttonWidth * 2.0f + buttonGap;
    const float heroHeight = compact ? 342.0f : 238.0f;
    const float licenseHeight = 92.0f;

    ui.column("about.body")
        .size(width, height)
        .alignItems(core::Align::CENTER)
        .gap(20.0f)
        .content([&] {
            ui.stack("about.hero")
                .size(contentWidth, heroHeight)
                .content([&] {
                    ui.rect("about.hero.bg")
                        .size(contentWidth, heroHeight)
                        .color(surface())
                        .radius(22.0f)
                        .border(1.0f, borderColor())
                        .build();

                    const float logoX = compact ? (contentWidth - logoSize) * 0.5f : 24.0f;
                    const float logoY = compact ? 20.0f : 40.0f;
                    ui.rect("about.logo.frame")
                        .x(logoX)
                        .y(logoY)
                        .size(logoSize, logoSize)
                        .color(surfaceSoft())
                        .radius(28.0f)
                        .shadow(20.0f, 0.0f, 10.0f, shadowColor(0.24f, 0.12f))
                        .build();

                    ui.image("about.logo.image")
                        .x(logoX)
                        .y(logoY)
                        .size(logoSize, logoSize)
                        .source("assets/icon.png")
                        .radius(28.0f)
                        .cover()
                        .build();

                    const float infoX = compact ? 24.0f : logoX + logoSize + 30.0f;
                    const float infoY = compact ? logoY + logoSize + 18.0f : 38.0f;
                    const float infoWidth = compact ? std::max(0.0f, contentWidth - 48.0f) : std::max(0.0f, contentWidth - infoX - 24.0f);
                    ui.text("about.hero.title")
                        .x(infoX)
                        .y(infoY)
                        .size(infoWidth, 36.0f)
                        .text("EUI Neo")
                        .fontSize(32.0f)
                        .lineHeight(36.0f)
                        .color(textPrimary())
                        .horizontalAlign(compact ? core::HorizontalAlign::Center : core::HorizontalAlign::Left)
                        .build();

                    ui.text("about.hero.copy")
                        .x(infoX)
                        .y(infoY + 42.0f)
                        .size(infoWidth, compact ? 58.0f : 58.0f)
                        .text("A lightweight C++ UI playground for themed controls, motion and image rendering.")
                        .fontSize(17.0f)
                        .lineHeight(24.0f)
                        .maxWidth(infoWidth)
                        .wrap(true)
                        .color(textMuted())
                        .horizontalAlign(compact ? core::HorizontalAlign::Center : core::HorizontalAlign::Left)
                        .build();

                    ui.row("about.actions")
                        .x(compact ? (contentWidth - buttonRowWidth) * 0.5f : infoX)
                        .y(compact ? heroHeight - 74.0f : 162.0f)
                        .size(buttonRowWidth, 52.0f)
                        .gap(buttonGap)
                        .content([&] {
                            components::button(ui, "about.github")
                                .size(buttonWidth, 52.0f)
                                .icon(0xF0C1)
                                .iconSize(20.0f)
                                .fontSize(19.0f)
                                .text("GitHub")
                                .colors(accent(), buttonHover(accent()), buttonPressed(accent()))
                                .radius(12.0f)
                                .border(1.0f, withAlpha(accent(), 0.58f))
                                .shadow(14.0f, 0.0f, 5.0f, shadowColor(0.22f, 0.10f))
                                .transition(pageTransition())
                                .onClick([] {
                                    core::platform::openUrl("https://github.com/sudoevolve/EUI-NEO");
                                })
                                .build();

                            components::button(ui, "about.group")
                                .size(buttonWidth, 52.0f)
                                .icon(0xF0C0)
                                .iconSize(19.0f)
                                .fontSize(19.0f)
                                .text("Group")
                                .colors(surfaceSoft(), buttonHover(surfaceSoft()), buttonPressed(surfaceSoft()))
                                .textColor(textPrimary())
                                .iconColor(textPrimary())
                                .radius(12.0f)
                                .border(1.0f, borderColor())
                                .shadow(12.0f, 0.0f, 4.0f, shadowColor(0.18f, 0.08f))
                                .transition(pageTransition())
                                .onClick([] {
                                    core::platform::openUrl("https://qm.qq.com/q/kaPB4paOpa");
                                })
                                .build();
                        });
                })
                .build();

            ui.stack("about.license")
                .size(contentWidth, licenseHeight)
                .content([&] {
                    ui.rect("about.license.bg")
                        .size(contentWidth, licenseHeight)
                        .color(surface())
                        .radius(18.0f)
                        .border(1.0f, borderColor())
                        .build();

                    ui.text("about.license.title")
                        .x(22.0f)
                        .y(12.0f)
                        .size(std::max(0.0f, contentWidth - 44.0f), 26.0f)
                        .text("License")
                        .fontSize(22.0f)
                        .lineHeight(25.0f)
                        .color(textPrimary())
                        .build();

                    ui.text("about.license.copy")
                        .x(22.0f)
                        .y(40.0f)
                        .size(std::max(0.0f, contentWidth - 44.0f), 22.0f)
                        .text("Copyright @2026 SudoEvolve")
                        .fontSize(17.0f)
                        .lineHeight(21.0f)
                        .color(textMuted())
                        .build();

                    ui.text("about.license.type")
                        .x(22.0f)
                        .y(64.0f)
                        .size(std::max(0.0f, contentWidth - 44.0f), 22.0f)
                        .text("Licensed under apache2.0")
                        .fontSize(16.0f)
                        .lineHeight(20.0f)
                        .color(textMuted())
                        .build();

                })
                .build();
        });
}

void composePageBody(core::dsl::Ui& ui, float width, float height) {
    if (selectedPage == 0) {
        composeControlsPage(ui, width, height);
    } else if (selectedPage == 1) {
        composeStylePage(ui, width, height);
    } else if (selectedPage == 2) {
        composeAnimationPage(ui, width, height);
    } else if (selectedPage == 3) {
        composeSettingsPage(ui, width, height);
    } else if (selectedPage == 4) {
        composeBingPage(ui, width, height);
    } else {
        composeAboutPage(ui, width, height);
    }
}

float pageBodyContentHeight(float viewportHeight) {
    const float bodyGap = optionDense ? 18.0f : 26.0f;
    if (selectedPage == 0) {
        const float controlsContentHeight =
            30.0f + 68.0f + 44.0f + 92.0f + 14.0f + 32.0f + 46.0f +
            30.0f + 82.0f + 22.0f + 30.0f + 56.0f + 200.0f + 30.0f + 236.0f +
            30.0f + 144.0f + 144.0f + bodyGap * 17.0f + 56.0f;
        return std::max(viewportHeight, controlsContentHeight);
    }
    if (selectedPage == 1) {
        const float styleContentHeight =
            380.0f + 30.0f + 88.0f + 88.0f + 30.0f + 88.0f +
            bodyGap * 5.0f + 40.0f;
        return std::max(viewportHeight, styleContentHeight);
    }
    if (selectedPage == 2) {
        return viewportHeight;
    }
    if (selectedPage == 3) {
        return std::max(viewportHeight, 430.0f);
    }
    if (selectedPage == 4) {
        return std::max(viewportHeight, 440.0f);
    }
    return viewportHeight;
}

void composeContent(core::dsl::Ui& ui, float width, float height) {
    const float shellWidth = std::max(0.0f, width - 72.0f);
    const float innerWidth = std::max(0.0f, shellWidth - 64.0f);
    const float shellHeight = std::max(0.0f, height - 72.0f);
    const float innerHeight = std::max(0.0f, shellHeight - 64.0f);
    const float headerGap = optionDense ? 18.0f : 26.0f;
    const float bodyHeight = std::max(0.0f, innerHeight - 46.0f - 30.0f - headerGap * 2.0f);
    const float contentHeight = pageBodyContentHeight(bodyHeight);
    const float maxScroll = std::max(0.0f, contentHeight - bodyHeight);
    const bool scrollable = maxScroll > 0.0f;
    const int page = std::clamp(selectedPage, 0, 5);
    pageScroll[page] = std::clamp(pageScroll[page], 0.0f, maxScroll);
    const float scrollOffset = pageScroll[page];
    const float scrollWidth = scrollable ? 8.0f : 0.0f;
    const float scrollGap = scrollable ? 16.0f : 0.0f;
    const float bodyContentWidth = std::max(0.0f, innerWidth - scrollWidth - scrollGap);

    ui.stack("content.area")
        .size(width, height)
        .content([&] {
            ui.rect("content.bg")
                .size(width, height)
                .color(appBg())
                .build();

            ui.rect("page.shell")
                .size(shellWidth, shellHeight)
                .margin(36.0f)
                .color(surface())
                .radius(26.0f)
                .border(1.0f, borderColor())
                .shadow(30.0f, 0.0f, 16.0f, shadowColor(0.28f, 0.14f))
                .transition(pageTransition())
                .build();

            ui.column("page.content")
                .size(innerWidth, innerHeight)
                .margin(68.0f)
                .gap(headerGap)
                .content([&] {
                    ui.text("page.title")
                        .size(innerWidth, 46.0f)
                        .text(pageTitle())
                        .fontSize(38.0f)
                        .lineHeight(44.0f)
                        .color(accent())
                        .transition(textTransition())
                        .build();

                    ui.text("page.subtitle")
                        .size(innerWidth, 30.0f)
                        .text(pageSubtitle())
                        .fontSize(20.0f)
                        .lineHeight(28.0f)
                        .color(textMuted())
                        .transition(textTransition())
                        .build();

                    auto body = ui.stack("page.body")
                        .size(innerWidth, bodyHeight)
                        .clip();
                    if (scrollable) {
                        body.onScroll([page, maxScroll](const core::ScrollEvent& event) {
                            pageScroll[page] = std::clamp(pageScroll[page] - static_cast<float>(event.y) * 48.0f, 0.0f, maxScroll);
                        });
                    }
                    body.content([&] {
                            ui.column("page.body.content")
                                .y(-scrollOffset)
                                .size(bodyContentWidth, contentHeight)
                                .gap(headerGap)
                                .content([&] {
                                    composePageBody(ui, bodyContentWidth, contentHeight);
                                })
                                .build();

                            if (scrollable) {
                                components::scroll(ui, "page.body.scroll")
                                    .theme(themeColors())
                                    .x(std::max(0.0f, innerWidth - scrollWidth))
                                    .size(scrollWidth, bodyHeight)
                                    .viewport(bodyHeight)
                                    .content(contentHeight)
                                    .offset(scrollOffset)
                                    .zIndex(10)
                                    .onChange([page](float value) {
                                        pageScroll[page] = value;
                                    })
                                    .build();
                            }
                        })
                        .build();
                });
        });
}

} // namespace

const DslAppConfig& dslAppConfig() {
    static DslAppConfig config = DslAppConfig{}
        .title("EUI Gallery")
        .pageId("gallery")
        .clearColor({0.07f, 0.08f, 0.10f, 1.0f})
        .windowSize(1440, 1100)
        .fps(galleryFrameRateLimit());
    config.fps(galleryFrameRateLimit());
    return config;
}

void compose(core::dsl::Ui& ui, const core::dsl::Screen& screen) {
    const float contentWidth = std::max(0.0f, screen.width - kSidebarWidth);

    ui.row("root")
        .size(screen.width, screen.height)
        .content([&] {
            composeSidebar(ui, screen.height);
            composeContent(ui, contentWidth, screen.height);
        });

    components::dialog(ui, "feedback.dialog")
        .theme(themeColors())
        .screen(screen.width, screen.height)
        .size(430.0f, 228.0f)
        .open(sampleDialogOpen)
        .title("Dialog Component")
        .message("A modal surface for focused confirmation. It uses the same theme tokens, buttons and dirty-region rendering path as the rest of the gallery.")
        .primaryText("Confirm")
        .secondaryText("Cancel")
        .onPrimary([] {
            sampleDialogOpen = false;
            sampleToastVisible = true;
            sampleFeedback = "Dialog confirmed";
        })
        .onSecondary([] {
            sampleDialogOpen = false;
            sampleFeedback = "Dialog cancelled";
        })
        .onClose([] {
            sampleDialogOpen = false;
            sampleFeedback = "Dialog closed";
        })
        .build();

    components::contextMenu(ui, "feedback.context")
        .theme(themeColors())
        .screen(screen.width, screen.height)
        .position(sampleContextMenuX, sampleContextMenuY)
        .items({"Inspect", "Duplicate", "Copy Token", "Dismiss"})
        .open(sampleContextMenuOpen)
        .onSelect([](int index) {
            sampleContextMenuOpen = false;
            sampleToastVisible = true;
            if (index == 0) {
                sampleFeedback = "Inspect selected";
            } else if (index == 1) {
                sampleFeedback = "Duplicate selected";
            } else if (index == 2) {
                sampleFeedback = "Copy Token selected";
            } else {
                sampleFeedback = "Context menu dismissed";
                sampleToastVisible = false;
            }
        })
        .onDismiss([] {
            sampleContextMenuOpen = false;
            sampleFeedback = "Context menu dismissed";
        })
        .build();

    components::datepicker(ui, "feedback.datepicker")
        .theme(themeColors())
        .screen(screen.width, screen.height)
        .size(420.0f, 270.0f)
        .date(sampleYear, sampleMonth, sampleDay)
        .open(sampleDateOpen)
        .transition(pageTransition())
        .z(1200)
        .onOpenChange([](bool open) {
            sampleDateOpen = open;
        })
        .onChange([](int year, int month, int day) {
            sampleYear = year;
            sampleMonth = month;
            sampleDay = day;
            sampleFeedback = "Date changed";
        })
        .build();

    components::timepicker(ui, "feedback.timepicker")
        .theme(themeColors())
        .screen(screen.width, screen.height)
        .size(330.0f, 264.0f)
        .time(sampleHour, sampleMinute)
        .minuteStep(5)
        .open(sampleTimeOpen)
        .transition(pageTransition())
        .z(1200)
        .onOpenChange([](bool open) {
            sampleTimeOpen = open;
        })
        .onChange([](int hour, int minute) {
            sampleHour = hour;
            sampleMinute = minute;
            sampleFeedback = "Time changed";
        })
        .build();

    components::colorpicker(ui, "feedback.colorpicker")
        .theme(themeColors())
        .screen(screen.width, screen.height)
        .size(420.0f, 320.0f)
        .value(sampleColor)
        .open(sampleColorOpen)
        .transition(pageTransition())
        .z(1200)
        .onOpenChange([](bool open) {
            sampleColorOpen = open;
        })
        .onChange([](core::Color color) {
            sampleColor = color;
            sampleFeedback = "Color changed";
        })
        .build();

    components::toast(ui, "feedback.toast")
        .theme(themeColors())
        .screen(screen.width, screen.height)
        .visible(sampleToastVisible)
        .duration(3.0f)
        .title("Gallery Feedback")
        .message(sampleFeedback)
        .onAutoDismiss([] {
            sampleToastVisible = false;
            sampleFeedback = "Ready";
        })
        .onDismiss([] {
            sampleToastVisible = false;
            sampleFeedback = "Toast dismissed";
        })
        .build();
}

} // namespace app
