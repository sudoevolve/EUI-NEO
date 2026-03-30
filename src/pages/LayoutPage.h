#pragma once

#include "../EUINEO.h"
#include "../ui/UIContext.h"
#include "../ui/ThemeTokens.h"
#include <algorithm>
#include <functional>
#include <string>

namespace EUINEO {

class LayoutPage {
public:
    struct Actions {
        std::function<void(float)> onSplitChange;
    };

    static void Compose(UIContext& ui, const std::string& idPrefix, const RectFrame& bounds,
                        float splitRatio, const Actions& actions) {
        if (bounds.width <= 0.0f || bounds.height <= 0.0f) {
            return;
        }

        const PageVisualTokens visuals = CurrentPageVisuals();
        const PageHeaderLayout header = ComposePageHeader(
            ui,
            idPrefix,
            bounds,
            "Layout",
            "row / column / flex."
        );

        const float split = std::clamp(splitRatio, 0.28f, 0.72f);
        const float sliderValue = (split - 0.28f) / 0.44f;
        const float gap = visuals.sectionGap;
        const RectFrame controlFrame{bounds.x, header.contentY, bounds.width, 78.0f};
        const float demoY = controlFrame.y + controlFrame.height + gap;
        const RectFrame demoFrame{
            bounds.x,
            demoY,
            bounds.width,
            std::max(0.0f, bounds.y + bounds.height - demoY)
        };

        ComposePageSection(ui, idPrefix + ".control", controlFrame);
        ui.row()
            .position(controlFrame.x, controlFrame.y)
            .size(controlFrame.width, controlFrame.height)
            .padding(20.0f, 18.0f)
            .gap(12.0f)
            .content([&] {
                ui.label(idPrefix + ".control.label")
                    .text("Split")
                    .fontSize(visuals.labelSize)
                    .build();

                ui.slider(idPrefix + ".control.slider")
                    .flex(1.0f)
                    .height(18.0f)
                    .value(sliderValue)
                    .onChange([action = actions.onSplitChange](float value) {
                        if (action) {
                            action(0.28f + std::clamp(value, 0.0f, 1.0f) * 0.44f);
                        }
                    })
                    .build();

                ui.label(idPrefix + ".control.value")
                    .text(std::to_string(static_cast<int>(split * 100.0f)) + "%")
                    .fontSize(16.0f)
                    .color(visuals.bodyColor)
                    .build();

            });

        if (demoFrame.height <= 0.0f) {
            return;
        }

        ComposePageSection(ui, idPrefix + ".demo", demoFrame);
        ui.flex()
            .direction(FlexDirection::Row)
            .position(demoFrame.x, demoFrame.y)
            .size(demoFrame.width, demoFrame.height)
            .padding(20.0f)
            .gap(gap)
            .content([&] {
                ComposeLeftColumn(ui, idPrefix + ".left", split);

                ui.panel(idPrefix + ".divider")
                    .width(gap)
                    .background(visuals.softAccentColor)
                    .rounding(8.0f)
                    .build();

                ComposeRightColumn(ui, idPrefix + ".right", 1.0f - split);
            });
    }

private:
    static void ComposeLeftColumn(UIContext& ui, const std::string& idPrefix, float flexWeight) {
        const PageVisualTokens visuals = CurrentPageVisuals();

        ui.column()
            .flex(flexWeight)
            .gap(12.0f)
            .content([&] {
                ui.label(idPrefix + ".title")
                    .text("column()")
                    .fontSize(22.0f)
                    .margin(0.0f, 16.0f, 0.0f, 0.0f)
                    .build();

                ui.label(idPrefix + ".note")
                    .text("Vertical stack. Auto fill width.")
                    .fontSize(18.0f)
                    .color(visuals.bodyColor)
                    .build();

                ui.button(idPrefix + ".primary")
                    .text("Full Width Button")
                    .style(ButtonStyle::Primary)
                    .build();

                ui.button(idPrefix + ".outline")
                    .text("Second Item")
                    .style(ButtonStyle::Outline)
                    .build();

                ui.image(idPrefix + ".fill")
                    .flex(1.0f)
                    .path("bing://daily?idx=0&mkt=zh-CN")
                    .rounding(14.0f)
                    .build();
            });
    }

    static void ComposeRightColumn(UIContext& ui, const std::string& idPrefix, float flexWeight) {
        const PageVisualTokens visuals = CurrentPageVisuals();

        ui.column()
            .flex(flexWeight)
            .gap(12.0f)
            .content([&] {
                ui.label(idPrefix + ".title")
                    .text("flex()")
                    .fontSize(22.0f)
                    .margin(0.0f, 16.0f, 0.0f, 0.0f)
                    .build();

                ui.label(idPrefix + ".note")
                    .text("flex(n) splits remaining space.")
                    .fontSize(18.0f)
                    .color(visuals.bodyColor)
                    .build();

                ui.row()
                    .height(40.0f)
                    .gap(12.0f)
                    .content([&] {
                        ui.button(idPrefix + ".one")
                            .flex(1.0f)
                            .text("1x")
                            .build();

                        ui.button(idPrefix + ".two")
                            .flex(2.0f)
                            .text("2x")
                            .build();
                    });

                ui.progress(idPrefix + ".progress")
                    .value(std::clamp(flexWeight, 0.0f, 1.0f))
                    .build();

                ui.image(idPrefix + ".fill")
                    .flex(1.0f)
                    .path("bing://daily?idx=1&mkt=zh-CN")
                    .rounding(14.0f)
                    .build();
            });
    }
};

} // namespace EUINEO
