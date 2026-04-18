#include "app/DslAppRuntime.h"
#include "calculator_logic.h"
#include <algorithm>
#include <functional>

int main() {
    EUINEO::DslAppConfig config;
    config.title = "EUI Calculator Demo";
    config.width = 450;
    config.height = 760;
    config.pageId = "calculator_demo";
    config.fps = 120;
    config.darkTitleBar = true;

    return EUINEO::RunDslApp(config, [](EUINEO::UIContext& ui, const EUINEO::RectFrame& screen) {
        static EUINEO::CalculatorLogic calc;
        const float panelWidth = std::min(432.0f, screen.width - 24.0f);
        const float panelHeight = std::min(792.0f, screen.height - 24.0f);
        const float keyHeight = 84.0f;
        const EUINEO::Color stageBg(0.00f, 0.00f, 0.00f, 1.0f);
        const EUINEO::Color cardBg(0.00f, 0.00f, 0.00f, 1.0f);
        const EUINEO::Color displayText(0.98f, 0.98f, 0.98f, 1.0f);
        const EUINEO::Color secondaryText(0.42f, 0.42f, 0.46f, 1.0f);
        const EUINEO::Color normalKeyBg(0.17f, 0.17f, 0.18f, 1.0f);
        const EUINEO::Color normalKeyBorder(0.23f, 0.23f, 0.24f, 1.0f);
        const EUINEO::Color functionKeyBg(0.40f, 0.40f, 0.41f, 1.0f);
        const EUINEO::Color functionKeyBorder(0.56f, 0.56f, 0.57f, 1.0f);
        const EUINEO::Color operatorKeyBg(0.98f, 0.63f, 0.19f, 1.0f);
        const EUINEO::Color operatorKeyBorder(1.00f, 0.75f, 0.35f, 1.0f);
        const EUINEO::CalculatorState& state = calc.state();
        const auto composeKey = [&](const std::string& keyId, float keyFlex, const std::string& text,
                                    float fontSize, const EUINEO::Color& textColor,
                                    const EUINEO::Color& fillColor,
                                    const EUINEO::Color& borderColor,
                                    const std::function<void()>& onClick) {
            ui.column()
                .flex(keyFlex)
                .height(keyHeight)
                .content([&] {
                    ui.button(keyId)
                        .height(keyHeight)
                        .rounding(22.0f)
                        .style(EUINEO::ButtonStyle::Outline)
                        .background(fillColor)
                        .border(1.0f, borderColor)
                        .text(text)
                        .fontSize(fontSize)
                        .textColor(textColor)
                        .hoverScale(1.0f, 1.03f, 0.14f)
                        .onClick(onClick)
                        .build();
                });
        };
        const auto row = [&](const std::function<void()>& content) {
            ui.row()
                .height(keyHeight)
                .gap(12.0f)
                .content(content);
        };
        const auto composeCentered = [&](const std::function<void()>& content) {
            ui.column()
                .position(0.0f, 0.0f)
                .size(screen.width, screen.height)
                .justifyContent(EUINEO::MainAxisAlignment::Center)
                .alignItems(EUINEO::CrossAxisAlignment::Center)
                .content(content);
        };

        ui.panel("stage")
            .position(0.0f, 0.0f)
            .size(screen.width, screen.height)
            .background(stageBg)
            .build();

        composeCentered([&] {
            ui.panel("calc.card")
                .size(panelWidth, panelHeight)
                .rounding(24.0f)
                .background(cardBg)
                .build();
        });

        composeCentered([&] {
            ui.column()
                .size(panelWidth, panelHeight)
                .padding(14.0f, 18.0f)
                .gap(10.0f)
                .content([&] {
                ui.row()
                    .height(40.0f)
                    .justifyContent(EUINEO::MainAxisAlignment::End)
                    .alignItems(EUINEO::CrossAxisAlignment::Center)
                    .content([&] {
                        ui.button("calc.top.undo")
                            .size(48.0f, 36.0f)
                            .rounding(12.0f)
                            .style(EUINEO::ButtonStyle::Outline)
                            .background(normalKeyBg)
                            .border(1.0f, normalKeyBorder)
                            .text("<")
                            .fontSize(22.0f)
                            .textColor(displayText)
                            .hoverScale(1.0f, 1.03f, 0.14f)
                            .onClick([&]() { calc.backspace(); })
                            .build();
                    });

                ui.row()
                    .height(44.0f)
                    .justifyContent(EUINEO::MainAxisAlignment::End)
                    .alignItems(EUINEO::CrossAxisAlignment::Center)
                    .content([&] {
                        ui.label("calc.history.left")
                            .text(state.historyLeft)
                            .fontSize(18.0f)
                            .color(secondaryText)
                            .build();

                        ui.label("calc.history.op")
                            .margin(8.0f, 0.0f, 8.0f, 0.0f)
                            .text(state.historyOp)
                            .fontSize(22.0f)
                            .color(operatorKeyBg)
                            .build();

                        ui.label("calc.history.right")
                            .text(state.historyRight)
                            .fontSize(18.0f)
                            .color(secondaryText)
                            .build();
                    });

                ui.row()
                    .height(100.0f)
                    .justifyContent(EUINEO::MainAxisAlignment::End)
                    .alignItems(EUINEO::CrossAxisAlignment::Center)
                    .content([&] {
                        ui.label("calc.display.value")
                            .text(state.input)
                            .fontSize(62.0f)
                            .color(displayText)
                            .build();
                    });

                ui.panel("calc.split")
                    .height(2.0f)
                    .background(EUINEO::Color(0.07f, 0.07f, 0.08f, 1.0f))
                    .build();

                ui.column()
                    .flex(1.0f)
                    .gap(10.0f)
                    .content([&] {
                        row([&] {
                            composeKey("calc.key.ac", 1.0f, "AC", 40.0f, displayText, functionKeyBg, functionKeyBorder,
                                       [&]() { calc.clearAll(); });
                            composeKey("calc.key.sign", 1.0f, "\xC2\xB1", 40.0f, displayText, functionKeyBg, functionKeyBorder,
                                       [&]() { calc.toggleSign(); });
                            composeKey("calc.key.percent", 1.0f, "%", 40.0f, displayText, functionKeyBg, functionKeyBorder,
                                       [&]() { calc.percent(); });
                            composeKey("calc.key.divide", 1.0f, "\xC3\xB7", 40.0f, displayText, operatorKeyBg, operatorKeyBorder,
                                       [&]() { calc.inputOperator('/'); });
                        });

                        row([&] {
                            composeKey("calc.key.7", 1.0f, "7", 38.0f, displayText, normalKeyBg, normalKeyBorder,
                                       [&]() { calc.inputDigit("7"); });
                            composeKey("calc.key.8", 1.0f, "8", 38.0f, displayText, normalKeyBg, normalKeyBorder,
                                       [&]() { calc.inputDigit("8"); });
                            composeKey("calc.key.9", 1.0f, "9", 38.0f, displayText, normalKeyBg, normalKeyBorder,
                                       [&]() { calc.inputDigit("9"); });
                            composeKey("calc.key.mul", 1.0f, "\xC3\x97", 38.0f, displayText, operatorKeyBg, operatorKeyBorder,
                                       [&]() { calc.inputOperator('*'); });
                        });

                        row([&] {
                            composeKey("calc.key.4", 1.0f, "4", 38.0f, displayText, normalKeyBg, normalKeyBorder,
                                       [&]() { calc.inputDigit("4"); });
                            composeKey("calc.key.5", 1.0f, "5", 38.0f, displayText, normalKeyBg, normalKeyBorder,
                                       [&]() { calc.inputDigit("5"); });
                            composeKey("calc.key.6", 1.0f, "6", 38.0f, displayText, normalKeyBg, normalKeyBorder,
                                       [&]() { calc.inputDigit("6"); });
                            composeKey("calc.key.sub", 1.0f, "-", 40.0f, displayText, operatorKeyBg, operatorKeyBorder,
                                       [&]() { calc.inputOperator('-'); });
                        });

                        row([&] {
                            composeKey("calc.key.1", 1.0f, "1", 38.0f, displayText, normalKeyBg, normalKeyBorder,
                                       [&]() { calc.inputDigit("1"); });
                            composeKey("calc.key.2", 1.0f, "2", 38.0f, displayText, normalKeyBg, normalKeyBorder,
                                       [&]() { calc.inputDigit("2"); });
                            composeKey("calc.key.3", 1.0f, "3", 38.0f, displayText, normalKeyBg, normalKeyBorder,
                                       [&]() { calc.inputDigit("3"); });
                            composeKey("calc.key.add", 1.0f, "+", 40.0f, displayText, operatorKeyBg, operatorKeyBorder,
                                       [&]() { calc.inputOperator('+'); });
                        });

                        row([&] {
                            composeKey("calc.key.0", 2.0f, "0", 38.0f, displayText, normalKeyBg, normalKeyBorder,
                                       [&]() { calc.inputDigit("0"); });
                            composeKey("calc.key.dot", 1.0f, ".", 40.0f, displayText, normalKeyBg, normalKeyBorder,
                                       [&]() { calc.inputDot(); });
                            composeKey("calc.key.eq", 1.0f, "=", 40.0f, displayText, operatorKeyBg, operatorKeyBorder,
                                       [&]() { calc.evaluate(); });
                        });
                    });
            });
        });
    });
}
