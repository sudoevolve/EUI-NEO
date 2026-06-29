#pragma once

#include "components/theme.h"
#include "core/dsl.h"
#include "eui/signal.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <utility>

namespace components {

struct TimePickerStyle {
    TimePickerStyle() : TimePickerStyle(theme::dark()) {}

    explicit TimePickerStyle(const theme::ThemeColorTokens& tokens) {
        backdrop = theme::color(0.0f, 0.0f, 0.0f, tokens.dark ? 0.42f : 0.26f);
        surface = tokens.dark
            ? core::mixColor(tokens.surface, theme::color(0.0f, 0.0f, 0.0f), 0.14f)
            : tokens.surface;
        column = theme::withAlpha(tokens.text, tokens.dark ? 0.045f : 0.035f);
        selected = theme::withAlpha(tokens.primary, tokens.dark ? 0.18f : 0.12f);
        text = tokens.text;
        mutedText = theme::withOpacity(tokens.text, 0.58f);
        accent = tokens.primary;
        border = theme::withOpacity(tokens.border, 0.80f);
        shadow = theme::popupShadow(tokens);
    }

    core::Color backdrop;
    core::Color surface;
    core::Color column;
    core::Color selected;
    core::Color text;
    core::Color mutedText;
    core::Color accent;
    core::Color border;
    core::Shadow shadow;
    float radius = 16.0f;
};

class TimePickerBuilder {
    struct TimeDraft;

public:
    TimePickerBuilder(core::dsl::Ui& ui, std::string id)
        : ui_(ui), id_(std::move(id)) {}

    TimePickerBuilder& open(bool value = true) { open_ = value; return *this; }
    TimePickerBuilder& bindOpen(eui::Signal<bool>& signal) {
        open(signal.get());
        onOpenChange([&signal](bool value) { signal.set(value); });
        return *this;
    }
    TimePickerBuilder& screen(float width, float height) { screenWidth_ = width; screenHeight_ = height; return *this; }
    TimePickerBuilder& size(float width, float height) { width_ = width; height_ = height; return *this; }
    TimePickerBuilder& time(int hour, int minute) {
        hour_ = std::clamp(hour, 0, 23);
        minute_ = std::clamp(minute, 0, 59);
        return *this;
    }
    TimePickerBuilder& minuteStep(int value) { minuteStep_ = std::clamp(value, 1, 30); return *this; }
    TimePickerBuilder& style(const TimePickerStyle& value) { style_ = value; return *this; }
    TimePickerBuilder& theme(const theme::ThemeColorTokens& tokens) { style_ = TimePickerStyle(tokens); return *this; }
    TimePickerBuilder& transition(const core::Transition& value) { transition_ = value; return *this; }
    TimePickerBuilder& zIndex(int value) { zIndex_ = value; return *this; }
    TimePickerBuilder& onChange(std::function<void(int, int)> callback) { onChange_ = std::move(callback); return *this; }
    TimePickerBuilder& onOpenChange(std::function<void(bool)> callback) { onOpenChange_ = std::move(callback); return *this; }

    void build() {
        const float panelWidth = std::min(width_, std::max(0.0f, screenWidth_ - 48.0f));
        const float panelHeight = std::min(height_, std::max(0.0f, screenHeight_ - 48.0f));
        const float panelX = std::max(24.0f, (screenWidth_ - panelWidth) * 0.5f);
        const float panelY = std::max(24.0f, (screenHeight_ - panelHeight) * 0.5f);
        const float visible = open_ ? 1.0f : 0.0f;
        const float panelScale = open_ ? 1.0f : 0.965f;
        const float panelOffsetY = open_ ? 0.0f : 14.0f;
        const std::function<void(bool)> onOpenChange = onOpenChange_;
        TimeDraft* draft = &ui_.state<TimeDraft>(id_ + ".draft");
        syncDraft(*draft, open_, hour_, minute_);

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
    struct DragState {
        core::Rect bounds;
        double startY = 0.0;
        int startValue = 0;
    };

    struct TimeDraft {
        bool active = false;
        int hour = 0;
        int minute = 0;
    };

    struct WheelItemVisual {
        float opacity = 1.0f;
        float translateY = 0.0f;
        float scaleX = 1.0f;
        float scaleY = 1.0f;
    };

    static void syncDraft(TimeDraft& draft, bool open, int hour, int minute) {
        if (!open || !draft.active) {
            draft.hour = std::clamp(hour, 0, 23);
            draft.minute = std::clamp(minute, 0, 59);
            draft.active = open;
        }
    }

    static int wrapValue(int value, int minValue, int maxValue) {
        const int span = maxValue - minValue + 1;
        if (span <= 0) {
            return minValue;
        }
        int shifted = (value - minValue) % span;
        if (shifted < 0) {
            shifted += span;
        }
        return minValue + shifted;
    }

    static std::string twoDigits(int value) {
        char buffer[8] = {};
        std::snprintf(buffer, sizeof(buffer), "%02d", value);
        return std::string(buffer);
    }

    static std::string formatTime(int hour, int minute) {
        char buffer[8] = {};
        std::snprintf(buffer, sizeof(buffer), "%02d:%02d", hour, minute);
        return std::string(buffer);
    }

    static int resolvedMinuteStep(int step) {
        return std::clamp(step, 1, 30);
    }

    static int minuteCount(int step) {
        const int safeStep = resolvedMinuteStep(step);
        return std::max(1, (60 + safeStep - 1) / safeStep);
    }

    static int hour12(int hour) {
        const int value = hour % 12;
        return value == 0 ? 12 : value;
    }

    static bool pm(int hour) {
        return hour >= 12;
    }

    static int columnValue(int column, int hour, int minute, int step) {
        if (column == 0) {
            return hour12(hour);
        }
        if (column == 1) {
            return minute / resolvedMinuteStep(step);
        }
        return pm(hour) ? 1 : 0;
    }

    static std::string itemText(int column, int value, int offset, int step) {
        if (column == 0) {
            return std::to_string(wrapValue(value + offset, 1, 12));
        }
        if (column == 1) {
            const int index = wrapValue(value + offset, 0, minuteCount(step) - 1);
            return twoDigits(std::clamp(index * resolvedMinuteStep(step), 0, 59));
        }
        return wrapValue(value + offset, 0, 1) == 1 ? "PM" : "AM";
    }

    static void applyColumnValue(int column, int value, int step, TimeDraft* draft) {
        if (draft == nullptr) {
            return;
        }

        int nextHour = std::clamp(draft->hour, 0, 23);
        int nextMinute = std::clamp(draft->minute, 0, 59);
        if (column == 0) {
            const int nextHour12 = wrapValue(value, 1, 12);
            nextHour = pm(nextHour)
                ? (nextHour12 == 12 ? 12 : nextHour12 + 12)
                : (nextHour12 == 12 ? 0 : nextHour12);
        } else if (column == 1) {
            const int index = wrapValue(value, 0, minuteCount(step) - 1);
            nextMinute = std::clamp(index * resolvedMinuteStep(step), 0, 59);
        } else {
            const bool nextPm = wrapValue(value, 0, 1) == 1;
            const int currentHour12 = hour12(nextHour);
            nextHour = nextPm
                ? (currentHour12 == 12 ? 12 : currentHour12 + 12)
                : (currentHour12 == 12 ? 0 : currentHour12);
        }

        draft->hour = nextHour;
        draft->minute = nextMinute;
    }

    static int rowOffsetFromPointer(double pointerY, const core::Rect& bounds, float columnHeight, float rowHeight) {
        const float scale = columnHeight > 0.0f ? bounds.height / columnHeight : 1.0f;
        const float localY = static_cast<float>((pointerY - bounds.y) / std::max(0.001f, scale));
        return std::clamp(static_cast<int>(std::round((localY - columnHeight * 0.5f) / rowHeight)), -3, 3);
    }

    static WheelItemVisual wheelItemVisual(int offset) {
        const int distance = offset < 0 ? -offset : offset;
        const float distanceAmount = static_cast<float>(distance);
        const float direction = offset < 0 ? -1.0f : (offset > 0 ? 1.0f : 0.0f);
        return {
            std::clamp(1.0f - distanceAmount * 0.25f, 0.48f, 1.0f),
            -direction * distanceAmount * distanceAmount * 2.4f,
            std::clamp(1.0f - distanceAmount * 0.015f, 0.95f, 1.0f),
            std::clamp(1.0f - distanceAmount * 0.120f, 0.72f, 1.0f)
        };
    }

    void panel(float width, float height, TimeDraft* draft) {
        const float titleHeight = 58.0f;
        const float bottomPad = 24.0f;
        const float rowHeight = 38.0f;
        const float columnY = titleHeight + 8.0f;
        const float columnHeight = std::max(150.0f, height - titleHeight - bottomPad - 8.0f);
        const float gap = 12.0f;
        const float pad = 24.0f;
        const float columnWidth = std::max(1.0f, (width - pad * 2.0f - gap * 2.0f) / 3.0f);
        const std::function<void(bool)> onOpenChange = onOpenChange_;
        const std::function<void(int, int)> onChange = onChange_;
        const int committedHour = hour_;
        const int committedMinute = minute_;

        ui_.rect(id_ + ".panel.bg")
            .size(width, height)
            .color(style_.surface)
            .radius(style_.radius)
            .border(1.0f, style_.border)
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
            .x(24.0f)
            .y(18.0f)
            .size(std::max(0.0f, width - 124.0f), 30.0f)
            .text("Time")
            .fontSize(24.0f)
            .lineHeight(29.0f)
            .color(style_.text)
            .build();

        ui_.rect(id_ + ".done.bg")
            .x(std::max(0.0f, width - 86.0f))
            .y(18.0f)
            .size(62.0f, 30.0f)
            .states(style_.accent,
                    core::mixColor(style_.accent, theme::color(1.0f, 1.0f, 1.0f), 0.12f),
                    core::mixColor(style_.accent, theme::color(0.0f, 0.0f, 0.0f), 0.14f))
            .radius(15.0f)
            .disabled(!open_)
            .onClick([draft, committedHour, committedMinute, onChange, onOpenChange] {
                if (draft != nullptr && onChange && (draft->hour != committedHour || draft->minute != committedMinute)) {
                    onChange(draft->hour, draft->minute);
                }
                if (onOpenChange) {
                    onOpenChange(false);
                }
            })
            .build();

        ui_.text(id_ + ".done.text")
            .x(std::max(0.0f, width - 86.0f))
            .y(18.0f)
            .size(62.0f, 30.0f)
            .text("Done")
            .fontSize(13.0f)
            .lineHeight(16.0f)
            .color(theme::color(1.0f, 1.0f, 1.0f))
            .horizontalAlign(core::HorizontalAlign::Center)
            .verticalAlign(core::VerticalAlign::Center)
            .build();

        for (int column = 0; column < 3; ++column) {
            const float x = pad + static_cast<float>(column) * (columnWidth + gap);
            wheelColumn(column, x, columnY, columnWidth, columnHeight, rowHeight, draft);
        }
    }

    void wheelColumn(int column, float x, float y, float width, float height, float rowHeight, TimeDraft* draft) {
        const std::string columnId = id_ + ".column." + std::to_string(column);
        DragState* state = &ui_.state<DragState>(columnId + ".drag");
        const int hour = draft != nullptr ? draft->hour : hour_;
        const int minute = draft != nullptr ? draft->minute : minute_;
        const int step = minuteStep_;
        const int value = columnValue(column, hour, minute, step);

        ui_.rect(columnId + ".bg")
            .x(x)
            .y(y)
            .size(width, height)
            .color(style_.column)
            .radius(style_.radius)
            .build();

        ui_.rect(columnId + ".selected")
            .x(x + 7.0f)
            .y(y + height * 0.5f - rowHeight * 0.5f)
            .size(std::max(0.0f, width - 14.0f), rowHeight)
            .color(style_.selected)
            .radius(11.0f)
            .build();

        ui_.rect(columnId + ".hit")
            .x(x)
            .y(y)
            .size(width, height)
            .states(theme::color(0.0f, 0.0f, 0.0f, 0.0f),
                    theme::color(0.0f, 0.0f, 0.0f, 0.0f),
                    theme::color(0.0f, 0.0f, 0.0f, 0.0f))
            .disabled(!open_)
            .onPress([state, draft, column, height, rowHeight, step, value](const core::PointerEvent& event, const core::Rect& bounds) {
                state->bounds = bounds;
                state->startY = event.y;
                state->startValue = value;
                applyColumnValue(column, value + rowOffsetFromPointer(event.y, bounds, height, rowHeight), step, draft);
            })
            .onDrag([state, draft, column, height, rowHeight, step](const core::dsl::DragEvent& event) {
                const float scale = height > 0.0f ? state->bounds.height / height : 1.0f;
                const int delta = static_cast<int>(std::round((state->startY - event.y) / std::max(0.001f, scale) / rowHeight));
                applyColumnValue(column, state->startValue + delta, step, draft);
            })
            .onScroll([draft, column, step, value](const core::ScrollEvent& event) {
                if (std::fabs(event.y) > 0.001) {
                    const int currentValue = draft != nullptr ? columnValue(column, draft->hour, draft->minute, step) : value;
                    applyColumnValue(column, currentValue + (event.y > 0.0 ? -1 : 1), step, draft);
                }
            })
            .build();

        for (int offset = -2; offset <= 2; ++offset) {
            const bool active = offset == 0;
            const int distance = offset < 0 ? -offset : offset;
            const WheelItemVisual visual = wheelItemVisual(offset);
            ui_.text(columnId + ".item." + std::to_string(offset + 2))
                .x(x + 4.0f)
                .y(y + height * 0.5f - rowHeight * 0.5f + static_cast<float>(offset) * rowHeight)
                .size(std::max(0.0f, width - 8.0f), rowHeight)
                .zIndex(10 - distance)
                .text(itemText(column, value, offset, step))
                .fontSize(active ? 24.0f : 16.0f)
                .lineHeight(active ? 28.0f : 20.0f)
                .color(active ? style_.text : style_.mutedText)
                .opacity(visual.opacity)
                .translateY(visual.translateY)
                .scale(visual.scaleX, visual.scaleY)
                .transformOrigin(0.5f, 0.5f)
                .horizontalAlign(core::HorizontalAlign::Center)
                .verticalAlign(core::VerticalAlign::Center)
                .transition(transition_)
                .animate(core::AnimProperty::TextColor | core::AnimProperty::Opacity | core::AnimProperty::Transform)
                .build();
        }
    }

    core::dsl::Ui& ui_;
    std::string id_;
    TimePickerStyle style_;
    core::Transition transition_ = core::Transition::make(0.16f, core::Ease::OutCubic);
    std::function<void(int, int)> onChange_;
    std::function<void(bool)> onOpenChange_;
    int hour_ = 9;
    int minute_ = 30;
    int minuteStep_ = 1;
    float screenWidth_ = 800.0f;
    float screenHeight_ = 600.0f;
    float width_ = 330.0f;
    float height_ = 264.0f;
    bool open_ = false;
    int zIndex_ = 1000;
};

inline TimePickerBuilder timePicker(core::dsl::Ui& ui, const std::string& id) {
    return TimePickerBuilder(ui, id);
}

} // namespace components
