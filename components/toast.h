#pragma once

#include "components/theme.h"
#include "core/dsl.h"
#include "eui/signal.h"

#include <algorithm>
#include <functional>
#include <string>
#include <utility>

namespace components {

struct ToastStyle {
    ToastStyle() : ToastStyle(theme::dark()) {}

    explicit ToastStyle(const theme::ThemeColorTokens& tokens) {
        background = tokens.dark
            ? core::mixColor(tokens.surface, theme::color(0.0f, 0.0f, 0.0f), 0.18f)
            : tokens.surface;
        border = theme::withOpacity(tokens.border, 0.82f);
        text = tokens.text;
        mutedText = theme::withOpacity(tokens.text, 0.68f);
        accent = tokens.primary;
        shadow = theme::popupShadow(tokens);
        radius = tokens.metrics.radius.elevated;
    }

    core::Color background;
    core::Color border;
    core::Color text;
    core::Color mutedText;
    core::Color accent;
    core::Shadow shadow;
    float radius = 14.0f;
};

class ToastBuilder {
public:
    ToastBuilder(core::dsl::Ui& ui, std::string id)
        : ui_(ui), id_(std::move(id)) {}

    ToastBuilder& visible(bool value = true) { visible_ = value; return *this; }
    ToastBuilder& bindVisible(eui::Signal<bool>& signal) {
        visible(signal.get());
        onDismiss([&signal] { signal.set(false); });
        onAutoDismiss([&signal] { signal.set(false); });
        return *this;
    }
    ToastBuilder& screen(float width, float height) { screenWidth_ = width; screenHeight_ = height; return *this; }
    ToastBuilder& size(float width, float height) { width_ = width; height_ = height; return *this; }
    ToastBuilder& title(const std::string& value) { title_ = value; return *this; }
    ToastBuilder& message(const std::string& value) { message_ = value; return *this; }
    ToastBuilder& icon(unsigned int codepoint) { icon_ = core::dsl::utf8(codepoint); return *this; }
    ToastBuilder& icon(const std::string& value) { icon_ = value; return *this; }
    ToastBuilder& style(const ToastStyle& value) { style_ = value; return *this; }
    ToastBuilder& theme(const theme::ThemeColorTokens& tokens) {
        style_ = ToastStyle(tokens);
        metrics_ = tokens.metrics;
        return *this;
    }
    ToastBuilder& transition(const core::Transition& value) { transition_ = value; return *this; }
    ToastBuilder& zIndex(int value) { zIndex_ = value; return *this; }
    ToastBuilder& duration(float seconds) { autoDismissSeconds_ = std::max(0.0f, seconds); return *this; }
    ToastBuilder& onAutoDismiss(std::function<void()> callback) { onAutoDismiss_ = std::move(callback); return *this; }
    ToastBuilder& onDismiss(std::function<void()> callback) { onDismiss_ = std::move(callback); return *this; }

    void build() {
        const float edgeInset = metrics_.spacing.section;
        const float width = std::min(width_, std::max(0.0f, screenWidth_ - edgeInset * 2.0f));
        const float height = std::min(height_, std::max(0.0f, screenHeight_ - edgeInset * 2.0f));
        const float x = std::max(edgeInset, screenWidth_ - width - metrics_.control.compact);
        const float y = std::max(edgeInset, screenHeight_ - height - metrics_.control.compact);
        const float iconSize = metrics_.control.indicator;
        const float textX = metrics_.control.control + metrics_.spacing.content;
        const float closeSize = metrics_.control.compact;
        const float textWidth = std::max(0.0f, width - textX - closeSize - metrics_.control.indicator);
        const float visible = visible_ ? 1.0f : 0.0f;
        const float toastOffsetX = visible_ ? 0.0f : 18.0f;
        const float toastOffsetY = visible_ ? 0.0f : 10.0f;
        const std::function<void()> onDismiss = onDismiss_;
        const std::function<void()> onAutoDismiss = onAutoDismiss_ ? onAutoDismiss_ : onDismiss_;

        ui_.stack(id_)
            .x(x)
            .y(y)
            .size(width, height)
            .zIndex(zIndex_)
            .opacity(visible)
            .translate(toastOffsetX, toastOffsetY)
            .transition(transition_)
            .animate(core::AnimProperty::Opacity | core::AnimProperty::Transform)
            .content([&] {
                ui_.rect(id_ + ".bg")
                    .size(width, height)
                    .color(style_.background)
                    .radius(style_.radius)
                    .border(metrics_.spacing.hairline, style_.border)
                    .shadow(style_.shadow)
                    .build();

                ui_.text(id_ + ".icon")
                    .x(metrics_.spacing.large)
                    .y(metrics_.spacing.large)
                    .size(iconSize, iconSize)
                    .icon(icon_)
                    .fontSize(iconSize)
                    .lineHeight(iconSize)
                    .color(style_.accent)
                    .horizontalAlign(core::HorizontalAlign::Center)
                    .build();

                ui_.text(id_ + ".title")
                    .x(textX)
                    .y(metrics_.spacing.section)
                    .size(textWidth, metrics_.control.switchHeight)
                    .text(title_)
                    .fontSize(metrics_.typography.control)
                    .lineHeight(metrics_.typography.control + metrics_.typography.lineGap)
                    .color(style_.text)
                    .build();

                ui_.text(id_ + ".message")
                    .x(textX)
                    .y(metrics_.control.control)
                    .size(textWidth, std::max(0.0f, height - 50.0f))
                    .text(message_)
                    .fontSize(metrics_.typography.label)
                    .lineHeight(metrics_.typography.label + metrics_.typography.lineGap)
                    .maxWidth(textWidth)
                    .wrap(true)
                    .color(style_.mutedText)
                    .build();

                ui_.rect(id_ + ".close.hit")
                    .x(std::max(0.0f, width - closeSize - metrics_.spacing.content))
                    .y(metrics_.spacing.content)
                    .size(closeSize, closeSize)
                    .states(theme::color(0.0f, 0.0f, 0.0f, 0.0f),
                            theme::withOpacity(style_.border, 0.36f),
                            theme::withOpacity(style_.border, 0.56f))
                    .radius(metrics_.radius.control)
                    .disabled(!visible_)
                    .onClick(onDismiss)
                    .build();

                ui_.text(id_ + ".close")
                    .x(std::max(0.0f, width - closeSize - metrics_.spacing.content))
                    .y(metrics_.typography.input)
                    .size(closeSize, closeSize)
                    .icon(0xF00D)
                    .fontSize(metrics_.typography.option)
                    .lineHeight(metrics_.typography.option + metrics_.typography.lineGapTight)
                    .color(style_.mutedText)
                    .horizontalAlign(core::HorizontalAlign::Center)
                    .verticalAlign(core::VerticalAlign::Top)
                    .build();

                {
                    auto timer = ui_.stack(id_ + ".timer")
                        .size(0.0f, 0.0f);
                    if (visible_ && autoDismissSeconds_ > 0.0f && onAutoDismiss) {
                        timer.onTimer(autoDismissSeconds_, onAutoDismiss);
                    }
                    timer.build();
                }
            })
            .build();
    }

private:
    core::dsl::Ui& ui_;
    std::string id_;
    ToastStyle style_;
    theme::ThemeMetricTokens metrics_;
    core::Transition transition_ = core::Transition::make(0.16f, core::Ease::OutCubic);
    std::function<void()> onDismiss_;
    std::function<void()> onAutoDismiss_;
    std::string title_ = "Toast";
    std::string message_ = "Short status message for non-blocking feedback.";
    std::string icon_ = core::dsl::utf8(0xF058);
    bool visible_ = false;
    float screenWidth_ = 800.0f;
    float screenHeight_ = 600.0f;
    float width_ = 360.0f;
    float height_ = 88.0f;
    float autoDismissSeconds_ = 0.0f;
    int zIndex_ = 1100;
};

inline ToastBuilder toast(core::dsl::Ui& ui, const std::string& id) {
    return ToastBuilder(ui, id);
}

} // namespace components
