#pragma once

#include "components/theme.h"
#include "components/tooltip.h"
#include "core/dsl.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>
#include <memory>
#include <limits>
#include <cstdint>

namespace components {

struct LineChartStyle {
    LineChartStyle() : LineChartStyle(theme::dark()) {}

    explicit LineChartStyle(const theme::ThemeColorTokens& tokens) {
        background = tokens.surface;
        title = tokens.text;
        label = theme::withOpacity(tokens.text, 0.56f);
        grid = theme::withOpacity(tokens.border, tokens.dark ? 0.38f : 0.36f);
        line = tokens.primary;
        point = tokens.primary;
        pointHover = core::mixColor(tokens.primary, theme::color(1.0f, 1.0f, 1.0f), tokens.dark ? 0.18f : 0.10f);
        pointPressed = core::mixColor(tokens.primary, theme::color(0.0f, 0.0f, 0.0f), 0.12f);
        tooltipBackground = tokens.dark
            ? core::mixColor(tokens.surface, theme::color(0.0f, 0.0f, 0.0f), 0.18f)
            : theme::color(1.0f, 1.0f, 1.0f, 0.96f);
        tooltipText = tokens.text;
        border = theme::withOpacity(tokens.border, 0.76f);
        shadow = theme::shadow(tokens, 18.0f, 4.0f, 0.20f, 0.10f);
        radius = tokens.metrics.radius.section;
    }

    core::Color background;
    core::Color title;
    core::Color label;
    core::Color grid;
    core::Color line;
    core::Color point;
    core::Color pointHover;
    core::Color pointPressed;
    core::Color tooltipBackground;
    core::Color tooltipText;
    core::Color border;
    core::Shadow shadow;
    float radius = 18.0f;
};

enum class LineStyle {
    Linear,
    Curve,
    Step
};

namespace linechart_detail {
// 每个横向桶最多保留首、谷、峰、尾，按原始顺序输出；含缺失值的桶断开路径。
inline std::vector<core::Vec2> downsample(const std::vector<float>& values, std::size_t first,
    std::size_t last, float width, float height, float minimum, float maximum) {
    std::vector<core::Vec2> points;
    last = std::min(last, values.size());
    first = std::min(first, last);
    const auto count = last - first;
    if (!count || !std::isfinite(width) || width <= 0 || !std::isfinite(minimum) ||
        !std::isfinite(maximum) || maximum <= minimum) return points;
    const auto buckets = std::min(count, static_cast<std::size_t>(std::max(1.0f, std::ceil(width))));
    points.reserve(buckets * 4);
    for (std::size_t bucket = 0; bucket < buckets; ++bucket) {
        const auto begin = first + count * bucket / buckets;
        const auto end = first + count * (bucket + 1) / buckets;
        auto low = begin, high = begin;
        bool gap = false;
        for (auto i = begin; i < end; ++i) {
            if (!std::isfinite(values[i])) { gap = true; continue; }
            if (values[i] < values[low]) low = i;
            if (values[i] > values[high]) high = i;
        }
        if (gap) { points.push_back({std::numeric_limits<float>::quiet_NaN(), 0}); continue; }
        std::size_t indices[] = {begin, low, high, end - 1};
        std::sort(std::begin(indices), std::end(indices));
        std::size_t previous = last;
        for (auto index : indices) {
            if (index == previous) continue;
            previous = index;
            const float normalized = static_cast<float>((static_cast<double>(values[index]) - minimum) /
                (static_cast<double>(maximum) - minimum));
            points.push_back({count > 1 ? width * static_cast<float>(index - first) / static_cast<float>(count - 1) : width * .5f,
                height * (1 - std::clamp(normalized, 0.0f, 1.0f))});
        }
    }
    return points;
}
struct Cache {
    std::shared_ptr<const std::vector<float>> source;
    std::uint64_t revision = 0;
    std::size_t first = 0, last = 0;
    float width = -1, height = -1, minimum = 0, maximum = 1;
    std::vector<core::Vec2> points;
};
} // namespace linechart_detail

class LineChartBuilder {
    struct TooltipItem {
        std::string sourceId;
        std::string text;
        float x = 0.0f;
        float y = 0.0f;
    };

public:
    LineChartBuilder(core::dsl::Ui& ui, std::string id)
        : ui_(ui), id_(std::move(id)) {}

    LineChartBuilder& size(float width, float height) { width_ = width; height_ = height; return *this; }
    LineChartBuilder& title(const std::string& value) { title_ = value; return *this; }
    LineChartBuilder& values(std::vector<float> value) { values_ = std::move(value); return *this; }
    /** @brief 实时模式避免复制源数据；原地修改后递增 revision，compose 期间不得并发写入。 */
    LineChartBuilder& data(std::shared_ptr<const std::vector<float>> value, std::uint64_t revision = 0) {
        data_ = std::move(value); revision_ = revision; realtime_ = true; return *this;
    }
    LineChartBuilder& realtime(bool value = true) { realtime_ = value; return *this; }
    /** @brief 等间距采样的可见半开区间 [first, last)。 */
    LineChartBuilder& visibleRange(std::size_t first, std::size_t last) { first_ = first; last_ = last; return *this; }
    LineChartBuilder& range(float minimum, float maximum) { minimum_ = minimum; maximum_ = maximum; return *this; }
    LineChartBuilder& labels(std::vector<std::string> value) { labels_ = std::move(value); return *this; }
    LineChartBuilder& style(const LineChartStyle& value) { style_ = value; return *this; }
    LineChartBuilder& style(LineStyle value) { lineStyle_ = value; return *this; }
    LineChartBuilder& theme(const theme::ThemeColorTokens& tokens) {
        style_ = LineChartStyle(tokens);
        tokens_ = tokens;
        return *this;
    }
    LineChartBuilder& transition(const core::Transition& value) { transition_ = value; return *this; }

    void build() {
        if (realtime_) { buildRealtime(); return; }
        if (values_.empty()) {
            values_ = {0.22f, 0.30f, 0.20f, 0.55f, 0.42f, 0.86f};
        }

        const theme::ThemeMetricTokens& metrics = tokens_.metrics;
        const float titleX = metrics.spacing.large;
        const float titleY = metrics.typography.control;
        const float plotX = 28.0f;
        const float plotY = 70.0f;
        const float plotWidth = std::max(1.0f, width_ - 56.0f);
        const float plotHeight = std::max(1.0f, height_ - 112.0f);
        const float bottomY = plotY + plotHeight;
        AnimState& anim = ui_.state<AnimState>(id_ + ".anim");
        syncAnimation(anim);
        const std::vector<float>& displayValues = anim.display;
        const int count = static_cast<int>(displayValues.size());
        const float stepX = count > 1 ? plotWidth / static_cast<float>(count - 1) : 0.0f;
        const float lineHeight = 4.0f;
        const float pointSize = 13.0f;
        std::vector<core::Vec2> displayPoints;
        displayPoints.reserve(displayValues.size());
        for (int index = 0; index < count; ++index) {
            displayPoints.push_back(pointAt(displayValues, index, plotX, bottomY, plotHeight, stepX));
        }

        ui_.stack(id_)
            .size(width_, height_)
            .content([&] {
                ui_.rect(id_ + ".bg")
                    .size(width_, height_)
                    .color(style_.background)
                    .radius(style_.radius)
                    .border(1.0f, style_.border)
                    .shadow(style_.shadow)
                    .build();

                ui_.text(id_ + ".title")
                    .x(titleX)
                    .y(titleY)
                    .size(std::max(0.0f, width_ - titleX * 2.0f), metrics.control.compact)
                    .text(title_)
                    .fontSize(metrics.typography.title)
                    .lineHeight(metrics.typography.title + metrics.typography.lineGap)
                    .color(style_.title)
                    .build();

                for (int line = 0; line < 4; ++line) {
                    const float y = plotY + static_cast<float>(line) * plotHeight / 3.0f;
                    ui_.rect(id_ + ".grid." + std::to_string(line))
                        .x(plotX)
                        .y(y)
                        .size(plotWidth, metrics.spacing.hairline)
                        .color(style_.grid)
                        .build();
                }

                auto drawCapsuleSegment = [&](const std::string& segmentId, const core::Vec2& from, const core::Vec2& to) {
                    const float dx = to.x - from.x;
                    const float dy = to.y - from.y;
                    if (std::sqrt(dx * dx + dy * dy) <= 0.001f) {
                        return;
                    }
                    const float radius = lineHeight * 0.5f;
                    const float segmentX = std::min(from.x, to.x) - radius;
                    const float segmentY = std::min(from.y, to.y) - radius;
                    const float segmentWidth = std::fabs(to.x - from.x) + lineHeight;
                    const float segmentHeight = std::fabs(to.y - from.y) + lineHeight;

                    ui_.polygon(segmentId)
                        .x(segmentX)
                        .y(segmentY)
                        .size(segmentWidth, segmentHeight)
                        .points(capsulePoints(from, to, radius, segmentX, segmentY))
                        .color(style_.line)
                        .transition(transition_)
                        .build();
                };

                if (lineStyle_ == LineStyle::Curve && count > 2) {
                    const std::vector<core::Vec2> curve = curvePoints(displayPoints, plotY, bottomY);
                    for (std::size_t index = 0; index + 1 < curve.size(); ++index) {
                        drawCapsuleSegment(id_ + ".segment.curve." + std::to_string(index), curve[index], curve[index + 1]);
                    }
                } else {
                    for (int index = 0; index + 1 < count; ++index) {
                        const core::Vec2 from = displayPoints[static_cast<std::size_t>(index)];
                        const core::Vec2 to = displayPoints[static_cast<std::size_t>(index + 1)];
                        if (lineStyle_ == LineStyle::Step) {
                            const core::Vec2 elbow = {to.x, from.y};
                            drawCapsuleSegment(id_ + ".segment." + std::to_string(index) + ".h", from, elbow);
                            drawCapsuleSegment(id_ + ".segment." + std::to_string(index) + ".v", elbow, to);
                        } else {
                            drawCapsuleSegment(id_ + ".segment." + std::to_string(index), from, to);
                        }
                    }
                }

                std::vector<TooltipItem> tooltips;
                tooltips.reserve(displayValues.size());
                for (int index = 0; index < count; ++index) {
                    const core::Vec2 point = displayPoints[static_cast<std::size_t>(index)];
                    const std::string pointId = id_ + ".point." + std::to_string(index);
                    ui_.rect(pointId)
                        .x(point.x - pointSize * 0.5f)
                        .y(point.y - pointSize * 0.5f)
                        .size(pointSize, pointSize)
                        .states(style_.point, style_.pointHover, style_.pointPressed)
                        .radius(pointSize * 0.5f)
                        .instantStates()
                        .transition(transition_)
                        .animate(core::AnimProperty::Color)
                        .build();

                    tooltips.push_back({pointId, dataLabel(index) + "  " + percent(valueAt(values_, index)), point.x, point.y});
                }

                const float labelWidth = std::max(28.0f, std::min(42.0f, count > 1 ? stepX : plotWidth));
                for (int index = 0; index < count; ++index) {
                    const core::Vec2 point = displayPoints[static_cast<std::size_t>(index)];
                    const float labelX = std::clamp(point.x - labelWidth * 0.5f, 0.0f, std::max(0.0f, width_ - labelWidth));
                    label(id_ + ".label." + std::to_string(index), dataLabel(index), labelX, height_ - 34.0f, labelWidth);
                }

                for (const TooltipItem& item : tooltips) {
                    components::tooltip(ui_, item.sourceId + ".tooltip")
                        .theme(tokens_)
                        .source(item.sourceId)
                        .value(item.text)
                        .anchor(item.x, item.y)
                        .bounds(width_, height_)
                        .style(tooltipStyle())
                        .build();
                }

                if (anim.animating) {
                    ui_.stack(id_ + ".animator")
                        .size(0.0f, 0.0f)
                        .onTimer(0.016f, [] {})
                        .build();
                }
            })
            .build();
    }

private:
    void buildRealtime() {
        auto& cache = ui_.state<linechart_detail::Cache>(id_ + ".samples");
        auto source = data_ ? data_ : std::make_shared<const std::vector<float>>(std::move(values_));
        const float w = std::max(1.0f, width_ - 56), h = std::max(1.0f, height_ - 112);
        if (cache.source != source || cache.revision != revision_ || cache.first != first_ || cache.last != last_ ||
            cache.width != w || cache.height != h || cache.minimum != minimum_ || cache.maximum != maximum_) {
            cache.source = source; cache.revision = revision_; cache.first = first_; cache.last = last_;
            cache.width = w; cache.height = h; cache.minimum = minimum_; cache.maximum = maximum_;
            cache.points = linechart_detail::downsample(*source, first_, last_, w, h, minimum_, maximum_);
        }
        ui_.stack(id_).size(width_, height_).clip().content([&] {
            ui_.rect(id_ + ".bg").size(width_, height_).color(style_.background).radius(style_.radius)
                .border(1, style_.border).build();
            ui_.text(id_ + ".title").x(28).y(20).size(w, 30).text(title_)
                .fontSize(tokens_.metrics.typography.title).color(style_.title).build();
            for (int i = 0; i < 4; ++i) ui_.rect(id_ + ".grid." + std::to_string(i)).x(28).y(70 + h * i / 3)
                .size(w, 1).color(style_.grid).build();
            ui_.stack(id_ + ".plot").x(26).y(68).size(w + 4, h + 4).clip().content([&] {
                ui_.polygon(id_ + ".line").x(2).y(2).size(w, h).points(cache.points).strokeWidth(2)
                    .color(style_.line).build();
                if (cache.points.size() == 1 && std::isfinite(cache.points.front().x)) {
                    const auto p = cache.points.front();
                    ui_.rect(id_ + ".single").x(p.x).y(p.y).size(4, 4).radius(2).color(style_.line).build();
                }
            }).build();
        }).build();
    }
    struct AnimState {
        std::vector<float> display;
        std::vector<float> target;
        bool animating = false;
    };

    static float valueAt(const std::vector<float>& values, int index) {
        if (index < 0 || index >= static_cast<int>(values.size())) {
            return 0.0f;
        }
        return values[static_cast<std::size_t>(index)];
    }

    static bool closeValues(const std::vector<float>& left, const std::vector<float>& right) {
        if (left.size() != right.size()) {
            return false;
        }
        for (std::size_t i = 0; i < left.size(); ++i) {
            if (std::fabs(left[i] - right[i]) > 0.001f) {
                return false;
            }
        }
        return true;
    }

    void syncAnimation(AnimState& anim) const {
        if (anim.display.size() != values_.size()) {
            anim.display = values_;
            anim.target = values_;
            anim.animating = false;
            return;
        }

        if (!closeValues(anim.target, values_)) {
            anim.target = values_;
            anim.animating = true;
        }

        if (!anim.animating) {
            return;
        }

        bool moving = false;
        for (std::size_t i = 0; i < anim.display.size(); ++i) {
            const float next = anim.display[i] + (anim.target[i] - anim.display[i]) * 0.20f;
            if (std::fabs(next - anim.target[i]) > 0.002f) {
                anim.display[i] = next;
                moving = true;
            } else {
                anim.display[i] = anim.target[i];
            }
        }
        anim.animating = moving;
    }

    static core::Vec2 pointAt(const std::vector<float>& values,
                              int index,
                              float plotX,
                              float bottomY,
                              float plotHeight,
                              float stepX) {
        const float value = std::clamp(valueAt(values, index), 0.0f, 1.0f);
        return {
            plotX + static_cast<float>(index) * stepX,
            bottomY - value * plotHeight
        };
    }

    static std::vector<core::Vec2> capsulePoints(const core::Vec2& from,
                                                 const core::Vec2& to,
                                                 float radius,
                                                 float originX,
                                                 float originY) {
        const float dx = to.x - from.x;
        const float dy = to.y - from.y;
        const float length = std::sqrt(dx * dx + dy * dy);
        if (length <= 0.001f) {
            return circlePoints(from, radius, originX, originY);
        }

        const float angle = std::atan2(dy, dx);
        const float normalAngle = angle + 1.57079632679f;
        std::vector<core::Vec2> points;
        points.reserve(18u);
        appendArc(points, to, radius, normalAngle, normalAngle - 3.14159265359f, originX, originY);
        appendArc(points, from, radius, normalAngle - 3.14159265359f, normalAngle - 6.28318530718f, originX, originY);
        return points;
    }

    static core::Vec2 catmullRom(const core::Vec2& p0,
                                 const core::Vec2& p1,
                                 const core::Vec2& p2,
                                 const core::Vec2& p3,
                                 float t) {
        const float t2 = t * t;
        const float t3 = t2 * t;
        return {
            0.5f * ((2.0f * p1.x) + (-p0.x + p2.x) * t +
                    (2.0f * p0.x - 5.0f * p1.x + 4.0f * p2.x - p3.x) * t2 +
                    (-p0.x + 3.0f * p1.x - 3.0f * p2.x + p3.x) * t3),
            0.5f * ((2.0f * p1.y) + (-p0.y + p2.y) * t +
                    (2.0f * p0.y - 5.0f * p1.y + 4.0f * p2.y - p3.y) * t2 +
                    (-p0.y + 3.0f * p1.y - 3.0f * p2.y + p3.y) * t3)
        };
    }

    static std::vector<core::Vec2> curvePoints(const std::vector<core::Vec2>& points, float minY, float maxY) {
        constexpr int samplesPerSegment = 8;
        std::vector<core::Vec2> result;
        if (points.size() < 2) {
            return result;
        }
        result.reserve((points.size() - 1u) * samplesPerSegment + 1u);
        result.push_back(points.front());
        for (std::size_t index = 0; index + 1 < points.size(); ++index) {
            const core::Vec2& p0 = points[index == 0 ? 0 : index - 1u];
            const core::Vec2& p1 = points[index];
            const core::Vec2& p2 = points[index + 1u];
            const core::Vec2& p3 = points[index + 2u < points.size() ? index + 2u : points.size() - 1u];
            for (int sample = 1; sample <= samplesPerSegment; ++sample) {
                core::Vec2 point = catmullRom(p0, p1, p2, p3, static_cast<float>(sample) / static_cast<float>(samplesPerSegment));
                point.y = std::clamp(point.y, minY, maxY);
                result.push_back(point);
            }
        }
        return result;
    }

    static std::vector<core::Vec2> circlePoints(const core::Vec2& center, float radius, float originX, float originY) {
        std::vector<core::Vec2> points;
        points.reserve(18u);
        appendArc(points, center, radius, 0.0f, 6.28318530718f, originX, originY);
        return points;
    }

    static void appendArc(std::vector<core::Vec2>& points,
                          const core::Vec2& center,
                          float radius,
                          float startAngle,
                          float endAngle,
                          float originX,
                          float originY) {
        constexpr int segments = 8;
        for (int step = 0; step <= segments; ++step) {
            const float t = static_cast<float>(step) / static_cast<float>(segments);
            const float angle = startAngle + (endAngle - startAngle) * t;
            points.push_back({
                center.x + std::cos(angle) * radius - originX,
                center.y + std::sin(angle) * radius - originY
            });
        }
    }

    void label(const std::string& id, const std::string& value, float x, float y, float width) {
        ui_.text(id)
            .x(x)
            .y(y)
            .size(width, tokens_.metrics.control.indicator)
            .text(value)
            .fontSize(tokens_.metrics.typography.label)
            .lineHeight(tokens_.metrics.typography.label + tokens_.metrics.typography.lineGap)
            .color(style_.label)
            .horizontalAlign(core::HorizontalAlign::Center)
            .build();
    }

    std::string dataLabel(int index) const {
        if (index >= 0 && index < static_cast<int>(labels_.size())) {
            return labels_[index];
        }
        return "P" + std::to_string(index + 1);
    }

    static std::string percent(float value) {
        return std::to_string(static_cast<int>(std::clamp(value, 0.0f, 1.0f) * 100.0f + 0.5f)) + "%";
    }

    TooltipStyle tooltipStyle() const {
        TooltipStyle result;
        result.background = style_.tooltipBackground;
        result.text = style_.tooltipText;
        result.border = style_.border;
        return result;
    }

    core::dsl::Ui& ui_;
    std::string id_;
    std::string title_ = "LineChart";
    std::vector<float> values_;
    std::shared_ptr<const std::vector<float>> data_;
    std::uint64_t revision_ = 0;
    std::size_t first_ = 0, last_ = std::numeric_limits<std::size_t>::max();
    float minimum_ = 0, maximum_ = 1;
    bool realtime_ = false;
    std::vector<std::string> labels_ = {"Jan", "Feb", "Mar", "Apr", "May", "Jun"};
    LineChartStyle style_;
    theme::ThemeColorTokens tokens_ = theme::dark();
    LineStyle lineStyle_ = LineStyle::Linear;
    core::Transition transition_ = core::Transition::make(0.16f, core::Ease::OutCubic);
    float width_ = 206.0f;
    float height_ = 236.0f;
};

inline LineChartBuilder lineChart(core::dsl::Ui& ui, const std::string& id) {
    return LineChartBuilder(ui, id);
}

} // namespace components
