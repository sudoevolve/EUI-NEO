#include "eui_neo.h"
#include "modules/plot/contour.h"
#include "modules/plot/export.h"
#include "modules/plot/figure.h"
#include "modules/plot/plot.h"
#include "modules/plot/vector.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

namespace app {
namespace {
using namespace modules::plot;

std::unique_ptr<Figure> linkedFigure;
std::unique_ptr<PolarPlot> polar;
std::unique_ptr<HeatmapPlot> heatmap;
std::unique_ptr<Plot> contours;
std::unique_ptr<Plot> vectors;
std::unique_ptr<Plot> exportPreview;

Style lineStyle(std::array<float, 4> color, double width = 1.5) {
    Style style;
    style.color = color;
    style.lineWidth = width;
    return style;
}

void setupLinkedFigure() {
    linkedFigure = std::make_unique<Figure>();
    auto& primary = linkedFigure->addPlot();
    auto& secondary = linkedFigure->addPlot();
    std::vector<double> x, firstY, secondY;
    for (int index = 0; index <= 160; ++index) {
        const double value = index * 10.0 / 160.0;
        x.push_back(value);
        firstY.push_back(std::sin(value));
        secondY.push_back(100 + 25 * std::cos(value * 0.7));
    }
    Series first;
    first.name = "signal";
    first.data = Data(x, firstY);
    first.style = lineStyle({0.2f, 0.8f, 1, 1}, 2);
    Series secondSeries;
    secondSeries.name = "temperature";
    secondSeries.data = Data(x, secondY);
    secondSeries.yAxis = YAxis::Secondary;
    secondSeries.style = lineStyle({1, 0.55f, 0.2f, 1}, 2);
    primary.setSeries({first});
    primary.setTitle("Linked primary axis");
    primary.setAnnotations({{{4, 0.8}, "signal(t)"}});
    Axis secondaryAxis;
    secondaryAxis.label = "deg C";
    secondary.setSecondaryYAxis(secondaryAxis);
    secondary.setSeries({first, secondSeries});
    secondary.setTitle("Dual Y axis");
    secondary.setAnnotations({{{5, 120}, "temperature rate"}});
    Axes axes;
    axes.x.label = "time";
    axes.y.label = "signal";
    primary.setAxes(axes);
    secondary.setAxes(axes);
    primary.setCursor({{5, std::sin(5)}});
    linkedFigure->setColumns(2);
    linkedFigure->linkX({0, 1});
    linkedFigure->linkCursor({0, 1});
}

void setupPolar() {
    polar = std::make_unique<PolarPlot>();
    std::vector<double> angles, radius;
    for (int index = 0; index <= 360; ++index) {
        angles.push_back(double(index));
        radius.push_back(0.2 + index / 420.0);
    }
    Series spiral;
    spiral.data = Data(angles, radius);
    spiral.style = lineStyle({0.2f, 0.8f, 1, 1}, 2);
    Series samples;
    samples.graph = Graph::Scatter;
    samples.data = Data({0, 45, 90, 135, 180, 225, 270, 315}, {0.8, 0.6, 0.7, 0.5, 0.75, 0.55, 0.65, 0.5});
    samples.style = lineStyle({1, 0.75f, 0.2f, 1});
    samples.style.marker = Marker::Circle;
    samples.style.markerSize = 7;
    polar->setSeries({spiral, samples});
    PolarAxes axes;
    axes.setAngleUnit(AngleUnit::Degrees);
    axes.setZeroAngle(90);
    axes.setClockwise(true);
    polar->setAxes(axes);
    polar->setTitle("Polar degrees / clockwise");
}

ScalarField makeField(std::size_t rows, std::size_t columns) {
    std::vector<double> values;
    values.reserve(rows * columns);
    for (std::size_t row = 0; row < rows; ++row)
        for (std::size_t column = 0; column < columns; ++column) {
            const double x = -3 + 6.0 * column / (columns - 1);
            const double y = -2 + 4.0 * row / (rows - 1);
            values.push_back(std::sin(x * 1.5) * std::cos(y * 1.8) + 0.12 * x);
        }
    return ScalarField(rows, columns, std::move(values), {-3, 3}, {-2, 2}, FieldOrigin::LowerLeft,
                       FieldSampling::GridPoints);
}

void setupHeatmap() {
    heatmap = std::make_unique<HeatmapPlot>();
    heatmap->setField(makeField(36, 48));
    ColorScale scale;
    scale.setColorMap(ColorMap::Turbo);
    scale.setRange({-1.2, 1.2});
    scale.setDiscreteLevels(12);
    heatmap->setColorScale(scale);
    heatmap->setTitle("Heatmap / discrete colorbar / missing color");
}

void setupContours() {
    const auto field = makeField(41, 51);
    const auto levels = automaticContourLevels(field, 7);
    ColorScale scale;
    scale.setColorMap(ColorMap::Viridis);
    scale.fit(field);
    std::vector<Path> paths;
    const auto bands = filledContours(field, levels);
    const auto range = field.finiteRange();
    for (const auto& band : bands) {
        Style style;
        style.color = scale.map(std::clamp((std::max(band.lower, range->min) + std::min(band.upper, range->max)) / 2,
                                            range->min, range->max));
        for (const auto& triangle : band.triangles)
            paths.push_back({{triangle[0], triangle[1], triangle[2]}, style, true, true});
    }
    const auto lines = marchingSquares(field, levels);
    Style line = lineStyle({0.95f, 0.95f, 0.95f, 0.95f}, 1);
    for (const auto& level : lines)
        for (const auto& segment : level.segments)
            paths.push_back({{segment.from, segment.to}, line, false, false});
    contours = std::make_unique<Plot>();
    contours->setPaths(std::move(paths));
    contours->setAnnotations({{{-2.7, 1.6}, "radial level"}});
    Axes axes;
    axes.x.setRange(field.xRange());
    axes.y.setRange(field.yRange());
    contours->setAxes(axes);
    contours->setTitle("Marching squares / filled bands / labels");
}

void setupVectors() {
    std::vector<double> x(13), y(11), horizontal(x.size() * y.size()), vertical(horizontal.size());
    for (std::size_t column = 0; column < x.size(); ++column)
        x[column] = -2 + 4.0 * column / (x.size() - 1);
    for (std::size_t row = 0; row < y.size(); ++row)
        y[row] = -1.5 + 3.0 * row / (y.size() - 1);
    for (std::size_t row = 0; row < y.size(); ++row)
        for (std::size_t column = 0; column < x.size(); ++column) {
            const auto index = row * x.size() + column;
            horizontal[index] = -y[row];
            vertical[index] = x[column];
        }
    const VectorField field(RectilinearField(x, y, horizontal), RectilinearField(x, y, vertical));
    std::vector<Path> paths;
    Style arrows = lineStyle({1, 0.75f, 0.2f, 0.9f}, 1.2);
    for (const auto& arrow : vectorArrows(field, 0.13, 0.05)) {
        paths.push_back({{arrow.from, arrow.to}, arrows, false, false});
        const double dx = arrow.to.x - arrow.from.x;
        const double dy = arrow.to.y - arrow.from.y;
        const double length = std::hypot(dx, dy);
        if (length > 0) {
            const Point base{arrow.to.x - dx / length * 0.08, arrow.to.y - dy / length * 0.08};
            const Point normal{-dy / length * 0.035, dx / length * 0.035};
            paths.push_back({{arrow.to, {base.x + normal.x, base.y + normal.y}}, arrows, false, false});
            paths.push_back({{arrow.to, {base.x - normal.x, base.y - normal.y}}, arrows, false, false});
        }
    }
    const TriangulatedField irregular({{-1.7, -1.2}, {-0.7, -0.9}, {-1.2, -0.1}, {-0.3, -0.2}},
                                       {0, 1, 2, 3}, {{0, 1, 2}, {1, 3, 2}});
    Style mesh = lineStyle({0.7f, 0.75f, 0.85f, 0.7f}, 1);
    for (const auto triangle : irregular.triangles())
        paths.push_back({{irregular.points()[triangle[0]], irregular.points()[triangle[1]],
                          irregular.points()[triangle[2]], irregular.points()[triangle[0]]},
                         mesh, true, false});
    Style stream = lineStyle({0.2f, 0.85f, 1, 0.95f}, 1.5);
    for (const auto& line : streamlines(field, {{-1.5, -1}, {-1.5, 0}, {-1.5, 1}, {0, -1}, {0, 1}}, 0.04, 160))
        paths.push_back({line.points, stream, false, false});
    vectors = std::make_unique<Plot>();
    vectors->setPaths(std::move(paths));
    Axes axes;
    axes.x.setRange({-2, 2});
    axes.y.setRange({-1.5, 1.5});
    vectors->setAxes(axes);
    vectors->setTitle("Rectilinear vector grid / arrows / RK4 streamlines");
}

void setupExport() {
    ExportScene scene;
    scene.width = 640;
    scene.height = 420;
    scene.background = {0.04f, 0.05f, 0.07f, 1};
    scene.paths.push_back({{{40, 360}, {180, 100}, {320, 280}, {520, 60}}, false, false, {0.2f, 0.8f, 1, 1}, 3});
    scene.texts.push_back({{48, 390}, "Phase 2 export", 18, {1, 1, 1, 1}});
    writeSvg(scene, "scientific_plot_phase2.svg");
    writePdf(scene, "scientific_plot_phase2.pdf");
    std::vector<std::uint8_t> pixels(320 * 210 * 4, 0);
    for (std::size_t index = 0; index < pixels.size(); index += 4)
        pixels[index + 3] = 255;
    writePng("scientific_plot_phase2.png", 320, 210, pixels, 144);
    exportPreview = std::make_unique<Plot>();
    exportPreview->setPaths({{{{0, 0}, {1, 1}, {2, 0}}, lineStyle({0.2f, 0.8f, 1, 1}, 2), false, false}});
    Axes axes;
    axes.x.setRange({0, 2});
    axes.y.setRange({0, 1});
    exportPreview->setAxes(axes);
    exportPreview->setAnnotations({{{0.25, 0.75}, "formula text pending"}});
    exportPreview->setTitle("Shared math layout / PNG SVG PDF export");
}

void initialize() {
    setupLinkedFigure();
    setupPolar();
    setupHeatmap();
    setupContours();
    setupVectors();
    setupExport();
}

void releaseGpu() {
    if (linkedFigure) linkedFigure->releaseGpu();
    if (polar) polar->releaseGpu();
    if (heatmap) heatmap->releaseGpu();
    if (contours) contours->releaseGpu();
    if (vectors) vectors->releaseGpu();
    if (exportPreview) exportPreview->releaseGpu();
}
} // namespace

const DslAppConfig& dslAppConfig() {
    static const auto config = DslAppConfig{}
                                   .title("Scientific Plot Phase 2")
                                   .pageId("scientific_plot_phase2")
                                   .windowSize(1440, 960)
                                   .onShutdown([] {
                                       releaseGpu();
                                       linkedFigure.reset();
                                       polar.reset();
                                       heatmap.reset();
                                       contours.reset();
                                       vectors.reset();
                                       exportPreview.reset();
                                   });
    return config;
}

void compose(eui::Ui& ui, const eui::Screen& screen) {
    if (!linkedFigure)
        initialize();
    const float gap = 12;
    const float cellWidth = std::max(160.f, (screen.width - 5 * gap) / 4);
    const float cellHeight = std::max(120.f, (screen.height - 3 * gap) / 2);
    ui.stack("phase2.dashboard")
        .size(screen.width, screen.height)
        .clip()
        .content([&] {
            constexpr double renderDpi = 2;
            ui.stack("phase2.linked.cell")
                .position(0, 0)
                .size(cellWidth * 2 + gap, cellHeight)
                .content([&] { linkedFigure->compose(ui, "phase2.linked", cellWidth * 2 + gap, cellHeight, renderDpi); })
                .build();
            ui.stack("phase2.polar.cell")
                .position((cellWidth + gap) * 2, 0)
                .size(cellWidth, cellHeight)
                .content([&] { polar->compose(ui, "phase2.polar", cellWidth, cellHeight, renderDpi); })
                .build();
            ui.stack("phase2.heatmap.cell")
                .position((cellWidth + gap) * 3, 0)
                .size(cellWidth, cellHeight)
                .content([&] { heatmap->compose(ui, "phase2.heatmap", cellWidth, cellHeight, renderDpi); })
                .build();
            ui.stack("phase2.contours.cell")
                .position(0, cellHeight + gap)
                .size(cellWidth, cellHeight)
                .content([&] { contours->compose(ui, "phase2.contours", cellWidth, cellHeight, renderDpi); })
                .build();
            ui.stack("phase2.vectors.cell")
                .position(cellWidth + gap, cellHeight + gap)
                .size(cellWidth, cellHeight)
                .content([&] { vectors->compose(ui, "phase2.vectors", cellWidth, cellHeight, renderDpi); })
                .build();
            ui.stack("phase2.export.cell")
                .position((cellWidth + gap) * 2, cellHeight + gap)
                .size(cellWidth, cellHeight)
                .content([&] { exportPreview->compose(ui, "phase2.export", cellWidth, cellHeight, renderDpi); })
                .build();
        })
        .build();
}
} // namespace app