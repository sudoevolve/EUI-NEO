#include "app/dsl_app.h"

#include "components/components.h"

namespace app {

const DslAppConfig& dslAppConfig() {
    static const DslAppConfig config = {
        "DSL Demo", // windowtitle
        "demo", // pageid
        {0.16f, 0.18f, 0.20f, 1.0f}, // bgcolor
        800, // window width
        600 // window height
    };
    return config;
}

void compose(core::dsl::Ui& ui, const core::dsl::Screen& screen) {
    ui.stack("root")
        .size(screen.width, screen.height)
        .align(core::Align::CENTER, core::Align::CENTER)
        .content([&] {
            ui.rect("card")
                .size(360.0f, 260.0f)
                .radius(18.0f)
                .gradient({0.10f, 0.12f, 0.16f, 1.0f}, {0.05f, 0.07f, 0.10f, 1.0f})
                .border(1.0f, {0.23f, 0.29f, 0.38f, 1.0f})
                .shadow(26.0f, 0.0f, 8.0f, {0.0f, 0.0f, 0.0f, 0.26f})
                .build();

            ui.column("content")
                .size(360.0f, 260.0f)
                .gap(8.0f)
                .justifyContent(core::Align::CENTER)
                .alignItems(core::Align::CENTER)
                .content([&] {
                    ui.text("title")
                        .size(300.0f, 38.0f)
                        .text("Light GUI")
                        .fontSize(30.0f)
                        .lineHeight(38.0f)
                        .color({0.94f, 0.97f, 1.0f, 1.0f})
                        .horizontalAlign(core::HorizontalAlign::Center)
                        .build();

                    ui.text("subtitle")
                        .size(300.0f, 30.0f)
                        .margin(0.0f, 0.0f, 0.0f, 16.0f)
                        .text("Text Button Component")
                        .fontSize(24.0f)
                        .lineHeight(30.0f)
                        .color({0.62f, 0.70f, 0.82f, 1.0f})
                        .horizontalAlign(core::HorizontalAlign::Center)
                        .build();

                    components::button(ui, "primary")
                        .size(240.0f, 70.0f)
                        .text("Click Me")
                        .build();
                });
        });
}

} // namespace app
