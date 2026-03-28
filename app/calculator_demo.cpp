#include "app/DslAppRuntime.h"
#include "calculator_logic.h"
#include <algorithm>
#include <functional>

int main() {
    EUINEO::DslAppConfig config;
    config.title = "EUI Calculator Demo";
    config.width = 480;
    config.height = 830;
    config.pageId = "calculator_demo";
    config.fps = 120;

    return EUINEO::RunDslApp(config, [](EUINEO::UIContext& ui, const EUINEO::RectFrame& screen) {
        static EUINEO::CalculatorLogic calc;
        const float panelWidth = std::min(480.0f, screen.width - 24.0f);
        const float panelHeight = std::min(900.0f, screen.height - 24.0f);
        const float panelX = (screen.width - panelWidth) * 0.5f;
        const float panelY = (screen.height - panelHeight) * 0.5f;
        const float keyHeight = 94.0f;
        const EUINEO::Color cyanText(0.00f, 0.90f, 1.00f, 1.0f);
        const EUINEO::Color redText(0.95f, 0.32f, 0.32f, 1.0f);
        const EUINEO::Color normalText(0.95f, 0.97f, 1.0f, 1.0f);
        const EUINEO::CalculatorState& state = calc.state();

        const auto composeKey = [&](const std::string& keyId,
                                    float keyFlex,
                                    const std::string& text,
                                    float fontSize,
                                    const EUINEO::Color& textColor,
                                    bool,
                                    const std::function<void()>& onClick) {
            ui.column()
                .flex(keyFlex)
                .height(keyHeight)
                .content([&] {
                    auto buttonBuilder = ui.button(keyId)
                        .height(keyHeight)
                        .rounding(24.0f)
                        .text(text)
                        .fontSize(fontSize)
                        .textColor(textColor)
                        .hoverScale(1.0f, 1.03f, 0.14f)
                        .onClick(onClick);

                    buttonBuilder.build();
                });
        };

        ui.panel("stage")
            .position(0.0f, 0.0f)
            .size(screen.width, screen.height)
            .background(EUINEO::Color(0.10f, 0.13f, 0.18f, 1.0f))
            .build();

        ui.panel("calc.card")
            .position(panelX, panelY)
            .size(panelWidth, panelHeight)
            .rounding(28.0f)
            .background(EUINEO::Color(0.13f, 0.17f, 0.23f, 0.98f))
            .build();

        ui.column()
            .position(panelX, panelY)
            .size(panelWidth, panelHeight)
            .padding(16.0f, 20.0f)
            .gap(12.0f)
            .content([&] {
                ui.row()
                    .height(44.0f)
                    .content([&] {
                        ui.label("calc.top.spacer")
                            .flex(1.0f)
                            .text("")
                            .build();

                        ui.column()
                            .width(52.0f)
                            .height(40.0f)
                            .content([&] {
                                ui.button("calc.top.undo")
                                    .height(40.0f)
                                    .rounding(12.0f)
                                    .text("<")
                                    .fontSize(24.0f)
                                    .textColor(normalText)
                                    .hoverScale(1.0f, 1.03f, 0.14f)
                                    .onClick([&]() { calc.backspace(); })
                                    .build();
                            });
                    });

                ui.row()
                    .height(50.0f)
                    .content([&] {
                        ui.label("calc.history.spacer")
                            .flex(1.0f)
                            .text("")
                            .build();

                        ui.label("calc.history.left")
                            .text(state.historyLeft)
                            .fontSize(20.0f)
                            .color(EUINEO::Color(0.55f, 0.60f, 0.67f, 1.0f))
                            .build();

                        ui.label("calc.history.op")
                            .margin(10.0f, 0.0f, 10.0f, 0.0f)
                            .text(state.historyOp)
                            .fontSize(24.0f)
                            .color(EUINEO::Color(0.95f, 0.32f, 0.32f, 1.0f))
                            .build();

                        ui.label("calc.history.right")
                            .text(state.historyRight)
                            .fontSize(20.0f)
                            .color(EUINEO::Color(0.55f, 0.60f, 0.67f, 1.0f))
                            .build();
                    });

                ui.row()
                    .height(112.0f)
                    .content([&] {
                        ui.label("calc.display.spacer")
                            .flex(1.0f)
                            .text("")
                            .build();

                        ui.label("calc.display.value")
                            .text(state.input)
                            .fontSize(70.0f)
                            .color(EUINEO::Color(0.98f, 0.99f, 1.0f, 1.0f))
                            .build();
                    });

                ui.panel("calc.split")
                    .height(2.0f)
                    .background(EUINEO::Color(0.20f, 0.24f, 0.30f, 1.0f))
                    .build();

                ui.column()
                    .flex(1.0f)
                    .gap(12.0f)
                    .content([&] {
                        ui.row()
                            .height(keyHeight)
                            .gap(12.0f)
                            .content([&] {
                                composeKey("calc.key.ac", 1.0f, "AC", 44.0f, cyanText, false, [&]() { calc.clearAll(); });
                                composeKey("calc.key.sign", 1.0f, "\xC2\xB1", 44.0f, cyanText, false, [&]() { calc.toggleSign(); });
                                composeKey("calc.key.percent", 1.0f, "%", 44.0f, cyanText, false, [&]() { calc.percent(); });
                                composeKey("calc.key.divide", 1.0f, "\xC3\xB7", 44.0f, redText, false, [&]() { calc.inputOperator('/'); });
                            });

                        ui.row()
                            .height(keyHeight)
                            .gap(12.0f)
                            .content([&] {
                                composeKey("calc.key.7", 1.0f, "7", 42.0f, normalText, false, [&]() { calc.inputDigit("7"); });
                                composeKey("calc.key.8", 1.0f, "8", 42.0f, normalText, false, [&]() { calc.inputDigit("8"); });
                                composeKey("calc.key.9", 1.0f, "9", 42.0f, normalText, false, [&]() { calc.inputDigit("9"); });
                                composeKey("calc.key.mul", 1.0f, "\xC3\x97", 42.0f, redText, false, [&]() { calc.inputOperator('*'); });
                            });

                        ui.row()
                            .height(keyHeight)
                            .gap(12.0f)
                            .content([&] {
                                composeKey("calc.key.4", 1.0f, "4", 42.0f, normalText, false, [&]() { calc.inputDigit("4"); });
                                composeKey("calc.key.5", 1.0f, "5", 42.0f, normalText, false, [&]() { calc.inputDigit("5"); });
                                composeKey("calc.key.6", 1.0f, "6", 42.0f, normalText, false, [&]() { calc.inputDigit("6"); });
                                composeKey("calc.key.sub", 1.0f, "-", 44.0f, redText, false, [&]() { calc.inputOperator('-'); });
                            });

                        ui.row()
                            .height(keyHeight)
                            .gap(12.0f)
                            .content([&] {
                                composeKey("calc.key.1", 1.0f, "1", 42.0f, normalText, false, [&]() { calc.inputDigit("1"); });
                                composeKey("calc.key.2", 1.0f, "2", 42.0f, normalText, false, [&]() { calc.inputDigit("2"); });
                                composeKey("calc.key.3", 1.0f, "3", 42.0f, normalText, false, [&]() { calc.inputDigit("3"); });
                                composeKey("calc.key.add", 1.0f, "+", 44.0f, redText, false, [&]() { calc.inputOperator('+'); });
                            });

                        ui.row()
                            .height(keyHeight)
                            .gap(12.0f)
                            .content([&] {
                                composeKey("calc.key.0", 2.0f, "0", 42.0f, normalText, false, [&]() { calc.inputDigit("0"); });
                                composeKey("calc.key.dot", 1.0f, ".", 44.0f, normalText, false, [&]() { calc.inputDot(); });
                                composeKey("calc.key.eq", 1.0f, "=", 44.0f, normalText, true, [&]() { calc.evaluate(); });
                            });
                    });
            });
    });
}
