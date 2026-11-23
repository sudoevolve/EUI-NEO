#pragma once

#include "components/layout.h"
#include "components/theme.h"

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace components {

/** @brief 表单的逻辑尺寸；标签和控件共用行框，通过垂直居中对齐。 */
struct FormMetrics {
    float labelWidth = 124;
    float controlHeight = 32;
    float rowGap = 12;
    float columnGap = 12;
    float padding = 16;
};

/** @brief 两列表单；内容回调在 build 时同步执行，接收实际可用的控件宽高。 */
class FormBuilder : public ComponentLayout<FormBuilder> {
public:
    using Content = std::function<void(core::dsl::Ui&, float, float)>;
    FormBuilder(core::dsl::Ui& ui, std::string id) : ui_(ui), id_(std::move(id)) {}
    FormBuilder& width(float value) { width_ = std::max(0.0f, value); return *this; }
    FormBuilder& metrics(const FormMetrics& value) { metrics_ = value; return *this; }
    FormBuilder& labelWidth(float value) { metrics_.labelWidth = std::max(0.0f, value); return *this; }
    FormBuilder& controlHeight(float value) { metrics_.controlHeight = std::max(1.0f, value); return *this; }
    FormBuilder& gap(float value) { metrics_.rowGap = std::max(0.0f, value); return *this; }
    FormBuilder& padding(float value) { metrics_.padding = std::max(0.0f, value); return *this; }
    FormBuilder& theme(const theme::ThemeColorTokens& value) {
        tokens_ = value;
        metrics_.controlHeight = value.metrics.control.control;
        metrics_.rowGap = value.metrics.spacing.control;
        metrics_.columnGap = value.metrics.spacing.control;
        metrics_.padding = value.metrics.spacing.panel;
        return *this;
    }
    /** @brief id 必须在同一表单内唯一；内容高度默认为 controlHeight。 */
    FormBuilder& row(std::string id, std::string label, Content content, float height = 0) {
        rows_.push_back({std::move(id), std::move(label), std::move(content), height, false}); return *this;
    }
    /** @brief 占满内容区的行，适用于说明或复选框组。 */
    FormBuilder& fullRow(std::string id, Content content, float height = 0) {
        rows_.push_back({std::move(id), {}, std::move(content), height, true}); return *this;
    }
    /** @brief 操作按钮与控件列对齐。 */
    FormBuilder& actions(std::string id, Content content, float height = 0) {
        return row(std::move(id), {}, std::move(content), height);
    }
    void build() {
        const float padding = std::clamp(metrics_.padding, 0.0f, width_ * .5f);
        const float inner = std::max(0.0f, width_ - padding * 2);
        const float gap = std::clamp(metrics_.columnGap, 0.0f, inner);
        const float labelWidth = std::clamp(metrics_.labelWidth, 0.0f, std::max(0.0f, inner - gap) * .45f);
        const float controlWidth = std::max(0.0f, inner - labelWidth - gap);
        const float rowGap = std::max(0.0f, metrics_.rowGap);
        float height = padding * 2 + (rows_.empty() ? 0 : rowGap * (rows_.size() - 1));
        for (const auto& row : rows_) height += resolvedHeight(row);
        layoutRoot(ui_.column(id_)).size(width_, height).padding(padding).gap(rowGap).content([&] {
            for (const auto& row : rows_) {
                const float h = resolvedHeight(row);
                const auto rowId = id_ + "." + row.id;
                ui_.row(rowId).size(inner, h).gap(gap).alignItems(core::Align::CENTER).content([&] {
                    if (!row.full) {
                        ui_.stack(rowId + ".label.box").size(labelWidth, h).clip().content([&] {
                            ui_.text(rowId + ".label").size(labelWidth, h).text(row.label)
                                .fontSize(tokens_.metrics.typography.body).color(tokens_.text)
                                .maxWidth(labelWidth).wrap(true)
                                .verticalAlign(core::VerticalAlign::Center).build();
                        }).build();
                    }
                    const float w = row.full ? inner : controlWidth;
                    ui_.stack(rowId + ".content").size(w, h).content([&] {
                        if (row.content) row.content(ui_, w, h);
                    }).build();
                }).build();
            }
        }).build();
    }
private:
    struct Row { std::string id, label; Content content; float height; bool full; };
    float resolvedHeight(const Row& row) const { return std::max(1.0f, row.height > 0 ? row.height : metrics_.controlHeight); }
    core::dsl::Ui& ui_;
    std::string id_;
    std::vector<Row> rows_;
    FormMetrics metrics_;
    theme::ThemeColorTokens tokens_ = theme::dark();
    float width_ = 600;
};

inline FormBuilder form(core::dsl::Ui& ui, const std::string& id) { return FormBuilder(ui, id); }

/** @brief 单行控件标签；保留基础 TextBuilder 的 Top 默认值用于正文。 */
inline core::dsl::TextBuilder label(core::dsl::Ui& ui, const std::string& id) {
    return ui.text(id).verticalAlign(core::VerticalAlign::Center);
}

} // namespace components
