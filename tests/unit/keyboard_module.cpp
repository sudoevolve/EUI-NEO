#include "modules/keyboard/keyboard.h"

#include <cassert>
#include <string>

int main() {
    modules::keyboard::KeyboardOptions options;
    options.mode = modules::keyboard::KeyboardMode::Symbols;
    options.theme = modules::keyboard::KeyboardTheme::Night;
    options.onText = [](const std::string&) {};
    options.onAction = [](modules::keyboard::KeyboardAction) {};

    modules::keyboard::KeyboardController controller;
    assert(!controller.visible());
    controller.show(options);
    assert(controller.visible());
    assert(controller.mode() == modules::keyboard::KeyboardMode::Symbols);
    controller.setMode(modules::keyboard::KeyboardMode::MoreSymbols);
    assert(controller.mode() == modules::keyboard::KeyboardMode::MoreSymbols);
    controller.setMode(modules::keyboard::KeyboardMode::Letters);
    assert(controller.mode() == modules::keyboard::KeyboardMode::Letters);
    assert(controller.uppercase());
    controller.setUppercase(false);
    assert(!controller.uppercase());
    controller.hide();
    assert(!controller.visible());

    const core::Color accent{0.92f, 0.28f, 0.46f, 1.0f};
    const auto light = modules::keyboard::KeyboardStyle::light(accent);
    const auto night = modules::keyboard::KeyboardStyle::night(accent);
    assert(light.key.a > 0.0f);
    assert(night.key.a > 0.0f);
    assert(light.primaryKey.r == accent.r);
    assert(night.primaryKey.g == accent.g);

    std::string single;
    std::string multi;
    modules::keyboard::KeyboardPanelController panel({
        {"single.input", single},
        {"multi.input", multi, true},
    });
    panel.bindInput("single.input", single);
    panel.setAppearance(modules::keyboard::KeyboardTheme::Light, accent);
    return 0;
}
