#include "modules/plot/plot3d.h"
#include "modules/plot/volume.h"
#include "modules/plot/gpu_scene3d.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace modules::plot {
namespace {
std::unique_ptr<SceneRenderer3D> makeRenderer(Scene3D scene, const ViewState3D& view,
                                              const RenderSettings3D& settings) {
    for (auto& object : scene.objects)
        if (object.material.scalarColors)
            object.material.colorScale = view.colorScale;
    return std::make_unique<SceneRenderer3D>(std::move(scene), settings);
}
std::vector<ExportText> labels(const Scene3D& scene, const ViewState3D& view, const Rendered3D& frame,
                               double dpi, const GpuSceneRenderer3D* gpu = nullptr) {
    std::vector<ExportText> result;
    std::vector<std::array<double, 4>> occupied;
    auto add = [&](const Annotation3D& a, bool occlusion) {
        const auto p = view.camera.project(a.position, frame.width, frame.height);
        if (!p)
            return;
        const auto x = std::min(std::uint32_t(p->pixel.x), frame.width - 1),
                   y = std::min(std::uint32_t(p->pixel.y), frame.height - 1);
        if (occlusion &&
            p->depth > (gpu ? gpu->depthAt(x, y) : frame.depth[std::size_t(y) * frame.width + x]) +
                           length(scene.bounds.max - scene.bounds.min) * 0.003)
            return;
        Point position{p->pixel.x / dpi + 5, p->pixel.y / dpi + 16};
        if (!occlusion) {
            const double width = std::max(8.0, 7.0 * a.text.size());
            position.x = std::clamp(position.x, 0.0, std::max(0.0, frame.width / dpi - width));
            position.y = std::clamp(position.y, 12.0, std::max(12.0, frame.height / dpi));
            for (const auto& rect : occupied)
                if (position.x < rect[0] + rect[2] && position.x + width > rect[0] &&
                    position.y - 12 < rect[1] + rect[3] && position.y > rect[1])
                    return;
            occupied.push_back({position.x, position.y - 12, width, 16});
        }
        result.push_back({position, a.text, 12, {0.85f, 0.88f, 0.95f, 1}});
    };
    if (scene.showAxes)
        for (int dimension = 0; dimension < 3; ++dimension) {
            Axis axis;
            axis.setRange({scene.bounds.min[dimension], scene.bounds.max[dimension]});
            auto end = scene.bounds.min;
            end[dimension] = scene.bounds.max[dimension];
            add({end, std::string(1, "XYZ"[dimension])}, false);
            for (const auto& tick : axis.ticks(4))
                if (tick.major) {
                    auto p = scene.bounds.min;
                    p[dimension] = tick.value;
                    add({p, tick.label}, false);
                }
        }
    for (const auto& a : view.annotations)
        add(a, true);
    return result;
}
} // namespace
struct Plot3D::State {
    Scene3D scene;
    ViewState3D view;
    RenderSettings3D settings;
    Renderer texture;
    GpuSceneRenderer3D gpu;
    bool gpuFrame = false;
    std::uint64_t sceneRevision = 1, imageRevision = 0;
    std::vector<ExportText> texts;
    std::unique_ptr<SceneRenderer3D> renderer;
    Rendered3D frame;
    bool dirty = true, enabled = true, dragging = false, panning = false, measuring = false;
    float width = 0, height = 0;
    double dpi = 1, pointerScale = 1;
    std::uint64_t volumeRevision = 0;
    Camera3D dragCamera;
    Point press;
    std::optional<Pick3D> selected;
    std::optional<Vec3> measureStart;
    std::optional<Measurement3D> measurement;
    State(RenderSettings3D s, Budget b) : settings(s), texture(b), gpu(b) {}
};
Plot3D::Plot3D(RenderSettings3D s, Budget b) : state_(std::make_shared<State>(s, b)) {
    state_->renderer = makeRenderer(state_->scene, state_->view, state_->settings);
    state_->view.camera.fit(state_->scene.bounds);
}
Plot3D::~Plot3D() = default;
void Plot3D::setScene(Scene3D scene) {
    // Validate before replacing a working scene.
    auto renderer = makeRenderer(scene, state_->view, state_->settings);
    state_->scene = std::move(scene);
    state_->renderer = std::move(renderer);
    ++state_->sceneRevision;
    state_->dirty = true;
    state_->selected.reset();
    state_->measureStart.reset();
    state_->measurement.reset();
    state_->dragging = false;
}
const Scene3D& Plot3D::scene() const { return state_->scene; }
void Plot3D::setView(ViewState3D view) {
    view.camera.validate();
    for (const auto& a : view.annotations)
        if (!finite(a.position))
            throw std::invalid_argument("plot: invalid annotation");
    const auto& a = view.colorScale;
    const auto& b = state_->view.colorScale;
    if (a.colorMap() != b.colorMap() || a.mode() != b.mode() || a.automatic() != b.automatic() ||
        a.range().min != b.range().min || a.range().max != b.range().max ||
        a.discreteLevels() != b.discreteLevels() || a.missingColor() != b.missingColor()) {
        state_->renderer = makeRenderer(state_->scene, view, state_->settings);
        ++state_->sceneRevision;
    }
    state_->view = std::move(view);
    state_->dirty = true;
    state_->dragging = false;
}
const ViewState3D& Plot3D::view() const { return state_->view; }
void Plot3D::setSettings(RenderSettings3D settings) {
    auto renderer = makeRenderer(state_->scene, state_->view, settings);
    state_->settings = settings;
    state_->renderer = std::move(renderer);
    ++state_->sceneRevision;
    state_->dirty = true;
}
void Plot3D::setEnabled(bool enabled) {
    state_->enabled = enabled;
    if (!enabled) {
        state_->dragging = false;
        state_->measureStart.reset();
    }
}
void Plot3D::resetView() {
    auto& s = *state_;
    s.view.camera = Camera3D{};
    s.view.camera.fit(s.scene.bounds);
    s.dirty = true;
    s.dragging = false;
}
std::optional<Pick3D> Plot3D::selection() const { return state_->selected; }
std::optional<Measurement3D> Plot3D::measurement() const { return state_->measurement; }
Rendered3D Plot3D::render(std::uint32_t w, std::uint32_t h) const {
    return state_->renderer->render(state_->view.camera, w, h);
}
ExportScene Plot3D::exportScene(std::uint32_t w, std::uint32_t h, double textScale) const {
    if (!std::isfinite(textScale) || textScale <= 0 || textScale > 128)
        throw std::invalid_argument("plot: invalid export text scale");
    auto frame = render(w, h);
    ExportScene out;
    out.width = w;
    out.height = h;
    // The rendered raster already contains the configured background, including its alpha.
    out.background = {0, 0, 0, 0};
    out.texts = labels(state_->scene, state_->view, frame, textScale);
    for (auto& text : out.texts) {
        text.position.x *= textScale;
        text.position.y *= textScale;
        text.fontSize *= static_cast<float>(textScale);
    }
    out.rasters.push_back({{0, 0}, double(w), double(h), w, h, std::move(frame.rgba)});
    return out;
}
void Plot3D::releaseGpu() {
    state_->texture.release();
    state_->gpu.release();
    state_->frame = {};
    state_->dirty = true;
    state_->dragging = false;
}
void Plot3D::compose(eui::Ui& ui, const std::string& id, float width, float height, double dpi) {
    if (!std::isfinite(width) || !std::isfinite(height) || !std::isfinite(dpi) || width <= 0 || height <= 0 ||
        dpi <= 0 || width * dpi > 32768 || height * dpi > 32768)
        throw std::invalid_argument("plot: invalid 3D viewport");
    auto& s = *state_;
    const auto revision = s.scene.volume ? s.scene.volume->data->revision() : 0;
    if (s.width != width || s.height != height || s.dpi != dpi || s.volumeRevision != revision) {
        s.dirty = true;
        s.width = width;
        s.height = height;
        s.dpi = dpi;
        s.volumeRevision = revision;
    }
    if (s.dirty) {
        const auto w = std::uint32_t(std::ceil(width * dpi)), h = std::uint32_t(std::ceil(height * dpi));
        s.gpuFrame = s.gpu.render(*s.renderer, s.sceneRevision, s.view.camera, w, h);
        if (s.gpuFrame) {
            s.frame = {};
            s.frame.width = w;
            s.frame.height = h;
        } else {
            s.frame = render(w, h);
            s.texture.uploadRgba(w, h, s.frame.rgba);
        }
        s.texts = labels(s.scene, s.view, s.frame, dpi, s.gpuFrame ? &s.gpu : nullptr);
        ++s.imageRevision;
        s.dirty = false;
    }
    const std::weak_ptr<State> weak = state_;
    ui.stack(id)
        .size(width, height)
        .clip()
        .content([&] {
            ui.image(id + ".data")
                .size(width, height)
                .texture(s.gpuFrame ? s.gpu.image() : s.texture.image(), s.imageRevision)
                .build();
            const auto& texts = s.texts;
            for (std::size_t i = 0; i < texts.size(); ++i) {
                const auto& t = texts[i];
                ui.text(id + ".label." + std::to_string(i))
                    .position(float(t.position.x), float(t.position.y - t.fontSize))
                    .text(t.text)
                    .fontSize(t.fontSize)
                    .color({t.color[0], t.color[1], t.color[2], t.color[3]})
                    .build();
            }
            ui.rect(id + ".input")
                .size(width, height)
                .color({0, 0, 0, 0})
                .disabled(!s.enabled)
                .acceptedButtons(core::PointerButton::Left | core::PointerButton::Right)
                .onPress([weak](const core::PointerEvent& e, const core::Rect& b) {
                    if (auto s = weak.lock(); s && s->enabled) {
                        if (e.button == core::PointerButton::Right) {
                            s->view.camera = Camera3D{};
                            s->view.camera.fit(s->scene.bounds);
                            s->dirty = true;
                            s->dragging = false;
                            return;
                        }
                        s->pointerScale = b.width / s->width;
                        s->press = {(e.x - b.x) / s->pointerScale, (e.y - b.y) / s->pointerScale};
                        s->dragCamera = s->view.camera;
                        s->dragging = true;
                        s->panning = e.modifiers.shift;
                        s->measuring = e.modifiers.control;
                    }
                })
                .onDrag([weak](const core::dsl::DragEvent& e) {
                    if (auto s = weak.lock(); s && s->enabled && s->dragging && !s->measuring) {
                        auto camera = s->dragCamera;
                        const double x = e.totalX / s->pointerScale / s->height,
                                     y = e.totalY / s->pointerScale / s->height;
                        if (s->panning)
                            camera.pan(-x, y);
                        else
                            camera.orbit(-x * 3, -y * 3);
                        s->view.camera = camera;
                        s->dirty = true;
                    }
                })
                .onRelease([weak](const core::PointerEvent& e, const core::Rect& b) {
                    if (auto s = weak.lock(); s && s->dragging) {
                        s->dragging = false;
                        if (!s->enabled || e.action == core::PointerAction::Cancel)
                            return;
                        const Point p{(e.x - b.x) / s->pointerScale, (e.y - b.y) / s->pointerScale};
                        if (p.x < 0 || p.y < 0 || p.x >= s->width || p.y >= s->height ||
                            std::hypot(p.x - s->press.x, p.y - s->press.y) > 3)
                            return;
                        const auto hit = s->renderer->pick(s->view.camera, p, s->width, s->height);
                        s->selected = hit;
                        if (s->measuring && hit) {
                            if (s->measureStart) {
                                s->measurement = Measurement3D{*s->measureStart, hit->position,
                                                               length(hit->position - *s->measureStart)};
                                s->measureStart.reset();
                            } else
                                s->measureStart = hit->position;
                        }
                    }
                })
                .onScroll([weak](const core::ScrollEvent& e) {
                    if (auto s = weak.lock(); s && s->enabled) {
                        s->view.camera.zoom(std::exp(std::clamp(-e.y * 0.12, -4.0, 4.0)));
                        s->dirty = true;
                        return true;
                    }
                    return false;
                })
                .build();
        })
        .build();
}
} // namespace modules::plot
