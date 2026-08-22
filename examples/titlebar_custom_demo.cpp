// Frameless window with a fully custom toolbar titlebar.
// Features:
//   - Custom titlebar with app icon, title, and a toolbar of buttons
//   - Drag titlebar to move window
//   - Resize handles on all edges and corners
//   - Tab-bar style navigation in the content area
#include "eui_neo.h"

#include <iostream>
#include <string>

namespace app {

static int activeTab = 0;
static int clickCount = 0;

const DslAppConfig& dslAppConfig() {
    static const DslAppConfig config = DslAppConfig{}
        .title("Frameless Titlebar Demo")
        .windowSize(960, 640)
        .fps(60.0)
        .frameless(true);
    return config;
}

void compose(eui::Ui& ui, const eui::Screen& screen) {
    const float titleHeight = 52.0f;
    const float pad = 14.0f;
    const float btnSize = 36.0f;
    const float tabHeight = 42.0f;

    ui.stack("root")
        .size(screen.width, screen.height)
        .content([&] {
            // ---- Custom titlebar with toolbar ----
            components::titlebar(ui, "titlebar")
                .size(screen.width, screen.height)
                .titleHeight(titleHeight)
                .content([&] {
                    // App icon
                    ui.text("tb.icon")
                        .x(pad)
                        .size(24.0f, titleHeight)
                        .icon(0xF015) // fa-home
                        .fontSize(18.0f)
                        .color({0.40f, 0.70f, 1.0f, 1.0f})
                        .horizontalAlign(core::HorizontalAlign::Center)
                        .verticalAlign(core::VerticalAlign::Center)
                        .build();

                    // Title
                    ui.text("tb.title")
                        .x(pad + 32.0f)
                        .size(180.0f, titleHeight)
                        .text("EUI-NEO Studio")
                        .fontSize(15.0f)
                        .color({0.92f, 0.94f, 0.97f, 1.0f})
                        .horizontalAlign(core::HorizontalAlign::Left)
                        .verticalAlign(core::VerticalAlign::Center)
                        .build();

                    // Toolbar buttons (left side)
                    const float tbStart = pad + 220.0f;
                    ui.row("tb.toolbar")
                        .x(tbStart)
                        .y((titleHeight - btnSize) * 0.5f)
                        .size(btnSize * 5.0f + 24.0f, btnSize)
                        .gap(6.0f)
                        .content([&] {
                            // File button
                            components::button(ui, "tb.file")
                                .size(btnSize, btnSize).text("").icon(0xF016)
                                .onClick([] { std::cout << "File menu" << std::endl; })
                                .build();
                            // Edit button
                            components::button(ui, "tb.edit")
                                .size(btnSize, btnSize).text("").icon(0xF044)
                                .onClick([] { std::cout << "Edit menu" << std::endl; })
                                .build();
                            // View button
                            components::button(ui, "tb.view")
                                .size(btnSize, btnSize).text("").icon(0xF06E)
                                .onClick([] { std::cout << "View menu" << std::endl; })
                                .build();
                            // Tools button
                            components::button(ui, "tb.tools")
                                .size(btnSize, btnSize).text("").icon(0xF0AD)
                                .onClick([] { std::cout << "Tools menu" << std::endl; })
                                .build();
                            // Help button
                            components::button(ui, "tb.help")
                                .size(btnSize, btnSize).text("").icon(0xF059)
                                .onClick([] { std::cout << "Help menu" << std::endl; })
                                .build();
                        })
                        .build();

                    // Window controls (right side)
                    const float wcWidth = btnSize * 3.0f + 12.0f;
                    ui.row("tb.winctrl")
                        .x(screen.width - pad - wcWidth)
                        .y((titleHeight - btnSize) * 0.5f)
                        .size(wcWidth, btnSize)
                        .gap(6.0f)
                        .content([&] {
                            // Minimize
                            components::button(ui, "tb.min")
                                .size(btnSize, btnSize).text("").icon(0xF2D1)
                                .onClick([] { app::minimizeWindow(); })
                                .build();
                            // Maximize / Restore
                            components::button(ui, "tb.max")
                                .size(btnSize, btnSize).text("")
                                .icon(app::isWindowMaximized() ? 0xF2D2 : 0xF2D0)
                                .onClick([] { app::toggleMaximizeWindow(); })
                                .build();
                            // Close
                            components::button(ui, "tb.close")
                                .size(btnSize, btnSize).text("").icon(0xF00D)
                                .colors({0.90f, 0.25f, 0.23f, 1.0f},
                                        {0.95f, 0.30f, 0.28f, 1.0f},
                                        {0.75f, 0.18f, 0.16f, 1.0f})
                                .onClick([] { app::requestWindowClose(); })
                                .build();
                        })
                        .build();
                })
                .build();

            // ---- Content area below titlebar ----
            const float contentY = titleHeight;
            const float contentH = screen.height - contentY;

            // Tab bar
            ui.row("tabbar")
                .y(contentY)
                .size(screen.width, tabHeight)
                .padding(12.0f, 0.0f)
                .gap(0.0f)
                .justifyContent(core::Align::START)
                .content([&] {
                    const char* tabLabels[] = {"Workspace", "Preview", "Console", "Settings"};
                    for (int i = 0; i < 4; i++) {
                        const bool isActive = (i == activeTab);
                        std::string tid = "tab." + std::to_string(i);
                        ui.rect(tid + ".bg")
                            .width(130.0f)
                            .height(tabHeight)
                            .color(isActive
                                ? core::Color{0.16f, 0.18f, 0.22f, 1.0f}
                                : core::Color{0.0f, 0.0f, 0.0f, 0.0f})
                            .onClick([i] {
                                activeTab = i;
                                std::cout << "Switched to tab " << i << std::endl;
                            })
                            .build();

                        ui.text(tid + ".label")
                            .width(130.0f)
                            .height(tabHeight)
                            .text(tabLabels[i])
                            .fontSize(13.0f)
                            .color(isActive
                                ? core::Color{0.92f, 0.94f, 1.0f, 1.0f}
                                : core::Color{0.50f, 0.52f, 0.58f, 1.0f})
                            .horizontalAlign(core::HorizontalAlign::Center)
                            .verticalAlign(core::VerticalAlign::Center)
                            .build();
                    }
                })
                .build();

            // Tab content
            const float tabContentY = contentY + tabHeight;
            const float tabContentH = contentH - tabHeight;

            ui.rect("tab.content.bg")
                .y(tabContentY)
                .size(screen.width, tabContentH)
                .color({0.16f, 0.18f, 0.22f, 1.0f})
                .build();

            ui.column("tab.content")
                .y(tabContentY + 20.0f)
                .size(screen.width, tabContentH - 20.0f)
                .padding(32.0f)
                .gap(16.0f)
                .content([&] {
                    if (activeTab == 0) {
                        // Workspace tab
                        ui.text("wc.title")
                            .text("Workspace")
                            .fontSize(24.0f)
                            .color({0.92f, 0.94f, 1.0f, 1.0f})
                            .build();

                        ui.text("wc.desc")
                            .text("This is a frameless window demo with a fully custom titlebar.\n"
                                   "Drag the titlebar to move the window. Resize by dragging edges or corners.\n"
                                   "The toolbar buttons demonstrate app-style menu icons.")
                            .fontSize(14.0f)
                            .color({0.55f, 0.58f, 0.64f, 1.0f})
                            .wrap(true)
                            .build();

                        // Action buttons
                        ui.row("wc.actions")
                            .gap(12.0f)
                            .content([&] {
                                components::button(ui, "wc.btn1")
                                    .text("New Project")
                                    .size(160.0f, 48.0f)
                                    .onClick([] {
                                        clickCount++;
                                        std::cout << "New Project! total=" << clickCount << std::endl;
                                    })
                                    .build();

                                components::button(ui, "wc.btn2")
                                    .text("Open File")
                                    .size(160.0f, 48.0f)
                                    .theme(components::theme::dark(), false)
                                    .onClick([] {
                                        clickCount++;
                                        std::cout << "Open File! total=" << clickCount << std::endl;
                                    })
                                    .build();

                                components::button(ui, "wc.btn3")
                                    .text("Run")
                                    .size(120.0f, 48.0f)
                                    .colors({0.16f, 0.55f, 0.30f, 1.0f},
                                            {0.20f, 0.65f, 0.36f, 1.0f},
                                            {0.12f, 0.44f, 0.24f, 1.0f})
                                    .onClick([] {
                                        clickCount++;
                                        std::cout << "Run! total=" << clickCount << std::endl;
                                    })
                                    .build();
                            })
                            .build();

                        // Stats panel
                        ui.rect("wc.stats.bg")
                            .size(screen.width - 64.0f, 80.0f)
                            .radius(8.0f)
                            .color({0.12f, 0.14f, 0.17f, 1.0f})
                            .border(1.0f, {0.30f, 0.32f, 0.38f, 0.30f})
                            .build();

                        ui.row("wc.stats")
                            .size(screen.width - 64.0f, 80.0f)
                            .padding(24.0f)
                            .gap(0.0f)
                            .justifyContent(core::Align::CENTER)
                            .content([&] {
                                auto statCol = [&](const std::string& id, const std::string& val, const std::string& label) {
                                    ui.column(id)
                                        .flexGrow(1.0f)
                                        .gap(4.0f)
                                        .alignItems(core::Align::CENTER)
                                        .content([&] {
                                            ui.text(id + ".val")
                                                .text(val)
                                                .fontSize(22.0f)
                                                .color({0.92f, 0.94f, 1.0f, 1.0f})
                                                .build();
                                            ui.text(id + ".label")
                                                .text(label)
                                                .fontSize(12.0f)
                                                .color({0.45f, 0.48f, 0.54f, 1.0f})
                                                .build();
                                        })
                                        .build();
                                };
                                statCol("stat.click", std::to_string(clickCount), "Clicks");
                                statCol("stat.tab", std::to_string(activeTab + 1) + "/4", "Active Tab");
                                statCol("stat.res", "960x640", "Window Size");
                            })
                            .build();

                    } else if (activeTab == 1) {
                        // Preview tab
                        ui.text("pv.title")
                            .text("Preview")
                            .fontSize(24.0f)
                            .color({0.92f, 0.94f, 1.0f, 1.0f})
                            .build();

                        ui.text("pv.desc")
                            .text("Preview panel content goes here. Click other tabs to explore.")
                            .fontSize(14.0f)
                            .color({0.55f, 0.58f, 0.64f, 1.0f})
                            .build();

                        // Preview area placeholder
                        ui.rect("pv.area")
                            .size(screen.width - 64.0f, 200.0f)
                            .radius(8.0f)
                            .color({0.12f, 0.14f, 0.17f, 1.0f})
                            .border(1.0f, {0.30f, 0.32f, 0.38f, 0.30f})
                            .build();

                    } else if (activeTab == 2) {
                        // Console tab
                        ui.text("con.title")
                            .text("Console")
                            .fontSize(24.0f)
                            .color({0.92f, 0.94f, 1.0f, 1.0f})
                            .build();

                        ui.rect("con.bg")
                            .size(screen.width - 64.0f, 260.0f)
                            .radius(6.0f)
                            .color({0.08f, 0.09f, 0.11f, 1.0f})
                            .border(1.0f, {0.20f, 0.22f, 0.28f, 0.50f})
                            .build();

                        ui.column("con.lines")
                            .size(screen.width - 64.0f, 260.0f)
                            .padding(16.0f)
                            .gap(4.0f)
                            .content([&] {
                                const char* lines[] = {
                                    "[INFO]  EUI-NEO Studio v0.5.6",
                                    "[INFO]  Frameless window initialized",
                                    "[INFO]  OpenGL render backend active",
                                    "[INFO]  Titlebar: custom content mode",
                                    "[OK]    Ready",
                                };
                                for (int i = 0; i < 5; i++) {
                                    ui.text("con.l" + std::to_string(i))
                                        .text(lines[i])
                                        .fontSize(12.0f)
                                        .fontFamily("monospace")
                                        .color({0.45f, 0.90f, 0.50f, 1.0f})
                                        .build();
                                }
                            })
                            .build();

                    } else {
                        // Settings tab
                        ui.text("st.title")
                            .text("Settings")
                            .fontSize(24.0f)
                            .color({0.92f, 0.94f, 1.0f, 1.0f})
                            .build();

                        ui.column("st.options")
                            .gap(12.0f)
                            .content([&] {
                                auto settingRow = [&](const std::string& id, const std::string& label) {
                                    ui.rect(id + ".bg")
                                        .size(screen.width - 64.0f, 48.0f)
                                        .radius(6.0f)
                                        .color({0.12f, 0.14f, 0.17f, 1.0f})
                                        .border(1.0f, {0.30f, 0.32f, 0.38f, 0.25f})
                                        .build();
                                    ui.text(id + ".label")
                                        .size(screen.width - 64.0f, 48.0f)
                                        .padding(16.0f, 0.0f)
                                        .text(label)
                                        .fontSize(14.0f)
                                        .color({0.85f, 0.87f, 0.92f, 1.0f})
                                        .verticalAlign(core::VerticalAlign::Center)
                                        .build();
                                };
                                settingRow("st.opt1", "  Auto-save");
                                settingRow("st.opt2", "  Show line numbers");
                                settingRow("st.opt3", "  Dark theme");
                                settingRow("st.opt4", "  Enable animations");
                            })
                            .build();
                    }
                })
                .build();
        })
        .build();
}

} // namespace app