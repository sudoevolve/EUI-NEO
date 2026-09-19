#include "modules/plot/plot3d.h"
#include "modules/plot/volume.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <map>
#include <stdexcept>

using namespace modules::plot;
namespace {
void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
void near(double a, double b, double epsilon = 1e-8) {
    require(std::abs(a - b) <= epsilon, "numeric mismatch");
}
template <class F> void rejects(F f) {
    bool rejected = false;
    try {
        f();
    } catch (const std::exception&) {
        rejected = true;
    }
    require(rejected, "invalid input accepted");
}
Camera3D camera() {
    Camera3D c;
    c.yaw = 0;
    c.pitch = 0;
    c.distance = 4;
    c.verticalSpan = 2;
    c.nearPlane = 0.01;
    c.farPlane = 10;
    return c;
}
Object3D plane(double z, std::array<float, 4> color) {
    auto g = std::make_shared<Geometry3D>();
    g->positions = {{-1, -1, z}, {1, -1, z}, {1, 1, z}, {-1, 1, z}};
    g->triangles = {{0, 1, 2}, {0, 2, 3}};
    g->sourceIndices = {40, 41, 42, 43};
    Material3D material;
    material.color = color;
    material.lighting = false;
    return {g, material, true};
}
Scene3D empty() {
    Scene3D scene;
    scene.showAxes = false;
    return scene;
}
std::array<int, 4> pixel(const Rendered3D& frame, int x, int y) {
    const auto i = (std::size_t(y) * frame.width + x) * 4;
    return {frame.rgba[i], frame.rgba[i + 1], frame.rgba[i + 2], frame.rgba[i + 3]};
}
std::shared_ptr<VolumeData> sphereVolume(std::size_t n) {
    VolumeLayout l{{n, n, n}, {2.0 / (n - 1), 2.0 / (n - 1), 2.0 / (n - 1)}, {-1, -1, -1}};
    auto data = std::make_shared<std::vector<double>>();
    data->reserve(l.sampleCount());
    for (std::size_t z = 0; z < n; ++z)
        for (std::size_t y = 0; y < n; ++y)
            for (std::size_t x = 0; x < n; ++x) {
                auto p = l.origin + Vec3{x * l.spacing.x, y * l.spacing.y, z * l.spacing.z};
                data->push_back(dot(p, p));
            }
    return std::make_shared<VolumeData>(l, data);
}
void cameraTests() {
    for (auto projection : {Projection3D::Orthographic, Projection3D::Perspective}) {
        auto c = camera();
        c.projection = projection;
        c.orbit(0.4, 0.2);
        for (Point p : std::vector<Point>{{40, 30}, {70, 50}, {50, 50}}) {
            auto ray = c.ray(p, 100, 80);
            auto world = ray.origin + ray.direction * 3;
            const auto result = c.project(world, 100, 80);
            require(bool(result), "projection clipped valid point");
            near(result->pixel.x, p.x);
            near(result->pixel.y, p.y);
        }
        const auto original = c;
        c.zoom(0.5);
        c.zoom(2);
        near(c.distance, original.distance);
        near(c.verticalSpan, original.verticalSpan);
        c.pan(0.1, -0.2);
        require(length(c.target - original.target) > 0, "pan did not move");
    }
    auto c = camera();
    require(!c.project({0, 0, 4}, 100, 100), "near clipping");
    require(!c.project({0, 0, -100}, 100, 100), "far clipping");
    c.target = {1e12, 1e12, 1e12};
    auto p = c.project(c.target, 100, 100);
    require(bool(p), "large coordinate projection");
    near(p->pixel.x, 50);
    near(p->pixel.y, 50);
    rejects([&] {
        auto invalid = c;
        invalid.nearPlane = 0;
        invalid.ray({0, 0}, 1, 1);
    });
    rejects([] { curve3D({1}, {}, {1}); });
    const double nan = std::numeric_limits<double>::quiet_NaN();
    auto line = curve3D({-0.5, 0, nan, 0, 0.5}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0});
    require(line.lines.size() == 4, "original XYZ connectivity");
    RectilinearField f({-1, 0, 1}, {-1, 0, 1}, {1, 1, 1, 1, 0, 1, 1, 1, 1});
    auto surface = surface3D(f);
    require(surface.triangles.size() == 8 && surface.normals.size() == 9, "surface grid topology");
    auto wire = surface3D(f, true);
    require(wire.lines.size() == 12, "wireframe boundaries");
    auto contours = contourProjection3D(
        ScalarField(2, 2, {0, 0, 1, 1}, {0, 1}, {0, 1}, FieldOrigin::LowerLeft, FieldSampling::GridPoints),
        {0.5}, -1);
    require(!contours.lines.empty(), "contour projection empty");
    for (auto point : contours.positions)
        near(point.z, -1);
}
void geometryTests() {
    auto scene = empty();
    scene.objects = {plane(-0.3, {0, 0, 1, 1}), plane(0.3, {1, 0, 0, 1})};
    SceneRenderer3D renderer(scene);
    auto frame = renderer.render(camera(), 64, 64);
    auto color = pixel(frame, 32, 32);
    require(color[0] == 255 && color[2] == 0, "depth test follows object order");
    near(frame.depth[32 * 64 + 32], 3.7);
    auto hit = renderer.pick(camera(), {32.5, 32.5}, 64, 64);
    require(hit && hit->object == 1 && hit->sourceIndex >= 40, "source picking");
    near(hit->position.z, 0.3);
    near(hit->weights[0] + hit->weights[1] + hit->weights[2], 1);
    std::reverse(scene.objects.begin(), scene.objects.end());
    require(SceneRenderer3D(scene).render(camera(), 64, 64).rgba == frame.rgba, "opaque order dependence");
    scene.objects = {plane(0, {1, 0, 0, 1})};
    scene.objects[0].material.cullBackFaces = true;
    auto back = camera();
    back.yaw = 3.141592653589793;
    require(pixel(SceneRenderer3D(scene).render(back, 64, 64), 32, 32)[0] < 30, "backface cull");
    auto clipped = camera();
    clipped.nearPlane = 4.1;
    require(pixel(SceneRenderer3D(scene).render(clipped, 64, 64), 32, 32)[0] < 30, "near clipped face");
    scene.bounds = {{-0.2, -1, -1}, {0.2, 1, 1}};
    frame = SceneRenderer3D(scene).render(camera(), 64, 64);
    require(pixel(frame, 4, 32)[0] < 30 && pixel(frame, 32, 32)[0] > 240, "world bounds clip");
    scene = empty();
    auto a = plane(0, {1, 0, 0, 0.5f}), b = plane(0, {0, 0, 1, 0.5f});
    auto ga = std::make_shared<Geometry3D>(*a.geometry), gb = std::make_shared<Geometry3D>(*b.geometry);
    for (auto& p : ga->positions)
        p.z = p.x * 0.4;
    for (auto& p : gb->positions)
        p.z = -p.x * 0.4;
    a.geometry = ga;
    b.geometry = gb;
    scene.objects = {a, b};
    frame = SceneRenderer3D(scene).render(camera(), 64, 64);
    const auto left = pixel(frame, 12, 32), right = pixel(frame, 52, 32);
    require(left[2] > left[0] && right[0] > right[2], "intersecting transparency was object sorted");
    std::reverse(scene.objects.begin(), scene.objects.end());
    require(SceneRenderer3D(scene).render(camera(), 64, 64).rgba == frame.rgba,
            "transparent order dependence");
    scene = empty();
    auto points = std::make_shared<Geometry3D>(curve3D({0}, {0}, {0}, true));
    Material3D m;
    m.color = {0, 1, 0, 1};
    m.lighting = false;
    m.radius = 0.2;
    scene.objects = {{points, m, true}};
    renderer = SceneRenderer3D(scene);
    require(pixel(renderer.render(camera(), 64, 64), 32, 32)[1] == 255, "point sphere");
    require(renderer.pick(camera(), {32, 32}, 64, 64)->sourceIndex == 0, "point source index");
    auto lines = std::make_shared<Geometry3D>(curve3D({-1, 1}, {0, 0}, {0, 0}));
    scene.objects = {{lines, m, true}};
    require(pixel(SceneRenderer3D(scene).render(camera(), 64, 64), 32, 32)[1] == 255, "line capsule");
    rejects([&] {
        auto bad = std::make_shared<Geometry3D>(*lines);
        bad->lines = {{0, 4}};
        auto s = empty();
        s.objects = {{bad, m, true}};
        SceneRenderer3D r(s);
    });
    RenderSettings3D limited;
    limited.workingBytes = 1024 * 1024;
    rejects([&] { SceneRenderer3D(scene, limited).render(camera(), 4096, 4096); });
    limited.maxFragmentsPerRay = 1;
    scene.objects = {plane(0.2, {1, 0, 0, 0.5f}), plane(-0.2, {0, 0, 1, 0.5f})};
    rejects([&] { SceneRenderer3D(scene, limited).render(camera(), 8, 8); });
    auto blank = SceneRenderer3D(empty()).render(camera(), 8, 8);
    require(pixel(blank, 4, 4)[0] < 30 && !std::isfinite(blank.depth[0]), "empty scene not empty");
}
void volumeTests() {
    VolumeLayout l{{4, 5, 6}, {0.5, 2, 3}, {10, 20, 30}};
    auto values = std::make_shared<std::vector<double>>();
    for (std::size_t z = 0; z < 6; ++z)
        for (std::size_t y = 0; y < 5; ++y)
            for (std::size_t x = 0; x < 4; ++x)
                values->push_back(double(x + 10 * y + 100 * z));
    VolumeData volume(l, values);
    near(volume.value(3, 4, 5), 543);
    near(*volume.sample({10.25, 21, 31.5}), 55.5);
    require(!volume.sample({9, 20, 30}), "out of bounds sampling");
    const auto gradient = volume.gradient({10.5, 22, 33});
    near(gradient.x, 2);
    near(gradient.y, 5);
    near(gradient.z, 100.0 / 3);
    SliceLink link(l);
    const auto revision = link.revision();
    link.setPosition({10.5, 22, 33});
    require(link.revision() > revision, "slice revision");
    for (std::size_t axis = 0; axis < 3; ++axis) {
        const auto plane = link.plane(axis);
        near(plane.origin[axis], link.position()[axis]);
        auto mesh = volumeSlice(volume, plane);
        require(!mesh.triangles.empty(), "axial slice empty");
        for (std::size_t i = 0; i < mesh.positions.size(); ++i) {
            near(mesh.positions[i][axis], link.position()[axis]);
            near(mesh.scalars[i], *volume.sample(mesh.positions[i]));
        }
    }
    SlicePlane oblique{{10.5, 22, 33}, {1, 1, 0}, {0, 0, 1}, 0.5, 1, 4, 4};
    auto slice = volumeSlice(volume, oblique);
    near(slice.scalars[0], 111);
    rejects([&] { volumeSlice(volume, oblique, 10); });
    std::size_t calls = 0;
    double offset = 0;
    VolumeData bricks(
        l,
        [&](const VolumeBrick& b, std::vector<double>& out) {
            ++calls;
            for (std::size_t z = 0; z < b.size[2]; ++z)
                for (std::size_t y = 0; y < b.size[1]; ++y)
                    for (std::size_t x = 0; x < b.size[0]; ++x)
                        out[(z * b.size[1] + y) * b.size[0] + x] =
                            double((x + b.first[0]) + 10 * (y + b.first[1]) + 100 * (z + b.first[2])) +
                            offset;
        },
        512, 2);
    near(bricks.value(0, 0, 0), 0);
    near(bricks.value(1, 1, 1), 111);
    require(calls == 1, "brick reuse");
    for (std::size_t z = 0; z < 6; ++z)
        for (std::size_t y = 0; y < 5; ++y)
            for (std::size_t x = 0; x < 4; ++x) {
                near(bricks.value(x, y, z), volume.value(x, y, z));
                require(bricks.cacheBytes() <= 512, "LRU exceeded budget");
            }
    const auto before = bricks.revision();
    offset = 1;
    bricks.invalidate({{0, 0, 0}, {2, 2, 2}});
    near(bricks.value(0, 0, 0), 1);
    require(bricks.revision() > before, "volume revision");
    bricks.clearCache();
    require(bricks.cacheBytes() == 0, "cache release");
    rejects([&] { VolumeData tooSmall(l, [](const VolumeBrick&, std::vector<double>&) {}, 1); });
    rejects([] { VolumeLayout{{std::numeric_limits<std::size_t>::max(), 2, 2}, {1, 1, 1}, {}}.validate(); });
    auto missing = std::make_shared<std::vector<double>>(*values);
    (*missing)[0] = std::numeric_limits<double>::quiet_NaN();
    VolumeData absent(l, missing);
    require(!absent.sample(l.origin), "NaN replaced with zero");
    near(*absent.sample({10.5, 20, 30}), 1);
    auto sphere = sphereVolume(12);
    auto iso = isoSurface(*sphere, 0.38);
    require(!iso.triangles.empty(), "isosurface empty");
    std::map<std::pair<std::size_t, std::size_t>, int> edges;
    for (auto t : iso.triangles) {
        for (int j = 0; j < 3; ++j)
            ++edges[std::minmax(t[j], t[(j + 1) % 3])];
        const auto a = iso.positions[t[0]], b = iso.positions[t[1]], c = iso.positions[t[2]];
        require(dot(cross(b - a, c - a), a + b + c) > 0, "isosurface winding");
    }
    for (const auto& edge : edges)
        require(edge.second == 2, "closed sphere has nonmanifold/boundary edge");
    require(std::ptrdiff_t(iso.positions.size()) - std::ptrdiff_t(edges.size()) +
                    std::ptrdiff_t(iso.triangles.size()) ==
                2,
            "sphere Euler characteristic");
    for (std::size_t i = 0; i < iso.positions.size(); ++i) {
        near(*sphere->sample(iso.positions[i]), 0.38, 0.025);
        require(dot(iso.positions[i], iso.normals[i]) > 0, "iso normal sign");
        require(iso.sourceIndices[i] < sphere->layout().sampleCount(), "iso original index");
    }
    require(isoSurface(*sphere, -1).triangles.empty(), "out of range iso");
    rejects([&] { isoSurface(*sphere, 0.38, 1024); });
    auto affineValues = std::make_shared<std::vector<double>>();
    for (int z = 0; z < 3; ++z)
        for (int y = 0; y < 3; ++y)
            for (int x = 0; x < 3; ++x)
                affineValues->push_back(double(x));
    VolumeData affine(VolumeLayout{{3, 3, 3}, {1, 1, 1}, {}}, affineValues);
    for (double level : {0.0, 0.5, 1.0, 1.5, 2.0}) {
        const auto mesh = isoSurface(affine, level);
        for (auto p : mesh.positions)
            near(p.x, level);
    }
    auto missingCube = std::make_shared<std::vector<double>>(
        std::initializer_list<double>{0, 1, 0, 1, 0, 1, 0, std::numeric_limits<double>::quiet_NaN()});
    VolumeData missingIso(VolumeLayout{{2, 2, 2}, {1, 1, 1}, {}}, missingCube);
    require(isoSurface(missingIso, 0.5).triangles.empty(), "isosurface bridged missing cube");
    auto constant = std::make_shared<VolumeData>(VolumeLayout{{2, 2, 2}, {2, 2, 2}, {-1, -1, -1}},
                                                 std::make_shared<const std::vector<double>>(8, 1));
    auto layer = std::make_shared<VolumeLayer>();
    layer->data = constant;
    layer->referenceStep = 1;
    layer->transfer = TransferFunction({{0, {1, 0, 0, 0.25f}}, {1, {1, 0, 0, 0.25f}}});
    auto scene = empty();
    scene.volume = layer;
    RenderSettings3D settings;
    settings.background = {0, 0, 0, 1};
    for (double step : {0.5, 0.1, 0.025}) {
        settings.volumeStep = step;
        auto rendered = SceneRenderer3D(scene, settings).render(camera(), 32, 32);
        near(pixel(rendered, 16, 16)[0] / 255.0, 1 - 0.75 * 0.75, 1.0 / 255);
        std::cout << "{\"case\":\"constant_volume\",\"step\":" << step
                  << ",\"milliseconds\":" << rendered.milliseconds
                  << ",\"samples\":" << rendered.volumeSamples << "}\n";
    }
    scene.objects = {plane(0, {0, 0, 1, 0.5f})};
    auto mixed = SceneRenderer3D(scene, settings).render(camera(), 32, 32);
    auto color = pixel(mixed, 16, 16);
    near(color[0] / 255.0, 0.25 + 0.75 * 0.5 * 0.25, 1.0 / 255);
    near(color[2] / 255.0, 0.75 * 0.5, 1.0 / 255);
    settings.maxStepsPerRay = 1;
    rejects([&] { SceneRenderer3D(scene, settings).render(camera(), 8, 8); });
    TransferFunction transfer({{0, {0, 0, 0, 0}}, {2, {1, 0.5f, 0.25f, 1}}});
    near(transfer.map(1)[0], 0.5);
    near(transfer.map(1)[3], 0.5);
    near(transfer.map(std::numeric_limits<double>::infinity())[3], 0);
    rejects([] { TransferFunction({{1, {0, 0, 0, 0}}, {0, {1, 1, 1, 1}}}); });
}
void stateTests() {
    ViewState3D v;
    v.camera = camera();
    v.camera.target = {0.1, -0.2, 0.3};
    v.camera.projection = Projection3D::Perspective;
    v.colorScale.setMode(ColorScaleMode::Log10);
    v.colorScale.setRange({0.01, 100});
    v.colorScale.setColorMap(ColorMap::Turbo);
    v.colorScale.setDiscreteLevels(8);
    v.annotations.push_back({{0.1, 0.2, 0.3}, "a (test) \\\" quoted\nline"});
    const auto text = saveView3D(v);
    const auto restored = restoreView3D(text);
    require(saveView3D(restored) == text, "view roundtrip");
    rejects([&] { restoreView3D(text + "garbage"); });
    rejects([] { restoreView3D("EUI_PLOT_VIEW 999"); });
    auto scene = empty();
    scene.objects = {plane(0, {1, 0, 0, 1})};
    Plot3D plot;
    plot.setScene(scene);
    plot.setView(v);
    auto image = plot.render(32, 32);
    plot.setView(restored);
    require(plot.render(32, 32).rgba == image.rgba, "restored view pixels");
    auto exportScene = plot.exportScene(64, 48);
    require(exportScene.rasters.size() == 1 && exportScene.rasters[0].rgba.size() == 64 * 48 * 4,
            "export rerender dimensions");
    v.camera = camera();
    v.annotations = {{{0, 0, -0.5}, "occluded"}, {{0, 0, 0.5}, "visible"}};
    plot.setView(v);
    const auto annotations = plot.exportScene(64, 64).texts;
    require(annotations.size() == 1 && annotations[0].text == "visible", "annotation depth occlusion");
    const auto scaled = plot.exportScene(64, 64, 2);
    near(scaled.texts.front().fontSize, 24);
    rejects([&] { plot.exportScene(64, 64, 0); });
    RenderSettings3D transparent;
    transparent.background = {0, 1, 0, 0.5f};
    plot.setScene(empty());
    plot.setSettings(transparent);
    const auto alpha = plot.exportScene(32, 32);
    near(alpha.background[3], 0);
    require(alpha.rasters[0].rgba[3] == 128, "export background applied twice");
    plot.setScene(scene);
    const auto highResolution=plot.exportScene(1200,900,2);
    require(highResolution.rasters[0].pixelWidth==1200&&highResolution.rasters[0].pixelHeight==900,
            "high resolution export dimensions");
    const auto font=std::filesystem::path(__FILE__).parent_path().parent_path().parent_path()/
                    "assets/JingNanJunJunTi-JinNanJunJunTi-Bold-2.ttf";
    writeRasterPng(highResolution,"plot_spatial_export.png",font.string(),192);
    writeSvg(highResolution,"plot_spatial_export.svg");
    writePdf(highResolution,"plot_spatial_export.pdf");
    std::remove("plot_spatial_export.png");std::remove("plot_spatial_export.svg");std::remove("plot_spatial_export.pdf");
}
} // namespace
int main() {
    try {
        cameraTests();
        geometryTests();
        volumeTests();
        stateTests();
        std::cout << "plot_spatial: passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "plot_spatial: " << e.what() << '\n';
        return 1;
    }
}
