#include "eui_neo.h"

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>

namespace app {

const DslAppConfig& dslAppConfig() {
    static const DslAppConfig config = DslAppConfig{}
        .title("Multi-page Table")
        .pageId("multipage_table")
        .clearColor({0.035f, 0.045f, 0.060f, 1.0f})
        .windowSize(1440, 900)
        .fps(90.0);
    return config;
}

namespace {

constexpr int kColumnCount = 16;
constexpr int kRowsPerPage = 36;
constexpr int kPageCount = 12;
constexpr float kRowHeight = 34.0f;
constexpr float kHeaderHeight = 42.0f;

struct TableState {
    int page = 0;
    int selectedRow = -1;
    int refresh = 0;
    eui::Signal<float> scroll{0.0f};
};

std::string cellValue(int page, int row, int column, int refresh) {
    if (column == 0) {
        return "R" + std::to_string(page * kRowsPerPage + row + 1);
    }

    if (column == 1) {
        return "Item " + std::to_string(page * kRowsPerPage + row + 1);
    }

    if (column == 2) {
        return (row + page) % 3 == 0 ? "Ready" : ((row + page) % 3 == 1 ? "Queued" : "Review");
    }

    if (column == 3) {
        return (row * 17 + page * 11 + refresh * 7) % 1000 < 720 ? "Online" : "Offline";
    }

    std::ostringstream value;
    value << std::setw(3) << std::setfill('0')
          << ((page + 1) * (row + 3) * (column + 5) + refresh * 13) % 997;
    return value.str();
}

std::string columnLabel(int column) {
    if (column == 0) return "ID";
    if (column == 1) return "Name";
    if (column == 2) return "Status";
    if (column == 3) return "Network";
    return "Metric " + std::to_string(column - 3);
}

void drawCell(eui::Ui& ui,
              const std::string& id,
              float x,
              float y,
              float width,
              float height,
              const std::string& value,
              const eui::Color& color,
              float fontSize = 12.0f) {
    ui.text(id)
        .position(x + 10.0f, y)
        .size(std::max(0.0f, width - 20.0f), height)
        .text(value)
        .fontSize(fontSize)
        .lineHeight(height)
        .color(color)
        .verticalAlign(eui::VerticalAlign::Center)
        .build();
}

} // namespace

void compose(eui::Ui& ui, const eui::Screen& screen) {
    TableState& state = ui.state<TableState>("multipage_table.state");
    state.page = std::clamp(state.page, 0, kPageCount - 1);

    const components::theme::ThemeColorTokens tokens = components::theme::dark();
    const float margin = 28.0f;
    const float width = std::max(0.0f, screen.width - margin * 2.0f);
    const float tableWidth = std::min(width, 1380.0f);
    const float bodyHeight = std::max(240.0f, screen.height - 238.0f);
    const float columnWidth = tableWidth / static_cast<float>(kColumnCount);
    const eui::Color border = components::theme::withOpacity(tokens.border, 0.72f);
    const eui::Color muted = components::theme::withOpacity(tokens.text, 0.64f);

    ui.column("page")
        .size(screen.width, screen.height)
        .padding(24.0f)
        .gap(12.0f)
        .alignItems(eui::Align::CENTER)
        .content([&] {
            ui.row("toolbar")
                .width(tableWidth)
                .height(56.0f)
                .gap(14.0f)
                .alignItems(eui::Align::CENTER)
                .content([&] {
                    ui.column("title")
                        .width(std::max(260.0f, tableWidth - 600.0f))
                        .height(56.0f)
                        .gap(2.0f)
                        .content([&] {
                            ui.text("title.text")
                                .height(30.0f)
                                .text("Multi-page data table")
                                .fontSize(24.0f)
                                .lineHeight(30.0f)
                                .color(tokens.text)
                                .build();
                            ui.text("title.detail")
                                .height(20.0f)
                                .text(std::to_string(kPageCount * kRowsPerPage) + " rows · " +
                                     std::to_string(kColumnCount) + " columns · " +
                                     std::to_string(kRowsPerPage * kColumnCount) + " cells per page")
                                .fontSize(12.0f)
                                .lineHeight(18.0f)
                                .color(muted)
                                .build();
                        })
                        .build();

                    components::button(ui, "toolbar.refresh")
                        .size(128.0f, 40.0f)
                        .text("Refresh")
                        .theme(tokens, false)
                        .onClick([&state] { ++state.refresh; })
                        .build();
                    components::button(ui, "toolbar.first")
                        .size(74.0f, 40.0f)
                        .text("First")
                        .theme(tokens, false)
                        .disabled(state.page == 0)
                        .onClick([&state] { state.page = 0; state.selectedRow = -1; })
                        .build();
                    components::button(ui, "toolbar.last")
                        .size(74.0f, 40.0f)
                        .text("Last")
                        .theme(tokens, false)
                        .disabled(state.page == kPageCount - 1)
                        .onClick([&state] { state.page = kPageCount - 1; state.selectedRow = -1; })
                        .build();
                })
                .build();

            ui.stack("table")
                .size(tableWidth, kHeaderHeight + bodyHeight)
                .content([&] {
                    ui.rect("table.background")
                        .size(tableWidth, kHeaderHeight + bodyHeight)
                        .color(tokens.surface)
                        .radius(tokens.metrics.radius.card)
                        .border(1.0f, border)
                        .build();

                    ui.rect("table.header.background")
                        .size(tableWidth, kHeaderHeight)
                        .color(tokens.surfaceHover)
                        .radius(tokens.metrics.radius.card)
                        .build();

                    ui.rect("table.header.bottom.patch")
                        .position(0.0f, std::max(0.0f, kHeaderHeight - tokens.metrics.radius.card))
                        .size(tableWidth, tokens.metrics.radius.card)
                        .color(tokens.surfaceHover)
                        .build();

                    for (int column = 0; column < kColumnCount; ++column) {
                        const float x = static_cast<float>(column) * columnWidth;
                        drawCell(ui, "table.header." + std::to_string(column), x, 0.0f,
                                 columnWidth, kHeaderHeight, columnLabel(column),
                                 column == 0 ? tokens.primary : muted, 11.0f);
                        if (column > 0) {
                            ui.rect("table.header.divider." + std::to_string(column))
                                .position(x, 0.0f)
                                .size(1.0f, kHeaderHeight)
                                .color(border)
                                .build();
                        }
                    }

                    components::scrollView(ui, "table.body")
                        .position(1.0f, kHeaderHeight)
                        .size(std::max(0.0f, tableWidth - 2.0f), std::max(0.0f, bodyHeight - 2.0f))
                        .theme(tokens)
                        .bind(state.scroll)
                        .step(kRowHeight)
                        .scrollbarWidth(8.0f)
                        .scrollbarGap(8.0f)
                        .contentKey("table.page." + std::to_string(state.page))
                        .content([&](eui::Ui& body, float contentWidth, float) {
                            body.stack("table.body.canvas")
                                .size(contentWidth, kRowHeight * static_cast<float>(kRowsPerPage))
                                .content([&] {
                                    for (int row = 0; row < kRowsPerPage; ++row) {
                                        const float y = static_cast<float>(row) * kRowHeight;
                                        const bool selected = state.selectedRow == row;
                                        const eui::Color rowColor = selected
                                            ? eui::mixColor(tokens.surfaceHover, tokens.primary, 0.18f)
                                            : (row % 2 == 0 ? tokens.surface : tokens.surfaceHover);

                                        body.rect("table.row." + std::to_string(row))
                                            .position(0.0f, y)
                                            .size(contentWidth, kRowHeight)
                                            .color(rowColor)
                                            .onClick([&state, row] { state.selectedRow = row; })
                                            .build();

                                        for (int column = 0; column < kColumnCount; ++column) {
                                            const float x = static_cast<float>(column) * columnWidth;
                                            drawCell(body,
                                                     "table.cell." + std::to_string(row) + "." + std::to_string(column),
                                                     x, y, columnWidth, kRowHeight,
                                                     cellValue(state.page, row, column, state.refresh),
                                                     column == 0 ? tokens.text : muted);
                                            if (column > 0) {
                                                body.rect("table.divider." + std::to_string(row) + "." + std::to_string(column))
                                                    .position(x, y)
                                                    .size(1.0f, kRowHeight)
                                                    .color(components::theme::withOpacity(border, 0.48f))
                                                    .build();
                                            }
                                        }

                                        body.rect("table.row.divider." + std::to_string(row))
                                            .position(0.0f, y + kRowHeight - 1.0f)
                                            .size(contentWidth, 1.0f)
                                            .color(components::theme::withOpacity(border, 0.58f))
                                            .build();
                                    }
                                })
                                .build();
                        })
                        .build();
                })
                .build();

            ui.row("pagination")
                .width(tableWidth)
                .height(54.0f)
                .gap(8.0f)
                .justifyContent(eui::Align::CENTER)
                .alignItems(eui::Align::CENTER)
                .content([&] {
                    components::button(ui, "pagination.previous")
                        .size(92.0f, 40.0f)
                        .text("Previous")
                        .theme(tokens, false)
                        .disabled(state.page == 0)
                        .onClick([&state] {
                            state.page = std::max(0, state.page - 1);
                            state.selectedRow = -1;
                        })
                        .build();

                    for (int page = 0; page < kPageCount; ++page) {
                        components::button(ui, "pagination.page." + std::to_string(page))
                            .size(42.0f, 40.0f)
                            .text(std::to_string(page + 1))
                            .theme(tokens, page == state.page)
                            .onClick([&state, page] {
                                state.page = page;
                                state.selectedRow = -1;
                            })
                            .build();
                    }

                    components::button(ui, "pagination.next")
                        .size(92.0f, 40.0f)
                        .text("Next")
                        .theme(tokens, false)
                        .disabled(state.page == kPageCount - 1)
                        .onClick([&state] {
                            state.page = std::min(kPageCount - 1, state.page + 1);
                            state.selectedRow = -1;
                        })
                        .build();
                })
                .build();

            ui.text("status")
                .width(tableWidth)
                .height(20.0f)
                .text(state.selectedRow < 0
                    ? "Click a row to select it; wheel over the table to scroll."
                    : "Selected row " + std::to_string(state.page * kRowsPerPage + state.selectedRow + 1) +
                      " on page " + std::to_string(state.page + 1))
                .fontSize(12.0f)
                .lineHeight(18.0f)
                .color(muted)
                .horizontalAlign(eui::HorizontalAlign::Center)
                .build();
        })
        .build();
}

} // namespace app
