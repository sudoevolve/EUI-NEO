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

    return EUINEO::RunDslApp(config, [](EUINEO::UIContext& ui, const EUINEO::RectFrame& screen) {
        EUINEO::UseDslDarkTheme(EUINEO::Color(0.0f, 0.0f, 0.0f, 1.0f));

        // Header
        const float headerHeight = 60.0f;
        ui.panel("demo.header")
            .position(0.0f, 0.0f)
            .size(screen.width, headerHeight)
            .background(EUINEO::Color(0.12f, 0.12f, 0.14f, 1.0f))
            .border(1.0f, EUINEO::Color(0.2f, 0.2f, 0.25f, 1.0f))
            .build();

        ui.label("demo.header.title")
            .position(20.0f, 48.0f)
            .text("1,000,000 Items Virtual List View Demo")
            .fontSize(24.0f)
            .color(EUINEO::Color(1.0f, 1.0f, 1.0f, 1.0f))
            .build();

        // ListView configuration
        const int itemCount = 1000000;
        const float itemHeight = 48.0f;
        const float listX = 20.0f;
        const float listY = headerHeight + 20.0f;
        const float listWidth = std::max(0.0f, screen.width - 40.0f);
        const float listHeight = std::max(0.0f, screen.height - listY - 20.0f);

        if (listWidth > 0.0f && listHeight > 0.0f) {
            EUINEO::ListView::Compose(
                ui, 
                "demo.list", 
                listX, listY, listWidth, listHeight, 
                itemCount, 
                itemHeight, 
                [&](int index, float itemY) {
                    // Draw item background
                    ui.panel("demo.list.itemBg." + std::to_string(index))
                        .position(listX, itemY)
                        .size(listWidth, itemHeight - 4.0f)
                        .background(EUINEO::Color(0.15f, 0.15f, 0.18f, 1.0f))
                        .rounding(8.0f)
                        .build();

                    const float fontSize = 16.0f;
                    const float itemCenterY = itemY + (itemHeight - 4.0f) * 0.5f;
                    const float textY = itemCenterY + (fontSize * 0.25f); 
                    
                    ui.label("demo.list.itemText." + std::to_string(index))
                        .position(listX + 16.0f, textY)
                        .text("List Item #" + std::to_string(index + 1) + " - Fast Virtual Rendering")
                        .fontSize(fontSize)
                        .color(EUINEO::Color(0.8f, 0.8f, 0.8f, 1.0f))
                        .build();
                        
                    // Draw a button for interaction - also vertically centered
                    const float btnHeight = 32.0f;
                    const float btnY = itemY + (itemHeight - 4.0f - btnHeight) * 0.5f;
                    
                    ui.button("demo.list.itemBtn." + std::to_string(index))
                        .position(listX + listWidth - 100.0f, btnY)
                        .size(80.0f, btnHeight)
                        .text("Action")
                        .style(EUINEO::ButtonStyle::Primary)
                        .build();
                }
            );
        }
    });
}
