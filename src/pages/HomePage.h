#pragma once

#include <algorithm>
#include "../EUINEO.h"
#include "../ui/UIContext.h"
#include <functional>
#include <string>
#include <vector>

namespace EUINEO {

class HomePage {
public:
    struct Actions {
        std::function<void()> onRandomizeThemeColor;
        std::function<void()> onToggleIconAccent;
        std::function<void(float)> onProgressChange;
        std::function<void(int)> onSegmentedChange;
        std::function<void(const std::string&)> onInputChange;
        std::function<void(int)> onComboChange;
    };

    static void Compose(UIContext& ui, const std::string& idPrefix, const RectFrame& bounds,
                        bool iconAccentEnabled,
                        float progressValue,
                        const std::vector<std::string>& segmentedItems, int segmentedIndex,
                        const std::string& inputText,
                        const std::vector<std::string>& comboItems, int comboSelection,
                        const Actions& actions) {
        const float gap = 16.0f;
        const float titleY = bounds.y + 24.0f;
        const float subtitleY = titleY + 30.0f;
        const float actionsY = subtitleY + 38.0f;
        const bool wideActions = bounds.width >= 420.0f;
        const float actionHeight = wideActions ? 76.0f : 144.0f;
        const float formY = actionsY + actionHeight + gap;
        const float formHeight = std::max(0.0f, bounds.y + bounds.height - formY);
        const float buttonGap = 12.0f;
        const float stackedButtonGap = 6.0f;
        const float buttonTop = wideActions ? 18.0f : 12.0f;
        const float buttonWidth = wideActions
            ? (bounds.width - buttonGap * 2.0f - 40.0f) / 3.0f
            : std::max(0.0f, bounds.width - 40.0f);
        const float buttonX = bounds.x + 20.0f;
        const float formInset = 20.0f;
        const float columnGap = 16.0f;
        const Color cardColor = CurrentTheme->surface;

        ui.label(idPrefix + ".title")
            .text("Home Controls")
            .position(bounds.x, titleY)
            .fontSize(31.0f)
            .color(Color(CurrentTheme->text.r, CurrentTheme->text.g, CurrentTheme->text.b, 0.98f))
            .build();

        ui.label(idPrefix + ".subtitle")
            .text("Basic widgets use the same page spacing and top-aligned layout.")
            .position(bounds.x, subtitleY)
            .fontSize(17.0f)
            .color(Color(CurrentTheme->text.r, CurrentTheme->text.g, CurrentTheme->text.b, 0.72f))
            .build();

        ui.panel(idPrefix + ".actions")
            .position(bounds.x, actionsY)
            .size(bounds.width, actionHeight)
            .background(cardColor)
            .rounding(18.0f)
            .build();

        ui.button(idPrefix + ".primary")
            .text("Primary")
            .position(buttonX, actionsY + buttonTop)
            .size(buttonWidth, 40.0f)
            .style(ButtonStyle::Primary)
            .fontSize(20.0f)
            .onClick([action = actions.onRandomizeThemeColor] {
                if (action) {
                    action();
                }
            })
            .build();

        ui.button(idPrefix + ".outline")
            .text("Outline")
            .position(
                wideActions ? buttonX + buttonWidth + buttonGap : buttonX,
                wideActions ? actionsY + buttonTop : actionsY + buttonTop + 40.0f + stackedButtonGap
            )
            .size(buttonWidth, 40.0f)
            .style(ButtonStyle::Outline)
            .fontSize(20.0f)
            .build();

        ui.button(idPrefix + ".icon")
            .text("Icon  \xEF\x80\x93")
            .position(
                wideActions ? buttonX + (buttonWidth + buttonGap) * 2.0f : buttonX,
                wideActions ? actionsY + buttonTop : actionsY + buttonTop + (40.0f + stackedButtonGap) * 2.0f
            )
            .size(buttonWidth, 40.0f)
            .fontSize(20.0f)
            .textColor(iconAccentEnabled ? CurrentTheme->primary : CurrentTheme->text)
            .onClick([action = actions.onToggleIconAccent] {
                if (action) {
                    action();
                }
            })
            .build();

        if (formHeight <= 0.0f) {
            return;
        }

        ui.panel(idPrefix + ".form")
            .position(bounds.x, formY)
            .size(bounds.width, formHeight)
            .background(cardColor)
            .rounding(18.0f)
            .build();

        const float innerX = bounds.x + formInset;
        const float innerY = formY + formInset;
        const float innerWidth = std::max(0.0f, bounds.width - formInset * 2.0f);
        const float innerHeight = std::max(0.0f, formHeight - formInset * 2.0f);
        if (innerWidth <= 0.0f || innerHeight <= 0.0f) {
            return;
        }

        const float columnWidth = std::max(0.0f, (innerWidth - columnGap) * 0.5f);
        const float leftX = innerX;
        const float leftY = innerY;
        const float rightX = innerX + columnWidth + columnGap;
        const float rightY = innerY;
        const float leftFieldWidth = std::max(0.0f, columnWidth - 36.0f);
        const float rightFieldWidth = std::max(0.0f, columnWidth - 36.0f);
        const float progressValueX = std::max(leftX + 18.0f, leftX + columnWidth - 58.0f);

        ui.label(idPrefix + ".progress.label")
            .text("Progress")
            .position(leftX + 18.0f, leftY + 28.0f)
            .fontSize(17.0f)
            .build();

        ui.label(idPrefix + ".progress.value")
            .text(std::to_string(static_cast<int>(progressValue * 100.0f)) + "%")
            .position(progressValueX, leftY + 28.0f)
            .fontSize(16.0f)
            .color(Color(CurrentTheme->text.r, CurrentTheme->text.g, CurrentTheme->text.b, 0.68f))
            .build();

        ui.progress(idPrefix + ".progress")
            .position(leftX + 18.0f, leftY + 56.0f)
            .size(leftFieldWidth, 15.0f)
            .value(progressValue)
            .build();

        ui.slider(idPrefix + ".slider")
            .position(leftX + 18.0f, leftY + 82.0f)
            .size(leftFieldWidth, 20.0f)
            .value(progressValue)
            .onChange([action = actions.onProgressChange](float value) {
                if (action) {
                    action(value);
                }
            })
            .build();

        ui.label(idPrefix + ".segmented.label")
            .text("Segment")
            .position(leftX + 18.0f, leftY + 126.0f)
            .fontSize(17.0f)
            .build();

        ui.segmented(idPrefix + ".segmented")
            .position(leftX + 18.0f, leftY + 154.0f)
            .size(leftFieldWidth, 35.0f)
            .items(segmentedItems)
            .selected(segmentedIndex)
            .fontSize(20.0f)
            .onChange([action = actions.onSegmentedChange](int index) {
                if (action) {
                    action(index);
                }
            })
            .build();

        ui.label(idPrefix + ".input.label")
            .text("Input")
            .position(rightX + 18.0f, rightY + 28.0f)
            .fontSize(17.0f)
            .build();

        ui.input(idPrefix + ".input")
            .position(rightX + 18.0f, rightY + 56.0f)
            .size(rightFieldWidth, 35.0f)
            .placeholder("Type something...")
            .fontSize(20.0f)
            .text(inputText)
            .onChange([action = actions.onInputChange](const std::string& text) {
                if (action) {
                    action(text);
                }
            })
            .build();

        ui.label(idPrefix + ".combo.label")
            .text("Combo")
            .position(rightX + 18.0f, rightY + 108.0f)
            .fontSize(17.0f)
            .build();

        ui.combo(idPrefix + ".combo")
            .position(rightX + 18.0f, rightY + 136.0f)
            .size(rightFieldWidth, 35.0f)
            .placeholder("Select an option")
            .fontSize(20.0f)
            .items(comboItems)
            .selected(comboSelection)
            .onChange([action = actions.onComboChange](int index) {
                if (action) {
                    action(index);
                }
            })
            .build();
    }
};

} // namespace EUINEO
