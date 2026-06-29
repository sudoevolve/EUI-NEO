#pragma once

#include "components/scroll.h"
#include "core/dsl.h"
#include "eui/signal.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace components {

class ScrollViewBuilder {
public:
    ScrollViewBuilder(core::dsl::Ui& ui, std::string id)
        : ui_(ui), id_(std::move(id)) {}

    ScrollViewBuilder& x(float value) { x_ = value; hasX_ = true; return *this; }
    ScrollViewBuilder& y(float value) { y_ = value; hasY_ = true; return *this; }
    ScrollViewBuilder& position(float xValue, float yValue) {
        x_ = xValue;
        y_ = yValue;
        hasX_ = true;
        hasY_ = true;
        return *this;
    }
    ScrollViewBuilder& size(float width, float height) { width_ = width; height_ = height; return *this; }
    ScrollViewBuilder& offset(float value) { offset_ = std::max(0.0f, value); return *this; }
    ScrollViewBuilder& bind(eui::Signal<float>& signal) {
        offset(signal.get());
        onChange([&signal](float value) { signal.set(value); });
        return *this;
    }
    ScrollViewBuilder& gap(float value) { gap_ = std::max(0.0f, value); return *this; }
    ScrollViewBuilder& step(float value) { step_ = std::max(1.0f, value); return *this; }
    ScrollViewBuilder& scrollbarWidth(float value) { scrollbarWidth_ = std::max(0.0f, value); return *this; }
    ScrollViewBuilder& scrollbarGap(float value) { scrollbarGap_ = std::max(0.0f, value); return *this; }
    ScrollViewBuilder& zIndex(int value) { zIndex_ = value; return *this; }
    ScrollViewBuilder& style(const ScrollStyle& value) { scrollStyle_ = value; return *this; }
    ScrollViewBuilder& theme(const theme::ThemeColorTokens& tokens) { scrollStyle_ = ScrollStyle(tokens); return *this; }
    ScrollViewBuilder& contentKey(std::string value) { contentKey_ = std::move(value); return *this; }
    ScrollViewBuilder& transition(const core::Transition& value) { transition_ = value; return *this; }
    ScrollViewBuilder& transition(float duration, core::Ease ease = core::Ease::OutCubic) {
        transition_ = core::Transition::make(duration, ease);
        return *this;
    }
    ScrollViewBuilder& onChange(std::function<void(float)> callback) {
        onChange_ = std::move(callback);
        return *this;
    }

    template <typename ComposeFn>
    ScrollViewBuilder& content(ComposeFn&& compose) {
        content_ = std::forward<ComposeFn>(compose);
        return *this;
    }

    void build() {
        MeasureCache* cache = contentKey_.empty() ? nullptr : &ui_.state<MeasureCache>(id_ + ".measure");
        const float initialContentHeight = contentKey_.empty()
            ? measureContentHeight(width_, height_)
            : cachedContentHeight(*cache, width_, height_);
        const bool scrollable = initialContentHeight > height_;
        const float scrollWidth = scrollable ? scrollbarWidth_ : 0.0f;
        const float scrollGap = scrollable ? scrollbarGap_ : 0.0f;
        const float contentWidth = std::max(0.0f, width_ - scrollWidth - scrollGap);
        const float contentHeight = scrollable
            ? (contentKey_.empty() ? measureContentHeight(contentWidth, height_) : cachedContentHeight(*cache, contentWidth, height_))
            : initialContentHeight;
        const float maxOffset = std::max(0.0f, contentHeight - height_);
        const float currentOffset = std::clamp(offset_, 0.0f, maxOffset);
        const std::function<void(float)> onChange = onChange_;
        const float scrollStep = step_;

        auto root = ui_.stack(id_)
            .size(width_, height_)
            .zIndex(zIndex_)
            .clip()
            .scrollState(id_, currentOffset, maxOffset, scrollStep)
            .onScrollOffsetChanged(onChange);
        if (hasX_) {
            root.x(x_);
        }
        if (hasY_) {
            root.y(y_);
        }
        root.content([&] {
                ui_.column(id_ + ".content")
                    .width(contentWidth)
                    .height(core::SizeValue::wrapContent())
                    .gap(gap_)
                    .scrollContentFrom(id_)
                    .content([&] {
                        if (content_) {
                            content_(ui_, contentWidth, height_);
                        }
                    })
                    .build();

                if (scrollable) {
                    components::scroll(ui_, id_ + ".scroll")
                        .style(scrollStyle_)
                        .scrollStateId(id_)
                        .x(std::max(0.0f, width_ - scrollWidth))
                        .size(scrollWidth, height_)
                        .viewport(height_)
                        .content(contentHeight)
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
    struct MeasureCache {
        struct Entry {
            float width = -1.0f;
            float viewportHeight = -1.0f;
            float gap = -1.0f;
            float height = 0.0f;
            std::string contentKey;
        };

        std::vector<Entry> entries;
    };

    float cachedContentHeight(MeasureCache& cache, float contentWidth, float viewportHeight) const {
        for (const MeasureCache::Entry& entry : cache.entries) {
            if (closeEnough(entry.width, contentWidth) &&
                closeEnough(entry.viewportHeight, viewportHeight) &&
                closeEnough(entry.gap, gap_) &&
                entry.contentKey == contentKey_) {
                return entry.height;
            }
        }

        const float height = measureContentHeight(contentWidth, viewportHeight);
        if (cache.entries.size() >= 4) {
            cache.entries.erase(cache.entries.begin());
        }
        cache.entries.push_back({contentWidth, viewportHeight, gap_, height, contentKey_});
        return height;
    }

    static bool closeEnough(float left, float right) {
        return std::fabs(left - right) <= 0.5f;
    }

    float measureContentHeight(float contentWidth, float viewportHeight) const {
        if (!content_) {
            return viewportHeight;
        }

        core::dsl::Ui measureUi;
        measureUi.begin(id_ + ".measure");
        measureUi.column("content")
            .width(contentWidth)
            .height(core::SizeValue::wrapContent())
            .gap(gap_)
            .content([&] {
                content_(measureUi, contentWidth, viewportHeight);
            })
            .build();
        measureUi.end();
        measureUi.layout(contentWidth, 0.0f);
        const core::dsl::Element* content = measureUi.find("content");
        if (content == nullptr) {
            return viewportHeight;
        }
        return std::max(viewportHeight, content->frame.height);
    }

    core::dsl::Ui& ui_;
    std::string id_;
    ScrollStyle scrollStyle_;
    core::Transition transition_ = core::Transition::make(0.12f, core::Ease::OutCubic);
    std::function<void(float)> onChange_;
    std::function<void(core::dsl::Ui&, float, float)> content_;
    std::string contentKey_;
    float width_ = 320.0f;
    float height_ = 220.0f;
    float offset_ = 0.0f;
    float gap_ = 0.0f;
    float step_ = 48.0f;
    float scrollbarWidth_ = 8.0f;
    float scrollbarGap_ = 16.0f;
    float x_ = 0.0f;
    float y_ = 0.0f;
    bool hasX_ = false;
    bool hasY_ = false;
    int zIndex_ = 0;
};

inline ScrollViewBuilder scrollView(core::dsl::Ui& ui, const std::string& id) {
    return ScrollViewBuilder(ui, id);
}

} // namespace components
