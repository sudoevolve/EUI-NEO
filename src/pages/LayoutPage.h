#pragma once

#include "../EUINEO.h"
#include "../ui/UIContext.h"
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

        const float gap = 16.0f;
        const float split = std::clamp(splitRatio, 0.28f, 0.72f);
        const float sliderValue = (split - 0.28f) / 0.44f;

        const Color cardColor = CurrentTheme->surface;
        const float titleY = bounds.y + 24.0f;
        const float subtitleY = titleY + 30.0f;
        const float controlY = subtitleY + 38.0f;
        const float controlH = 76.0f;
        const float previewY = controlY + controlH + gap;
        const float previewH = std::max(0.0f, bounds.y + bounds.height - previewY);

        ui.label(idPrefix + ".title")
            .text("Layout Playground")
            .position(bounds.x, titleY)
            .fontSize(31.0f)
            .color(Color(CurrentTheme->text.r, CurrentTheme->text.g, CurrentTheme->text.b, 0.98f))
            .build();

        ui.label(idPrefix + ".subtitle")
            .text("Drag the slider to test left and right layout balance.")
            .position(bounds.x, subtitleY)
            .fontSize(17.0f)
            .color(Color(CurrentTheme->text.r, CurrentTheme->text.g, CurrentTheme->text.b, 0.72f))
            .build();

        ui.panel(idPrefix + ".control")
            .position(bounds.x, controlY)
            .size(bounds.width, controlH)
            .background(cardColor)
            .rounding(18.0f)
            .build();

        ui.label(idPrefix + ".control.label")
            .text("Left / Right Split")
            .position(bounds.x + 20.0f, controlY + 26.0f)
            .fontSize(17.0f)
            .build();

        ui.label(idPrefix + ".control.value")
            .text(std::to_string(static_cast<int>(split * 100.0f)) + "%")
            .position(std::max(bounds.x + 20.0f, bounds.x + bounds.width - 72.0f), controlY + 26.0f)
            .fontSize(16.0f)
            .color(Color(CurrentTheme->text.r, CurrentTheme->text.g, CurrentTheme->text.b, 0.68f))
            .build();

        ui.slider(idPrefix + ".control.slider")
            .position(bounds.x + 20.0f, controlY + 46.0f)
            .size(std::max(0.0f, bounds.width - 40.0f), 18.0f)
            .value(sliderValue)
            .onChange([action = actions.onSplitChange](float value) {
                if (action) {
                    action(0.28f + std::clamp(value, 0.0f, 1.0f) * 0.44f);
                }
            })
            .build();

        if (previewH <= 0.0f) {
            return;
        }

        ui.panel(idPrefix + ".preview")
            .position(bounds.x, previewY)
            .size(bounds.width, previewH)
            .background(cardColor)
            .rounding(18.0f)
            .build();

        const float innerX = bounds.x + 20.0f;
        const float innerY = previewY + 20.0f;
        const float innerW = std::max(0.0f, bounds.width - 40.0f);
        const float innerH = std::max(0.0f, previewH - 40.0f);
        if (innerW <= 0.0f || innerH <= 0.0f) {
            return;
        }

        const float splitW = std::max(0.0f, innerW - gap);
        float leftW = splitW * split;
        if (splitW > 180.0f) {
            leftW = std::clamp(leftW, 84.0f, splitW - 84.0f);
        } else {
            leftW = splitW * 0.5f;
        }
        const float rightW = std::max(0.0f, splitW - leftW);

        ui.panel(idPrefix + ".divider")
            .position(innerX + leftW, innerY + 18.0f)
            .size(gap, std::max(0.0f, innerH - 36.0f))
            .background(Color(CurrentTheme->primary.r, CurrentTheme->primary.g, CurrentTheme->primary.b, 0.16f))
            .rounding(8.0f)
            .build();

        ComposePane(ui, idPrefix + ".left.info", innerX, innerY, leftW, innerH,
                    "Left Pane", "Navigation, filters or tools.");
        ComposePane(ui, idPrefix + ".right.info", innerX + leftW + gap, innerY, rightW, innerH,
                    "Right Pane", "Content, preview or inspector.");
    }

private:
    static void ComposePane(UIContext& ui, const std::string& idPrefix,
                            float x, float y, float width, float height,
                            const char* title, const char* note) {
        if (width <= 0.0f || height <= 0.0f) {
            return;
        }

        ui.label(idPrefix + ".title")
            .text(title)
            .position(x + 18.0f, y + 30.0f)
            .fontSize(22.0f)
            .build();

        if (height < 120.0f) {
            ui.label(idPrefix + ".width")
                .text(std::to_string(static_cast<int>(width)) + " px")
                .position(x + 18.0f, y + 62.0f)
                .fontSize(18.0f)
                .color(Color(CurrentTheme->primary.r, CurrentTheme->primary.g, CurrentTheme->primary.b, 0.92f))
                .build();
            return;
        }

        ui.label(idPrefix + ".note")
            .text(note)
            .position(x + 18.0f, y + 56.0f)
            .fontSize(14.0f)
            .color(Color(CurrentTheme->text.r, CurrentTheme->text.g, CurrentTheme->text.b, 0.68f))
            .build();

        ui.label(idPrefix + ".width")
            .text(std::to_string(static_cast<int>(width)) + " px")
            .position(x + 18.0f, y + 92.0f)
            .fontSize(18.0f)
            .color(Color(CurrentTheme->primary.r, CurrentTheme->primary.g, CurrentTheme->primary.b, 0.92f))
            .build();

        if (height < 150.0f) {
            return;
        }

        ui.panel(idPrefix + ".preview")
            .position(x + 18.0f, y + 122.0f)
            .size(std::max(0.0f, width - 36.0f), std::max(0.0f, height - 142.0f))
            .background(CurrentTheme->surface)
            .rounding(12.0f)
            .build();
    }
};

} // namespace EUINEO
