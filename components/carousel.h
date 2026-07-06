#pragma once

#include "components/mousearea.h"
#include "components/theme.h"
#include "core/dsl.h"
#include "eui/image.h"
#include "eui/signal.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace components {

struct CarouselItem {
    std::string source;
    std::string title;
    std::string subtitle;
};

struct CarouselStyle {
    CarouselStyle() : CarouselStyle(theme::dark()) {}

    explicit CarouselStyle(const theme::ThemeColorTokens& tokens) {
        background = tokens.surface;
        border = theme::withOpacity(tokens.border, 0.62f);
        text = theme::withAlpha(tokens.text, 0.96f);
        mutedText = theme::withAlpha(tokens.text, 0.66f);
        overlayTop = theme::color(0.0f, 0.0f, 0.0f, 0.0f);
        overlayBottom = theme::color(0.0f, 0.0f, 0.0f, tokens.dark ? 0.60f : 0.46f);
        indicator = theme::withAlpha(tokens.text, 0.28f);
        activeIndicator = tokens.primary;
        shadow = theme::shadow(tokens, 26.0f, 10.0f, 0.28f, 0.14f);
    }

    core::Color background;
    core::Color border;
    core::Color text;
    core::Color mutedText;
    core::Color overlayTop;
    core::Color overlayBottom;
    core::Color indicator;
    core::Color activeIndicator;
    core::Shadow shadow;
    float radius = 22.0f;
};

class CarouselBuilder {
public:
    CarouselBuilder(core::dsl::Ui& ui, std::string id)
        : ui_(ui), id_(std::move(id)) {}

    CarouselBuilder& size(float width, float height) { width_ = width; height_ = height; return *this; }
    CarouselBuilder& items(std::vector<CarouselItem> value) { items_ = std::move(value); return *this; }
    CarouselBuilder& index(float value) { index_ = value; return *this; }
    CarouselBuilder& bind(eui::Signal<float>& signal) {
        index(signal.get());
        onChange([&signal](float value) { signal.set(value); });
        return *this;
    }
    CarouselBuilder& cardWidthRatio(float value) { cardWidthRatio_ = std::clamp(value, 0.32f, 1.0f); return *this; }
    CarouselBuilder& overlap(float value) { overlap_ = std::clamp(value, 0.0f, 0.60f); return *this; }
    CarouselBuilder& parallax(float value) { parallax_ = std::max(0.0f, value); return *this; }
    CarouselBuilder& style(const CarouselStyle& value) { style_ = value; return *this; }
    CarouselBuilder& theme(const theme::ThemeColorTokens& tokens) { style_ = CarouselStyle(tokens); return *this; }
    CarouselBuilder& transition(const core::Transition& value) { transition_ = value; return *this; }
    CarouselBuilder& transition(float duration, core::Ease ease = core::Ease::OutCubic) {
        transition_ = core::Transition::make(duration, ease);
        return *this;
    }
    CarouselBuilder& onChange(std::function<void(float)> callback) { onChange_ = std::move(callback); return *this; }

    void build() {
        const int count = static_cast<int>(items_.size());
        const float safeWidth = std::max(1.0f, width_);
        const float safeHeight = std::max(1.0f, height_);
        CarouselState& state = ui_.state<CarouselState>(id_);
        const float inputIndex = clampIndex(index_, count);
        if (!state.initialized) {
            state.displayIndex = inputIndex;
            state.targetIndex = inputIndex;
            state.boundIndex = inputIndex;
            state.initialized = true;
        } else if (shouldEmitIndex(state.boundIndex, inputIndex)) {
            state.targetIndex = inputIndex;
            state.boundIndex = inputIndex;
        }
        state.displayIndex = clampIndex(state.displayIndex, count);
        state.targetIndex = clampIndex(state.targetIndex, count);
        const float center = state.displayIndex;
        const float cardWidth = std::clamp(safeWidth * cardWidthRatio_, std::min(260.0f, safeWidth), safeWidth);
        const float sideWidthRatio = std::clamp(kSideCardWidthRatio + (0.36f - overlap_) * 0.35f, 0.30f, 0.52f);
        const float sideCardWidth = std::clamp(cardWidth * sideWidthRatio, std::min(150.0f, cardWidth), cardWidth);
        const float cardHeight = safeHeight;
        const float sideGap = std::clamp(parallax_, 0.0f, cardWidth * 0.18f);
        const float sideStep = (cardWidth + sideCardWidth) * 0.5f + sideGap;
        float activeCenterX = safeWidth * 0.5f;
        if (count > 1) {
            const float stageLeft = std::max(0.0f, (safeWidth - cardWidth - sideCardWidth - sideGap) * 0.5f);
            const float firstCenterX = stageLeft + cardWidth * 0.5f;
            const float lastCenterX = safeWidth - stageLeft - cardWidth * 0.5f;
            const float lastBlend = smoothAmount(1.0f - std::clamp(static_cast<float>(count - 1) - center, 0.0f, 1.0f));
            activeCenterX = firstCenterX + (lastCenterX - firstCenterX) * lastBlend;
        }
        const float dragStep = std::max(80.0f, sideStep);
        const std::function<void(float)> onChange = onChange_;
        const core::Transition cardTransition = state.directMotion ? core::Transition::none() : transition_;
        const auto emitIndex = [&state, count, onChange](float next, bool directMotion) {
            if (count <= 1) {
                return;
            }
            const float clamped = clampIndex(next, count);
            state.targetIndex = clamped;
            state.directMotion = directMotion;
            if (!onChange || (state.hasEmittedIndex && !shouldEmitIndex(state.emittedIndex, clamped))) {
                return;
            }
            state.emittedIndex = clamped;
            state.hasEmittedIndex = true;
            onChange(clamped);
        };
        const auto scrollToIndex = [&state, emitIndex](const MouseScrollEvent& event) {
            if (event.stepY == 0.0f) {
                return;
            }
            emitIndex(state.targetIndex - event.stepY, false);
        };
        const auto beginDrag = [&state](const MouseEvent&) {
            state.dragStartIndex = state.targetIndex;
            state.directMotion = true;
            state.emittedIndex = state.targetIndex;
            state.hasEmittedIndex = true;
        };
        const auto dragToIndex = [&state, dragStep, emitIndex](const MouseDragEvent& event) {
            if (dragStep <= 0.0f) {
                return;
            }
            emitIndex(state.dragStartIndex - event.totalX / dragStep, true);
        };
        const auto endDrag = [&state](const MouseDragEvent&) {
            state.directMotion = false;
        };
        const auto jumpToIndex = [emitIndex](float next) {
            emitIndex(next, false);
        };
        const bool moving = shouldEmitIndex(state.displayIndex, state.targetIndex);

        ui_.stack(id_)
            .size(safeWidth, safeHeight)
            .clip()
            .content([&] {
                if (count == 0) {
                    drawEmptyState(safeWidth, safeHeight);
                    return;
                }

                if (moving) {
                    ui_.stack(id_ + ".ticker")
                        .size(1.0f, 1.0f)
                        .onFrame([&state](float deltaSeconds) {
                            advanceDisplayIndex(state, deltaSeconds);
                        })
                        .build();
                }

                mouseArea(ui_, id_ + ".input")
                    .size(safeWidth, safeHeight)
                    .zIndex(kRootInputZIndex)
                    .scrollStep(kScrollStep)
                    .maxScrollStep(1.0f)
                    .dragThreshold(kDragThreshold)
                    .onScroll(scrollToIndex)
                    .onDragStart(beginDrag)
                    .onDrag(dragToIndex)
                    .onDragEnd(endDrag)
                    .build();

                const int anchorIndex = static_cast<int>(std::floor(center));
                for (int offset = -1; offset <= 2; ++offset) {
                    const int itemIndex = anchorIndex + offset;
                    if (itemIndex < 0 || itemIndex >= count) {
                        continue;
                    }
                    drawCard(itemIndex, activeCenterX, 0.0f, cardWidth, sideCardWidth, cardHeight, sideStep, center, cardTransition, jumpToIndex, scrollToIndex, beginDrag, dragToIndex, endDrag);
                }

            })
            .build();
    }

private:
    static constexpr float kVisibleDistance = 1.45f;
    static constexpr float kDetailedDistance = 1.05f;
    static constexpr float kInteractiveDistance = 1.30f;
    static constexpr float kSideCardWidthRatio = 0.38f;
    static constexpr float kIndexEmitThreshold = 0.01f;
    static constexpr float kScrollStep = 0.18f;
    static constexpr float kDragThreshold = 3.0f;
    static constexpr int kRootInputZIndex = -1000;

    static float clampIndex(float value, int count) {
        if (count <= 0) {
            return 0.0f;
        }
        return std::clamp(value, 0.0f, static_cast<float>(count - 1));
    }

    static float smoothAmount(float value) {
        const float t = std::clamp(value, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    static bool shouldEmitIndex(float previous, float next) {
        return std::fabs(previous - next) >= kIndexEmitThreshold;
    }

    struct CarouselState {
        float dragStartIndex = 0.0f;
        float emittedIndex = 0.0f;
        float boundIndex = 0.0f;
        float targetIndex = 0.0f;
        float displayIndex = 0.0f;
        bool directMotion = false;
        bool hasEmittedIndex = false;
        bool initialized = false;
    };

    static void advanceDisplayIndex(CarouselState& state, float deltaSeconds) {
        const float delta = state.targetIndex - state.displayIndex;
        if (std::fabs(delta) < 0.001f) {
            state.displayIndex = state.targetIndex;
            state.directMotion = false;
            return;
        }
        const float speed = state.directMotion ? 36.0f : 18.0f;
        const float blend = 1.0f - std::exp(-speed * std::clamp(deltaSeconds, 0.0f, 0.08f));
        state.displayIndex += delta * blend;
    }

    void drawEmptyState(float width, float height) {
        ui_.rect(id_ + ".empty.bg")
            .size(width, height)
            .color(style_.background)
            .radius(style_.radius)
            .border(1.0f, style_.border)
            .build();

        ui_.text(id_ + ".empty.text")
            .size(width, height)
            .text("No items")
            .fontSize(18.0f)
            .lineHeight(22.0f)
            .color(style_.mutedText)
            .horizontalAlign(core::HorizontalAlign::Center)
            .verticalAlign(core::VerticalAlign::Center)
            .build();
    }

    void drawCard(int itemIndex,
                  float baseX,
                  float baseY,
                  float cardWidth,
                  float sideCardWidth,
                  float cardHeight,
                  float sideStep,
                  float center,
                  const core::Transition& cardTransition,
                  const std::function<void(float)>& onChange,
                  const std::function<void(const MouseScrollEvent&)>& onScroll,
                  const std::function<void(const MouseEvent&)>& onDragStart,
                  const std::function<void(const MouseDragEvent&)>& onDrag,
                  const std::function<void(const MouseDragEvent&)>& onDragEnd) {
        const CarouselItem& item = items_[static_cast<std::size_t>(itemIndex)];
        const float distance = static_cast<float>(itemIndex) - center;
        const float absDistance = std::fabs(distance);
        if (absDistance > kVisibleDistance) {
            return;
        }

        const float focus = 1.0f - std::clamp(absDistance, 0.0f, 1.0f);
        const float focusWidth = smoothAmount(focus);
        const float visualWidth = sideCardWidth + (cardWidth - sideCardWidth) * focusWidth;
        const float opacity = std::clamp(1.0f - absDistance * 0.16f, 0.48f, 1.0f);
        const float centerX = baseX + distance * sideStep;
        const float x = centerX - visualWidth * 0.5f;
        const float y = baseY;
        const float imageViewportX = std::max(0.0f, (cardWidth - visualWidth) * 0.5f);
        const float horizontalInset = std::clamp(visualWidth * 0.08f, 14.0f, 22.0f);
        const int z = 100 - static_cast<int>(std::round(absDistance * 10.0f));
        const bool active = absDistance < 0.55f;
        const bool detailed = absDistance <= kDetailedDistance;
        const bool interactive = absDistance <= kInteractiveDistance;
        const std::string cardId = id_ + ".card." + std::to_string(itemIndex);
        const bool imageReady = item.source.empty() || eui::image::isSourceReady(item.source);

        ui_.stack(cardId)
            .x(x)
            .y(y)
            .size(visualWidth, cardHeight)
            .zIndex(z)
            .clip()
            .opacity(opacity)
            .transition(cardTransition)
            .animate(core::AnimProperty::Frame | core::AnimProperty::Opacity)
            .content([&] {
                ui_.stack(cardId + ".image.layer")
                    .size(visualWidth, cardHeight)
                    .content([&] {
                        if (!imageReady) {
                            const float loaderSize = std::clamp(cardHeight * 0.22f, 34.0f, 64.0f);
                            ui_.image(cardId + ".loader")
                                .x((visualWidth - loaderSize) * 0.5f)
                                .y((cardHeight - loaderSize) * 0.5f)
                                .size(loaderSize, loaderSize)
                                .source("mona-loading-default.gif")
                                .contain()
                                .opacity(0.82f)
                                .build();
                        }

                        ui_.image(cardId + ".image")
                            .x(0.0f)
                            .y(0.0f)
                            .size(visualWidth, cardHeight)
                            .source(item.source)
                            .cover()
                            .coverViewport(cardWidth, cardHeight, imageViewportX)
                            .radius(style_.radius)
                            .transition(cardTransition)
                            .animate(core::AnimProperty::Frame)
                            .build();

                        ui_.rect(cardId + ".shade")
                            .size(visualWidth, cardHeight)
                            .gradient(style_.overlayTop, style_.overlayBottom, core::GradientDirection::Vertical)
                            .radius(style_.radius)
                            .opacity(active ? 1.0f : 0.82f)
                            .transition(cardTransition)
                            .animate(core::AnimProperty::Frame | core::AnimProperty::Opacity)
                            .build();
                    })
                    .build();

                if (detailed) {
                    ui_.text(cardId + ".title")
                        .x(horizontalInset)
                        .y(std::max(0.0f, cardHeight - 64.0f))
                        .size(std::max(0.0f, visualWidth - horizontalInset * 2.0f), 26.0f)
                        .text(item.title)
                        .fontSize(active ? 22.0f : 19.0f)
                        .lineHeight(24.0f)
                        .color(style_.text)
                        .build();

                    if (!item.subtitle.empty()) {
                        ui_.text(cardId + ".subtitle")
                            .x(horizontalInset)
                            .y(std::max(0.0f, cardHeight - 35.0f))
                            .size(std::max(0.0f, visualWidth - horizontalInset * 2.0f), 18.0f)
                            .text(item.subtitle)
                            .fontSize(14.0f)
                            .lineHeight(18.0f)
                            .color(style_.mutedText)
                            .build();
                    }
                }

                if (interactive) {
                    mouseArea(ui_, cardId + ".hit")
                        .size(visualWidth, cardHeight)
                        .zIndex(20)
                        .radius(style_.radius)
                        .scrollStep(kScrollStep)
                        .maxScrollStep(1.0f)
                        .dragThreshold(kDragThreshold)
                        .onTap([onChange, itemIndex] {
                            if (onChange) {
                                onChange(static_cast<float>(itemIndex));
                            }
                        })
                        .onScroll(onScroll)
                        .onDragStart(onDragStart)
                        .onDrag(onDrag)
                        .onDragEnd(onDragEnd)
                        .build();
                }
            })
            .build();
    }

    core::dsl::Ui& ui_;
    std::string id_;
    std::vector<CarouselItem> items_;
    CarouselStyle style_;
    core::Transition transition_ = core::Transition::make(0.24f, core::Ease::OutCubic);
    std::function<void(float)> onChange_;
    float width_ = 720.0f;
    float height_ = 260.0f;
    float index_ = 0.0f;
    float cardWidthRatio_ = 0.68f;
    float overlap_ = 0.36f;
    float parallax_ = 28.0f;
};

inline CarouselBuilder carousel(core::dsl::Ui& ui, const std::string& id) {
    return CarouselBuilder(ui, id);
}

} // namespace components
