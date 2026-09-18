#include "modules/plot/plot.h"

#include "core/render/text.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace modules::plot {
namespace {
bool transformAxes(Axes& axes, Point anchor, double factor, Point shift) {
    Axes next = axes;
    const auto change = [&](Axis& axis, double center, double delta) {
        auto a = axis.denormalize(center + (0 - center) * factor + delta);
        auto b = axis.denormalize(center + (1 - center) * factor + delta);
        if (!a || !b || *a == *b)
            return false;
        axis.setRange({std::min(*a, *b), std::max(*a, *b)});
        return true;
    };
    if (!change(next.x, anchor.x, shift.x) || !change(next.y, anchor.y, shift.y))
        return false;
    axes = next;
    return true;
}
void appendLine(Batch& batch, Point a, Point b, float width = 1) {
    const double length = std::hypot(b.x - a.x, b.y - a.y);
    if (length == 0)
        return;
    const double nx = -(b.y - a.y) / length * width / 2, ny = (b.x - a.x) / length * width / 2;
    const Vertex p[] = {{float(a.x + nx), float(a.y + ny)},
                        {float(b.x + nx), float(b.y + ny)},
                        {float(b.x - nx), float(b.y - ny)},
                        {float(a.x - nx), float(a.y - ny)}};
    for (int i : {0, 1, 2, 0, 2, 3})
        batch.vertices.push_back(p[i]);
}
} // namespace

struct Plot::State {
    explicit State(Budget budget) : renderer(budget) {}
    Axes axes;
    std::vector<Series> series;
    Renderer renderer;
    std::vector<PointIndex> indexes;
    bool dirty = true, enabled = true, dragging = false, boxZoom = false, measuring = false;
    Axes dragAxes;
    Point pointer, press;
    Viewport viewport;
    double dpi = 0, pointerScale = 1;
    std::string title;
    std::optional<Probe> probe;
    std::optional<Probe> selected;
    std::optional<Point> measureStart;
    std::optional<Measurement> measurement;
    std::vector<Path> paths;
    std::vector<Annotation> annotations;
    void invalidate() { dirty = true; }
    static Axes fitted(Axes axes, const std::vector<Series>& series) {
        std::vector<Data> visible;
        for (const auto& value : series)
            if (value.visible) {
                visible.push_back(value.data);
                // 自动范围需要包括柱宽、基线和误差包络，而不只是中心采样点。
                std::vector<double> x, y;
                const bool extra = value.graph == Graph::Bar || value.graph == Graph::Area ||
                                   value.graph == Graph::Stem || value.graph == Graph::ErrorBars ||
                                   value.graph == Graph::Band;
                for (std::size_t i = 0; extra && i < value.data.size(); ++i) {
                    const auto p = value.data.at(i);
                    if (!std::isfinite(p.x) || !std::isfinite(p.y))
                        continue;
                    if (value.graph == Graph::Bar) {
                        x.push_back(p.x - value.style.barWidth / 2);
                        y.push_back(p.y);
                        x.push_back(p.x + value.style.barWidth / 2);
                        y.push_back(p.y);
                    }
                    if (value.graph == Graph::Bar || value.graph == Graph::Area ||
                        value.graph == Graph::Stem) {
                        x.push_back(p.x);
                        y.push_back(value.baselines.empty() ? value.style.baseline : value.baselines.at(i));
                    }
                    if (value.graph == Graph::ErrorBars || value.graph == Graph::Band) {
                        if (value.lower.size() != value.data.size() ||
                            value.upper.size() != value.data.size())
                            throw std::invalid_argument("plot: invalid interval lengths");
                        x.push_back(p.x);
                        y.push_back(value.graph == Graph::Band ? value.lower[i] : p.y - value.lower[i]);
                        x.push_back(p.x);
                        y.push_back(value.graph == Graph::Band ? value.upper[i] : p.y + value.upper[i]);
                    }
                }
                if (!x.empty())
                    visible.emplace_back(x, y);
            }
        axes.fit(visible);
        return axes;
    }
    void fit() { axes = fitted(axes, series); }
    void updateProbe(Point local) {
        probe.reset();
        for (std::size_t i = 0; i < indexes.size(); ++i) {
            const auto hit = indexes[i].nearest(local);
            if (hit && (!probe || hit->distance < probe->point.distance))
                probe = Probe{i, *hit};
        }
    }
};
Plot::Plot(Budget budget) : state_(std::make_shared<State>(budget)) {}
Plot::~Plot() = default;
void Plot::setSeries(std::vector<Series> series) {
    const auto axes = State::fitted(state_->axes, series);
    state_->series = std::move(series);
    state_->axes = axes;
    state_->probe.reset();
    state_->selected.reset();
    state_->invalidate();
}
const std::vector<Series>& Plot::series() const { return state_->series; }
void Plot::setAxes(Axes axes) {
    state_->axes = State::fitted(std::move(axes), state_->series);
    state_->invalidate();
}
const Axes& Plot::axes() const { return state_->axes; }
void Plot::setVisible(std::size_t series, bool visible) {
    state_->series.at(series).visible = visible;
    state_->probe.reset();
    state_->fit();
    state_->invalidate();
    if (state_->selected && state_->selected->series == series)
        state_->selected.reset();
}
void Plot::setEnabled(bool enabled) {
    state_->enabled = enabled;
    if (!enabled) {
        state_->dragging = false;
        state_->probe.reset();
    }
}
void Plot::setTitle(std::string title) { state_->title = std::move(title); }
void Plot::setPaths(std::vector<Path> paths) {
    state_->paths = std::move(paths);
    state_->invalidate();
}
void Plot::setAnnotations(std::vector<Annotation> annotations) {
    state_->annotations = std::move(annotations);
}
std::optional<Probe> Plot::selection() const { return state_->selected; }
std::optional<Measurement> Plot::measurement() const { return state_->measurement; }
void Plot::resetView() {
    state_->axes.x.resetAuto();
    state_->axes.y.resetAuto();
    state_->fit();
    state_->invalidate();
}
void Plot::pan(double horizontal, double vertical) {
    if (transformAxes(state_->axes, {}, 1, {horizontal, vertical}))
        state_->invalidate();
}
void Plot::zoom(Point anchor, double factor) {
    if (std::isfinite(factor) && factor > 0 && transformAxes(state_->axes, anchor, factor, {}))
        state_->invalidate();
}
std::optional<Probe> Plot::probe() const { return state_->probe; }
void Plot::releaseGpu() {
    state_->renderer.release();
    state_->indexes.clear();
    state_->dirty = true;
}

void Plot::compose(eui::Ui& ui, const std::string& id, float width, float height, double dpi) {
    if (!std::isfinite(width) || !std::isfinite(height) || width < 160 || height < 120 ||
        !std::isfinite(dpi) || dpi <= 0)
        throw std::invalid_argument("plot: viewport needs at least 160x120 logical pixels");
    auto& state = *state_;
    const auto yticks = state.axes.y.ticks(std::clamp(int(height / 60), 2, 10));
    float labelWidth = 0;
    for (const auto& tick : yticks)
        if (tick.major)
            labelWidth = std::max(labelWidth, core::TextPrimitive::measureTextWidth(tick.label, {}, 12));
    const float left = std::min(width * 0.4f, std::max(50.f, labelWidth + 16));
    const bool legend = std::any_of(state.series.begin(), state.series.end(),
                                    [](const Series& s) { return !s.name.empty(); });
    const float top =
        (state.title.empty() && state.axes.y.label.empty() ? 16.f : 40.f) + (legend ? 24.f : 0.f);
    const float bottom = state.axes.x.label.empty() ? 38.f : 58.f;
    const float plotWidth = width - left - 18, plotHeight = height - top - bottom;
    if (plotWidth <= 0 || plotHeight <= 0)
        return;
    const Viewport viewport{0, 0, plotWidth, plotHeight};
    const auto xticks = state.axes.x.ticks(std::clamp(int(plotWidth / 90), 2, 10));
    if (state.viewport.width != plotWidth || state.viewport.height != plotHeight || state.dpi != dpi)
        state.dirty = true;
    if (state.dirty) {
        state.viewport = viewport;
        state.dpi = dpi;
        std::vector<Batch> batches;
        Batch grid{{}, {0.16f, 0.18f, 0.22f, 1}};
        for (const auto& tick : xticks)
            if (tick.major) {
                const auto p = state.axes.x.normalize(tick.value);
                if (p)
                    appendLine(grid, {*p * plotWidth, 0}, {*p * plotWidth, plotHeight});
            }
        for (const auto& tick : yticks)
            if (tick.major) {
                const auto p = state.axes.y.normalize(tick.value);
                if (p)
                    appendLine(grid, {0, (1 - *p) * plotHeight}, {plotWidth, (1 - *p) * plotHeight});
            }
        batches.push_back(std::move(grid));
        state.indexes.resize(state.series.size());
        for (std::size_t i = 0; i < state.series.size(); ++i) {
            const auto& series = state.series[i];
            if (series.visible) {
                batches.push_back({tessellate(series, state.axes, viewport), series.style.color});
                // Missing samples remain gaps; draw a small marker at the baseline in the configured
                // missing-value color so an invalid sample is visible without inventing a Y value.
                batches.push_back({missingGeometry(series, state.axes, viewport), series.style.missingColor});
                state.indexes[i].rebuild(series.data, state.axes, viewport);
            } else
                state.indexes[i].clear();
        }
        for (const auto& path : state.paths) {
            batches.push_back(
                {pathGeometry(path.points, path.style, path.closed, path.filled, state.axes, viewport),
                 path.style.color});
        }
        state.renderer.render(batches, plotWidth, plotHeight, dpi);
        state.dirty = false;
    }
    const std::weak_ptr<State> weak = state_;
    ui.stack(id)
        .size(width, height)
        .clip()
        .content([&] {
            ui.image(id + ".data")
                .position(left, top)
                .size(plotWidth, plotHeight)
                .texture(state.renderer.image(), state.renderer.revision())
                .flipVertically()
                .build();
            const auto label = [&](const std::string& name, const std::string& text, float x, float y,
                                   float w) {
                ui.text(id + name)
                    .position(x, y)
                    .size(w, 18)
                    .text(text)
                    .fontSize(12)
                    .color({0.8f, 0.83f, 0.88f, 1})
                    .build();
            };
            float previousEnd = -1;
            for (std::size_t i = 0; i < xticks.size(); ++i) {
                const auto& tick = xticks[i];
                if (!tick.major)
                    continue;
                const auto p = state.axes.x.normalize(tick.value);
                if (!p)
                    continue;
                const float w = core::TextPrimitive::measureTextWidth(tick.label, {}, 12);
                const float x =
                    std::clamp(left + float(*p) * plotWidth - w / 2, 0.f, std::max(0.f, width - w));
                if (x < previousEnd + 4)
                    continue;
                label(".xtick." + std::to_string(i), tick.label, x, top + plotHeight + 5, w + 2);
                previousEnd = x + w;
            }
            for (std::size_t i = 0; i < yticks.size(); ++i)
                if (yticks[i].major) {
                    const auto p = state.axes.y.normalize(yticks[i].value);
                    if (p)
                        label(".ytick." + std::to_string(i), yticks[i].label, 4,
                              top + float(1 - *p) * plotHeight - 7, left - 8);
                }
            label(".title", state.title, left, 2, plotWidth);
            label(".xlabel",
                  state.axes.x.label + (state.axes.x.unit.empty() ? "" : " (" + state.axes.x.unit + ")"),
                  left, top + plotHeight + 27, plotWidth);
            label(".ylabel",
                  state.axes.y.label + (state.axes.y.unit.empty() ? "" : " (" + state.axes.y.unit + ")"), 0,
                  20, width);
            std::vector<float> legendWidths;
            float legendTotal = 0;
            for (const auto& series : state.series) {
                if (series.name.empty())
                    continue;
                const float w =
                    std::min(plotWidth, core::TextPrimitive::measureTextWidth(series.name, {}, 12) + 24);
                legendWidths.push_back(w);
                legendTotal += w;
            }
            if (!legendWidths.empty())
                legendTotal += float(legendWidths.size() - 1) * 8;
            float legendX = left + std::max(0.f, (plotWidth - legendTotal) / 2);
            std::size_t legendWidthIndex = 0;
            for (std::size_t i = 0; i < state.series.size(); ++i) {
                const auto& series = state.series[i];
                if (series.name.empty())
                    continue;
                const float w = legendWidths[legendWidthIndex++];
                if (legendX + w > width - 8)
                    break;
                ui.rect(id + ".legend." + std::to_string(i))
                    .position(legendX, top - 22)
                    .size(w, 20)
                    .color({series.style.color[0], series.style.color[1], series.style.color[2],
                            series.visible ? 0.3f : 0.08f})
                    .disabled(!state.enabled)
                    .onClick([weak, i] {
                        if (const auto s = weak.lock(); s && i < s->series.size()) {
                            s->series[i].visible = !s->series[i].visible;
                            s->probe.reset();
                            s->selected.reset();
                            s->fit();
                            s->invalidate();
                        }
                    })
                    .build();
                ui.text(id + ".legend.text." + std::to_string(i))
                    .position(legendX, top - 21)
                    .size(w, 20)
                    .text(series.name)
                    .fontSize(12)
                    .horizontalAlign(core::HorizontalAlign::Center)
                    .verticalAlign(core::VerticalAlign::Center)
                    .color({0.8f, 0.83f, 0.88f, 1})
                    .build();
                legendX += w + 8;
            }
            ui.rect(id + ".input")
                .position(left, top)
                .size(plotWidth, plotHeight)
                .color({0, 0, 0, 0})
                .disabled(!state.enabled)
                .acceptedButtons(core::PointerButton::Left | core::PointerButton::Right)
                .onPress([weak](const core::PointerEvent& event, const core::Rect& bounds) {
                    if (const auto s = weak.lock()) {
                        if (event.button == core::PointerButton::Right) {
                            s->axes.x.resetAuto();
                            s->axes.y.resetAuto();
                            s->fit();
                            s->invalidate();
                            return;
                        }
                        // Runtime 的 press/drag 使用物理坐标，move 使用逻辑坐标。
                        s->pointerScale = bounds.width / s->viewport.width;
                        s->press = {(event.x - bounds.x) / s->pointerScale,
                                    (event.y - bounds.y) / s->pointerScale};
                        s->pointer = s->press;
                        s->dragAxes = s->axes;
                        s->dragging = true;
                        s->boxZoom = event.modifiers.shift;
                        s->measuring = event.modifiers.control;
                    }
                })
                .onMove([weak](const core::PointerEvent& event, const core::Rect& bounds) {
                    if (const auto s = weak.lock()) {
                        s->pointer = {event.x - bounds.x, event.y - bounds.y};
                        if (!s->dragging && !s->dirty)
                            s->updateProbe(s->pointer);
                    }
                    return true;
                })
                .onDrag([weak](const core::dsl::DragEvent& event) {
                    if (const auto s = weak.lock(); s && s->dragging && s->enabled) {
                        s->pointer = {s->press.x + event.totalX / s->pointerScale,
                                      s->press.y + event.totalY / s->pointerScale};
                        if (s->boxZoom || s->measuring)
                            return;
                        auto next = s->dragAxes;
                        if (transformAxes(next, {}, 1,
                                          {-event.totalX / s->pointerScale / s->viewport.width,
                                           event.totalY / s->pointerScale / s->viewport.height})) {
                            s->axes = next;
                            s->invalidate();
                        }
                    }
                })
                .onRelease([weak](const core::PointerEvent& event, const core::Rect& bounds) {
                    if (const auto s = weak.lock(); s && s->dragging) {
                        s->dragging = false;
                        if (event.action == core::PointerAction::Cancel)
                            return;
                        const Point end{
                            std::clamp((event.x - bounds.x) / s->pointerScale, 0.0, s->viewport.width),
                            std::clamp((event.y - bounds.y) / s->pointerScale, 0.0, s->viewport.height)};
                        const auto a = s->dragAxes.toData(s->press, s->viewport),
                                   b = s->dragAxes.toData(end, s->viewport);
                        if (s->boxZoom && a && b && std::abs(end.x - s->press.x) > 3 &&
                            std::abs(end.y - s->press.y) > 3) {
                            s->axes.x.setRange({std::min(a->x, b->x), std::max(a->x, b->x)});
                            s->axes.y.setRange({std::min(a->y, b->y), std::max(a->y, b->y)});
                            s->invalidate();
                        } else if (s->measuring && b) {
                            if (s->measureStart) {
                                s->measurement = Measurement{
                                    *s->measureStart, *b,
                                    std::hypot(b->x - s->measureStart->x, b->y - s->measureStart->y)};
                                s->measureStart.reset();
                            } else
                                s->measureStart = b;
                        } else if (std::hypot(end.x - s->press.x, end.y - s->press.y) <= 3 && !s->dirty) {
                            s->updateProbe(end);
                            s->selected = s->probe;
                        }
                    }
                })
                .onHover([weak](bool hovered) {
                    if (!hovered)
                        if (const auto s = weak.lock())
                            s->probe.reset();
                })
                .onScroll([weak](const core::ScrollEvent& event) {
                    if (const auto s = weak.lock(); s && s->enabled) {
                        const Point anchor{s->pointer.x / s->viewport.width,
                                           1 - s->pointer.y / s->viewport.height};
                        if (transformAxes(s->axes, anchor, std::exp(std::clamp(-event.y * 0.12, -4.0, 4.0)),
                                          {}))
                            s->invalidate();
                    }
                })
                .build();
            if (state.probe) {
                const auto& p = state.probe->point;
                ui.rect(id + ".cross.x")
                    .position(left + float(p.screen.x), top)
                    .size(1, plotHeight)
                    .color({0.9f, 0.9f, 0.9f, 0.5f})
                    .build();
                ui.rect(id + ".cross.y")
                    .position(left, top + float(p.screen.y))
                    .size(plotWidth, 1)
                    .color({0.9f, 0.9f, 0.9f, 0.5f})
                    .build();
            }
            if (state.dragging && state.boxZoom) {
                ui.rect(id + ".box")
                    .position(left + float(std::min(state.press.x, state.pointer.x)),
                              top + float(std::min(state.press.y, state.pointer.y)))
                    .size(float(std::abs(state.pointer.x - state.press.x)),
                          float(std::abs(state.pointer.y - state.press.y)))
                    .color({0.3f, 0.6f, 1, 0.2f})
                    .build();
            }
            for (std::size_t i = 0; i < state.annotations.size(); ++i) {
                const auto& note = state.annotations[i];
                const auto p = state.axes.toScreen(note.position, viewport);
                if (p && p->x >= 0 && p->y >= 0 && p->x < plotWidth && p->y < plotHeight)
                    label(".annotation." + std::to_string(i), note.text, left + float(p->x),
                          top + float(p->y), plotWidth - float(p->x));
            }
        })
        .build();
}
} // namespace modules::plot
