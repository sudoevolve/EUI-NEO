#pragma once

#include "components/theme.h"
#include "core/dsl.h"
#include "eui/signal.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <functional>
#include <string>
#include <utility>

namespace components {

struct DatePickerStyle {
    DatePickerStyle() : DatePickerStyle(theme::dark()) {}

    explicit DatePickerStyle(const theme::ThemeColorTokens& tokens) {
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

class DatePickerBuilder {
    struct DateDraft;

public:
    DatePickerBuilder(core::dsl::Ui& ui, std::string id)
        : ui_(ui), id_(std::move(id)) {}

    DatePickerBuilder& open(bool value = true) { open_ = value; return *this; }
    DatePickerBuilder& bindOpen(eui::Signal<bool>& signal) {
        open(signal.get());
        onOpenChange([&signal](bool value) { signal.set(value); });
        return *this;
    }
    DatePickerBuilder& screen(float width, float height) { screenWidth_ = width; screenHeight_ = height; return *this; }
    DatePickerBuilder& size(float width, float height) { width_ = width; height_ = height; return *this; }
    DatePickerBuilder& date(int year, int month, int day) {
        year_ = std::clamp(year, 1900, 2200);
        month_ = std::clamp(month, 1, 12);
        day_ = std::clamp(day, 1, daysInMonth(year_, month_));
        return *this;
    }
    DatePickerBuilder& style(const DatePickerStyle& value) { style_ = value; return *this; }
    DatePickerBuilder& theme(const theme::ThemeColorTokens& tokens) { style_ = DatePickerStyle(tokens); return *this; }
    DatePickerBuilder& transition(const core::Transition& value) { transition_ = value; return *this; }
    DatePickerBuilder& zIndex(int value) { zIndex_ = value; return *this; }
    DatePickerBuilder& onChange(std::function<void(int, int, int)> callback) { onChange_ = std::move(callback); return *this; }
    DatePickerBuilder& onOpenChange(std::function<void(bool)> callback) { onOpenChange_ = std::move(callback); return *this; }

    void build() {
        const float panelWidth = std::min(width_, std::max(0.0f, screenWidth_ - 48.0f));
        const float panelHeight = std::min(height_, std::max(0.0f, screenHeight_ - 48.0f));
        const float panelX = std::max(24.0f, (screenWidth_ - panelWidth) * 0.5f);
        const float panelY = std::max(24.0f, (screenHeight_ - panelHeight) * 0.5f);
        const float visible = open_ ? 1.0f : 0.0f;
        const float panelScale = open_ ? 1.0f : 0.965f;
        const float panelOffsetY = open_ ? 0.0f : 14.0f;
        const std::function<void(bool)> onOpenChange = onOpenChange_;
        DateDraft* draft = &ui_.state<DateDraft>(id_ + ".draft");
        syncDraft(*draft, open_, year_, month_, day_);

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

    struct DateDraft {
        bool active = false;
        int year = 2026;
        int month = 1;
        int day = 1;
    };

    struct WheelItemVisual {
        float opacity = 1.0f;
        float translateY = 0.0f;
        float scaleX = 1.0f;
        float scaleY = 1.0f;
    };

    static bool leapYear(int year) {
        return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    }

    static int daysInMonth(int year, int month) {
        static constexpr std::array<int, 12> days{{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}};
        month = std::clamp(month, 1, 12);
        if (month == 2 && leapYear(year)) {
            return 29;
        }
        return days[static_cast<std::size_t>(month - 1)];
    }

    static void syncDraft(DateDraft& draft, bool open, int year, int month, int day) {
        if (!open || !draft.active) {
            draft.year = std::clamp(year, 1900, 2200);
            draft.month = std::clamp(month, 1, 12);
            draft.day = std::clamp(day, 1, daysInMonth(draft.year, draft.month));
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

    static std::string formatDate(int year, int month, int day) {
        char buffer[16] = {};
        std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", year, month, day);
        return std::string(buffer);
    }

    static const char* monthName(int month) {
        static constexpr std::array<const char*, 12> names{{
            "January", "February", "March", "April", "May", "June",
            "July", "August", "September", "October", "November", "December"
        }};
        return names[static_cast<std::size_t>(std::clamp(month, 1, 12) - 1)];
    }

    static int columnValue(int column, int year, int month, int day) {
        if (column == 0) {
            return month;
        }
        if (column == 1) {
            return day;
        }
        return year;
    }

    static std::string itemText(int column, int value, int offset, int year, int month) {
        if (column == 0) {
            return monthName(wrapValue(value + offset, 1, 12));
        }
        if (column == 1) {
            return std::to_string(wrapValue(value + offset, 1, daysInMonth(year, month)));
        }
        const int nextYear = value + offset;
        if (nextYear < 1900 || nextYear > 2200) {
            return std::string{};
        }
        return std::to_string(nextYear);
    }

    static void applyColumnValue(int column, int value, DateDraft* draft) {
        if (draft == nullptr) {
            return;
        }

        int nextYear = std::clamp(draft->year, 1900, 2200);
        int nextMonth = std::clamp(draft->month, 1, 12);
        int nextDay = std::clamp(draft->day, 1, daysInMonth(nextYear, nextMonth));
        if (column == 0) {
            nextMonth = wrapValue(value, 1, 12);
            nextDay = std::min(nextDay, daysInMonth(nextYear, nextMonth));
        } else if (column == 1) {
            nextDay = wrapValue(value, 1, daysInMonth(nextYear, nextMonth));
        } else {
            nextYear = std::clamp(value, 1900, 2200);
            nextDay = std::min(nextDay, daysInMonth(nextYear, nextMonth));
        }

        draft->year = nextYear;
        draft->month = nextMonth;
        draft->day = nextDay;
    }

    static int rowOffsetFromPointer(double pointerY, const core::Rect& bounds, float columnHeight, float rowHeight) {
        const float scale = columnHeight > 0.0f ? bounds.height / columnHeight : 1.0f;
        const float localY = static_cast<float>((pointerY - bounds.y) / std::max(0.001f, scale));
        return std::clamp(static_cast<int>(std::round((localY - columnHeight * 0.5f) / rowHeight)), -3, 3);
    }

    static WheelItemVisual wheelItemVisual(int offset, bool hidden) {
        if (hidden) {
            return {0.0f, 0.0f, 1.0f, 1.0f};
        }
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

    void panel(float width, float height, DateDraft* draft) {
        const float titleHeight = 58.0f;
        const float bottomPad = 24.0f;
        const float rowHeight = 38.0f;
        const float columnY = titleHeight + 8.0f;
        const float columnHeight = std::max(150.0f, height - titleHeight - bottomPad - 8.0f);
        const float gap = 12.0f;
        const float pad = 24.0f;
        const float monthWidth = std::max(118.0f, (width - pad * 2.0f - gap * 2.0f) * 0.44f);
        const float dayWidth = std::max(68.0f, (width - pad * 2.0f - gap * 2.0f) * 0.22f);
        const float yearWidth = std::max(86.0f, width - pad * 2.0f - gap * 2.0f - monthWidth - dayWidth);
        const std::function<void(bool)> onOpenChange = onOpenChange_;
        const std::function<void(int, int, int)> onChange = onChange_;
        const int committedYear = year_;
        const int committedMonth = month_;
        const int committedDay = day_;

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
            .text("Date")
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
            .onClick([draft, committedYear, committedMonth, committedDay, onChange, onOpenChange] {
                if (draft != nullptr && onChange &&
                    (draft->year != committedYear || draft->month != committedMonth || draft->day != committedDay)) {
                    onChange(draft->year, draft->month, draft->day);
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

        wheelColumn(0, pad, columnY, monthWidth, columnHeight, rowHeight, draft);
        wheelColumn(1, pad + monthWidth + gap, columnY, dayWidth, columnHeight, rowHeight, draft);
        wheelColumn(2, pad + monthWidth + gap + dayWidth + gap, columnY, yearWidth, columnHeight, rowHeight, draft);
    }

    void wheelColumn(int column, float x, float y, float width, float height, float rowHeight, DateDraft* draft) {
        const std::string columnId = id_ + ".column." + std::to_string(column);
        DragState* state = &ui_.state<DragState>(columnId + ".drag");
        const int year = draft != nullptr ? draft->year : year_;
        const int month = draft != nullptr ? draft->month : month_;
        const int day = draft != nullptr ? draft->day : day_;
        const int value = columnValue(column, year, month, day);

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
            .onPress([state, draft, column, height, rowHeight, value](const core::PointerEvent& event, const core::Rect& bounds) {
                state->bounds = bounds;
                state->startY = event.y;
                state->startValue = value;
                applyColumnValue(column, value + rowOffsetFromPointer(event.y, bounds, height, rowHeight), draft);
            })
            .onDrag([state, draft, column, height, rowHeight](const core::dsl::DragEvent& event) {
                const float scale = height > 0.0f ? state->bounds.height / height : 1.0f;
                const int delta = static_cast<int>(std::round((state->startY - event.y) / std::max(0.001f, scale) / rowHeight));
                applyColumnValue(column, state->startValue + delta, draft);
            })
            .onScroll([draft, column, value](const core::ScrollEvent& event) {
                if (std::fabs(event.y) > 0.001) {
                    const int currentValue = draft != nullptr ? columnValue(column, draft->year, draft->month, draft->day) : value;
                    applyColumnValue(column, currentValue + (event.y > 0.0 ? -1 : 1), draft);
                }
            })
            .build();

        for (int offset = -2; offset <= 2; ++offset) {
            const std::string text = itemText(column, value, offset, year, month);
            const bool active = offset == 0;
            const int distance = offset < 0 ? -offset : offset;
            const WheelItemVisual visual = wheelItemVisual(offset, text.empty());
            ui_.text(columnId + ".item." + std::to_string(offset + 2))
                .x(x + 4.0f)
                .y(y + height * 0.5f - rowHeight * 0.5f + static_cast<float>(offset) * rowHeight)
                .size(std::max(0.0f, width - 8.0f), rowHeight)
                .zIndex(10 - distance)
                .text(text)
                .fontSize(active ? 22.0f : 15.0f)
                .lineHeight(active ? 26.0f : 19.0f)
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
    DatePickerStyle style_;
    core::Transition transition_ = core::Transition::make(0.16f, core::Ease::OutCubic);
    std::function<void(int, int, int)> onChange_;
    std::function<void(bool)> onOpenChange_;
    int year_ = 2026;
    int month_ = 4;
    int day_ = 28;
    float screenWidth_ = 800.0f;
    float screenHeight_ = 600.0f;
    float width_ = 420.0f;
    float height_ = 270.0f;
    bool open_ = false;
    int zIndex_ = 1000;
};

inline DatePickerBuilder datePicker(core::dsl::Ui& ui, const std::string& id) {
    return DatePickerBuilder(ui, id);
}

} // namespace components
