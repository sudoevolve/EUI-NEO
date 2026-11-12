#include "components/linechart.h"
#include "components/input_model.h"
#include "core/runtime/runtime_hit_test.h"
#include "core/runtime/runtime_geometry.h"
#include <chrono>
#include <iostream>

std::size_t nodes(const core::dsl::Ui& ui) {
    std::size_t count = 0;
    const auto visit = [&](const auto& self, const core::dsl::Element& element) -> void {
        ++count; for (const auto& child : element.children) self(self, *child);
    };
    for (const auto& root : ui.roots()) visit(visit, *root);
    return count;
}

int main(int argc, char**) {
    using namespace components;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    auto source = std::make_shared<std::vector<float>>(100000, .5f);
    (*source)[100] = 1; (*source)[101] = 0;
    auto points = linechart_detail::downsample(*source, 0, source->size(), 300, 200, 0, 1);
    if (points.size() > 1200 || points.front().x != 0 || points.back().x != 300) return 1;
    bool peak = false, valley = false;
    for (const auto& point : points) { peak |= point.y == 0; valley |= point.y == 200; }
    if (!peak || !valley) return 2;
    points = linechart_detail::downsample({0, .5f, nan, .5f, 1}, 0, 5, 100, 200, 0, 1);
    if (points.size() != 5 || std::isfinite(points[2].x)) return 3;
    if (!linechart_detail::downsample({}, 0, 20, 300, 200, 0, 1).empty()) return 4;
    if (!linechart_detail::downsample(*source, 200, 100, 300, 200, 0, 1).empty()) return 5;
    points = linechart_detail::downsample({-10, 0, 10}, 1, 3, 100, 200, -10, 10);
    if (points.size() != 2 || points.front().y != 100 || points.back().y != 0) return 6;
    core::dsl::Ui ui;
    std::uint64_t revision = 0;
    const auto build = [&](bool realtime) {
        ui.begin("chart");
        auto chart = lineChart(ui, "signal").size(356, 312);
        if (realtime) chart.data(source, ++revision);
        else chart.values(*source);
        chart.build(); ui.end(); ui.layout(356, 312);
    };
    build(true);
    if (nodes(ui) > 12 || ui.find("signal.line")->polygonPoints.size() > 1200) return 7;
    core::dsl::Element stroke;
    stroke.strokeWidth = 8; stroke.polygonPoints = {{0, 0}, {100, 0}, {nan, 0}, {200, 0}, {300, 0}};
    if (!core::dsl::polygonContains(stroke, 50, -3, 1, {0, 0, 300, 100}) ||
        core::dsl::polygonContains(stroke, 150, 0, 1, {0, 0, 300, 100})) return 8;
    const auto bounds = core::dsl::polygonVisualRect({0, 0, 300, 100}, 8);
    if (bounds.x != -6 || bounds.width != 312) return 9;
    // 仅显式传参时执行计时；常规 CTest 只验证行为与复杂度边界。
    if (argc > 1) {
        source->resize(5000);
        for (bool realtime : {false, true}) {
            for (int i = 0; i < 3; ++i) build(realtime);
            for (int i = 0; i < 10; ++i) {
                const auto start = std::chrono::steady_clock::now();
                build(realtime);
                const auto us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count();
                std::cout << "chart," << (realtime ? "realtime" : "legacy") << ",5000," << i << ',' << nodes(ui) << ',' << us << '\n';
            }
        }
        using Model = input_detail::InputModel;
        for (auto budget : {std::numeric_limits<std::size_t>::max(), std::size_t{8 * 1024 * 1024}}) {
            Model::InputState state; state.text.assign(1024 * 1024, 'a'); state.undoByteBudget = budget;
            for (int i = 0; i < 128; ++i) { state.text[0] = char('a' + i % 26); Model::pushUndoState(state); }
            std::cout << "undo," << budget << ',' << state.undoStack.size() << ',' << Model::historyBytes(state) << '\n';
        }
    }
    return 0;
}
