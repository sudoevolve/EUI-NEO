#include "app/DslAppRuntime.h"
#include <vector>

int main() {
    EUINEO::UseDslLightTheme(EUINEO::Color(1.0f, 1.0f, 1.0f, 1.0f));

    EUINEO::DslAppConfig config;
    config.title = "EUI Center Combo Demo";
    config.width = 960;
    config.height = 640;
    config.pageId = "center_combo_demo";
    config.fps = 120;

    return EUINEO::RunDslApp(config, [](EUINEO::UIContext& ui, const EUINEO::RectFrame& screen) {
        static int selectedIndex = 1;
        static const std::vector<std::string> items = {
            "Apple",
            "Banana",
            "Orange",
            "Grape"
        };

        const float comboWidth = 320.0f;
        const float comboHeight = 44.0f;
        const float comboX = (screen.width - comboWidth) * 0.5f;
        const float comboY = (screen.height - comboHeight) * 0.5f;

        ui.combo("center.combo")
            .position(comboX, comboY)
            .size(comboWidth, comboHeight)
            .placeholder("Select item")
            .fontSize(20.0f)
            .items(items)
            .selected(selectedIndex)
            .onChange([](int index) { selectedIndex = index; })
            .build();
    });
}
