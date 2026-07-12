#pragma once

#include "components/button.h"
#include "components/mousearea.h"
#include "components/theme.h"
#include "core/dsl.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace components::workshop {

struct CardSliderItem {
    std::string source;
    std::string title;
    std::string subtitle;
    std::string description;
};

struct CardSliderStyle {
    CardSliderStyle() : CardSliderStyle(theme::dark()) {}

    explicit CardSliderStyle(const theme::ThemeColorTokens& tokens) {
        background = tokens.background;
        overlay = theme::color(0.0f, 0.0f, 0.0f, tokens.dark ? 0.74f : 0.58f);
        title = theme::color(1.0f, 1.0f, 1.0f, 0.98f);
        subtitle = theme::color(1.0f, 1.0f, 1.0f, 0.80f);
        description = theme::color(1.0f, 1.0f, 1.0f, 0.70f);
        accent = tokens.primary;
        shadow = theme::color(0.0f, 0.0f, 0.0f, 0.30f);
        button = ButtonStyle(tokens, false);
        button.text = tokens.text;
        button.icon = tokens.text;
        button.radius = 12.0f;
    }

    core::Color background;
    core::Color overlay;
    core::Color title;
    core::Color subtitle;
    core::Color description;
    core::Color accent;
    core::Color shadow;
    ButtonStyle button;
    float radius = 8.0f;
};

class CardSliderBuilder {
public:
    CardSliderBuilder(core::dsl::Ui& ui, std::string id)
        : ui_(ui), id_(std::move(id)) {}

    CardSliderBuilder& size(float width, float height) { width_ = width; height_ = height; return *this; }
    CardSliderBuilder& items(std::vector<CardSliderItem> value) { items_ = std::move(value); return *this; }
    CardSliderBuilder& currentIndex(int value) { requestedIndex_ = value; hasRequestedIndex_ = true; return *this; }
    CardSliderBuilder& duration(float seconds) { duration_ = std::max(0.05f, seconds); return *this; }
    CardSliderBuilder& interval(float seconds) { interval_ = std::max(0.10f, seconds); return *this; }
    CardSliderBuilder& cardSpacing(float value) { cardSpacing_ = std::max(0.0f, value); return *this; }
    CardSliderBuilder& autoPlay(bool value = true) { autoPlay_ = value; return *this; }
    CardSliderBuilder& background(bool value = true) { backgroundEnabled_ = value; return *this; }
    CardSliderBuilder& tilt(bool value = true) { tiltEnabled_ = value; return *this; }
    CardSliderBuilder& style(const CardSliderStyle& value) { style_ = value; return *this; }
    CardSliderBuilder& theme(const theme::ThemeColorTokens& tokens) { style_ = CardSliderStyle(tokens); return *this; }
    CardSliderBuilder& onChange(std::function<void(int)> callback) { onChange_ = std::move(callback); return *this; }

    void build() {
        const int count = static_cast<int>(items_.size());
        const float safeWidth = std::max(1.0f, width_);
        const float safeHeight = std::max(1.0f, height_);
        CardSliderState& state = ui_.state<CardSliderState>(id_);
        initializeState(state, count);

        state.duration = duration_;
        state.interval = interval_;
        state.autoPlay = autoPlay_;
        state.backgroundEnabled = backgroundEnabled_;
        state.tiltEnabled = tiltEnabled_;
        if (!tiltEnabled_) {
            state.targetHoverAngle = 0.0f;
        }

        if (count > 0 && hasRequestedIndex_) {
            const int requested = wrapIndex(requestedIndex_, count);
            if (requested != state.current && !state.animating) {
                startSlide(state, requested, shortestDirection(state.current, requested, count), count);
            }
        }

        const core::Transition motion = core::Transition::make(0.10f, core::Ease::OutCubic)
            .animate(core::AnimProperty::Opacity | core::AnimProperty::Transform | core::AnimProperty::Frame);
        const std::function<void(int)> onChange = onChange_;
        CardSliderState* runtimeState = &state;
        const float cardSpacing = cardSpacing_;

        ui_.stack(id_)
            .size(safeWidth, safeHeight)
            .clip()
            .content([&] {
                drawBackground(state, safeWidth, safeHeight);

                if (count == 0) {
                    drawEmpty(safeWidth, safeHeight);
                    return;
                }

                if (needsFrameTick(state)) {
                    ui_.stack(id_ + ".ticker")
                        .size(1.0f, 1.0f)
                        .onFrame([runtimeState](float deltaSeconds) {
                            tick(*runtimeState, deltaSeconds);
                        })
                        .build();
                }

                if (state.autoPlay) {
                    ui_.stack(id_ + ".autoplay")
                        .size(1.0f, 1.0f)
                        .onTimer(state.interval, [runtimeState, count, onChange] {
                            if (!runtimeState->animating && count > 1) {
                                startSlide(*runtimeState, wrapIndex(runtimeState->current + 1, count), 1, count);
                                emitChange(*runtimeState, onChange);
                            }
                        })
                        .build();
                }

                const auto scroll = [runtimeState, count, onChange](const MouseScrollEvent& event) {
                    if (count <= 1 || runtimeState->animating || event.stepY == 0.0f) {
                        return;
                    }
                    const int direction = event.stepY < 0.0f ? 1 : -1;
                    startSlide(*runtimeState, wrapIndex(runtimeState->current + direction, count), direction, count);
                    emitChange(*runtimeState, onChange);
                };

                mouseArea(ui_, id_ + ".input")
                    .size(safeWidth, safeHeight)
                    .zIndex(900)
                    .scrollStep(1.0f)
                    .maxScrollStep(1.0f)
                    .onMove([runtimeState, safeWidth, safeHeight, cardSpacing](const MouseEvent& event) {
                        updateHoverTarget(*runtimeState, event.x, safeWidth, safeHeight, cardSpacing);
                    })
                    .onLeave([runtimeState] {
                        runtimeState->targetHoverAngle = 0.0f;
                    })
                    .onScroll(scroll)
                    .build();

                std::vector<VisibleCard> cards = visibleCards(state, count);
                std::stable_sort(cards.begin(), cards.end(), [](const VisibleCard& left, const VisibleCard& right) {
                    return cardZ(left.slot) < cardZ(right.slot);
                });
                for (const VisibleCard& card : cards) {
                    drawCard(card, state, safeWidth, safeHeight, motion, onChange);
                }

                drawInfo(state, safeWidth, safeHeight, motion);
            })
            .build();
    }

private:
    struct CardSliderState {
        int current = 0;
        int from = 0;
        int to = 0;
        int direction = 1;
        int lastDirection = 1;
        float linearProgress = 1.0f;
        float progress = 1.0f;
        float hoverAngle = 0.0f;
        float targetHoverAngle = 0.0f;
        float duration = 0.8f;
        float interval = 2.0f;
        bool animating = false;
        bool initialized = false;
        bool autoPlay = false;
        bool backgroundEnabled = true;
        bool tiltEnabled = true;
    };

    struct CardRect {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
    };

    struct CardVisual {
        float scale = 1.0f;
        float opacity = 1.0f;
        float rotation = 0.0f;
    };

    struct VisibleCard {
        float slot = 0.0f;
        int index = 0;
        float opacityFactor = 1.0f;
    };

    static constexpr float kPi = 3.14159265358979323846f;
    static constexpr float kBaseItemWidth = 250.0f;
    static constexpr float kBaseItemHeight = 400.0f;

    static int wrapIndex(int index, int count) {
        if (count <= 0) {
            return 0;
        }
        int wrapped = index % count;
        if (wrapped < 0) {
            wrapped += count;
        }
        return wrapped;
    }

    static int shortestDirection(int from, int to, int count) {
        if (count <= 1 || from == to) {
            return 1;
        }
        int step = (to - from) % count;
        if (step < 0) {
            step += count;
        }
        if (step > count / 2) {
            step -= count;
        }
        return step >= 0 ? 1 : -1;
    }

    static float easeOutCubic(float t) {
        t = std::clamp(t, 0.0f, 1.0f);
        return 1.0f - std::pow(1.0f - t, 3.0f);
    }

    static void initializeState(CardSliderState& state, int count) {
        if (!state.initialized) {
            state.current = count > 0 ? 0 : -1;
            state.from = state.current;
            state.to = state.current;
            state.initialized = true;
        }
        if (count <= 0) {
            state.current = state.from = state.to = -1;
            state.animating = false;
            state.progress = 1.0f;
            state.linearProgress = 1.0f;
            return;
        }
        state.current = wrapIndex(state.current, count);
        state.from = wrapIndex(state.from, count);
        state.to = wrapIndex(state.to, count);
    }

    static void startSlide(CardSliderState& state, int index, int direction, int count) {
        if (count <= 1 || index == state.current || state.animating) {
            return;
        }
        state.from = state.current;
        state.to = wrapIndex(index, count);
        state.direction = direction >= 0 ? 1 : -1;
        state.lastDirection = state.direction;
        state.current = state.to;
        state.linearProgress = 0.0f;
        state.progress = 0.0f;
        state.animating = true;
    }

    static void emitChange(const CardSliderState& state, const std::function<void(int)>& onChange) {
        if (onChange && state.current >= 0) {
            onChange(state.current);
        }
    }

    static void tick(CardSliderState& state, float deltaSeconds) {
        const float clampedDelta = std::clamp(deltaSeconds, 0.0f, 0.08f);
        if (state.animating) {
            state.linearProgress = std::min(1.0f, state.linearProgress + clampedDelta / std::max(0.05f, state.duration));
            state.progress = easeOutCubic(state.linearProgress);
            if (state.linearProgress >= 1.0f) {
                state.progress = 1.0f;
                state.from = state.to = state.current;
                state.animating = false;
            }
        }

        state.hoverAngle += (state.targetHoverAngle - state.hoverAngle) *
            (1.0f - std::exp(-16.0f * clampedDelta));
        if (std::fabs(state.hoverAngle - state.targetHoverAngle) < 0.0009f) {
            state.hoverAngle = state.targetHoverAngle;
        }
    }

    static bool needsFrameTick(const CardSliderState& state) {
        return state.animating || std::fabs(state.hoverAngle - state.targetHoverAngle) >= 0.0009f;
    }

    static void updateHoverTarget(CardSliderState& state,
                                  float pointerX,
                                  float width,
                                  float height,
                                  float cardSpacing) {
        if (!state.tiltEnabled) {
            state.targetHoverAngle = 0.0f;
            return;
        }
        const CardRect rect = cardRect(0.0f, width, height, cardSpacing);
        const float centerX = rect.x + rect.width * 0.5f;
        const float amount = std::clamp((pointerX - centerX) / std::max(1.0f, rect.width) * 2.0f, -1.0f, 1.0f);
        state.targetHoverAngle = amount * 12.0f * kPi / 180.0f;
    }

    static int sideIndex(const CardSliderState& state, int side, int count) {
        return wrapIndex(state.current + side, count);
    }

    static std::vector<VisibleCard> visibleCards(const CardSliderState& state, int count) {
        std::vector<VisibleCard> cards;
        if (count <= 0 || state.current < 0) {
            return cards;
        }
        if (count == 1) {
            cards.push_back({0.0f, state.current, 1.0f});
            return cards;
        }
        if (count == 2 && !state.animating) {
            const int side = -state.lastDirection;
            cards.push_back({0.0f, state.current, 1.0f});
            cards.push_back({static_cast<float>(side), sideIndex(state, side, count), 1.0f});
            return cards;
        }
        if (state.animating) {
            const float p = state.progress;
            const float d = static_cast<float>(state.direction);
            if (count == 2) {
                cards.push_back({-d * p, state.from, 1.0f});
                cards.push_back({d * (1.0f - p), state.to, 1.0f});
                return cards;
            }

            const int oldSide = wrapIndex(state.from - state.direction, count);
            const int newSide = wrapIndex(state.to + state.direction, count);
            cards.push_back({-d * p, state.from, 1.0f});
            cards.push_back({d * (1.0f - p), state.to, 1.0f});
            if (oldSide == newSide) {
                cards.push_back({-d + 2.0f * d * p, oldSide, 1.0f});
            } else {
                cards.push_back({-d, oldSide, 1.0f - p});
                cards.push_back({d, newSide, p});
            }
            return cards;
        }

        cards.push_back({0.0f, state.current, 1.0f});
        cards.push_back({-1.0f, sideIndex(state, -1, count), 1.0f});
        cards.push_back({1.0f, sideIndex(state, 1, count), 1.0f});
        return cards;
    }

    static CardRect cardRect(float slot, float width, float height, float cardSpacing) {
        const float scale = std::min({
            width / (kBaseItemWidth * 3.0f),
            std::max(1.0f, height - 56.0f) / (kBaseItemHeight * 1.25f),
            1.0f
        });
        const float cardW = std::round(kBaseItemWidth * scale);
        const float cardH = std::round(kBaseItemHeight * scale);
        const float cx = std::round(width * 0.5f);
        const float cy = std::round(height * 0.5f - 10.0f);
        return {
            std::round(cx - cardW * 0.5f + slot * (cardW * 1.10f + cardSpacing)),
            std::round(cy - cardH * 0.5f),
            cardW,
            cardH
        };
    }

    CardRect cardRect(float slot, float width, float height) const {
        return cardRect(slot, width, height, cardSpacing_);
    }

    static CardVisual cardVisual(float slot, float hoverAngle) {
        const float amount = std::min(1.0f, std::fabs(slot));
        return {
            1.20f + (0.90f - 1.20f) * amount,
            0.80f + (0.40f - 0.80f) * amount,
            (-25.0f * slot * kPi / 180.0f) + hoverAngle * (1.0f - amount)
        };
    }

    static int cardZ(float slot) {
        return 100 - static_cast<int>(std::fabs(slot) * 20.0f);
    }

    void drawBackground(const CardSliderState& state, float width, float height) {
        ui_.rect(id_ + ".base")
            .size(width, height)
            .color(style_.background)
            .build();

        if (!state.backgroundEnabled || state.current < 0 || items_.empty()) {
            return;
        }

        if (state.animating && state.from >= 0 && state.to >= 0) {
            drawBackgroundImage(".bg.from", state.from, 1.0f - state.progress, -0.25f * state.direction * state.progress, width, height);
            drawBackgroundImage(".bg.to", state.to, state.progress, 0.25f * state.direction * (1.0f - state.progress), width, height);
        } else {
            drawBackgroundImage(".bg.current", state.current, 1.0f, 0.0f, width, height);
        }

        ui_.rect(id_ + ".overlay")
            .size(width, height)
            .color(style_.overlay)
            .build();
    }

    void drawBackgroundImage(const std::string& suffix, int index, float opacity, float offsetFactor, float width, float height) {
        if (index < 0 || index >= static_cast<int>(items_.size()) || opacity <= 0.0f) {
            return;
        }
        const float bgW = width * 1.80f;
        const float bgH = height * 1.80f;
        ui_.image(id_ + suffix)
            .position((width - bgW) * 0.5f + offsetFactor * width, (height - bgH) * 0.5f)
            .size(bgW, bgH)
            .source(items_[static_cast<std::size_t>(index)].source)
            .cover()
            .opacity(opacity)
            .build();
    }

    void drawCard(const VisibleCard& visible,
                  CardSliderState& state,
                  float width,
                  float height,
                  const core::Transition& motion,
                  const std::function<void(int)>& onChange) {
        if (visible.index < 0 || visible.index >= static_cast<int>(items_.size())) {
            return;
        }

        const CardRect base = cardRect(visible.slot, width, height);
        const CardVisual visual = cardVisual(visible.slot, state.hoverAngle);
        const float cardW = base.width * visual.scale;
        const float cardH = base.height * visual.scale;
        const float x = base.x + (base.width - cardW) * 0.5f;
        const float y = base.y + (base.height - cardH) * 0.5f;
        const float opacity = std::clamp((visual.opacity + 0.2f * (1.0f - std::min(1.0f, std::fabs(visible.slot)))) * visible.opacityFactor, 0.0f, 1.0f);
        const std::string cardId = id_ + ".card." + std::to_string(visible.index);

        ui_.stack(cardId)
            .position(x, y)
            .size(cardW, cardH)
            .zIndex(cardZ(visible.slot))
            .opacity(opacity)
            .rotateY(visual.rotation)
            .perspective(620.0f)
            .transformOrigin(0.5f, 0.5f)
            .transition(motion)
            .animate(core::AnimProperty::Frame | core::AnimProperty::Opacity | core::AnimProperty::Transform)
            .content([&] {
                ui_.rect(cardId + ".shadow")
                    .position(0.0f, 8.0f)
                    .size(cardW, cardH)
                    .color(theme::color(0.0f, 0.0f, 0.0f, 0.0f))
                    .radius(style_.radius)
                    .shadow(30.0f, 0.0f, 10.0f, style_.shadow)
                    .build();

                ui_.image(cardId + ".image")
                    .size(cardW, cardH)
                    .source(items_[static_cast<std::size_t>(visible.index)].source)
                    .cover()
                    .radius(style_.radius)
                    .build();

                const float shadeAlpha = 0.58f * (1.0f - visual.opacity);
                if (shadeAlpha > 0.01f) {
                    ui_.rect(cardId + ".shade")
                        .size(cardW, cardH)
                        .color(theme::color(0.0f, 0.0f, 0.0f, shadeAlpha))
                        .radius(style_.radius)
                        .build();
                }

            })
            .build();
    }

    void drawInfo(const CardSliderState& state, float width, float height, const core::Transition& motion) {
        if (state.current < 0 || state.current >= static_cast<int>(items_.size())) {
            return;
        }

        const float p = state.animating ? state.progress : 1.0f;
        const float oldOpacity = 1.0f - std::min(1.0f, p * 1.8f);
        const float newOpacity = std::min(1.0f, std::max(0.0f, (p - 0.35f) / 0.65f));
        if (state.animating && state.from >= 0 && oldOpacity > 0.0f) {
            drawInfoBlock(".info.old", items_[static_cast<std::size_t>(state.from)], oldOpacity, -120.0f * p, state.hoverAngle, width, height, motion);
        }
        drawInfoBlock(".info.current", items_[static_cast<std::size_t>(state.current)], newOpacity, 40.0f * (1.0f - newOpacity), state.hoverAngle, width, height, motion);
    }

    void drawInfoBlock(const std::string& suffix,
                       const CardSliderItem& item,
                       float opacity,
                       float offsetY,
                       float hoverAngle,
                       float width,
                       float height,
                       const core::Transition& motion) {
        if (opacity <= 0.0f) {
            return;
        }

        const CardRect rect = cardRect(0.0f, width, height);
        const float ratio = rect.width / kBaseItemWidth;
        const float infoW = std::max(160.0f, rect.width * 1.55f);
        const float baseX = width * 0.5f - rect.width * 1.5f + rect.width / 1.5f;
        const float baseY = rect.y + rect.height - rect.height / 8.0f + offsetY;
        const std::string rootId = id_ + suffix;

        ui_.stack(rootId)
            .position(baseX, baseY - 96.0f * ratio)
            .size(infoW, 150.0f * ratio)
            .zIndex(250)
            .opacity(opacity)
            .rotateY(hoverAngle * 0.35f)
            .perspective(620.0f)
            .transformOrigin(0.0f, 0.5f)
            .transition(motion)
            .animate(core::AnimProperty::Frame | core::AnimProperty::Opacity | core::AnimProperty::Transform)
            .content([&] {
                ui_.text(rootId + ".title")
                    .size(infoW, 46.0f * ratio)
                    .text(upperAscii(item.title))
                    .fontSize(std::max(18.0f, rect.width * 0.18f))
                    .lineHeight(std::max(22.0f, rect.width * 0.20f))
                    .fontWeight(860)
                    .color(style_.title)
                    .build();

                ui_.rect(rootId + ".mark.a")
                    .position(0.0f, 54.0f * ratio)
                    .size(20.0f * ratio, 5.0f * ratio)
                    .color(style_.title)
                    .radius(2.0f * ratio)
                    .build();
                ui_.rect(rootId + ".mark.b")
                    .position(0.0f, 94.0f * ratio)
                    .size(60.0f * ratio, 2.0f * ratio)
                    .color(style_.title)
                    .radius(1.0f * ratio)
                    .build();

                ui_.text(rootId + ".subtitle")
                    .position(40.0f * ratio, 38.0f * ratio)
                    .size(std::max(0.0f, infoW - 40.0f * ratio), 34.0f * ratio)
                    .text(upperAscii(item.subtitle))
                    .fontSize(std::max(13.0f, rect.width * 0.12f))
                    .lineHeight(std::max(17.0f, rect.width * 0.14f))
                    .fontWeight(760)
                    .color(style_.subtitle)
                    .build();

                ui_.text(rootId + ".description")
                    .position(0.0f, 88.0f * ratio)
                    .size(infoW, 42.0f * ratio)
                    .text(item.description)
                    .fontSize(std::max(11.0f, rect.width * 0.062f))
                    .lineHeight(std::max(15.0f, rect.width * 0.082f))
                    .fontWeight(540)
                    .wrap(true)
                    .color(style_.description)
                    .build();
            })
            .build();
    }

    void drawEmpty(float width, float height) {
        ui_.text(id_ + ".empty")
            .size(width, height)
            .text("No cards")
            .fontSize(18.0f)
            .lineHeight(24.0f)
            .color(style_.description)
            .horizontalAlign(core::HorizontalAlign::Center)
            .verticalAlign(core::VerticalAlign::Center)
            .build();
    }

    static std::string upperAscii(std::string value) {
        for (char& ch : value) {
            if (ch >= 'a' && ch <= 'z') {
                ch = static_cast<char>(ch - 'a' + 'A');
            }
        }
        return value;
    }

    core::dsl::Ui& ui_;
    std::string id_;
    std::vector<CardSliderItem> items_;
    CardSliderStyle style_;
    std::function<void(int)> onChange_;
    float width_ = 900.0f;
    float height_ = 560.0f;
    float duration_ = 0.8f;
    float interval_ = 2.0f;
    float cardSpacing_ = 0.0f;
    int requestedIndex_ = 0;
    bool hasRequestedIndex_ = false;
    bool autoPlay_ = false;
    bool backgroundEnabled_ = true;
    bool tiltEnabled_ = true;
};

inline CardSliderBuilder cardSlider(core::dsl::Ui& ui, const std::string& id) {
    return CardSliderBuilder(ui, id);
}

} // namespace components::workshop
