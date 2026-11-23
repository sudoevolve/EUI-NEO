#include "eui_neo.h"

namespace app {
const DslAppConfig& dslAppConfig() {
    static const auto config = DslAppConfig{}.title("Desktop settings")
        .logicalWindowSize(960, 720).minWindowSize(640, 480).iconPath("");
    return config;
}

namespace {
struct Settings {
    bool sound = true;
    int proxy = 0;
    float limit = 100;
    std::string address;
};
struct Download { std::string id, name, status; };
struct State {
    Settings saved;
    eui::Signal<bool> sound{true};
    eui::Signal<int> proxy{0};
    eui::Signal<float> limit{100};
    eui::Signal<std::string> address;
    eui::Signal<std::vector<std::string>> selected;
    bool options = false;
    bool about = false;
    std::shared_ptr<std::vector<Download>> downloads = std::make_shared<std::vector<Download>>(
        std::vector<Download>{{"1", "Documentation.zip", "Complete"}, {"2", "Sources.tar.gz", "Downloading"}, {"3", "Assets.zip", "Queued"}});
    void apply() { saved = {sound.get(), proxy.get(), limit.get(), address.get()}; }
    void reset() {
        sound.set(saved.sound); proxy.set(saved.proxy); limit.set(saved.limit); address.set(saved.address);
    }
};
}

void compose(eui::Ui& ui, const eui::Screen& screen) {
    auto& state = ui.state<State>("settings");
    const auto theme = components::theme::light();
    ui.rect("background").size(screen.width, screen.height).color(theme.background).build();
    components::menuBar(ui, "menu").screen(screen.width, screen.height).theme(theme).height(32)
        .menus({{"File", {{"Preferences", [&] { state.reset(); state.options = true; }}}},
                {"Help", {{"About", [&] { state.about = true; }}}}}).build();
    components::label(ui, "title").position(24, 48).size(screen.width - 48, 32)
        .text("Downloads").fontSize(24).color(theme.text).build();
    components::dataTable(ui, "downloads").position(24, 96).size(std::max(0.0f, screen.width - 48), std::max(0.0f, screen.height - 160))
        .theme(theme).data<Download>(state.downloads, {
            {"Name", [](const Download& row) { return row.name; }},
            {"Status", [](const Download& row) { return row.status; }}})
        .rowKey([rows = state.downloads](std::size_t index) { return (*rows)[index].id; })
        .selectionMode(components::DataTableSelectionMode::Multiple).bindSelection(state.selected).build();
    components::label(ui, "selection").position(24, screen.height - 48).size(screen.width - 48, 24)
        .text("Selected: " + std::to_string(state.selected.get().size()) + "   |   File > Preferences")
        .fontSize(14).color(theme.text).build();

    if (state.about) {
        components::dialog(ui, "about").screen(screen.width, screen.height).size(400, 160).theme(theme).open(true)
            .onOpenChange([&](bool open) { state.about = open; })
            .content([&] {
                components::label(ui, "about.text").position(24, 16).size(352, 64)
                    .text("EUI-NEO desktop controls example").fontSize(16).color(theme.text).build();
                components::button(ui, "about.close").position(280, 104).size(96, 32).text("Close").theme(theme)
                    .onClick([&] { state.about = false; }).build();
            }).build();
    }
    if (state.options) {
        const float width = std::max(0.0f, std::min(680.0f, screen.width - 32));
        components::dialog(ui, "options").screen(screen.width, screen.height).size(width, 380).theme(theme).open(true)
            .onOpenChange([&](bool open) { state.options = open; if (!open) state.reset(); })
            .content([&] {
                components::label(ui, "options.title").position(24, 12).size(width - 48, 32)
                    .text("Preferences").fontSize(20).color(theme.text).build();
                components::form(ui, "options.form").position(8, 50).width(width - 16).theme(theme).controlHeight(28).gap(12)
                    .row("proxy", "Proxy type", [&](auto& u, float w, float h) {
                        components::dropdown(u, "options.proxy").size(w, h).theme(theme)
                            .items({"System", "HTTP", "SOCKS5"}).bind(state.proxy).build();
                    })
                    .row("address", "Address", [&](auto& u, float w, float h) {
                        components::input(u, "options.address").size(w, h).theme(theme).bind(state.address).build();
                    })
                    .row("limit", "Limit (MiB/s)", [&](auto& u, float w, float h) {
                        components::slider(u, "options.limit").size(w, h).theme(theme).range(0, 500).step(5).bind(state.limit).build();
                    })
                    .fullRow("sound", [&](auto& u, float w, float h) {
                        components::checkbox(u, "options.sound").size(w, h).theme(theme)
                            .text("Play sound on completion").bind(state.sound).build();
                    })
                    .actions("buttons", [&](auto& u, float w, float h) {
                        u.row("options.actions").size(w, h).gap(12).content([&] {
                            components::button(u, "options.apply").size(96, h).theme(theme).text("Apply")
                                .onClick([&] { state.apply(); }).build();
                            components::button(u, "options.ok").size(80, h).theme(theme, false).text("OK")
                                .onClick([&] { state.apply(); state.options = false; }).build();
                            components::button(u, "options.cancel").size(80, h).theme(theme, false).text("Cancel")
                                .onClick([&] { state.reset(); state.options = false; }).build();
                        }).build();
                    }).build();
            }).build();
    }
}
} // namespace app
