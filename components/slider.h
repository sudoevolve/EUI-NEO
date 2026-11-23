#pragma once

#include "components/layout.h"

#include "components/theme.h"
#include "core/dsl.h"
#include "eui/signal.h"

#include <algorithm>
#include <functional>
#include <string>
#include <utility>

namespace components {

struct SliderStyle {
    SliderStyle() : SliderStyle(theme::dark()) {}

    explicit SliderStyle(const theme::ThemeColorTokens& tokens) {
        track = core::mixColor(tokens.surfaceHover, tokens.surfaceActive, tokens.dark ? 0.24f : 0.18f);
        fill = tokens.primary;
        knob = tokens.text;
    }

    core::Color track;
    core::Color fill;
    core::Color knob;
};

class SliderBuilder : public ComponentLayout<SliderBuilder> {
public:
    SliderBuilder(core::dsl::Ui& ui, std::string id)
        : ui_(ui), id_(std::move(id)) {}

    SliderBuilder& size(float width, float height) { width_ = width; height_ = height; return *this; }
    SliderBuilder& value(float value) { value_ = value; return *this; }
    /** @brief 业务值范围，默认 0..1；端点逆序时自动交换，相等时为固定值。 */
    SliderBuilder& range(float minimum, float maximum) {
        if (std::isfinite(minimum) && std::isfinite(maximum)) {
            minimum_ = std::min(minimum, maximum); maximum_ = std::max(minimum, maximum);
        }
        return *this;
    }
    /** @brief 相对最小值吸附的业务步长；0 表示连续取值。 */
    SliderBuilder& step(float value) { step_ = std::isfinite(value) && value > 0 ? value : 0; return *this; }
    SliderBuilder& accessibleName(std::string value) { accessibleName_ = std::move(value); return *this; }
    SliderBuilder& disabled(bool value = true) { disabled_ = value; return *this; }
    SliderBuilder& bind(eui::Signal<float>& signal) {
        value(signal.get());
        onChange([&signal](float value) { signal.set(value); });
        return *this;
    }
    SliderBuilder& style(const SliderStyle& value) { style_ = value; return *this; }
    SliderBuilder& theme(const theme::ThemeColorTokens& tokens) {
        style_ = SliderStyle(tokens);
        metrics_ = tokens.metrics;
        return *this;
    }
    SliderBuilder& transition(const core::Transition& value) { transition_ = value; return *this; }
    SliderBuilder& transition(float duration, core::Ease ease = core::Ease::OutCubic) {
        transition_ = core::Transition::make(duration, ease);
        return *this;
    }
    SliderBuilder& onChange(std::function<void(float)> callback) { onChange_ = std::move(callback); return *this; }

    void build() {
        const float minimum = minimum_, maximum = maximum_, step = step_;
        const double span = static_cast<double>(maximum) - minimum;
        const auto snap = [minimum, maximum, step](double value) {
            double result = std::isnan(value) ? minimum : std::clamp(value, static_cast<double>(minimum), static_cast<double>(maximum));
            if (step > 0 && result > minimum && result < maximum) {
                result = minimum + std::round((result - minimum) / step) * step;
            }
            return static_cast<float>(std::clamp(result, static_cast<double>(minimum), static_cast<double>(maximum)));
        };
        const float value = snap(value_);
        const float unit = span > 0 ? static_cast<float>((value - static_cast<double>(minimum)) / span) : 0;
        const auto emit = onChange_;
        const auto onChange = [emit, minimum, span, snap](float normalized) {
            if (emit) emit(snap(static_cast<float>(minimum + span * normalized)));
        };
        const float trackHeight = std::max(metrics_.spacing.tiny - metrics_.spacing.hairline,
                                           height_ * 0.18f);
        const float trackY = (height_ - trackHeight) * 0.5f;
        const float knobSize = std::max(metrics_.typography.label, height_ * 0.72f);

        layoutRoot(ui_.stack(id_))
            .disabled(disabled_ || span <= 0)
            .size(width_, height_)
            .sliderState(id_, unit, width_, knobSize, onChange, span > 0 ? static_cast<float>(step / span) : 0)
            .content([&] {
                ui_.rect(id_ + ".track")
                    .y(trackY)
                    .size(width_, trackHeight)
                    .color(style_.track)
                    .radius(trackHeight * 0.5f)
                    .build();

                ui_.rect(id_ + ".fill")
                    .y(trackY)
                    .size(width_ * unit, trackHeight)
                    .color(style_.fill)
                    .radius(trackHeight * 0.5f)
                    .transition(transition_)
                    .animate(core::AnimProperty::Color)
                    .sliderFillFrom(id_)
                    .build();

                ui_.rect(id_ + ".knob")
                    .y((height_ - knobSize) * 0.5f)
                    .size(knobSize, knobSize)
                    .color(style_.knob)
                    .radius(knobSize * 0.5f)
                    .shadow(12.0f, 0.0f, 4.0f, theme::withAlpha(style_.fill, 0.20f))
                    .transition(transition_)
                    .animate(core::AnimProperty::Color | core::AnimProperty::Shadow)
                    .sliderKnobFrom(id_)
                    .build();

                // Hit area keeps input priority via composition order (last
                // child of this stack): ordered children are hit-tested in
                // reverse, so no zIndex is needed here. A raised internal
                // zIndex inflates this subtree's max z-index and makes the
                // whole slider outrank host-level siblings (e.g. root-level
                // overlays/scrim/menus composed at default z=0), breaking
                // their hit testing and paint order.
                ui_.rect(id_ + ".hit")
                    .accessible(core::AccessibleRole::Slider, accessibleName_, std::to_string(value))
                    .onKeyEvent([emit, value, minimum, maximum, step, span, snap](const core::KeyEvent& key) {
                        if (!key.isDown()) return false;
                        double next = value;
                        const auto moveTicks = [&](int count) {
                            if (step <= 0) return value + span * .01 * count;
                            const double index = (static_cast<double>(value) - minimum) / step;
                            // maximum 也属于合法刻度，即使范围不是 step 的整数倍。
                            const double endIndex = std::round(span / step);
                            const bool endOnTick = static_cast<float>(minimum + endIndex * step) == maximum;
                            const double tick = value == maximum ? (endOnTick ? endIndex : std::ceil(span / step)) : std::round(index);
                            return static_cast<double>(minimum) + (tick + count) * step;
                        };
                        switch (key.key) {
                        case core::InputKey::Left: case core::InputKey::Down: next = moveTicks(-1); break;
                        case core::InputKey::Right: case core::InputKey::Up: next = moveTicks(1); break;
                        case core::InputKey::Home: next = minimum; break;
                        case core::InputKey::End: next = maximum; break;
                        case core::InputKey::PageDown: next = moveTicks(-10); break;
                        case core::InputKey::PageUp: next = moveTicks(10); break;
                        default: return false;
                        }
                        if (emit) emit(snap(next));
                        return true;
                    })
                    .size(width_, height_)
                    .states(theme::color(0.0f, 0.0f, 0.0f, 0.0f),
                            theme::color(0.0f, 0.0f, 0.0f, 0.0f),
                            theme::color(0.0f, 0.0f, 0.0f, 0.0f))
                    .interactive()
                    .sliderInputFrom(id_)
                    .build();
            })
            .build();
    }

private:
    core::dsl::Ui& ui_;
    std::string id_;
    std::string accessibleName_ = "Slider";
    bool disabled_ = false;
    SliderStyle style_;
    theme::ThemeMetricTokens metrics_;
    core::Transition transition_ = core::Transition::make(0.16f, core::Ease::OutCubic);
    std::function<void(float)> onChange_;
    float width_ = 300.0f;
    float height_ = 32.0f;
    float value_ = 0.0f;
    float minimum_ = 0.0f, maximum_ = 1.0f, step_ = 0.0f;
};

inline SliderBuilder slider(core::dsl::Ui& ui, const std::string& id) {
    return SliderBuilder(ui, id);
}

} // namespace components
