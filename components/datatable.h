#pragma once

#include "components/theme.h"
#include "components/virtuallist.h"
#include "core/dsl.h"

#include <algorithm>
#include <memory>
#include <numeric>
#include <functional>
#include <cstdlib>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace components {

struct DataTableStyle {
    DataTableStyle() : DataTableStyle(theme::dark()) {}

    explicit DataTableStyle(const theme::ThemeColorTokens& tokens) {
        background = tokens.surface;
        header = tokens.dark
            ? core::mixColor(tokens.surfaceHover, tokens.surface, 0.32f)
            : tokens.surfaceHover;
        row = tokens.surface;
        rowAlt = core::mixColor(tokens.surface, tokens.surfaceHover, tokens.dark ? 0.30f : 0.36f);
        rowHover = tokens.surfaceHover;
        text = tokens.text;
        mutedText = theme::withOpacity(tokens.text, 0.62f);
        accent = tokens.primary;
        border = theme::withOpacity(tokens.border, 0.72f);
        divider = theme::withOpacity(tokens.border, tokens.dark ? 0.42f : 0.46f);
        radius = tokens.metrics.radius.card;
    }

    core::Color background;
    core::Color header;
    core::Color row;
    core::Color rowAlt;
    core::Color rowHover;
    core::Color text;
    core::Color mutedText;
    core::Color accent;
    core::Color border;
    core::Color divider;
    float radius = 12.0f;
};

using DataTableRows = std::vector<std::vector<std::string>>;

namespace table_detail {
struct State {
    std::shared_ptr<const DataTableRows> source;
    std::uint64_t revision = 0;
    std::vector<std::size_t> order;
    std::vector<float> widths, suppliedWidths, dragWidths;
    std::string filter;
    int sort = -1, cachedSort = -2;
    bool descending = false, cachedDescending = false, cachedNumeric = false;
    float offset = 0, available = -1;

    static const std::string& cell(const DataTableRows& rows, std::size_t row, int column) {
        static const std::string empty;
        return column >= 0 && static_cast<std::size_t>(column) < rows[row].size() ? rows[row][column] : empty;
    }
    void refresh(std::shared_ptr<const DataTableRows> data, std::uint64_t version, const std::string& query, bool numeric) {
        if (source == data && revision == version && filter == query && cachedSort == sort &&
            cachedDescending == descending && cachedNumeric == numeric) return;
        source = std::move(data); revision = version; filter = query;
        cachedSort = sort; cachedDescending = descending; cachedNumeric = numeric;
        order.clear();
        if (source) {
            for (std::size_t i = 0; i < source->size(); ++i) {
                if (query.empty() || std::any_of((*source)[i].begin(), (*source)[i].end(), [&](const auto& value) {
                    return value.find(query) != std::string::npos;
                })) order.push_back(i);
            }
            if (sort >= 0) std::stable_sort(order.begin(), order.end(), [&](auto left, auto right) {
                const auto& a = cell(*source, left, sort);
                const auto& b = cell(*source, right, sort);
                int comparison = a.compare(b);
                if (numeric) {
                    char *ae = nullptr, *be = nullptr;
                    const double av = std::strtod(a.c_str(), &ae), bv = std::strtod(b.c_str(), &be);
                    const bool an = ae != a.c_str() && *ae == 0 && std::isfinite(av);
                    const bool bn = be != b.c_str() && *be == 0 && std::isfinite(bv);
                    if (an && bn) comparison = av < bv ? -1 : (av > bv ? 1 : 0);
                    else if (an != bn) comparison = an ? -1 : 1;
                }
                return descending ? comparison > 0 : comparison < 0;
            });
        }
        offset = 0;
    }
};

inline void resizeBoundary(std::vector<float>& widths, std::size_t column, float delta, float minimum) {
    if (column + 1 >= widths.size() || !std::isfinite(delta)) return;
    const float sum = widths[column] + widths[column + 1];
    const float low = std::min(minimum, sum * 0.5f);
    widths[column] = std::clamp(widths[column] + delta, low, sum - low);
    widths[column + 1] = sum - widths[column];
}

inline void fitWidths(std::vector<float>& widths, float available, float minimum) {
    if (widths.empty()) return;
    minimum = std::min(minimum, available / widths.size());
    float excess = 0;
    for (auto& width : widths) {
        if (width < minimum) { excess += minimum - width; width = minimum; }
    }
    if (excess <= 0) return;
    float spare = 0;
    for (const auto width : widths) spare += width - minimum;
    const float ratio = spare > 0 ? std::max(0.0f, (spare - excess) / spare) : 0;
    for (auto& width : widths) width = minimum + (width - minimum) * ratio;
}
} // namespace table_detail

/** @brief 固定行高虚拟表格；共享数据源只有在 identity/revision、筛选或排序改变时重建索引。 */
class DataTableBuilder {
public:
    DataTableBuilder(core::dsl::Ui& ui, std::string id) : ui_(ui), id_(std::move(id)) {}
    DataTableBuilder& size(float w, float h) { width_ = std::max(0.0f, w); height_ = std::max(0.0f, h); return *this; }
    DataTableBuilder& columns(std::vector<std::string> value) { columns_ = std::move(value); return *this; }
    DataTableBuilder& rows(DataTableRows value) { data_ = std::make_shared<DataTableRows>(std::move(value)); return *this; }
    /** @brief 原地更新共享数据时必须递增 revision；UI compose 与读取期间不得并发修改。 */
    DataTableBuilder& data(std::shared_ptr<const DataTableRows> value, std::uint64_t revision = 0) { data_ = std::move(value); revision_ = revision; return *this; }
    DataTableBuilder& rowHeight(float value) { rowHeight_ = std::max(1.0f, value); return *this; }
    DataTableBuilder& filter(std::string value) { filter_ = std::move(value); return *this; }
    DataTableBuilder& sort(int column, bool descending = false) { sort_ = column; descending_ = descending; hasSort_ = true; return *this; }
    DataTableBuilder& numericSort(bool value = true) { numeric_ = value; return *this; }
    DataTableBuilder& sortable(bool value = true) { sortable_ = value; return *this; }
    DataTableBuilder& resizable(bool value = true) { resizable_ = value; return *this; }
    DataTableBuilder& columnWidths(std::vector<float> value) { widths_ = std::move(value); return *this; }
    DataTableBuilder& minColumnWidth(float value) { minWidth_ = std::max(1.0f, value); return *this; }
    DataTableBuilder& onSort(std::function<void(int, bool)> fn) { onSort_ = std::move(fn); return *this; }
    DataTableBuilder& onColumnWidthsChange(std::function<void(const std::vector<float>&)> fn) { onWidths_ = std::move(fn); return *this; }
    /** @brief 回调索引指向原始数据行，排序和筛选不会改变其含义。 */
    DataTableBuilder& onRowClick(std::function<void(std::size_t)> fn) { onRow_ = std::move(fn); return *this; }
    DataTableBuilder& style(const DataTableStyle& value) { style_ = value; return *this; }
    DataTableBuilder& theme(const theme::ThemeColorTokens& value) { style_ = DataTableStyle(value); tokens_ = value; metrics_ = value.metrics; return *this; }
    DataTableBuilder& transition(const core::Transition& value) { transition_ = value; return *this; }

    void build() {
        auto& state = ui_.state<table_detail::State>(id_ + ".tableState");
        const auto count = columns_.size();
        const float bodyWidth = std::max(0.0f, width_ - 2.0f);
        const float headerHeight = std::min(height_, metrics_.control.control);
        const float bodyHeight = std::max(0.0f, height_ - headerHeight - 2.0f);
        if (hasSort_) { state.sort = sort_; state.descending = descending_; }
        if (state.sort >= static_cast<int>(count)) state.sort = -1;
        state.refresh(data_, revision_, filter_, numeric_);
        const float usable = std::max(0.0f, bodyWidth - (state.order.size() * rowHeight_ > bodyHeight ? metrics_.control.scrollbar + 4.0f : 0.0f));
        if (state.widths.size() != count || state.suppliedWidths != widths_ || state.available != usable) {
            state.widths.resize(count, count ? usable / count : 0);
            if (state.suppliedWidths != widths_ || state.available < 0) {
                for (std::size_t i = 0; i < count; ++i)
                    state.widths[i] = i < widths_.size() && std::isfinite(widths_[i]) ? std::max(1.0f, widths_[i]) : (count ? usable / count : 0);
            }
            float total = std::accumulate(state.widths.begin(), state.widths.end(), 0.0f);
            if (total > 0) for (auto& w : state.widths) w *= usable / total;
            state.suppliedWidths = widths_; state.available = usable;
        }
        const float minimum = count ? std::min(minWidth_, usable / count) : 0;
        table_detail::fitWidths(state.widths, usable, minimum);
        const auto widths = state.widths;
        const auto onSort = onSort_;
        const auto onWidths = onWidths_;
        ui_.stack(id_).size(width_, height_).clip()
            .accessible(core::AccessibleRole::Table, id_)
            .content([&] {
                ui_.rect(id_ + ".bg").size(width_, height_).color(style_.background).radius(style_.radius).border(1, style_.border).build();
                ui_.rect(id_ + ".header").size(bodyWidth, headerHeight).color(style_.header).build();
                float x = 1;
                for (std::size_t column = 0; column < count; ++column) {
                    const std::string prefix = id_ + ".column." + std::to_string(column);
                    auto header = ui_.rect(prefix).x(x).size(widths[column], headerHeight).transition(transition_)
                        .color(style_.header).accessible(core::AccessibleRole::ColumnHeader, columns_[column],
                            state.sort == static_cast<int>(column) ? (state.descending ? "descending" : "ascending") : "unsorted");
                    if (sortable_) header.onClick([&state, column, onSort] {
                        state.descending = state.sort == static_cast<int>(column) && !state.descending;
                        state.sort = static_cast<int>(column);
                        if (onSort) onSort(state.sort, state.descending);
                    });
                    header.build();
                    ui_.text(prefix + ".label").x(x + 8).size(std::max(0.0f, widths[column] - 16), headerHeight)
                        .text(columns_[column] + (state.sort == static_cast<int>(column) ? (state.descending ? " v" : " ^") : ""))
                        .fontSize(metrics_.typography.label).color(style_.text).verticalAlign(core::VerticalAlign::Center).build();
                    x += widths[column];
                    if (resizable_ && column + 1 < count) {
                        ui_.rect(prefix + ".resize").x(x - 4).size(8, headerHeight).color({0, 0, 0, 0})
                            .accessible(core::AccessibleRole::Slider, columns_[column] + " width", std::to_string(widths[column]))
                            .onPress([&state](const core::PointerEvent&, const core::Rect&) { state.dragWidths = state.widths; })
                            .onDrag([&state, column, minimum, onWidths](const core::dsl::DragEvent& event) {
                                state.widths = state.dragWidths;
                                table_detail::resizeBoundary(state.widths, column, static_cast<float>(event.totalX), minimum);
                                if (onWidths) onWidths(state.widths);
                            })
                            .onKeyEvent([&state, column, minimum, onWidths](const core::KeyEvent& event) {
                                if (!event.isDown() || (event.key != core::InputKey::Left && event.key != core::InputKey::Right)) return false;
                                table_detail::resizeBoundary(state.widths, column, event.key == core::InputKey::Left ? -8.0f : 8.0f, minimum);
                                if (onWidths) onWidths(state.widths);
                                return true;
                            }).build();
                    }
                }
                const auto source = state.source;
                const auto onRow = onRow_;
                const auto style = style_;
                const auto metrics = metrics_;
                components::virtualList(ui_, id_ + ".body").x(1).y(headerHeight).size(bodyWidth, bodyHeight)
                    .theme(tokens_).scrollbarGap(4).rowHeight(rowHeight_).itemCount(static_cast<std::int64_t>(state.order.size()))
                    .offset(state.offset).onChange([&state](float value) { state.offset = value; })
                    .row([&state, source, widths, onRow, style, metrics](core::dsl::Ui& ui, const std::string& slot, std::int64_t view, float w, float h) {
                        const auto row = state.order[static_cast<std::size_t>(view)];
                        auto background = ui.rect(slot + ".bg").size(w, h).color(view % 2 ? style.rowAlt : style.row);
                        if (onRow) background.onClick([onRow, row] { onRow(row); }).accessible(core::AccessibleRole::ListItem, "Row " + std::to_string(row + 1));
                        background.build();
                        float x = 0;
                        for (std::size_t col = 0; col < widths.size(); ++col) {
                            const auto& value = table_detail::State::cell(*source, row, static_cast<int>(col));
                            ui.text(slot + ".cell." + std::to_string(col)).x(x + 8).size(std::max(0.0f, widths[col] - 16), h)
                                .text(value).accessible(core::AccessibleRole::Cell, value).fontSize(metrics.typography.label)
                                .color(col ? style.mutedText : style.text).verticalAlign(core::VerticalAlign::Center).build();
                            x += widths[col];
                        }
                    }).build();
            }).build();
    }

private:
    core::dsl::Ui& ui_;
    std::string id_;
    std::vector<std::string> columns_;
    std::shared_ptr<const DataTableRows> data_;
    std::uint64_t revision_ = 0;
    std::string filter_;
    std::vector<float> widths_;
    std::function<void(int, bool)> onSort_;
    std::function<void(const std::vector<float>&)> onWidths_;
    std::function<void(std::size_t)> onRow_;
    DataTableStyle style_;
    theme::ThemeColorTokens tokens_ = theme::dark();
    theme::ThemeMetricTokens metrics_;
    core::Transition transition_;
    float width_ = 420, height_ = 174, rowHeight_ = 32, minWidth_ = 40;
    int sort_ = -1;
    bool descending_ = false, hasSort_ = false, numeric_ = false, sortable_ = true, resizable_ = true;
};

inline DataTableBuilder dataTable(core::dsl::Ui& ui, const std::string& id) { return DataTableBuilder(ui, id); }

} // namespace components
