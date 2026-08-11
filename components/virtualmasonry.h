#pragma once

#include "components/scroll.h"
#include "core/dsl.h"
#include "core/platform/platform.h"
#include "eui/signal.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace components {

class VirtualMasonryBuilder {
public:
    using ItemHeight = std::function<float(std::int64_t, float)>;
    using ItemCompose = std::function<void(core::dsl::Ui&, const std::string&, std::int64_t, float, float)>;

    VirtualMasonryBuilder(core::dsl::Ui& ui, std::string id)
        : ui_(ui), id_(std::move(id)) {}

    VirtualMasonryBuilder& x(float value) { x_ = value; hasX_ = true; return *this; }
    VirtualMasonryBuilder& y(float value) { y_ = value; hasY_ = true; return *this; }
    VirtualMasonryBuilder& position(float xValue, float yValue) {
        x_ = xValue;
        y_ = yValue;
        hasX_ = true;
        hasY_ = true;
        return *this;
    }
    VirtualMasonryBuilder& size(float width, float height) { width_ = width; height_ = height; return *this; }
    VirtualMasonryBuilder& itemCount(std::int64_t value) { itemCount_ = std::max<std::int64_t>(0, value); return *this; }
    VirtualMasonryBuilder& columns(int value) { columns_ = std::max(1, value); return *this; }
    VirtualMasonryBuilder& gap(float value) { gap_ = std::max(0.0f, value); return *this; }
    VirtualMasonryBuilder& offset(float value) { offset_ = std::max(0.0f, value); return *this; }
    VirtualMasonryBuilder& bind(eui::Signal<float>& signal) {
        offset(signal.get());
        onChange([&signal](float value) { signal.set(value); });
        return *this;
    }
    VirtualMasonryBuilder& step(float value) { step_ = std::max(1.0f, value); return *this; }
    VirtualMasonryBuilder& overscanViewports(float value) { overscanViewports_ = std::max(0.0f, value); return *this; }
    VirtualMasonryBuilder& endReachedThresholdViewports(float value) {
        endReachedThresholdViewports_ = std::max(0.0f, value);
        return *this;
    }
    VirtualMasonryBuilder& scrollbarWidth(float value) { scrollbarWidth_ = std::max(0.0f, value); return *this; }
    VirtualMasonryBuilder& scrollbarGap(float value) { scrollbarGap_ = std::max(0.0f, value); return *this; }
    VirtualMasonryBuilder& zIndex(int value) { zIndex_ = value; return *this; }
    VirtualMasonryBuilder& style(const ScrollStyle& value) { scrollStyle_ = value; return *this; }
    VirtualMasonryBuilder& theme(const theme::ThemeColorTokens& tokens) {
        scrollStyle_ = ScrollStyle(tokens);
        metrics_ = tokens.metrics;
        tokens_ = tokens;
        return *this;
    }
    VirtualMasonryBuilder& transition(const core::Transition& value) { transition_ = value; return *this; }
    VirtualMasonryBuilder& transition(float duration, core::Ease ease = core::Ease::OutCubic) {
        transition_ = core::Transition::make(duration, ease);
        return *this;
    }
    VirtualMasonryBuilder& onChange(std::function<void(float)> callback) {
        onChange_ = std::move(callback);
        return *this;
    }
    VirtualMasonryBuilder& onEndReached(std::function<void()> callback) {
        onEndReached_ = std::move(callback);
        return *this;
    }
    VirtualMasonryBuilder& itemHeight(ItemHeight callback) {
        itemHeight_ = std::move(callback);
        return *this;
    }
    VirtualMasonryBuilder& item(ItemCompose compose) {
        item_ = std::move(compose);
        return *this;
    }

    void build() {
        const float viewportWidth = std::max(0.0f, width_);
        const float viewportHeight = std::max(0.0f, height_);
        const float scrollbarWidth = scrollbarWidth_ >= 0.0f ? scrollbarWidth_ : metrics_.control.scrollbar;
        const float scrollbarGap = scrollbarGap_ >= 0.0f ? scrollbarGap_ : metrics_.spacing.section;

        Layout layout = makeLayout(viewportWidth);
        bool scrollable = layout.totalHeight > viewportHeight;
        const float scrollWidth = scrollable ? scrollbarWidth : 0.0f;
        const float scrollGap = scrollable ? scrollbarGap : 0.0f;
        float contentWidth = std::max(0.0f, viewportWidth - scrollWidth - scrollGap);
        if (scrollable && std::fabs(contentWidth - viewportWidth) > 0.01f) {
            layout = makeLayout(contentWidth);
            scrollable = layout.totalHeight > viewportHeight;
            if (!scrollable) {
                contentWidth = viewportWidth;
                layout = makeLayout(contentWidth);
            }
        }

        const float maxOffset = std::max(0.0f, layout.totalHeight - viewportHeight);
        const float currentOffset = std::clamp(offset_, 0.0f, maxOffset);
        const float scrollStep = step_ > 0.0f ? step_ : metrics_.spacing.overlay;
        const float overscan = viewportHeight * overscanViewports_;
        const float visibleTop = std::max(0.0f, currentOffset - overscan);
        const float visibleBottom = currentOffset + viewportHeight + overscan;
        const float endThreshold = viewportHeight * endReachedThresholdViewports_;
        const std::function<void(float)> onChange = onChange_;
        const std::function<void()> onEndReached = onEndReached_;
        std::int64_t& lastAutoEndCount = ui_.state<std::int64_t>(id_ + ".end.count");
        if (onEndReached && itemCount_ > 0 && maxOffset - currentOffset <= endThreshold &&
            lastAutoEndCount != itemCount_) {
            lastAutoEndCount = itemCount_;
            onEndReached();
        }

        auto root = ui_.stack(id_)
            .size(viewportWidth, viewportHeight)
            .zIndex(zIndex_)
            .clip()
            .scrollState(id_, currentOffset, maxOffset, scrollStep)
            .composeOnScrollOffsetChange()
            .onScrollOffsetChanged([onChange, onEndReached, maxOffset, endThreshold](float value) {
                if (onChange) {
                    onChange(value);
                }
                if (onEndReached && maxOffset - value <= endThreshold) {
                    onEndReached();
                }
                core::platform::requestUiUpdate();
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
                    .dirtyKey(id_ + ".virtual")
                    .content([&] {
                        if (!item_) {
                            return;
                        }
                        for (std::size_t index = 0; index < layout.items.size(); ++index) {
                            const ItemFrame& frame = layout.items[index];
                            if (frame.y + frame.height < visibleTop || frame.y > visibleBottom) {
                                continue;
                            }
                            const std::string itemId = id_ + ".item." + std::to_string(index);
                            ui_.stack(itemId)
                                .position(frame.x, frame.y - currentOffset)
                                .size(frame.width, frame.height)
                                .content([&] {
                                    item_(ui_, itemId, static_cast<std::int64_t>(index), frame.width, frame.height);
                                })
                                .build();
                        }
                    })
                    .build();

                if (scrollable) {
                    components::scroll(ui_, id_ + ".scroll")
                        .theme(tokens_)
                        .style(scrollStyle_)
                        .scrollStateId(id_)
                        .x(std::max(0.0f, viewportWidth - scrollbarWidth))
                        .size(scrollbarWidth, viewportHeight)
                        .viewport(viewportHeight)
                        .content(layout.totalHeight)
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
    struct ItemFrame {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
    };

    struct Layout {
        std::vector<ItemFrame> items;
        float totalHeight = 0.0f;
    };

    Layout makeLayout(float contentWidth) const {
        Layout layout;
        if (itemCount_ <= 0 || contentWidth <= 0.0f) {
            return layout;
        }

        const int columnCount = std::max(1, columns_);
        const float totalGap = gap_ * static_cast<float>(columnCount - 1);
        const float itemWidth = std::max(1.0f, (contentWidth - totalGap) / static_cast<float>(columnCount));
        std::vector<float> columnHeights(static_cast<std::size_t>(columnCount), 0.0f);
        layout.items.reserve(static_cast<std::size_t>(itemCount_));

        for (std::int64_t index = 0; index < itemCount_; ++index) {
            int column = 0;
            for (int candidate = 1; candidate < columnCount; ++candidate) {
                if (columnHeights[static_cast<std::size_t>(candidate)] < columnHeights[static_cast<std::size_t>(column)]) {
                    column = candidate;
                }
            }
            const float height = std::max(1.0f, itemHeight_ ? itemHeight_(index, itemWidth) : itemWidth);
            const float x = static_cast<float>(column) * (itemWidth + gap_);
            const float y = columnHeights[static_cast<std::size_t>(column)];
            layout.items.push_back({x, y, itemWidth, height});
            columnHeights[static_cast<std::size_t>(column)] = y + height + gap_;
        }

        for (float height : columnHeights) {
            layout.totalHeight = std::max(layout.totalHeight, height);
        }
        layout.totalHeight = std::max(0.0f, layout.totalHeight - gap_);
        return layout;
    }

    core::dsl::Ui& ui_;
    std::string id_;
    ScrollStyle scrollStyle_;
    theme::ThemeMetricTokens metrics_;
    theme::ThemeColorTokens tokens_ = theme::dark();
    core::Transition transition_ = core::Transition::make(0.12f, core::Ease::OutCubic);
    ItemHeight itemHeight_;
    ItemCompose item_;
    std::function<void(float)> onChange_;
    std::function<void()> onEndReached_;
    std::int64_t itemCount_ = 0;
    int columns_ = 1;
    float width_ = 320.0f;
    float height_ = 220.0f;
    float gap_ = 0.0f;
    float offset_ = 0.0f;
    float step_ = 0.0f;
    float overscanViewports_ = 1.0f;
    float endReachedThresholdViewports_ = 1.0f;
    float scrollbarWidth_ = -1.0f;
    float scrollbarGap_ = -1.0f;
    float x_ = 0.0f;
    float y_ = 0.0f;
    bool hasX_ = false;
    bool hasY_ = false;
    int zIndex_ = 0;
};

inline VirtualMasonryBuilder virtualMasonry(core::dsl::Ui& ui, const std::string& id) {
    return VirtualMasonryBuilder(ui, id);
}

} // namespace components
