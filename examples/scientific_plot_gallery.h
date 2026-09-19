#pragma once

#include "eui_neo.h"
#include "modules/plot/plot3d.h"
#include "modules/plot/volume.h"
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace scientific_gallery {
using namespace modules::plot;
struct Panel {
    std::string title, detail;
    std::unique_ptr<Plot3D> plot = std::make_unique<Plot3D>();
};
inline Object3D object(Geometry3D geometry, std::array<float, 4> color = {1, 1, 1, 1}, bool scalar = false) {
    Material3D material;
    material.color = color;
    material.scalarColors = scalar;
    return {std::make_shared<Geometry3D>(std::move(geometry)), material, true};
}
inline void button(eui::Ui& ui, const std::string& id, const std::string& text, std::function<void()> action,
                   float width = 94) {
    components::button(ui, id).size(width, 28).text(text).onClick(std::move(action)).build();
}
inline void text(eui::Ui& ui, const std::string& id, const std::string& value, float x, float y, float width,
                 float size = 12) {
    ui.text(id)
        .position(x, y)
        .size(width, 20)
        .text(value)
        .fontSize(size)
        .color({0.82f, 0.86f, 0.94f, 1})
        .build();
}
inline std::string selection(const Plot3D& plot) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(3);
    if (auto hit = plot.selection())
        out << "Source " << hit->sourceIndex << " | XYZ " << hit->position.x << ", " << hit->position.y
            << ", " << hit->position.z;
    else
        out << "Click: source index | Ctrl + two clicks: distance";
    if (auto measure = plot.measurement())
        out << " | d=" << measure->distance;
    return out.str();
}
inline void panel(eui::Ui& ui, Panel& panel, std::size_t index, float x, float y, float width, float height,
                  double dpi, const std::function<void()>& controls) {
    const auto id = "panel." + std::to_string(index);
    ui.stack(id)
        .position(x, y)
        .size(width, height)
        .clip()
        .content([&] {
            ui.rect(id + ".background")
                .size(width, height)
                .color({0.055f, 0.07f, 0.10f, 1})
                .radius(8)
                .build();
            text(ui, id + ".title", panel.title, 10, 6, width - 20, 15);
            text(ui, id + ".detail", panel.detail, 10, 29, width - 20, 10);
            ui.row(id + ".controls").position(10, 52).size(width - 20, 28).gap(6).content(controls).build();
            ui.stack(id + ".viewport")
                .position(4, 86)
                .size(width - 8, height - 114)
                .content([&] { panel.plot->compose(ui, id + ".plot", width - 8, height - 114, dpi); })
                .build();
            text(ui, id + ".selection", selection(*panel.plot), 8, height - 24, width - 16, 10);
        })
        .build();
}
inline void release(std::vector<Panel>& panels) {
    for (auto& panel : panels)
        panel.plot->releaseGpu();
    panels.clear();
}
inline void fit(Panel& panel, Scene3D scene) {
    panel.plot->setScene(std::move(scene));
    panel.plot->resetView();
}
inline void projection(Plot3D& plot) {
    auto view = plot.view();
    view.camera.projection = view.camera.projection == Projection3D::Perspective ? Projection3D::Orthographic
                                                                                 : Projection3D::Perspective;
    plot.setView(view);
}
inline void exportGallery(const std::vector<Panel>& panels, std::size_t columns,
                          const std::string& filename) {
    constexpr int width = 600, height = 440;
    ExportScene page;
    page.width = width * columns;
    page.height = height * ((panels.size() + columns - 1) / columns);
    page.background = {0.04f, 0.05f, 0.07f, 1};
    for (std::size_t i = 0; i < panels.size(); ++i) {
        const double x = (i % columns) * width, y = (i / columns) * height;
        auto scene = panels[i].plot->exportScene(width - 20, height - 70, 1.5);
        for (auto& raster : scene.rasters) {
            raster.position.x += x + 10;
            raster.position.y += y + 60;
            page.rasters.push_back(std::move(raster));
        }
        for (auto& label : scene.texts) {
            label.position.x += x + 10;
            label.position.y += y + 60;
            page.texts.push_back(std::move(label));
        }
        page.texts.push_back({{x + 12, y + 25}, panels[i].title, 20, {1, 1, 1, 1}});
        page.texts.push_back({{x + 12, y + 46}, panels[i].detail, 12, {0.8f, 0.84f, 0.9f, 1}});
    }
    writeRasterPng(page, filename + ".png", "assets/JingNanJunJunTi-JinNanJunJunTi-Bold-2.ttf", 192);
    writeSvg(page, filename + ".svg");
    writePdf(page, filename + ".pdf");
}
inline void saveGallery(const std::vector<Panel>& panels, const std::string& filename,
                        const std::string& settings) {
    std::ofstream out(filename);
    out << "EUI_GALLERY 1 " << panels.size() << '\n' << std::quoted(settings) << '\n';
    for (const auto& panel : panels)
        out << std::quoted(saveView3D(panel.plot->view())) << '\n';
    if (!out)
        throw std::runtime_error("Cannot save gallery state");
}
inline void restoreGallery(std::vector<Panel>& panels, const std::string& filename,
                           const std::function<void(const std::string&)>& settings) {
    std::ifstream in(filename);
    std::string magic, payload;
    int version = 0;
    std::size_t count = 0;
    in >> magic >> version >> count >> std::quoted(payload);
    if (!in || magic != "EUI_GALLERY" || version != 1 || count != panels.size())
        throw std::runtime_error("Invalid gallery state");
    std::vector<ViewState3D> views;
    for (std::size_t i = 0; i < count; ++i) {
        std::string view;
        in >> std::quoted(view);
        if (!in)
            throw std::runtime_error("Incomplete gallery state");
        views.push_back(restoreView3D(view));
    }
    settings(payload);
    for (std::size_t i = 0; i < count; ++i)
        panels[i].plot->setView(std::move(views[i]));
}
inline void attempt(std::string& message, const std::string& success, const std::function<void()>& action) {
    try {
        action();
        message = success;
    } catch (const std::exception& e) {
        message = e.what();
    }
}
} // namespace scientific_gallery
