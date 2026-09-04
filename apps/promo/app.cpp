#include "eui_neo.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

namespace app {

const DslAppConfig& dslAppConfig() {
    static const DslAppConfig config = DslAppConfig{}
        .title("EUI-NEO — Built for Motion")
        .pageId("promo")
        .clearColor({0.968f, 0.968f, 0.958f, 1.0f})
        .windowSize(1440, 810)
        .fps(90.0)
        // This demo deliberately uses the requested system font. Do not ship
        // this system path as a product asset.
        .textFont("C:/Windows/Fonts/msyhbd.ttc");
    return config;
}

namespace {

constexpr const char* kMusic = "assets/music/Les Gordon - IKB (Audio).mp3";
constexpr float kEnd = 90.0f;
constexpr eui::Color kPaper{0.968f, 0.968f, 0.958f, 1.0f};
constexpr eui::Color kInk{0.055f, 0.055f, 0.052f, 1.0f};
constexpr eui::Color kSubtle{0.39f, 0.39f, 0.37f, 1.0f};
constexpr eui::Color kBlue{0.02f, 0.38f, 0.92f, 1.0f};
constexpr eui::Color kGreen{0.08f, 0.68f, 0.36f, 1.0f};
constexpr eui::Color kOrange{1.0f, 0.45f, 0.08f, 1.0f};
constexpr eui::Color kPurple{0.45f, 0.25f, 0.95f, 1.0f};

struct PromoState {
    eui::audio::Player audio;
    bool started = false;
    bool ended = false;
    float finaleHold = 0.0f;
    float posterPhase = 0.0f;
    bool sound = false;
    int segment = 0;
    float slider = 0.32f;
    std::string error;
    bool workshopLiked = false;
    float artworkReveal = 0.0f;
};

std::string musicPath() {
    const std::string resolved = eui::platform::resolveResourcePath(kMusic);
    return resolved.empty() ? kMusic : resolved;
}

std::string galleryResourcePath(const std::string& path) {
    std::string resolved = eui::platform::resolveResourcePath(path);
    if (!resolved.empty()) return resolved;
    // promo.exe is normally in build/; walk back to the repository so the
    // gallery can use the original docs/pic assets without copying them.
    resolved = eui::platform::resolveResourcePath("../" + path);
    if (!resolved.empty()) return resolved;
    return eui::platform::resolveResourcePath("../../" + path);
}

float clamp01(float value) { return std::clamp(value, 0.0f, 1.0f); }

float smoothStep(float edge0, float edge1, float value) {
    const float x = clamp01((value - edge0) / std::max(0.001f, edge1 - edge0));
    return x * x * (3.0f - 2.0f * x);
}

float outCubic(float value) {
    const float x = clamp01(value);
    return 1.0f - std::pow(1.0f - x, 3.0f);
}

float appear(float t, float at, float duration = 0.52f) {
    return outCubic((t - at) / duration);
}

float scene(float t, float first, float last, float edge = 0.55f) {
    return std::min(appear(t, first, edge), 1.0f - appear(t, last - edge, edge));
}

eui::Transition transition(float duration = 0.34f) {
    auto value = eui::Transition::make(duration, eui::Ease::OutCubic);
    value.animate(eui::AnimProperty::Frame | eui::AnimProperty::Opacity |
                  eui::AnimProperty::Transform | eui::AnimProperty::Color |
                  eui::AnimProperty::Border | eui::AnimProperty::Shadow);
    return value;
}

components::theme::ThemeColorTokens lightTheme() {
    auto tokens = components::theme::light();
    tokens.background = kPaper;
    tokens.surface = {1.0f, 1.0f, 1.0f, 1.0f};
    tokens.surfaceHover = {0.91f, 0.92f, 0.94f, 1.0f};
    tokens.surfaceActive = {0.84f, 0.87f, 0.92f, 1.0f};
    tokens.text = kInk;
    tokens.primary = kBlue;
    tokens.border = {0.10f, 0.10f, 0.10f, 0.16f};
    tokens.metrics.radius.control = 12.0f;
    tokens.metrics.radius.overlay = 18.0f;
    tokens.metrics.typography.body = 16.0f;
    return tokens;
}

components::theme::ThemeColorTokens darkTheme() {
    auto tokens = components::theme::dark();
    tokens.background = {0.035f, 0.04f, 0.055f, 1.0f};
    tokens.surface = {0.085f, 0.095f, 0.125f, 1.0f};
    tokens.surfaceHover = {0.14f, 0.16f, 0.20f, 1.0f};
    tokens.surfaceActive = {0.20f, 0.24f, 0.32f, 1.0f};
    tokens.text = {0.96f, 0.97f, 1.0f, 1.0f};
    tokens.primary = {0.25f, 0.55f, 1.0f, 1.0f};
    tokens.border = {1.0f, 1.0f, 1.0f, 0.18f};
    return tokens;
}

components::theme::ThemeColorTokens blendTheme(const components::theme::ThemeColorTokens& day,
                                                const components::theme::ThemeColorTokens& night,
                                                float amount) {
    const float t = clamp01(amount);
    auto mixed = day;
    mixed.background = eui::mixColor(day.background, night.background, t);
    mixed.primary = eui::mixColor(day.primary, night.primary, t);
    mixed.surface = eui::mixColor(day.surface, night.surface, t);
    mixed.surfaceHover = eui::mixColor(day.surfaceHover, night.surfaceHover, t);
    mixed.surfaceActive = eui::mixColor(day.surfaceActive, night.surfaceActive, t);
    mixed.text = eui::mixColor(day.text, night.text, t);
    mixed.border = eui::mixColor(day.border, night.border, t);
    mixed.dark = t > 0.50f;
    return mixed;
}

void label(eui::Ui& ui, const std::string& id, const std::string& content,
           float x, float y, float w, float h, float fontSize, float opacity = 1.0f,
           eui::Color color = kInk, eui::HorizontalAlign alignment = eui::HorizontalAlign::Left,
           float translateX = 0.0f, float translateY = 0.0f, float translateZ = 0.0f,
           float rotateY = 0.0f, float perspective = 0.0f, float scale = 1.0f) {
    auto builder = ui.text(id)
        .position(x, y).size(w, h).text(content)
        .fontSize(fontSize).fontWeight(fontSize >= 48.0f ? 820 : 700)
        .lineHeight(h).color(color).opacity(clamp01(opacity))
        .horizontalAlign(alignment).verticalAlign(eui::VerticalAlign::Center)
        .translate(translateX, translateY).translateZ(translateZ).rotateY(rotateY).scale(scale)
        .transition(transition());
    if (perspective > 0.0f) builder.perspective(perspective);
    builder.build();
}

void whiteCard(eui::Ui& ui, const std::string& id, float x, float y, float w, float h,
               float opacity = 1.0f, float radius = 28.0f,
               float translateX = 0.0f, float translateY = 0.0f,
               float translateZ = 0.0f, float rotateY = 0.0f,
               eui::Color fill = {1.0f, 1.0f, 1.0f, 1.0f}) {
    ui.rect(id)
        .position(x, y).size(w, h).radius(radius).color(fill)
        .opacity(opacity).border(1.0f, {kInk.r, kInk.g, kInk.b, 0.09f})
        .shadow(42.0f, 0.0f, 18.0f, {0.0f, 0.0f, 0.0f, 0.10f})
        .translate(translateX, translateY).translateZ(translateZ).rotateY(rotateY).perspective(860.0f)
        .transition(transition()).build();
}

void smallPill(eui::Ui& ui, const std::string& id, const std::string& content,
               float x, float y, float w, eui::Color fill, float opacity) {
    ui.rect(id + ".bg").position(x, y).size(w, 30.0f).radius(15.0f)
        .color(fill).opacity(opacity).transition(transition()).build();
    label(ui, id + ".label", content, x, y + 1.0f, w, 27.0f, 12.0f, opacity,
          (fill.r + fill.g + fill.b) < 0.35f ? kPaper : kInk, eui::HorizontalAlign::Center);
}

void bigWord(eui::Ui& ui, const std::string& id, const std::string& content,
              float width, float height, float y, float opacity, float xOffset = 0.0f,
              float scale = 1.0f, eui::Color color = kInk) {
    const float size = std::min(width * 0.205f, 246.0f);
    const float entrance = 1.0f - opacity;
    ui.text(id).position(xOffset, y).size(width, height).text(content)
        .fontSize(size).fontWeight(860).lineHeight(height).color(color).opacity(opacity)
        .horizontalAlign(eui::HorizontalAlign::Center).verticalAlign(eui::VerticalAlign::Center)
        .scale(scale).translateZ(entrance * -180.0f).rotateY(entrance * -0.28f).perspective(900.0f)
        .transition(transition(0.46f)).build();
}

void opening(eui::Ui& ui, float width, float height, float t) {
    // Beat 1: UI. lands. Beat 3→4: the brand icon enters from the right.
    // Beat 4: impact drives the whole UI lock-up to the left as one unit. The logo rebounds,
    // tilts from the hit, then eases back to a level lock-up.
    const float uiIn = appear(t, 0.418f, 0.30f);
    const float approach = smoothStep(1.277f, 1.707f, t);
    const float impact = smoothStep(1.707f, 2.136f, t);
    const float settle = smoothStep(2.136f, 3.018f, t);
    const float resolve = appear(t, 3.459f, 0.48f);
    const float hold = scene(t, 0.418f, 7.454f, 0.36f);
    const float iconSize = std::min(width * 0.17f, 220.0f);
    const float startX = width + iconSize * 0.28f;
    const float startY = height * 0.28f;
    const float endX = width * 0.48f - iconSize * 0.5f;
    const float endY = height * 0.45f - iconSize * 0.5f;
    const float logoTravel = outCubic(approach);
    const float reboundPhase = clamp01((t - 2.136f) / 0.882f);
    const float hitProgress = outCubic(impact);
    const float spring = std::exp(-4.2f * reboundPhase) * std::cos(11.0f * reboundPhase);
    const float logoX = t < 2.136f
        ? startX + (endX - startX) * logoTravel - hitProgress * 110.0f
        : endX - 110.0f * spring;
    const float logoY = t < 2.136f
        ? startY + (endY - startY) * logoTravel + hitProgress * 22.0f
        : endY + 24.0f * spring;
    const float logoTilt = t < 2.136f
        ? 0.20f - hitProgress * 0.34f
        : -0.14f * spring;
    // Keep the lock-up compact before impact, then send the entire group past
    // the left edge. The overshoot makes the hit read as physical, not as a fade.
    const float uiKick = impact * width * 0.82f +
        std::sin(clamp01((t - 1.707f) / 0.52f) * 3.1415926f) * width * 0.06f;

    label(ui, "opening.ui.left", "U", width * 0.255f, height * 0.24f, width * 0.18f, height * 0.33f,
          std::min(width * 0.26f, 300.0f), hold * uiIn, kInk, eui::HorizontalAlign::Center,
          -uiKick, impact * 18.0f, -impact * 80.0f, -impact * 0.18f, 1100.0f, 1.0f - impact * 0.08f);
    label(ui, "opening.ui.right", "I.", width * 0.435f, height * 0.24f, width * 0.20f, height * 0.33f,
          std::min(width * 0.26f, 300.0f), hold * uiIn, kInk, eui::HorizontalAlign::Center,
          -uiKick, -impact * 18.0f, -impact * 100.0f, impact * 0.20f, 1100.0f, 1.0f - impact * 0.08f);
    ui.image("opening.logo")
        .position(logoX, logoY)
        .size(iconSize, iconSize).source("assets/icon.png").contain().opacity(hold * appear(t, 0.848f, 0.16f))
        .scale(0.64f + logoTravel * 0.62f + (1.0f - settle) * 0.04f).rotate(logoTilt)
        .translateZ(-260.0f + logoTravel * 340.0f + std::sin(reboundPhase * 3.1415926f) * 70.0f)
        .rotateY(-0.18f + logoTravel * 0.18f + std::sin(reboundPhase * 3.1415926f) * 0.12f).perspective(1100.0f)
        .transition(transition(0.12f)).build();
    ui.rect("opening.impact.ring").position(endX + iconSize * 0.5f - 4.0f, endY + iconSize * 0.5f - 4.0f)
        .size(8.0f + impact * 280.0f, 8.0f + impact * 280.0f).radius(999.0f)
        .color({kBlue.r, kBlue.g, kBlue.b, 0.16f * (1.0f - impact)}).opacity(hold).transition(transition(0.12f)).build();
    label(ui, "opening.brand", "EUI-NEO", 0.0f, height * 0.66f, width, height * 0.16f,
          std::min(width * 0.105f, 150.0f), hold * resolve, kInk, eui::HorizontalAlign::Center,
          0.0f, (1.0f - resolve) * 65.0f, (1.0f - resolve) * -160.0f, 0.0f, 1100.0f,
          0.90f + resolve * 0.10f);
    label(ui, "opening.tagline", "全新 C++ 跨平台高性能轻量级 UI 框架", 0.0f, height * 0.82f, width, 34.0f, 22.0f,
          hold * appear(t, 3.901f, 0.28f), kBlue, eui::HorizontalAlign::Center);
}

void codeScene(eui::Ui& ui, float width, float height, float t, float opacity) {
    const float cardW = std::min(width * 0.56f, 760.0f);
    const float cardH = std::min(height * 0.52f, 410.0f);
    const float inProgress = appear(t, 7.454f, 0.72f);
    const float x = width * 0.07f + (1.0f - inProgress) * width * 0.16f;
    const float y = height * 0.38f;
    label(ui, "code.head", "Written in C++.", width * 0.10f, height * 0.13f, width * 0.8f, 108.0f,
          std::min(width * 0.095f, 118.0f), opacity, kInk, eui::HorizontalAlign::Center,
          (1.0f - inProgress) * width * 0.15f, (1.0f - inProgress) * -38.0f,
          (1.0f - inProgress) * -120.0f, (1.0f - inProgress) * 0.22f, 900.0f,
          0.94f + inProgress * 0.06f);
    whiteCard(ui, "code.card", x, y, cardW, cardH, opacity, 28.0f,
              (1.0f - inProgress) * width * 0.18f, (1.0f - inProgress) * 30.0f,
              (1.0f - inProgress) * -150.0f, (1.0f - inProgress) * -0.30f);
    smallPill(ui, "code.pill", "C++17", x + 34.0f, y + 30.0f, 72.0f, kBlue, opacity);
    label(ui, "code.one", "#include \"eui_neo.h\"", x + 38.0f, y + 90.0f, cardW - 76.0f, 38.0f, 20.0f,
          opacity * appear(t, 7.906f, 0.24f), kPurple);
    label(ui, "code.two", "namespace app {", x + 38.0f, y + 137.0f, cardW - 76.0f, 32.0f, 20.0f,
          opacity * appear(t, 8.348f, 0.24f), kInk);
    label(ui, "code.three", "const DslAppConfig& dslAppConfig() {", x + 38.0f, y + 184.0f, cardW - 76.0f, 32.0f, 17.0f,
          opacity * appear(t, 8.800f, 0.24f), kInk);
    label(ui, "code.four", "  .windowSize(800, 600).fps(90.0);", x + 38.0f, y + 231.0f, cardW - 76.0f, 32.0f, 17.0f,
          opacity * appear(t, 9.242f, 0.24f), kBlue);
    label(ui, "code.five", "void compose(eui::Ui& ui, const eui::Screen& screen) {", x + 38.0f, y + 278.0f, cardW - 76.0f, 32.0f, 16.0f,
          opacity * appear(t, 9.682f, 0.24f), kSubtle);
    label(ui, "code.six", "  ui.text(\"title\").text(\"Hello, EUI-NEO\");", x + 38.0f, y + 325.0f, cardW - 76.0f, 32.0f, 16.0f,
          opacity * appear(t, 10.136f, 0.24f), kInk);
    label(ui, "code.seven", "  components::button(ui, \"my_button\").text(\"Click me\").build();", x + 38.0f, y + 372.0f, cardW - 76.0f, 32.0f, 15.0f,
          opacity * appear(t, 10.588f, 0.24f), kBlue);
    const float previewOpacity = opacity * appear(t, 10.136f, 0.42f);
    const float previewX = width * 0.67f;
    const float previewY = height * 0.28f;
    const float previewW = width * 0.27f;
    const float previewH = height * 0.42f;
    whiteCard(ui, "code.preview.card", previewX, previewY, previewW, previewH, previewOpacity, 24.0f,
              (1.0f - previewOpacity) * width * 0.08f, 0.0f, (1.0f - previewOpacity) * -150.0f,
              (1.0f - previewOpacity) * 0.18f);
    label(ui, "code.preview.title", "Live preview", previewX + 28.0f, previewY + 26.0f, previewW - 56.0f, 32.0f, 22.0f, previewOpacity, kInk);
    components::input(ui, "code.preview.input").position(previewX + 28.0f, previewY + 82.0f)
        .size(previewW - 56.0f, 44.0f).value("EUI-NEO").placeholder("Type here")
        .theme(lightTheme()).transition(transition(0.42f)).onChange([](const std::string&) {}).build();
    components::button(ui, "code.preview.button").position(previewX + 28.0f, previewY + 148.0f)
        .size(previewW - 56.0f, 48.0f).text("Run UI").fontSize(16.0f).theme(lightTheme())
        .transition(transition(0.42f)).onClick([] {}).build();
    label(ui, "code.preview.status", "Frame  ·  90 FPS", previewX + 28.0f, previewY + 222.0f, previewW - 56.0f, 26.0f, 15.0f, previewOpacity, kBlue);
}

void productControls(eui::Ui& ui, PromoState& state, float width, float height, float t, float opacity) {
    const float local = std::max(0.0f, t - 14.791f);
    // Theme change is staged on a beat: the whole card travels from its lower-left
    // anchor, turns in 3D, swaps theme at the midpoint, then settles back.
    const float nightIn = smoothStep(8.15f, 10.25f, local);
    const float nightOut = smoothStep(11.25f, 13.35f, local);
    const float nightWave = nightIn * (1.0f - nightOut);
    const auto dayTheme = lightTheme();
    const auto nightTheme = darkTheme();
    // Every region gets a different continuous theme value. This makes the
    // lower-left to upper-right dark-mode travel visible inside the card.
    const auto themeAt = [&](float diagonalPhase) {
        const float start = clamp01(diagonalPhase) * 0.58f;
        return blendTheme(dayTheme, nightTheme,
                          clamp01((nightWave - start) / std::max(0.18f, 1.0f - start)));
    };
    const auto cardTheme = blendTheme(dayTheme, nightTheme, nightWave);
    const auto segmentTheme = themeAt(0.82f);
    const auto switchTheme = themeAt(1.0f);
    const auto sliderTheme = themeAt(0.60f);
    const auto progressTheme = themeAt(0.50f);
    const auto rackTheme = themeAt(0.18f);
    const auto bottomTheme = themeAt(0.08f);
    const float cardW = std::min(width * 0.74f, 950.0f);
    const float cardH = std::min(height * 0.66f, 535.0f);
    const float inProgress = appear(t, 14.791f, 0.72f);
    const float outProgress = 1.0f - scene(t, 14.791f, 29.025f, 0.70f);
    const float travel = std::sin((t - 14.791f) * 0.62f) * 26.0f;
    const float x = (width - cardW) * 0.5f + travel + outProgress * width * 0.20f;
    const float y = height * 0.27f;
    label(ui, "product.title", t < 23.708f ? "Feels native." : "Feels alive.",
          width * 0.085f, height * 0.07f, width * 0.82f, 86.0f,
          std::min(width * 0.09f, 108.0f), opacity, kInk, eui::HorizontalAlign::Left,
          (1.0f - inProgress) * -width * 0.14f + outProgress * width * 0.12f,
          (1.0f - inProgress) * 46.0f - outProgress * 30.0f,
          (1.0f - inProgress) * -180.0f - outProgress * 120.0f,
          (1.0f - inProgress) * -0.25f + outProgress * 0.25f, 900.0f,
          0.94f + inProgress * 0.06f);
    const float cardSweep = std::sin(nightWave * 3.1415926f);
    whiteCard(ui, "product.card", x, y, cardW, cardH, opacity, 28.0f,
              (1.0f - inProgress) * -width * 0.12f - cardSweep * cardW * 0.10f,
              (1.0f - inProgress) * 36.0f + cardSweep * cardH * 0.08f,
              (1.0f - inProgress) * -130.0f + cardSweep * 90.0f,
              (1.0f - inProgress) * -0.22f + outProgress * 0.20f + cardSweep * 0.10f,
              cardTheme.surface);
    const eui::Color controlText = cardTheme.text;
    label(ui, "product.caption", nightWave > 0.50f ? "NIGHT MODE · LIVE" : "DEFAULT LIGHT THEME", x + 42.0f, y + 34.0f, cardW - 84.0f, 28.0f, 13.0f, opacity, controlText);

    ui.stack("product.segment.wrap").position(x + 42.0f, y + 100.0f).size(344.0f, 46.0f)
        .translate(std::sin(local * 4.2f) * 5.0f, std::cos(local * 3.6f) * 3.0f)
        .translateZ(std::sin(local * 2.1f) * 18.0f).rotateY(std::sin(local * 1.7f) * 0.035f).perspective(800.0f).opacity(opacity).content([&] {
        components::segmented(ui, "product.segment")
            .size(344.0f, 46.0f).items({"Design", "Motion", "Ship"})
            .selected(t >= 22.361f ? 1 : state.segment).fontSize(15.0f).theme(segmentTheme)
            .transition(transition(0.72f))
            .onChange([&state](int value) { state.segment = value; }).build();
    }).build();
    ui.stack("product.switch.wrap").position(x + 440.0f, y + 98.0f).size(220.0f, 48.0f)
        .translate(std::sin(local * 4.2f + 1.0f) * 5.0f, std::cos(local * 3.6f + 1.0f) * 3.0f)
        .translateZ(std::sin(local * 2.1f + 1.0f) * 18.0f).rotateY(std::sin(local * 1.7f + 1.0f) * 0.035f).perspective(800.0f).opacity(opacity).content([&] {
        components::toggleSwitch(ui, "product.switch").size(220.0f, 48.0f)
            .text("Live motion").checked(t >= 22.361f || state.sound).theme(switchTheme)
            .transition(transition(0.72f))
            .onChange([&state](bool value) { state.sound = value; }).build();
    }).build();
    label(ui, "product.slider.label", "Transition", x + 42.0f, y + 188.0f, 180.0f, 28.0f, 14.0f, opacity, sliderTheme.text);
    ui.stack("product.slider.wrap").position(x + 42.0f, y + 224.0f).size(500.0f, 42.0f)
        .translate(std::sin(local * 4.2f + 2.0f) * 5.0f, std::cos(local * 3.6f + 2.0f) * 3.0f)
        .translateZ(std::sin(local * 2.1f + 2.0f) * 18.0f).rotateY(std::sin(local * 1.7f + 2.0f) * 0.035f).perspective(800.0f).opacity(opacity).content([&] {
        const float value = t < 23.708f ? clamp01((t - 22.4f) / 1.1f) * 0.78f : 0.78f;
        components::slider(ui, "product.slider").size(500.0f, 42.0f).value(value).theme(sliderTheme)
            .transition(transition(0.72f))
            .onChange([&state](float next) { state.slider = next; }).build();
    }).build();
    label(ui, "product.progress.label", "Composed", x + 42.0f, y + 300.0f, 180.0f, 28.0f, 14.0f, opacity, progressTheme.text);
    ui.stack("product.progress.wrap").position(x + 42.0f, y + 338.0f).size(500.0f, 13.0f).opacity(opacity).content([&] {
        components::progress(ui, "product.progress").size(500.0f, 13.0f)
            .value(t < 24.149f ? 0.62f : 1.0f).theme(progressTheme).transition(transition(0.72f)).build();
    }).build();
    ui.row("product.controls.extra").position(x + 42.0f, y + 382.0f).size(cardW - 84.0f, 48.0f)
        .gap(18.0f).translate(std::sin(local * 4.2f + 3.0f) * 5.0f, std::cos(local * 3.6f + 3.0f) * 3.0f)
        .translateZ(std::sin(local * 2.1f + 3.0f) * 18.0f).rotateY(std::sin(local * 1.7f + 3.0f) * 0.035f).perspective(800.0f).opacity(opacity).content([&] {
        components::checkbox(ui, "product.checkbox").size(150.0f, 38.0f).text("Enabled")
            .checked(t > 20.0f || state.sound).theme(rackTheme).transition(transition(0.72f)).onChange([](bool) {}).build();
        components::radio(ui, "product.radio").size(140.0f, 38.0f).text("Auto")
            .selected(t > 21.0f).theme(rackTheme).transition(transition(0.72f)).onChange([](bool) {}).build();
        components::stepper(ui, "product.stepper").size(132.0f, 38.0f).value(3).min(0).max(9)
            .step(1).theme(rackTheme).transition(transition(0.72f)).onChange([](long long) {}).build();
        components::dropdown(ui, "product.dropdown").size(190.0f, 38.0f)
            .items({"Draft", "Review", "Ship"}).selected(t > 22.0f ? 1 : 0).theme(rackTheme).transition(transition(0.72f))
            .onChange([](int) {}).build();
    }).build();
    ui.row("product.controls.extra2").position(x + 42.0f, y + 436.0f).size(cardW - 84.0f, 42.0f)
        .gap(18.0f).translate(std::sin(local * 4.2f + 4.0f) * 5.0f, std::cos(local * 3.6f + 4.0f) * 3.0f)
        .translateZ(std::sin(local * 2.1f + 4.0f) * 18.0f).rotateY(std::sin(local * 1.7f + 4.0f) * 0.035f).perspective(800.0f).opacity(opacity).content([&] {
        components::tabs(ui, "product.tabs").size(300.0f, 38.0f).items({"Preview", "Inspect", "Code"})
            .selected(static_cast<int>(std::fmod(std::max(0.0f, t - 18.0f), 6.0f) > 3.0f))
            .theme(bottomTheme).transition(transition(0.72f)).onChange([](int) {}).build();
        components::input(ui, "product.input").size(300.0f, 38.0f).value("Motion-ready")
            .placeholder("Type to compose").theme(bottomTheme).transition(transition(0.72f)).onChange([](const std::string&) {}).build();
    }).build();
    components::button(ui, "product.button").position(x + cardW - 226.0f, y + cardH - 92.0f).size(184.0f, 54.0f)
        .text("Preview").fontSize(16.0f).theme(themeAt(0.34f)).transition(transition(0.72f)).build();
}

void motionScene(eui::Ui& ui, float width, float height, float t, float opacity) {
    label(ui, "motion.title", "Moves with intent.", width * 0.08f, height * 0.10f, width * 0.84f, 100.0f,
          std::min(width * 0.10f, 122.0f), opacity, kInk);
    const float centerX = width * 0.50f;
    const float cardW = std::min(width * 0.30f, 390.0f);
    const float cardH = std::min(height * 0.38f, 300.0f);
    const float cardY = height * 0.48f;
    const float phase = clamp01((t - 29.025f) / 12.0f);
    const float gap = cardW * 0.64f;
    for (int index = 0; index < 3; ++index) {
        const float relative = static_cast<float>(index - 1);
        const float x = centerX - cardW * 0.5f + relative * gap + std::sin(t * 0.8f + index) * 12.0f;
        const float rotation = relative * -0.34f + (index == 1 ? std::sin(t * 1.8f) * 0.12f : 0.0f);
        ui.stack("motion.card." + std::to_string(index)).position(x, cardY).size(cardW, cardH)
            .rotateY(rotation).translateZ((1.0f - std::abs(relative) * 0.45f) * 44.0f).perspective(900.0f)
            .opacity(opacity * (index == 1 ? 1.0f : 0.76f)).transition(transition(0.42f)).content([&] {
                whiteCard(ui, "motion.card.bg." + std::to_string(index), 0.0f, 0.0f, cardW, cardH, 1.0f);
                ui.rect("motion.card.color." + std::to_string(index)).position(22.0f, 22.0f).size(cardW - 44.0f, 12.0f).radius(6.0f)
                    .color(index == 0 ? kOrange : (index == 1 ? kBlue : kPurple)).build();
                label(ui, "motion.card.head." + std::to_string(index), index == 1 ? "Animation" : "Transition",
                      24.0f, 64.0f, cardW - 48.0f, 38.0f, 25.0f, 1.0f, kInk);
                label(ui, "motion.card.body." + std::to_string(index), index == 1 ? "Frame · opacity · transform" : "One motion system",
                      24.0f, 116.0f, cardW - 48.0f, 26.0f, 14.0f, 1.0f, kSubtle);
                ui.rect("motion.card.dot." + std::to_string(index)).position(24.0f + phase * (cardW - 82.0f), cardH - 56.0f)
                    .size(28.0f, 28.0f).radius(14.0f).color(index == 1 ? kBlue : kOrange).transition(transition(0.24f)).build();
            }).build();
    }
}

void advantagesScene(eui::Ui& ui, float width, float height, float t, float opacity) {
    const float local = std::max(0.0f, t - 29.025f);
    const float metricsIn = appear(t, 31.695f, 0.34f);
    const float startIn = appear(t, 33.018f, 0.34f);
    const float controlsIn = appear(t, 34.366f, 0.34f);
    const float nightAmount = smoothStep(36.130f, 38.359f, t);
    const auto liveTheme = blendTheme(lightTheme(), darkTheme(), nightAmount);
    const std::string title = local < 2.9f ? "小，而强大。" : (local < 5.0f ? "从这里开始。" : (local < 7.0f ? "内置，不止一种。" : "昼夜，皆宜。"));
    const std::string subtitle = local < 2.9f ? "无需背负庞大的依赖。" : (local < 5.0f ? "CMake 加一个 main，马上开始写。" : (local < 7.0f ? "控件、状态与动效，开箱即用。" : "内置夜间模式，自然切换。"));
    label(ui, "adv.intro", "来了解一下 EUI-NEO", width * 0.08f, height * 0.045f, width * 0.84f, 34.0f,
          22.0f, opacity * appear(t, 29.025f, 0.28f), kBlue, eui::HorizontalAlign::Left,
          (1.0f - appear(t, 29.025f, 0.28f)) * -70.0f, 0.0f, -80.0f, -0.08f, 900.0f);
    const float introIn = smoothStep(29.025f, 29.466f, t);
    const float introOut = smoothStep(30.813f, 31.254f, t);
    const float introMix = introIn * (1.0f - introOut);
    const float introY = -height * 0.34f + height * 0.68f * introIn + height * 0.92f * introOut;
#if 0
    label(ui, "adv.intro.hero", "来了解一下 EUI-NEO", 0.0f, introY, width, 130.0f,
          std::min(width * 0.105f, 148.0f), opacity * introMix, kBlue,
          eui::HorizontalAlign::Center, 0.0f, 0.0f, 80.0f * (1.0f - introIn),
          -0.04f * (1.0f - introIn), 1000.0f, 0.92f + 0.08f * introIn);
    label(ui, "adv.intro.hero.cn", "来了解一下 EUI-NEO", 0.0f, introY, width, 130.0f,
          std::min(width * 0.105f, 148.0f), opacity * introMix, kBlue,
          eui::HorizontalAlign::Center, 0.0f, 0.0f, 80.0f * (1.0f - introIn),
          -0.04f * (1.0f - introIn), 1000.0f, 0.92f + 0.08f * introIn);
#endif
    label(ui, "adv.intro.hero.final", "来了解一下 EUI-NEO", 0.0f, introY, width, 130.0f,
          std::min(width * 0.105f, 148.0f), opacity * introMix, kBlue,
          eui::HorizontalAlign::Center, 0.0f, 0.0f, 80.0f * (1.0f - introIn),
          -0.04f * (1.0f - introIn), 1000.0f, 0.92f + 0.08f * introIn);
    const float titleReveal = appear(t, 31.695f, 0.30f);
    label(ui, "adv.title", title, width * 0.08f, height * 0.09f, width * 0.84f, 100.0f,
          std::min(width * 0.09f, 108.0f), opacity * titleReveal, kInk);
    label(ui, "adv.subtitle", subtitle, width * 0.085f, height * 0.225f, width * 0.78f, 34.0f, 22.0f, opacity * titleReveal, kSubtle);

    const float statY = height * 0.38f;
    label(ui, "adv.source.number", "15 MB", width * 0.10f, statY, width * 0.34f, 86.0f, std::min(width * 0.085f, 106.0f),
          opacity * metricsIn, kBlue, eui::HorizontalAlign::Left, (1.0f - metricsIn) * -90.0f, 0.0f, (1.0f - metricsIn) * -100.0f, -0.13f, 900.0f);
    label(ui, "adv.source.label", "源码体积", width * 0.10f, statY + 90.0f, width * 0.30f, 30.0f, 20.0f, opacity * metricsIn, kInk);
    label(ui, "adv.source.detail", "无需下载 Qt、Flutter 这类庞大依赖。", width * 0.10f, statY + 128.0f, width * 0.37f, 30.0f, 16.0f, opacity * metricsIn, kSubtle);
    label(ui, "adv.binary.number", "1.8 MB", width * 0.57f, statY, width * 0.34f, 86.0f, std::min(width * 0.085f, 106.0f),
          opacity * metricsIn, kGreen, eui::HorizontalAlign::Left, (1.0f - metricsIn) * 90.0f, 0.0f, (1.0f - metricsIn) * -100.0f, 0.13f, 900.0f);
    label(ui, "adv.binary.label", "编译产物", width * 0.57f, statY + 90.0f, width * 0.28f, 30.0f, 20.0f, opacity * metricsIn, kInk);
    label(ui, "adv.binary.detail", "轻装上阵，交付更轻。", width * 0.57f, statY + 128.0f, width * 0.28f, 30.0f, 16.0f, opacity * metricsIn, kSubtle);

    const float panelW = std::min(width * 0.77f, 1000.0f);
    const float panelH = std::min(height * 0.36f, 276.0f);
    const float panelX = (width - panelW) * 0.5f;
    const float panelY = height * 0.54f;
    whiteCard(ui, "adv.panel", panelX, panelY, panelW, panelH, opacity * std::max(startIn, controlsIn), 26.0f,
              (1.0f - startIn) * width * 0.14f, (1.0f - startIn) * 34.0f, (1.0f - startIn) * -140.0f,
              (1.0f - startIn) * -0.16f, liveTheme.surface);
    label(ui, "adv.cmake", "cmake -S . -B build", panelX + 34.0f, panelY + 28.0f, panelW * 0.48f, 34.0f, 23.0f, opacity * startIn, kPurple);
    label(ui, "adv.main", "int main() { return eui::run(); }", panelX + 34.0f, panelY + 70.0f, panelW * 0.55f, 28.0f, 17.0f, opacity * startIn, liveTheme.text);
    components::button(ui, "adv.start.button").position(panelX + panelW - 220.0f, panelY + 30.0f).size(184.0f, 48.0f)
        .text("Start building").fontSize(15.0f).theme(liveTheme).transition(transition(0.42f)).build();
    ui.stack("adv.controls.segment.wrap").position(panelX + 34.0f, panelY + 124.0f).size(290.0f, 42.0f).content([&] {
        components::segmented(ui, "adv.controls.segment").size(290.0f, 42.0f)
            .items({"Design", "Data", "Motion"}).selected(local > 6.2f ? 2 : 0).theme(liveTheme).transition(transition(0.50f)).build();
    }).build();
    ui.stack("adv.controls.night.wrap").position(panelX + 358.0f, panelY + 121.0f).size(220.0f, 48.0f).content([&] {
        components::toggleSwitch(ui, "adv.controls.night").size(220.0f, 48.0f)
            .text("Night mode").checked(local > 7.0f).theme(liveTheme).transition(transition(0.50f)).build();
    }).build();
    ui.stack("adv.controls.slider.wrap").position(panelX + 34.0f, panelY + 189.0f).size(panelW - 68.0f, 36.0f).content([&] {
        components::slider(ui, "adv.controls.slider").size(panelW - 68.0f, 36.0f)
            .value(clamp01((local - 5.4f) / 3.5f)).theme(liveTheme).transition(transition(0.50f)).build();
    }).build();
    label(ui, "adv.ready", "丰富内置控件 · 主题已就绪", panelX + panelW - 350.0f, panelY + 238.0f, 316.0f, 22.0f, 14.0f,
          opacity * controlsIn, nightAmount > 0.5f ? eui::Color{0.72f, 0.84f, 1.0f, 1.0f} : kBlue, eui::HorizontalAlign::Right);
}

void dataScene(eui::Ui& ui, float width, float height, float t, float opacity) {
    const auto theme = lightTheme();
    const float inProgress = appear(t, 43.236f, 0.72f);
    label(ui, "data.title", "One runtime.", width * 0.08f, height * 0.08f, width * 0.84f, 98.0f,
          std::min(width * 0.10f, 122.0f), opacity, kInk, eui::HorizontalAlign::Left,
          (1.0f - inProgress) * width * 0.12f, (1.0f - inProgress) * -24.0f,
          (1.0f - inProgress) * -150.0f, (1.0f - inProgress) * 0.20f, 900.0f,
          0.95f + inProgress * 0.05f);
    const float cardW = std::min(width * 0.78f, 1050.0f);
    const float x = (width - cardW) * 0.5f;
    const float y = height * 0.31f;
    const float wobble = std::sin(t * 0.7f) * 12.0f;
    std::vector<float> signalValues;
    for (int i = 0; i < 7; ++i) {
        signalValues.push_back(clamp01(0.48f + 0.25f * std::sin(t * 1.7f + i * 0.82f) +
                                       0.10f * std::sin(t * 3.1f + i * 1.7f)));
    }
    std::vector<float> stateValues;
    for (int i = 0; i < 5; ++i) stateValues.push_back(clamp01(0.42f + 0.28f * std::sin(t * 1.15f + i * 0.9f)));
    ui.stack("data.chart.one").position(x + wobble, y).size(cardW * 0.48f, 315.0f)
        .translateZ((1.0f - inProgress) * -90.0f).rotateY((1.0f - inProgress) * 0.16f).perspective(900.0f)
        .opacity(opacity).content([&] {
        components::lineChart(ui, "data.line").size(cardW * 0.48f, 315.0f).title("Signals · LIVE")
            .values(signalValues)
            .labels({"01", "02", "03", "04", "05", "06", "07"}).theme(theme).transition(transition()).build();
    }).build();
    ui.stack("data.chart.two").position(x + cardW * 0.54f - wobble, y).size(cardW * 0.46f, 315.0f)
        .translateZ((1.0f - inProgress) * -150.0f).rotateY((1.0f - inProgress) * -0.14f).perspective(900.0f)
        .opacity(opacity).content([&] {
        components::barChart(ui, "data.bar").size(cardW * 0.46f, 315.0f).title("States · 90 FPS")
            .values(stateValues).labels({"UI", "State", "Frame", "Render", "Ship"})
            .colors({kBlue, kPurple, kOrange, kGreen, kInk}).theme(theme).transition(transition()).build();
    }).build();
    smallPill(ui, "data.state", "state → target → transition", x, y + 338.0f, 224.0f, kInk, opacity);
    label(ui, "data.metric", std::to_string(static_cast<int>(62 + 18 * std::sin(t * 1.4f))) + " ms",
          x + cardW * 0.70f, y + 338.0f, cardW * 0.28f, 34.0f, 24.0f, opacity, kBlue,
          eui::HorizontalAlign::Right);
}

void dataSceneEnhanced(eui::Ui& ui, float width, float height, float t, float opacity) {
    const auto theme = lightTheme();
    const float inProgress = appear(t, 43.236f, 0.72f);
    label(ui, "data.v2.title", "One runtime.", width * 0.08f, height * 0.08f, width * 0.84f, 98.0f,
          std::min(width * 0.10f, 122.0f), opacity, kInk, eui::HorizontalAlign::Left,
          (1.0f - inProgress) * width * 0.12f, (1.0f - inProgress) * -24.0f,
          (1.0f - inProgress) * -150.0f, (1.0f - inProgress) * 0.20f, 900.0f,
          0.95f + inProgress * 0.05f);
    const float cardW = std::min(width * 0.84f, 1120.0f);
    const float x = (width - cardW) * 0.5f;
    const float y = height * 0.27f;
    const float gap = 18.0f;
    const float rowH = std::min(height * 0.30f, 246.0f);
    const float lineW = cardW * 0.40f;
    const float barW = cardW * 0.25f;
    const float pieW = cardW - lineW - barW - gap * 2.0f;
    const float drift = std::sin(t * 0.72f) * 8.0f;
    const float beat = 0.5f + 0.5f * std::sin(t * 6.2831853f * 1.5f);
    ui.rect("data.signal.beam").position(width * 0.08f, height * 0.235f).size(width * 0.84f, 3.0f)
        .radius(2.0f).color(kBlue).opacity(opacity * (0.22f + beat * 0.24f))
        .translateZ(24.0f).scale(0.96f + beat * 0.04f).build();
    std::vector<float> signalValues;
    for (int i = 0; i < 7; ++i) signalValues.push_back(clamp01(0.48f + 0.25f * std::sin(t * 1.7f + i * 0.82f) + 0.10f * std::sin(t * 3.1f + i * 1.7f)));
    std::vector<float> stateValues;
    for (int i = 0; i < 5; ++i) stateValues.push_back(clamp01(0.42f + 0.28f * std::sin(t * 1.15f + i * 0.9f)));
    ui.stack("data.v2.line").position(x + drift, y).size(lineW, rowH)
        .scale(0.985f + beat * 0.015f)
        .translateZ((1.0f - inProgress) * -90.0f).rotateY((1.0f - inProgress) * 0.16f).perspective(900.0f).opacity(opacity).content([&] {
        components::lineChart(ui, "data.v2.line.chart").size(lineW, rowH).title("Signals · LIVE").values(signalValues)
            .labels({"01", "02", "03", "04", "05", "06", "07"}).theme(theme).transition(transition()).build();
    }).build();
    ui.stack("data.v2.bar").position(x + lineW + gap - drift * 0.5f, y).size(barW, rowH)
        .translateZ((1.0f - inProgress) * -150.0f).rotateY((1.0f - inProgress) * -0.14f).perspective(900.0f).opacity(opacity).content([&] {
        components::barChart(ui, "data.v2.bar.chart").size(barW, rowH).title("States · 90 FPS").values(stateValues)
            .labels({"UI", "State", "Frame", "Render", "Ship"}).colors({kBlue, kPurple, kOrange, kGreen, kInk}).theme(theme).transition(transition()).build();
    }).build();
    const float pieX = x + lineW + barW + gap * 2.0f + drift * 0.35f;
    const std::vector<float> pieValues = {0.34f + 0.08f * std::sin(t * 1.20f), 0.26f + 0.06f * std::sin(t * 1.20f + 1.8f), 0.22f + 0.05f * std::sin(t * 1.20f + 3.2f), 0.18f + 0.04f * std::sin(t * 1.20f + 4.6f)};
    ui.stack("data.v2.pie").position(pieX, y).size(pieW, rowH)
        .translateZ((1.0f - inProgress) * -210.0f).rotateY((1.0f - inProgress) * 0.11f).perspective(900.0f).opacity(opacity).content([&] {
        components::pieChart(ui, "data.v2.pie.chart").size(pieW, rowH).title("Composition · LIVE").values(pieValues)
            .labels({"UI", "State", "Render", "Ship"}).colors({kBlue, kGreen, kOrange, kPurple}).theme(theme).transition(transition(0.46f)).build();
    }).build();
    const int phase = static_cast<int>(std::floor(std::max(0.0f, t - 43.236f) * 2.0f)) % 4;
    const int fps = 88 + ((phase * 3 + static_cast<int>(std::floor(t))) % 7);
    const int jobs = 12 + ((phase * 5 + static_cast<int>(std::floor(t * 0.5f))) % 9);
    const std::vector<std::vector<std::string>> rows = {
        {"Input", phase == 0 ? "SYNC" : (phase == 1 ? "READY" : "LIVE"), std::to_string(fps) + " fps", "eui::input"},
        {"Canvas", phase == 2 ? "DRAW" : "STABLE", std::to_string(fps + 1) + " fps", "shader pass"},
        {"Motion", phase == 3 ? "MORPH" : "SMOOTH", std::to_string(fps - 1) + " fps", std::to_string(jobs) + " jobs"},
        {"Export", phase == 1 ? "PACK" : "READY", "1.8 MB", "CMake"}
    };
    const float tableY = y + rowH + 18.0f;
    const float tableW = cardW * 0.72f;
    const float tableH = std::min(height * 0.19f, 154.0f);
    ui.stack("data.v2.table").position(x, tableY).size(tableW, tableH)
        .translateZ((1.0f - inProgress) * -130.0f).rotateY((1.0f - inProgress) * -0.06f).perspective(900.0f).opacity(opacity).content([&] {
        components::dataTable(ui, "data.v2.table.widget").size(tableW, tableH).columns({"MODULE", "STATUS", "PERF", "PIPELINE"})
            .rows(rows).theme(theme).transition(transition(0.38f)).build();
    }).build();
    smallPill(ui, "data.v2.state", "state -> target -> transition", x + tableW + 18.0f, tableY + 8.0f, cardW - tableW - 18.0f, kInk, opacity);
    label(ui, "data.v2.metric", std::to_string(static_cast<int>(62 + 18 * std::sin(t * 1.4f))) + " ms", x + tableW + 18.0f, tableY + 62.0f, cardW - tableW - 18.0f, 48.0f, 30.0f, opacity, kBlue, eui::HorizontalAlign::Left);
}

void platformWindow(eui::Ui& ui, const std::string& id, const std::string& platform,
                    float x, float y, float w, float opacity, eui::Color accent,
                    float translateZ = 0.0f, float rotateY = 0.0f) {
    whiteCard(ui, id + ".card", x, y, w, 170.0f, opacity, 22.0f,
              0.0f, 0.0f, translateZ, rotateY);
    ui.rect(id + ".bar").position(x + 16.0f, y + 16.0f).size(w - 32.0f, 9.0f).radius(5.0f).color(accent).opacity(opacity).build();
    label(ui, id + ".name", platform, x + 18.0f, y + 42.0f, w - 36.0f, 26.0f, 15.0f, opacity, kInk);
    if (platform == "WINDOWS") {
        for (int cell = 0; cell < 4; ++cell) {
            const float px = x + 20.0f + (cell % 2) * 30.0f;
            const float py = y + 82.0f + (cell / 2) * 30.0f;
            ui.rect(id + ".win." + std::to_string(cell)).position(px, py).size(24.0f, 24.0f).color(accent).opacity(opacity).build();
        }
        label(ui, id + ".detail", "Win32  ·  OpenGL", x + 86.0f, y + 90.0f, w - 104.0f, 30.0f, 14.0f, opacity, kSubtle);
        label(ui, id + ".run", "RUNNING", x + 86.0f, y + 120.0f, w - 104.0f, 24.0f, 13.0f, opacity, accent);
    } else if (platform == "LINUX") {
        ui.rect(id + ".terminal").position(x + 18.0f, y + 80.0f).size(w - 36.0f, 68.0f).radius(9.0f)
            .color({0.05f, 0.08f, 0.07f, 1.0f}).opacity(opacity).build();
        label(ui, id + ".prompt", "$ ./eui-neo", x + 34.0f, y + 91.0f, w - 68.0f, 22.0f, 14.0f, opacity, {0.50f, 0.94f, 0.66f, 1.0f});
        label(ui, id + ".result", "90 FPS  ·  RUNNING", x + 34.0f, y + 117.0f, w - 68.0f, 20.0f, 12.0f, opacity, {0.78f, 0.88f, 0.80f, 1.0f});
    } else {
        ui.rect(id + ".mac.window").position(x + 18.0f, y + 80.0f).size(w - 36.0f, 68.0f).radius(10.0f)
            .color({kInk.r, kInk.g, kInk.b, 0.06f}).opacity(opacity).build();
        const eui::Color dots[] = {{1.0f, 0.36f, 0.32f, 1.0f}, {1.0f, 0.74f, 0.20f, 1.0f}, {0.18f, 0.78f, 0.37f, 1.0f}};
        for (int dot = 0; dot < 3; ++dot) ui.rect(id + ".mac.dot." + std::to_string(dot)).position(x + 32.0f + dot * 18.0f, y + 94.0f).size(10.0f, 10.0f).radius(5.0f).color(dots[dot]).opacity(opacity).build();
        label(ui, id + ".mac.detail", "macOS  ·  OpenGL", x + 32.0f, y + 116.0f, w - 64.0f, 22.0f, 13.0f, opacity, kSubtle);
    }
}

void platforms(eui::Ui& ui, float width, float height, float t, float opacity) {
    const float local = std::max(0.0f, t - 64.575f);
    label(ui, "platform.title", "Write once.", width * 0.08f, height * 0.07f, width * 0.84f, 94.0f,
          std::min(width * 0.10f, 120.0f), opacity, kInk);
    label(ui, "platform.second", "Run everywhere.", width * 0.08f, height * 0.19f, width * 0.84f, 66.0f,
          std::min(width * 0.058f, 72.0f), opacity * appear(t, 61.0f), kSubtle);
    const float sourceIn = smoothStep(0.0f, 0.72f, local);
    const float sourceX = width * 0.5f - 150.0f;
    const float sourceY = height * 0.31f;
    ui.stack("platform.source").position(sourceX, sourceY).size(300.0f, 64.0f)
        .translateZ((1.0f - sourceIn) * -180.0f).rotateY((1.0f - sourceIn) * -0.18f).perspective(900.0f)
        .opacity(opacity * sourceIn).content([&] {
            whiteCard(ui, "platform.source.card", 0.0f, 0.0f, 300.0f, 64.0f, 1.0f, 18.0f);
            label(ui, "platform.source.code", "main.cpp", 18.0f, 11.0f, 120.0f, 22.0f, 16.0f, 1.0f, kBlue);
            label(ui, "platform.source.copy", "one source / three targets", 18.0f, 35.0f, 260.0f, 18.0f, 12.0f, 1.0f, kSubtle);
        }).build();
    const float w = std::min(width * 0.23f, 280.0f);
    const float gap = std::min(width * 0.038f, 52.0f);
    const float total = w * 3.0f + gap * 2.0f;
    const float startX = (width - total) * 0.5f;
    const float y = height * 0.40f;
    const float open = smoothStep(0.72f, 2.10f, local);
    for (int index = 0; index < 3; ++index) {
        const float target = startX + index * (w + gap);
        const float source = width * 0.5f - w * 0.5f;
        const float stagger = smoothStep(0.72f + index * 0.22f, 1.72f + index * 0.22f, local);
        const float x = source + (target - source) * stagger;
        const float fan = (1.0f - stagger) * (index - 1) * 0.28f;
        platformWindow(ui, "platform.window." + std::to_string(index), index == 0 ? "WINDOWS" : (index == 1 ? "LINUX" : "macOS"),
                       x, y, w, opacity * stagger, index == 0 ? kBlue : (index == 1 ? kGreen : kPurple),
                       (1.0f - stagger) * -120.0f, fan);
        const float centerX = source + w * 0.5f;
        // Connect each pulse to the visual center of its platform window.
        // Using the card's left edge made the macOS branch appear detached.
        const float targetCenter = target + w * 0.5f;
        const float nodeX = centerX + (targetCenter - centerX) * stagger;
        const float lineY = sourceY + 76.0f;
        const eui::Color accent = index == 0 ? kBlue : (index == 1 ? kGreen : kPurple);
        ui.rect("platform.branch." + std::to_string(index)).position(std::min(centerX, nodeX), lineY)
            .size(std::max(2.0f, std::fabs(nodeX - centerX)), 3.0f).radius(2.0f).color(accent).opacity(opacity * stagger * 0.72f).build();
        ui.rect("platform.connector." + std::to_string(index)).position(nodeX - 1.5f, lineY)
            .size(3.0f, std::max(2.0f, y - lineY + 2.0f)).radius(1.5f).color(accent)
            .opacity(opacity * stagger * 0.56f).build();
        const float pulse = std::fmod(local * 0.62f + index * 0.24f, 1.0f);
        ui.rect("platform.pulse." + std::to_string(index)).position(centerX + (nodeX - centerX) * pulse - 6.0f, lineY - 5.0f)
            .size(12.0f, 12.0f).radius(6.0f).color(accent).opacity(opacity * stagger).build();
    }
    label(ui, "platform.cmake", "cmake -S . -B build   →   cmake --build build", 0.0f, height * 0.72f, width, 28.0f, 15.0f,
          opacity * appear(t, 64.575f), kInk, eui::HorizontalAlign::Center);
    label(ui, "platform.stack", "CMake   ·   GLFW / SDL2   ·   OpenGL / Vulkan", 0.0f, height * 0.78f, width, 26.0f, 14.0f,
          opacity * appear(t, 66.374f), kSubtle, eui::HorizontalAlign::Center);
}

void galleryStripLegacy(eui::Ui& ui, float width, float height, float t, float opacity) {
    label(ui, "gallery.title", t < 75.0f ? "Build." : (t < 78.0f ? "Move." : "Ship."),
          width * 0.08f, height * 0.08f, width * 0.84f, 96.0f,
          std::min(width * 0.11f, 132.0f), opacity, kInk);
    const float w = std::min(width * 0.38f, 500.0f);
    const float h = std::min(height * 0.46f, 340.0f);
    const float elapsed = std::max(0.0f, t - 74.0f);
    const float start = width * 0.5f - w * 0.5f - elapsed * 42.0f;
    const char* names[] = {"Controls", "Layout", "Animation", "Data"};
    const eui::Color accents[] = {kBlue, kOrange, kPurple, kGreen};
    for (int index = 0; index < 4; ++index) {
        const float x = start + index * w * 0.92f;
        const float scale = index == 1 ? 1.0f : 0.92f;
        const float depth = index == 1 ? 52.0f : -40.0f;
        const float angle = (index - 1) * -0.12f + std::sin(t * 0.45f + index) * 0.025f;
        ui.stack("gallery.card." + std::to_string(index)).position(x, height * 0.38f).size(w, h)
            .scale(scale).translateZ(depth).rotateY(angle).perspective(920.0f)
            .opacity(opacity * (index == 1 ? 1.0f : 0.68f)).transition(transition()).content([&] {
                whiteCard(ui, "gallery.card.bg." + std::to_string(index), 0.0f, 0.0f, w, h, 1.0f);
                ui.rect("gallery.card.accent." + std::to_string(index)).position(28.0f, 28.0f).size(74.0f, 12.0f).radius(6.0f).color(accents[index]).build();
                label(ui, "gallery.card.title." + std::to_string(index), names[index], 28.0f, 68.0f, w - 56.0f, 42.0f, 28.0f);
                ui.rect("gallery.card.line." + std::to_string(index)).position(28.0f, 130.0f).size(w - 56.0f, 10.0f).radius(5.0f).color({kInk.r, kInk.g, kInk.b, 0.10f}).build();
                ui.rect("gallery.card.block." + std::to_string(index)).position(28.0f, 164.0f).size(w * 0.52f, h - 196.0f).radius(14.0f).color(accents[index]).build();
                ui.rect("gallery.card.block2." + std::to_string(index)).position(w * 0.62f, 164.0f).size(w * 0.23f, h - 196.0f).radius(14.0f).color({kInk.r, kInk.g, kInk.b, 0.12f}).build();
            }).build();
    }
}

void galleryStrip(eui::Ui& ui, float width, float height, float t, float opacity) {
    const float local = std::max(0.0f, t - 74.0f);
    const float intro = smoothStep(0.0f, 0.62f, local);
    const float beat = 0.5f + 0.5f * std::sin(local * 6.2831853f * 1.5f);
    label(ui, "gallery.story", "Every surface tells a story.", width * 0.08f, height * 0.07f,
          width * 0.84f, 90.0f, std::min(width * 0.075f, 94.0f), opacity * intro, kInk,
          eui::HorizontalAlign::Left, (1.0f - intro) * -width * 0.12f, 0.0f,
          (1.0f - intro) * -140.0f, (1.0f - intro) * 0.08f, 900.0f);
    label(ui, "gallery.story.sub", "Built-in controls, layouts, motion and data — ready to compose.", width * 0.08f, height * 0.18f,
          width * 0.84f, 30.0f, 18.0f, opacity * smoothStep(0.28f, 0.92f, local), kSubtle);

    const char* names[] = {"Controls", "Layout", "Animation", "Data", "Workshop", "Ship"};
    const char* galleryPaths[] = {"docs/pic/1.jpg", "docs/pic/2.jpg", "docs/pic/3.jpg", "docs/pic/4.jpg", "docs/pic/示例1.jpg", "docs/pic/示例2.jpg"};
    const eui::Color accents[] = {kBlue, kOrange, kPurple, kGreen, kBlue, kOrange};
    const int count = 6;
    const float carousel = local * 0.72f;
    const int active = static_cast<int>(std::floor(carousel)) % count;
    const float fractional = carousel - std::floor(carousel);
    const float photoW = std::min(width * 0.40f, 610.0f);
    const float photoH = std::min(height * 0.57f, photoW * 0.685f + 72.0f);
    const float photoX = (width - photoW) * 0.5f;
    const float photoY = height * 0.31f;
    for (int index = 0; index < count; ++index) {
        int wrapped = index - active;
        if (wrapped > count / 2) wrapped -= count;
        if (wrapped < -count / 2) wrapped += count;
        const float relative = static_cast<float>(wrapped) - fractional;
        if (std::fabs(relative) > 2.35f) continue;
        const float targetX = photoX + relative * photoW * 0.78f;
        const float centerWeight = clamp01(1.0f - std::fabs(relative));
        const float depth = centerWeight * 90.0f + (1.0f - centerWeight) * -120.0f;
        const float scale = 0.80f + centerWeight * 0.20f + (centerWeight > 0.92f ? beat * 0.018f : 0.0f);
        const float itemOpacity = opacity * intro * (0.26f + centerWeight * 0.74f);
        const std::string resolved = galleryResourcePath(galleryPaths[index]);
        ui.stack("gallery.story.item." + std::to_string(index)).position(targetX, photoY).size(photoW, photoH)
            .scale(scale).translateZ(depth).rotateY(relative * -0.12f).perspective(960.0f)
            .opacity(itemOpacity).transition(transition(0.58f)).content([&] {
                whiteCard(ui, "gallery.story.card." + std::to_string(index), 0.0f, 0.0f, photoW, photoH, 1.0f, 24.0f);
                const float imageH = photoH - 72.0f;
                ui.image("gallery.story.image." + std::to_string(index)).position(14.0f, 14.0f).size(photoW - 28.0f, imageH)
                    .source(resolved.empty() ? galleryPaths[index] : resolved).contain().radius(16.0f).opacity(0.96f).build();
                ui.rect("gallery.story.accent." + std::to_string(index)).position(18.0f, photoH - 42.0f).size(54.0f, 8.0f)
                    .radius(4.0f).color(accents[index]).build();
                label(ui, "gallery.story.name." + std::to_string(index), names[index], 84.0f, photoH - 53.0f,
                      photoW - 104.0f, 28.0f, 19.0f, 1.0f, kInk);
            }).build();
    }
    label(ui, "gallery.story.counter", "0" + std::to_string(active + 1) + " / 06", width * 0.08f, height * 0.84f,
          width * 0.84f, 24.0f, 15.0f, opacity * intro, kSubtle, eui::HorizontalAlign::Center);
}

void finalScene(eui::Ui& ui, float width, float height, float t) {
    const float opacity = appear(t, 84.126f, 0.55f);
    ui.rect("final.black").size(width, height).color({0.015f, 0.015f, 0.018f, 1.0f}).opacity(opacity).build();
    const float iconSize = std::min(width * 0.065f, 88.0f);
    ui.image("final.icon").position((width - iconSize) * 0.5f, height * 0.16f).size(iconSize, iconSize)
        .source("assets/icon.svg").contain().opacity(opacity)
        .translateZ((1.0f - opacity) * -160.0f).rotateY((1.0f - opacity) * -0.24f).perspective(900.0f)
        .transition(transition(0.55f)).build();
    bigWord(ui, "final.brand", "EUI-NEO", width, height * 0.26f, height * 0.30f, opacity, 0.0f, 0.92f + opacity * 0.08f,
            {0.96f, 0.97f, 1.0f, 1.0f});
    label(ui, "final.promise", "C++ UI. All platforms.", 0.0f, height * 0.58f, width, 34.0f, 19.0f,
          opacity, {0.74f, 0.76f, 0.80f, 1.0f}, eui::HorizontalAlign::Center);
    label(ui, "final.message", "本视频由 EUI-NEO 制作", 0.0f, height * 0.66f, width, 42.0f, 24.0f,
          opacity * appear(t, 86.9f, 0.30f), {0.96f, 0.97f, 1.0f, 1.0f}, eui::HorizontalAlign::Center);
    label(ui, "final.github", "github.com/sudoevolve/EUI-NEO", 0.0f, height * 0.75f, width, 34.0f, 19.0f,
          opacity * appear(t, 87.679f, 0.30f), {0.74f, 0.76f, 0.80f, 1.0f}, eui::HorizontalAlign::Center);
    ui.rect("final.github.hit").position(width * 0.27f, height * 0.71f).size(width * 0.46f, 48.0f)
        .color({0.0f, 0.0f, 0.0f, 0.0f}).onClick([] {
            eui::platform::openUrl("https://github.com/sudoevolve/EUI-NEO");
        }).build();
    label(ui, "final.credit", "made by eui-neo", 0.0f, height * 0.84f, width, 28.0f, 14.0f,
          opacity * appear(t, 89.466f, 0.20f), {0.58f, 0.60f, 0.64f, 1.0f}, eui::HorizontalAlign::Center);
}

void shaderScene(eui::Ui& ui, float width, float height, float t, float opacity) {
    const float inProgress = appear(t, 40.136f, 0.7f);
    label(ui, "shader.title", "Render in motion.", width * 0.08f, height * 0.08f, width * 0.84f, 92.0f,
          std::min(width * 0.085f, 104.0f), opacity, kInk, eui::HorizontalAlign::Left,
          (1.0f - inProgress) * -width * 0.18f, (1.0f - inProgress) * 36.0f,
          (1.0f - inProgress) * -180.0f, (1.0f - inProgress) * -0.2f, 880.0f);
    ui.shadertoy("shader.canvas")
        .position(width * 0.08f, height * 0.34f)
        .size(width * 0.84f, height * 0.47f)
        .graph([] {
            eui::ShaderToyGraph graph;
            const std::string source = eui::platform::resolveResourcePath("assets/shaders/shadertoy/demo.frag");
            const std::string spirv = eui::platform::resolveResourcePath("assets/shaders/shadertoy/demo.frag.spv");
            graph.addPass("image", source.empty() ? "assets/shaders/shadertoy/demo.frag" : source,
                          spirv.empty() ? "assets/shaders/shadertoy/demo.frag.spv" : spirv);
            return graph;
        }())
        .radius(24.0f).opacity(opacity * inProgress).timeScale(1.0f).build();
    label(ui, "shader.caption", "SHADERTOY · OPENGL / VULKAN", width * 0.08f, height * 0.84f, width * 0.84f, 26.0f, 14.0f,
          opacity * inProgress, kSubtle, eui::HorizontalAlign::Center);
}

void showcaseScene(eui::Ui& ui, float width, float height, float t, float opacity) {
    const auto theme = lightTheme();
    label(ui, "showcase.title", "Made to be composed.", width * 0.07f, height * 0.06f, width * 0.86f, 86.0f,
          std::min(width * 0.075f, 92.0f), opacity, kInk, eui::HorizontalAlign::Left,
          std::sin(t * 0.5f) * 18.0f, std::cos(t * 0.5f) * 8.0f, 32.0f, 0.02f, 900.0f);
    const float baseX = width * 0.09f;
    const float cardW = std::min(width * 0.24f, 300.0f);
    const float y = height * 0.34f;
    ui.stack("showcase.neumorphic.anchor").position(baseX, y).size(cardW, 150.0f)
        .translateZ(std::sin(t * 0.8f) * 18.0f).rotateY(std::sin(t * 0.65f) * 0.06f).perspective(900.0f).content([&] {
            components::workshop::neumorphicButton(ui, "showcase.neo.button")
                .size(cardW, 150.0f).text("Neumorphic").fontSize(17.0f).theme(theme)
                .transition(transition()).onClick([] {}).build();
        }).build();
    ui.stack("showcase.svg.anchor").position(baseX + cardW + 34.0f, y).size(cardW, 150.0f)
        .translateZ(std::sin(t * 0.8f + 1.0f) * 18.0f).rotateY(-std::sin(t * 0.65f + 1.0f) * 0.06f).perspective(900.0f).content([&] {
            whiteCard(ui, "showcase.svg.card", 0.0f, 0.0f, cardW, 150.0f, 1.0f, 22.0f);
            const float svgPulse = 1.0f + 0.08f * std::sin(t * 3.0f);
            ui.rect("showcase.svg.glow").position(cardW * 0.20f, 12.0f).size(cardW * 0.60f, 92.0f)
                .radius(18.0f).color({kBlue.r, kBlue.g, kBlue.b, 0.10f}).build();
            ui.svg("showcase.svg.icon").position(cardW * 0.31f, 18.0f).size(cardW * 0.38f, 82.0f)
                .source(R"svg(<svg viewBox="0 0 24 24" xmlns="http://www.w3.org/2000/svg"><path d="M12 2 21 7v10l-9 5-9-5V7l9-5Z" fill="none" stroke="#0A61E8" stroke-width="1.6"/><path d="m8 12 2.5 2.5L16.5 8" fill="none" stroke="#0A61E8" stroke-width="1.6"/></svg>)svg")
                .contain().scale(svgPulse).rotateY(std::sin(t * 1.8f) * 0.10f).translateZ(32.0f).perspective(700.0f).build();
            label(ui, "showcase.svg.label", "SVG button", 0.0f, 104.0f, cardW, 28.0f, 17.0f, 1.0f, kInk, eui::HorizontalAlign::Center);
            components::button(ui, "showcase.svg.action").position(cardW * 0.31f, 126.0f)
                .size(cardW * 0.38f, 28.0f).text("Tap SVG").fontSize(12.0f).theme(theme)
                .onClick([] {}).build();
        }).build();
    ui.stack("showcase.poster.anchor").position(baseX + (cardW + 34.0f) * 2.0f, y).size(cardW, 150.0f)
        .translateZ(std::sin(t * 0.8f + 2.0f) * 18.0f).rotateY(std::sin(t * 0.65f + 2.0f) * 0.06f).perspective(900.0f).content([&] {
            whiteCard(ui, "showcase.poster.card", 0.0f, 0.0f, cardW, 150.0f, 1.0f, 22.0f);
            const char* posters[] = {"docs/pic/1.jpg", "docs/pic/2.jpg", "docs/pic/3.jpg", "docs/pic/4.jpg", "docs/pic/示例1.jpg", "docs/pic/示例2.jpg"};
            for (int index = 0; index < 6; ++index) {
                const float px = 16.0f + (index % 3) * (cardW * 0.31f);
                const float py = 20.0f + (index / 3) * 55.0f;
                ui.image("showcase.poster." + std::to_string(index)).position(px, py).size(cardW * 0.27f, 42.0f)
                    .source(posters[index]).cover().opacity(0.92f).radius(8.0f).build();
            }
        }).build();
}

void svgWorkshopStory(eui::Ui& ui, PromoState& state, float width, float height, float t, float opacity) {
    const auto theme = lightTheme();
    const float local = std::max(0.0f, t - 57.469f);
    const float intro = appear(t, 57.469f, 0.34f);
    const float autoHeart = smoothStep(58.35f, 59.05f, t);
    const float reveal = smoothStep(59.05f, 60.15f, t);
    const float impact = smoothStep(58.92f, 59.34f, t) * (1.0f - smoothStep(59.34f, 59.82f, t));
    state.workshopLiked = autoHeart > 0.5f;
    state.artworkReveal = reveal;
    label(ui, "svgstory.title", "交互，先于画面。",
          width * 0.08f, height * 0.08f, width * 0.84f, 86.0f, std::min(width * 0.075f, 92.0f), opacity * intro, kInk);
    label(ui, "svgstory.subtitle", "Workshop / SVG in motion",
          width * 0.08f, height * 0.19f, width * 0.78f, 34.0f, 22.0f, opacity * intro, kSubtle);
    const float heart = std::min(width * 0.22f, 250.0f);
    ui.stack("svgstory.heart").position(width * 0.12f, height * 0.39f).size(heart, heart)
        .translateZ((1.0f - intro) * -160.0f).scale(1.0f + impact * 0.08f).rotateY((1.0f - intro) * -0.16f).perspective(900.0f).content([&] {
            whiteCard(ui, "svgstory.heart.card", 0.0f, 0.0f, heart, heart, 1.0f, 28.0f);
            components::workshop::heartSwitch(ui, "svgstory.heart.switch").size(heart * 0.58f, heart * 0.58f)
                .theme(theme).checked(state.workshopLiked).transition(transition(0.32f)).onChange([](bool) {}).build();
            label(ui, "svgstory.heart.caption", "heart-switch", 0.0f, heart * 0.76f, heart, 30.0f, 18.0f, 1.0f, kInk, eui::HorizontalAlign::Center);
            label(ui, "svgstory.heart.hint", "WORKSHOP", 0.0f, heart * 0.87f, heart, 22.0f, 12.0f, 0.70f, kBlue, eui::HorizontalAlign::Center);
        }).build();
    const float artX = width * 0.43f, artY = height * 0.33f, artW = width * 0.47f, artH = height * 0.54f;
    whiteCard(ui, "svgstory.canvas", artX, artY, artW, artH, opacity * reveal, 28.0f,
              (1.0f - reveal) * width * 0.10f, (1.0f - reveal) * 18.0f, (1.0f - reveal) * -180.0f, -0.12f);
    const std::string painting = eui::platform::resolveResourcePath("assets/Hard drive-rafiki.svg");
    // SvgBuilder::source() accepts inline SVG markup; a filesystem path is an image source.
    // Use the image loader so the SVG file is rasterized by the same path-aware decoder.
    ui.image("svgstory.painting").position(artX + artW * 0.05f, artY + artH * 0.08f).size(artW * 0.90f, artH * 0.72f)
        .source(painting.empty() ? "assets/Hard drive-rafiki.svg" : painting).contain().scale(0.82f + reveal * 0.18f).rotateY(std::sin(local * 0.8f) * 0.03f)
        .translateZ(42.0f).perspective(900.0f).opacity(opacity * reveal).transition(transition(0.46f)).build();
    const std::string blossom = R"svg(<svg viewBox="0 0 360 240" xmlns="http://www.w3.org/2000/svg"><defs><linearGradient id="p" x2="1" y2="1"><stop stop-color="#ffd66e"/><stop offset="1" stop-color="#ff6f83"/></linearGradient></defs><rect width="360" height="240" rx="26" fill="url(#p)"/><path d="M35 180 Q100 35 180 165 T325 65" fill="none" stroke="#fff" stroke-width="12"/><circle cx="100" cy="75" r="24" fill="#fff" opacity=".75"/><circle cx="270" cy="180" r="34" fill="#754dff" opacity=".6"/></svg>)svg";
    const std::string orbit = R"svg(<svg viewBox="0 0 360 240" xmlns="http://www.w3.org/2000/svg"><defs><linearGradient id="o" x2="1" y2="1"><stop stop-color="#0e182b"/><stop offset="1" stop-color="#11b88c"/></linearGradient></defs><rect width="360" height="240" rx="26" fill="url(#o)"/><path d="M38 180 L100 55 L170 180 L235 55 L325 180" fill="none" stroke="#b9ffe6" stroke-width="12"/><circle cx="180" cy="120" r="40" fill="#fff" opacity=".18"/></svg>)svg";
    ui.svg("svgstory.thumb1").position(artX + artW * 0.08f, artY + artH * 0.84f).size(artW * 0.25f, artH * 0.13f).source(blossom).contain().opacity(opacity * reveal).build();
    ui.svg("svgstory.thumb2").position(artX + artW * 0.38f, artY + artH * 0.84f).size(artW * 0.25f, artH * 0.13f).source(orbit).contain().opacity(opacity * reveal).build();
    label(ui, "svgstory.caption", "SVG · paths · gradients · motion", artX + artW * 0.62f, artY + artH * 0.87f, artW * 0.32f, 24.0f, 13.0f, opacity * reveal, kBlue, eui::HorizontalAlign::Right);
}

void idle(eui::Ui& ui, PromoState& state, float width, float height) {
    label(ui, "idle.brand", "EUI-NEO", 0.0f, height * 0.26f, width, 100.0f, std::min(width * 0.115f, 140.0f), 1.0f, kInk, eui::HorizontalAlign::Center);
    label(ui, "idle.sub", "C++ UI, BUILT TO MOVE.", 0.0f, height * 0.42f, width, 30.0f, 15.0f, 1.0f, kSubtle, eui::HorizontalAlign::Center);
    const auto theme = lightTheme();
    components::button(ui, "idle.start").position((width - 230.0f) * 0.5f, height * 0.58f).size(230.0f, 58.0f)
        .text(state.ended ? "Replay experience" : "Start experience").fontSize(16.0f).theme(theme).transition(transition())
        .onClick([&state] {
            if (!state.audio.loaded() && !state.audio.load(musicPath())) {
                state.error = state.audio.error();
                return;
            }
            state.audio.stop();
            state.audio.play();
            state.error.clear();
            state.started = true;
            state.ended = false;
            state.finaleHold = 0.0f;
        }).build();
    if (!state.error.empty()) {
        label(ui, "idle.error", state.error, width * 0.16f, height * 0.69f, width * 0.68f, 28.0f, 12.0f, 1.0f, kOrange, eui::HorizontalAlign::Center);
    }
}

} // namespace

void compose(eui::Ui& ui, const eui::Screen& screen) {
    PromoState& state = ui.state<PromoState>("promo.state");
    const float t = state.started ? static_cast<float>(state.audio.positionSeconds()) : 0.0f;
    if (state.started && (state.audio.finished() || t >= kEnd)) {
        state.audio.stop();
        state.started = false;
        state.ended = true;
        state.finaleHold = 0.0f;
    }

    // `onFrame` makes Runtime continuously request compose/paint work.  Keep the
    // idle landing page event-driven; only the actual film and its 3 s outro
    // hold need a ticking frame source.
    const bool holdingFinale = state.ended && state.finaleHold < 3.0f;
    const bool needsAnimationFrame = state.started || holdingFinale;

    auto root = ui.stack("promo.root").size(screen.width, screen.height).content([&] {
        ui.rect("promo.paper").size(screen.width, screen.height).color(kPaper).build();
        if (!state.started && !holdingFinale) {
            idle(ui, state, screen.width, screen.height);
            return;
        }
        if (holdingFinale) {
            finalScene(ui, screen.width, screen.height, kEnd + state.finaleHold);
            return;
        }
        const float openingOpacity = scene(t, 0.0f, 7.454f);
        const float codeOpacity = scene(t, 7.454f, 14.791f);
        const float controlsOpacity = scene(t, 14.791f, 29.025f);
        const float motionOpacity = scene(t, 29.025f, 40.136f, 0.32f);
        const float shaderOpacity = scene(t, 40.136f, 47.300f, 0.32f);
        const float dataOpacity = scene(t, 47.300f, 57.469f, 0.32f);
        const float showcaseOpacity = scene(t, 57.469f, 64.575f, 0.32f);
        const float platformOpacity = scene(t, 64.575f, 74.000f, 0.32f);
        const float galleryOpacity = scene(t, 74.000f, 84.126f, 0.32f);
        if (openingOpacity > 0.001f) opening(ui, screen.width, screen.height, t);
        if (codeOpacity > 0.001f) codeScene(ui, screen.width, screen.height, t, codeOpacity);
        if (controlsOpacity > 0.001f) productControls(ui, state, screen.width, screen.height, t, controlsOpacity);
        if (motionOpacity > 0.001f) advantagesScene(ui, screen.width, screen.height, t, motionOpacity);
        if (dataOpacity > 0.001f) dataSceneEnhanced(ui, screen.width, screen.height, t, dataOpacity);
        if (shaderOpacity > 0.001f) shaderScene(ui, screen.width, screen.height, t, shaderOpacity);
        if (showcaseOpacity > 0.001f) svgWorkshopStory(ui, state, screen.width, screen.height, t, showcaseOpacity);
        if (platformOpacity > 0.001f) platforms(ui, screen.width, screen.height, t, platformOpacity);
        if (galleryOpacity > 0.001f) galleryStrip(ui, screen.width, screen.height, t, galleryOpacity);
        if (t >= 84.126f) finalScene(ui, screen.width, screen.height, t);
    });

    if (needsAnimationFrame) {
        root.onFrame([&state](float deltaSeconds) {
            const float target = state.workshopLiked ? 1.0f : 0.0f;
            const float speed = state.workshopLiked ? 2.4f : 3.8f;
            state.artworkReveal += (target - state.artworkReveal) * std::min(1.0f, std::max(0.0f, deltaSeconds) * speed);
            if (state.ended && state.finaleHold < 3.0f) {
                state.finaleHold = std::min(3.0f, state.finaleHold + std::max(0.0f, deltaSeconds));
            }
        });
    }
    root.build();
}

} // namespace app
