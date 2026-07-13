#pragma once

#include "components/slider.h"
#include "components/theme.h"
#include "core/dsl.h"
#include "eui/signal.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace components {

struct ColorPickerStyle {
    ColorPickerStyle() : ColorPickerStyle(theme::dark()) {}

    explicit ColorPickerStyle(const theme::ThemeColorTokens& tokens) {
        backdrop = theme::color(0.0f, 0.0f, 0.0f, tokens.dark ? 0.42f : 0.26f);
        surface = tokens.dark
            ? core::mixColor(tokens.surface, theme::color(0.0f, 0.0f, 0.0f), 0.14f)
            : tokens.surface;
        track = theme::withAlpha(tokens.text, tokens.dark ? 0.12f : 0.10f);
        text = tokens.text;
        mutedText = theme::withOpacity(tokens.text, 0.62f);
        accent = tokens.primary;
        border = theme::withOpacity(tokens.border, 0.80f);
        knob = tokens.dark ? theme::color(0.96f, 0.98f, 1.0f) : theme::color(1.0f, 1.0f, 1.0f);
        shadow = theme::popupShadow(tokens);
        radius = tokens.metrics.radius.overlay;
    }

    core::Color backdrop;
    core::Color surface;
    core::Color track;
    core::Color text;
    core::Color mutedText;
    core::Color accent;
    core::Color border;
    core::Color knob;
    core::Shadow shadow;
    float radius = 16.0f;
};

class ColorPickerBuilder {
    struct ColorDraft;

public:
    ColorPickerBuilder(core::dsl::Ui& ui, std::string id)
        : ui_(ui), id_(std::move(id)) {}

    ColorPickerBuilder& open(bool value = true) { open_ = value; return *this; }
    ColorPickerBuilder& bindOpen(eui::Signal<bool>& signal) {
        open(signal.get());
        onOpenChange([&signal](bool value) { signal.set(value); });
        return *this;
    }
    ColorPickerBuilder& screen(float width, float height) { screenWidth_ = width; screenHeight_ = height; return *this; }
    ColorPickerBuilder& size(float width, float height) { width_ = width; height_ = height; return *this; }
    ColorPickerBuilder& value(core::Color value) { value_ = clampColor(value); return *this; }
    ColorPickerBuilder& bind(eui::Signal<core::Color>& signal) {
        value(signal.get());
        onChange([&signal](core::Color value) { signal.set(value); });
        return *this;
    }
    ColorPickerBuilder& colors(std::vector<core::Color> value) { colors_ = std::move(value); return *this; }
    ColorPickerBuilder& style(const ColorPickerStyle& value) { style_ = value; return *this; }
    ColorPickerBuilder& theme(const theme::ThemeColorTokens& tokens) {
        style_ = ColorPickerStyle(tokens);
        metrics_ = tokens.metrics;
        tokens_ = tokens;
        return *this;
    }
    ColorPickerBuilder& transition(const core::Transition& value) { transition_ = value; return *this; }
    ColorPickerBuilder& zIndex(int value) { zIndex_ = value; return *this; }
    ColorPickerBuilder& onChange(std::function<void(core::Color)> callback) { onChange_ = std::move(callback); return *this; }
    ColorPickerBuilder& onOpenChange(std::function<void(bool)> callback) { onOpenChange_ = std::move(callback); return *this; }

    void build() {
        const float panelWidth = std::min(width_, std::max(0.0f, screenWidth_ - metrics_.spacing.overlay));
        const float panelHeight = std::min(height_, std::max(0.0f, screenHeight_ - metrics_.spacing.overlay));
        const float panelX = std::max(metrics_.spacing.panel, (screenWidth_ - panelWidth) * 0.5f);
        const float panelY = std::max(metrics_.spacing.panel, (screenHeight_ - panelHeight) * 0.5f);
        const float visible = open_ ? 1.0f : 0.0f;
        const float panelScale = open_ ? 1.0f : 0.965f;
        const float panelOffsetY = open_ ? 0.0f : 14.0f;
        const std::function<void(bool)> onOpenChange = onOpenChange_;
        ColorDraft* draft = &ui_.state<ColorDraft>(id_ + ".draft");
        syncDraft(*draft, open_, value_);

        ui_.stack(id_)
            .size(screenWidth_, screenHeight_)
            .zIndex(zIndex_)
            .content([&] {
                ui_.rect(id_ + ".backdrop")
                    .size(screenWidth_, screenHeight_)
                    .states(style_.backdrop, style_.backdrop, style_.backdrop)
                    .opacity(visible)
                    .transition(transition_)
                    .animate(core::AnimProperty::Opacity)
                    .disabled(!open_)
                    .onClick([onOpenChange] {
                        if (onOpenChange) {
                            onOpenChange(false);
                        }
                    })
                    .onScroll([](const core::ScrollEvent&) {})
                    .build();

                ui_.stack(id_ + ".panel")
                    .x(panelX)
                    .y(panelY)
                    .size(panelWidth, panelHeight)
                    .opacity(visible)
                    .translateY(panelOffsetY)
                    .scale(panelScale)
                    .transformOrigin(0.5f, 0.5f)
                    .transition(transition_)
                    .animate(core::AnimProperty::Opacity | core::AnimProperty::Transform)
                    .content([&, draft] {
                        panel(panelWidth, panelHeight, draft);
                    })
                    .build();
            })
            .build();
    }

private:
    struct ColorDraft {
        bool active = false;
        core::Color value = theme::color(0.22f, 0.50f, 0.88f);
    };

    static void syncDraft(ColorDraft& draft, bool open, core::Color value) {
        if (!open || !draft.active) {
            draft.value = clampColor(value);
            draft.active = open;
        }
    }

    static core::Color clampColor(core::Color value) {
        value.r = std::clamp(value.r, 0.0f, 1.0f);
        value.g = std::clamp(value.g, 0.0f, 1.0f);
        value.b = std::clamp(value.b, 0.0f, 1.0f);
        value.a = 1.0f;
        return value;
    }

    static bool sameColor(core::Color a, core::Color b) {
        return std::fabs(a.r - b.r) < 0.001f &&
               std::fabs(a.g - b.g) < 0.001f &&
               std::fabs(a.b - b.b) < 0.001f;
    }

    static int channelToInt(float value) {
        return std::clamp(static_cast<int>(std::round(std::clamp(value, 0.0f, 1.0f) * 255.0f)), 0, 255);
    }

    static std::string formatHex(core::Color color) {
        char buffer[10] = {};
        std::snprintf(buffer, sizeof(buffer), "#%02X%02X%02X",
                      channelToInt(color.r), channelToInt(color.g), channelToInt(color.b));
        return std::string(buffer);
    }

    static float channelValue(core::Color color, int channel) {
        if (channel == 0) {
            return color.r;
        }
        if (channel == 1) {
            return color.g;
        }
        return color.b;
    }

    static core::Color withChannel(core::Color color, int channel, float nextValue) {
        nextValue = std::clamp(nextValue, 0.0f, 1.0f);
        if (channel == 0) {
            color.r = nextValue;
        } else if (channel == 1) {
            color.g = nextValue;
        } else {
            color.b = nextValue;
        }
        return clampColor(color);
    }

    static void emitColor(core::Color current,
                          core::Color next,
                          const std::function<void(core::Color)>& onChange) {
        next = clampColor(next);
        if (onChange && !sameColor(current, next)) {
            onChange(next);
        }
    }

    std::vector<core::Color> palette() const {
        if (!colors_.empty()) {
            std::vector<core::Color> result;
            result.reserve(colors_.size());
            for (core::Color color : colors_) {
                result.push_back(clampColor(color));
            }
            return result;
        }
        return {
            style_.accent,
            theme::color(0.20f, 0.50f, 0.90f),
            theme::color(0.12f, 0.72f, 0.78f),
            theme::color(0.15f, 0.78f, 0.48f),
            theme::color(0.96f, 0.68f, 0.18f),
            theme::color(0.92f, 0.28f, 0.46f),
            theme::color(0.56f, 0.36f, 0.96f),
            theme::color(0.88f, 0.18f, 0.24f)
        };
    }

    void panel(float width, float height, ColorDraft* draft) {
        const float pad = metrics_.spacing.panel;
        const float titleHeight = metrics_.spacing.header * 2.0f;
        const float previewY = titleHeight;
        const float previewHeight = metrics_.control.compact * 2.0f + metrics_.spacing.micro;
        const float slidersY = previewY + previewHeight + metrics_.typography.control;
        const float swatchesY = height - metrics_.spacing.overlay;
        const float sliderAreaHeight = std::max(
            0.0f, swatchesY - slidersY - metrics_.spacing.compact);
        const float sliderRowHeight = std::min(
            metrics_.control.menuItem, sliderAreaHeight / 3.0f);
        const float sliderReservedWidth = metrics_.spacing.header * 3.0f;
        const float sliderWidth = std::max(sliderReservedWidth, width - pad * 2.0f - sliderReservedWidth);
        const std::function<void(bool)> onOpenChange = onOpenChange_;
        const std::function<void(core::Color)> onChange = onChange_;
        const core::Color committed = value_;
        const core::Color current = draft != nullptr ? draft->value : value_;

        ui_.rect(id_ + ".panel.bg")
            .size(width, height)
            .color(style_.surface)
            .radius(style_.radius)
            .border(metrics_.spacing.hairline, style_.border)
            .shadow(style_.shadow)
            .build();

        ui_.rect(id_ + ".panel.hit")
            .size(width, height)
            .states(theme::color(0.0f, 0.0f, 0.0f, 0.0f),
                    theme::color(0.0f, 0.0f, 0.0f, 0.0f),
                    theme::color(0.0f, 0.0f, 0.0f, 0.0f))
            .disabled(!open_)
            .onClick([] {})
            .onScroll([](const core::ScrollEvent&) {})
            .build();

        ui_.text(id_ + ".title")
            .x(metrics_.spacing.panel)
            .y(metrics_.typography.control)
            .size(std::max(0.0f, width - metrics_.spacing.overlay * 2.0f - metrics_.control.compact),
                  metrics_.spacing.header)
            .text("Color")
            .fontSize(metrics_.typography.heading)
            .lineHeight(metrics_.typography.heading + metrics_.typography.lineGapRelaxed)
            .color(style_.text)
            .build();

        ui_.rect(id_ + ".done.bg")
            .x(std::max(0.0f, width - metrics_.spacing.overlay - metrics_.control.segmented))
            .y(metrics_.typography.control)
            .size(metrics_.spacing.overlay + metrics_.spacing.content, metrics_.spacing.header)
            .states(style_.accent,
                    core::mixColor(style_.accent, theme::color(1.0f, 1.0f, 1.0f), 0.12f),
                    core::mixColor(style_.accent, theme::color(0.0f, 0.0f, 0.0f), 0.14f))
            .radius(metrics_.spacing.header * 0.5f)
            .disabled(!open_)
            .onClick([draft, committed, onChange, onOpenChange] {
                if (draft != nullptr) {
                    emitColor(committed, draft->value, onChange);
                }
                if (onOpenChange) {
                    onOpenChange(false);
                }
            })
            .build();

        ui_.text(id_ + ".done.text")
            .x(std::max(0.0f, width - metrics_.spacing.overlay - metrics_.control.segmented))
            .y(metrics_.typography.control)
            .size(metrics_.spacing.overlay + metrics_.spacing.content, metrics_.spacing.header)
            .text("Done")
            .fontSize(metrics_.typography.option)
            .lineHeight(metrics_.typography.option + metrics_.typography.lineGapTight)
            .color(theme::color(1.0f, 1.0f, 1.0f))
            .horizontalAlign(core::HorizontalAlign::Center)
            .verticalAlign(core::VerticalAlign::Center)
            .build();

        ui_.rect(id_ + ".preview")
            .x(pad)
            .y(previewY)
            .size(std::max(0.0f, width - pad * 2.0f), previewHeight)
            .color(current)
            .radius(metrics_.radius.overlay)
            .shadow(18.0f, 0.0f, 6.0f, theme::withAlpha(current, 0.24f))
            .transition(transition_)
            .animate(core::AnimProperty::Color | core::AnimProperty::Shadow)
            .build();

        ui_.text(id_ + ".preview.hex")
            .x(pad)
            .y(previewY)
            .size(std::max(0.0f, width - pad * 2.0f - metrics_.spacing.section), previewHeight)
            .text(formatHex(current))
            .fontSize(metrics_.typography.input)
            .lineHeight(metrics_.typography.input + metrics_.typography.lineGapRelaxed)
            .color(theme::color(1.0f, 1.0f, 1.0f, 0.94f))
            .horizontalAlign(core::HorizontalAlign::Right)
            .verticalAlign(core::VerticalAlign::Center)
            .build();

        if (open_) {
            channelSlider(0, "R", theme::color(0.92f, 0.20f, 0.22f), pad, slidersY, sliderWidth, sliderRowHeight, current, draft);
            channelSlider(1, "G", theme::color(0.15f, 0.74f, 0.40f), pad, slidersY + sliderRowHeight, sliderWidth, sliderRowHeight, current, draft);
            channelSlider(2, "B", theme::color(0.20f, 0.46f, 0.92f), pad, slidersY + sliderRowHeight * 2.0f, sliderWidth, sliderRowHeight, current, draft);

            const std::vector<core::Color> swatches = palette();
            const float swatchSize = metrics_.control.switchHeight;
            const float swatchGap = metrics_.spacing.compact;
            for (int index = 0; index < static_cast<int>(swatches.size()); ++index) {
                const float swatchX = pad + static_cast<float>(index) * (swatchSize + swatchGap);
                if (swatchX + swatchSize > width - pad) {
                    break;
                }
                const core::Color swatch = swatches[static_cast<std::size_t>(index)];
                ui_.rect(id_ + ".swatch.border." + std::to_string(index))
                    .x(swatchX - (metrics_.spacing.tiny - metrics_.spacing.hairline))
                    .y(swatchesY - (metrics_.spacing.tiny - metrics_.spacing.hairline))
                    .size(swatchSize + metrics_.spacing.small, swatchSize + metrics_.spacing.small)
                    .color(sameColor(swatch, current) ? style_.accent : theme::withAlpha(style_.text, 0.08f))
                    .radius(metrics_.radius.popup)
                    .transition(transition_)
                    .animate(core::AnimProperty::Color)
                    .build();

                ui_.rect(id_ + ".swatch." + std::to_string(index))
                    .x(swatchX)
                    .y(swatchesY)
                    .size(swatchSize, swatchSize)
                    .states(swatch,
                            core::mixColor(swatch, theme::color(1.0f, 1.0f, 1.0f), 0.16f),
                            core::mixColor(swatch, theme::color(0.0f, 0.0f, 0.0f), 0.14f))
                    .radius(metrics_.radius.control)
                    .onClick([draft, swatch] {
                        if (draft != nullptr) {
                            draft->value = clampColor(swatch);
                        }
                    })
                    .build();
            }
        }
    }

    void channelSlider(int channel,
                       const std::string& label,
                       const core::Color& fill,
                       float x,
                       float y,
                       float sliderWidth,
                       float rowHeight,
                       core::Color current,
                       ColorDraft* draft) {
        const float sliderHeight = std::min(
            metrics_.control.indicator, std::max(0.0f, rowHeight));
        const float sliderOffsetY = std::max(0.0f, (rowHeight - sliderHeight) * 0.5f);
        ui_.text(id_ + ".slider.label." + std::to_string(channel))
            .x(x)
            .y(y)
            .size(metrics_.control.indicator + metrics_.spacing.micro, rowHeight)
            .text(label)
            .fontSize(metrics_.typography.label)
            .lineHeight(metrics_.typography.label + metrics_.typography.lineGap)
            .color(style_.text)
            .verticalAlign(core::VerticalAlign::Center)
            .build();

        ui_.stack(id_ + ".slider.wrap." + std::to_string(channel))
            .x(x + metrics_.spacing.header + metrics_.spacing.micro)
            .y(y + sliderOffsetY)
            .size(sliderWidth, sliderHeight)
            .content([&] {
                SliderStyle sliderStyle;
                sliderStyle.track = style_.track;
                sliderStyle.fill = fill;
                sliderStyle.knob = style_.knob;
                components::slider(ui_, id_ + ".slider." + std::to_string(channel))
                    .theme(tokens_)
                    .size(sliderWidth, sliderHeight)
                    .value(channelValue(current, channel))
                    .style(sliderStyle)
                    .transition(transition_)
                    .onChange([draft, channel](float value) {
                        if (draft != nullptr) {
                            draft->value = withChannel(draft->value, channel, value);
                        }
                    })
                    .build();
            })
            .build();

        ui_.text(id_ + ".slider.value." + std::to_string(channel))
            .x(x + metrics_.control.control + sliderWidth)
            .y(y)
            .size(metrics_.control.input, rowHeight)
            .text(std::to_string(channelToInt(channelValue(current, channel))))
            .fontSize(metrics_.typography.caption)
            .lineHeight(metrics_.typography.caption + metrics_.typography.lineGap)
            .color(style_.mutedText)
            .verticalAlign(core::VerticalAlign::Center)
            .build();
    }

    core::dsl::Ui& ui_;
    std::string id_;
    ColorPickerStyle style_;
    theme::ThemeMetricTokens metrics_;
    theme::ThemeColorTokens tokens_ = theme::dark();
    core::Transition transition_ = core::Transition::make(0.16f, core::Ease::OutCubic);
    std::function<void(core::Color)> onChange_;
    std::function<void(bool)> onOpenChange_;
    std::vector<core::Color> colors_;
    core::Color value_ = theme::color(0.22f, 0.50f, 0.88f);
    float screenWidth_ = 800.0f;
    float screenHeight_ = 600.0f;
    float width_ = 420.0f;
    float height_ = 320.0f;
    bool open_ = false;
    int zIndex_ = 1000;
};

inline ColorPickerBuilder colorPicker(core::dsl::Ui& ui, const std::string& id) {
    return ColorPickerBuilder(ui, id);
}

} // namespace components
