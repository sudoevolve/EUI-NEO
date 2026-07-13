#pragma once

#include "components/scroll.h"
#include "core/dsl.h"
#include "eui/signal.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>

namespace components {

class VirtualListBuilder {
public:
    using RowCompose = std::function<void(core::dsl::Ui&, const std::string&, std::int64_t, float, float)>;

    VirtualListBuilder(core::dsl::Ui& ui, std::string id)
        : ui_(ui), id_(std::move(id)) {}

    VirtualListBuilder& x(float value) { x_ = value; hasX_ = true; return *this; }
    VirtualListBuilder& y(float value) { y_ = value; hasY_ = true; return *this; }
    VirtualListBuilder& position(float xValue, float yValue) {
        x_ = xValue;
        y_ = yValue;
        hasX_ = true;
        hasY_ = true;
        return *this;
    }
    VirtualListBuilder& size(float width, float height) { width_ = width; height_ = height; return *this; }
    VirtualListBuilder& itemCount(std::int64_t value) { itemCount_ = std::max<std::int64_t>(0, value); return *this; }
    VirtualListBuilder& rowHeight(float value) { rowHeight_ = std::max(1.0f, value); return *this; }
    VirtualListBuilder& offset(float value) { offset_ = std::max(0.0f, value); return *this; }
    VirtualListBuilder& bind(eui::Signal<float>& signal) {
        offset(signal.get());
        onChange([&signal](float value) { signal.set(value); });
        return *this;
    }
    VirtualListBuilder& step(float value) { step_ = std::max(1.0f, value); return *this; }
    VirtualListBuilder& overscanViewports(float value) { overscanViewports_ = std::max(0.0f, value); return *this; }
    VirtualListBuilder& scrollbarWidth(float value) { scrollbarWidth_ = std::max(0.0f, value); return *this; }
    VirtualListBuilder& scrollbarGap(float value) { scrollbarGap_ = std::max(0.0f, value); return *this; }
    VirtualListBuilder& zIndex(int value) { zIndex_ = value; return *this; }
    VirtualListBuilder& style(const ScrollStyle& value) { scrollStyle_ = value; return *this; }
    VirtualListBuilder& theme(const theme::ThemeColorTokens& tokens) {
        scrollStyle_ = ScrollStyle(tokens);
        metrics_ = tokens.metrics;
        tokens_ = tokens;
        return *this;
    }
    VirtualListBuilder& transition(const core::Transition& value) { transition_ = value; return *this; }
    VirtualListBuilder& transition(float duration, core::Ease ease = core::Ease::OutCubic) {
        transition_ = core::Transition::make(duration, ease);
        return *this;
    }
    VirtualListBuilder& onChange(std::function<void(float)> callback) {
        onChange_ = std::move(callback);
        return *this;
    }
    VirtualListBuilder& row(RowCompose compose) {
        row_ = std::move(compose);
        return *this;
    }

    void build() {
        const float viewportWidth = std::max(0.0f, width_);
        const float viewportHeight = std::max(0.0f, height_);
        const float rowHeight = std::max(1.0f, rowHeight_);
        const double totalHeightValue = static_cast<double>(itemCount_) * static_cast<double>(rowHeight);
        const float totalHeight = static_cast<float>(totalHeightValue);
        const float maxOffset = std::max(0.0f, totalHeight - viewportHeight);
        const float currentOffset = std::clamp(offset_, 0.0f, maxOffset);
        const bool scrollable = maxOffset > 0.0f;
        const float scrollbarWidth = scrollbarWidth_ >= 0.0f ? scrollbarWidth_ : metrics_.control.scrollbar;
        const float scrollbarGap = scrollbarGap_ >= 0.0f ? scrollbarGap_ : metrics_.spacing.section;
        const float scrollWidth = scrollable ? scrollbarWidth : 0.0f;
        const float scrollGap = scrollable ? scrollbarGap : 0.0f;
        const float contentWidth = std::max(0.0f, viewportWidth - scrollWidth - scrollGap);
        const std::function<void(float)> onChange = onChange_;
        const float scrollStep = step_ > 0.0f ? step_ : metrics_.spacing.overlay;

        const float overscanPixels = viewportHeight * overscanViewports_;
        const double firstPixel = std::max(0.0, static_cast<double>(currentOffset) - static_cast<double>(overscanPixels));
        const double lastPixel = std::min(totalHeightValue,
                                          static_cast<double>(currentOffset) +
                                              static_cast<double>(viewportHeight) +
                                              static_cast<double>(overscanPixels));
        const std::int64_t firstIndex = itemCount_ > 0
            ? std::clamp<std::int64_t>(
                  static_cast<std::int64_t>(std::floor(firstPixel / static_cast<double>(rowHeight))),
                  0,
                  itemCount_ - 1)
            : 0;
        const std::int64_t lastIndex = itemCount_ > 0
            ? std::clamp<std::int64_t>(
                  static_cast<std::int64_t>(std::ceil(lastPixel / static_cast<double>(rowHeight))) + 1,
                  firstIndex,
                  itemCount_)
            : 0;
        auto root = ui_.stack(id_)
            .size(viewportWidth, viewportHeight)
            .zIndex(zIndex_)
            .clip()
            .scrollState(id_, currentOffset, maxOffset, scrollStep)
            .composeOnScrollOffsetChangeInterval(rowHeight)
            .onScrollOffsetChanged([onChange](float value) {
                if (onChange) {
                    onChange(value);
                }
            });
        if (hasX_) {
            root.x(x_);
        }
        if (hasY_) {
            root.y(y_);
        }
        root.content([&] {
                ui_.stack(id_ + ".window")
                    .size(contentWidth, viewportHeight)
                    .translateY(currentOffset)
                    .scrollContentFrom(id_)
                    .dirtyKey(id_ + ".virtual")
                    .content([&] {
                        if (!row_) {
                            return;
                        }
                        const double offset = static_cast<double>(currentOffset);
                        std::int64_t slot = 0;
                        for (std::int64_t index = firstIndex; index < lastIndex; ++index) {
                            const std::string rowId = id_ + ".slot." + std::to_string(slot);
                            const double absoluteY = static_cast<double>(index) * static_cast<double>(rowHeight);
                            const float rowY = static_cast<float>(absoluteY - offset);
                            ui_.stack(rowId)
                                .y(rowY)
                                .size(contentWidth, rowHeight)
                                .content([&] {
                                    row_(ui_, rowId, index, contentWidth, rowHeight);
                                })
                                .build();
                            ++slot;
                        }
                    })
                    .build();

                if (scrollable) {
                    components::scroll(ui_, id_ + ".scroll")
                        .theme(tokens_)
                        .style(scrollStyle_)
                        .scrollStateId(id_)
                        .x(std::max(0.0f, viewportWidth - scrollWidth))
                        .size(scrollWidth, viewportHeight)
                        .viewport(viewportHeight)
                        .content(totalHeight)
                        .offset(currentOffset)
                        .step(scrollStep)
                        .zIndex(zIndex_ + 1)
                        .transition(transition_)
                        .build();
                }
            })
            .build();
    }

private:
    core::dsl::Ui& ui_;
    std::string id_;
    ScrollStyle scrollStyle_;
    theme::ThemeMetricTokens metrics_;
    theme::ThemeColorTokens tokens_ = theme::dark();
    core::Transition transition_ = core::Transition::make(0.12f, core::Ease::OutCubic);
    std::function<void(float)> onChange_;
    RowCompose row_;
    std::int64_t itemCount_ = 0;
    float width_ = 320.0f;
    float height_ = 220.0f;
    float rowHeight_ = 36.0f;
    float offset_ = 0.0f;
    float step_ = 0.0f;
    float overscanViewports_ = 1.0f;
    float scrollbarWidth_ = -1.0f;
    float scrollbarGap_ = -1.0f;
    float x_ = 0.0f;
    float y_ = 0.0f;
    bool hasX_ = false;
    bool hasY_ = false;
    int zIndex_ = 0;
};

inline VirtualListBuilder virtualList(core::dsl::Ui& ui, const std::string& id) {
    return VirtualListBuilder(ui, id);
}

} // namespace components
