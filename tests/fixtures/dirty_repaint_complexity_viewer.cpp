#include "eui_neo.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace app {
namespace {

struct ViewerState {
    int level = 0;
};

ViewerState& state() {
    static ViewerState value;
    return value;
}

eui::Color rgba(float r, float g, float b, float a = 1.0f) {
    return {r, g, b, a};
}

int levelCount() {
    return 5;
}

void switchLevel(int delta) {
    ViewerState& viewer = state();
    viewer.level = (viewer.level + delta + levelCount()) % levelCount();
    app::requestUpdate();
}

bool handleKey(const core::KeyEvent& event) {
    if (!event.isDown()) {
        return false;
    }
    if (event.key == core::InputKey::Left) {
        switchLevel(-1);
        return true;
    }
    if (event.key == core::InputKey::Right) {
        switchLevel(1);
        return true;
    }
    return false;
}

std::string levelName(int level) {
    switch (level) {
    case 0: return "0 baseline";
    case 1: return "1 distant static";
    case 2: return "2 overlap light";
    case 3: return "3 overlap heavy";
    default: return "4 overlap text";
    }
}

void drawCard(eui::Ui& ui,
              const std::string& id,
              float x,
              float y,
              float width,
              float height,
              int index,
              float alpha = 1.0f) {
    const float tone = static_cast<float>((index * 37) % 100) / 100.0f;
    const eui::Color fill = rgba(0.13f + tone * 0.11f, 0.18f + tone * 0.10f, 0.22f + tone * 0.08f, 0.70f * alpha);
    const eui::Color line = rgba(0.65f, 0.78f, 0.92f, 0.14f * alpha);
    ui.rect(id)
        .x(x)
        .y(y)
        .size(width, height)
        .radius(14.0f + static_cast<float>(index % 5))
        .color(fill)
        .border(1.0f, line)
        .shadow(14.0f + static_cast<float>(index % 4) * 3.0f,
                0.0f,
                5.0f + static_cast<float>(index % 3) * 2.0f,
                rgba(0.0f, 0.0f, 0.0f, 0.12f * alpha))
        .build();
}

void drawDistantStatic(eui::Ui& ui, float screenWidth) {
    const float startX = std::min(450.0f, std::max(320.0f, screenWidth * 0.42f));
    for (int i = 0; i < 96; ++i) {
        const int column = i % 12;
        const int row = i / 12;
        const float x = startX + static_cast<float>(column) * 58.0f;
        const float y = 92.0f + static_cast<float>(row) * 48.0f;
        drawCard(ui,
                 "distant.card." + std::to_string(i),
                 x,
                 y,
                 44.0f + static_cast<float>(i % 3) * 7.0f,
                 32.0f + static_cast<float>(i % 4) * 5.0f,
                 i,
                 0.82f);
    }
}

void drawOverlappingCards(eui::Ui& ui, int count, float buttonX, float buttonY, float buttonWidth, float buttonHeight) {
    for (int i = 0; i < count; ++i) {
        const float wave = std::sin(static_cast<float>(i) * 0.77f);
        const float waveB = std::cos(static_cast<float>(i) * 0.51f);
        const float x = buttonX - 34.0f + static_cast<float>((i * 19) % 92) - 46.0f + wave * 10.0f;
        const float y = buttonY - 24.0f + static_cast<float>((i * 13) % 58) - 29.0f + waveB * 8.0f;
        const float width = buttonWidth + 48.0f + static_cast<float>(i % 5) * 18.0f;
        const float height = buttonHeight + 18.0f + static_cast<float>(i % 4) * 12.0f;
        drawCard(ui, "overlap.card." + std::to_string(i), x, y, width, height, i, 0.48f);
    }
}

void drawOverlappingText(eui::Ui& ui, int count, float buttonX, float buttonY, float buttonWidth) {
    for (int i = 0; i < count; ++i) {
        const float x = buttonX - 18.0f + static_cast<float>((i * 31) % 84) - 42.0f;
        const float y = buttonY - 18.0f + static_cast<float>((i * 17) % 76) - 38.0f;
        ui.text("overlap.text." + std::to_string(i))
            .x(x)
            .y(y)
            .size(buttonWidth + 90.0f, 22.0f)
            .text("static text layer " + std::to_string(i))
            .fontSize(12.0f + static_cast<float>(i % 5))
            .color(rgba(0.78f, 0.86f, 0.94f, 0.22f))
            .build();
    }
}

void drawSceneComplexity(eui::Ui& ui, int level, float screenWidth, float buttonX, float buttonY, float buttonWidth, float buttonHeight) {
    if (level >= 1) {
        drawDistantStatic(ui, screenWidth);
    }
    if (level == 2) {
        drawOverlappingCards(ui, 12, buttonX, buttonY, buttonWidth, buttonHeight);
    } else if (level == 3) {
        drawOverlappingCards(ui, 56, buttonX, buttonY, buttonWidth, buttonHeight);
    } else if (level >= 4) {
        drawOverlappingCards(ui, 56, buttonX, buttonY, buttonWidth, buttonHeight);
        drawOverlappingText(ui, 64, buttonX, buttonY, buttonWidth);
    }
}

void drawBaseButton(eui::Ui& ui, float x, float y, float width, float height) {
    ui.stack("base.button")
        .x(x)
        .y(y)
        .size(width, height)
        .visualStateFrom("base.button.surface", 0.96f)
        .content([&] {
            ui.rect("base.button.surface")
                .size(width, height)
                .radius(18.0f)
                .states(rgba(0.18f, 0.44f, 0.92f),
                        rgba(0.25f, 0.53f, 1.0f),
                        rgba(0.12f, 0.32f, 0.78f))
                .border(1.0f, rgba(0.90f, 0.96f, 1.0f, 0.20f))
                .shadow(20.0f, 0.0f, 8.0f, rgba(0.0f, 0.0f, 0.0f, 0.25f))
                .transition(0.12f, eui::Ease::OutCubic)
                .focusable(true)
                .onKeyEvent(handleKey)
                .build();

            ui.text("base.button.label")
                .size(width, height)
                .text("Base Button")
                .fontSize(21.0f)
                .horizontalAlign(eui::HorizontalAlign::Center)
                .verticalAlign(eui::VerticalAlign::Center)
                .color(rgba(1.0f, 1.0f, 1.0f))
                .build();
        })
        .build();
}

} // namespace

const DslAppConfig& dslAppConfig() {
    static const DslAppConfig config = DslAppConfig{}
        .title("Dirty Repaint Complexity Viewer")
        .pageId("dirty_repaint_complexity_viewer")
        .clearColor({0.07f, 0.08f, 0.10f, 1.0f})
        .windowSize(1120, 780)
        .showDebugStatsInTitle(true)
        .fps(90.0)
        .iconPath("");
    return config;
}

void compose(eui::Ui& ui, const eui::Screen& screen) {
    const ViewerState& viewer = state();
    const float buttonX = 80.0f;
    const float buttonY = std::max(430.0f, screen.height - 170.0f);
    const float buttonWidth = 260.0f;
    const float buttonHeight = 76.0f;

    ui.stack("keyboard.focus")
        .size(screen.width, screen.height)
        .focusable(true)
        .onKeyEvent(handleKey)
        .content([&] {
            ui.rect("background")
                .size(screen.width, screen.height)
                .gradient(rgba(0.07f, 0.08f, 0.10f),
                          rgba(0.11f, 0.13f, 0.15f),
                          eui::GradientDirection::Vertical)
                .build();

            drawSceneComplexity(ui, viewer.level, screen.width, buttonX, buttonY, buttonWidth, buttonHeight);

            ui.text("level.title")
                .x(32.0f)
                .y(24.0f)
                .size(std::max(0.0f, screen.width - 64.0f), 30.0f)
                .text(levelName(viewer.level))
                .fontSize(23.0f)
                .color(rgba(0.94f, 0.97f, 1.0f))
                .build();

            ui.text("level.note")
                .x(32.0f)
                .y(60.0f)
                .size(std::max(0.0f, screen.width - 64.0f), 22.0f)
                .text("Left/Right switches scenes. Hover or press the same button and watch Dirty/Draw/GPU.")
                .fontSize(14.0f)
                .color(rgba(0.64f, 0.70f, 0.78f))
                .build();

            drawBaseButton(ui, buttonX, buttonY, buttonWidth, buttonHeight);
        })
        .build();
}

} // namespace app
