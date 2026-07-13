#pragma once

#include "components/theme.h"
#include "core/dsl.h"

#include <algorithm>
#include <functional>
#include <string>
#include <utility>

namespace components::workshop {

struct NeumorphicButtonStyle {
    core::Color surface{0.91f, 0.91f, 0.91f, 1.0f};
    core::Color hover{0.95f, 0.95f, 0.95f, 1.0f};
    core::Color pressed{0.84f, 0.84f, 0.84f, 1.0f};
    core::Color text{0.035f, 0.035f, 0.035f, 1.0f};
    core::Color pressedText{0.40f, 0.40f, 0.40f, 1.0f};
    core::Color border{0.91f, 0.91f, 0.91f, 1.0f};
    core::Color darkShadow{0.77f, 0.77f, 0.77f, 1.0f};
    core::Color lightShadow{1.0f, 1.0f, 1.0f, 1.0f};
    core::Color innerDark{0.77f, 0.77f, 0.77f, 1.0f};
    core::Color innerLight{1.0f, 1.0f, 1.0f, 1.0f};
    float radius = 14.0f;
    float pressScale = 0.986f;
};

inline NeumorphicButtonStyle neumorphicButtonStyle(const theme::ThemeColorTokens& tokens) {
    NeumorphicButtonStyle style;
    style.radius = tokens.metrics.radius.elevated;
    const core::Color sidebarSurface = tokens.dark
        ? core::mixColor(tokens.surface, {0.0f, 0.0f, 0.0f, 1.0f}, 0.14f)
        : tokens.surface;
    if (tokens.dark) {
        style.surface = sidebarSurface;
        style.hover = core::mixColor(style.surface, {1.0f, 1.0f, 1.0f, 1.0f}, 0.035f);
        style.pressed = core::mixColor(style.surface, {0.0f, 0.0f, 0.0f, 1.0f}, 0.08f);
        style.text = theme::withAlpha(tokens.text, 0.94f);
        style.pressedText = theme::withAlpha(tokens.text, 0.66f);
        style.border = theme::withAlpha(style.surface, 0.90f);
        style.darkShadow = {0.0f, 0.0f, 0.0f, 0.38f};
        style.lightShadow = {1.0f, 1.0f, 1.0f, 0.18f};
        style.innerDark = {0.0f, 0.0f, 0.0f, 0.28f};
        style.innerLight = {1.0f, 1.0f, 1.0f, 0.13f};
    } else {
        style.surface = core::mixColor(sidebarSurface, tokens.surfaceHover, 0.08f);
        style.hover = core::mixColor(style.surface, tokens.surfaceHover, 0.10f);
        style.pressed = core::mixColor(style.surface, tokens.surfaceActive, 0.18f);
        style.text = theme::withAlpha(tokens.text, 0.94f);
        style.pressedText = theme::withAlpha(tokens.text, 0.58f);
        style.border = theme::withAlpha(style.surface, 0.95f);
        style.darkShadow = {0.63f, 0.66f, 0.72f, 0.26f};
        style.lightShadow = {1.0f, 1.0f, 1.0f, 1.0f};
        style.innerDark = {0.62f, 0.64f, 0.70f, 0.34f};
        style.innerLight = {1.0f, 1.0f, 1.0f, 0.92f};
    }
    return style;
}

class NeumorphicButtonBuilder {
public:
    NeumorphicButtonBuilder(core::dsl::Ui& ui, std::string id)
        : ui_(ui), id_(std::move(id)) {}

    NeumorphicButtonBuilder& size(float width, float height) {
        width_ = width;
        height_ = height;
        return *this;
    }

    NeumorphicButtonBuilder& text(std::string value) {
        text_ = std::move(value);
        return *this;
    }

    NeumorphicButtonBuilder& fontSize(float value) {
        fontSize_ = value;
        return *this;
    }

    NeumorphicButtonBuilder& style(const NeumorphicButtonStyle& value) {
        style_ = value;
        return *this;
    }

    NeumorphicButtonBuilder& theme(const theme::ThemeColorTokens& tokens) {
        style_ = neumorphicButtonStyle(tokens);
        metrics_ = tokens.metrics;
        return *this;
    }

    NeumorphicButtonBuilder& radius(float value) {
        style_.radius = std::max(0.0f, value);
        return *this;
    }

    NeumorphicButtonBuilder& pressScale(float value) {
        style_.pressScale = std::clamp(value, 0.80f, 1.0f);
        return *this;
    }

    NeumorphicButtonBuilder& disabled(bool value = true) {
        disabled_ = value;
        return *this;
    }

    NeumorphicButtonBuilder& transition(const core::Transition& value) {
        transition_ = value;
        return *this;
    }

    NeumorphicButtonBuilder& transition(float duration, core::Ease ease = core::Ease::OutCubic) {
        transition_ = core::Transition::make(duration, ease);
        return *this;
    }

    NeumorphicButtonBuilder& onClick(std::function<void()> callback) {
        onClick_ = std::move(callback);
        return *this;
    }

    void build() {
        const float w = std::max(1.0f, width_);
        const float h = std::max(1.0f, height_);
        const float padX = std::clamp(w * 0.10f, 20.0f, 38.0f);
        const float padY = std::clamp(h * 0.18f, 18.0f, 34.0f);
        const float buttonW = std::max(1.0f, w - padX * 2.0f);
        const float buttonH = std::max(1.0f, h - padY * 2.0f);
        const float radius = std::min(style_.radius, buttonH * 0.5f);
        const float font = fontSize_ > 0.0f ? fontSize_ : metrics_.typography.control;
        bool* pressedState = &ui_.state<bool>(id_ + ".pressed");
        const bool pressed = *pressedState;
        const core::Color surface = pressed ? style_.pressed : style_.surface;
        const core::Color hover = pressed ? style_.pressed : style_.hover;
        const core::Color text = pressed ? style_.pressedText : style_.text;
        const core::Color outerDark = pressed ? transparentLike(style_.darkShadow) : style_.darkShadow;
        const core::Color outerLight = pressed ? transparentLike(style_.lightShadow) : style_.lightShadow;
        const core::Color innerDark = pressed ? style_.innerDark : transparentLike(style_.innerDark);
        const core::Color innerLight = pressed ? style_.innerLight : transparentLike(style_.innerLight);
        const core::Transition shadowCutTransition = core::Transition::none();

        ui_.stack(id_)
            .size(w, h)
            .visualStateFrom(id_ + ".surface", style_.pressScale)
            .content([&] {
                ui_.rect(id_ + ".light.shadow")
                    .x(padX)
                    .y(padY)
                    .size(buttonW, buttonH)
                    .color(transparent())
                    .radius(radius)
                    .shadow(10.0f, -5.0f, -5.0f, outerLight)
                    .disabled(true)
                    .transition(shadowCutTransition)
                    .build();

                ui_.rect(id_ + ".dark.shadow")
                    .x(padX)
                    .y(padY)
                    .size(buttonW, buttonH)
                    .color(transparent())
                    .radius(radius)
                    .shadow(14.0f, 7.0f, 7.0f, outerDark)
                    .disabled(true)
                    .transition(shadowCutTransition)
                    .build();

                ui_.rect(id_ + ".surface")
                    .x(padX)
                    .y(padY)
                    .size(buttonW, buttonH)
                    .states(surface, hover, style_.pressed)
                    .radius(radius)
                    .border(1.0f, style_.border)
                    .disabled(disabled_)
                    .transition(transition_)
                    .onPress([pressedState](const core::PointerEvent&, const core::Rect&) {
                        *pressedState = true;
                    })
                    .onRelease([pressedState](const core::PointerEvent&, const core::Rect&) {
                        *pressedState = false;
                    })
                    .onClick(onClick_)
                    .build();

                ui_.rect(id_ + ".inset.dark")
                    .x(padX)
                    .y(padY)
                    .size(buttonW, buttonH)
                    .color(transparent())
                    .radius(radius)
                    .insetShadow(10.0f, 4.0f, 4.0f, innerDark)
                    .disabled(true)
                    .transition(transition_)
                    .build();

                ui_.rect(id_ + ".inset.light")
                    .x(padX)
                    .y(padY)
                    .size(buttonW, buttonH)
                    .color(transparent())
                    .radius(radius)
                    .insetShadow(10.0f, -4.0f, -4.0f, innerLight)
                    .disabled(true)
                    .transition(transition_)
                    .build();

                ui_.text(id_ + ".text")
                    .x(padX)
                    .y(padY)
                    .size(buttonW, buttonH)
                    .text(text_)
                    .fontSize(font)
                    .lineHeight(font)
                    .fontWeight(500)
                    .color(text)
                    .horizontalAlign(core::HorizontalAlign::Center)
                    .verticalAlign(core::VerticalAlign::Center)
                    .transition(transition_)
                    .build();
            })
            .build();
    }

private:
    static core::Color transparent() {
        return {0.0f, 0.0f, 0.0f, 0.0f};
    }

    static core::Color transparentLike(core::Color color) {
        color.a = 0.0f;
        return color;
    }

    core::dsl::Ui& ui_;
    std::string id_;
    std::string text_ = "Click me";
    NeumorphicButtonStyle style_;
    theme::ThemeMetricTokens metrics_;
    core::Transition transition_;
    std::function<void()> onClick_;
    float width_ = 190.0f;
    float height_ = 54.0f;
    float fontSize_ = 0.0f;
    bool disabled_ = false;
};

inline NeumorphicButtonBuilder neumorphicButton(core::dsl::Ui& ui, const std::string& id) {
    return NeumorphicButtonBuilder(ui, id);
}

} // namespace components::workshop
