#include "eui_neo.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace app {

namespace {

bool checked = true;
bool switchOn = true;
bool radioA = true;
bool optionDense = false;
bool optionMotion = true;
bool optionNetwork = true;
bool optionNight = true;
bool optionGlass = false;
bool modalOpen = false;
bool toastVisible = false;
bool contextMenuOpen = false;
bool dateOpen = false;
bool timeOpen = false;
bool colorOpen = false;
int navPage = 0;
int segment = 1;
int tab = 0;
int dropdown = 1;
int year = 2026;
int month = 7;
int day = 6;
int hour = 16;
int minute = 30;
long long stepperValue = 42;
bool animMoved = false;
bool animRotated = false;
bool animFaded = false;
bool animScaled = false;
bool animRounded = false;
bool animGlowing = false;
bool animFlipX = false;
bool animFlipY = false;
bool animDepth = false;
std::string singleInput = "Tap to edit 😀";
std::string multiInput = "Large touch targets 🚀\nAndroid IME test\nScroll the page";
eui::Signal<float> basicScroll{0.0f};
eui::Signal<float> styleScroll{0.0f};
eui::Signal<float> animationScroll{0.0f};
eui::Signal<float> settingsScroll{0.0f};
eui::Signal<float> virtualListScroll{0.0f};
eui::Signal<float> sliderValue{0.58f};
eui::Signal<float> animationSpeed{0.58f};
eui::Signal<eui::Color> pickedColor{components::theme::defaultPrimary()};
eui::Signal<eui::Color> sampleColor{components::theme::defaultPrimary()};
eui::Signal<bool> dropdownOpen{false};
float contextMenuX = 120.0f;
float contextMenuY = 240.0f;
float carouselPosition = 0.0f;
std::string feedback = "Ready";

components::theme::ThemeColorTokens theme() {
    auto tokens = optionNight ? components::theme::dark() : components::theme::light();
    tokens.primary = pickedColor.get();
    return tokens;
}

float animationDurationScale() {
    const float speed = std::clamp(animationSpeed.get(), 0.0f, 1.0f);
    return optionMotion ? 1.85f - speed * 1.35f : 0.0f;
}

void syncGlobalMotion() {
    core::setGlobalTransitionDurationScale(animationDurationScale());
}

float motionDuration(float baseDuration) {
    return optionMotion ? baseDuration : 0.0f;
}

eui::Transition motion(float baseDuration, eui::Ease ease = eui::Ease::OutCubic) {
    return eui::Transition::make(motionDuration(baseDuration), ease);
}

eui::Transition motion() {
    return motion(0.18f, eui::Ease::OutCubic);
}

eui::Color alpha(eui::Color color, float value) {
    color.a = std::clamp(value, 0.0f, 1.0f);
    return color;
}

eui::Color textPrimary() {
    return components::theme::pageVisuals(theme()).titleColor;
}

eui::Color textMuted() {
    return components::theme::pageVisuals(theme()).subtitleColor;
}

eui::Color bodyText() {
    return components::theme::pageVisuals(theme()).bodyColor;
}

eui::Color transparent() {
    return {0.0f, 0.0f, 0.0f, 0.0f};
}

float desktopRadius(float androidHeight, float desktopHeight, float sourceRadius) {
    return desktopHeight > 0.0f ? androidHeight * sourceRadius / desktopHeight : sourceRadius;
}

std::string percentText(float value) {
    char buffer[16] = {};
    std::snprintf(buffer, sizeof(buffer), "%.0f%%", std::clamp(value, 0.0f, 1.0f) * 100.0f);
    return buffer;
}

std::string dropdownText() {
    switch (dropdown) {
    case 0: return "Alpha";
    case 1: return "Beta";
    case 2: return "Gamma";
    default: return "Choose";
    }
}

std::string dateText() {
    char buffer[16] = {};
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", year, month, day);
    return buffer;
}

std::string timeText() {
    char buffer[8] = {};
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d", hour, minute);
    return buffer;
}

std::string colorHex(eui::Color color) {
    const auto channel = [](float value) {
        return std::clamp(static_cast<int>(value * 255.0f + 0.5f), 0, 255);
    };
    char buffer[10] = {};
    std::snprintf(buffer, sizeof(buffer), "#%02X%02X%02X",
                  channel(color.r), channel(color.g), channel(color.b));
    return buffer;
}

const char* markdownSample() {
    return R"(# Markdown card

Common syntax rendered inside the Style page.

## Inline

Use **strong**, *emphasis*, `inline code`, [links](https://example.com), ~~deleted text~~, $a^2 + b^2$, and [[Wiki Target]].

## Lists

- Unordered item
- Wrapped item with a longer sentence to check line wrapping in a constrained mobile card.

1. Ordered item
2. Second ordered item

> Markdown composes into normal EUI primitives.

```cpp
components::markdown(ui, "style.markdown")
    .theme(themeColors())
    .width(contentWidth)
    .markdown(source)
    .build();
```
)";
}

std::string jsonStringValue(const std::string& json, const std::string& key) {
    const std::string token = "\"" + key + "\":\"";
    const std::size_t begin = json.find(token);
    if (begin == std::string::npos) {
        return {};
    }
    const std::size_t valueBegin = begin + token.size();
    std::string value;
    bool escaping = false;
    for (std::size_t i = valueBegin; i < json.size(); ++i) {
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
    if (!optionNetwork) {
        return "Bing network sample is disabled in Settings.";
    }
    const std::string key = "android.gallery.bing.text";
    eui::network::requestText(key, "https://www.bing.com/HPImageArchive.aspx?format=js&n=1&idx=0&mkt=zh-CN");
    const eui::network::TextResult result = eui::network::textResult(key);
    if (!result.ready) {
        return "Loading Bing API text...";
    }
    if (!result.ok) {
        return "Network text request failed.";
    }
    const std::string copyright = jsonStringValue(result.body, "copyright");
    return copyright.empty() ? "Bing API returned text data." : copyright;
}

void title(eui::Ui& ui, const std::string& id, const std::string& text, const std::string& sub, float width) {
    ui.stack(id)
        .size(width, 122.0f)
        .content([&] {
            ui.text(id + ".title")
                .position(0.0f, 0.0f)
                .size(width, 44.0f)
                .text(text)
                .fontSize(34.0f)
                .lineHeight(42.0f)
                .color(textPrimary())
                .build();
            ui.text(id + ".sub")
                .position(0.0f, 54.0f)
                .size(width, 68.0f)
                .text(sub)
                .fontSize(22.0f)
                .lineHeight(30.0f)
                .wrap(true)
                .color(textMuted())
                .build();
        })
        .build();
}

void section(eui::Ui& ui, const std::string& id, const std::string& text, float width) {
    ui.text(id)
        .size(width, 44.0f)
        .text(text)
        .fontSize(28.0f)
        .lineHeight(36.0f)
        .color(textPrimary())
        .build();
}

void pill(eui::Ui& ui, const std::string& id, const std::string& label, eui::Color color, float width) {
    ui.stack(id)
        .size(width, 76.0f)
        .content([&] {
            ui.rect(id + ".bg")
                .size(width, 76.0f)
                .color(alpha(color, 0.22f))
                .radius(18.0f)
                .border(1.0f, alpha(color, 0.56f))
                .build();
            ui.text(id + ".label")
                .size(width, 76.0f)
                .text(label)
                .fontSize(28.0f)
                .lineHeight(34.0f)
                .color(textPrimary())
                .horizontalAlign(eui::HorizontalAlign::Center)
                .verticalAlign(eui::VerticalAlign::Center)
                .build();
        })
        .build();
}

void rowText(eui::Ui& ui, const std::string& id, const std::string& left, const std::string& right, float width) {
    const auto tokens = theme();
    ui.stack(id)
        .size(width, 82.0f)
        .content([&] {
            ui.rect(id + ".bg")
                .size(width, 82.0f)
                .radius(18.0f)
                .color(alpha(tokens.surfaceHover, 0.62f))
                .border(1.0f, alpha(tokens.primary, 0.16f))
                .build();
            ui.text(id + ".left")
                .position(24.0f, 0.0f)
                .size(width * 0.54f, 82.0f)
                .text(left)
                .fontSize(30.0f)
                .lineHeight(36.0f)
                .color(textPrimary())
                .verticalAlign(eui::VerticalAlign::Center)
                .build();
            ui.text(id + ".right")
                .position(width * 0.58f, 0.0f)
                .size(std::max(0.0f, width * 0.42f - 24.0f), 82.0f)
                .text(right)
                .fontSize(28.0f)
                .lineHeight(34.0f)
                .color(textMuted())
                .horizontalAlign(eui::HorizontalAlign::Right)
                .verticalAlign(eui::VerticalAlign::Center)
                .build();
        })
        .build();
}

void barChart(eui::Ui& ui, const std::string& id, const std::string& label, const std::vector<float>& values, eui::Color color, float width) {
    const auto tokens = theme();
    const float height = 286.0f;
    ui.stack(id)
        .size(width, height)
        .content([&] {
            ui.rect(id + ".bg")
                .size(width, height)
                .radius(20.0f)
                .color(alpha(tokens.surfaceHover, 0.48f))
                .border(1.0f, alpha(color, 0.32f))
                .build();
            ui.text(id + ".title")
                .position(26.0f, 18.0f)
                .size(width - 52.0f, 48.0f)
                .text(label)
                .fontSize(34.0f)
                .lineHeight(40.0f)
                .color(textPrimary())
                .build();
            const float plotX = 30.0f;
            const float plotY = 86.0f;
            const float plotW = width - 60.0f;
            const float plotH = 164.0f;
            const float gap = 14.0f;
            const float barW = (plotW - gap * (static_cast<float>(values.size()) - 1.0f)) /
                               static_cast<float>(values.size());
            for (std::size_t i = 0; i < values.size(); ++i) {
                const float value = std::clamp(values[i], 0.0f, 1.0f);
                const float h = std::max(16.0f, plotH * value);
                ui.rect(id + ".bar." + std::to_string(i))
                    .position(plotX + static_cast<float>(i) * (barW + gap), plotY + plotH - h)
                    .size(barW, h)
                    .radius(12.0f)
                    .color(alpha(color, 0.74f))
                    .build();
            }
        })
        .build();
}

void composeButtons(eui::Ui& ui, float width) {
    const auto tokens = theme();
    const float gap = 18.0f;
    const float columns = width < 520.0f ? 2.0f : 3.0f;
    const float w = (width - gap * (columns - 1.0f)) / columns;
    const float buttonHeight = 88.0f;
    const float buttonRadius = desktopRadius(buttonHeight, 54.0f, 16.0f);
    ui.flow("basic.buttons")
        .width(width)
        .height(eui::SizeValue::wrapContent())
        .gap(gap)
        .lineGap(gap)
        .content([&] {
            components::button(ui, "button.primary")
                .theme(tokens, true)
                .size(w, buttonHeight)
                .radius(buttonRadius)
                .icon(0xF00C)
                .iconSize(27.0f)
                .text("Filled")
                .fontSize(24.0f)
                .onClick([] { feedback = "Filled clicked"; })
                .transition(motion())
                .build();
            components::button(ui, "button.secondary")
                .theme(tokens, false)
                .size(w, buttonHeight)
                .radius(buttonRadius)
                .icon(0xF0C8)
                .iconSize(26.0f)
                .text("Outline")
                .fontSize(24.0f)
                .transition(motion())
                .build();
            components::button(ui, "button.outline")
                .size(w, buttonHeight)
                .radius(buttonRadius)
                .icon(0xF1FC)
                .iconSize(26.0f)
                .text("Ghost")
                .fontSize(24.0f)
                .colors({0, 0, 0, 0}, alpha(tokens.primary, 0.14f), alpha(tokens.primary, 0.22f))
                .textColor(tokens.primary)
                .iconColor(tokens.primary)
                .border(0.0f, transparent())
                .shadow(0.0f, 0.0f, 0.0f, transparent())
                .transition(motion())
                .build();
        })
        .build();
}

void composeInputs(eui::Ui& ui, float width) {
    const auto tokens = theme();
    components::input(ui, "input.single")
        .theme(tokens)
        .size(width, 102.0f)
        .radius(22.0f)
        .value(singleInput)
        .placeholder("Single line input")
        .fontSize(36.0f)
        .inset(26.0f)
        .onChange([](const std::string& value) { singleInput = value; })
        .build();

    components::input(ui, "input.multi")
        .theme(tokens)
        .size(width, 210.0f)
        .radius(22.0f)
        .value(multiInput)
        .placeholder("Multiline input")
        .multiline(true)
        .fontSize(34.0f)
        .inset(26.0f)
        .onChange([](const std::string& value) { multiInput = value; })
        .build();
}

void composeSelection(eui::Ui& ui, float width) {
    const auto tokens = theme();
    const float gap = 18.0f;
    const float half = (width - gap) * 0.5f;
    ui.row("selection.row")
        .size(width, 188.0f)
        .gap(gap)
        .content([&] {
            ui.column("selection.left")
                .size(half, 188.0f)
                .gap(22.0f)
                .content([&] {
                    components::checkbox(ui, "selection.checkbox")
                        .theme(tokens)
                        .size(half, 72.0f)
                        .boxSize(46.0f)
                        .fontSize(32.0f)
                        .checked(checked)
                        .text("Checkbox")
                        .onChange([](bool value) { checked = value; })
                        .transition(motion())
                        .build();
                    components::toggleSwitch(ui, "selection.switch")
                        .theme(tokens)
                        .size(half, 72.0f)
                        .trackSize(88.0f, 48.0f)
                        .fontSize(32.0f)
                        .checked(switchOn)
                        .text("Switch")
                        .onChange([](bool value) { switchOn = value; })
                        .transition(motion())
                        .build();
                })
                .build();
            ui.column("selection.right")
                .size(half, 188.0f)
                .gap(22.0f)
                .content([&] {
                    components::radio(ui, "selection.radio.a")
                        .theme(tokens)
                        .size(half, 72.0f)
                        .dotSize(46.0f)
                        .fontSize(32.0f)
                        .selected(radioA)
                        .text("Radio A")
                        .onChange([](bool selected) { if (selected) { radioA = true; } })
                        .transition(motion())
                        .build();
                    components::radio(ui, "selection.radio.b")
                        .theme(tokens)
                        .size(half, 72.0f)
                        .dotSize(46.0f)
                        .fontSize(32.0f)
                        .selected(!radioA)
                        .text("Radio B")
                        .onChange([](bool selected) { if (selected) { radioA = false; } })
                        .transition(motion())
                        .build();
                })
                .build();
        })
        .build();
}

void composeChoice(eui::Ui& ui, float width) {
    const auto tokens = theme();
    components::segmented(ui, "choice.segmented")
        .theme(tokens)
        .size(width, 88.0f)
        .radius(desktopRadius(88.0f, 38.0f, 9.0f))
        .items({"Small", "Medium", "Large"})
        .selected(segment)
        .fontSize(30.0f)
        .onChange([](int value) { segment = value; })
        .transition(motion())
        .build();

    components::tabs(ui, "choice.tabs")
        .theme(tokens)
        .size(width, 88.0f)
        .items({"Overview", "Details", "Logs"})
        .selected(tab)
        .fontSize(30.0f)
        .onChange([](int value) { tab = value; })
        .transition(motion())
        .build();

    const float gap = 18.0f;
    const float half = (width - gap) * 0.5f;
    ui.row("choice.stepdrop")
        .size(width, 366.0f)
        .gap(gap)
        .content([&] {
            components::stepper(ui, "choice.stepper")
                .theme(tokens)
                .size(half, 96.0f)
                .radius(desktopRadius(96.0f, 40.0f, 12.0f))
                .value(stepperValue)
                .min(0)
                .max(99)
                .fontSize(34.0f)
                .onChange([](long long value) { stepperValue = value; })
                .transition(motion())
                .build();
            components::dropdown(ui, "choice.dropdown")
                .theme(tokens)
                .size(half, 96.0f)
                .radius(desktopRadius(96.0f, 44.0f, 12.0f))
                .items({"Alpha", "Beta", "Gamma"})
                .selected(dropdown)
                .open(dropdownOpen.get())
                .fontSize(20.0f)
                .itemFontSize(20.0f)
                .chevronSize(17.0f)
                .itemHeight(82.0f)
                .onOpenChange([](bool open) { dropdownOpen.set(open); })
                .onChange([](int index) {
                    dropdown = index;
                    dropdownOpen.set(false);
                    feedback = "Dropdown changed";
                })
                .transition(motion())
                .zIndex(900)
                .build();
        })
        .build();
}

void composeFeedback(eui::Ui& ui, float width) {
    const auto tokens = theme();
    const float gap = 16.0f;
    const float w = (width - gap) * 0.5f;
    const float buttonHeight = 92.0f;
    const float buttonRadius = desktopRadius(buttonHeight, 54.0f, 12.0f);
    ui.flow("feedback.buttons")
        .width(width)
        .height(eui::SizeValue::wrapContent())
        .gap(gap)
        .lineGap(gap)
        .content([&] {
            components::button(ui, "feedback.dialog")
                .theme(tokens, false)
                .size(w, buttonHeight)
                .radius(buttonRadius)
                .icon(0xF2D0)
                .iconSize(30.0f)
                .text("Dialog")
                .fontSize(30.0f)
                .onClick([] {
                    modalOpen = true;
                    feedback = "Dialog opened";
                })
                .transition(motion())
                .build();
            components::button(ui, "feedback.toast")
                .theme(tokens, false)
                .size(w, buttonHeight)
                .radius(buttonRadius)
                .icon(0xF0F3)
                .iconSize(30.0f)
                .text("Toast")
                .fontSize(30.0f)
                .onClick([] {
                    toastVisible = true;
                    feedback = "Toast queued";
                })
                .transition(motion())
                .build();
            components::button(ui, "feedback.menu")
                .theme(tokens, false)
                .size(w, buttonHeight)
                .radius(buttonRadius)
                .icon(0xF0C9)
                .iconSize(30.0f)
                .text("Context")
                .fontSize(30.0f)
                .onClick([] { feedback = "Long press Context"; })
                .onContextMenu([](const eui::PointerEvent& event, const eui::Rect&) {
                    contextMenuX = static_cast<float>(event.x);
                    contextMenuY = static_cast<float>(event.y);
                    contextMenuOpen = true;
                    feedback = "Context menu opened";
                })
                .transition(motion())
                .build();
            components::button(ui, "feedback.window")
                .theme(tokens, false)
                .size(w, buttonHeight)
                .radius(buttonRadius)
                .icon(0xF24D)
                .iconSize(30.0f)
                .text("Window")
                .fontSize(30.0f)
                .onClick([] { feedback = "Window demo is desktop-only here"; })
                .transition(motion())
                .build();
        })
        .build();
    rowText(ui, "feedback.state", "Feedback", feedback, width);
}

void composePickers(eui::Ui& ui, float width) {
    const auto tokens = theme();
    const float gap = 16.0f;
    const float w = (width - gap * 2.0f) / 3.0f;
    const float buttonHeight = 92.0f;
    const float buttonRadius = desktopRadius(buttonHeight, 44.0f, 12.0f);
    ui.flow("pickers.buttons")
        .width(width)
        .height(eui::SizeValue::wrapContent())
        .gap(gap)
        .lineGap(gap)
        .content([&] {
            components::button(ui, "picker.date")
                .theme(tokens, false)
                .size(w, buttonHeight)
                .radius(buttonRadius)
                .icon(0xF073)
                .iconSize(28.0f)
                .text(dateText())
                .fontSize(28.0f)
                .onClick([] {
                    dateOpen = true;
                    timeOpen = false;
                    colorOpen = false;
                    feedback = "Date picker opened";
                })
                .transition(motion())
                .build();
            components::button(ui, "picker.time")
                .theme(tokens, false)
                .size(w, buttonHeight)
                .radius(buttonRadius)
                .icon(0xF017)
                .iconSize(28.0f)
                .text(timeText())
                .fontSize(28.0f)
                .onClick([] {
                    timeOpen = true;
                    dateOpen = false;
                    colorOpen = false;
                    feedback = "Time picker opened";
                })
                .transition(motion())
                .build();
            components::button(ui, "picker.color")
                .theme(tokens, true)
                .size(w, buttonHeight)
                .radius(buttonRadius)
                .icon(0xF53F)
                .iconSize(28.0f)
                .text(colorHex(pickedColor.get()))
                .fontSize(28.0f)
                .onClick([] {
                    colorOpen = true;
                    dateOpen = false;
                    timeOpen = false;
                    feedback = "Color picker opened";
                })
                .transition(motion())
                .build();
        })
        .build();
}

void propertyCard(eui::Ui& ui, const std::string& id, const std::string& label, const std::string& note,
                  eui::Color color, const std::string& kind, float width) {
    const auto tokens = theme();
    ui.stack(id)
        .size(width, 178.0f)
        .visualStateFrom(id + ".bg", 0.95f)
        .content([&] {
            if (kind == "blur") {
                ui.rect(id + ".circle.primary")
                    .position(width * 0.14f, 24.0f)
                    .size(66.0f, 66.0f)
                    .color(alpha(tokens.primary, 0.95f))
                    .radius(33.0f)
                    .build();
                ui.rect(id + ".circle.warm")
                    .position(width * 0.48f, 80.0f)
                    .size(58.0f, 58.0f)
                    .color({1.0f, 0.54f, 0.18f, 0.92f})
                    .radius(29.0f)
                    .build();
                ui.rect(id + ".circle.cool")
                    .position(width * 0.66f, 22.0f)
                    .size(48.0f, 48.0f)
                    .color({0.16f, 0.82f, 0.72f, 0.90f})
                    .radius(24.0f)
                    .build();
            }

            auto rect = ui.rect(id + ".bg")
                .size(width, 178.0f)
                .radius(20.0f)
                .states(color, alpha(color, 0.92f), alpha(color, 0.82f))
                .transition(motion());
            if (kind == "border") {
                rect.border(3.0f, tokens.primary);
            } else if (kind == "shadow") {
                rect.shadow(34.0f, 0.0f, 14.0f,
                            optionNight ? eui::Color{0, 0, 0, 0.34f} : eui::Color{0.08f, 0.12f, 0.20f, 0.18f});
            } else if (kind == "blur") {
                rect.opacity(optionGlass ? 1.0f : 0.82f)
                    .blur(optionGlass ? 18.0f : 0.0f)
                    .border(1.0f, optionGlass ? alpha(textPrimary(), 0.35f) : alpha(tokens.border, 0.70f));
            } else if (kind == "rotate") {
                rect.rotate(0.08f).transformOrigin(0.5f, 0.5f);
            }
            rect.build();

            ui.text(id + ".label")
                .position(0.0f, 58.0f)
                .size(width, 42.0f)
                .text(label)
                .fontSize(30.0f)
                .lineHeight(36.0f)
                .color(textPrimary())
                .horizontalAlign(eui::HorizontalAlign::Center)
                .build();
            ui.text(id + ".note")
                .position(18.0f, 110.0f)
                .size(width - 36.0f, 44.0f)
                .text(note)
                .fontSize(23.0f)
                .lineHeight(28.0f)
                .color(textMuted())
                .horizontalAlign(eui::HorizontalAlign::Center)
                .wrap(true)
                .build();
        })
        .build();
}

void composePrimitiveProperties(eui::Ui& ui, float width) {
    const float gap = 16.0f;
    const float w = (width - gap) * 0.5f;
    ui.flow("properties.grid")
        .width(width)
        .height(eui::SizeValue::wrapContent())
        .gap(gap)
        .lineGap(gap)
        .content([&] {
            propertyCard(ui, "prop.color", "Color", "hover + press", {0.22f, 0.48f, 0.82f, 1.0f}, "color", w);
            propertyCard(ui, "prop.border", "Border", "animated edge", alpha(theme().surfaceHover, 0.90f), "border", w);
            propertyCard(ui, "prop.shadow", "Shadow", "elevation", theme().surface, "shadow", w);
            propertyCard(ui, "prop.opacity", "Opacity", "transparent fill", {0.86f, 0.38f, 0.52f, 0.58f}, "color", w);
            propertyCard(ui, "prop.blur", "Blur", "glass card", {0.42f, 0.78f, 0.92f, 0.30f}, "blur", w);
            propertyCard(ui, "prop.rotate", "Rotate", "transform", {0.48f, 0.64f, 0.36f, 1.0f}, "rotate", w);
        })
        .build();
}

void composeBasicPage(eui::Ui& ui, float width, float height) {
    (void)height;
    const auto tokens = theme();
    ui.column("page.basic")
        .width(width)
        .height(eui::SizeValue::wrapContent())
        .gap(28.0f)
        .content([&] {
            title(ui, "basic.hero", "Basic Controls", "Buttons, inputs, selection controls, ranges, tabs and large mobile data views.", width);
            section(ui, "section.buttons", "Buttons", width);
            composeButtons(ui, width);
            section(ui, "section.inputs", "Inputs", width);
            composeInputs(ui, width);
            section(ui, "section.selection", "Selection", width);
            composeSelection(ui, width);
            section(ui, "section.ranges", "Progress and Slider", width);
            components::progress(ui, "range.progress")
                .theme(tokens)
                .size(width, 30.0f)
                .value(sliderValue.get())
                .transition(motion())
                .build();
            components::slider(ui, "range.slider")
                .theme(tokens)
                .size(width, 88.0f)
                .bind(sliderValue)
                .transition(motion())
                .build();
            rowText(ui, "range.value", "Slider value", percentText(sliderValue.get()), width);
            section(ui, "section.choice", "Tabs and Choices", width);
            composeChoice(ui, width);
            section(ui, "section.feedback", "Feedback Components", width);
            composeFeedback(ui, width);
            section(ui, "section.pickers", "Pickers", width);
            composePickers(ui, width);
            section(ui, "section.data", "Selection and Data", width);
            components::dataTable(ui, "control.table")
                .theme(tokens)
                .size(width, 390.0f)
                .headerFontSize(18.0f)
                .fontSize(18.0f)
                .columns({"Name", "Status", "Owner"})
                .rows({
                    {"EUI Core", "Active", "Sudo"},
                    {"Gallery", "Review", "Design"},
                    {"Docs", "Draft", "DevRel"},
                    {"Runtime", "Stable", "Engine"}
                })
                .transition(motion())
                .build();
            section(ui, "section.charts", "Charts", width);
            components::lineChart(ui, "control.chart.line")
                .theme(tokens)
                .size(width, 360.0f)
                .title("LineChart")
                .titleFontSize(24.0f)
                .labelFontSize(17.0f)
                .values({0.22f, 0.30f, 0.20f, 0.55f, 0.42f, 0.86f})
                .labels({"Jan", "Feb", "Mar", "Apr", "May", "Jun"})
                .style(components::LineStyle::Linear)
                .transition(motion())
                .build();
            ui.row("control.charts.row")
                .size(width, 360.0f)
                .gap(18.0f)
                .content([&] {
                    const float chartWidth = (width - 18.0f) * 0.5f;
                    components::barChart(ui, "control.chart.bar")
                        .theme(tokens)
                        .size(chartWidth, 360.0f)
                        .title("BarChart")
                        .titleFontSize(23.0f)
                        .labelFontSize(17.0f)
                        .values({0.92f, 0.36f, 0.68f, 0.52f})
                        .labels({"D1", "D2", "D3", "D4"})
                        .transition(motion())
                        .build();
                    components::pieChart(ui, "control.chart.pie")
                        .theme(tokens)
                        .size(chartWidth, 360.0f)
                        .title("PieChart")
                        .titleFontSize(23.0f)
                        .values({0.42f, 0.24f, 0.18f, 0.16f})
                        .labels({"Blue", "Green", "Orange", "Pink"})
                        .transition(motion())
                        .build();
                })
                .build();
            section(ui, "section.properties", "Primitive Properties", width);
            composePrimitiveProperties(ui, width);
            ui.rect("basic.bottom.space").size(width, 80.0f).color({0, 0, 0, 0}).build();
        })
        .build();
}

void swatch(eui::Ui& ui, const std::string& id, const std::string& name, eui::Color color, float width) {
    const auto tokens = theme();
    ui.stack(id)
        .size(width, 118.0f)
        .content([&] {
            ui.rect(id + ".bg")
                .size(width, 118.0f)
                .radius(18.0f)
                .color(alpha(tokens.surfaceHover, 0.55f))
                .border(1.0f, alpha(tokens.primary, 0.18f))
                .build();
            ui.rect(id + ".hit")
                .size(width, 118.0f)
                .states(transparent(), alpha(color, 0.10f), alpha(color, 0.18f))
                .radius(18.0f)
                .onClick([color] {
                    pickedColor.set(color);
                    feedback = "Theme color changed";
                })
                .build();
            ui.rect(id + ".color")
                .position(18.0f, 16.0f)
                .size(width - 36.0f, 36.0f)
                .radius(10.0f)
                .color(color)
                .build();
            ui.text(id + ".name")
                .position(18.0f, 62.0f)
                .size(width - 36.0f, 42.0f)
                .text(name)
                .fontSize(27.0f)
                .lineHeight(34.0f)
                .color(textPrimary())
                .horizontalAlign(eui::HorizontalAlign::Center)
                .build();
        })
        .build();
}

void composeMarkdownCard(eui::Ui& ui, float width) {
    const auto tokens = theme();
    const float inset = 28.0f;
    const std::string source = markdownSample();
    components::MarkdownStyle style(tokens);
    style.bodySize = 26.0f;
    style.bodyLineHeight = 36.0f;
    style.h1Size = 38.0f;
    style.h2Size = 32.0f;
    style.h3Size = 28.0f;
    style.codeSize = 23.0f;
    style.blockGap = 18.0f;
    style.listIndent = 30.0f;
    style.codePadding = 18.0f;
    style.quotePadding = 18.0f;
    style.radius = 12.0f;
    const float markdownWidth = std::max(0.0f, width - inset * 2.0f);
    const float markdownHeight = components::MarkdownBuilder::estimateHeight(source, markdownWidth, style) + 12.0f;
    const float cardHeight = markdownHeight + inset * 2.0f + 56.0f;

    ui.stack("style.markdown.card")
        .size(width, cardHeight)
        .content([&] {
            ui.rect("style.markdown.card.bg")
                .size(width, cardHeight)
                .color(alpha(tokens.surfaceHover, 0.54f))
                .radius(22.0f)
                .border(1.0f, alpha(tokens.border, 0.72f))
                .shadow(optionNight ? 20.0f : 12.0f, 0.0f, 8.0f,
                        optionNight ? eui::Color{0, 0, 0, 0.22f} : eui::Color{0.08f, 0.12f, 0.20f, 0.10f})
                .build();
            ui.text("style.markdown.card.title")
                .position(inset, inset)
                .size(markdownWidth, 44.0f)
                .text("Markdown Preview")
                .fontSize(34.0f)
                .lineHeight(42.0f)
                .color(textPrimary())
                .build();
            components::markdown(ui, "style.markdown.preview")
                .position(inset, inset + 56.0f)
                .style(style)
                .width(markdownWidth)
                .height(markdownHeight)
                .markdown(source)
                .zIndex(1)
                .build();
        })
        .build();
}

void composeVirtualListCard(eui::Ui& ui, float width) {
    const auto tokens = theme();
    const float height = 460.0f;
    const float inset = 24.0f;
    ui.stack("style.virtual.card")
        .size(width, height)
        .content([&] {
            ui.rect("style.virtual.card.bg")
                .size(width, height)
                .radius(22.0f)
                .color(alpha(tokens.surfaceHover, 0.52f))
                .border(1.0f, alpha(tokens.border, 0.72f))
                .build();
            ui.text("style.virtual.title")
                .position(inset, 18.0f)
                .size(width - inset * 2.0f, 44.0f)
                .text("Virtual List")
                .fontSize(34.0f)
                .lineHeight(42.0f)
                .color(textPrimary())
                .build();
            components::virtualList(ui, "style.virtual.list")
                .position(inset, 76.0f)
                .size(std::max(0.0f, width - inset * 2.0f), height - 98.0f)
                .itemCount(10000)
                .rowHeight(78.0f)
                .bind(virtualListScroll)
                .step(110.0f)
                .overscanViewports(0.2f)
                .scrollbarWidth(9.0f)
                .scrollbarGap(8.0f)
                .theme(tokens)
                .row([](eui::Ui& rowUi, const std::string& rowId, std::int64_t index, float rowWidth, float rowHeight) {
                    const auto rowTokens = theme();
                    rowUi.rect(rowId + ".bg")
                        .position(0.0f, 6.0f)
                        .size(rowWidth, rowHeight - 12.0f)
                        .radius(16.0f)
                        .color(alpha(rowTokens.surface, index % 2 == 0 ? 0.54f : 0.34f))
                        .border(1.0f, alpha(rowTokens.border, 0.48f))
                        .build();
                    rowUi.text(rowId + ".title")
                        .position(20.0f, 12.0f)
                        .size(rowWidth * 0.52f, 30.0f)
                        .text("Virtual row " + std::to_string(index + 1))
                        .fontSize(27.0f)
                        .lineHeight(32.0f)
                        .color(textPrimary())
                        .build();
                    rowUi.text(rowId + ".meta")
                        .position(20.0f, 43.0f)
                        .size(rowWidth * 0.56f, 24.0f)
                        .text(index % 3 == 0 ? "recycled slot" : "stable layout")
                        .fontSize(22.0f)
                        .lineHeight(26.0f)
                        .color(textMuted())
                        .build();
                    rowUi.text(rowId + ".badge")
                        .position(rowWidth - 170.0f, 20.0f)
                        .size(146.0f, 34.0f)
                        .text(index % 2 == 0 ? "Active" : "Idle")
                        .fontSize(22.0f)
                        .lineHeight(28.0f)
                        .horizontalAlign(eui::HorizontalAlign::Center)
                        .verticalAlign(eui::VerticalAlign::Center)
                        .color(rowTokens.primary)
                        .build();
                })
                .transition(motion())
                .build();
        })
        .build();
}

void composeBingPanel(eui::Ui& ui, float width) {
    const auto tokens = theme();
    const float mediaHeight = 560.0f;
    const float apiHeight = 188.0f;
    const std::vector<components::CarouselItem> bingItems = {
        {"bing://daily?idx=0&mkt=zh-CN", "Bing Today", "zh-CN daily image"},
        {"bing://daily?idx=1&mkt=zh-CN", "Bing Yesterday", "previous daily image"},
        {"bing://daily?idx=2&mkt=zh-CN", "Two Days Ago", "archive image"},
        {"bing://daily?idx=3&mkt=zh-CN", "Earlier", "archive image"}
    };
    components::CarouselStyle carouselStyle(tokens);
    carouselStyle.background = tokens.surface;
    carouselStyle.border = alpha(tokens.border, 0.72f);
    carouselStyle.text = {1.0f, 1.0f, 1.0f, 0.96f};
    carouselStyle.mutedText = {1.0f, 1.0f, 1.0f, 0.72f};
    carouselStyle.activeIndicator = tokens.primary;
    carouselStyle.radius = 24.0f;

    components::carousel(ui, "bing.media.carousel")
        .size(width, mediaHeight)
        .items(bingItems)
        .index(carouselPosition)
        .cardWidthRatio(0.86f)
        .overlap(0.22f)
        .parallax(28.0f)
        .style(carouselStyle)
        .transition(motion())
        .onChange([](float next) {
            carouselPosition = std::clamp(next, 0.0f, 3.0f);
        })
        .build();

    ui.stack("bing.api")
        .size(width, apiHeight)
        .content([&] {
            ui.rect("bing.api.bg")
                .size(width, apiHeight)
                .color(alpha(tokens.surfaceHover, 0.52f))
                .radius(20.0f)
                .border(1.0f, alpha(tokens.border, 0.72f))
                .build();
            ui.text("bing.api.title")
                .position(24.0f, 22.0f)
                .size(width - 48.0f, 38.0f)
                .text("Bing API text")
                .fontSize(32.0f)
                .lineHeight(38.0f)
                .color(textPrimary())
                .build();
            ui.text("bing.api.text")
                .position(24.0f, 72.0f)
                .size(width - 48.0f, apiHeight - 92.0f)
                .text(bingApiText())
                .fontSize(26.0f)
                .lineHeight(34.0f)
                .maxWidth(width - 48.0f)
                .wrap(true)
                .color(textMuted())
                .build();
        })
        .build();
}

void composeStylePage(eui::Ui& ui, float width, float height) {
    (void)height;
    const auto tokens = theme();
    ui.column("page.style")
        .width(width)
        .height(eui::SizeValue::wrapContent())
        .gap(28.0f)
        .content([&] {
            title(ui, "style.hero", "Style + Bing", "Theme tokens, typography, visual samples and a mobile-sized Bing content panel.", width);
            section(ui, "section.colors", "Theme Tokens", width);
            ui.flow("style.swatches")
                .width(width)
                .height(eui::SizeValue::wrapContent())
                .gap(16.0f)
                .lineGap(16.0f)
                .content([&] {
                    const float w = (width - 16.0f) * 0.5f;
                    swatch(ui, "swatch.primary", "Primary", tokens.primary, w);
                    swatch(ui, "swatch.surface", "Surface", tokens.surface, w);
                    swatch(ui, "swatch.hover", "Hover", tokens.surfaceHover, w);
                    swatch(ui, "swatch.active", "Active", tokens.surfaceActive, w);
                })
                .build();

            section(ui, "section.type", "Typography", width);
            ui.text("type.display").size(width, 58.0f).text("Display 42").fontSize(42.0f).lineHeight(52.0f).color(textPrimary()).build();
            ui.text("type.heading").size(width, 48.0f).text("Heading 34").fontSize(34.0f).lineHeight(42.0f).color(textPrimary()).build();
            ui.text("type.body").size(width, 86.0f).text("Body text wraps with a readable Android line height. This page keeps text large enough for real phone testing.").fontSize(28.0f).lineHeight(36.0f).wrap(true).color(bodyText()).build();

            section(ui, "section.markdown", "Markdown Component", width);
            composeMarkdownCard(ui, width);
            section(ui, "section.virtual", "Virtual List", width);
            composeVirtualListCard(ui, width);
            section(ui, "section.bing", "Bing Panel", width);
            composeBingPanel(ui, width);
            barChart(ui, "style.chart", "Visual Density", {0.58f, 0.82f, 0.66f, 0.74f, 0.90f}, {0.16f, 0.78f, 0.56f, 1.0f}, width);
            ui.rect("style.bottom.space").size(width, 80.0f).color({0, 0, 0, 0}).build();
        })
        .build();
}

void animButton(eui::Ui& ui, const std::string& id, const std::string& label, bool active, eui::Color color, float width, const std::function<void()>& onClick) {
    const auto tokens = theme();
    const float buttonHeight = 82.0f;
    components::button(ui, id)
        .size(width, buttonHeight)
        .radius(desktopRadius(buttonHeight, 50.0f, 16.0f))
        .text(label)
        .fontSize(28.0f)
        .colors(active ? color : tokens.surfaceHover,
                active ? alpha(color, 0.86f) : alpha(tokens.surfaceActive, 0.90f),
                active ? alpha(color, 0.72f) : alpha(tokens.surfaceActive, 1.0f))
        .textColor(active || optionNight ? eui::Color{0.96f, 0.98f, 1.0f, 1.0f} : textPrimary())
        .border(1.0f, active ? alpha(color, 0.64f) : alpha(tokens.border, 0.70f))
        .shadow(active ? 18.0f : 8.0f, 0.0f, active ? 4.0f : 3.0f, optionNight ? eui::Color{0, 0, 0, 0.22f} : eui::Color{0.08f, 0.12f, 0.20f, 0.10f})
        .onClick(onClick)
        .transition(motion())
        .build();
}

void composeAnimationPage(eui::Ui& ui, float width, float height) {
    (void)height;
    const auto tokens = theme();
    const float gap = 16.0f;
    const float buttonWidth = (width - gap * 2.0f) / 3.0f;
    const float stageHeight = 520.0f;
    const float actorWidth = std::min(560.0f, width * 0.64f);
    const float actorHeight = 250.0f;
    const float actorScale = animScaled ? 1.16f : 1.0f;
    const float travel = std::max(0.0f, width - actorWidth * actorScale - 90.0f);

    ui.column("page.animation")
        .width(width)
        .height(eui::SizeValue::wrapContent())
        .gap(28.0f)
        .content([&] {
            title(ui, "animation.hero", "Animation", "Transitions, transforms, opacity, radius and shadow states from the original gallery, sized for touch.", width);

            ui.flow("animation.controls")
                .width(width)
                .height(eui::SizeValue::wrapContent())
                .gap(gap)
                .lineGap(gap)
                .content([&] {
                    animButton(ui, "anim.move", "Move", animMoved, tokens.primary, buttonWidth, [] { animMoved = !animMoved; });
                    animButton(ui, "anim.rotate", "Rotate", animRotated, {0.84f, 0.46f, 0.60f, 1.0f}, buttonWidth, [] { animRotated = !animRotated; });
                    animButton(ui, "anim.fade", "Fade", animFaded, {0.50f, 0.72f, 0.34f, 1.0f}, buttonWidth, [] { animFaded = !animFaded; });
                    animButton(ui, "anim.scale", "Scale", animScaled, {0.92f, 0.62f, 0.26f, 1.0f}, buttonWidth, [] { animScaled = !animScaled; });
                    animButton(ui, "anim.radius", "Radius", animRounded, {0.50f, 0.58f, 0.94f, 1.0f}, buttonWidth, [] { animRounded = !animRounded; });
                    animButton(ui, "anim.glow", "Glow", animGlowing, {0.28f, 0.76f, 0.72f, 1.0f}, buttonWidth, [] { animGlowing = !animGlowing; });
                    animButton(ui, "anim.flipx", "Flip X", animFlipX, {0.74f, 0.46f, 0.92f, 1.0f}, buttonWidth, [] { animFlipX = !animFlipX; });
                    animButton(ui, "anim.flipy", "Flip Y", animFlipY, {0.34f, 0.68f, 0.94f, 1.0f}, buttonWidth, [] { animFlipY = !animFlipY; });
                    animButton(ui, "anim.depth", "Depth", animDepth, {0.94f, 0.66f, 0.30f, 1.0f}, buttonWidth, [] { animDepth = !animDepth; });
                })
                .build();

            ui.stack("animation.stage")
                .size(width, stageHeight)
                .content([&] {
                    ui.rect("animation.stage.bg")
                        .size(width, stageHeight)
                        .radius(24.0f)
                        .color(alpha(tokens.surfaceHover, 0.44f))
                        .border(1.0f, alpha(tokens.border, 0.70f))
                        .build();
                    ui.rect("animation.track")
                        .position(38.0f, stageHeight - 52.0f)
                        .size(width - 76.0f, 4.0f)
                        .radius(2.0f)
                        .color(alpha(tokens.border, 0.55f))
                        .build();

                    ui.stack("animation.actor")
                        .position(42.0f, 110.0f)
                        .size(actorWidth, actorHeight)
                        .translate(animMoved ? travel : 0.0f, animMoved ? -18.0f : 0.0f)
                        .rotate(animRotated ? 0.34f : 0.0f)
                        .rotateX(animFlipX ? 0.82f : 0.0f)
                        .rotateY(animFlipY ? -0.72f : (animDepth ? 0.42f : 0.0f))
                        .translateZ(animDepth ? 72.0f : 0.0f)
                        .perspective(520.0f)
                        .scale(actorScale)
                        .transformOrigin(0.5f, 0.5f)
                        .transformedHitTest()
                        .opacity(animFaded ? 0.38f : 1.0f)
                        .transition(motion(0.42f, eui::Ease::OutBack))
                        .animate(eui::AnimProperty::Opacity | eui::AnimProperty::Transform)
                        .content([&] {
                            ui.rect("animation.actor.bg")
                                .size(actorWidth, actorHeight)
                                .radius(animRounded ? 48.0f : 22.0f)
                                .color(animMoved ? tokens.primary : eui::Color{0.50f, 0.58f, 0.94f, 1.0f})
                                .border(1.0f, alpha(tokens.primary, 0.44f))
                                .shadow(animGlowing ? 48.0f : 24.0f, 0.0f, animGlowing ? 0.0f : 10.0f,
                                        animGlowing ? alpha(eui::Color{0.28f, 0.76f, 0.72f, 1.0f}, optionNight ? 0.46f : 0.28f)
                                                    : eui::Color{0, 0, 0, optionNight ? 0.30f : 0.12f})
                                .transition(motion(0.42f, eui::Ease::OutBack))
                                .animate(eui::AnimProperty::Color | eui::AnimProperty::Radius | eui::AnimProperty::Shadow)
                                .build();
                            ui.image("animation.actor.icon")
                                .position(actorWidth - 92.0f, actorHeight - 92.0f)
                                .size(62.0f, 62.0f)
                                .source("assets/icon.png")
                                .cover()
                                .radius(14.0f)
                                .build();
                            ui.text("animation.actor.title")
                                .position(26.0f, 34.0f)
                                .size(actorWidth - 130.0f, 50.0f)
                                .text("Motion")
                                .fontSize(38.0f)
                                .lineHeight(46.0f)
                                .color({0.96f, 0.98f, 1.0f, 1.0f})
                                .build();
                            ui.text("animation.actor.note")
                                .position(26.0f, 94.0f)
                                .size(actorWidth - 130.0f, 118.0f)
                                .text(animFlipX ? "rotateX + perspective." :
                                      (animFlipY ? "rotateY + perspective." :
                                      (animDepth ? "translateZ + perspective." :
                                      "Frame, transform and visual properties animate together.")))
                                .fontSize(28.0f)
                                .lineHeight(36.0f)
                                .wrap(true)
                                .color({0.84f, 0.90f, 1.0f, 0.80f})
                                .build();
                        })
                        .build();
                })
                .build();

            rowText(ui, "animation.state.move", "Move", animMoved ? "On" : "Off", width);
            rowText(ui, "animation.state.visual", "Visual", animGlowing ? "Glow" : "Normal", width);
            rowText(ui, "animation.state.matrix", "Matrix", (animFlipX || animFlipY || animDepth) ? "3D active" : "Flat", width);
            ui.rect("animation.bottom.space").size(width, 80.0f).color(transparent()).build();
        })
        .build();
}

void settingRow(eui::Ui& ui, const std::string& id, const std::string& label, const std::string& note, bool value, float width, const std::function<void(bool)>& onChange) {
    const auto tokens = theme();
    ui.stack(id)
        .size(width, 112.0f)
        .content([&] {
            ui.rect(id + ".bg")
                .size(width, 112.0f)
                .radius(18.0f)
                .color(alpha(tokens.surfaceHover, 0.55f))
                .border(1.0f, alpha(tokens.primary, 0.16f))
                .build();
            ui.text(id + ".label")
                .position(24.0f, 16.0f)
                .size(width - 140.0f, 42.0f)
                .text(label)
                .fontSize(30.0f)
                .lineHeight(36.0f)
                .color(textPrimary())
                .build();
            ui.text(id + ".note")
                .position(24.0f, 58.0f)
                .size(width - 140.0f, 38.0f)
                .text(note)
                .fontSize(24.0f)
                .lineHeight(30.0f)
                .color(textMuted())
                .build();
            ui.stack(id + ".switch.wrap")
                .position(width - 112.0f, 32.0f)
                .size(88.0f, 50.0f)
                .content([&] {
                    components::toggleSwitch(ui, id + ".switch")
                        .theme(tokens)
                        .size(88.0f, 50.0f)
                        .trackSize(88.0f, 50.0f)
                        .checked(value)
                        .onChange(onChange)
                        .transition(motion())
                        .build();
                })
                .build();
        })
        .build();
}

void composeSettingsPage(eui::Ui& ui, float width, float height) {
    (void)height;
    const auto tokens = theme();
    ui.column("page.settings")
        .width(width)
        .height(eui::SizeValue::wrapContent())
        .gap(28.0f)
        .content([&] {
            title(ui, "settings.hero", "Settings + About", "Runtime options and project information in a phone-sized layout.", width);
            section(ui, "section.settings", "Settings", width);
            settingRow(ui, "setting.night", "Night mode", "Switch gallery between light and dark theme tokens.", optionNight, width, [](bool v) { optionNight = v; });
            settingRow(ui, "setting.glass", "Glass surfaces", "Show transparent panel examples in primitive cards.", optionGlass, width, [](bool v) { optionGlass = v; });
            settingRow(ui, "setting.motion", "Motion", "Enable animated transitions.", optionMotion, width, [](bool v) { optionMotion = v; });
            settingRow(ui, "setting.network", "Bing network", "Allow Bing panel data integration.", optionNetwork, width, [](bool v) { optionNetwork = v; });
            settingRow(ui, "setting.dense", "Dense layout", "Use tighter spacing in future pages.", optionDense, width, [](bool v) { optionDense = v; });
            section(ui, "section.theme.pick", "Theme Color", width);
            ui.flow("settings.colors")
                .width(width)
                .height(eui::SizeValue::wrapContent())
                .gap(16.0f)
                .lineGap(16.0f)
                .content([&] {
                    const float w = (width - 16.0f) * 0.5f;
                    swatch(ui, "settings.color.blue", "Blue", {0.26f, 0.47f, 0.86f, 1.0f}, w);
                    swatch(ui, "settings.color.green", "Green", {0.16f, 0.68f, 0.44f, 1.0f}, w);
                    swatch(ui, "settings.color.orange", "Orange", {0.92f, 0.50f, 0.20f, 1.0f}, w);
                    swatch(ui, "settings.color.pink", "Pink", {0.86f, 0.36f, 0.58f, 1.0f}, w);
                })
                .build();
            rowText(ui, "settings.slider.label", "Animation speed", percentText(animationSpeed.get()), width);
            components::slider(ui, "settings.slider")
                .theme(tokens)
                .size(width, 88.0f)
                .bind(animationSpeed)
                .transition(motion())
                .build();
            section(ui, "section.about", "About", width);
            ui.stack("about.panel")
                .size(width, 430.0f)
                .content([&] {
                    ui.rect("about.panel.bg")
                        .size(width, 430.0f)
                        .radius(22.0f)
                        .color(alpha(tokens.surfaceHover, 0.52f))
                        .border(1.0f, alpha(tokens.primary, 0.20f))
                        .build();
                    ui.image("about.icon")
                        .position(28.0f, 30.0f)
                        .size(112.0f, 112.0f)
                        .source("assets/icon.png")
                        .cover()
                        .radius(24.0f)
                        .build();
                    ui.text("about.name")
                        .position(160.0f, 32.0f)
                        .size(width - 188.0f, 50.0f)
                        .text("EUI Neo")
                        .fontSize(40.0f)
                        .lineHeight(46.0f)
                        .color(textPrimary())
                        .build();
                    ui.text("about.copy")
                        .position(160.0f, 88.0f)
                        .size(width - 188.0f, 88.0f)
                        .text("A lightweight C++ UI playground for themed controls, motion and image rendering.")
                        .fontSize(28.0f)
                        .lineHeight(36.0f)
                        .wrap(true)
                        .color(textMuted())
                        .build();
                    const float buttonGap = 18.0f;
                    const float buttonWidth = (width - 56.0f - buttonGap) * 0.5f;
                    components::button(ui, "about.github")
                        .theme(tokens, true)
                        .position(28.0f, 188.0f)
                        .size(buttonWidth, 76.0f)
                        .radius(20.0f)
                        .icon(0xF0C1)
                        .iconSize(28.0f)
                        .text("GitHub")
                        .fontSize(30.0f)
                        .onClick([] { eui::platform::openUrl("https://github.com/sudoevolve/EUI-NEO"); })
                        .transition(motion())
                        .build();
                    components::button(ui, "about.group")
                        .theme(tokens, false)
                        .position(28.0f + buttonWidth + buttonGap, 188.0f)
                        .size(buttonWidth, 76.0f)
                        .radius(20.0f)
                        .icon(0xF0C0)
                        .iconSize(28.0f)
                        .text("Group")
                        .fontSize(30.0f)
                        .onClick([] { eui::platform::openUrl("https://qm.qq.com/q/kaPB4paOpa"); })
                        .transition(motion())
                        .build();
                    ui.text("about.emoji")
                        .position(28.0f, 276.0f)
                        .size(width - 56.0f, 36.0f)
                        .text("Emoji fallback: 😀 🚀 ✨ ❤️")
                        .fontSize(24.0f)
                        .lineHeight(32.0f)
                        .color(textPrimary())
                        .build();
                    ui.text("about.runtime.title")
                        .position(28.0f, 322.0f)
                        .size(width - 56.0f, 36.0f)
                        .text("Runtime")
                        .fontSize(30.0f)
                        .lineHeight(36.0f)
                        .color(textPrimary())
                        .build();
                    ui.text("about.runtime.copy")
                        .position(28.0f, 364.0f)
                        .size(width - 56.0f, 34.0f)
                        .text("Window: SDL2     Render: Vulkan")
                        .fontSize(26.0f)
                        .lineHeight(32.0f)
                        .color(textMuted())
                        .build();
                    ui.text("about.license")
                        .position(28.0f, 398.0f)
                        .size(width - 56.0f, 34.0f)
                        .text("Copyright @2026 SudoEvolve, Apache-2.0")
                        .fontSize(24.0f)
                        .lineHeight(30.0f)
                        .color(textMuted())
                        .build();
                })
                .build();
            ui.rect("settings.bottom.space").size(width, 80.0f).color({0, 0, 0, 0}).build();
        })
        .build();
}

void composeCurrentPage(eui::Ui& ui, float width, float height) {
    if (navPage == 1) {
        composeStylePage(ui, width, height);
    } else if (navPage == 2) {
        composeAnimationPage(ui, width, height);
    } else if (navPage == 3) {
        composeSettingsPage(ui, width, height);
    } else {
        composeBasicPage(ui, width, height);
    }
}

void composeMobileModal(eui::Ui& ui, const eui::Screen& screen) {
    if (!modalOpen) {
        return;
    }
    const auto tokens = theme();
    const float side = 42.0f;
    const float width = std::max(0.0f, screen.width - side * 2.0f);
    const float height = std::min(520.0f, screen.height - 220.0f);
    const float x = side;
    const float y = std::max(130.0f, (screen.height - height) * 0.5f);
    ui.stack("mobile.modal")
        .size(screen.width, screen.height)
        .zIndex(1000)
        .content([&] {
            ui.rect("mobile.modal.backdrop")
                .size(screen.width, screen.height)
                .states({0, 0, 0, 0.54f}, {0, 0, 0, 0.54f}, {0, 0, 0, 0.54f})
                .onClick([] { modalOpen = false; })
                .build();
            ui.stack("mobile.modal.panel")
                .position(x, y)
                .size(width, height)
                .content([&] {
                    ui.rect("mobile.modal.bg")
                        .size(width, height)
                        .radius(24.0f)
                        .color(tokens.surface)
                        .border(1.0f, alpha(tokens.primary, 0.30f))
                        .shadow(30.0f, 0.0f, 12.0f, {0, 0, 0, 0.35f})
                        .build();
                    ui.rect("mobile.modal.hit")
                        .size(width, height)
                        .states({0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0})
                        .onClick([] {})
                        .build();
                    ui.text("mobile.modal.title")
                        .position(32.0f, 30.0f)
                        .size(width - 64.0f, 54.0f)
                        .text("Mobile Modal")
                        .fontSize(40.0f)
                        .lineHeight(48.0f)
                        .color({0.96f, 0.98f, 1.0f, 1.0f})
                        .build();
                    ui.text("mobile.modal.message")
                        .position(32.0f, 104.0f)
                        .size(width - 64.0f, height - 220.0f)
                        .text("This Android demo uses a phone-sized modal instead of the desktop dialog defaults, so content keeps proper padding and readable text.")
                        .fontSize(30.0f)
                        .lineHeight(40.0f)
                        .wrap(true)
                        .color({0.72f, 0.80f, 0.92f, 1.0f})
                        .build();
                    components::button(ui, "mobile.modal.close")
                        .theme(tokens, true)
                        .position(32.0f, height - 100.0f)
                        .size(width - 64.0f, 72.0f)
                        .radius(20.0f)
                        .text("Done")
                        .fontSize(30.0f)
                        .onClick([] { modalOpen = false; })
                        .transition(motion())
                        .build();
                })
                .build();
        })
        .build();
}

void composeOverlays(eui::Ui& ui, const eui::Screen& screen) {
    const auto tokens = theme();
    const float dialogWidth = std::min(720.0f, std::max(0.0f, screen.width - 84.0f));
    const float dialogHeight = 430.0f;

    components::dialog(ui, "feedback.dialog.overlay")
        .theme(tokens)
        .screen(screen.width, screen.height)
        .size(dialogWidth, dialogHeight)
        .open(modalOpen)
        .transition(motion())
        .zIndex(1200)
        .content([&] {
            ui.text("feedback.dialog.overlay.title")
                .position(34.0f, 30.0f)
                .size(dialogWidth - 68.0f, 52.0f)
                .text("Dialog Component")
                .fontSize(40.0f)
                .lineHeight(48.0f)
                .color(textPrimary())
                .build();
            ui.text("feedback.dialog.overlay.message")
                .position(34.0f, 98.0f)
                .size(dialogWidth - 68.0f, 158.0f)
                .text("A modal surface for focused confirmation. This Android page keeps the original dialog component and uses a touch-sized custom content layout.")
                .fontSize(29.0f)
                .lineHeight(38.0f)
                .wrap(true)
                .color(bodyText())
                .build();
            const float gap = 18.0f;
            const float buttonWidth = (dialogWidth - 68.0f - gap) * 0.5f;
            components::button(ui, "feedback.dialog.overlay.cancel")
                .theme(tokens, false)
                .position(34.0f, dialogHeight - 102.0f)
                .size(buttonWidth, 74.0f)
                .radius(20.0f)
                .text("Cancel")
                .fontSize(29.0f)
                .onClick([] {
                    modalOpen = false;
                    feedback = "Dialog cancelled";
                })
                .transition(motion())
                .build();
            components::button(ui, "feedback.dialog.overlay.confirm")
                .theme(tokens, true)
                .position(34.0f + buttonWidth + gap, dialogHeight - 102.0f)
                .size(buttonWidth, 74.0f)
                .radius(20.0f)
                .icon(0xF00C)
                .iconSize(28.0f)
                .text("Confirm")
                .fontSize(29.0f)
                .onClick([] {
                    modalOpen = false;
                    toastVisible = true;
                    feedback = "Dialog confirmed";
                })
                .transition(motion())
                .build();
        })
        .onOpenChange([](bool open) {
            modalOpen = open;
            if (!open && feedback == "Dialog opened") {
                feedback = "Dialog closed";
            }
        })
        .build();

    components::contextMenu(ui, "feedback.context.overlay")
        .theme(tokens)
        .screen(screen.width, screen.height)
        .position(contextMenuX, contextMenuY)
        .size(430.0f, 88.0f)
        .radius(desktopRadius(88.0f, 36.0f, 12.0f))
        .fontSize(20.0f)
        .items(std::vector<components::ContextMenuItem>{
            {"Inspect"},
            {"Duplicate"},
            {"Copy", {{"Token"}, {"Style", {{"CSS"}, {"JSON"}}}}},
            {"Dismiss"}
        })
        .open(contextMenuOpen)
        .transition(motion())
        .zIndex(1210)
        .onSelectPath([](const std::vector<int>& path) {
            contextMenuOpen = false;
            toastVisible = true;
            if (path == std::vector<int>{0}) {
                feedback = "Inspect selected";
            } else if (path == std::vector<int>{1}) {
                feedback = "Duplicate selected";
            } else if (path == std::vector<int>{2, 0}) {
                feedback = "Copy Token selected";
            } else if (path == std::vector<int>{2, 1, 0}) {
                feedback = "Copy CSS selected";
            } else if (path == std::vector<int>{2, 1, 1}) {
                feedback = "Copy JSON selected";
            } else {
                feedback = "Context menu dismissed";
                toastVisible = false;
            }
        })
        .onOpenChange([](bool open) {
            contextMenuOpen = open;
            if (!open && feedback == "Context menu opened") {
                feedback = "Context menu dismissed";
            }
        })
        .build();

    components::datePicker(ui, "feedback.datepicker.overlay")
        .theme(tokens)
        .screen(screen.width, screen.height)
        .size(std::min(520.0f, screen.width - 48.0f), 360.0f)
        .date(year, month, day)
        .titleFontSize(25.0f)
        .buttonFontSize(18.0f)
        .activeItemFontSize(25.0f)
        .itemFontSize(18.0f)
        .open(dateOpen)
        .transition(motion())
        .zIndex(1220)
        .onOpenChange([](bool open) { dateOpen = open; })
        .onChange([](int nextYear, int nextMonth, int nextDay) {
            year = nextYear;
            month = nextMonth;
            day = nextDay;
            feedback = "Date changed";
        })
        .build();

    components::timePicker(ui, "feedback.timepicker.overlay")
        .theme(tokens)
        .screen(screen.width, screen.height)
        .size(std::min(480.0f, screen.width - 48.0f), 340.0f)
        .time(hour, minute)
        .minuteStep(5)
        .titleFontSize(25.0f)
        .buttonFontSize(18.0f)
        .activeItemFontSize(26.0f)
        .itemFontSize(19.0f)
        .open(timeOpen)
        .transition(motion())
        .zIndex(1220)
        .onOpenChange([](bool open) { timeOpen = open; })
        .onChange([](int nextHour, int nextMinute) {
            hour = nextHour;
            minute = nextMinute;
            feedback = "Time changed";
        })
        .build();

    components::colorPicker(ui, "feedback.colorpicker.overlay")
        .theme(tokens)
        .screen(screen.width, screen.height)
        .size(std::min(520.0f, screen.width - 48.0f), 420.0f)
        .value(pickedColor.get())
        .titleFontSize(25.0f)
        .buttonFontSize(18.0f)
        .labelFontSize(17.0f)
        .valueFontSize(16.0f)
        .open(colorOpen)
        .transition(motion())
        .zIndex(1220)
        .onOpenChange([](bool open) { colorOpen = open; })
        .onChange([](eui::Color color) {
            pickedColor.set(color);
            sampleColor.set(color);
            feedback = "Color changed";
        })
        .build();

    components::toast(ui, "feedback.toast.overlay")
        .theme(tokens)
        .screen(screen.width, screen.height)
        .size(std::min(720.0f, screen.width - 56.0f), 160.0f)
        .visible(toastVisible)
        .duration(3.0f)
        .icon(0xF058)
        .iconSize(30.0f)
        .titleFontSize(22.0f)
        .messageFontSize(18.0f)
        .title("Gallery Feedback")
        .message(feedback)
        .transition(motion())
        .zIndex(1300)
        .onAutoDismiss([] {
            toastVisible = false;
            feedback = "Ready";
        })
        .onDismiss([] {
            toastVisible = false;
            feedback = "Toast dismissed";
        })
        .build();
}

void composeTopNav(eui::Ui& ui, float width, float height) {
    const auto tokens = theme();
    const std::vector<std::string> labels = {"Controls", "Style", "Animation", "Settings"};
    const float itemWidth = width / static_cast<float>(labels.size());
    ui.stack("top.nav")
        .size(width, height)
        .content([&] {
            ui.rect("top.nav.line")
                .position(0.0f, height - 2.0f)
                .size(width, 2.0f)
                .color(alpha(tokens.border, 0.70f))
                .build();
            for (int i = 0; i < static_cast<int>(labels.size()); ++i) {
                const bool active = navPage == i;
                const float x = static_cast<float>(i) * itemWidth;
                ui.rect("top.nav.hit." + std::to_string(i))
                    .position(x + 4.0f, 10.0f)
                    .size(std::max(0.0f, itemWidth - 8.0f), height - 20.0f)
                    .states(active ? alpha(tokens.primary, 0.16f) : transparent(),
                            alpha(tokens.primary, active ? 0.20f : 0.10f),
                            alpha(tokens.primary, 0.24f))
                    .radius(18.0f)
                    .onClick([i] { navPage = i; })
                    .build();
                ui.text("top.nav.label." + std::to_string(i))
                    .position(x, 0.0f)
                    .size(itemWidth, height - 10.0f)
                    .text(labels[static_cast<std::size_t>(i)])
                    .fontSize(28.0f)
                    .lineHeight(36.0f)
                    .color(active ? tokens.primary : textPrimary())
                    .horizontalAlign(eui::HorizontalAlign::Center)
                    .verticalAlign(eui::VerticalAlign::Center)
                    .build();
                if (active) {
                    ui.rect("top.nav.indicator." + std::to_string(i))
                        .position(x + 22.0f, height - 7.0f)
                        .size(std::max(0.0f, itemWidth - 44.0f), 5.0f)
                        .radius(2.5f)
                        .color(tokens.primary)
                        .build();
                }
            }
        })
        .build();
}

} // namespace

const DslAppConfig& dslAppConfig() {
    static const DslAppConfig config = DslAppConfig{}
        .title("EUI-NEO Android Gallery")
        .pageId("android_gallery")
        .windowSize(1080, 1920)
        .fps(120.0)
        .iconFont("assets/Font Awesome 7 Free-Solid-900.otf")
        .showDebugStatsInTitle(false);
    return config;
}

void compose(eui::Ui& ui, const eui::Screen& screen) {
    syncGlobalMotion();
    const auto tokens = theme();
    const float safeTop = 72.0f;
    const float safeBottom = 18.0f;
    const float side = 18.0f;
    const float contentWidth = std::max(0.0f, screen.width - side * 2.0f);
    const float navHeight = 96.0f;
    const float gap = 12.0f;
    const float scrollTop = safeTop + navHeight + gap;
    const float scrollHeight = std::max(0.0f, screen.height - scrollTop - safeBottom);

    ui.stack("root")
        .size(screen.width, screen.height)
        .content([&] {
            ui.rect("background")
                .size(screen.width, screen.height)
                .color(tokens.background)
                .build();

            ui.stack("top.nav.wrap")
                .position(side, safeTop)
                .size(contentWidth, navHeight)
                .zIndex(10)
                .content([&] {
                    components::tabs(ui, "top.nav")
                        .theme(tokens)
                        .size(contentWidth, navHeight)
                        .items({"Controls", "Style", "Animation", "Settings"})
                        .selected(navPage)
                        .fontSize(26.0f)
                        .onChange([](int value) { navPage = value; })
                        .transition(motion())
                        .build();
                })
                .build();

            auto scroll = components::scrollView(ui, "page.scroll")
                .position(side, scrollTop)
                .size(contentWidth, scrollHeight)
                .gap(0.0f)
                .step(96.0f)
                .scrollbarWidth(9.0f)
                .scrollbarGap(8.0f)
                .theme(tokens)
                .contentKey("android-tab-page-" + std::to_string(navPage))
                .content([](eui::Ui& contentUi, float pageWidth, float viewportHeight) {
                    composeCurrentPage(contentUi, pageWidth, viewportHeight);
                });
            if (navPage == 1) {
                scroll.bind(styleScroll);
            } else if (navPage == 2) {
                scroll.bind(animationScroll);
            } else if (navPage == 3) {
                scroll.bind(settingsScroll);
            } else {
                scroll.bind(basicScroll);
            }
            scroll.build();

            composeOverlays(ui, screen);
        })
        .build();
}

} // namespace app
