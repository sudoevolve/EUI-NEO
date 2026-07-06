#pragma once

#include "components/theme.h"
#include "core/dsl.h"

#include <algorithm>
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

class DataTableBuilder {
public:
    DataTableBuilder(core::dsl::Ui& ui, std::string id)
        : ui_(ui), id_(std::move(id)) {}

    DataTableBuilder& size(float width, float height) { width_ = width; height_ = height; return *this; }
    DataTableBuilder& columns(std::vector<std::string> value) { columns_ = std::move(value); return *this; }
    DataTableBuilder& rows(std::vector<std::vector<std::string>> value) { rows_ = std::move(value); return *this; }
    DataTableBuilder& style(const DataTableStyle& value) { style_ = value; return *this; }
    DataTableBuilder& theme(const theme::ThemeColorTokens& tokens) { style_ = DataTableStyle(tokens); return *this; }
    DataTableBuilder& transition(const core::Transition& value) { transition_ = value; return *this; }

    void build() {
        const int columnCount = std::max(1, static_cast<int>(columns_.size()));
        const int rowCount = static_cast<int>(rows_.size());
        const float borderWidth = 1.0f;
        const float contentX = borderWidth;
        const float contentY = borderWidth;
        const float contentWidth = std::max(0.0f, width_ - borderWidth * 2.0f);
        const float contentHeight = std::max(0.0f, height_ - borderWidth * 2.0f);
        const float contentRadius = std::max(0.0f, style_.radius - borderWidth);
        const float headerHeight = std::min(38.0f, contentHeight);
        const float bodyHeight = std::max(0.0f, contentHeight - headerHeight);
        const float rowHeight = rowCount > 0 ? bodyHeight / static_cast<float>(rowCount) : bodyHeight;
        const float columnWidth = contentWidth / static_cast<float>(columnCount);
        const float headerPatchY = std::max(0.0f, headerHeight - contentRadius);
        const float headerPatchHeight = std::min(contentRadius, headerHeight);
        const float textInset = 16.0f;

        ui_.stack(id_)
            .size(width_, height_)
            .content([&] {
                ui_.rect(id_ + ".bg")
                    .size(width_, height_)
                    .color(style_.background)
                    .radius(style_.radius)
                    .border(borderWidth, style_.border)
                    .build();

                ui_.stack(id_ + ".content")
                    .x(contentX)
                    .y(contentY)
                    .size(contentWidth, contentHeight)
                    .clip()
                    .content([&] {
                        ui_.rect(id_ + ".header.cap")
                            .size(contentWidth, headerHeight)
                            .color(style_.header)
                            .radius(contentRadius)
                            .build();

                        if (headerPatchHeight > 0.0f) {
                            ui_.rect(id_ + ".header.patch")
                                .y(headerPatchY)
                                .size(contentWidth, headerPatchHeight)
                                .color(style_.header)
                                .build();
                        }

                        ui_.rect(id_ + ".header.divider")
                            .y(std::max(0.0f, headerHeight - 1.0f))
                            .size(contentWidth, 1.0f)
                            .color(style_.divider)
                            .build();

                        for (int column = 0; column < columnCount; ++column) {
                            const float x = static_cast<float>(column) * columnWidth;
                            const std::string label = column < static_cast<int>(columns_.size()) ? columns_[column] : "";
                            ui_.text(id_ + ".header." + std::to_string(column))
                                .x(x + textInset)
                                .size(std::max(0.0f, columnWidth - textInset * 2.0f), headerHeight)
                                .text(label)
                                .fontSize(14.0f)
                                .lineHeight(18.0f)
                                .color(column == 0 ? style_.accent : style_.mutedText)
                                .verticalAlign(core::VerticalAlign::Center)
                                .build();
                        }

                        for (int row = 0; row < rowCount; ++row) {
                            const bool lastRow = row == rowCount - 1;
                            const float y = headerHeight + static_cast<float>(row) * rowHeight;
                            const float nextY = lastRow
                                ? contentHeight
                                : headerHeight + static_cast<float>(row + 1) * rowHeight;
                            const float height = std::max(0.0f, nextY - y);
                            const float topPatchHeight = std::min(contentRadius, height);
                            const core::Color rowColor = row % 2 == 0 ? style_.row : style_.rowAlt;

                            ui_.rect(id_ + ".row." + std::to_string(row))
                                .y(y)
                                .size(contentWidth, height)
                                .color(rowColor)
                                .radius(lastRow ? contentRadius : 0.0f)
                                .build();

                            if (lastRow && topPatchHeight > 0.0f) {
                                ui_.rect(id_ + ".row." + std::to_string(row) + ".top.patch")
                                    .y(y)
                                    .size(contentWidth, topPatchHeight)
                                    .color(rowColor)
                                    .build();
                            }

                            if (!lastRow) {
                                ui_.rect(id_ + ".row." + std::to_string(row) + ".divider")
                                    .y(std::max(headerHeight, nextY - 1.0f))
                                    .size(contentWidth, 1.0f)
                                    .color(style_.divider)
                                    .build();
                            }

                            for (int column = 0; column < columnCount; ++column) {
                                const float x = static_cast<float>(column) * columnWidth;
                                const std::string value =
                                    column < static_cast<int>(rows_[row].size()) ? rows_[row][column] : "";
                                ui_.text(id_ + ".cell." + std::to_string(row) + "." + std::to_string(column))
                                    .x(x + textInset)
                                    .y(y)
                                    .size(std::max(0.0f, columnWidth - textInset * 2.0f), height)
                                    .text(value)
                                    .fontSize(14.0f)
                                    .lineHeight(18.0f)
                                    .color(column == 0 ? style_.text : style_.mutedText)
                                    .verticalAlign(core::VerticalAlign::Center)
                                    .transition(transition_)
                                    .animate(core::AnimProperty::TextColor)
                                    .build();
                            }
                        }
                    })
                    .build();
            })
            .build();
    }

private:
    core::dsl::Ui& ui_;
    std::string id_;
    std::vector<std::string> columns_;
    std::vector<std::vector<std::string>> rows_;
    DataTableStyle style_;
    core::Transition transition_ = core::Transition::make(0.12f, core::Ease::OutCubic);
    float width_ = 420.0f;
    float height_ = 174.0f;
};

inline DataTableBuilder dataTable(core::dsl::Ui& ui, const std::string& id) {
    return DataTableBuilder(ui, id);
}

} // namespace components
