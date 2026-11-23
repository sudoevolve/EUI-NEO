#pragma once

#include "components/layout.h"

#include "components/theme.h"
#include "components/virtuallist.h"
#include "core/dsl.h"
#include "eui/signal.h"

#include <algorithm>
#include <memory>
#include <numeric>
#include <functional>
#include <cstdlib>
#include <cmath>
#include <string>
#include <utility>
#include <vector>
#include <unordered_set>

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

/** @brief None 保持只读；Multiple 支持 Ctrl/Command 切换和 Shift 范围选择。 */
enum class DataTableSelectionMode { None, Single, Multiple };

/** @brief 类型化列；getter 在数据 identity/revision 改变时于 compose 线程求值。 */
template <typename T>
struct DataTableColumn {
    std::string title;
    std::function<std::string(const T&)> value;
};

/** @brief 以稳定行 key 存储选择，排序和筛选不会丢失选择；key 必须唯一。 */
class DataTableSelection {
public:
    std::size_t size() const { return selected_.size(); }
    bool contains(const std::string& key) const { return selected_.count(key) != 0; }
    std::vector<std::string> keys() const {
        std::vector<std::string> result(selected_.begin(), selected_.end());
        std::sort(result.begin(), result.end()); return result;
    }
    void set(const std::vector<std::string>& keys) { selected_ = {keys.begin(), keys.end()}; }
    void retain(const std::vector<std::string>& keys) {
        const std::unordered_set<std::string> valid(keys.begin(), keys.end());
        for (auto it = selected_.begin(); it != selected_.end();) {
            if (!valid.count(*it)) it = selected_.erase(it); else ++it;
        }
    }
    void select(const std::vector<std::string>& visible, std::size_t index,
                DataTableSelectionMode mode, bool toggle = false, bool extend = false) {
        if (mode == DataTableSelectionMode::None || index >= visible.size()) return;
        const auto& key = visible[index];
        if (mode == DataTableSelectionMode::Single) { selected_ = {key}; anchor_ = key; return; }
        if (extend) {
            const auto anchor = std::find(visible.begin(), visible.end(), anchor_);
            const std::size_t start = anchor == visible.end() ? index : static_cast<std::size_t>(anchor - visible.begin());
            if (!toggle) selected_.clear();
            for (std::size_t i = std::min(start, index); i <= std::max(start, index); ++i) selected_.insert(visible[i]);
        } else {
            if (!toggle) selected_.clear();
            if (toggle && contains(key)) selected_.erase(key); else selected_.insert(key);
            anchor_ = key;
        }
    }
private:
    std::unordered_set<std::string> selected_;
    std::string anchor_;
};

namespace table_detail {
template <typename T> struct TypedCache {
    std::shared_ptr<const std::vector<T>> source;
    std::uint64_t revision = 0;
    std::vector<std::string> titles;
    std::shared_ptr<DataTableRows> rows;
};
struct State {
    std::shared_ptr<const DataTableRows> source;
    std::uint64_t revision = 0;
    std::vector<std::size_t> order;
    std::vector<float> widths, suppliedWidths, dragWidths;
    std::string filter;
    int sort = -1, cachedSort = -2;
    bool descending = false, cachedDescending = false, cachedNumeric = false;
    float offset = 0, available = -1;
    DataTableSelection selection;
    std::vector<std::string> sourceKeys, visibleKeys;
    core::KeyModifiers pressModifiers;
    std::string focusedKey;
    bool keyed = false;
    bool selectionEnabled = false, focusRequested = false;

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
class DataTableBuilder : public ComponentLayout<DataTableBuilder> {
public:
    DataTableBuilder(core::dsl::Ui& ui, std::string id) : ui_(ui), id_(std::move(id)) {}
    DataTableBuilder& size(float w, float h) { width_ = std::max(0.0f, w); height_ = std::max(0.0f, h); return *this; }
    DataTableBuilder& columns(std::vector<std::string> value) { columns_ = std::move(value); return *this; }
    DataTableBuilder& rows(DataTableRows value) { data_ = std::make_shared<DataTableRows>(std::move(value)); return *this; }
    /** @brief 原地更新共享数据时必须递增 revision；UI compose 与读取期间不得并发修改。 */
    DataTableBuilder& data(std::shared_ptr<const DataTableRows> value, std::uint64_t revision = 0) { data_ = std::move(value); revision_ = revision; return *this; }
    /** @brief 绑定业务对象；数据、getter 或 key 规则变化时递增 revision，不得与 compose 并发修改。 */
    template <typename T>
    DataTableBuilder& data(std::shared_ptr<const std::vector<T>> source,
                           std::vector<DataTableColumn<T>> columns, std::uint64_t revision = 0) {
        auto& cache = ui_.state<table_detail::TypedCache<T>>(id_ + ".typedData");
        std::vector<std::string> titles;
        for (const auto& column : columns) titles.push_back(column.title);
        if (!cache.rows || cache.source != source || cache.revision != revision || cache.titles != titles) {
            auto rows = std::make_shared<DataTableRows>();
            if (source) {
                rows->reserve(source->size());
                for (const auto& item : *source) {
                    std::vector<std::string> row;
                    row.reserve(columns.size());
                    for (const auto& column : columns) row.push_back(column.value ? column.value(item) : std::string{});
                    rows->push_back(std::move(row));
                }
            }
            cache.source = std::move(source); cache.revision = revision;
            cache.titles = titles; cache.rows = std::move(rows);
        }
        columns_ = std::move(titles); return data(cache.rows, revision);
    }
    template <typename T>
    DataTableBuilder& data(std::shared_ptr<std::vector<T>> source,
                           std::vector<DataTableColumn<T>> columns, std::uint64_t revision = 0) {
        return data<T>(std::shared_ptr<const std::vector<T>>(std::move(source)), std::move(columns), revision);
    }
    /** @brief 直接提供业务行快照；频繁 compose 的大数据源应使用共享 data + revision。 */
    template <typename T>
    DataTableBuilder& rows(std::vector<T> source, std::vector<DataTableColumn<T>> columns) {
        return data<T>(std::make_shared<std::vector<T>>(std::move(source)), std::move(columns));
    }
    /** @brief 默认 key 为原始行索引；源数据会插入、删除或重排时应提供业务 ID。 */
    DataTableBuilder& rowKey(std::function<std::string(std::size_t)> fn) { rowKey_ = std::move(fn); return *this; }
    DataTableBuilder& selectionMode(DataTableSelectionMode value) { selectionMode_ = value; hasSelectionMode_ = true; return *this; }
    DataTableBuilder& selectedKeys(std::vector<std::string> value) {
        if (!hasSelectionMode_) selectionMode_ = DataTableSelectionMode::Single;
        selected_ = std::move(value); controlledSelection_ = true; return *this;
    }
    DataTableBuilder& bindSelection(eui::Signal<std::vector<std::string>>& signal) {
        if (!hasSelectionMode_) selectionMode_ = DataTableSelectionMode::Single;
        selectedKeys(signal.get()); return onSelectionChange([&signal](const auto& keys) { signal.set(keys); });
    }
    DataTableBuilder& onSelectionChange(std::function<void(const std::vector<std::string>&)> fn) { onSelection_ = std::move(fn); return *this; }
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
        const float radius = std::clamp(style_.radius, 0.0f, std::min(width_, height_) * 0.5f);
        if (hasSort_) { state.sort = sort_; state.descending = descending_; }
        if (state.sort >= static_cast<int>(count)) state.sort = -1;
        const bool selectionEnabled = selectionMode_ != DataTableSelectionMode::None;
        const bool sourceChanged = state.source != data_ || state.revision != revision_ || state.keyed != static_cast<bool>(rowKey_) || state.selectionEnabled != selectionEnabled;
        const auto previousSelection = sourceChanged && !controlledSelection_ ? state.selection.keys() : std::vector<std::string>{};
        const bool orderChanged = sourceChanged || state.filter != filter_ || state.cachedSort != state.sort || state.cachedDescending != state.descending || state.cachedNumeric != numeric_;
        state.refresh(data_, revision_, filter_, numeric_);
        if (sourceChanged) {
            state.sourceKeys.clear();
            if (selectionEnabled && data_) for (std::size_t i = 0; i < data_->size(); ++i) state.sourceKeys.push_back(rowKey_ ? rowKey_(i) : std::to_string(i));
            state.keyed = static_cast<bool>(rowKey_);
            state.selectionEnabled = selectionEnabled;
            if (selectionEnabled) state.selection.retain(state.sourceKeys);
        }
        if (orderChanged) {
            state.visibleKeys.clear();
            if (selectionEnabled) for (const auto row : state.order) state.visibleKeys.push_back(state.sourceKeys[row]);
        }
        if (controlledSelection_ && selectionEnabled) {
            state.selection.set(selected_); state.selection.retain(state.sourceKeys);
        }
        if (selectionEnabled && selectionMode_ == DataTableSelectionMode::Single && state.selection.size() > 1) {
            state.selection.set({state.selection.keys().front()});
        }
        if (selectionEnabled && onSelection_ && controlledSelection_) {
            auto supplied = selected_; std::sort(supplied.begin(), supplied.end());
            const auto resolved = state.selection.keys();
            if (supplied != resolved) onSelection_(resolved);
        }
        if (selectionEnabled && onSelection_ && sourceChanged && !controlledSelection_) {
            const auto resolved = state.selection.keys();
            if (previousSelection != resolved) onSelection_(resolved);
        }
        state.offset = std::clamp(state.offset, 0.0f, std::max(0.0f, state.order.size() * rowHeight_ - bodyHeight));
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
        layoutRoot(ui_.stack(id_)).size(width_, height_).clip()
            .accessible(core::AccessibleRole::Table, id_)
            .content([&] {
                ui_.rect(id_ + ".bg").size(width_, height_).color(style_.background).radius(radius).build();
                // clip() 是矩形裁剪；在表头带内绘制整张圆角面板，避免直角底色盖住外框。
                ui_.stack(id_ + ".header.clip").size(width_, headerHeight).clip().content([&] {
                    ui_.rect(id_ + ".header").size(width_, height_).color(style_.header).radius(radius).build();
                }).build();
                float x = 1;
                for (std::size_t column = 0; column < count; ++column) {
                    const std::string prefix = id_ + ".column." + std::to_string(column);
                    auto header = ui_.rect(prefix).x(x).size(widths[column], headerHeight).transition(transition_)
                        .color({0, 0, 0, 0}).radius(radius).accessible(core::AccessibleRole::ColumnHeader, columns_[column],
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
                const auto onSelection = onSelection_;
                const auto mode = selectionMode_;
                const auto focusGroup = id_ + ".rows";
                const auto style = style_;
                const auto metrics = metrics_;
                const float tableWidth = width_, tableHeight = height_, rowHeight = rowHeight_, offset = state.offset;
                components::virtualList(ui_, id_ + ".body").x(1).y(headerHeight).size(bodyWidth, bodyHeight)
                    .theme(tokens_).scrollbarGap(4).scrollbarInset(radius).rowHeight(rowHeight_).itemCount(static_cast<std::int64_t>(state.order.size()))
                    .composeOnScrollOffsetChange(radius > 0)
                    .offset(state.offset).onChange([&state](float value) { state.offset = value; })
                    .row([&state, source, widths, onRow, onSelection, mode, focusGroup, style, metrics, tableWidth, tableHeight, headerHeight, rowHeight, offset, radius, bodyHeight](core::dsl::Ui& ui, const std::string& slot, std::int64_t view, float w, float h) {
                        const auto row = state.order[static_cast<std::size_t>(view)];
                        if (mode != DataTableSelectionMode::None && state.focusRequested && state.focusedKey == state.sourceKeys[row]) {
                            ui.requestFocus(slot + ".bg"); state.focusRequested = false;
                        }
                        const bool selected = mode != DataTableSelectionMode::None && state.selection.contains(state.sourceKeys[row]);
                        const auto color = selected ? core::mixColor(style.row, style.accent, .24f) : (view % 2 ? style.rowAlt : style.row);
                        // 每行只显示整张圆角面板与本行的交集，圆角始终锚定视口而非数据行。
                        if (radius > 0) ui.stack(slot + ".paint.clip").size(w, h).clip().content([&] {
                            ui.rect(slot + ".paint").x(-1).y(offset - headerHeight - static_cast<float>(view * static_cast<double>(rowHeight)))
                                .size(tableWidth, tableHeight).radius(radius).color(color).build();
                        }).build();
                        auto background = ui.rect(slot + ".bg").size(w, h).color(radius > 0 ? core::Color{0, 0, 0, 0} : color);
                        if (onRow || mode != DataTableSelectionMode::None) {
                            background.accessible(core::AccessibleRole::ListItem, "Row " + std::to_string(row + 1))
                                .accessibleSelected(selected).focusGroup(focusGroup)
                                .onFocusChanged([&state, mode, row](bool focused) {
                                    if (focused && mode != DataTableSelectionMode::None) state.focusedKey = state.sourceKeys[row];
                                })
                                .onPress([&state](const core::PointerEvent& event, const core::Rect&) { state.pressModifiers = event.modifiers; })
                                .onClick([&state, onRow, onSelection, mode, row, view] {
                                    const auto modifiers = state.pressModifiers; state.pressModifiers = {};
                                    if (mode != DataTableSelectionMode::None) state.focusedKey = state.sourceKeys[row];
                                    state.selection.select(state.visibleKeys, static_cast<std::size_t>(view), mode, modifiers.shortcut(), modifiers.shift);
                                    if (mode != DataTableSelectionMode::None && onSelection) onSelection(state.selection.keys());
                                    if (onRow) onRow(row);
                                })
                                .onKeyEvent([&state, onRow, onSelection, mode, view, rowHeight, bodyHeight](const core::KeyEvent& key) {
                                    if (!key.isDown() || mode == DataTableSelectionMode::None || state.order.empty()) return false;
                                    auto next = view;
                                    const auto focused = std::find(state.visibleKeys.begin(), state.visibleKeys.end(), state.focusedKey);
                                    if (focused != state.visibleKeys.end()) next = focused - state.visibleKeys.begin();
                                    switch (key.key) {
                                    case core::InputKey::Up: --next; break;
                                    case core::InputKey::Down: ++next; break;
                                    case core::InputKey::Home: next = 0; break;
                                    case core::InputKey::End: next = static_cast<std::int64_t>(state.order.size()) - 1; break;
                                    case core::InputKey::Space: case core::InputKey::Enter: break;
                                    case core::InputKey::A:
                                        if (!key.modifiers.shortcut() || mode != DataTableSelectionMode::Multiple) return false;
                                        state.selection.set(state.visibleKeys);
                                        if (onSelection) onSelection(state.selection.keys());
                                        return true;
                                    default: return false;
                                    }
                                    next = std::clamp<std::int64_t>(next, 0, state.order.size() - 1);
                                    state.focusedKey = state.visibleKeys[static_cast<std::size_t>(next)];
                                    state.focusRequested = true;
                                    state.selection.select(state.visibleKeys, static_cast<std::size_t>(next), mode,
                                        key.key == core::InputKey::Space && key.modifiers.shortcut(), key.modifiers.shift);
                                    const float top = static_cast<float>(next) * rowHeight;
                                    state.offset = std::clamp(state.offset,
                                        std::max(0.0f, top + std::min(rowHeight, bodyHeight) - bodyHeight), top);
                                    if (onSelection) onSelection(state.selection.keys());
                                    if (key.key == core::InputKey::Enter && onRow) onRow(state.order[static_cast<std::size_t>(next)]);
                                    return true;
                                });
                        }
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
                ui_.rect(id_ + ".border").size(width_, height_).radius(radius).color({0, 0, 0, 0}).border(1, style_.border).build();
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
    std::function<std::string(std::size_t)> rowKey_;
    std::function<void(const std::vector<std::string>&)> onSelection_;
    std::vector<std::string> selected_;
    DataTableSelectionMode selectionMode_ = DataTableSelectionMode::None;
    bool controlledSelection_ = false;
    bool hasSelectionMode_ = false;
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
