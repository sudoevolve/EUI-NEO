#define EUI_BACKEND_OPENGL
#define EUI_PLATFORM_GLFW
#include "EUI.h"

#include <algorithm>
#include <string>

using eui::Rect;
using eui::quick::UI;

namespace {

enum class SidebarPage {
    Overview,
    Library,
    Inbox,
    Settings,
};

struct SidebarDemoState {
    bool dark_mode{true};
    SidebarPage active_page{SidebarPage::Overview};
    float page_anim{1.0f};
    float sync_progress{0.72f};
    float release_progress{0.58f};
    float inbox_progress{0.34f};
};

struct SidebarPalette {
    std::uint32_t shell_top{};
    std::uint32_t shell_bottom{};
    std::uint32_t shell_stroke{};
    std::uint32_t sidebar_top{};
    std::uint32_t sidebar_bottom{};
    std::uint32_t sidebar_stroke{};
    std::uint32_t page_top{};
    std::uint32_t page_bottom{};
    std::uint32_t page_stroke{};
    std::uint32_t metric_fill{};
    std::uint32_t metric_stroke{};
    std::uint32_t subtle_text{};
    std::uint32_t title_text{};
    std::uint32_t kicker_text{};
};

float clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

SidebarPalette make_sidebar_palette(bool dark_mode) {
    if (dark_mode) {
        return SidebarPalette{
            0x0D1624,
            0x111D2E,
            0x2A4159,
            0x111C2C,
            0x162439,
            0x2C455F,
            0x121C2C,
            0x162234,
            0x2A425D,
            0x17263A,
            0x325271,
            0x9FB0C4,
            0xE7F1FF,
            0x72859C,
        };
    }

    return SidebarPalette{
        0xF5F8FC,
        0xEDF3F8,
        0xCFDAE6,
        0xFFFFFF,
        0xF4F7FB,
        0xD4DFEA,
        0xFFFFFF,
        0xF7FAFD,
        0xD2DDE8,
        0xF2F6FA,
        0xD6E0EA,
        0x5E7085,
        0x182638,
        0x8090A4,
    };
}

void draw_sidebar_entry(UI& ui, SidebarDemoState& state, SidebarPage page,
                        std::string_view label, float height) {
    const bool selected = state.active_page == page;
    if ((selected ? ui.button(label).primary() : ui.button(label).ghost()).height(height).draw() && !selected) {
        state.active_page = page;
        state.page_anim = 0.0f;
    }
}

void draw_metric_card(UI& ui, const Rect& rect, std::string_view title,
                      std::string_view value, std::string_view note, float scale,
                      const SidebarPalette& palette) {
    const auto dp = [scale](float v) { return v * scale; };
    ui.metric(title, value)
        .in(rect)
        .radius(dp(18.0f))
        .fill(palette.metric_fill)
        .stroke(palette.metric_stroke, 1.0f, 0.74f)
        .shadow(0.0f, 0.0f, 0.0f, 0x000000, 0.0f)
        .caption(note)
        .draw();
}

void draw_text_panel(UI& ui, const Rect& rect, std::string_view title, std::string_view body,
                     float scale, const SidebarPalette& palette) {
    const auto dp = [scale](float v) { return v * scale; };
    ui.card(title)
        .in(rect)
        .radius(dp(18.0f))
        .fill(palette.metric_fill)
        .stroke(palette.metric_stroke, 1.0f)
        .shadow(0.0f, 0.0f, 0.0f, 0x000000, 0.0f)
        .begin([&](auto& card) {
            ui.text(body)
                .in(card.content())
                .font(dp(12.0f))
                .color(palette.subtle_text)
                .draw();
        });
}

void draw_overview_page(UI& ui, SidebarDemoState& state, const Rect& rect, float scale,
                        const SidebarPalette& palette) {
    const auto dp = [scale](float v) { return v * scale; };
    ui.card("OVERVIEW")
        .in(rect)
        .radius(dp(24.0f))
        .gradient(palette.page_top, palette.page_bottom)
        .stroke(palette.page_stroke, 1.0f)
        .shadow(0.0f, 0.0f, 0.0f, 0x000000, 0.0f)
        .begin([&](auto& card) {
            ui.text("A quick-syntax sidebar layout with restrained page reveal.")
                .in(card.dock_top(dp(18.0f), dp(12.0f)))
                .font(dp(12.0f))
                .color(palette.subtle_text)
                .draw();

            const Rect metrics = card.dock_top(dp(112.0f), dp(12.0f));
            const auto pair = ui.split_h_ratio(metrics, 0.50f, dp(8.0f));
            draw_metric_card(ui, pair.first, "Workspace Sync", "72%", "Assets ready for review", scale, palette);
            draw_metric_card(ui, pair.second, "Release Ready", "58%", "Docs pending", scale, palette);

            const Rect progress_rect = card.dock_top(dp(96.0f), dp(12.0f));
            ui.scope(progress_rect, [&](auto&) {
                ui.progress("Workspace Sync", state.sync_progress).height(dp(8.0f)).draw();
                ui.spacer(dp(8.0f));
                ui.progress("Release Readiness", state.release_progress).height(dp(8.0f)).draw();
                ui.spacer(dp(8.0f));
                ui.progress("Inbox Processing", state.inbox_progress).height(dp(8.0f)).draw();
            });

            draw_text_panel(
                ui, card.content(), "TODAY",
                "Refine motion curves for navigation.\n"
                "Polish spacing and hierarchy.\n"
                "Check icon and text weight balance.\n"
                "Keep transitions short and stable.",
                scale, palette);
        });
}

void draw_library_page(UI& ui, const Rect& rect, float scale, const SidebarPalette& palette) {
    const auto dp = [scale](float v) { return v * scale; };
    ui.card("LIBRARY")
        .in(rect)
        .radius(dp(24.0f))
        .gradient(palette.page_top, palette.page_bottom)
        .stroke(palette.page_stroke, 1.0f)
        .shadow(0.0f, 0.0f, 0.0f, 0x000000, 0.0f)
        .begin([&](auto& card) {
            ui.text("A static content page still reads cleanly with the same shell.")
                .in(card.dock_top(dp(18.0f), dp(12.0f)))
                .font(dp(12.0f))
                .color(palette.subtle_text)
                .draw();

            const Rect info_rect = card.dock_top(dp(172.0f), dp(12.0f));
            ui.card("COLLECTION")
                .in(info_rect)
                .radius(dp(18.0f))
                .fill(palette.metric_fill)
                .stroke(palette.metric_stroke, 1.0f)
                .shadow(0.0f, 0.0f, 0.0f, 0x000000, 0.0f)
                .begin([&](auto&) {
                    ui.readonly("Current Collection", "Brand Motion Kit").height(dp(34.0f)).muted(false).draw();
                    ui.spacer(dp(8.0f));
                    ui.readonly("Latest Export", "sidebar-transition-v4.fig").height(dp(34.0f)).muted(false).draw();
                    ui.spacer(dp(8.0f));
                    ui.progress("Asset Coverage", 0.83f).height(dp(8.0f)).draw();
                });

            draw_text_panel(
                ui, card.content(), "PINNED NOTES",
                "Use soft reveal on entry.\n"
                "Avoid hover zoom on navigation items.\n"
                "Keep the right page readable during motion.\n"
                "Prefer subtle vertical drift over large transforms.",
                scale, palette);
        });
}

void draw_inbox_page(UI& ui, const Rect& rect, float scale, const SidebarPalette& palette) {
    const auto dp = [scale](float v) { return v * scale; };
    ui.card("INBOX")
        .in(rect)
        .radius(dp(24.0f))
        .gradient(palette.page_top, palette.page_bottom)
        .stroke(palette.page_stroke, 1.0f)
        .shadow(0.0f, 0.0f, 0.0f, 0x000000, 0.0f)
        .begin([&](auto& card) {
            ui.text("Message stack with compact density.")
                .in(card.dock_top(dp(18.0f), dp(12.0f)))
                .font(dp(12.0f))
                .color(palette.subtle_text)
                .draw();

            const Rect message_rect = card.dock_top(dp(176.0f), dp(12.0f));
            ui.card("MESSAGES")
                .in(message_rect)
                .radius(dp(18.0f))
                .fill(palette.metric_fill)
                .stroke(palette.metric_stroke, 1.0f)
                .shadow(0.0f, 0.0f, 0.0f, 0x000000, 0.0f)
                .begin([&](auto&) {
                    ui.readonly("08:42", "Design review moved to 10:30").height(dp(34.0f)).muted(false).draw();
                    ui.spacer(dp(8.0f));
                    ui.readonly("09:15", "Sidebar motion pass approved").height(dp(34.0f)).muted(false).draw();
                    ui.spacer(dp(8.0f));
                    ui.readonly("09:50", "Need final QA build after polish").height(dp(34.0f)).muted(false).draw();
                });

            const auto row = ui.split_h_ratio(card.content(), 0.50f, dp(8.0f));
            draw_metric_card(ui, row.first, "Priority", "05", "Requires response", scale, palette);
            draw_metric_card(ui, row.second, "Resolved", "19", "Closed today", scale, palette);
        });
}

void draw_settings_page(UI& ui, SidebarDemoState& state, const Rect& rect, float scale,
                        const SidebarPalette& palette) {
    const auto dp = [scale](float v) { return v * scale; };
    ui.card("SETTINGS")
        .in(rect)
        .radius(dp(24.0f))
        .gradient(palette.page_top, palette.page_bottom)
        .stroke(palette.page_stroke, 1.0f)
        .shadow(0.0f, 0.0f, 0.0f, 0x000000, 0.0f)
        .begin([&](auto& card) {
            ui.text("Keep the right page simple and readable.")
                .in(card.dock_top(dp(18.0f), dp(12.0f)))
                .font(dp(12.0f))
                .color(palette.subtle_text)
                .draw();

            const Rect info_rect = card.dock_top(dp(220.0f), dp(12.0f));
            ui.card("CURRENT")
                .in(info_rect)
                .radius(dp(18.0f))
                .fill(palette.metric_fill)
                .stroke(palette.metric_stroke, 1.0f)
                .shadow(0.0f, 0.0f, 0.0f, 0x000000, 0.0f)
                .begin([&](auto&) {
                    ui.readonly("Theme", state.dark_mode ? "Dark" : "Light").height(dp(34.0f)).muted(false).draw();
                    ui.spacer(dp(8.0f));
                    ui.readonly("Navigation Motion", "Fade + slight upward drift").height(dp(34.0f)).muted(false).draw();
                    ui.spacer(dp(8.0f));
                    ui.readonly("Hover Behavior", "No scale, color only").height(dp(34.0f)).muted(false).draw();
                    ui.spacer(dp(8.0f));
                    ui.progress("Visual Polish", 0.91f).height(dp(8.0f)).draw();
                });

            draw_text_panel(
                ui, card.content(), "GUIDELINES",
                "Keep hover feedback readable but stable.\n"
                "Use press motion sparingly.\n"
                "Animate container reveal, not every pixel.\n"
                "Let the content settle quickly after the switch.",
                scale, palette);
        });
}

}  // namespace

int main() {
    SidebarDemoState state{};

    eui::app::AppOptions options{};
    options.title = "EUI Sidebar Navigation Demo";
    options.width = 1080;
    options.height = 720;
    options.vsync = true;
    options.continuous_render = false;
    options.max_fps = 120.0;

    return eui::app::run(
        [&](eui::app::FrameContext frame) {
            auto& ctx = frame.context();
            const auto metrics = frame.window_metrics();
            const float scale = std::max(1.0f, metrics.dpi_scale);
            const auto dp = [scale](float v) { return v * scale; };
            const SidebarPalette palette = make_sidebar_palette(state.dark_mode);

            ctx.set_theme_mode(state.dark_mode ? eui::ThemeMode::Dark : eui::ThemeMode::Light);
            ctx.set_primary_color(eui::rgba(0.30f, 0.74f, 0.77f, 1.0f));
            ctx.set_corner_radius(dp(12.0f));

            UI ui(ctx);
            const float margin = dp(18.0f);
            const Rect frame_rect{
                margin,
                margin,
                std::max(0.0f, static_cast<float>(metrics.framebuffer_w) - margin * 2.0f),
                std::max(0.0f, static_cast<float>(metrics.framebuffer_h) - margin * 2.0f),
            };

            ui.panel()
                .in(frame_rect)
                .padding(dp(16.0f))
                .radius(dp(30.0f))
                .gradient(palette.shell_top, palette.shell_bottom)
                .stroke(palette.shell_stroke, 1.0f)
                .begin([&](auto& root) {
                    const Rect root_rect = root.content();
                    const float sidebar_target = std::clamp(root_rect.w * 0.23f, dp(188.0f), dp(248.0f));
                    const float sidebar_width =
                        std::min(sidebar_target, std::max(dp(170.0f), root_rect.w - dp(340.0f)));
                    const Rect sidebar_rect = root.dock_left(sidebar_width, dp(14.0f));
                    const Rect content_rect = root.content();

                    ui.card()
                        .in(sidebar_rect)
                        .radius(dp(24.0f))
                        .gradient(palette.sidebar_top, palette.sidebar_bottom)
                        .stroke(palette.sidebar_stroke, 1.0f)
                        .begin([&](auto& card) {
                            ui.text("EUI NAV")
                                .in(card.dock_top(dp(30.0f)))
                                .font(dp(24.0f))
                                .color(palette.title_text)
                                .draw();
                            ui.text("Minimal sidebar and page switch sample")
                                .in(card.dock_top(dp(20.0f), dp(12.0f)))
                                .font(dp(12.0f))
                                .color(palette.subtle_text)
                                .draw();

                            ui.text("PAGES")
                                .in(card.dock_top(dp(16.0f), dp(8.0f)))
                                .font(dp(11.0f))
                                .color(palette.kicker_text)
                                .draw();

                            draw_sidebar_entry(ui, state, SidebarPage::Overview, "Overview", dp(34.0f));
                            ui.spacer(dp(8.0f));
                            draw_sidebar_entry(ui, state, SidebarPage::Library, "Library", dp(34.0f));
                            ui.spacer(dp(8.0f));
                            draw_sidebar_entry(ui, state, SidebarPage::Inbox, "Inbox", dp(34.0f));
                            ui.spacer(dp(8.0f));
                            draw_sidebar_entry(ui, state, SidebarPage::Settings, "Settings", dp(34.0f));
                            ui.spacer(dp(14.0f));
                            ui.progress("Sync", state.sync_progress).height(dp(6.0f)).draw();
                            ui.spacer(dp(12.0f));
                            if (ui.button(state.dark_mode ? "Light Theme" : "Dark Theme").secondary().height(dp(34.0f)).draw()) {
                                state.dark_mode = !state.dark_mode;
                            }
                        });

                    const float reveal = state.page_anim * state.page_anim * (3.0f - 2.0f * state.page_anim);
                    const float reveal_offset = (1.0f - reveal) * dp(12.0f);
                    ctx.set_global_alpha(clamp01(reveal));
                    const Rect animated_rect{
                        content_rect.x,
                        content_rect.y + reveal_offset,
                        content_rect.w,
                        content_rect.h,
                    };

                    ui.clip(content_rect, [&] {
                        switch (state.active_page) {
                            case SidebarPage::Overview:
                                draw_overview_page(ui, state, animated_rect, scale, palette);
                                break;
                            case SidebarPage::Library:
                                draw_library_page(ui, animated_rect, scale, palette);
                                break;
                            case SidebarPage::Inbox:
                                draw_inbox_page(ui, animated_rect, scale, palette);
                                break;
                            case SidebarPage::Settings:
                                draw_settings_page(ui, state, animated_rect, scale, palette);
                                break;
                        }
                    });
                    ctx.set_global_alpha(1.0f);
                });

            if (state.page_anim < 1.0f) {
                state.page_anim = std::min(1.0f, state.page_anim + frame.delta_seconds_f32() * 2.4f);
                frame.request_next_frame();
            }
        },
        options);
}

#if defined(_WIN32)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    return main();
}
#endif
