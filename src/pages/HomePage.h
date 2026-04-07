#pragma once

#include <algorithm>
#include "../EUINEO.h"
#include "../ui/UIContext.h"
#include "../ui/ThemeTokens.h"
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
        std::function<void(const std::string&)> onTextAreaChange;
        std::function<void(int)> onComboChange;
    };

    static void Compose(UIContext& ui, const std::string& idPrefix, const RectFrame& bounds,
                        bool iconAccentEnabled, float progressValue, int segmentedIndex,
                        const std::string& inputText, const std::string& textAreaText,
                        int comboSelection, const Actions& actions) {
        if (bounds.width <= 0.0f || bounds.height <= 0.0f) {
            return;
        }
        const PageVisualTokens visuals = CurrentPageVisuals();
        const PageHeaderLayout header = ComposePageHeader(
            ui,
            idPrefix,
            bounds,
            "Home Controls",
            "Explore all the EUI controls"
        );
        if (bounds.y + bounds.height - header.contentY <= 0.0f) {
            return;
        }

        const float viewportX = bounds.x + visuals.sectionInset;
        const float viewportY = header.contentY;
        const float viewportWidth = std::max(0.0f, bounds.width - visuals.sectionInset * 2.0f);
        const float viewportHeight = std::max(0.0f, bounds.y + bounds.height - header.contentY);
        ComposePageSection(ui, idPrefix + ".viewport", RectFrame{viewportX, viewportY, viewportWidth, viewportHeight});

        ui.scrollArea(
            idPrefix + ".scroll",
            viewportX,
            viewportY,
            viewportWidth,
            viewportHeight,
            visuals.sectionInset
                + (bounds.width >= 420.0f ? 76.0f : 144.0f)
                + visuals.sectionGap
                + 240.0f
                + visuals.sectionGap
                + 186.0f
                + visuals.sectionGap
                + 132.0f
                + visuals.sectionInset,
            [&](float) {
                const float contentX = viewportX + 18.0f;
                const float contentWidth = std::max(0.0f, viewportWidth - 36.0f);
                const float innerX = contentX + 18.0f;
                const float halfColumnWidth = std::max(0.0f, (contentWidth - visuals.sectionGap) * 0.5f);
                const float fieldWidth = std::max(0.0f, halfColumnWidth - 36.0f);
                float y = viewportY + visuals.sectionInset;

                ComposePageSection(
                    ui,
                    idPrefix + ".actions",
                    RectFrame{
                        contentX,
                        y,
                        contentWidth,
                        bounds.width >= 420.0f ? 76.0f : 144.0f
                    }
                );

                ui.button(idPrefix + ".primary")
                    .text("Primary")
                    .position(innerX, y + (bounds.width >= 420.0f ? 18.0f : 12.0f))
                    .size(
                        bounds.width >= 420.0f
                            ? (contentWidth - 12.0f * 2.0f - 36.0f) / 3.0f
                            : std::max(0.0f, contentWidth - 36.0f),
                        40.0f
                    )
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
                        bounds.width >= 420.0f
                            ? innerX + ((contentWidth - 12.0f * 2.0f - 36.0f) / 3.0f) + 12.0f
                            : innerX,
                        bounds.width >= 420.0f
                            ? y + 18.0f
                            : y + 12.0f + 40.0f + 6.0f
                    )
                    .size(
                        bounds.width >= 420.0f
                            ? (contentWidth - 12.0f * 2.0f - 36.0f) / 3.0f
                            : std::max(0.0f, contentWidth - 36.0f),
                        40.0f
                    )
                    .style(ButtonStyle::Outline)
                    .fontSize(20.0f)
                    .build();

                ui.button(idPrefix + ".icon")
                    .text("Icon")
                    .icon("\xEF\x80\x93")
                    .iconPlacement(ButtonIconPlacement::Trailing)
                    .position(
                        bounds.width >= 420.0f
                            ? innerX + ((contentWidth - 12.0f * 2.0f - 36.0f) / 3.0f + 12.0f) * 2.0f
                            : innerX,
                        bounds.width >= 420.0f
                            ? y + 18.0f
                            : y + 12.0f + (40.0f + 6.0f) * 2.0f
                    )
                    .size(
                        bounds.width >= 420.0f
                            ? (contentWidth - 12.0f * 2.0f - 36.0f) / 3.0f
                            : std::max(0.0f, contentWidth - 36.0f),
                        40.0f
                    )
                    .fontSize(20.0f)
                    .textColor(iconAccentEnabled ? CurrentTheme->primary : CurrentTheme->text)
                    .onClick([action = actions.onToggleIconAccent] {
                        if (action) {
                            action();
                        }
                    })
                    .build();

                y += (bounds.width >= 420.0f ? 76.0f : 144.0f) + visuals.sectionGap;

                ComposePageSection(ui, idPrefix + ".form", RectFrame{contentX, y, contentWidth, 240.0f});

                ui.label(idPrefix + ".progress.label")
                    .text("Progress")
                    .position(innerX, y + visuals.sectionInset + 28.0f)
                    .fontSize(visuals.labelSize)
                    .build();

                ui.label(idPrefix + ".progress.value")
                    .text(std::to_string(static_cast<int>(progressValue * 100.0f)) + "%")
                    .position(
                        std::max(
                            innerX,
                            contentX + (halfColumnWidth - 58.0f)
                        ),
                        y + visuals.sectionInset + 28.0f
                    )
                    .fontSize(16.0f)
                    .color(visuals.bodyColor)
                    .build();

                ui.progress(idPrefix + ".progress")
                    .position(innerX, y + visuals.sectionInset + 56.0f)
                    .size(fieldWidth, 15.0f)
                    .value(progressValue)
                    .build();

                ui.slider(idPrefix + ".slider")
                    .position(innerX, y + visuals.sectionInset + 82.0f)
                    .size(fieldWidth, 20.0f)
                    .value(progressValue)
                    .onChange([action = actions.onProgressChange](float value) {
                        if (action) {
                            action(value);
                        }
                    })
                    .build();

                ui.label(idPrefix + ".segmented.label")
                    .text("Segment")
                    .position(innerX, y + visuals.sectionInset + 126.0f)
                    .fontSize(visuals.labelSize)
                    .build();

                ui.segmented(idPrefix + ".segmented")
                    .position(innerX, y + visuals.sectionInset + 154.0f)
                    .size(fieldWidth, 35.0f)
                    .items({"Apple", "Banana", "Cherry"})
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
                    .position(
                        contentX + halfColumnWidth + visuals.sectionGap + 18.0f,
                        y + visuals.sectionInset + 28.0f
                    )
                    .fontSize(visuals.labelSize)
                    .build();

                ui.input(idPrefix + ".input")
                    .position(
                        contentX + halfColumnWidth + visuals.sectionGap + 18.0f,
                        y + visuals.sectionInset + 56.0f
                    )
                    .size(fieldWidth, visuals.fieldHeight)
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
                    .position(
                        contentX + halfColumnWidth + visuals.sectionGap + 18.0f,
                        y + visuals.sectionInset + 108.0f
                    )
                    .fontSize(visuals.labelSize)
                    .build();

                ui.combo(idPrefix + ".combo")
                    .position(
                        contentX + halfColumnWidth + visuals.sectionGap + 18.0f,
                        y + visuals.sectionInset + 136.0f
                    )
                    .size(fieldWidth, visuals.fieldHeight)
                    .placeholder("Select an option")
                    .fontSize(20.0f)
                    .maxVisibleItems(4)
                    .startOpen(false)
                    .items({"Apple", "Banana", "Cherry", "Grape", "Orange", "Peach", "Pear"})
                    .selected(comboSelection)
                    .onChange([action = actions.onComboChange](int index) {
                        if (action) {
                            action(index);
                        }
                    })
                    .build();

                y += 240.0f + visuals.sectionGap;

                ComposePageSection(ui, idPrefix + ".newwidgets", RectFrame{contentX, y, contentWidth, 186.0f});

                ui.switcher(idPrefix + ".newwidgets.switch")
                    .position(innerX, y + 36.0f)
                    .size(46.0f, 24.0f)
                    .checked(iconAccentEnabled)
                    .label("Icon Accent")
                    .onChange([action = actions.onToggleIconAccent](bool) {
                        if (action) {
                            action();
                        }
                    })
                    .build();

                ui.checkbox(idPrefix + ".newwidgets.checkbox")
                    .position(innerX, y + 76.0f)
                    .size(22.0f, 22.0f)
                    .checked(progressValue >= 0.5f)
                    .text("Progress >= 50%")
                    .onChange([action = actions.onProgressChange](bool checked) {
                        if (action) {
                            action(checked ? 0.75f : 0.25f);
                        }
                    })
                    .build();

                ui.radio(idPrefix + ".newwidgets.radio")
                    .position(innerX, y + 114.0f)
                    .size(22.0f, 22.0f)
                    .selected(segmentedIndex == 2)
                    .text("Cherry Mode")
                    .onChange([action = actions.onSegmentedChange](bool selected) {
                        if (action && selected) {
                            action(2);
                        }
                    })
                    .build();

                ui.textArea(idPrefix + ".newwidgets.textarea")
                    .position(contentX + contentWidth * 0.45f, y + 30.0f)
                    .size(std::max(140.0f, contentWidth - contentWidth * 0.45f - 18.0f), 124.0f)
                    .placeholder("TextArea...")
                    .fontSize(18.0f)
                    .multiline(true)
                    .text(textAreaText)
                    .onChange([action = actions.onTextAreaChange](const std::string& text) {
                        if (action) {
                            action(text);
                        }
                    })
                    .build();

                ui.dialog(idPrefix + ".newwidgets.dialog")
                    .open(segmentedIndex == 2)
                    .title("Cherry Mode")
                    .message("Segmented is on Cherry.\nClick confirm to randomize theme.")
                    .confirmText("Randomize")
                    .cancelText("Close")
                    .showCancel(true)
                    .closeOnBackdrop(true)
                    .onConfirm([action = actions.onRandomizeThemeColor, close = actions.onSegmentedChange] {
                        if (action) {
                            action();
                        }
                        if (close) {
                            close(0);
                        }
                    })
                    .onCancel([close = actions.onSegmentedChange] {
                        if (close) {
                            close(0);
                        }
                    })
                    .onClose([close = actions.onSegmentedChange] {
                        if (close) {
                            close(0);
                        }
                    })
                    .build();

                y += 186.0f + visuals.sectionGap;

                ComposePageSection(ui, idPrefix + ".colors", RectFrame{contentX, y, contentWidth, 132.0f});

                ui.panel(idPrefix + ".colors.primary")
                    .position(innerX, y + 30.0f)
                    .size(std::max(72.0f, (contentWidth - 18.0f * 2.0f) / 4.0f - 8.0f), 70.0f)
                    .background(CurrentTheme->primary)
                    .rounding(12.0f)
                    .build();
                ui.tooltip(idPrefix + ".colors.primary.tip")
                    .position(innerX, y + 30.0f)
                    .size(std::max(72.0f, (contentWidth - 18.0f * 2.0f) / 4.0f - 8.0f), 70.0f)
                    .text("Primary")
                    .triggerOnHover(true)
                    .followMouse(true)
                    .build();

                ui.panel(idPrefix + ".colors.surface")
                    .position(innerX + std::max(72.0f, (contentWidth - 18.0f * 2.0f) / 4.0f - 8.0f) + 10.0f, y + 30.0f)
                    .size(std::max(72.0f, (contentWidth - 18.0f * 2.0f) / 4.0f - 8.0f), 70.0f)
                    .background(CurrentTheme->surface)
                    .border(1.0f, CurrentTheme->border)
                    .rounding(12.0f)
                    .build();
                ui.tooltip(idPrefix + ".colors.surface.tip")
                    .position(innerX + std::max(72.0f, (contentWidth - 18.0f * 2.0f) / 4.0f - 8.0f) + 10.0f, y + 30.0f)
                    .size(std::max(72.0f, (contentWidth - 18.0f * 2.0f) / 4.0f - 8.0f), 70.0f)
                    .text("Surface")
                    .triggerOnHover(true)
                    .followMouse(true)
                    .build();

                ui.panel(idPrefix + ".colors.hover")
                    .position(innerX + (std::max(72.0f, (contentWidth - 18.0f * 2.0f) / 4.0f - 8.0f) + 10.0f) * 2.0f, y + 30.0f)
                    .size(std::max(72.0f, (contentWidth - 18.0f * 2.0f) / 4.0f - 8.0f), 70.0f)
                    .background(CurrentTheme->surfaceHover)
                    .rounding(12.0f)
                    .build();
                ui.tooltip(idPrefix + ".colors.hover.tip")
                    .position(innerX + (std::max(72.0f, (contentWidth - 18.0f * 2.0f) / 4.0f - 8.0f) + 10.0f) * 2.0f, y + 30.0f)
                    .size(std::max(72.0f, (contentWidth - 18.0f * 2.0f) / 4.0f - 8.0f), 70.0f)
                    .text("Surface Hover")
                    .triggerOnHover(true)
                    .followMouse(true)
                    .build();

                ui.panel(idPrefix + ".colors.text")
                    .position(innerX + (std::max(72.0f, (contentWidth - 18.0f * 2.0f) / 4.0f - 8.0f) + 10.0f) * 3.0f, y + 30.0f)
                    .size(std::max(72.0f, (contentWidth - 18.0f * 2.0f) / 4.0f - 8.0f), 70.0f)
                    .background(CurrentTheme->text)
                    .rounding(12.0f)
                    .build();
                ui.tooltip(idPrefix + ".colors.text.tip")
                    .position(innerX + (std::max(72.0f, (contentWidth - 18.0f * 2.0f) / 4.0f - 8.0f) + 10.0f) * 3.0f, y + 30.0f)
                    .size(std::max(72.0f, (contentWidth - 18.0f * 2.0f) / 4.0f - 8.0f), 70.0f)
                    .text("Text")
                    .triggerOnHover(true)
                    .followMouse(true)
                    .build();
            }
        );
    }
};

} // namespace EUINEO
