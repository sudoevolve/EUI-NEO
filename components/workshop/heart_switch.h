#pragma once

#include "components/theme.h"
#include "core/dsl.h"
#include "eui/signal.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <string>
#include <utility>

namespace components::workshop {

struct HeartSwitchStyle {
    core::Color heart{1.0f, 91.0f / 255.0f, 137.0f / 255.0f, 1.0f};
    core::Color hover{1.0f, 113.0f / 255.0f, 153.0f / 255.0f, 1.0f};
    core::Color pressed{0.90f, 65.0f / 255.0f, 112.0f / 255.0f, 1.0f};
};

inline HeartSwitchStyle heartSwitchStyle(const theme::ThemeColorTokens& tokens) {
    HeartSwitchStyle style;
    if (tokens.dark) {
        style.heart = {1.0f, 0.42f, 0.62f, 1.0f};
        style.hover = {1.0f, 0.50f, 0.68f, 1.0f};
        style.pressed = {0.92f, 0.28f, 0.50f, 1.0f};
    }
    return style;
}

class HeartSwitchBuilder {
public:
    HeartSwitchBuilder(core::dsl::Ui& ui, std::string id)
        : ui_(ui), id_(std::move(id)) {}

    HeartSwitchBuilder& size(float width, float height) {
        width_ = width;
        height_ = height;
        return *this;
    }

    HeartSwitchBuilder& checked(bool value) {
        checked_ = value;
        return *this;
    }

    HeartSwitchBuilder& bind(eui::Signal<bool>& signal) {
        checked(signal.get());
        onChange([&signal](bool value) { signal.set(value); });
        return *this;
    }

    HeartSwitchBuilder& style(const HeartSwitchStyle& value) {
        style_ = value;
        return *this;
    }

    HeartSwitchBuilder& theme(const theme::ThemeColorTokens& tokens) {
        style_ = heartSwitchStyle(tokens);
        return *this;
    }

    HeartSwitchBuilder& transition(const core::Transition& value) {
        transition_ = value;
        return *this;
    }

    HeartSwitchBuilder& transition(float duration, core::Ease ease = core::Ease::OutCubic) {
        transition_ = core::Transition::make(duration, ease);
        return *this;
    }

    HeartSwitchBuilder& disabled(bool value = true) {
        disabled_ = value;
        return *this;
    }

    HeartSwitchBuilder& onChange(std::function<void(bool)> callback) {
        onChange_ = std::move(callback);
        return *this;
    }

    void build() {
        HeartSwitchState& state = ui_.state<HeartSwitchState>(id_);
        if (!state.initialized) {
            state.initialized = true;
            state.lastChecked = checked_;
        }
        if (checked_ && !state.lastChecked) {
            state.celebrationSeconds = kCelebrationDuration;
        }
        state.lastChecked = checked_;

        const float w = std::max(1.0f, width_);
        const float h = std::max(1.0f, height_);
        const float side = std::min(w, h);
        const float iconSize = side * 0.74f;
        const float iconX = (w - iconSize) * 0.5f;
        const float iconY = (h - iconSize) * 0.5f;
        const float celebrationT = state.celebrationSeconds > 0.0f
            ? 1.0f - std::clamp(state.celebrationSeconds / kCelebrationDuration, 0.0f, 1.0f)
            : 1.0f;
        const float filledScale = checked_ ? fillScale(celebrationT, state.celebrationSeconds > 0.0f) : 0.0f;
        const float celebrateScale = state.celebrationSeconds > 0.0f ? 1.4f * easeOut(celebrationT) : 1.4f;
        const float celebrateOpacity = state.celebrationSeconds > 0.0f
            ? (celebrationT < 0.50f ? celebrationT * 2.0f : 1.0f - (celebrationT - 0.50f) * 2.0f)
            : 0.0f;
        const core::Color heartColor = brightenedHeart(celebrationT, state.celebrationSeconds > 0.0f);
        const std::function<void(bool)> onChange = onChange_;
        const bool nextChecked = !checked_;
        const float outlineOpacity = checked_ ? 0.0f : 1.0f;
        const float fillOpacity = checked_ ? 1.0f : 0.0f;

        ui_.stack(id_)
            .size(w, h)
            .content([&] {
                ui_.rect(id_ + ".hit")
                    .size(w, h)
                    .states(transparent(), transparent(), transparent())
                    .radius(side * 0.50f)
                    .disabled(disabled_)
                    .transition(transition_)
                    .onClick([onChange, nextChecked] {
                        if (onChange) {
                            onChange(nextChecked);
                        }
                    })
                    .build();

                drawHeartSvg(id_ + ".outline", outlineSvg(), iconX, iconY, iconSize, style_.heart, outlineOpacity, 1.0f);
                drawHeartSvg(id_ + ".filled", filledSvg(), iconX, iconY, iconSize, heartColor, fillOpacity, std::max(0.001f, filledScale));

                drawCelebration(w, h, side, celebrateScale, std::clamp(celebrateOpacity, 0.0f, 1.0f));

                if (state.celebrationSeconds > 0.0f) {
                    ui_.stack(id_ + ".ticker")
                        .size(1.0f, 1.0f)
                        .onFrame([state = &state](float deltaSeconds) {
                            state->celebrationSeconds = std::max(0.0f, state->celebrationSeconds - deltaSeconds);
                        })
                        .build();
                }
            })
            .build();
    }

private:
    struct HeartSwitchState {
        bool initialized = false;
        bool lastChecked = false;
        float celebrationSeconds = 0.0f;
    };

    static constexpr float kCelebrationDuration = 0.50f;

    static core::Color transparent() {
        return {0.0f, 0.0f, 0.0f, 0.0f};
    }

    static float easeOut(float t) {
        const float clamped = std::clamp(t, 0.0f, 1.0f);
        return 1.0f - (1.0f - clamped) * (1.0f - clamped) * (1.0f - clamped);
    }

    static float fillScale(float t, bool celebrating) {
        if (!celebrating) {
            return 1.0f;
        }
        const float clamped = std::clamp(t, 0.0f, 1.0f);
        if (clamped < 0.25f) {
            return 1.20f * (clamped / 0.25f);
        }
        if (clamped < 0.50f) {
            return 1.20f - 0.20f * ((clamped - 0.25f) / 0.25f);
        }
        return 1.0f;
    }

    core::Color brightenedHeart(float t, bool celebrating) const {
        if (!celebrating || t < 0.25f || t > 0.55f) {
            return style_.heart;
        }
        const float amount = 0.28f * (1.0f - std::abs(t - 0.40f) / 0.15f);
        return core::mixColor(style_.heart, {1.0f, 1.0f, 1.0f, style_.heart.a}, std::clamp(amount, 0.0f, 0.28f));
    }

    static const char* outlineSvg() {
        return R"svg(<svg viewBox="0 0 24 24" xmlns="http://www.w3.org/2000/svg">
  <path fill="#ffffff" d="M17.5,1.917a6.4,6.4,0,0,0-5.5,3.3,6.4,6.4,0,0,0-5.5-3.3A6.8,6.8,0,0,0,0,8.967c0,4.547,4.786,9.513,8.8,12.88a4.974,4.974,0,0,0,6.4,0C19.214,18.48,24,13.514,24,8.967A6.8,6.8,0,0,0,17.5,1.917Zm-3.585,18.4a2.973,2.973,0,0,1-3.83,0C4.947,16.006,2,11.87,2,8.967a4.8,4.8,0,0,1,4.5-5.05A4.8,4.8,0,0,1,11,8.967a1,1,0,0,0,2,0,4.8,4.8,0,0,1,4.5-5.05A4.8,4.8,0,0,1,22,8.967C22,11.87,19.053,16.006,13.915,20.313Z"/>
</svg>
)svg";
    }

    static const char* filledSvg() {
        return R"svg(<svg viewBox="0 0 24 24" xmlns="http://www.w3.org/2000/svg">
  <path fill="#ffffff" d="M17.5,1.917a6.4,6.4,0,0,0-5.5,3.3,6.4,6.4,0,0,0-5.5-3.3A6.8,6.8,0,0,0,0,8.967c0,4.547,4.786,9.513,8.8,12.88a4.974,4.974,0,0,0,6.4,0C19.214,18.48,24,13.514,24,8.967A6.8,6.8,0,0,0,17.5,1.917Z"/>
</svg>
)svg";
    }

    void drawHeartSvg(const std::string& partId,
                      const char* source,
                      float x,
                      float y,
                      float size,
                      const core::Color& color,
                      float opacity,
                      float scale) {
        ui_.svg(partId)
            .source(source)
            .x(x)
            .y(y)
            .size(size, size)
            .tint(color)
            .contain()
            .opacity(opacity)
            .scale(scale)
            .transformOrigin(0.5f, 0.5f)
            .transition(transition_)
            .animate(core::AnimProperty::Opacity | core::AnimProperty::Transform)
            .build();
    }

    void drawCelebration(float width, float height, float side, float scale, float opacity) {
        const float cx = width * 0.5f;
        const float cy = height * 0.5f;
        const float radius = side * 0.48f * scale;
        const float lineLength = side * 0.20f;
        const float lineThickness = std::max(1.6f, side * 0.045f);
        constexpr std::array<float, 6> angles{-2.35f, 3.14f, 2.35f, -0.78f, 0.0f, 0.78f};

        for (std::size_t i = 0; i < angles.size(); ++i) {
            const float angle = angles[i];
            const float x = cx + std::cos(angle) * radius - lineLength * 0.5f;
            const float y = cy + std::sin(angle) * radius - lineThickness * 0.5f;
            ui_.rect(id_ + ".celebrate." + std::to_string(i))
                .x(x)
                .y(y)
                .size(lineLength, lineThickness)
                .color(theme::withAlpha(style_.heart, opacity))
                .radius(lineThickness * 0.5f)
                .rotate(angle)
                .transformOrigin(0.5f, 0.5f)
                .transition(transition_)
                .animate(core::AnimProperty::Color | core::AnimProperty::Transform)
                .disabled(true)
                .build();
        }
    }

    core::dsl::Ui& ui_;
    std::string id_;
    HeartSwitchStyle style_;
    core::Transition transition_ = core::Transition::make(0.18f, core::Ease::OutCubic);
    std::function<void(bool)> onChange_;
    bool checked_ = false;
    bool disabled_ = false;
    float width_ = 50.0f;
    float height_ = 50.0f;
};

inline HeartSwitchBuilder heartSwitch(core::dsl::Ui& ui, const std::string& id) {
    return HeartSwitchBuilder(ui, id);
}

} // namespace components::workshop
