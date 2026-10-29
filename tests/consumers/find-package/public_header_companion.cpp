#include "eui_neo.h"

bool publicHeaderCompanionIsLinked() {
    const eui::Color color{0.0f, 1.0f, 0.0f, 1.0f};
    eui::Ui ui;
    ui.begin("installed-viewport");
    components::markdown(ui, "document").width(200).markdown("Installed **viewport**")
        .viewport(0, 80).build();
    ui.end();
    ui.layout(200, 100);
    const auto revision = core::TextPrimitive::fontConfigurationRevision();
    return color.g > 0.5f && ui.find("document") && ui.find("document")->frame.height > 0 &&
           revision == core::TextPrimitive::fontConfigurationRevision();
}
