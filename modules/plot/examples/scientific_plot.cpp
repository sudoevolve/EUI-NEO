#include "eui_neo.h"
#include "modules/plot/plot.h"

#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace app {
namespace {
using modules::plot::Axes;
using modules::plot::Data;
using modules::plot::Graph;
using modules::plot::Histogram;
using modules::plot::Normalization;
using modules::plot::Path;
using modules::plot::Plot;
using modules::plot::Series;

std::vector<std::unique_ptr<Plot>> plots;

struct GalleryState {
    int updates = 0;
    int axesPreset = 0;
    bool interactionsEnabled = true;
};

GalleryState galleryState;

modules::plot::Style style(std::array<float, 4> color, double lineWidth = 1.5,
                           double markerSize = 5) {
    modules::plot::Style result;
    result.color = color;
    result.lineWidth = lineWidth;
    result.markerSize = markerSize;
    return result;
}

void setupPlots() {
    plots.clear();
    for (int i = 0; i < 4; ++i)
        plots.push_back(std::make_unique<Plot>());

    std::vector<double> curveX, curveY, stepX, stepY;
    for (int i = 0; i < 240; ++i) {
        const double x = std::pow(10.0, -1.0 + 2.0 * i / 239.0);
        curveX.push_back(x);
        curveY.push_back(std::sin(std::log(x) * 3.0));
        if (i >= 106 && i <= 119)
            curveY.back() = std::numeric_limits<double>::quiet_NaN();
        if (i == 164)
            curveY.back() = std::numeric_limits<double>::infinity();
        if (i % 12 == 0) {
            stepX.push_back(x);
            stepY.push_back(std::cos(std::log(x) * 2.0));
        }
    }
    Series line;
    line.name = "Line";
    line.data = Data(curveX, curveY);
    line.style = style({0.2f, 0.7f, 1, 1}, 2.2);
    line.style.cap = modules::plot::LineCap::Round;
    line.style.join = modules::plot::LineJoin::Round;
    line.style.missingColor = {1, 0.55f, 0.1f, 0.95f};
    Series scatter;
    scatter.name = "Scatter";
    scatter.graph = Graph::Scatter;
    scatter.data = Data(std::vector<double>(curveX.begin(), curveX.begin() + 80),
                        std::vector<double>(curveY.begin(), curveY.begin() + 80));
    scatter.style = style({1, 0.8f, 0.2f, 1}, 1, 7);
    scatter.style.marker = modules::plot::Marker::Circle;
    Series step;
    step.name = "Step";
    step.graph = Graph::Step;
    step.data = Data(stepX, stepY);
    step.style = style({0.95f, 0.35f, 0.65f, 1}, 1.5);
    step.style.pattern = modules::plot::LinePattern::Dashed;
    Series stem = step;
    stem.name = "Stem";
    stem.graph = Graph::Stem;
    stem.style = style({0.5f, 1, 0.45f, 0.8f}, 1.2);
    stem.style.baseline = 0;
    stem.style.pattern = modules::plot::LinePattern::Dotted;
    stem.style.markers = true;
    stem.style.marker = modules::plot::Marker::Diamond;
    std::vector<double> largeX, largeY;
    largeX.reserve(100000);
    largeY.reserve(100000);
    for (int i = 0; i < 100000; ++i) {
        const double t = static_cast<double>(i) / 99999.0;
        largeX.push_back(std::pow(10.0, -1.0 + 2.0 * t));
        largeY.push_back(0.16 * std::sin(t * 180.0) + 0.04 * std::cos(t * 950.0));
    }
    Series large;
    large.name = "100k downsampled";
    large.data = Data(largeX, largeY);
    large.style = style({0.65f, 0.7f, 0.8f, 0.45f}, 1.0);
    plots[0]->setSeries({line, scatter, step, stem, large});
    Axes curveAxes;
    curveAxes.x.setScale(modules::plot::Scale::Log10);
    curveAxes.x.label = "logarithmic X";
    curveAxes.y.label = "value";
    plots[0]->setAxes(curveAxes);
    plots[0]->setTitle("Curves: line / scatter / step / stem / 100k");

    const std::vector<double> barX{-3, -1, 1, 3};
    const auto grouped = modules::plot::arrangeBars(
        barX, {{2.0, -1.0, 1.5, 2.5}, {1.0, 2.0, -2.0, 1.0}}, false, 0.8);
    Series groupA;
    groupA.name = "Grouped A";
    groupA.graph = Graph::Bar;
    groupA.data = grouped[0].tops;
    groupA.baselines = grouped[0].baselines;
    groupA.style = style({0.25f, 0.55f, 1, 0.9f});
    groupA.style.barWidth = grouped[0].width;
    Series groupB;
    groupB.name = "Grouped B";
    groupB.graph = Graph::Bar;
    groupB.data = grouped[1].tops;
    groupB.baselines = grouped[1].baselines;
    groupB.style = style({1, 0.45f, 0.25f, 0.9f});
    groupB.style.barWidth = grouped[1].width;
    const std::vector<double> stackX{6, 8};
    const auto stacked = modules::plot::arrangeBars(stackX, {{1.5, 2.0}, {-0.8, 1.0}}, true, 0.9);
    Series stackA;
    stackA.name = "Stack +";
    stackA.graph = Graph::Bar;
    stackA.data = stacked[0].tops;
    stackA.baselines = stacked[0].baselines;
    stackA.style = style({0.35f, 0.9f, 0.45f, 0.9f});
    stackA.style.barWidth = stacked[0].width;
    Series stackB;
    stackB.name = "Stack -/+";
    stackB.graph = Graph::Bar;
    stackB.data = stacked[1].tops;
    stackB.baselines = stacked[1].baselines;
    stackB.style = style({0.75f, 0.35f, 0.9f, 0.9f});
    stackB.style.barWidth = stacked[1].width;
    plots[1]->setSeries({groupA, groupB, stackA, stackB});
    Axes barAxes;
    barAxes.x.label = "groups";
    barAxes.y.label = "count / value";
    plots[1]->setAxes(barAxes);
    plots[1]->setTitle("Bars: grouped, stacked, positive and negative");

    std::vector<double> x, y, lower, upper;
    for (int i = 0; i <= 200; ++i) {
        const double value = 10.0 * i / 200.0;
        const double center = std::sin(value);
        x.push_back(value);
        y.push_back(center);
        lower.push_back(center - 0.18 - 0.04 * std::cos(value));
        upper.push_back(center + 0.18 + 0.04 * std::cos(value));
    }
    Series area;
    area.name = "Area";
    area.graph = Graph::Area;
    area.data = Data(x, y);
    area.style = style({0.2f, 0.55f, 1, 0.3f}, 1.5);
    area.style.baseline = 0;
    area.style.colorMap = modules::plot::ColorMap::Viridis;
    area.style.colorRange = {-1.0, 1.0};
    Series band;
    band.name = "Band";
    band.graph = Graph::Band;
    band.data = Data(x, y);
    band.lower = lower;
    band.upper = upper;
    band.style = style({1, 0.55f, 0.2f, 0.24f});
    Series error;
    error.name = "Error bars";
    error.graph = Graph::ErrorBars;
    error.data = Data(x, y);
    error.lower.assign(error.data.size(), 0.08);
    error.upper.assign(error.data.size(), 0.08);
    error.style = style({0.95f, 0.95f, 0.95f, 0.85f}, 1, 5);
    plots[2]->setSeries({area, band, error});
    Axes intervalAxes;
    intervalAxes.x.label = "time";
    intervalAxes.x.unit = "s";
    intervalAxes.y.label = "signal";
    plots[2]->setAxes(intervalAxes);
    plots[2]->setTitle("Intervals: area, error bars and confidence band");

    std::vector<double> samples;
    for (int i = 0; i < 800; ++i)
        samples.push_back(std::sin(i * 0.37) + 0.25 * std::cos(i * 0.11));
    std::vector<double> edges;
    for (int i = 0; i <= 16; ++i)
        edges.push_back(-1.5 + 3.0 * i / 16.0);
    const Histogram histogram = modules::plot::histogram(samples, edges, Normalization::Density);
    std::vector<double> centers;
    for (std::size_t i = 0; i < histogram.values.size(); ++i)
        centers.push_back((histogram.edges[i] + histogram.edges[i + 1]) / 2);
    Series distribution;
    distribution.name = "Density histogram";
    distribution.graph = Graph::Bar;
    distribution.data = Data(centers, histogram.values);
    distribution.style = style({0.35f, 0.75f, 1, 0.8f});
    distribution.style.barWidth = histogram.edges[1] - histogram.edges[0];
    plots[3]->setSeries({distribution});
    plots[3]->setPaths({
        Path{{{-1.2, 0.12}, {0.8, 0.12}}, style({1, 0.4f, 0.3f, 1}, 2), false, false},
        Path{{{-0.7, 0.35}, {-0.15, 0.7}, {-0.45, 0.65}}, style({0.4f, 1, 0.4f, 0.8f}, 1.5), false,
             true},
    });
    plots[3]->setAnnotations({{{-1.1, 0.15}, "reference"}, {{-0.4, 0.72}, "convex region"}});
    Axes histogramAxes;
    histogramAxes.x.label = "sample value";
    histogramAxes.y.label = "density";
    plots[3]->setAxes(histogramAxes);
    plots[3]->setTitle("Histogram, path, arrow and annotation");
}

void updateCurveData() {
    if (plots.empty() || plots[0]->series().empty())
        return;
    auto series = plots[0]->series();
    auto& line = series.front();
    std::vector<double> x;
    std::vector<double> y;
    x.reserve(line.data.size());
    y.reserve(line.data.size());
    const double phase = 0.35 * static_cast<double>(galleryState.updates + 1);
    for (std::size_t i = 0; i < line.data.size(); ++i) {
        const auto point = line.data.at(i);
        x.push_back(point.x);
        y.push_back(std::isfinite(point.y) ? std::sin(std::log(point.x) * 3.0 + phase)
                                           : std::numeric_limits<double>::quiet_NaN());
    }
    line.data = line.data.replace(0, x, y);
    plots[0]->setSeries(std::move(series));
    ++galleryState.updates;
    requestUpdate();
}

void setGalleryEnabled(bool enabled) {
    galleryState.interactionsEnabled = enabled;
    for (auto& plot : plots)
        plot->setEnabled(enabled);
    requestUpdate();
}

void cycleAxesPreset() {
    if (plots.size() < 3)
        return;
    galleryState.axesPreset = (galleryState.axesPreset + 1) % 4;
    Axes axes;
    axes.x.label = "time";
    axes.x.unit = "s";
    axes.y.label = "signal";
    if (galleryState.axesPreset == 1) {
        axes.x.setRange({1.0, 9.0});
        axes.y.setRange({-1.25, 1.25});
    } else if (galleryState.axesPreset == 2) {
        axes.x.setRange({0.0, 10.0});
        axes.y.setRange({-1.25, 1.25});
        axes.y.setReversed(true);
        axes.x.formatter = [](double value) { return "t=" + std::to_string(value); };
    } else if (galleryState.axesPreset == 3) {
        axes.x.setRange({0.0, 10.0});
        axes.y.setRange({-1.25, 1.25});
        axes.equalize({0, 0, 520, 260});
    }
    plots[2]->setAxes(std::move(axes));
    requestUpdate();
}

std::string interactionStatus() {
    if (plots.empty())
        return {};
    if (const auto measurement = plots[0]->measurement())
        return "Measurement: distance = " + std::to_string(measurement->distance);
    if (const auto selected = plots[0]->selection())
        return "Selected: series " + std::to_string(selected->series + 1) +
               ", original sample " + std::to_string(selected->point.index);
    if (const auto probe = plots[0]->probe())
        return "Probe: series " + std::to_string(probe->series + 1) +
               ", sample " + std::to_string(probe->point.index) +
               "  (x=" + std::to_string(probe->point.data.x) +
               ", y=" + std::to_string(probe->point.data.y) + ")";
    return "Move over a curve to probe it; click to select; hold Ctrl and click twice to measure.";
}

} // namespace

const DslAppConfig& dslAppConfig() {
    static const auto config = DslAppConfig{}
                                   .title("Scientific Plot Gallery")
                                   .pageId("scientific_plot")
                                   .windowSize(1200, 800)
                                   .onShutdown([] {
                                       for (auto& plot : plots)
                                           if (plot)
                                               plot->releaseGpu();
                                       plots.clear();
                                   });
    return config;
}

void compose(eui::Ui& ui, const eui::Screen& screen) {
    if (plots.empty())
        setupPlots();
    const float gap = 12;
    const float margin = 16;
    const float toolbarHeight = 74;
    const float statusHeight = 28;
    const float gridTop = margin + toolbarHeight;
    const float gridBottom = std::max(gridTop + 240.f, screen.height - margin - statusHeight);
    const float width = std::max(160.f, (screen.width - margin * 2 - gap) / 2);
    const float height = std::max(120.f, (gridBottom - gridTop - gap) / 2);
    ui.stack("gallery")
        .size(screen.width, screen.height)
        .clip()
        .content([&] {
            ui.row("toolbar")
                .position(margin, 8)
                .size(screen.width - margin * 2, toolbarHeight - 8)
                .gap(8)
                .alignItems(eui::Align::CENTER)
                .content([&] {
                    ui.column("toolbar.copy")
                        .width(std::max(220.f, screen.width - 420.f))
                        .height(toolbarHeight - 8)
                        .gap(2)
                        .content([&] {
                            ui.text("toolbar.title")
                                .text("Scientific Plot Gallery")
                                .fontSize(22)
                                .lineHeight(28)
                                .build();
                            ui.text("toolbar.help")
                                .text("Drag pan | Wheel zoom | Shift+drag box zoom | Right click reset | Click legend to toggle")
                                .fontSize(11)
                                .lineHeight(17)
                                .color({0.68f, 0.72f, 0.8f, 1})
                                .build();
                        })
                        .build();
                    components::button(ui, "toolbar.update")
                        .size(120, 40)
                        .text("Update data")
                        .onClick(updateCurveData)
                        .build();
                    components::button(ui, "toolbar.interaction")
                        .size(120, 40)
                        .text(galleryState.interactionsEnabled ? "Disable input" : "Enable input")
                        .onClick([] { setGalleryEnabled(!galleryState.interactionsEnabled); })
                        .build();
                    components::button(ui, "toolbar.axes")
                        .size(120, 40)
                        .text("Axes preset " + std::to_string(galleryState.axesPreset))
                        .onClick(cycleAxesPreset)
                        .build();
                })
                .build();
            const auto slot = [&](const std::string& id, float x, float y, Plot& plot) {
                ui.stack("slot." + id)
                    .position(x, y)
                    .size(width, height)
                    // 展示页使用 2× 离屏纹理，再由 UI 缩回逻辑尺寸，降低斜线锯齿。
                    .content([&] { plot.compose(ui, "gallery." + id, width, height, 2.0); })
                    .build();
            };
            slot("curves", margin, gridTop, *plots[0]);
            slot("bars", margin + width + gap, gridTop, *plots[1]);
            slot("intervals", margin, gridTop + height + gap, *plots[2]);
            slot("histogram", margin + width + gap, gridTop + height + gap, *plots[3]);
            ui.text("status")
                .position(margin, screen.height - margin - statusHeight)
                .size(screen.width - margin * 2, statusHeight)
                .text("Updates: " + std::to_string(galleryState.updates) +
                      "  |  Axes preset: " + std::to_string(galleryState.axesPreset) +
                      "  |  " + interactionStatus())
                .fontSize(12)
                .lineHeight(statusHeight)
                .color({0.72f, 0.76f, 0.84f, 1})
                .horizontalAlign(eui::HorizontalAlign::Center)
                .verticalAlign(eui::VerticalAlign::Center)
                .build();
        })
        .build();
}
} // namespace app
