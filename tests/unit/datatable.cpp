#include "components/datatable.h"
#include <cmath>
#include <iostream>

int main() {
    using namespace components;
    auto rows = std::make_shared<DataTableRows>(DataTableRows{{"b", "10"}, {"a", "2"}, {"a", "2"}, {"z"}});
    table_detail::State state;
    state.sort = 1;
    state.refresh(rows, 0, "", true);
    if (state.order != std::vector<std::size_t>{1, 2, 0, 3}) return 1;
    state.descending = true;
    state.refresh(rows, 0, "a", true);
    if (state.order != std::vector<std::size_t>{1, 2}) return 2;
    (*rows)[1][0] = "c";
    state.refresh(rows, 1, "a", true);
    if (state.order != std::vector<std::size_t>{2}) return 3;
    std::vector<float> widths{100, 200, 100};
    table_detail::resizeBoundary(widths, 0, 500, 40);
    if (widths[0] != 260 || widths[1] != 40) return 4;
    table_detail::resizeBoundary(widths, 0, -1000, 40);
    if (widths[0] != 40 || widths[1] != 260) return 5;
    std::vector<float> narrow{1, 299, 100};
    table_detail::fitWidths(narrow, 400, 40);
    if (narrow.front() != 40 || std::fabs(std::accumulate(narrow.begin(), narrow.end(), 0.f) - 400) > .01f) return 14;
    rows->clear();
    for (int i = 0; i < 100000; ++i) rows->push_back({std::to_string(i), "reading"});
    core::dsl::Ui ui;
    std::size_t clicked = rows->size();
    const auto build = [&](const std::string& filter = "") {
        ui.begin("table");
        dataTable(ui, "samples").size(500, 250).columns({"Index", "Value"}).data(rows, 2).filter(filter)
            .onRowClick([&](std::size_t row) { clicked = row; }).build();
        ui.end(); ui.layout(500, 250);
    };
    build();
    std::size_t nodes = 0;
    const auto count = [&](const auto& self, const core::dsl::Element& element) -> void {
        ++nodes; for (const auto& child : element.children) self(self, *child);
    };
    for (const auto& root : ui.roots()) count(count, *root);
    if (nodes > 250) { std::cerr << "table virtualized nodes=" << nodes << '\n'; return 6; }
    auto* header = ui.find("samples.column.0");
    if (!header || !header->onClick) return 7;
    header->onClick();
    build();
    auto& model = ui.state<table_detail::State>("samples.tableState");
    if (model.sort != 0 || model.order.size() != 100000) return 8;
    const auto clickFirst = [&](const auto& self, const core::dsl::Element& element) -> bool {
        if (element.accessibleRole == core::AccessibleRole::ListItem && element.onClick) { element.onClick(); return true; }
        for (const auto& child : element.children) if (self(self, *child)) return true;
        return false;
    };
    header = ui.find("samples.column.0");
    header->onClick();
    build();
    for (const auto& root : ui.roots()) if (clickFirst(clickFirst, *root)) break;
    if (clicked != model.order.front()) return 10;
    const float before = model.widths.front();
    auto* resize = ui.find("samples.column.0.resize");
    if (!resize || !resize->onKeyEvent || !resize->onKeyEvent({core::InputKey::Right, core::KeyAction::Press, {}})) return 11;
    if (model.widths.front() <= before || std::fabs(model.widths[0] + model.widths[1] - model.available) > .01f) return 12;
    model.offset = 32000;
    build();
    if (model.offset != 32000) return 13;
    build("no such row");
    if (!model.order.empty()) return 9;
    return 0;
}
