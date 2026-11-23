#include "modules/plot/figure.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace modules::plot {
namespace {
bool changed(Range current, Range previous) {
    return current.min != previous.min || current.max != previous.max;
}
bool changed(const std::optional<Point>& current, const std::optional<Point>& previous) {
    return current.has_value() != previous.has_value() ||
           (current && (current->x != previous->x || current->y != previous->y));
}

void validateLink(const std::vector<std::size_t>& indexes, std::size_t count) {
    if (indexes.size() < 2)
        throw std::invalid_argument("plot: link needs at least two plots");
    for (const auto index : indexes)
        if (index >= count)
            throw std::out_of_range("plot: linked plot index");
    auto sorted = indexes;
    std::sort(sorted.begin(), sorted.end());
    if (std::adjacent_find(sorted.begin(), sorted.end()) != sorted.end())
        throw std::invalid_argument("plot: duplicate linked plot index");
}
} // namespace

Plot& Figure::addPlot(Budget budget) {
    plots_.push_back(std::make_unique<Plot>(budget));
    return *plots_.back();
}
Plot& Figure::plot(std::size_t index) { return *plots_.at(index); }
const Plot& Figure::plot(std::size_t index) const { return *plots_.at(index); }
std::size_t Figure::size() const noexcept { return plots_.size(); }
void Figure::setColumns(std::size_t columns) {
    if (columns == 0)
        throw std::invalid_argument("plot: figure columns must be positive");
    columns_ = columns;
}
std::size_t Figure::columns() const noexcept { return columns_; }
void Figure::linkX(std::vector<std::size_t> plots) {
    validateLink(plots, plots_.size());
    xLinks_.push_back({std::move(plots), {}});
}
void Figure::linkY(std::vector<std::size_t> plots) {
    validateLink(plots, plots_.size());
    yLinks_.push_back({std::move(plots), {}});
}
void Figure::linkCursor(std::vector<std::size_t> plots) {
    validateLink(plots, plots_.size());
    cursorLinks_.push_back({std::move(plots), {}});
}
void Figure::synchronize(std::vector<Link>& links, bool horizontal) {
    for (auto& link : links) {
        std::vector<Range> ranges;
        ranges.reserve(link.plots.size());
        for (const auto index : link.plots)
            ranges.push_back(horizontal ? plots_[index]->axes().x.range() : plots_[index]->axes().y.range());
        if (link.previous.empty()) {
            const auto source = ranges.front();
            for (std::size_t i = 1; i < link.plots.size(); ++i) {
                Axes axes = plots_[link.plots[i]]->axes();
                (horizontal ? axes.x : axes.y).setRange(source);
                plots_[link.plots[i]]->setAxes(std::move(axes));
                ranges[i] = source;
            }
        } else {
            std::size_t sourceIndex = ranges.size();
            for (std::size_t i = 0; i < ranges.size(); ++i)
                if (changed(ranges[i], link.previous[i])) {
                    sourceIndex = i;
                    break;
                }
            if (sourceIndex != ranges.size()) {
                const auto source = ranges[sourceIndex];
                for (std::size_t i = 0; i < link.plots.size(); ++i)
                    if (ranges[i].min != source.min || ranges[i].max != source.max) {
                        Axes axes = plots_[link.plots[i]]->axes();
                        (horizontal ? axes.x : axes.y).setRange(source);
                        plots_[link.plots[i]]->setAxes(std::move(axes));
                        ranges[i] = source;
                    }
            }
        }
        link.previous = std::move(ranges);
    }
}
void Figure::synchronizeCursors() {
    for (auto& link : cursorLinks_) {
        std::vector<std::optional<Point>> cursors;
        cursors.reserve(link.plots.size());
        for (const auto index : link.plots)
            cursors.push_back(plots_[index]->cursor());
        std::size_t source = cursors.size();
        if (link.previous.empty()) {
            for (std::size_t i = 0; i < cursors.size(); ++i)
                if (cursors[i]) {
                    source = i;
                    break;
                }
        } else {
            for (std::size_t i = 0; i < cursors.size(); ++i)
                if (changed(cursors[i], link.previous[i])) {
                    source = i;
                    break;
                }
        }
        if (source != cursors.size())
            for (std::size_t i = 0; i < link.plots.size(); ++i)
                if (changed(cursors[i], cursors[source])) {
                    plots_[link.plots[i]]->setCursor(cursors[source]);
                    cursors[i] = cursors[source];
                }
        link.previous = std::move(cursors);
    }
}
void Figure::synchronizeLinks() {
    synchronize(xLinks_, true);
    synchronize(yLinks_, false);
    synchronizeCursors();
}
void Figure::compose(eui::Ui& ui, const std::string& id, float width, float height, double dpi) {
    if (!std::isfinite(width) || !std::isfinite(height) || width <= 0 || height <= 0 ||
        !std::isfinite(dpi) || dpi <= 0)
        throw std::invalid_argument("plot: invalid figure dimensions");
    synchronizeLinks();
    if (plots_.empty())
        return;
    const auto rows = (plots_.size() + columns_ - 1) / columns_;
    const float cellWidth = width / static_cast<float>(columns_);
    const float cellHeight = height / static_cast<float>(rows);
    if (cellWidth < 160 || cellHeight < 120)
        throw std::invalid_argument("plot: figure cells need at least 160x120 logical pixels");
    ui.stack(id)
        .size(width, height)
        .clip()
        .content([&] {
            for (std::size_t index = 0; index < plots_.size(); ++index) {
                const auto column = index % columns_, row = index / columns_;
                ui.stack(id + ".plot." + std::to_string(index))
                    .position(float(column) * cellWidth, float(row) * cellHeight)
                    .size(cellWidth, cellHeight)
                    .content([&] { plots_[index]->compose(ui, id + ".data." + std::to_string(index), cellWidth, cellHeight, dpi); })
                    .build();
            }
        })
        .build();
}
void Figure::releaseGpu() {
    for (auto& plot : plots_)
        plot->releaseGpu();
}

} // namespace modules::plot