#include "examples/scientific_plot_gallery.h"

namespace app {
namespace {
using namespace scientific_gallery;
std::vector<Panel> panels;
std::shared_ptr<VolumeData> data, largeData;
std::unique_ptr<SliceLink> cursor;
std::string message =
    "Drag orbit | Shift drag pan | Wheel zoom | Click slices then Link pick | Export saves all eight views";
double isoLevel = 0.3, angle = 0.3, shift = 0, dpi = 1;
int quality = 0, transfer = 0;
bool holes = false;

double value(Vec3 p) { return std::exp(-((p.x - shift) * (p.x - shift) + p.y * p.y + p.z * p.z) * 5); }
void createData() {
    VolumeLayout layout{{21, 21, 21}, {0.1, 0.1, 0.1}, {-1, -1, -1}};
    data = std::make_shared<VolumeData>(
        layout,
        [layout](const VolumeBrick& b, std::vector<double>& out) {
            for (std::size_t z = 0; z < b.size[2]; ++z)
                for (std::size_t y = 0; y < b.size[1]; ++y)
                    for (std::size_t x = 0; x < b.size[0]; ++x) {
                        const auto p = layout.origin + Vec3{(x + b.first[0]) * layout.spacing.x,
                                                            (y + b.first[1]) * layout.spacing.y,
                                                            (z + b.first[2]) * layout.spacing.z};
                        out[(z * b.size[1] + y) * b.size[0] + x] =
                            holes && p.x > 0.1 && p.y > 0.1 ? std::numeric_limits<double>::quiet_NaN()
                                                            : value(p);
                    }
        },
        128 * 1024, 8);
    cursor = std::make_unique<SliceLink>(layout);
    VolumeLayout large{{512, 512, 512}, {2.0 / 511, 2.0 / 511, 2.0 / 511}, {-1, -1, -1}};
    largeData = std::make_shared<VolumeData>(
        large,
        [large](const VolumeBrick& b, std::vector<double>& out) {
            for (std::size_t z = 0; z < b.size[2]; ++z)
                for (std::size_t y = 0; y < b.size[1]; ++y)
                    for (std::size_t x = 0; x < b.size[0]; ++x) {
                        const auto p = large.origin + Vec3{(x + b.first[0]) * large.spacing.x,
                                                           (y + b.first[1]) * large.spacing.y,
                                                           (z + b.first[2]) * large.spacing.z};
                        out[(z * b.size[1] + y) * b.size[0] + x] =
                            std::sin(p.x * 6) * std::cos(p.y * 6) + p.z;
                    }
        },
        1024 * 1024, 16);
}
Scene3D sliceScene(const SlicePlane& plane, const VolumeData& source) {
    Scene3D scene;
    scene.bounds = source.layout().bounds();
    auto slice = object(volumeSlice(source, plane), {1, 1, 1, 1}, true);
    slice.material.lighting = false;
    scene.objects.push_back(slice);
    return scene;
}
void updateSlices() {
    for (std::size_t i = 0; i < 3; ++i)
        panels[i].plot->setScene(sliceScene(cursor->plane(i), *data));
    const Vec3 u{std::cos(angle), std::sin(angle), 0}, v{0, 0, 1};
    const auto origin = cursor->position() - u * 0.8 - v * 0.8;
    panels[3].plot->setScene(sliceScene({origin, u, v, 1.6, 1.6, 21, 21}, *data));
}
void updateLarge() {
    SlicePlane plane{{-1, -1, cursor->position().z}, {1, 0, 0}, {0, 1, 0}, 2, 2, 25, 25};
    panels[7].plot->setScene(sliceScene(plane, *largeData));
}
TransferFunction function() {
    if (transfer)
        return TransferFunction({{0, {0, 0, 0, 0}},
                                 {0.15, {0.3f, 0.1f, 0.5f, 0}},
                                 {0.4, {1, 0.2f, 0.3f, 0.12f}},
                                 {1, {1, 0.85f, 0.2f, 0.6f}}});
    return TransferFunction({{0, {0, 0, 0, 0}},
                             {0.05, {0.1f, 0.3f, 0.9f, 0}},
                             {0.3, {0.1f, 0.8f, 1, 0.08f}},
                             {1, {1, 0.6f, 0.1f, 0.5f}}});
}
void updateVolume() {
    auto mesh = isoSurface(*data, isoLevel, 16 * 1024 * 1024);
    Scene3D iso;
    iso.bounds = data->layout().bounds();
    iso.objects.push_back(object(mesh, {0.35f, 0.8f, 1, 1}));
    panels[4].plot->setScene(iso);
    auto layer = std::make_shared<VolumeLayer>();
    layer->data = data;
    layer->referenceStep = 0.1;
    layer->transfer = function();
    Scene3D volume;
    volume.bounds = iso.bounds;
    volume.volume = layer;
    panels[5].plot->setScene(volume);
    auto shell = object(std::move(mesh), {1, 0.35f, 0.2f, 0.25f});
    volume.objects.push_back(shell);
    auto slice = object(volumeSlice(*data, cursor->plane(2)), {1, 1, 1, 0.35f}, true);
    slice.material.lighting = false;
    volume.objects.push_back(slice);
    panels[6].plot->setScene(volume);
    RenderSettings3D settings;
    settings.volumeStep = 0.16 / std::pow(2., quality);
    settings.workingBytes = 32 * 1024 * 1024;
    panels[5].plot->setSettings(settings);
    panels[6].plot->setSettings(settings);
}
void refresh() {
    data->invalidate({{0, 0, 0}, data->layout().dimensions});
    updateSlices();
    updateVolume();
    updateLarge();
}
void initialize() {
    for (auto pair : std::vector<std::pair<std::string, std::string>>{
             {"X slice / YZ plane", "Shared cursor in world coordinates"},
             {"Y slice / ZX plane", "Independent camera, linked slice location"},
             {"Z slice / XY plane", "Trilinear scalar samples and original indices"},
             {"Oblique slice", "Arbitrary orthonormal plane; rotate with Angle"},
             {"Isosurface", "Shared topology, increasing-value normals"},
             {"Volume + transfer function", "Color / opacity knots and sampling quality"},
             {"Volume + geometry", "Interleaved slice, shell and volume depth"},
             {"512 cubed / on-demand slice", "1 GiB logical Float64 source / 1 MiB LRU"}}) {
        Panel panel;
        panel.title = pair.first;
        panel.detail = pair.second;
        panels.push_back(std::move(panel));
    }
    createData();
    updateSlices();
    updateVolume();
    updateLarge();
    for (auto& panel : panels) {
        panel.plot->resetView();
        auto view = panel.plot->view();
        view.colorScale.setRange({0, 1});
        view.colorScale.setColorMap(ColorMap::Turbo);
        panel.plot->setView(view);
    }
    auto largeView = panels[7].plot->view();
    largeView.colorScale.setRange({-2, 2});
    panels[7].plot->setView(largeView);
    auto annotated = panels[4].plot->view();
    annotated.annotations = {{{0, 0, 0.6}, "isovalue surface"}};
    panels[4].plot->setView(annotated);
}
std::string settings() {
    auto p = cursor->position();
    std::ostringstream out;
    out << std::setprecision(17) << isoLevel << ' ' << angle << ' ' << shift << ' ' << quality << ' '
        << transfer << ' ' << holes << ' ' << p.x << ' ' << p.y << ' ' << p.z;
    return out.str();
}
void restoreSettings(const std::string& text) {
    double level, a, s;
    int q, t;
    bool h;
    Vec3 p;
    std::istringstream in(text);
    in >> level >> a >> s >> q >> t >> h >> p.x >> p.y >> p.z;
    if (!in || !finite(p) || !std::isfinite(a) || !std::isfinite(s) || !std::isfinite(level) ||
        level < 0.05 || level > 0.9 || q < 0 || q > 2 || t < 0 || t > 1)
        throw std::runtime_error("Invalid volume gallery settings");
    isoLevel = level;
    angle = a;
    shift = s;
    quality = q;
    transfer = t;
    holes = h;
    cursor->setPosition(p);
    refresh();
}
void moveSlice(std::size_t axis) {
    auto p = cursor->position();
    p[axis] += 0.2;
    if (p[axis] > 0.8)
        p[axis] = -0.8;
    cursor->setPosition(p);
    updateSlices();
    updateLarge();
    updateVolume();
}
void controls(eui::Ui& ui, std::size_t i) {
    const auto id = "control." + std::to_string(i);
    if (i < 3) {
        button(ui, id + ".move", std::string("Move ") + "XYZ"[i], [i] { moveSlice(i); }, 78);
        button(
            ui, id + ".link", "Link pick",
            [i] {
                if (auto hit = panels[i].plot->selection()) {
                    cursor->setPosition(hit->position);
                    updateSlices();
                    updateLarge();
                    updateVolume();
                    message = "Linked cursor to selected world position";
                } else
                    message = "Click this slice to select a point first";
            },
            82);
    }
    if (i == 3) {
        button(
            ui, id + ".angle", "Angle",
            [] {
                angle += 0.25;
                updateSlices();
            },
            78);
        button(ui, id + ".position", "Move Z", [] { moveSlice(2); }, 82);
    }
    if (i == 4) {
        button(
            ui, id + ".level", "Isovalue",
            [] {
                isoLevel = isoLevel < 0.6 ? isoLevel + 0.15 : 0.15;
                updateVolume();
                message = "Isovalue = " + std::to_string(isoLevel);
            },
            84);
        button(
            ui, id + ".holes", holes ? "Fill missing" : "Missing region",
            [] {
                holes = !holes;
                refresh();
            },
            116);
    }
    if (i == 5 || i == 6) {
        button(
            ui, id + ".quality", "Quality " + std::to_string(quality),
            [] {
                quality = (quality + 1) % 3;
                updateVolume();
            },
            86);
        button(
            ui, id + ".transfer", "Transfer",
            [] {
                transfer = 1 - transfer;
                updateVolume();
            },
            84);
    }
    if (i == 7) {
        button(
            ui, id + ".update", "Reload slice",
            [] {
                largeData->invalidate({{0, 0, 0}, largeData->layout().dimensions});
                updateLarge();
            },
            104);
        button(
            ui, id + ".budget", "Budget check",
            [] {
                attempt(message, "Unexpected budget success", [] {
                    RenderSettings3D small;
                    small.workingBytes = 1024 * 1024;
                    SceneRenderer3D(panels[7].plot->scene(), small)
                        .render(panels[7].plot->view().camera, 4096, 4096);
                });
            },
            110);
    }
}
} // namespace
const DslAppConfig& dslAppConfig() {
    static const auto config = DslAppConfig{}
                                   .title("Scientific Plot Phase 4 - Volume Gallery")
                                   .pageId("scientific_plot_phase4")
                                   .windowSize(1680, 980)
                                   .onShutdown([] {
                                       release(panels);
                                       cursor.reset();
                                       data.reset();
                                       largeData.reset();
                                   });
    return config;
}
void compose(eui::Ui& ui, const eui::Screen& screen) {
    if (panels.empty())
        initialize();
    const float gap = 10, width = std::max(180.f, (screen.width - 5 * gap) / 4),
                height = std::max(180.f, (screen.height - 140 - 3 * gap) / 2);
    ui.stack("gallery")
        .size(screen.width, screen.height)
        .clip()
        .content([&] {
            text(ui, "title", "Scientific Plot / Phase 4 / Volume & Output", 12, 8, 700, 22);
            ui.row("toolbar")
                .position(12, 38)
                .height(30)
                .gap(6)
                .content([&] {
                    button(
                        ui, "reset", "Reset all",
                        [] {
                            for (auto& p : panels)
                                p.plot->resetView();
                        },
                        88);
                    button(
                        ui, "projection", "Projection all",
                        [] {
                            for (auto& p : panels)
                                projection(*p.plot);
                        },
                        112);
                    button(
                        ui, "update", "Update data",
                        [] {
                            shift = shift == 0 ? 0.25 : 0;
                            refresh();
                            message = "Published new brick data and rebuilt slices / isosurface";
                        },
                        108);
                    button(
                        ui, "cache", "Free caches",
                        [] {
                            data->clearCache();
                            largeData->clearCache();
                            message = "Released both CPU brick caches; static images remain visible";
                        },
                        102);
                    button(
                        ui, "save", "Save state",
                        [] {
                            attempt(message,
                                    "Saved cameras, color scales, annotations, cursor, transfer and quality",
                                    [] { saveGallery(panels, "scientific_plot_phase4.view", settings()); });
                        },
                        96);
                    button(
                        ui, "restore", "Restore state",
                        [] {
                            attempt(message, "Restored complete volume gallery state", [] {
                                restoreGallery(panels, "scientific_plot_phase4.view", restoreSettings);
                            });
                        },
                        112);
                    button(
                        ui, "export", "Export gallery",
                        [] {
                            attempt(message,
                                    "Saved scientific_plot_phase4.png / .svg / .pdf at 2400 x 880, 192 DPI",
                                    [] { exportGallery(panels, 4, "scientific_plot_phase4"); });
                        },
                        118);
                    button(ui, "dpi", dpi == 1 ? "DPI 2x" : "DPI 1x", [] { dpi = dpi == 1 ? 2 : 1; }, 78);
                    button(
                        ui, "colors", "Color map",
                        [] {
                            for (auto& panel : panels) {
                                auto view = panel.plot->view();
                                view.colorScale.setColorMap(view.colorScale.colorMap() == ColorMap::Turbo
                                                                ? ColorMap::Viridis
                                                                : ColorMap::Turbo);
                                panel.plot->setView(view);
                            }
                        },
                        88);
                })
                .build();
            text(ui, "help", message, 12, 76, screen.width - 24, 12);
            std::ostringstream status;
            auto p = cursor->position();
            status << std::fixed << std::setprecision(2) << "Cursor: " << p.x << ", " << p.y << ", " << p.z
                   << " | 21^3, XYZ spacing 0.1, origin (-1,-1,-1), Float64 X-fast | Cache "
                   << data->cacheBytes() / 1024 << " / 128 KiB"
                   << " | Large " << largeData->cacheBytes() / 1024 << " / 1024 KiB, loads "
                   << largeData->loads() << " | Step " << 0.16 / std::pow(2., quality) << " | Iso "
                   << isoLevel;
            text(ui, "status", status.str(), 12, 101, screen.width - 24, 11);
            for (std::size_t i = 0; i < panels.size(); ++i)
                panel(ui, panels[i], i, gap + (i % 4) * (width + gap), 138 + (i / 4) * (height + gap), width,
                      height, dpi, [&, i] { controls(ui, i); });
        })
        .build();
}
} // namespace app
