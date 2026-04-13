#include "app/DslAppRuntime.h"
#include "components/ListView.h"
#include <algorithm>
#include <string>

int main() {
    EUINEO::DslAppConfig config;
    config.title = "EUI ListView Demo";
    config.width = 800;
    config.height = 600;
    config.pageId = "listview_demo";
    config.fps = 120;
    config.darkTitleBar = true;

    return EUINEO::RunDslApp(config, [](EUINEO::UIContext& ui, const EUINEO::RectFrame& screen) {
        EUINEO::UseDslDarkTheme(EUINEO::Color(0.0f, 0.0f, 0.0f, 1.0f));
        const float headerHeight = 60.0f;
        const float pagePadding = 20.0f;
        const int itemCount = 1000000;
        const float itemHeight = 48.0f;
        const float rowHeight = itemHeight - 4.0f;
        const float listX = pagePadding;
        const float listY = headerHeight + pagePadding;
        const float listWidth = std::max(0.0f, screen.width - pagePadding * 2.0f);
        const float listHeight = std::max(0.0f, screen.height - listY - pagePadding);
        const auto composeItem = [&](int index, float itemY) {
            const std::string itemKey = "demo.list.item." + std::to_string(index);
            ui.panel(itemKey + ".bg")
                .position(listX, itemY)
                .size(listWidth, rowHeight)
                .background(EUINEO::Color(0.15f, 0.15f, 0.18f, 1.0f))
                .rounding(8.0f)
                .build();

            ui.row()
                .position(listX, itemY)
                .size(listWidth, rowHeight)
                .padding(16.0f, 0.0f)
                .gap(12.0f)
                .alignItems(EUINEO::CrossAxisAlignment::Center)
                .content([&] {
                    ui.label(itemKey + ".text")
                        .flex(1.0f)
                        .text("List Item #" + std::to_string(index + 1) + " - Fast Virtual Rendering")
                        .fontSize(16.0f)
                        .color(EUINEO::Color(0.8f, 0.8f, 0.8f, 1.0f))
                        .build();

                    ui.button(itemKey + ".action")
                        .size(80.0f, 32.0f)
                        .text("Action")
                        .style(EUINEO::ButtonStyle::Primary)
                        .build();
                });
        };

        ui.panel("demo.header")
            .position(0.0f, 0.0f)
            .size(screen.width, headerHeight)
            .background(EUINEO::Color(0.12f, 0.12f, 0.14f, 1.0f))
            .border(1.0f, EUINEO::Color(0.2f, 0.2f, 0.25f, 1.0f))
            .build();

        ui.row()
            .position(0.0f, 0.0f)
            .size(screen.width, headerHeight)
            .padding(pagePadding, 0.0f)
            .alignItems(EUINEO::CrossAxisAlignment::Center)
            .content([&] {
                ui.label("demo.header.title")
                    .text("1,000,000 Items Virtual List View Demo")
                    .fontSize(24.0f)
                    .color(EUINEO::Color(1.0f, 1.0f, 1.0f, 1.0f))
                    .build();
            });

        if (listWidth > 0.0f && listHeight > 0.0f) {
            EUINEO::ListView::Compose(
                ui,
                "demo.list",
                listX, listY, listWidth, listHeight,
                itemCount,
                itemHeight,
                composeItem
            );
        }
    });
}
