#include "modules/plot/field.h"
#include "modules/plot/contour.h"
#include "modules/plot/geometry.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
} // namespace
int main() {
    using namespace modules::plot;
    try {
        const auto nan = std::numeric_limits<double>::quiet_NaN();
        Axes axes;
        axes.x.setRange({0, 10});
        axes.y.setRange({-10, 10});
        const Viewport viewport{0, 0, 100, 100};
        Data missing({-1, 1, 2, 3, 5, 6, 12}, {0, 1, nan, -1, 0, 1, 0});
        const auto runs = selectDisplay(missing, axes, viewport);
        require(runs.size() == 2 && runs[0].size() == 2 && runs[1].size() == 4, "gap or edge neighbors lost");
        std::vector<double> x(10000), y(10000, 0);
        for (std::size_t i = 0; i < x.size(); ++i)
            x[i] = 10.0 * i / (x.size() - 1);
        y[5001] = 2;
        y[5002] = -2;
        Data dense(x, y);
        const auto selected = selectDisplay(dense, axes, viewport);
        bool peak = false, trough = false;
        for (const auto& p : selected[0]) {
            peak |= p.index == 5001;
            trough |= p.index == 5002;
        }
        require(peak && trough && selected[0].size() < 410, "pixel decimation lost extrema");
        PointIndex index;
        index.rebuild(dense, axes, viewport);
        const auto exact = axes.toScreen(dense.at(5001), viewport);
        const auto hit = index.nearest(*exact, 0.01);
        require(hit && hit->index == 5001 && hit->data.y == 2 && hit->kind == Pick::Kind::Original,
                "original index lookup");
        index.clear();
        require(!index.nearest(*exact), "index retained after clear");
        for (const auto kind : {Graph::Line, Graph::Scatter, Graph::Step, Graph::Stem, Graph::Bar,
                                Graph::Area, Graph::ErrorBars, Graph::Band}) {
            Series series;
            series.data = missing;
            series.graph = kind;
            series.lower = std::vector<double>(missing.size(), 0.2);
            series.upper = std::vector<double>(missing.size(), 0.5);
            const auto vertices = tessellate(series, axes, viewport);
            require(!vertices.empty() && vertices.size() % 3 == 0, "empty geometry");
            for (const auto& p : vertices)
                require(std::isfinite(p.x) && std::isfinite(p.y) && p.x >= 0 && p.y >= 0 && p.x <= 100 &&
                            p.y <= 100,
                        "clipping");
        }
        Series crossing;
        crossing.data = Data({-10, 20}, {0, 0});
        require(!tessellate(crossing, axes, viewport).empty(), "crossing segment disappeared");
        crossing.data = Data({0, 9, 1, 8, 2}, {-1, 1, -1, 1, -1});
        require(selectDisplay(crossing.data, axes, viewport)[0].size() == 5, "nonmonotonic data reordered");
        const auto counts = histogram({0, 1, 2, 3, 4, nan, -1}, {0, 1, 4});
        require(counts.values[0] == 1 && counts.values[1] == 4, "histogram boundaries");
        const auto density = histogram({0, 1, 2, 3, 4}, {0, 1, 4}, Normalization::Density);
        require(std::abs(density.values[0] + density.values[1] * 3 - 1) < 1e-12, "histogram normalization");
        const auto stacked = arrangeBars({1, 2}, {{1, -2}, {3, -4}}, true);
        require(stacked[1].tops.at(0).y == 4 && stacked[1].tops.at(1).y == -6 &&
                    stacked[1].baselines[1] == -2,
                "signed stacking");
        const auto grouped = arrangeBars({1}, {{1}, {2}}, false, 1);
        require(grouped[0].tops.at(0).x == 0.75 && grouped[1].tops.at(0).x == 1.25 && grouped[0].width == 0.5,
                "group positions");
        require(!bars(stacked[0].tops, stacked[0].baselines, 0.8, axes, viewport).empty(), "stack geometry");
        require(
            !pathGeometry({{-1, -1}, {2, -1}, {2, 1}, {-1, 1}}, Style{}, true, true, axes, viewport).empty(),
            "polygon clipping");
        for (const auto marker : {Marker::Square, Marker::Circle, Marker::Diamond, Marker::Cross}) {
            Series points;
            points.data = Data({5}, {0});
            points.graph = Graph::Scatter;
            points.style.marker = marker;
            require(!tessellate(points, axes, viewport).empty(), "marker");
        }
        PolarAxes polar;
        polar.setAngleUnit(AngleUnit::Degrees);
        polar.radius.setRange({0, 1});
        Series polarLine;
        polarLine.data = Data({0, 90, 180}, {1, 1, 1});
        const auto polarLineVertices = polarTessellate(polarLine, polar, viewport);
        require(!polarLineVertices.empty() && polarLineVertices.size() % 3 == 0, "polar line geometry");
        Series polarScatter = polarLine;
        polarScatter.graph = Graph::Scatter;
        require(!polarTessellate(polarScatter, polar, viewport).empty(), "polar scatter geometry");
        polarLine.graph = Graph::Bar;
        try {
            polarTessellate(polarLine, polar, viewport);
            throw std::runtime_error("unsupported polar graph");
        } catch (const std::invalid_argument&) {
        }
        ColorScale heatmapScale;
        heatmapScale.setRange({0, 6});
        ScalarField upperField(2, 3, {1, 2, 3, 4, 5, 6}, {0, 3}, {0, 2}, FieldOrigin::UpperLeft);
        const auto upperTiles = heatmapTiles(upperField, heatmapScale, viewport);
        require(upperTiles.size() == 6 && upperTiles.front().vertices.size() == 6 &&
                upperTiles.front().vertices.front().y == 0,
            "upper-origin heatmap tiles");
        ScalarField lowerField(2, 3, {1, 2, 3, 4, 5, 6}, {0, 3}, {0, 2}, FieldOrigin::LowerLeft);
        require(heatmapTiles(lowerField, heatmapScale, viewport).front().vertices.front().y == 50,
            "lower-origin heatmap tiles");
        ScalarField gridField(2, 2, {0, 2, 4, 6}, {0, 1}, {0, 1}, FieldOrigin::LowerLeft,
                      FieldSampling::GridPoints);
        require(heatmapTiles(gridField, heatmapScale, viewport).front().color == heatmapScale.map(3),
            "grid-point heatmap averaging");
        require(colorbarTiles(heatmapScale, {0, 0, 10, 100}, 8).size() == 8, "colorbar tiles");
        ScalarField contourField(2, 2, {0, 1, 1, 0}, {0, 1}, {0, 1}, FieldOrigin::LowerLeft,
                                 FieldSampling::GridPoints);
        const auto contours = marchingSquares(contourField, {0.5});
        require(contours.size() == 1 && contours[0].segments.size() == 2, "saddle contour");
        require(automaticContourLevels(ScalarField(1, 2, {0, 2}), 3) == std::vector<double>({0.5, 1, 1.5}),
            "automatic contour levels");
        require(contourLabels(contours).size() == 1, "contour label");
        const auto bands = filledContours(contourField, {0.5});
        require(bands.size() == 2 && !bands[0].triangles.empty() && !bands[1].triangles.empty(),
            "filled contours");
        ScalarField monotonicContour(2, 2, {0, 0, 1, 1}, {0, 1}, {0, 1}, FieldOrigin::LowerLeft,
                         FieldSampling::GridPoints);
        const auto monotonicBands = filledContours(monotonicContour, {0.5});
        require(!monotonicBands[0].triangles.empty(), "monotonic filled contour");
        const auto fillVertices =
            pathGeometry({monotonicBands[0].triangles[0][0], monotonicBands[0].triangles[0][1],
                          monotonicBands[0].triangles[0][2]},
                         Style{}, true, true, Axes{}, viewport);
        require(fillVertices.size() == 3 && fillVertices[0].x != fillVertices[1].x &&
                    fillVertices[0].y != fillVertices[2].y,
                "filled contour render geometry");
        ScalarField missingContour(2, 2, {0, 1, nan, 0}, {0, 1}, {0, 1}, FieldOrigin::LowerLeft,
                                   FieldSampling::GridPoints);
        require(marchingSquares(missingContour, {0.5})[0].segments.empty(), "missing contour cell");
        require(filledContours(missingContour, {0.5})[0].triangles.empty(), "missing contour fill");
        const RectilinearField rectilinear({0, 2, 5}, {0, 3}, {0, 2, 5, 3, 5, 8});
        require(rectilinear.interpolate({1, 1.5}) && *rectilinear.interpolate({1, 1.5}) == 2.5,
            "rectilinear interpolation");
        require(!rectilinear.interpolate({-1, 1}), "rectilinear boundary");
        axes.x.setRange({0, 5});
        axes.y.setRange({0, 3});
        require(rectilinearTiles(rectilinear, heatmapScale, axes, viewport).size() == 4,
            "rectilinear tiles");
        const TriangulatedField triangulated({{0, 0}, {2, 0}, {0, 2}}, {0, 2, 4}, {{0, 1, 2}});
        require(triangulated.interpolate({0.5, 0.5}) && *triangulated.interpolate({0.5, 0.5}) == 1.5,
            "triangulated interpolation");
        axes.x.setRange({0, 2});
        axes.y.setRange({0, 2});
        require(triangulatedTiles(triangulated, heatmapScale, axes, viewport).size() == 1, "triangulated tiles");
        try {
            TriangulatedField({{0, 0}, {1, 0}, {2, 0}}, {0, 1, 2}, {{0, 1, 2}});
            throw std::runtime_error("degenerate triangulated cell");
        } catch (const std::invalid_argument&) {
        }
        try {
            marchingSquares(contourField, {1, 1});
            throw std::runtime_error("invalid contour levels");
        } catch (const std::invalid_argument&) {
        }
        axes.x.setRange({0, 10});
        axes.y.setRange({-2, 2});
        Series interpolated;
        interpolated.curve = CurveKind::Interpolated;
        interpolated.data = Data({0, 2}, {0, 2});
        require(tessellate(interpolated, axes, viewport).size() >
                    tessellate(Series{Data({0, 2}, {0, 2})}, axes, viewport).size(),
                "interpolated curve");
        Series fitted;
        axes.y.setRange({-10, 10});
        fitted.curve = CurveKind::Fitted;
        fitted.data = Data({0, 1, 2, 3, 4}, {0, 10, 0, 10, 0});
        require(!tessellate(fitted, axes, viewport).empty(), "fitted curve");
        for (int shift = -20; shift <= 20; ++shift) {
            axes.x.setRange({shift / 2.0, 10 + shift / 2.0});
            Series edge;
            edge.data = Data({-100, 100}, {0, 0});
            const auto vertices = tessellate(edge, axes, viewport);
            bool left = false, right = false;
            for (auto p : vertices) {
                left |= p.x == 0;
                right |= p.x == 100;
            }
            require(left && right, "edge disappeared while panning");
        }
        std::cout << "plot_geometry: passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
