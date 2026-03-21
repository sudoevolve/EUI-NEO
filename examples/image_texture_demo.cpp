#define EUI_BACKEND_OPENGL
#define EUI_PLATFORM_GLFW
#include "EUI.h"

#include <algorithm>

namespace {

using eui::Rect;
using eui::quick::UI;

void draw_mode_card(UI& ui, const Rect& rect, const char* title, const char* path,
                    eui::graphics::ImageFit fit, float scale) {
    ui.card(title)
        .in(rect)
        .radius(20.0f * scale)
        .fill(0x121A2B)
        .stroke(0x2A3550, 1.0f)
        .begin([&](auto& card) {
            const Rect area = card.content();
            const Rect stage{
                area.x,
                area.y,
                area.w,
                std::max(0.0f, area.h - 26.0f * scale),
            };
            const Rect footer{
                area.x,
                area.y + area.h - 22.0f * scale,
                area.w,
                22.0f * scale,
            };

            ui.shape()
                .in(stage)
                .radius(14.0f * scale)
                .fill(0x0B1020)
                .fill_image(path)
                .image_fit(fit)
                .stroke(0x31405F, 1.0f, 0.85f)
                .draw();

            ui.text(path)
                .in(footer)
                .font(11.0f * scale)
                .left()
                .color(0x90A3BF, 1.0f)
                .draw();
        });
}

}  // namespace

int main() {
    static constexpr const char* kHeroImage = "../preview/0.jpg";
    static constexpr const char* kCoverImage = "../preview/1.jpg";
    static constexpr const char* kContainImage = "../preview/2.jpg";
    static constexpr const char* kStretchImage = "../preview/3.jpg";

    eui::app::AppOptions options{};
    options.title = "Image Texture Demo";
    options.width = 1180;
    options.height = 760;
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
            ctx.set_primary_color(eui::rgba(0.25f, 0.58f, 0.98f, 1.0f));

            UI ui(ctx);
            const float margin = 18.0f * scale;
            const Rect frame_rect{
                margin,
                margin,
                std::max(0.0f, static_cast<float>(metrics.framebuffer_w) - margin * 2.0f),
                std::max(0.0f, static_cast<float>(metrics.framebuffer_h) - margin * 2.0f),
            };

            ui.panel("Image / Texture Fill")
                .in(frame_rect)
                .radius(26.0f * scale)
                .fill(0x0B1020)
                .gradient(0x121A2B, 0x0B1020)
                .stroke(0x26324A, 1.0f)
                .begin([&](auto& root) {
                    ui.column(root.content())
                        .tracks({eui::quick::px(48.0f * scale), eui::quick::fr()})
                        .gap(16.0f * scale)
                        .begin([&](auto& rows) {
                            const Rect header = rows.next();
                            const Rect body = rows.next();

                            ui.text("Left uses ui.image(...).cover(). Right uses shape().fill_image(...) with cover / contain / stretch.")
                                .in(header)
                                .font(14.0f * scale)
                                .left()
                                .color(0xA5B4CC, 1.0f)
                                .draw();

                            ui.row(body)
                                .tracks({eui::quick::fr(1.1f), eui::quick::fr()})
                                .gap(16.0f * scale)
                                .begin([&](auto& cols) {
                                    const Rect hero = cols.next();
                                    const Rect side = cols.next();

                                    ui.card("ui.image(...)")
                                        .in(hero)
                                        .radius(22.0f * scale)
                                        .fill(0x121A2B)
                                        .stroke(0x2A3550, 1.0f)
                                        .begin([&](auto& card) {
                                            const Rect area = card.content();
                                            ui.image(kHeroImage)
                                                .in(area)
                                                .radius(18.0f * scale)
                                                .cover()
                                                .draw();
                                        });

                                    ui.column(side)
                                        .tracks({eui::quick::fr(), eui::quick::fr(), eui::quick::fr()})
                                        .gap(12.0f * scale)
                                        .begin([&](auto& stack) {
                                            draw_mode_card(ui, stack.next(), "Cover", kCoverImage,
                                                           eui::graphics::ImageFit::cover, scale);
                                            draw_mode_card(ui, stack.next(), "Contain", kContainImage,
                                                           eui::graphics::ImageFit::contain, scale);
                                            draw_mode_card(ui, stack.next(), "Stretch", kStretchImage,
                                                           eui::graphics::ImageFit::stretch, scale);
                                        });
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
