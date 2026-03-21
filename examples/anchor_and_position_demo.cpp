#define EUI_BACKEND_OPENGL
#define EUI_PLATFORM_GLFW
#include "EUI.h"

#include <algorithm>

namespace {

using eui::Rect;
using eui::quick::UI;

void draw_manual_example(UI& ui, const Rect& rect, float scale) {
    ui.card("Manual .at().size()")
        .in(rect)
        .radius(22.0f * scale)
        .fill(0x121A2B)
        .stroke(0x2A3550, 1.0f)
        .begin([&](auto& card) {
            const Rect area = card.content();
            const float x = area.x + 18.0f * scale;
            const float y = area.y + 16.0f * scale;
            const float w = area.w - 36.0f * scale;

            ui.text("Write x / y / w / h directly")
                .at(x, y)
                .size(w, 24.0f * scale)
                .font(18.0f * scale)
                .left()
                .color(0xF8FAFC, 1.0f)
                .draw();

            ui.text("Good for small fixed blocks.")
                .below_last(0.0f, 8.0f * scale)
                .size(w, 18.0f * scale)
                .font(12.5f * scale)
                .left()
                .color(0x94A3B8, 1.0f)
                .draw();

            const Rect subtitle = ui.last();
            const Rect action{
                subtitle.x,
                subtitle.y + subtitle.h + 12.0f * scale,
                120.0f * scale,
                36.0f * scale,
            };
            const Rect next{
                action.x + action.w + 12.0f * scale,
                action.y,
                120.0f * scale,
                36.0f * scale,
            };

            ui.button("Action").in(action).primary().draw();
            ui.button("Next").in(next).secondary().draw();

            ui.shape()
                .at(x, area.y + 128.0f * scale)
                .size(w, 80.0f * scale)
                .radius(16.0f * scale)
                .fill(0x1D4ED8, 0.18f)
                .stroke(0x60A5FA, 1.0f, 0.85f)
                .draw();

            ui.text("Rect { x, y, w, h }")
                .at(x + 16.0f * scale, area.y + 156.0f * scale)
                .size(w - 32.0f * scale, 18.0f * scale)
                .font(13.0f * scale)
                .left()
                .color(0xDBEAFE, 1.0f)
                .draw();
        });
}

void draw_anchor_example(UI& ui, const Rect& rect, float scale) {
    ui.card("Anchor .resolve()")
        .in(rect)
        .radius(22.0f * scale)
        .fill(0x121A2B)
        .stroke(0x2A3550, 1.0f)
        .begin([&](auto& card) {
            const Rect area = card.content();

            const Rect title = ui.anchor()
                                   .in(area)
                                   .left(18.0f * scale)
                                   .top(16.0f * scale)
                                   .size(area.w - 36.0f * scale, 24.0f * scale)
                                   .resolve();
            const Rect subtitle = ui.anchor()
                                      .in(area)
                                      .left(18.0f * scale)
                                      .top(48.0f * scale)
                                      .size(area.w - 36.0f * scale, 18.0f * scale)
                                      .resolve();
            const Rect badge = ui.anchor()
                                   .in(area)
                                   .right(18.0f * scale)
                                   .top(16.0f * scale)
                                   .size(72.0f * scale, 28.0f * scale)
                                   .resolve();
            const Rect stage = ui.anchor()
                                   .in(area)
                                   .left(18.0f * scale)
                                   .right(18.0f * scale)
                                   .bottom(18.0f * scale)
                                   .height(120.0f * scale)
                                   .resolve();
            const Rect floating = ui.anchor()
                                      .in(stage)
                                      .center_x()
                                      .center_y()
                                      .size(170.0f * scale, 40.0f * scale)
                                      .resolve();

            ui.text("Anchor to parent edges")
                .in(title)
                .font(18.0f * scale)
                .left()
                .color(0xF8FAFC, 1.0f)
                .draw();

            ui.text("Better when the parent may resize.")
                .in(subtitle)
                .font(12.5f * scale)
                .left()
                .color(0x94A3B8, 1.0f)
                .draw();

            ui.shape()
                .in(badge)
                .radius(999.0f)
                .fill(0x0F766E, 0.22f)
                .stroke(0x2DD4BF, 1.0f, 0.9f)
                .draw();
            ui.text("Anchor")
                .in(badge)
                .font(12.0f * scale)
                .center()
                .color(0xCCFBF1, 1.0f)
                .draw();

            ui.shape()
                .in(stage)
                .radius(16.0f * scale)
                .fill(0x7C3AED, 0.16f)
                .stroke(0xA78BFA, 1.0f, 0.85f)
                .draw();

            ui.shape()
                .in(floating)
                .radius(14.0f * scale)
                .fill(0x8B5CF6, 0.30f)
                .stroke(0xDDD6FE, 1.0f, 0.95f)
                .draw();
            ui.text("center_x() + center_y()")
                .in(floating)
                .font(13.0f * scale)
                .center()
                .color(0xF5F3FF, 1.0f)
                .draw();
        });
}

}  // namespace

int main() {
    eui::app::AppOptions options{};
    options.title = "Anchor And Position Demo";
    options.width = 1080;
    options.height = 640;
    options.vsync = true;
    options.continuous_render = true;
    options.max_fps = 240.0;
    options.text_font_family = "Segoe UI";
    options.text_font_weight = 600;

    return eui::app::run(
        [&](eui::app::FrameContext frame) {
            auto& ctx = frame.context();
            const auto metrics = frame.window_metrics();
            const float scale = std::max(1.0f, metrics.dpi_scale);

            ctx.set_theme_mode(eui::ThemeMode::Dark);
            ctx.set_primary_color(eui::rgba(0.37f, 0.62f, 0.98f, 1.0f));

            UI ui(ctx);
            const float margin = 18.0f * scale;
            const Rect frame_rect{
                margin,
                margin,
                std::max(0.0f, static_cast<float>(metrics.framebuffer_w) - margin * 2.0f),
                std::max(0.0f, static_cast<float>(metrics.framebuffer_h) - margin * 2.0f),
            };

            ui.panel("Anchor vs Position/Size")
                .in(frame_rect)
                .radius(26.0f * scale)
                .fill(0x0B1020)
                .gradient(0x121A2B, 0x0B1020)
                .stroke(0x26324A, 1.0f)
                .begin([&](auto& root) {
                    ui.column(root.content())
                        .tracks({eui::quick::px(44.0f * scale), eui::quick::fr()})
                        .gap(14.0f * scale)
                        .begin([&](auto& rows) {
                            const Rect header = rows.next();
                            const Rect body = rows.next();

                            ui.text("Left: direct coordinates. Right: anchor from parent.")
                                .in(header)
                                .font(14.0f * scale)
                                .left()
                                .color(0xA5B4CC, 1.0f)
                                .draw();

                            ui.row(body)
                                .tracks({eui::quick::fr(), eui::quick::fr()})
                                .gap(16.0f * scale)
                                .begin([&](auto& cols) {
                                    draw_manual_example(ui, cols.next(), scale);
                                    draw_anchor_example(ui, cols.next(), scale);
                                });
                        });
                });
        },
        options);
}

#if defined(_WIN32)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    return main();
}
#endif
