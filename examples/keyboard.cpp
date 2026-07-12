#include "eui_neo.h"
#include "modules/keyboard/keyboard.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace app {

const DslAppConfig& dslAppConfig() {
    static const DslAppConfig config = DslAppConfig{}
        .title("Keyboard Example")
        .pageId("keyboard")
        .clearColor({0.07f, 0.08f, 0.10f, 1.0f})
        .windowSize(1180, 820)
        .fps(90.0);
    return config;
}

namespace {

bool darkMode = true;
eui::Color accentColor = components::theme::defaultPrimary();
std::string singleText = "Tap here";
std::string multiText = "Multiline input";

modules::keyboard::KeyboardPanelController keyboard({
    {"single.input", singleText},
    {"multi.input", multiText, true},
});

components::theme::ThemeColorTokens theme() {
    auto tokens = darkMode ? components::theme::dark() : components::theme::light();
    tokens.primary = accentColor;
    return tokens;
}

eui::Transition motion() {
    auto transition = eui::Transition::make(0.18f, eui::Ease::OutCubic);
    transition.animate(eui::AnimProperty::Color | eui::AnimProperty::TextColor |
                       eui::AnimProperty::Border | eui::AnimProperty::Shadow);
    return transition;
}

} // namespace

void compose(eui::Ui& ui, const eui::Screen& screen) {
    keyboard.setAppearance(
        darkMode ? modules::keyboard::KeyboardTheme::Night : modules::keyboard::KeyboardTheme::Light,
        accentColor);

    const auto tokens = theme();
    const auto page = components::theme::pageVisuals(tokens);
    const float panelX = 34.0f;
    const float panelY = 34.0f;
    const float panelW = std::min(760.0f, std::max(320.0f, screen.width - 68.0f));
    const float panelH = std::min(380.0f, std::max(320.0f, screen.height - 68.0f));
    const float innerX = panelX + 24.0f;
    const float innerW = std::max(260.0f, panelW - 48.0f);
    const eui::Transition transition = motion();

    ui.stack("root")
        .size(screen.width, screen.height)
        .content([&] {
            ui.rect("root.bg")
                .size(screen.width, screen.height)
                .color(tokens.background)
                .transition(transition)
                .animate(eui::AnimProperty::Color)
                .build();

            ui.rect("inputs.panel")
                .position(panelX, panelY)
                .size(panelW, panelH)
                .color(tokens.surface)
                .radius(18.0f)
                .border(1.0f, components::theme::withOpacity(tokens.border, 0.72f))
                .shadow(22.0f, 0.0f, 8.0f, darkMode ? eui::Color{0.0f, 0.0f, 0.0f, 0.30f}
                                                     : eui::Color{0.10f, 0.14f, 0.22f, 0.16f})
                .transition(transition)
                .animate(eui::AnimProperty::Color | eui::AnimProperty::Border | eui::AnimProperty::Shadow)
                .build();

            ui.text("title")
                .position(innerX, panelY + 24.0f)
                .size(innerW, 38.0f)
                .text("Keyboard Example")
                .fontSize(30.0f)
                .lineHeight(36.0f)
                .fontWeight(840)
                .color(tokens.primary)
                .transition(transition)
                .animate(eui::AnimProperty::TextColor)
                .build();

            components::button(ui, "toolbar.theme")
                .position(innerX, panelY + 80.0f)
                .size(132.0f, 44.0f)
                .text(darkMode ? "Light" : "Night")
                .theme(tokens, false)
                .fontSize(15.0f)
                .transition(transition)
                .onClick([] {
                    darkMode = !darkMode;
                })
                .build();

            const eui::Color swatches[] = {
                components::theme::defaultPrimary(),
                {0.12f, 0.72f, 0.78f, 1.0f},
                {0.92f, 0.28f, 0.46f, 1.0f},
            };
            for (int i = 0; i < 3; ++i) {
                const eui::Color color = swatches[i];
                const float x = innerX + 154.0f + static_cast<float>(i) * 48.0f;
                const bool active = std::fabs(color.r - accentColor.r) < 0.01f &&
                                    std::fabs(color.g - accentColor.g) < 0.01f &&
                                    std::fabs(color.b - accentColor.b) < 0.01f;
                ui.rect("toolbar.color.ring." + std::to_string(i))
                    .position(x - 3.0f, panelY + 77.0f)
                    .size(38.0f, 38.0f)
                    .color(active ? tokens.primary : components::theme::withOpacity(tokens.border, 0.56f))
                    .radius(13.0f)
                    .transition(transition)
                    .animate(eui::AnimProperty::Color)
                    .build();
                ui.rect("toolbar.color." + std::to_string(i))
                    .position(x, panelY + 80.0f)
                    .size(32.0f, 32.0f)
                    .states(color,
                            eui::mixColor(color, eui::Color{1.0f, 1.0f, 1.0f, 1.0f}, 0.16f),
                            eui::mixColor(color, eui::Color{0.0f, 0.0f, 0.0f, 1.0f}, 0.16f))
                    .radius(10.0f)
                    .onClick([color] {
                        accentColor = color;
                    })
                    .build();
            }

            ui.text("single.label")
                .position(innerX, panelY + 142.0f)
                .size(innerW, 22.0f)
                .text("Single line")
                .fontSize(15.0f)
                .lineHeight(20.0f)
                .fontWeight(740)
                .color(page.subtitleColor)
                .transition(transition)
                .animate(eui::AnimProperty::TextColor)
                .build();

            components::input(ui, "single.input")
                .position(innerX, panelY + 170.0f)
                .size(innerW, 52.0f)
                .value(singleText)
                .placeholder("Type here")
                .theme(tokens)
                .transition(transition)
                .onChange([](const std::string& value) {
                    singleText = value;
                })
                .build();

            ui.text("multi.label")
                .position(innerX, panelY + 232.0f)
                .size(innerW, 22.0f)
                .text("Multiline")
                .fontSize(15.0f)
                .lineHeight(20.0f)
                .fontWeight(740)
                .color(page.subtitleColor)
                .transition(transition)
                .animate(eui::AnimProperty::TextColor)
                .build();

            components::input(ui, "multi.input")
                .position(innerX, panelY + 260.0f)
                .size(innerW, std::max(96.0f, panelH - 284.0f))
                .value(multiText)
                .placeholder("Write notes")
                .multiline(true)
                .theme(tokens)
                .transition(transition)
                .onChange([](const std::string& value) {
                    multiText = value;
                })
                .build();

            keyboard.compose(ui, "keyboard.panel", screen.width, screen.height);
        })
        .build();
}

} // namespace app
