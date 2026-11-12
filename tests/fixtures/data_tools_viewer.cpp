#include "eui_neo.h"
#include <cmath>
#include <fstream>
#include <filesystem>

namespace app {
namespace {
auto rows = [] {
    auto result = std::make_shared<components::DataTableRows>();
    for (int i = 0; i < 100000; ++i) result->push_back({std::to_string(i), "Sensor " + std::to_string(i % 16), std::to_string(i % 1000)});
    return result;
}();
auto samples = std::make_shared<std::vector<float>>(100000);
std::uint64_t revision = 0;
std::string filter, message = "Tab / Shift+Tab: focus. Enter: activate. Drag column edges to resize.";
std::string notes = "Type here; Ctrl+Z / Ctrl+Y use a 2 MiB history budget.";
bool paused = false;
}
const DslAppConfig& dslAppConfig() {
    static const auto config = DslAppConfig{}.title("Data tools: table + realtime chart")
        .windowSize(1120, 760).clearColor("#0F172A").tray(false).iconPath("");
    return config;
}
void compose(eui::Ui& ui, const eui::Screen& screen) {
    const float width = std::max(200.f, screen.width - 40);
    ui.text("title").position(20, 12).size(width, 32).fontSize(22).color("#F8FAFC")
        .text("100,000 rows / 100,000 samples").build();
    components::input(ui, "filter").position(20, 54).size(220, 36).placeholder("Filter all columns")
        .accessibleName("Table filter").value(filter).onChange([](const auto& text) { filter = text; }).build();
    components::button(ui, "save").position(256, 54).size(150, 36).text("Save notes")
        .onClick([] {
            core::platform::FileDialogOptions options;
            options.defaultFileName = "notes.txt"; options.allowedExtensions = {"txt"};
            auto result = core::platform::saveFileDialog(options);
            if (result.selected()) {
                std::ofstream output(std::filesystem::u8path(result.paths.front()), std::ios::binary);
                output << notes;
                message = output ? "Saved: " + result.paths.front() : "Write failed";
            } else message = result.error.empty() ? "Save cancelled" : result.error;
        }).build();
    components::button(ui, "directory").position(422, 54).size(180, 36).text("Choose directory")
        .onClick([] {
            auto result = core::platform::selectDirectoryDialog();
            message = result.selected() ? result.paths.front() : (result.error.empty() ? "Directory cancelled" : result.error);
        }).build();
    components::button(ui, "pause").position(618, 54).size(150, 36).text(paused ? "Resume" : "Pause")
        .onClick([] { paused = !paused; }).build();
    ui.stack("table.position").position(20, 106).size(width, 230).content([&] {
        components::dataTable(ui, "table").size(width, 230).columns({"Index", "Sensor", "Value"})
            .data(rows).numericSort().filter(filter).columnWidths({200, 500, 200})
            .onRowClick([](auto index) { message = "Source row: " + std::to_string(index); }).build();
    }).build();
    if (!paused) {
        ++revision;
        for (std::size_t i = 0; i < samples->size(); ++i)
            (*samples)[i] = std::sin(float(i) * .001f + float(revision) * .04f);
    }
    ui.stack("chart.position").position(20, 350).size(width, 250).content([&] {
        components::lineChart(ui, "signal").size(width, 250).title("Live signal / peak-preserving sampling")
            .data(samples, revision).range(-1, 1).build();
    }).build();
    components::input(ui, "notes").position(20, 614).size(width, std::max(40.f, screen.height - 664))
        .accessibleName("Notes").multiline().scrollbar().undoByteBudget(2 * 1024 * 1024).value(notes)
        .onChange([](const auto& text) { notes = text; }).build();
    ui.text("status").position(20, screen.height - 32).size(width, 26).fontSize(13).color("#94A3B8").text(message).build();
    ui.stack("clock").size(0, 0).onTimer(.033f, [] {}).build();
}
} // namespace app
