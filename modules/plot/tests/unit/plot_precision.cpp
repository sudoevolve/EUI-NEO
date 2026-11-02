#include "modules/plot/contour.h"
#include "modules/plot/geometry.h"
#include "modules/plot/waveform.h"
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <random>
#include <set>
#include <stdexcept>
using namespace modules::plot;
namespace {
void require(bool c, const char* m) {
    if (!c)
        throw std::runtime_error(m);
}
void near(double a, double b, double tolerance = 1e-6) {
    require(std::isfinite(a) && std::abs(a - b) <= tolerance, "precision mismatch");
}
void colors() {
    ColorScale c;
    c.setColorMap(ColorMap::Grayscale);
    c.setRange({-1e308, 1e308});
    near(c.map(-1e308)[0], 0);
    near(c.map(0)[0], .5);
    near(c.map(1e308)[0], 1);
    const auto bar = colorbarTiles(c, {0, 0, 10, 100}, 2);
    near(bar[0].color[0], .25);
    near(bar[1].color[0], .75);
    const TriangulatedField triangle({{0, 0}, {1, 0}, {0, 1}}, {1e308, 1e308, 1e308}, {{0, 1, 2}});
    Axes axes;
    axes.x.setRange({0, 1});
    axes.y.setRange({0, 1});
    near(triangulatedTiles(triangle, c, axes, {0, 0, 100, 100})[0].color[0], 1);
    c.setMode(ColorScaleMode::Linear);
    c.resetAuto();
    for (double value : {std::numeric_limits<double>::max(), -std::numeric_limits<double>::max(),
                         std::numeric_limits<double>::denorm_min()}) {
        c.fit(Range{value, value});
        auto r = c.range();
        require(std::isfinite(r.min) && std::isfinite(r.max) && r.min < r.max, "constant color extent");
        require(c.map(value) != c.missingColor(), "finite constant became missing");
    }
    c.setMode(ColorScaleMode::Log10);
    c.setRange({1e12, std::nextafter(1e12, INFINITY)});
    near(c.map(c.range().min)[0], 0);
    near(c.map(c.range().max)[0], 1);
    c.resetAuto();
    c.fit(Range{std::numeric_limits<double>::denorm_min(), std::numeric_limits<double>::denorm_min()});
    require(c.range().min > 0 && c.range().min < c.range().max, "subnormal log color range");
    c.setMode(ColorScaleMode::Linear);
    c.setRange({0, 1});
    c.setColorMap(ColorMap::Viridis);
    auto v = c.map(128.0 / 255);
    near(v[0], .127568);
    near(v[1], .566949);
    near(v[2], .550556);
    c.setColorMap(ColorMap::Plasma);
    v = c.map(128.0 / 255);
    near(v[0], .798216);
    near(v[1], .280197);
    near(v[2], .469538);
    c.setColorMap(ColorMap::Turbo);
    v = c.map(0);
    near(v[0], .18995);
    near(v[1], .07176);
    near(v[2], .23217);
}
void contours() {
    for (bool negate : {false, true}) {
        const double sign = negate ? -1 : 1;
        ScalarField f(2, 2, {-sign, 2 * sign, 2 * sign, -sign}, {0, 1}, {0, 1}, FieldOrigin::LowerLeft,
                      FieldSampling::GridPoints);
        auto lines = marchingSquares(f, {0})[0].segments;
        require(lines.size() == 2, "saddle segment count");
        bool bottomLeft = false;
        for (auto s : lines)
            bottomLeft |= (s.from.y == 0 && s.to.x == 0) || (s.to.y == 0 && s.from.x == 0);
        require(bottomLeft, "saddle topology is not sign invariant");
    }
    auto levels = automaticContourLevels(ScalarField(1, 2, {-1e308, 1e308}), 3);
    ScalarField dynamic(2, 2, {1e300, -1e100, -1e-100, 0.5e-300}, {0, 1}, {0, 1}, FieldOrigin::LowerLeft,
                        FieldSampling::GridPoints);
    bool bottomLeft = false;
    const auto dynamicLines = marchingSquares(dynamic, {0});
    for (auto s : dynamicLines[0].segments)
        bottomLeft |= (s.from.y == 0 && s.to.x == 0) || (s.to.y == 0 && s.from.x == 0);
    require(bottomLeft, "saddle product underflow");
    require(levels.size() == 3, "large contour count");
    near(levels[0] / 1e308, -.5);
    near(levels[1], 0);
    near(levels[2] / 1e308, .5);
    levels = automaticContourLevels(ScalarField(1, 2, {1, std::nextafter(1., 2.)}), 3);
    require(levels.empty(), "unrepresentable internal contour levels");
    ScalarField f(2, 2, {-1e308, 1e308, -1e308, 1e308}, {-1e308, 1e308}, {-1, 1}, FieldOrigin::LowerLeft,
                  FieldSampling::GridPoints);
    const auto hugeLines = marchingSquares(f, {0});
    for (auto s : hugeLines[0].segments) {
        near(s.from.x, 0);
        near(s.to.x, 0);
    }
}
void geometry() {
    Axes joinAxes;
    joinAxes.x.setRange({0, 100});
    joinAxes.y.setRange({0, 100});
    Series corner;
    corner.data = Data({20, 50, 50}, {50, 50, 80});
    corner.style.lineWidth = 10;
    const auto area = [](const std::vector<Vertex>& vertices) {
        double result = 0;
        for (std::size_t i = 0; i + 2 < vertices.size(); i += 3) {
            const auto a = vertices[i], b = vertices[i + 1], c = vertices[i + 2];
            result += std::abs((b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x)) / 2;
        }
        return result;
    };
    corner.style.join = LineJoin::Miter;
    const auto miter = tessellate(corner, joinAxes, {0, 0, 100, 100});
    corner.style.join = LineJoin::Bevel;
    const auto bevel = tessellate(corner, joinAxes, {0, 0, 100, 100});
    corner.style.join = LineJoin::Round;
    const auto round = tessellate(corner, joinAxes, {0, 0, 100, 100});
    near(area(miter), 600, 1e-4);
    near(area(bevel), 587.5, 1e-4);
    require(area(round) > area(bevel) && area(round) < area(miter), "round join geometry");
    corner.style.cap = LineCap::Square;
    near(area(tessellate(corner, joinAxes, {0, 0, 100, 100})) - area(round), 100, 1e-3);
    corner.style.cap = LineCap::Round;
    near(area(tessellate(corner, joinAxes, {0, 0, 100, 100})) - area(round), 78.036, 0.01);
    corner.data = Data({20, 80, 80, 20, 20}, {20, 20, 80, 80, 20});
    corner.style.join = LineJoin::Miter;
    near(area(tessellate(corner, joinAxes, {0, 0, 100, 100})), 2400, 1e-3);
    corner.data = Data({10, 80, 10}, {50, 50, 50.00001});
    for (auto join : {LineJoin::Miter, LineJoin::Bevel, LineJoin::Round}) {
        corner.style.join = join;
        for (auto v : tessellate(corner, joinAxes, {0, 0, 100, 100}))
            require(std::isfinite(v.x) && std::isfinite(v.y), "near reversal join");
    }
    Axes axes;
    axes.x.setRange({1e308, 1.2e308});
    axes.y.setRange({0, 3});
    Series s;
    s.data = Data({1e308, 1.1e308}, {1, 2});
    s.curve = CurveKind::Interpolated;
    require(!tessellate(s, axes, {0, 0, 200, 100}).empty(), "finite interpolated curve vanished");
    axes.x.setRange({0, 4});
    axes.y.setRange({-1e308, 1e308});
    s.data = Data({0, 1, 2, 3, 4}, {1e308, 1e308, 1e308, 1e308, 1e308});
    s.curve = CurveKind::Fitted;
    require(!tessellate(s, axes, {0, 0, 200, 100}).empty(), "finite smoothed curve overflow");
}
void ticks() {
    Axis a;
    a.setRange({1e12, 1e12 + .01});
    for (auto scale : {Scale::Linear, Scale::Log10}) {
        a.setScale(scale);
        std::set<std::string> labels;
        std::size_t count = 0;
        for (auto t : a.ticks())
            if (t.major) {
                ++count;
                labels.insert(t.label);
            }
        require(count >= 2 && labels.size() == count, "distinct major ticks have identical labels");
    }
    a.formatter = [](double) { return std::string("custom"); };
    for (auto t : a.ticks())
        if (t.major)
            require(t.label == "custom", "formatter overridden");
}
template <class T> void integerSamples(SampleFormat format) {
    WaveformBuffer::Config config;
    config.channels = 1;
    config.sampleRate = 125000000;
    config.retainedSamples = 65536;
    config.blockSamples = 65536;
    config.spareBlocks = 2;
    config.budgetBytes = 8 * 1024 * 1024;
    config.format = format;
    WaveformBuffer buffer(config);
    std::vector<T> samples(65536);
    const auto minimum = static_cast<long long>(std::numeric_limits<T>::min());
    const auto count =
        static_cast<unsigned long long>(static_cast<long long>(std::numeric_limits<T>::max()) - minimum + 1);
    for (std::size_t i = 0; i < samples.size(); ++i)
        samples[i] = static_cast<T>(minimum + static_cast<long long>(i % count));
    require(buffer.tryAppend(samples.data(), samples.size() * sizeof(T)), "integer append");
    const auto snapshot = buffer.snapshot();
    for (std::size_t i = 0; i < samples.size(); ++i)
        require(snapshot.channels[0].at(i).y == static_cast<double>(samples[i]), "integer sample precision");
}
void storagePrecision() {
    integerSamples<std::uint8_t>(SampleFormat::UInt8);
    integerSamples<std::int16_t>(SampleFormat::Int16);
    integerSamples<std::uint16_t>(SampleFormat::UInt16);
    std::mt19937 random(20261101);
    std::vector<double> values;
    for (int i = 0; i < 100000; ++i) {
        const std::uint32_t bits = random();
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        if (std::isfinite(value))
            values.push_back(static_cast<double>(value));
    }
    values.insert(values.end(),
                  {0., -0., std::numeric_limits<double>::denorm_min(), std::numeric_limits<double>::max(),
                   -std::numeric_limits<double>::max(), 1e-300, 1e300});
    const Data data(values, values);
    for (std::size_t i = 0; i < values.size(); ++i) {
        const auto point = data.at(i);
        require(std::memcmp(&point.y, &values[i], sizeof(double)) == 0 &&
                std::memcmp(&point.x, &values[i], sizeof(double)) == 0, "sample storage changed bits");
    }
}
} // namespace
int main() {
    try {
        storagePrecision();
        colors();
        contours();
        geometry();
        ticks();
        std::cout << "plot_precision: passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
