#pragma once

#include "components/theme.h"
#include "core/dsl.h"

#include <algorithm>
#include <functional>
#include <string>
#include <utility>

namespace components {

class SidebarBuilder {
public:
    SidebarBuilder(core::dsl::Ui& ui, std::string id)
        : ui_(ui), id_(std::move(id)) {}

    SidebarBuilder& size(float width, float height) { width_ = width; height_ = height; return *this; }
    SidebarBuilder& drawerWidth(float value) { drawerWidth_ = std::max(280.0f, value); return *this; }
    SidebarBuilder& open(bool value = true) { open_ = value; return *this; }
    SidebarBuilder& title(std::string value) { title_ = std::move(value); return *this; }
    SidebarBuilder& eyebrow(std::string value) { eyebrow_ = std::move(value); return *this; }
    SidebarBuilder& theme(const theme::ThemeColorTokens& tokens) { tokens_ = tokens; return *this; }
    SidebarBuilder& zIndex(int value) { zIndex_ = value; return *this; }
    SidebarBuilder& z(int value) { return zIndex(value); }
    SidebarBuilder& transition(const core::Transition& value) { transition_ = value; return *this; }

    SidebarBuilder& onClose(std::function<void()> callback) {
        onClose_ = std::move(callback);
        return *this;
    }

    template <typename ComposeFn>
    SidebarBuilder& content(ComposeFn&& compose) {
        content_ = std::forward<ComposeFn>(compose);
        return *this;
    }

    void build() {
        const float panelWidth = std::min(drawerWidth_, std::max(280.0f, width_ - 24.0f));
        const float panelX = std::max(0.0f, width_ - panelWidth);
        const float panelOffset = open_ ? 0.0f : panelWidth + 8.0f;
        const float contentWidth = std::max(0.0f, panelWidth - 44.0f);
        const float headerHeight = 104.0f;
        const float bodyHeight = std::max(0.0f, height_ - headerHeight - 44.0f);
        const core::Transition motionTransition = resolveMotionTransition();
        const std::function<void()> onClose = onClose_;

        ui_.stack(id_)
            .size(width_, height_)
            .zIndex(zIndex_)
            .clip()
            .content([&] {
                ui_.rect(id_ + ".scrim")
                    .size(panelX, height_)
                    .color(transparentColor())
                    .disabled(!open_)
                    .onClick(onClose)
                    .onScroll([](const core::ScrollEvent&) {})
                    .build();

                ui_.stack(id_ + ".panel")
                    .x(panelX)
                    .size(panelWidth, height_)
                    .translateX(panelOffset)
                    .transition(motionTransition)
                    .animate(core::AnimProperty::Transform)
                    .content([&] {
                        ui_.rect(id_ + ".panel.bg")
                            .size(panelWidth, height_)
                            .color(surfaceColor())
                            .shadow(34.0f, -10.0f, 12.0f, shadowColor())
                            .disabled(!open_)
                            .transition(transition_)
                            .onClick([] {})
                            .onScroll([](const core::ScrollEvent&) {})
                            .build();

                        ui_.column(id_ + ".content")
                            .size(panelWidth, height_)
                            .padding(22.0f, 22.0f)
                            .gap(18.0f)
                            .content([&] {
                                composeHeader(contentWidth, headerHeight, onClose);

                                ui_.stack(id_ + ".body")
                                    .size(contentWidth, bodyHeight)
                                    .clip()
                                    .content([&] {
                                        if (content_) {
                                            content_(ui_, contentWidth, bodyHeight);
                                        }
                                    })
                                    .build();
                            })
                            .build();
                    })
                    .build();
            })
            .build();
    }

private:
    core::Color withOpacity(core::Color color, float opacity) const {
        color.a *= std::clamp(opacity, 0.0f, 1.0f);
        return color;
    }

    core::Color withAlpha(core::Color color, float alpha) const {
        color.a = std::clamp(alpha, 0.0f, 1.0f);
        return color;
    }

    core::Color textColor(float opacity = 1.0f) const {
        return withOpacity(tokens_.text, opacity);
    }

    core::Color surfaceColor() const {
        return tokens_.dark
            ? core::mixColor(tokens_.surface, {0.0f, 0.0f, 0.0f, 1.0f}, 0.14f)
            : tokens_.surface;
    }

    core::Color softSurfaceColor() const {
        return tokens_.dark
            ? core::mixColor(tokens_.surfaceHover, {0.0f, 0.0f, 0.0f, 1.0f}, 0.08f)
            : core::mixColor(tokens_.surface, tokens_.surfaceHover, 0.58f);
    }

    core::Color transparentColor() const {
        return core::Color{0.0f, 0.0f, 0.0f, 0.0f};
    }

    core::Color borderColor(float opacity = 1.0f) const {
        return withOpacity(tokens_.border, opacity);
    }

    core::Color shadowColor() const {
        return tokens_.dark
            ? core::Color{0.0f, 0.0f, 0.0f, 0.34f}
            : core::Color{0.10f, 0.14f, 0.22f, 0.16f};
    }

    core::Transition resolveMotionTransition() const {
        if (!transition_.enabled) {
            return transition_;
        }
        core::Transition value = transition_;
        value.ease = core::Ease::OutExpo;
        value.durationSeconds = std::max(value.durationSeconds, 0.92f);
        return value;
    }

    void composeHeader(float width, float height, const std::function<void()>& onClose) {
        const float closeSize = 38.0f;
        const float closeX = std::max(0.0f, width - closeSize);
        ui_.stack(id_ + ".header")
            .size(width, height)
            .content([&] {
                ui_.text(id_ + ".eyebrow")
                    .size(std::max(0.0f, width - closeSize - 12.0f), 22.0f)
                    .text(eyebrow_)
                    .fontSize(15.0f)
                    .lineHeight(20.0f)
                    .fontWeight(800)
                    .color(withAlpha(tokens_.primary, 0.90f))
                    .build();

                ui_.text(id_ + ".title")
                    .y(42.0f)
                    .size(width, 54.0f)
                    .text(title_)
                    .fontSize(42.0f)
                    .lineHeight(50.0f)
                    .fontWeight(820)
                    .color(textColor())
                    .build();

                ui_.stack(id_ + ".close")
                    .x(closeX)
                    .size(closeSize, closeSize)
                    .content([&] {
                        ui_.rect(id_ + ".close.hit")
                            .size(closeSize, closeSize)
                            .states({0.0f, 0.0f, 0.0f, 0.0f}, softSurfaceColor(), tokens_.surfaceActive)
                            .radius(10.0f)
                            .disabled(!open_)
                            .transition(transition_)
                            .onClick(onClose)
                            .build();

                        ui_.text(id_ + ".close.icon")
                            .size(closeSize, closeSize)
                            .icon(0xF00D)
                            .fontSize(24.0f)
                            .lineHeight(28.0f)
                            .color(textColor())
                            .horizontalAlign(core::HorizontalAlign::Center)
                            .verticalAlign(core::VerticalAlign::Center)
                            .build();
                    })
                    .build();
            })
            .build();
    }

    core::dsl::Ui& ui_;
    std::string id_;
    std::string title_ = "Workshop";
    std::string eyebrow_ = "WORKSHOP";
    theme::ThemeColorTokens tokens_ = theme::DarkThemeColors();
    core::Transition transition_ = core::Transition::make(0.24f, core::Ease::OutCubic);
    std::function<void()> onClose_;
    std::function<void(core::dsl::Ui&, float, float)> content_;
    float width_ = 960.0f;
    float height_ = 640.0f;
    float drawerWidth_ = 420.0f;
    int zIndex_ = 60;
    bool open_ = false;
};

inline SidebarBuilder sidebar(core::dsl::Ui& ui, const std::string& id) {
    return SidebarBuilder(ui, id);
}

} // namespace components
