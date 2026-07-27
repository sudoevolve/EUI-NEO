#include "eui_neo.h"

#include <algorithm>

namespace app {

const DslAppConfig& dslAppConfig() {
    static const DslAppConfig config = DslAppConfig{}
        .title("Animated Card")
        .pageId("animated_card")
        .clearColor({0.055f, 0.062f, 0.075f, 1.0f})
        .windowSize(760, 560)
        .fps(90.0);
    return config;
}

namespace {

struct CardState {
    bool expanded = false;
};

} // namespace

void compose(eui::Ui& ui, const eui::Screen& screen) {
    CardState& state = ui.state<CardState>("card.state");
    const float cardWidth = std::max(280.0f, std::min(390.0f, screen.width - 48.0f));
    constexpr float expandedHeight = 238.0f;
    const float cardHeight = state.expanded ? 238.0f : 148.0f;
    const float cardTop = (expandedHeight - cardHeight) * 0.5f;
    const eui::Transition motion = eui::Transition::make(0.34f, eui::Ease::OutCubic);

    ui.stack("page")
        .size(screen.width, screen.height)
        .align(eui::Align::CENTER, eui::Align::CENTER)
        .content([&] {
            ui.stack("card")
                .size(cardWidth, expandedHeight)
                .translateY(state.expanded ? -10.0f : 0.0f)
                .scale(state.expanded ? 1.025f : 1.0f)
                .transition(motion)
                .animate(eui::AnimProperty::Transform)
                .onClick([&state] {
                    state.expanded = !state.expanded;
                })
                .content([&] {
                    ui.rect("card.background")
                        .position(0.0f, cardTop)
                        .size(cardWidth, cardHeight)
                        .radius(state.expanded ? 24.0f : 16.0f)
                        .color(state.expanded
                            ? eui::Color{0.10f, 0.42f, 0.66f, 1.0f}
                            : eui::Color{0.13f, 0.15f, 0.19f, 1.0f})
                        .border(1.0f, state.expanded
                            ? eui::Color{0.36f, 0.72f, 0.90f, 0.72f}
                            : eui::Color{0.34f, 0.38f, 0.45f, 0.58f})
                        .shadow(state.expanded ? 38.0f : 22.0f,
                                0.0f,
                                state.expanded ? 16.0f : 8.0f,
                                {0.0f, 0.0f, 0.0f, state.expanded ? 0.38f : 0.26f})
                        .transition(motion)
                        .animate(eui::AnimProperty::Color |
                                 eui::AnimProperty::Frame |
                                 eui::AnimProperty::Border |
                                 eui::AnimProperty::Radius |
                                 eui::AnimProperty::Shadow)
                        .build();

                    ui.rect("card.accent")
                        .position(24.0f, cardTop + 24.0f)
                        .size(state.expanded ? 54.0f : 38.0f, 4.0f)
                        .radius(2.0f)
                        .color({0.62f, 0.90f, 1.0f, 1.0f})
                        .transition(motion)
                        .animate(eui::AnimProperty::Frame)
                        .build();

                    ui.text("card.title")
                        .position(24.0f, cardTop + 44.0f)
                        .size(cardWidth - 48.0f, 34.0f)
                        .text("Motion Card")
                        .fontSize(25.0f)
                        .lineHeight(32.0f)
                        .color({0.96f, 0.98f, 1.0f, 1.0f})
                        .transition(motion)
                        .animate(eui::AnimProperty::Frame)
                        .build();

                    ui.text("card.status")
                        .position(24.0f, cardTop + 82.0f)
                        .size(cardWidth - 48.0f, 26.0f)
                        .text(state.expanded ? "Expanded" : "Compact")
                        .fontSize(15.0f)
                        .lineHeight(22.0f)
                        .color({0.72f, 0.84f, 0.91f, 1.0f})
                        .transition(motion)
                        .animate(eui::AnimProperty::Frame)
                        .build();

                    ui.text("card.details")
                        .position(24.0f, cardTop + 124.0f)
                        .size(cardWidth - 48.0f, 76.0f)
                        .text("Layout and visual properties share one transition, while each node declares exactly what should animate.")
                        .fontSize(15.0f)
                        .lineHeight(22.0f)
                        .wrap(true)
                        .opacity(state.expanded ? 1.0f : 0.0f)
                        .translateY(state.expanded ? 0.0f : 8.0f)
                        .transition(0.24f, eui::Ease::OutCubic)
                        .animate(eui::AnimProperty::Frame |
                                 eui::AnimProperty::Opacity |
                                 eui::AnimProperty::Transform)
                        .build();
                })
                .build();
        })
        .build();
}

} // namespace app
