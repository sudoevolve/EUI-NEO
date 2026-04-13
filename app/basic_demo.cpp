#include "app/DslAppRuntime.h"
#include <algorithm>

int main() {
    EUINEO::DslAppConfig config;
    config.title = "EUI Basic Contact Demo";
    config.width = 960;
    config.height = 640;
    config.pageId = "basic_demo";
    config.fps = 120;
    config.darkTitleBar = true;

    return EUINEO::RunDslApp(config, [](EUINEO::UIContext& ui, const EUINEO::RectFrame& screen) {
        EUINEO::UseDslLightTheme(EUINEO::Color(0.0f, 0.0f, 0.0f, 1.0f));

        const float cardW = std::min(520.0f, screen.width - 32.0f);
        const float cardH = 240.0f;
        const auto composeCentered = [&](const auto& centerContent) {
            ui.column()
                .position(0.0f, 0.0f)
                .size(screen.width, screen.height)
                .padding(16.0f)
                .justifyContent(EUINEO::MainAxisAlignment::Center)
                .alignItems(EUINEO::CrossAxisAlignment::Center)
                .content([&] {
                    centerContent();
                });
        };

        ui.panel("basic.stage")
            .position(0.0f, 0.0f)
            .size(screen.width, screen.height)
            .background(EUINEO::Color(0.07f, 0.07f, 0.09f, 1.0f))
            .build();

        composeCentered([&] {
            ui.panel("basic.card")
                .width(cardW)
                .height(cardH)
                .rounding(24.0f)
                .background(EUINEO::Color(0.09f, 0.09f, 0.11f, 1.0f))
                .border(1.0f, EUINEO::Color(0.22f, 0.22f, 0.28f, 1.0f))
                .build();
        });

        composeCentered([&] {
            ui.column()
                .width(cardW)
                .height(cardH)
                .padding(34.0f, 28.0f)
                .gap(12.0f)
                .content([&] {
                    ui.row()
                        .height(28.0f)
                        .content([&] {
                            ui.label("basic.title")
                                .fontSize(22.0f)
                                .color(EUINEO::Color(0.70f, 0.72f, 0.78f, 1.0f))
                                .text("Email")
                                .build();
                        });

                    ui.row()
                        .height(56.0f)
                        .content([&] {
                            ui.label("basic.email")
                                .fontSize(38.0f)
                                .color(EUINEO::Color(0.97f, 0.97f, 0.98f, 1.0f))
                                .text("sudoevolve@gmail.com")
                                .build();
                        });

                    ui.label("basic.card.spacer")
                        .flex(1.0f)
                        .text("")
                        .build();

                    ui.row()
                        .height(62.0f)
                        .justifyContent(EUINEO::MainAxisAlignment::End)
                        .content([&] {
                            ui.button("basic.github.button")
                                .size(72.0f, 62.0f)
                                .rounding(18.0f)
                                .style(EUINEO::ButtonStyle::Primary)
                                .icon("\xE2\x86\x97")
                                .fontSize(32.0f)
                                .onClick([]() {
                                    EUINEO::OpenDslUrl("https://github.com/sudoevolve");
                                })
                                .build();
                        });
                });
        });
    });
}
