#include "components/datatable.h"

#include <iostream>

int main() {
    using namespace components;
    int failures = 0;
    const auto require = [&](bool value, const char* message) { if (!value) { std::cerr << message << '\n'; ++failures; } };
    DataTableSelection model;
    const std::vector<std::string> visible{"a", "c", "b", "d"};
    model.select(visible, 1, DataTableSelectionMode::Multiple);
    model.select(visible, 3, DataTableSelectionMode::Multiple, false, true);
    require(model.keys() == std::vector<std::string>({"b", "c", "d"}), "Shift range must follow view order");
    model.select(visible, 2, DataTableSelectionMode::Multiple, true);
    require(!model.contains("b") && model.contains("c"), "Ctrl toggle loses unrelated rows");
    model.retain({"d", "c"});
    require(model.keys() == std::vector<std::string>({"c", "d"}), "data deletion pruning");
    struct Item { std::string id, name; };
    auto items = std::make_shared<std::vector<Item>>(std::vector<Item>{{"one", "Zulu"}, {"two", "Alpha"}, {"three", "Bravo"}});
    core::dsl::Ui ui;
    eui::Signal<std::vector<std::string>> selected;
    std::uint64_t revision = 0;
    std::string filter;
    int formats = 0;
    const auto compose = [&] {
        ui.begin("table");
        dataTable(ui, "items").size(500, 250)
            .data<Item>(items, {{"Name", [&](const Item& item) { ++formats; return item.name; }}}, revision)
            .rowKey([items](std::size_t row) { return (*items)[row].id; })
            .sort(0).filter(filter).selectionMode(DataTableSelectionMode::Multiple).bindSelection(selected).build();
        ui.end(); ui.layout(500, 250);
    };
    const auto firstRow = [&](const auto& self, const core::dsl::Element& element) -> const core::dsl::Element* {
        if (element.accessibleRole == core::AccessibleRole::ListItem && element.onClick) return &element;
        for (const auto& child : element.children) if (const auto* row = self(self, *child)) return row;
        return nullptr;
    };
    compose(); compose();
    require(formats == 3, "unchanged typed data was reformatted");
    const auto* row = firstRow(firstRow, *ui.find("items"));
    require(row != nullptr, "table has no selectable row");
    if (!row) return 1;
    row->onClick(); compose();
    require(selected.get() == std::vector<std::string>{"two"}, "selection key did not follow sorted source row");
    row = firstRow(firstRow, *ui.find("items"));
    require(row->accessibleSelected, "selected row semantics not exposed");
    row->onKeyEvent({core::InputKey::Down, core::KeyAction::Press, {}}); compose();
    require(selected.get() == std::vector<std::string>{"three"}, "keyboard selection did not move");
    row = firstRow(firstRow, *ui.find("items"));
    row->onKeyEvent({core::InputKey::Down, core::KeyAction::Press, {}}); compose();
    require(selected.get() == std::vector<std::string>{"one"}, "repeated keyboard navigation stuck on a slot");
    std::reverse(items->begin(), items->end()); ++revision; compose();
    require(ui.state<table_detail::State>("items.tableState").selection.contains("one"), "source reorder loses stable selection");
    filter = "Alpha"; compose();
    require(ui.state<table_detail::State>("items.tableState").selection.contains("one"), "filter discards hidden selection");
    filter.clear(); items->erase(std::remove_if(items->begin(), items->end(), [](const Item& item) { return item.id == "one"; }), items->end());
    ++revision; compose();
    require(!ui.state<table_detail::State>("items.tableState").selection.contains("one"), "removed source row stays selected internally");
    require(selected.get().empty(), "deleted row key was not removed from controlled signal");

    ui.begin("short-table");
    dataTable(ui, "short").size(300, 40).columns({"Name"})
        .rows({{"A"}, {"B"}}).rowHeight(32).bindSelection(selected).build();
    ui.end(); ui.layout(300, 40);
    row = firstRow(firstRow, *ui.find("short"));
    require(row != nullptr, "short table has no focusable row");
    if (row) {
        row->onKeyEvent({core::InputKey::End, core::KeyAction::Press, {}});
        require(selected.get() == std::vector<std::string>{"1"} &&
                ui.state<table_detail::State>("short.tableState").offset == 32,
                "viewport shorter than a row must scroll to the row start");
    }
    return failures ? 1 : 0;
}
