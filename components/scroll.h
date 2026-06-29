#pragma once

#include "components/theme.h"
#include "core/dsl.h"

#include <algorithm>
#include <string>
#include <utility>

namespace components {

struct ScrollStyle {
    ScrollStyle() : ScrollStyle(theme::dark()) {}

    explicit ScrollStyle(const theme::ThemeColorTokens& tokens) {
        track = theme::withOpacity(tokens.surfaceHover, tokens.dark ? 0.34f : 0.46f);
        thumb = theme::withOpacity(tokens.text, tokens.dark ? 0.34f : 0.28f);
        thumbHover = theme::withOpacity(tokens.text, tokens.dark ? 0.46f : 0.38f);
        thumbPressed = theme::withOpacity(tokens.primary, 0.76f);
    }

    core::Color track;
    core::Color thumb;
    core::Color thumbHover;
    core::Color thumbPressed;
    float radius = 999.0f;
};

class ScrollBuilder {
public:
    ScrollBuilder(core::dsl::Ui& ui, std::string id)
        : ui_(ui), id_(std::move(id)) {}

    ScrollBuilder& x(float value) { x_ = value; hasX_ = true; return *this; }
    ScrollBuilder& y(float value) { y_ = value; hasY_ = true; return *this; }
    ScrollBuilder& position(float xValue, float yValue) {
        x_ = xValue;
        y_ = yValue;
        hasX_ = true;
        hasY_ = true;
        return *this;
    }
    ScrollBuilder& size(float width, float height) { width_ = width; height_ = height; return *this; }
    ScrollBuilder& scrollStateId(const std::string& id) { stateId_ = id; return *this; }
    ScrollBuilder& offset(float value) { offset_ = std::max(0.0f, value); return *this; }
    ScrollBuilder& viewport(float value) { viewport_ = std::max(0.0f, value); return *this; }
    ScrollBuilder& content(float value) { content_ = std::max(0.0f, value); return *this; }
    ScrollBuilder& step(float value) { step_ = std::max(1.0f, value); return *this; }
    ScrollBuilder& zIndex(int value) { zIndex_ = value; return *this; }
    ScrollBuilder& style(const ScrollStyle& value) { style_ = value; return *this; }
    ScrollBuilder& theme(const theme::ThemeColorTokens& tokens) { style_ = ScrollStyle(tokens); return *this; }
    ScrollBuilder& transition(const core::Transition& value) { transition_ = value; return *this; }
    ScrollBuilder& transition(float duration, core::Ease ease = core::Ease::OutCubic) {
        transition_ = core::Transition::make(duration, ease);
        return *this;
    }

    void build() {
        const float maxOffset = std::max(0.0f, content_ - viewport_);
        const bool scrollable = maxOffset > 0.0f && viewport_ > 0.0f && content_ > 0.0f;
        const float thumbHeight = scrollable
            ? std::clamp(height_ * (viewport_ / content_), std::min(height_, 24.0f), height_)
            : height_;
        const float travel = std::max(0.0f, height_ - thumbHeight);
        const float currentOffset = std::clamp(offset_, 0.0f, maxOffset);
        const float scrollStep = step_;
        const std::string runtimeStateId = stateId_.empty() ? id_ : stateId_;

        auto root = ui_.stack(id_)
            .size(width_, height_)
            .zIndex(zIndex_)
            .scrollState(runtimeStateId, currentOffset, maxOffset, scrollStep);
        if (hasX_) {
            root.x(x_);
        }
        if (hasY_) {
            root.y(y_);
        }
        root.content([&] {
                ui_.rect(id_ + ".track")
                    .size(width_, height_)
                    .color(style_.track)
                    .radius(style_.radius)
                    .scrollState(runtimeStateId, currentOffset, maxOffset, scrollStep)
                    .build();

                ui_.rect(id_ + ".thumb")
                    .size(width_, thumbHeight)
                    .states(style_.thumb, style_.thumbHover, style_.thumbPressed)
                    .radius(style_.radius)
                    .cursor(core::CursorShape::Hand)
                    .transition(transition_)
                    .animate(core::AnimProperty::Color)
                    .scrollDragFrom(runtimeStateId, travel)
                    .scrollThumbFrom(runtimeStateId, travel)
                    .transformedHitTest()
                    .build();
            })
            .build();
    }

private:
    core::dsl::Ui& ui_;
    std::string id_;
    std::string stateId_;
    ScrollStyle style_;
    core::Transition transition_ = core::Transition::make(0.12f, core::Ease::OutCubic);
    float width_ = 8.0f;
    float height_ = 180.0f;
    float viewport_ = 180.0f;
    float content_ = 180.0f;
    float offset_ = 0.0f;
    float step_ = 42.0f;
    float x_ = 0.0f;
    float y_ = 0.0f;
    bool hasX_ = false;
    bool hasY_ = false;
    int zIndex_ = 0;
};

inline ScrollBuilder scroll(core::dsl::Ui& ui, const std::string& id) {
    return ScrollBuilder(ui, id);
}

} // namespace components
