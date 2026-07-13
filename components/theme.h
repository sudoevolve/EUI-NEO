#pragma once

#include "core/render/render_types.h"

#include <algorithm>

namespace components::theme {

struct TypographyTokens {
    float micro = 11.0f;
    float caption = 12.0f;
    float hint = 13.0f;
    float label = 14.0f;
    float option = 15.0f;
    float body = 16.0f;
    float input = 17.0f;
    float control = 18.0f;
    float cardTitle = 19.0f;
    float subtitle = 20.0f;
    float title = 22.0f;
    float heading = 24.0f;
    float headline = 27.0f;
    float displayCompact = 30.0f;
    float display = 31.0f;
    float hero = 42.0f;
    float lineGapTight = 3.0f;
    float lineGap = 4.0f;
    float lineGapRelaxed = 5.0f;
    float lineGapLoose = 6.0f;
    float lineGapComfortable = 7.0f;
    float lineGapWide = 8.0f;
};

struct SpacingTokens {
    float hairline = 1.0f;
    float micro = 2.0f;
    float tiny = 4.0f;
    float small = 6.0f;
    float compact = 8.0f;
    float control = 10.0f;
    float content = 12.0f;
    float section = 16.0f;
    float large = 20.0f;
    float panel = 24.0f;
    float header = 30.0f;
    float page = 40.0f;
    float overlay = 48.0f;
};

struct RadiusTokens {
    float micro = 2.0f;
    float tiny = 4.0f;
    float small = 6.0f;
    float control = 8.0f;
    float tooltip = 9.0f;
    float popup = 10.0f;
    float card = 12.0f;
    float elevated = 14.0f;
    float overlay = 16.0f;
    float section = 18.0f;
    float feature = 22.0f;
    float full = 999.0f;
};

struct ControlSizeTokens {
    float progress = 15.0f;
    float indicator = 22.0f;
    float switchHeight = 24.0f;
    float compact = 28.0f;
    float menuItem = 34.0f;
    float field = 35.0f;
    float segmented = 36.0f;
    float input = 40.0f;
    float control = 42.0f;
    float large = 44.0f;
    float switchWidth = 46.0f;
    float navigation = 50.0f;
    float scrollbar = 8.0f;
};

struct ThemeMetricTokens {
    TypographyTokens typography;
    SpacingTokens spacing;
    RadiusTokens radius;
    ControlSizeTokens control;
};

struct ThemeColorTokens {
    core::Color background;
    core::Color primary;
    core::Color surface;
    core::Color surfaceHover;
    core::Color surfaceActive;
    core::Color text;
    core::Color border;
    bool dark = false;
    ThemeMetricTokens metrics;
};

struct PageVisualTokens {
    core::Color titleColor;
    core::Color subtitleColor;
    core::Color bodyColor;
    core::Color cardColor;
    core::Color mutedCardColor;
    core::Color softAccentColor;
    float headerTopInset = 24.0f;
    float headerTitleGap = 30.0f;
    float headerContentGap = 40.0f;
    float headerTitleSize = 31.0f;
    float headerSubtitleSize = 20.0f;
    float sectionGap = 16.0f;
    float sectionInset = 20.0f;
    float sectionRounding = 18.0f;
    float labelSize = 17.0f;
    float fieldHeight = 35.0f;
};

struct FieldVisualTokens {
    float rounding = 6.0f;
    float horizontalInset = 10.0f;
    float focusLineHeight = 2.0f;
    float borderLineHeight = 1.0f;
    float popupRounding = 10.0f;
    float popupOverlap = 1.0f;
    core::Color popupShadowColor;
    float popupShadowBlur = 0.0f;
    float popupShadowOffsetY = 0.0f;
};

struct PageHeaderLayout {
    float titleY = 0.0f;
    float subtitleY = 0.0f;
    float contentY = 0.0f;
};

inline core::Color color(float r, float g, float b, float a = 1.0f) {
    return {r, g, b, a};
}

inline core::Color defaultPrimary(float a = 1.0f) {
    return color(56.0f / 255.0f, 113.0f / 255.0f, 224.0f / 255.0f, a);
}

inline core::Color withAlpha(core::Color value, float alpha) {
    value.a = std::clamp(alpha, 0.0f, 1.0f);
    return value;
}

inline core::Color withOpacity(core::Color value, float opacity) {
    value.a *= std::clamp(opacity, 0.0f, 1.0f);
    return value;
}

inline ThemeColorTokens light() {
    return {
        color(0.95f, 0.95f, 0.97f),
        defaultPrimary(),
        color(1.00f, 1.00f, 1.00f),
        color(0.90f, 0.90f, 0.90f),
        color(0.80f, 0.80f, 0.80f),
        color(0.00f, 0.00f, 0.00f),
        color(0.80f, 0.80f, 0.80f),
        false
    };
}

inline ThemeColorTokens dark() {
    return {
        color(0.10f, 0.10f, 0.12f),
        defaultPrimary(),
        color(0.15f, 0.15f, 0.18f),
        color(0.25f, 0.25f, 0.28f),
        color(0.35f, 0.35f, 0.38f),
        color(1.00f, 1.00f, 1.00f),
        color(0.30f, 0.30f, 0.30f),
        true
    };
}

inline PageVisualTokens pageVisuals(const ThemeColorTokens& tokens) {
    const ThemeMetricTokens& metrics = tokens.metrics;
    return {
        withAlpha(tokens.text, 0.98f),
        withAlpha(tokens.text, 0.72f),
        withAlpha(tokens.text, 0.68f),
        tokens.surface,
        tokens.surfaceHover,
        withAlpha(tokens.primary, 0.16f),
        metrics.spacing.panel,
        metrics.spacing.header,
        metrics.spacing.page,
        metrics.typography.display,
        metrics.typography.subtitle,
        metrics.spacing.section,
        metrics.spacing.large,
        metrics.radius.section,
        metrics.typography.input,
        metrics.control.field
    };
}

inline FieldVisualTokens fieldVisuals(const ThemeColorTokens& tokens) {
    FieldVisualTokens result;
    result.rounding = tokens.metrics.radius.small;
    result.horizontalInset = tokens.metrics.spacing.control;
    result.focusLineHeight = tokens.metrics.spacing.micro;
    result.borderLineHeight = tokens.metrics.spacing.hairline;
    result.popupRounding = tokens.metrics.radius.popup;
    result.popupOverlap = tokens.metrics.spacing.hairline;
    result.popupShadowColor = tokens.dark
        ? color(0.0f, 0.0f, 0.0f, 0.28f)
        : color(0.10f, 0.14f, 0.22f, 0.14f);
    result.popupShadowBlur = tokens.dark ? 18.0f : 12.0f;
    result.popupShadowOffsetY = tokens.dark ? 8.0f : 5.0f;
    return result;
}

inline core::Color resolveFieldFill(const ThemeColorTokens& tokens,
                                    core::Color baseColor,
                                    float hoverAmount,
                                    float activeAmount) {
    const float hover = std::clamp(hoverAmount, 0.0f, 1.0f);
    const float active = std::clamp(activeAmount, 0.0f, 1.0f);
    const core::Color base = baseColor.a > 0.0f ? baseColor : tokens.surface;
    const core::Color hoverColor = baseColor.a > 0.0f
        ? core::mixColor(base, tokens.surfaceHover, 0.65f)
        : tokens.surfaceHover;
    return core::mixColor(core::mixColor(base, tokens.surfaceActive, active), hoverColor, hover);
}

inline core::Color buttonHover(const ThemeColorTokens& tokens, core::Color base) {
    return core::mixColor(base, tokens.dark ? color(1.0f, 1.0f, 1.0f, base.a) : tokens.primary, tokens.dark ? 0.16f : 0.10f);
}

inline core::Color buttonPressed(const ThemeColorTokens& tokens, core::Color base) {
    return core::mixColor(base, tokens.dark ? color(0.0f, 0.0f, 0.0f, base.a) : tokens.surfaceActive, tokens.dark ? 0.34f : 0.22f);
}

inline core::Border border(const ThemeColorTokens& tokens, float width = 1.0f, float opacity = 1.0f) {
    return {width, withOpacity(tokens.border, opacity)};
}

inline core::Border buttonBorder(const ThemeColorTokens& tokens, bool primary = true) {
    return {1.0f, primary ? withAlpha(tokens.primary, 0.58f) : withOpacity(tokens.border, 0.70f)};
}

inline core::Shadow shadow(const ThemeColorTokens& tokens,
                           float blur,
                           float offsetY,
                           float darkAlpha,
                           float lightAlpha) {
    return {
        true,
        {0.0f, offsetY},
        blur,
        0.0f,
        tokens.dark ? color(0.0f, 0.0f, 0.0f, darkAlpha) : color(0.10f, 0.14f, 0.22f, lightAlpha)
    };
}

inline core::Shadow buttonShadow(const ThemeColorTokens& tokens) {
    return shadow(tokens, 14.0f, 4.0f, 0.22f, 0.10f);
}

inline core::Shadow panelShadow(const ThemeColorTokens& tokens) {
    return shadow(tokens, 24.0f, 8.0f, 0.28f, 0.12f);
}

inline core::Shadow popupShadow(const ThemeColorTokens& tokens) {
    const FieldVisualTokens field = fieldVisuals(tokens);
    return {true, {0.0f, field.popupShadowOffsetY}, field.popupShadowBlur, 0.0f, field.popupShadowColor};
}

} // namespace components::theme
