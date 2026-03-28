#include "app/DslAppRuntime.h"

int main() {
    EUINEO::DslAppConfig config;
    config.title = "EUI-Basic Demo";
    config.width = 800;
    config.height = 600;
    config.pageId = "basic_demo";
    config.fps = 120;

    return EUINEO::RunDslApp(config, [](EUINEO::UIContext& ui, const EUINEO::RectFrame& screen) {
        const float cardWidth = 460.0f;
        const float cardHeight = 260.0f;
        const float cardX = (screen.width - cardWidth) * 0.5f;
        const float cardY = (screen.height - cardHeight) * 0.5f;

        const float triangleWidth = 150.0f;
        const float triangleHeight = 120.0f;
        const float contentPaddingX = 44.0f;
        const float contentPaddingY = 70.0f;
        const float contentGap = 36.0f;

        const std::string titleText = "EUI-Demo";
        const float titleFontSize = 40.0f;

        const std::string subtitleText = "sudoevolve@gmail.com";
        const float subtitleFontSize = 24.0f;

        ui.panel("stage")
            .position(0.0f, 0.0f)
            .size(screen.width, screen.height)
            .background(EUINEO::Color(0.08f, 0.10f, 0.14f, 1.0f))
            .build();

        ui.panel("card")
            .position(cardX, cardY)
            .size(cardWidth, cardHeight)
            .rounding(18.0f)
            .background(EUINEO::Color(0.18f, 0.24f, 0.35f, 0.94f))
            .build();

        ui.row()
            .position(cardX, cardY)
            .size(cardWidth, cardHeight)
            .padding(contentPaddingX, contentPaddingY)
            .gap(contentGap)
            .content([&] {
                ui.polygon("triangle")
                    .width(triangleWidth)
                    .height(triangleHeight)
                    .background(EUINEO::Color(0.34f, 0.69f, 0.95f, 1.0f))
                    .points({
                        EUINEO::Point2{0.0f, 1.0f},
                        EUINEO::Point2{0.5f, 0.0f},
                        EUINEO::Point2{1.0f, 1.0f}
                    })
                    .build();

                ui.column()
                    .margin(0.0f, 8.0f, 0.0f, 0.0f)
                    .gap(14.0f)
                    .content([&] {
                        ui.label("title")
                            .fontSize(titleFontSize)
                            .color(EUINEO::Color(0.95f, 0.97f, 1.0f, 1.0f))
                            .text(titleText)
                            .build();

                        ui.label("subtitle")
                            .fontSize(subtitleFontSize)
                            .color(EUINEO::Color(0.72f, 0.82f, 0.96f, 1.0f))
                            .text(subtitleText)
                            .build();
                    });
            });
    });
}
