#pragma once

#include "components/theme.h"
#include "core/dsl.h"
#include "eui/signal.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <string>
#include <utility>

namespace components {

struct StepperStyle {
    StepperStyle() : StepperStyle(theme::dark()) {}

    explicit StepperStyle(const theme::ThemeColorTokens& tokens) {
        button = tokens.surface;
        buttonHover = tokens.surfaceHover;
        buttonPressed = tokens.surfaceActive;
        buttonDisabled = core::mixColor(tokens.surface, tokens.background, tokens.dark ? 0.34f : 0.22f);
        field = core::mixColor(tokens.surface, tokens.background, tokens.dark ? 0.12f : 0.04f);
        fieldBorder = theme::withOpacity(tokens.border, 0.82f);
        text = tokens.text;
        mutedText = theme::withOpacity(tokens.text, 0.42f);
        accent = tokens.primary;
        shadow = theme::buttonShadow(tokens);
    }

    core::Color button;
    core::Color buttonHover;
    core::Color buttonPressed;
    core::Color buttonDisabled;
    core::Color field;
    core::Color fieldBorder;
    core::Color text;
    core::Color mutedText;
    core::Color accent;
    core::Shadow shadow;
    float radius = 12.0f;
};

class StepperBuilder {
public:
    StepperBuilder(core::dsl::Ui& ui, std::string id)
        : ui_(ui), id_(std::move(id)) {}

    StepperBuilder& size(float width, float height) { width_ = width; height_ = height; return *this; }
    StepperBuilder& value(long long value) { value_ = value; return *this; }
    StepperBuilder& bind(eui::Signal<long long>& signal) {
        value(signal.get());
        onChange([&signal](long long value) { signal.set(value); });
        return *this;
    }
    StepperBuilder& step(long long value) { step_ = value; return *this; }
    StepperBuilder& min(long long value) { min_ = value; hasMin_ = true; return *this; }
    StepperBuilder& max(long long value) { max_ = value; hasMax_ = true; return *this; }
    StepperBuilder& base(int value) { base_ = value; return *this; }
    StepperBuilder& digits(int value) { digits_ = std::max(0, value); return *this; }
    StepperBuilder& bitWidth(int value) { bitWidth_ = std::max(0, value); return *this; }
    StepperBuilder& showBasePrefix(bool value = true) { showBasePrefix_ = value; return *this; }
    StepperBuilder& prefix(std::string value) { prefix_ = std::move(value); return *this; }
    StepperBuilder& uppercase(bool value = true) { uppercase_ = value; return *this; }
    StepperBuilder& fontSize(float value) { fontSize_ = std::max(1.0f, value); return *this; }
    StepperBuilder& style(const StepperStyle& value) { style_ = value; return *this; }
    StepperBuilder& theme(const theme::ThemeColorTokens& tokens) { style_ = StepperStyle(tokens); return *this; }
    StepperBuilder& transition(const core::Transition& value) { transition_ = value; return *this; }
    StepperBuilder& transition(float duration, core::Ease ease = core::Ease::OutCubic) {
        transition_ = core::Transition::make(duration, ease);
        return *this;
    }
    StepperBuilder& onChange(std::function<void(long long)> callback) { onChange_ = std::move(callback); return *this; }

    void build() {
        const int base = clampBase(base_);
        const int bitWidth = std::max(0, bitWidth_);
        const long long minValue = resolvedMin(bitWidth);
        const long long maxValue = resolvedMax(bitWidth);
        const long long currentValue = std::clamp(value_, minValue, maxValue);
        const long long stepValue = std::max(1LL, step_);
        const long long previousValue = decrementValue(currentValue, stepValue, minValue);
        const long long nextValue = incrementValue(currentValue, stepValue, maxValue);
        const bool canDecrease = previousValue != currentValue;
        const bool canIncrease = nextValue != currentValue;
        const float buttonWidth = std::max(28.0f, height_);
        const float gap = 8.0f;
        const float displayWidth = std::max(36.0f, width_ - buttonWidth * 2.0f - gap * 2.0f);
        const float displayX = buttonWidth + gap;
        const float valueFontSize = std::min(fontSize_, height_ * 0.48f);
        const float controlFontSize = std::max(13.0f, height_ * 0.42f);
        const std::string valueText = formatValue(currentValue, base, effectiveDigits(base, bitWidth));
        const std::function<void(long long)> onChange = onChange_;

        ui_.stack(id_)
            .size(width_, height_)
            .content([&] {
                ui_.rect(id_ + ".minus.bg")
                    .size(buttonWidth, height_)
                    .states(canDecrease ? style_.button : style_.buttonDisabled,
                            canDecrease ? style_.buttonHover : style_.buttonDisabled,
                            canDecrease ? style_.buttonPressed : style_.buttonDisabled)
                    .radius(style_.radius)
                    .border(1.0f, style_.fieldBorder)
                    .shadow(canDecrease ? style_.shadow : core::Shadow{})
                    .transition(transition_)
                    .animate(core::AnimProperty::Color | core::AnimProperty::Border | core::AnimProperty::Shadow)
                    .disabled(!canDecrease)
                    .onClick([onChange, previousValue] {
                        if (onChange) {
                            onChange(previousValue);
                        }
                    })
                    .build();

                ui_.text(id_ + ".minus.text")
                    .size(buttonWidth, height_)
                    .text("-")
                    .fontSize(controlFontSize)
                    .lineHeight(controlFontSize)
                    .color(canDecrease ? style_.text : style_.mutedText)
                    .horizontalAlign(core::HorizontalAlign::Center)
                    .verticalAlign(core::VerticalAlign::Center)
                    .transition(transition_)
                    .animate(core::AnimProperty::TextColor)
                    .build();

                ui_.rect(id_ + ".value.bg")
                    .x(displayX)
                    .size(displayWidth, height_)
                    .color(style_.field)
                    .radius(style_.radius)
                    .border(1.0f, style_.fieldBorder)
                    .transition(transition_)
                    .animate(core::AnimProperty::Color | core::AnimProperty::Border)
                    .build();

                ui_.text(id_ + ".value.text")
                    .x(displayX + 10.0f)
                    .size(std::max(0.0f, displayWidth - 20.0f), height_)
                    .text(valueText)
                    .fontSize(valueFontSize)
                    .lineHeight(valueFontSize)
                    .color(style_.accent)
                    .horizontalAlign(core::HorizontalAlign::Center)
                    .verticalAlign(core::VerticalAlign::Center)
                    .transition(transition_)
                    .animate(core::AnimProperty::TextColor)
                    .build();

                ui_.rect(id_ + ".plus.bg")
                    .x(displayX + displayWidth + gap)
                    .size(buttonWidth, height_)
                    .states(canIncrease ? style_.button : style_.buttonDisabled,
                            canIncrease ? style_.buttonHover : style_.buttonDisabled,
                            canIncrease ? style_.buttonPressed : style_.buttonDisabled)
                    .radius(style_.radius)
                    .border(1.0f, style_.fieldBorder)
                    .shadow(canIncrease ? style_.shadow : core::Shadow{})
                    .transition(transition_)
                    .animate(core::AnimProperty::Color | core::AnimProperty::Border | core::AnimProperty::Shadow)
                    .disabled(!canIncrease)
                    .onClick([onChange, nextValue] {
                        if (onChange) {
                            onChange(nextValue);
                        }
                    })
                    .build();

                ui_.text(id_ + ".plus.text")
                    .x(displayX + displayWidth + gap)
                    .size(buttonWidth, height_)
                    .text("+")
                    .fontSize(controlFontSize)
                    .lineHeight(controlFontSize)
                    .color(canIncrease ? style_.text : style_.mutedText)
                    .horizontalAlign(core::HorizontalAlign::Center)
                    .verticalAlign(core::VerticalAlign::Center)
                    .transition(transition_)
                    .animate(core::AnimProperty::TextColor)
                    .build();
            })
            .build();
    }

private:
    static int clampBase(int value) {
        return std::clamp(value, 2, 36);
    }

    static unsigned long long magnitudeOf(long long value) {
        if (value >= 0) {
            return static_cast<unsigned long long>(value);
        }
        return static_cast<unsigned long long>(-(value + 1LL)) + 1ULL;
    }

    static std::string formatUnsigned(unsigned long long value, int base, bool uppercase) {
        static constexpr char kDigitsLower[] = "0123456789abcdefghijklmnopqrstuvwxyz";
        static constexpr char kDigitsUpper[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        const char* digits = uppercase ? kDigitsUpper : kDigitsLower;

        if (value == 0ULL) {
            return "0";
        }

        std::string text;
        while (value > 0ULL) {
            const unsigned long long digit = value % static_cast<unsigned long long>(base);
            text.push_back(digits[static_cast<std::size_t>(digit)]);
            value /= static_cast<unsigned long long>(base);
        }
        std::reverse(text.begin(), text.end());
        return text;
    }

    static std::string defaultPrefixForBase(int base, bool uppercase) {
        if (base == 2) {
            return uppercase ? "0B" : "0b";
        }
        if (base == 8) {
            return uppercase ? "0O" : "0o";
        }
        if (base == 16) {
            return uppercase ? "0X" : "0x";
        }
        return {};
    }

    static int digitsFromBitWidth(int base, int bitWidth) {
        if (bitWidth <= 0) {
            return 0;
        }
        const double baseLog = std::log2(static_cast<double>(base));
        if (baseLog <= 0.0) {
            return bitWidth;
        }
        return std::max(1, static_cast<int>(std::ceil(static_cast<double>(bitWidth) / baseLog - 1e-9)));
    }

    static long long incrementValue(long long value, long long step, long long maxValue) {
        if (value >= maxValue) {
            return maxValue;
        }
        if (maxValue < std::numeric_limits<long long>::min() + step) {
            return maxValue;
        }
        if (value > maxValue - step) {
            return maxValue;
        }
        return value + step;
    }

    static long long decrementValue(long long value, long long step, long long minValue) {
        if (value <= minValue) {
            return minValue;
        }
        if (minValue > std::numeric_limits<long long>::max() - step) {
            return minValue;
        }
        if (value < minValue + step) {
            return minValue;
        }
        return value - step;
    }

    long long resolvedMin(int bitWidth) const {
        if (hasMin_) {
            return min_;
        }
        if (bitWidth > 0) {
            return 0;
        }
        return std::numeric_limits<long long>::min();
    }

    long long resolvedMax(int bitWidth) const {
        if (hasMax_) {
            return max_;
        }
        if (bitWidth <= 0) {
            return std::numeric_limits<long long>::max();
        }
        if (bitWidth >= 63) {
            return std::numeric_limits<long long>::max();
        }
        return static_cast<long long>((1ULL << bitWidth) - 1ULL);
    }

    int effectiveDigits(int base, int bitWidth) const {
        if (digits_ > 0) {
            return digits_;
        }
        return digitsFromBitWidth(base, bitWidth);
    }

    std::string formatValue(long long value, int base, int digits) const {
        std::string body = formatUnsigned(magnitudeOf(value), base, uppercase_);
        if (digits > 0 && static_cast<int>(body.size()) < digits) {
            body.insert(body.begin(), static_cast<std::size_t>(digits - static_cast<int>(body.size())), '0');
        }

        const std::string prefixText = prefix_.empty()
            ? (showBasePrefix_ ? defaultPrefixForBase(base, uppercase_) : std::string{})
            : prefix_;

        if (value < 0) {
            return "-" + prefixText + body;
        }
        return prefixText + body;
    }

    core::dsl::Ui& ui_;
    std::string id_;
    StepperStyle style_;
    core::Transition transition_ = core::Transition::make(0.16f, core::Ease::OutCubic);
    std::function<void(long long)> onChange_;
    std::string prefix_;
    long long value_ = 0;
    long long step_ = 1;
    long long min_ = 0;
    long long max_ = 0;
    float width_ = 180.0f;
    float height_ = 38.0f;
    float fontSize_ = 16.0f;
    int base_ = 10;
    int digits_ = 0;
    int bitWidth_ = 0;
    bool hasMin_ = false;
    bool hasMax_ = false;
    bool showBasePrefix_ = true;
    bool uppercase_ = true;
};

inline StepperBuilder stepper(core::dsl::Ui& ui, const std::string& id) {
    return StepperBuilder(ui, id);
}

} // namespace components
