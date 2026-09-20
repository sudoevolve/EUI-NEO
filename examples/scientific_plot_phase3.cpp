#include "examples/scientific_plot_gallery.h"

namespace app {
namespace {
using namespace scientific_gallery;
std::vector<Panel> panels;
std::string message = "Six interactive views: drag orbit | Shift drag pan | Wheel zoom | Right click reset";
bool enabled = true;
double dpi = 1;
bool normals = true, lighting = true, culling = false, clipping = true, transparent = true, gaps = true;
int palette = 0;

Geometry3D quad(double slope, double offset = 0) {
    Geometry3D mesh;
    mesh.positions = {
        {-1, -1, -slope + offset}, {1, -1, slope + offset}, {1, 1, slope + offset}, {-1, 1, -slope + offset}};
    mesh.triangles = {{0, 1, 2}, {0, 2, 3}};
    computeNormals(mesh);
    return mesh;
}
RectilinearField field() {
    std::vector<double> x, y, z;
    for (int i = 0; i < 19; ++i) {
        x.push_back(-1.5 + 3.0 * i / 18);
        y.push_back(x.back());
    }
    for (auto yy : y)
        for (auto xx : x)
            z.push_back(0.65 * std::cos(2 * (xx * xx + yy * yy)));
    return {x, y, z};
}
void curves() {
    std::vector<double> x, y, z, sx, sy, sz;
    for (int i = 0; i < 100; ++i) {
        const double t = i * 0.12;
        x.push_back(std::cos(t));
        y.push_back(std::sin(t));
        z.push_back(-0.9 + i * 0.018);
        if (i % 8 == 0) {
            sx.push_back(x.back());
            sy.push_back(y.back());
            sz.push_back(z.back());
        }
        if (gaps && i >= 40 && i <= 45)
            z.back() = std::numeric_limits<double>::quiet_NaN();
    }
    Scene3D scene;
    scene.bounds = {{-1.25, -1.25, -1.1}, {1.25, 1.25, 1.1}};
    auto line = object(curve3D(x, y, z), {0.15f, 0.8f, 1, 1});
    line.material.radius = 0.018;
    auto points = object(curve3D(sx, sy, sz, true), {1, 0.65f, 0.18f, 1});
    points.material.radius = 0.06;
    scene.objects = {line, points};
    panels[0].plot->setScene(scene);
}
void surfaceColors() {
    auto v = panels[1].plot->view();
    v.colorScale.setRange({-0.65, 0.65});
    v.colorScale.setColorMap(palette % 2 ? ColorMap::Turbo : ColorMap::Viridis);
    v.colorScale.setDiscreteLevels(palette == 2 ? 8 : 0);
    panels[1].plot->setView(v);
}
void surface() {
    auto f = field();
    Scene3D scene;
    scene.bounds = {{-1.6, -1.6, -1}, {1.6, 1.6, 1}};
    auto surface = object(surface3D(f), {1, 1, 1, 0.85f}, true);
    scene.objects.push_back(surface);
    std::vector<double> values;
    for (std::size_t y = 0; y < f.rows(); ++y)
        for (std::size_t x = 0; x < f.columns(); ++x)
            values.push_back(f.value(y, x));
    ScalarField scalar(f.rows(), f.columns(), values, {-1.5, 1.5}, {-1.5, 1.5}, FieldOrigin::LowerLeft,
                       FieldSampling::GridPoints);
    auto contours = object(contourProjection3D(scalar, {-0.5, 0, 0.5}, -0.92), {1, 1, 1, 1}, true);
    contours.material.radius = 0.012;
    scene.objects.push_back(contours);
    panels[1].plot->setScene(scene);
    auto v = panels[1].plot->view();
    v.annotations = {{{0, 0, 0.65}, "peak"}};
    panels[1].plot->setView(v);
    surfaceColors();
}
void wireframe() {
    const auto f = field();
    Scene3D wireScene;
    wireScene.bounds = {{-1.6, -1.6, -1}, {1.6, 1.6, 1}};
    auto wire = object(surface3D(f, true), {0.2f, 0.85f, 0.9f, 1});
    wire.material.radius = 0.012;
    wireScene.objects.push_back(wire);
    if (normals) {
        const auto mesh = surface3D(f);
        Geometry3D lines;
        lines.primitive = Primitive3D::Lines;
        for (std::size_t i = 0; i < mesh.positions.size(); i += 4) {
            auto first = lines.positions.size();
            lines.positions.push_back(mesh.positions[i]);
            lines.positions.push_back(mesh.positions[i] + mesh.normals[i] * 0.18);
            lines.lines.push_back({first, first + 1});
        }
        auto normal = object(std::move(lines), {1, 0.7f, 0.15f, 1});
        normal.material.radius = 0.009;
        wireScene.objects.push_back(normal);
    }
    panels[2].plot->setScene(wireScene);
}
void mesh() {
    Geometry3D mesh;
    mesh.positions = {{0, 0, 1}, {-1, -0.8, -0.5}, {1, -0.8, -0.5}, {0, 1, -0.5}};
    mesh.triangles = {{0, 1, 2}, {0, 2, 3}, {0, 3, 1}, {1, 3, 2}};
    mesh.colors = {{1, 0.4f, 0.2f, 1}, {0.2f, 1, 0.5f, 1}, {0.25f, 0.4f, 1, 1}, {1, 0.9f, 0.2f, 1}};
    computeNormals(mesh);
    auto item = object(std::move(mesh));
    item.material.lighting = lighting;
    item.material.cullBackFaces = culling;
    item.material.ambient = 0.15;
    Scene3D scene;
    scene.bounds = {{-1.25, -1.1, -0.8}, {1.25, 1.25, 1.2}};
    scene.objects = {item};
    panels[3].plot->setScene(scene);
}
void intersections() {
    Scene3D scene;
    scene.bounds = {{-1.2, -1.2, -0.8}, {1.2, 1.2, 0.8}};
    scene.objects = {object(quad(0.5), {1, 0.18f, 0.2f, transparent ? 0.5f : 1.f}),
                     object(quad(-0.5), {0.15f, 0.55f, 1, transparent ? 0.5f : 1.f})};
    for (auto& item : scene.objects)
        item.material.lighting = false;
    panels[4].plot->setScene(scene);
}
void picking() {
    Scene3D scene;
    scene.bounds = {{-0.85, -1.1, -0.9}, {0.85, 1.1, 0.9}};
    scene.clipToBounds = clipping;
    auto front = quad(0, 0.2);
    front.sourceIndices = {100, 101, 102, 103};
    scene.objects = {object(std::move(front), {0.3f, 0.85f, 0.55f, 1}),
                     object(quad(0, -0.4), {1, 0.4f, 0.2f, 1})};
    panels[5].plot->setScene(scene);
    auto v = panels[5].plot->view();
    v.annotations = {{{0, 0, 0.25}, "front label"}, {{0, 0, -0.35}, "occluded label"}};
    panels[5].plot->setView(v);
}
void initialize() {
    panels.clear();
    for (auto pair : std::vector<std::pair<std::string, std::string>>{
             {"XYZ curves + scatter", "Independent XYZ, source samples, NaN breaks"},
             {"Surface + contour projection", "Scalar colors, transparency, projected isolines"},
             {"Wire grid + vertex normals", "Grid topology and normal direction"},
             {"Indexed triangle mesh", "Vertex colors, directional light, face culling"},
             {"Intersecting transparency", "Per-pixel depth order of crossing surfaces"},
             {"Depth, clipping + measurement", "Source 100..103; occluded annotation hidden"}}) {
        Panel p;
        p.title = pair.first;
        p.detail = pair.second;
        panels.push_back(std::move(p));
    }
    curves();
    surface();
    wireframe();
    mesh();
    intersections();
    picking();
    for (auto& p : panels)
        p.plot->resetView();
    for (int i : {4, 5}) {
        auto v = panels[i].plot->view();
        v.camera.yaw = 0;
        v.camera.pitch = 0;
        panels[i].plot->setView(v);
    }
}
void controls(eui::Ui& ui, std::size_t i) {
    const auto id = "control." + std::to_string(i);
    if (i == 0)
        button(ui, id + ".gap", gaps ? "Fill gap" : "NaN gap", [] {
            gaps = !gaps;
            curves();
        });
    if (i == 1)
        button(ui, id + ".map", "Color scale", [] {
            palette = (palette + 1) % 3;
            surfaceColors();
        });
    if (i == 2)
        button(
            ui, id + ".normals", normals ? "Hide normals" : "Show normals",
            [] {
                normals = !normals;
                wireframe();
            },
            110);
    if (i == 3) {
        button(ui, id + ".light", lighting ? "Light off" : "Light on", [] {
            lighting = !lighting;
            mesh();
        });
        button(ui, id + ".cull", culling ? "Cull off" : "Cull on", [] {
            culling = !culling;
            mesh();
        });
    }
    if (i == 4)
        button(
            ui, id + ".alpha", transparent ? "Opaque" : "Transparent",
            [] {
                transparent = !transparent;
                intersections();
            },
            104);
    if (i == 5)
        button(ui, id + ".clip", clipping ? "Clip off" : "Clip on", [] {
            clipping = !clipping;
            picking();
        });
    button(ui, id + ".projection", "Projection", [i] { projection(*panels[i].plot); });
}
} // namespace
const DslAppConfig& dslAppConfig() {
    static const auto config = DslAppConfig{}
                                   .title("Scientific Plot Phase 3 - 3D Gallery")
                                   .pageId("scientific_plot_phase3")
                                   .windowSize(1680, 980)
                                   .onShutdown([] { release(panels); });
    return config;
}
void compose(eui::Ui& ui, const eui::Screen& screen) {
    if (panels.empty())
        initialize();
    const float gap = 12, width = std::max(180.f, (screen.width - 4 * gap) / 3),
                height = std::max(180.f, (screen.height - 110 - 3 * gap) / 2);
    ui.stack("gallery")
        .size(screen.width, screen.height)
        .clip()
        .content([&] {
            text(ui, "title", "Scientific Plot / Phase 3 / 3D Geometry", 12, 8, 600, 22);
            ui.row("toolbar")
                .position(12, 38)
                .height(30)
                .gap(8)
                .content([&] {
                    button(ui, "reset", "Reset all", [] {
                        for (auto& p : panels)
                            p.plot->resetView();
                    });
                    button(
                        ui, "projection", "Projection all",
                        [] {
                            for (auto& p : panels)
                                projection(*p.plot);
                        },
                        114);
                    button(
                        ui, "input", enabled ? "Disable input" : "Enable input",
                        [] {
                            enabled = !enabled;
                            for (auto& p : panels)
                                p.plot->setEnabled(enabled);
                        },
                        110);
                    button(ui, "dpi", dpi == 1 ? "DPI 2x" : "DPI 1x", [] { dpi = dpi == 1 ? 2 : 1; });
                    button(
                        ui, "recreate", "Recreate views",
                        [] {
                            release(panels);
                            initialize();
                            enabled = true;
                            message = "Views and textures recreated";
                        },
                        122);
                    button(
                        ui, "export", "Export gallery",
                        [] {
                            attempt(message, "Saved scientific_plot_phase3.png / .svg / .pdf",
                                    [] { exportGallery(panels, 3, "scientific_plot_phase3"); });
                        },
                        124);
                })
                .build();
            text(ui, "help", message, 12, 76, screen.width - 24, 12);
            for (std::size_t i = 0; i < panels.size(); ++i)
                panel(ui, panels[i], i, gap + (i % 3) * (width + gap), 110 + (i / 3) * (height + gap), width,
                      height, dpi, [&, i] { controls(ui, i); });
        })
        .build();
}
} // namespace app
