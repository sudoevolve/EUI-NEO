#pragma once

#include "components/button.h"
#include "components/mousearea.h"
#include "eui/app.h"
#include "core/dsl.h"

#include <algorithm>
#include <functional>
#include <string>
#include <utility>

namespace components {

// Custom window decoration for a frameless window (see DslAppConfig::frameless).
// Covers the WHOLE window: a title strip at the top (title + minimize/maximize/
// close buttons, drag-to-move) plus optional resize handles along the four
// edges and corners. Pass size() the full window dimensions and titleHeight()
// the strip height.
//
// By default the controls drive the running DSL app's window through
// app::minimizeWindow() / app::toggleMaximizeWindow() / app::requestWindowClose()
// / app::dragWindow() / app::startWindowResize(edge). Every action can be
// overridden with the on*() setters.
class TitleBarBuilder {
public:
    TitleBarBuilder(core::dsl::Ui& ui, std::string id)
        : ui_(ui), id_(std::move(id)) {}

    TitleBarBuilder& size(float width, float height) {
        width_ = width;
        height_ = height;
        return *this;
    }
    TitleBarBuilder& titleHeight(float value) { titleHeight_ = value; return *this; }
    TitleBarBuilder& title(std::string value) { title_ = std::move(value); return *this; }
    TitleBarBuilder& fontSize(float value) { fontSize_ = value; return *this; }
    TitleBarBuilder& background(const core::Color& value) { background_ = value; return *this; }
    TitleBarBuilder& titleColor(const core::Color& value) { titleColor_ = value; return *this; }
    TitleBarBuilder& buttonHover(const core::Color& value) { buttonHover_ = value; return *this; }
    TitleBarBuilder& closeHover(const core::Color& value) { closeHover_ = value; return *this; }
    TitleBarBuilder& showMinimize(bool value = true) { showMinimize_ = value; return *this; }
    TitleBarBuilder& showMaximize(bool value = true) { showMaximize_ = value; return *this; }
    TitleBarBuilder& showClose(bool value = true) { showClose_ = value; return *this; }
    TitleBarBuilder& draggable(bool value = true) { draggable_ = value; return *this; }
    TitleBarBuilder& resizable(bool value = true) { resizable_ = value; return *this; }
    TitleBarBuilder& resizeMargin(float value) { resizeMargin_ = value; return *this; }
    TitleBarBuilder& onMinimize(std::function<void()> callback) { onMinimize_ = std::move(callback); return *this; }
    TitleBarBuilder& onMaximize(std::function<void()> callback) { onMaximize_ = std::move(callback); return *this; }
    TitleBarBuilder& onClose(std::function<void()> callback) { onClose_ = std::move(callback); return *this; }
    TitleBarBuilder& onDrag(std::function<void()> callback) { onDrag_ = std::move(callback); return *this; }
    // 自定义标题栏内容:传入后完全接管标题栏内容区(取代默认的 title + 三按钮),
    // 背景拖动 / resize 手柄仍自动提供。回调里用 ui.text / components::button 等自由搭建。
    TitleBarBuilder& content(std::function<void()> callback) { content_ = std::move(callback); return *this; }

    void build() {
        const float w = width_ > 0.0f ? width_ : 400.0f;
        const float h = height_ > 0.0f ? height_ : 300.0f;
        const float th = titleHeight_ > 0.0f ? titleHeight_ : 48.0f;
        const float m = resizable_ ? std::max(2.0f, resizeMargin_) : 0.0f;
        const float font = fontSize_ > 0.0f ? fontSize_ : th * 0.40f;
        const float buttonSize = th * 0.88f;
        const float buttonGap = th * 0.12f;
        const float pad = th * 0.35f;

        const std::function<void()> minimize = onMinimize_ ? onMinimize_ : [] { app::minimizeWindow(); };
        const std::function<void()> maximize = onMaximize_ ? onMaximize_ : [] { app::toggleMaximizeWindow(); };
        const std::function<void()> close = onClose_ ? onClose_ : [] { app::requestWindowClose(); };
        const std::function<void()> drag = onDrag_ ? onDrag_ : [] { app::dragWindow(); };

        int controlCount = 0;
        if (showMinimize_) { ++controlCount; }
        if (showMaximize_) { ++controlCount; }
        if (showClose_) { ++controlCount; }
        const float buttonsWidth = controlCount > 0
            ? static_cast<float>(controlCount) * buttonSize + static_cast<float>(controlCount - 1) * buttonGap
            : 0.0f;

        ui_.stack(id_)
            .size(w, h)
            .content([&] {
                // Title strip background. The top `m` pixels are covered by the
                // resize handle (higher z-index), so the draggable region is
                // effectively y in [m, th].
                auto background = ui_.rect(id_ + ".bg")
                    .size(w, th)
                    .color(background_);
                if (draggable_) {
                    background.onPress([drag](const core::PointerEvent&, const core::Rect&) { drag(); });
                }
                background.build();

                if (content_) {
                    // 用户自定义标题栏内容:stack 容器,可用 row/column/x/y 自由布局
                    ui_.stack(id_ + ".content")
                        .size(w, th)
                        .content(content_)
                        .build();
                } else {
                    // 默认布局:标题靠左,窗口控制按钮靠右
                    if (!title_.empty()) {
                        ui_.text(id_ + ".title")
                            .x(pad)
                            .size(std::max(0.0f, w - pad - buttonsWidth - buttonGap - pad), th)
                            .text(title_)
                            .fontSize(font)
                            .color(titleColor_)
                            .horizontalAlign(core::HorizontalAlign::Left)
                            .verticalAlign(core::VerticalAlign::Center)
                            .build();
                    }

                    if (controlCount > 0) {
                        ui_.row(id_ + ".buttons")
                            .x(w - pad - buttonsWidth)
                            .y((th - buttonSize) * 0.5f)
                            .size(buttonsWidth, buttonSize)
                            .gap(buttonGap)
                            .content([&] {
                                if (showMinimize_) {
                                    controlButton(id_ + ".min", 0xF2D1, buttonSize, background_, buttonHover_, minimize);
                                }
                                if (showMaximize_) {
                                    // 全屏/还原图标随窗口状态切换:未全屏显示全屏图标,
                                    // 已全屏显示还原图标
                                    controlButton(id_ + ".max",
                                                  app::isWindowMaximized() ? 0xF2D2 : 0xF2D0,
                                                  buttonSize, background_, buttonHover_, maximize);
                                }
                                if (showClose_) {
                                    controlButton(id_ + ".close", 0xF00D, buttonSize, background_, closeHover_, close);
                                }
                            })
                            .build();
                    }
                }

                if (resizable_) {
                    resizeHandle(id_ + ".rs.tl", 0.0f, 0.0f, m, m, eui::window::ResizeEdge::TopLeft);
                    resizeHandle(id_ + ".rs.t", m, 0.0f, w - 2.0f * m, m, eui::window::ResizeEdge::Top);
                    resizeHandle(id_ + ".rs.tr", w - m, 0.0f, m, m, eui::window::ResizeEdge::TopRight);
                    resizeHandle(id_ + ".rs.r", w - m, m, m, h - 2.0f * m, eui::window::ResizeEdge::Right);
                    resizeHandle(id_ + ".rs.br", w - m, h - m, m, m, eui::window::ResizeEdge::BottomRight);
                    resizeHandle(id_ + ".rs.b", m, h - m, w - 2.0f * m, m, eui::window::ResizeEdge::Bottom);
                    resizeHandle(id_ + ".rs.bl", 0.0f, h - m, m, m, eui::window::ResizeEdge::BottomLeft);
                    resizeHandle(id_ + ".rs.l", 0.0f, m, m, h - 2.0f * m, eui::window::ResizeEdge::Left);
                }
            })
            .build();
    }

private:
    void controlButton(const std::string& buttonId,
                       unsigned int codepoint,
                       float buttonSize,
                       const core::Color& normal,
                       const core::Color& hover,
                       const std::function<void()>& action) {
        components::button(ui_, buttonId)
            .size(buttonSize, buttonSize)
            .text("") // 清掉默认 "Button" 文字,只保留图标
            .icon(codepoint)
            .iconSize(buttonSize * 0.46f)
            .colors(normal, hover, hover)
            .iconColor(titleColor_)
            .radius(buttonSize * 0.22f)
            .shadow(0.0f, 0.0f, 0.0f, core::Color{0.0f, 0.0f, 0.0f, 0.0f})
            .onClick(action)
            .build();
    }

    void resizeHandle(const std::string& handleId,
                      float x,
                      float y,
                      float width,
                      float height,
                      eui::window::ResizeEdge edge) {
        components::mouseArea(ui_, handleId)
            .position(x, y)
            .size(width, height)
            .zIndex(100)
            .color(core::Color{0.0f, 0.0f, 0.0f, 0.0f})
            .onPress([edge](const MouseEvent&) { app::startWindowResize(edge); })
            .build();
    }

    core::dsl::Ui& ui_;
    std::string id_;
    float width_ = 0.0f;
    float height_ = 0.0f;
    float titleHeight_ = 0.0f;
    float fontSize_ = 0.0f;
    float resizeMargin_ = 6.0f;
    std::string title_;
    core::Color background_ = {0.12f, 0.13f, 0.15f, 1.0f};
    core::Color titleColor_ = {0.92f, 0.94f, 0.97f, 1.0f};
    core::Color buttonHover_ = {1.0f, 1.0f, 1.0f, 0.12f};
    core::Color closeHover_ = {0.90f, 0.25f, 0.23f, 1.0f};
    bool showMinimize_ = true;
    bool showMaximize_ = true;
    bool showClose_ = true;
    bool draggable_ = true;
    bool resizable_ = true;
    std::function<void()> onMinimize_;
    std::function<void()> onMaximize_;
    std::function<void()> onClose_;
    std::function<void()> onDrag_;
    std::function<void()> content_;
};

inline TitleBarBuilder titlebar(core::dsl::Ui& ui, const std::string& id) {
    return TitleBarBuilder(ui, id);
}

} // namespace components
