#pragma once

#include "../ui/UIBuilder.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace EUINEO {

class SidebarNode : public UINode {
public:
    struct ItemSpec {
        std::string icon;
        std::string label;
        std::function<void()> onClick;
    };

    class Builder : public UIBuilderBase<SidebarNode, Builder> {
    public:
        Builder(UIContext& context, SidebarNode& node) : UIBuilderBase<SidebarNode, Builder>(context, node) {}

        Builder& brand(std::string primary, std::string secondary) {
            this->node_.trackComposeValue("brandPrimary", primary);
            this->node_.trackComposeValue("brandSecondary", secondary);
            this->node_.brandPrimary_ = std::move(primary);
            this->node_.brandSecondary_ = std::move(secondary);
            return *this;
        }

        Builder& width(float collapsed, float expanded) {
            this->node_.trackComposeValue("collapsedWidth", collapsed);
            this->node_.trackComposeValue("expandedWidth", expanded);
            this->node_.collapsedWidth_ = collapsed;
            this->node_.expandedWidth_ = expanded;
            this->node_.primitive().width = expanded;
            return *this;
        }

        Builder& item(std::string icon, std::string label, std::function<void()> handler) {
            this->node_.trackComposeValue("itemIcon", icon);
            this->node_.trackComposeValue("itemLabel", label);
            this->node_.items_.push_back(ItemSpec{std::move(icon), std::move(label), std::move(handler)});
            return *this;
        }

        Builder& selectedIndex(int index) {
            this->node_.trackComposeValue("selectedIndex", index);
            this->node_.selectedIndex_ = index;
            return *this;
        }

        Builder& themeToggle(std::function<void()> handler) {
            this->node_.onThemeToggle_ = std::move(handler);
            return *this;
        }
    };

    explicit SidebarNode(const std::string& key) : UINode(key) {
        themeRotationAnimation_.Bind(&themeRotation_);
        themeBlendAnimation_.Bind(&themeBlend_);
        resetDefaults();
        themeBlend_ = CurrentTheme == &DarkTheme ? 0.0f : 1.0f;
    }

    static constexpr const char* StaticTypeName() {
        return "SidebarNode";
    }

    const char* typeName() const override {
        return StaticTypeName();
    }

    void update() override;
    void draw() override;

protected:
    void resetDefaults() override;

private:
    static bool floatEq(float a, float b, float epsilon = 0.0001f) {
        return std::abs(a - b) <= epsilon;
    }

    static bool contains(const RectFrame& frame, float x, float y) {
        return x >= frame.x && x <= frame.x + frame.width &&
               y >= frame.y && y <= frame.y + frame.height;
    }

    static Color withAlpha(const Color& color, float alpha) {
        return Color(color.r, color.g, color.b, alpha);
    }

    static Color blendAlpha(const Color& color, float factor) {
        return Color(color.r, color.g, color.b, color.a * factor);
    }

    static void drawPanel(const RectFrame& frame, const RectStyle& fillStyle,
                          float borderWidth, const Color& borderColor) {
        if (borderWidth > 0.0f && borderColor.a > 0.0f &&
            frame.width > borderWidth * 2.0f && frame.height > borderWidth * 2.0f) {
            RectStyle borderStyle;
            borderStyle.color = borderColor;
            borderStyle.rounding = fillStyle.rounding;
            borderStyle.transform = fillStyle.transform;
            Renderer::DrawRect(frame.x, frame.y, frame.width, frame.height, borderStyle);

            RectStyle innerStyle = fillStyle;
            innerStyle.rounding = std::max(0.0f, fillStyle.rounding - borderWidth);
            Renderer::DrawRect(frame.x + borderWidth, frame.y + borderWidth,
                               frame.width - borderWidth * 2.0f, frame.height - borderWidth * 2.0f, innerStyle);
            return;
        }

        Renderer::DrawRect(frame.x, frame.y, frame.width, frame.height, fillStyle);
    }

    void ensureRuntimeState();
    RectFrame indicatorFrameFor(const RectFrame& shell) const;
    float itemHeight(const RectFrame& shell) const;
    RectFrame itemFrameFor(const RectFrame& shell, int index) const;
    RectFrame themeFrameFor(const RectFrame& shell) const;

    void requestRepaint(float expand = 18.0f, float duration = 0.0f) {
        (void)expand;
        requestVisualRepaint(duration);
    }

    std::string brandPrimary_;
    std::string brandSecondary_;
    int selectedIndex_ = 0;
    float collapsedWidth_ = 60.0f;
    float expandedWidth_ = 200.0f;
    std::vector<ItemSpec> items_;
    std::function<void()> onThemeToggle_;

    std::vector<float> itemHover_;
    float selectionAnim_ = 0.0f;
    bool selectionReady_ = false;
    float themeHover_ = 0.0f;
    bool themePressed_ = false;
    float themeRotation_ = 0.0f;
    FloatAnimation themeRotationAnimation_;
    float themeBlend_ = 0.0f;
    FloatAnimation themeBlendAnimation_;
};

inline void SidebarNode::update() {
    ensureRuntimeState();
    const RectFrame shell = PrimitiveFrame(primitive_);
    const int clampedSelectedIndex = items_.empty()
        ? 0
        : std::clamp(selectedIndex_, 0, static_cast<int>(items_.size()) - 1);

    if (!selectionReady_) {
        selectionAnim_ = static_cast<float>(clampedSelectedIndex);
        selectionReady_ = true;
    }

    const float targetSelection = static_cast<float>(clampedSelectedIndex);
    if (!floatEq(selectionAnim_, targetSelection, 0.001f)) {
        selectionAnim_ = Lerp(selectionAnim_, targetSelection, State.deltaTime * 15.0f);
        if (std::abs(selectionAnim_ - targetSelection) < 0.01f) {
            selectionAnim_ = targetSelection;
        }
        requestRepaint(24.0f, 0.18f);
    }

    for (int index = 0; index < static_cast<int>(items_.size()); ++index) {
        const RectFrame itemFrame = itemFrameFor(shell, index);
        const bool hovered = contains(itemFrame, State.mouseX, State.mouseY);
        const float targetHover = hovered ? 1.0f : 0.0f;
        if (!floatEq(itemHover_[index], targetHover, 0.001f)) {
            itemHover_[index] = Lerp(itemHover_[index], targetHover, State.deltaTime * 15.0f);
            if (std::abs(itemHover_[index] - targetHover) < 0.01f) {
                itemHover_[index] = targetHover;
            }
            requestRepaint(24.0f);
        }

        if (hovered && State.mouseClicked && items_[index].onClick) {
            items_[index].onClick();
            requestRepaint(24.0f, 0.18f);
        }
    }

    const RectFrame toggleFrame = themeFrameFor(shell);
    const bool hoveredToggle = contains(toggleFrame, State.mouseX, State.mouseY);
    const float targetThemeHover = hoveredToggle ? 1.0f : 0.0f;
    if (!floatEq(themeHover_, targetThemeHover, 0.001f)) {
        themeHover_ = Lerp(themeHover_, targetThemeHover, State.deltaTime * 15.0f);
        if (std::abs(themeHover_ - targetThemeHover) < 0.01f) {
            themeHover_ = targetThemeHover;
        }
        requestRepaint(24.0f);
    }

    if (hoveredToggle && State.mouseClicked) {
        themePressed_ = true;
        themeRotationAnimation_.PlayTo(20.0f, 0.12f, Easing::EaseOut);
        requestRepaint(24.0f, 0.18f);
    }

    if (themePressed_ && !State.mouseDown) {
        const bool shouldToggle = hoveredToggle;
        themePressed_ = false;
        themeRotationAnimation_.PlayTo(-18.0f, 0.10f, Easing::EaseOut);
        themeRotationAnimation_.Queue(0.0f, 0.16f, Easing::EaseInOut);
        if (shouldToggle && onThemeToggle_) {
            onThemeToggle_();
            themeBlendAnimation_.PlayTo(CurrentTheme == &DarkTheme ? 0.0f : 1.0f, 0.18f, Easing::EaseInOut);
        }
        requestRepaint(24.0f, 0.20f);
    }

    if (themeRotationAnimation_.Update(State.deltaTime)) {
        requestRepaint(24.0f, 0.18f);
    }

    if (themeBlendAnimation_.Update(State.deltaTime)) {
        requestRepaint(24.0f, 0.18f);
    }
}

inline void SidebarNode::draw() {
    ensureRuntimeState();
    PrimitiveClipScope clip(primitive_);
    const RectFrame shell = PrimitiveFrame(primitive_);
    const RectStyle shellStyle = MakeStyle(primitive_);
    drawPanel(shell, shellStyle, primitive_.borderWidth, ApplyOpacity(primitive_.borderColor, primitive_.opacity));

    const bool showLabels = shell.width >= 128.0f;
    const float brandPrimaryScale = 28.0f / 24.0f;
    const float brandSecondaryScale = 22.0f / 24.0f;
    const float primaryWidth = Renderer::MeasureTextWidth(brandPrimary_, brandPrimaryScale);
    const float secondaryWidth = Renderer::MeasureTextWidth(brandSecondary_, brandSecondaryScale);
    const float primaryX = showLabels ? shell.x + 20.0f : shell.x + (shell.width - primaryWidth) * 0.5f;
    const float secondaryX = showLabels ? shell.x + 20.0f : shell.x + (shell.width - secondaryWidth) * 0.5f;

    Renderer::DrawTextStr(brandPrimary_, primaryX, shell.y + 44.0f,
                          withAlpha(CurrentTheme->primary, CurrentTheme == &DarkTheme ? 1.0f : 0.94f),
                          brandPrimaryScale);
    Renderer::DrawTextStr(brandSecondary_, secondaryX, shell.y + 68.0f,
                          withAlpha(CurrentTheme->text, CurrentTheme == &DarkTheme ? 0.82f : 0.70f),
                          brandSecondaryScale);

    if (!items_.empty()) {
        const RectFrame indicator = indicatorFrameFor(shell);
        Renderer::DrawRect(indicator.x, indicator.y, indicator.width, indicator.height,
                           withAlpha(CurrentTheme->primary, CurrentTheme == &DarkTheme ? 0.22f : 0.16f), 14.0f);
    }

    for (int index = 0; index < static_cast<int>(items_.size()); ++index) {
        const RectFrame frame = itemFrameFor(shell, index);
        const bool selected = index == selectedIndex_;

        Color highlight = selected
            ? Color(0.0f, 0.0f, 0.0f, 0.0f)
            : Lerp(Color(0.0f, 0.0f, 0.0f, 0.0f),
                   withAlpha(CurrentTheme->surfaceHover, CurrentTheme == &DarkTheme ? 0.90f : 0.80f),
                   itemHover_[index]);
        if (highlight.a > 0.01f) {
            Renderer::DrawRect(frame.x, frame.y, frame.width, frame.height, highlight, 14.0f);
        }

        const float iconScale = 20.0f / 24.0f;
        const Color iconColor = selected
            ? withAlpha(CurrentTheme->text, 0.98f)
            : Lerp(withAlpha(CurrentTheme->text, CurrentTheme == &DarkTheme ? 0.60f : 0.72f),
                   withAlpha(CurrentTheme->text, 0.96f), itemHover_[index]);

        float iconX = frame.x + 16.0f;
        if (!showLabels) {
            const float iconWidth = Renderer::MeasureTextWidth(items_[index].icon, iconScale);
            iconX = frame.x + (frame.width - iconWidth) * 0.5f;
        }

        Renderer::DrawTextStr(items_[index].icon, iconX, frame.y + frame.height * 0.5f + 8.0f, iconColor, iconScale);

        if (showLabels) {
            const Color labelColor = selected
                ? withAlpha(CurrentTheme->text, 0.98f)
                : Lerp(withAlpha(CurrentTheme->text, CurrentTheme == &DarkTheme ? 0.68f : 0.80f),
                       withAlpha(CurrentTheme->text, 0.96f), itemHover_[index] * 0.85f);
            Renderer::DrawTextStr(items_[index].label, frame.x + 48.0f, frame.y + frame.height * 0.5f + 7.0f,
                                  labelColor, 16.0f / 24.0f);
        }
    }

    const RectFrame toggleFrame = themeFrameFor(shell);
    const float toggleMix = std::clamp(0.14f + themeHover_ * 0.14f + (themePressed_ ? 0.14f : 0.0f), 0.0f, 0.38f);
    Renderer::DrawRect(toggleFrame.x, toggleFrame.y, toggleFrame.width, toggleFrame.height,
                       Lerp(CurrentTheme->surfaceHover, CurrentTheme->primary, toggleMix), 14.0f);

    const float iconScale = 18.0f / 24.0f;
    float iconX = toggleFrame.x + 16.0f;
    if (!showLabels) {
        const float moonWidth = Renderer::MeasureTextWidth("\xEF\x86\x86", iconScale);
        iconX = toggleFrame.x + (toggleFrame.width - moonWidth) * 0.5f;
    }
    const float iconY = toggleFrame.y + toggleFrame.height * 0.5f + 7.0f;
    const Color iconColor = withAlpha(CurrentTheme->text, 0.96f);
    Renderer::DrawTextStr("\xEF\x86\x86", iconX, iconY,
                          blendAlpha(iconColor, 1.0f - std::clamp(themeBlend_, 0.0f, 1.0f)),
                          iconScale, themeRotation_);
    Renderer::DrawTextStr("\xEF\x86\x85", iconX, iconY,
                          blendAlpha(iconColor, std::clamp(themeBlend_, 0.0f, 1.0f)),
                          iconScale, themeRotation_);

    if (showLabels) {
        Renderer::DrawTextStr(CurrentTheme == &DarkTheme ? "Dark Theme" : "Light Theme",
                              toggleFrame.x + 48.0f, toggleFrame.y + toggleFrame.height * 0.5f + 7.0f,
                              withAlpha(CurrentTheme->text, 0.90f), 15.0f / 24.0f);
    }
}

inline void SidebarNode::resetDefaults() {
    primitive_ = UIPrimitive{};
    primitive_.width = 200.0f;
    primitive_.height = 300.0f;
    primitive_.background = CurrentTheme->surface;
    primitive_.rounding = 20.0f;
    primitive_.borderWidth = 1.0f;
    primitive_.borderColor = CurrentTheme->border;
    primitive_.shadow.blur = CurrentTheme == &DarkTheme ? 18.0f : 14.0f;
    primitive_.shadow.offsetY = CurrentTheme == &DarkTheme ? 8.0f : 6.0f;
    primitive_.shadow.color = CurrentTheme == &DarkTheme
        ? Color(0.0f, 0.0f, 0.0f, 0.24f)
        : Color(0.10f, 0.14f, 0.22f, 0.14f);
    brandPrimary_ = "EUI";
    brandSecondary_ = "NEO";
    selectedIndex_ = 0;
    collapsedWidth_ = 60.0f;
    expandedWidth_ = 200.0f;
    items_.clear();
    onThemeToggle_ = {};
}

inline void SidebarNode::ensureRuntimeState() {
    if (itemHover_.size() != items_.size()) {
        itemHover_.resize(items_.size(), 0.0f);
    }
}

inline RectFrame SidebarNode::indicatorFrameFor(const RectFrame& shell) const {
    RectFrame frame = itemFrameFor(shell, 0);
    if (items_.empty()) {
        return frame;
    }
    frame.y = shell.y + 112.0f + selectionAnim_ * (itemHeight(shell) + 10.0f);
    return frame;
}

inline float SidebarNode::itemHeight(const RectFrame& shell) const {
    return shell.width >= 128.0f ? 46.0f : 52.0f;
}

inline RectFrame SidebarNode::itemFrameFor(const RectFrame& shell, int index) const {
    const float rowHeight = itemHeight(shell);
    const float sideInset = shell.width >= 128.0f ? 12.0f : std::max(6.0f, (shell.width - rowHeight) * 0.5f);
    RectFrame frame;
    frame.x = shell.x + sideInset;
    frame.y = shell.y + 112.0f + index * (rowHeight + 10.0f);
    frame.width = shell.width >= 128.0f ? shell.width - sideInset * 2.0f : rowHeight;
    frame.height = rowHeight;
    return frame;
}

inline RectFrame SidebarNode::themeFrameFor(const RectFrame& shell) const {
    const float rowHeight = itemHeight(shell);
    const float sideInset = shell.width >= 128.0f ? 12.0f : std::max(6.0f, (shell.width - rowHeight) * 0.5f);
    RectFrame frame;
    frame.x = shell.x + sideInset;
    frame.y = shell.y + shell.height - rowHeight - 16.0f;
    frame.width = shell.width >= 128.0f ? shell.width - sideInset * 2.0f : rowHeight;
    frame.height = rowHeight;
    return frame;
}

} // namespace EUINEO
