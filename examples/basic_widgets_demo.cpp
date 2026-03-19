#define EUI_BACKEND_OPENGL
#define EUI_PLATFORM_GLFW
#include "EUI.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <string>

namespace {

using eui::Rect;
using eui::quick::UI;

struct AssetPreset {
    const char* title;
    const char* detail;
};

static constexpr std::array<AssetPreset, 5> kAssetPresets{{
    {"CARD 01  Renderer Budget", "GPU budget is stable. Current pass cost is 4.8 ms with cached text atlas reuse."},
    {"CARD 02  Runtime Trace", "Navigation transitions stay below the target frame budget and no long spikes were detected."},
    {"CARD 03  Font Atlas", "Icon fallback is healthy. Glyph cache hit rate is high enough for dense dashboard screens."},
    {"CARD 04  Dirty Regions", "Partial redraw remains active. Region merge quality is acceptable for editor and form panels."},
    {"CARD 05  Motion Curves", "Micro motion still needs final polish, but easing timing already feels restrained and consistent."},
}};

struct BasicPalette {
    std::uint32_t shell_top;
    std::uint32_t shell_bottom;
    std::uint32_t shell_stroke;
    std::uint32_t card_fill;
    std::uint32_t card_stroke;
};

BasicPalette make_basic_palette(bool dark_mode) {
    if (dark_mode) {
        return BasicPalette{
            0x0A1119,
            0x0E1722,
            0x1B2938,
            0x162230,
            0x2B3F55,
        };
    }
    return BasicPalette{
        0xF6FAFC,
        0xE7EFF5,
        0xC8D7E2,
        0xFFFFFF,
        0xD6E2EB,
    };
}

struct BasicDemoState {
    std::string handle_text{"@sudoevolve"};
    std::string email_text{"sudoevolve@gmail.com"};
    std::string story_text{
        u8"team notes: small progress every day, one bug less, one smile more.\n"
        u8"workflow: \uF0C9 nav  \uF002 find  \uF013 settings  \uF0E0 message.\n"
        u8"bonus icons: \uF060\uF061\uF002\uF013\uF005\uF0E0\uF2BD.\n"
        u8"long text keeps wrapping at the edge and you can scroll to read more lines.\n"
        u8"have a great day, keep coding, and let every screen feel friendly."
    };
    int single_tab{2};
    std::array<bool, 3> multi_tabs{{true, true, false}};
    float opacity{11.0f};
    float exposure{169.0f};
    float gamma{2.20f};
    float loading_assets{0.18f};
    bool advanced_open{true};
    bool dark_mode{true};
    bool playback_active{false};
    std::size_t selected_asset{0u};
    std::string last_action{"Primary action armed."};
};

std::string format_dpi(float scale) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "DPI %.2fx", scale);
    return std::string(buffer);
}

std::string format_percent(float value) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.0f%%", std::clamp(value, 0.0f, 1.0f) * 100.0f);
    return std::string(buffer);
}

std::string playback_button_label(const BasicDemoState& state) {
    if (state.playback_active) {
        return "PAUSE";
    }
    if (state.loading_assets >= 0.999f) {
        return "RESTART";
    }
    return "START";
}

std::string playback_status(const BasicDemoState& state) {
    if (state.playback_active) {
        return "Running  " + format_percent(state.loading_assets);
    }
    if (state.loading_assets >= 0.999f) {
        return "Complete  100%";
    }
    return "Paused  " + format_percent(state.loading_assets);
}

void advance_loading(BasicDemoState& state, float dt) {
    if (!state.playback_active) {
        return;
    }

    state.loading_assets = std::min(1.0f, state.loading_assets + std::max(0.0f, dt) * 0.24f);
    if (state.loading_assets >= 0.999f) {
        state.loading_assets = 1.0f;
        state.playback_active = false;
        state.last_action = "Asset loading completed.";
    }
}

template <typename Fn>
void draw_demo_card(UI& ui, const Rect& rect, float scale, const BasicPalette& palette, Fn&& fn) {
    const auto dp = [scale](float value) { return value * scale; };
    ui.card()
        .in(rect)
        .padding(dp(10.0f))
        .radius(dp(16.0f))
        .fill(palette.card_fill)
        .stroke(palette.card_stroke, 1.0f)
        .begin(std::forward<Fn>(fn));
}

void draw_overview_card(UI& ui, BasicDemoState& state, const Rect& rect, float scale, float dpi_scale,
                        const BasicPalette& palette) {
    const auto dp = [scale](float value) { return value * scale; };

    draw_demo_card(ui, rect, scale, palette, [&](auto& card) {
        const Rect hero = card.content();
        const auto split = ui.split_h_ratio(hero, 0.56f, dp(10.0f));
        ui.scope(split.first, [&](auto&) {
            ui.label("OVERVIEW").font(dp(12.0f)).muted().height(dp(16.0f)).draw();
            ui.spacer(dp(4.0f));
            ui.label("EUI WIDGETS").font(dp(22.0f)).height(dp(24.0f)).draw();
            ui.spacer(dp(6.0f));
            ui.label(format_dpi(dpi_scale)).font(dp(13.0f)).muted().height(dp(16.0f)).draw();
        });

        ui.scope(split.second, [&](auto&) {
            ui.label("THEME").font(dp(12.0f)).muted().height(dp(16.0f)).draw();
            ui.spacer(dp(6.0f));
            if (ui.button(state.dark_mode ? "LIGHT MODE" : "DARK MODE")
                    .secondary()
                    .height(dp(38.0f))
                    .draw()) {
                state.dark_mode = !state.dark_mode;
                state.last_action = state.dark_mode ? "Dark mode enabled." : "Light mode enabled.";
            }
        });
    });
}

void draw_contact_card(UI& ui, BasicDemoState& state, const Rect& rect, float scale, const BasicPalette& palette) {
    const auto dp = [scale](float value) { return value * scale; };

    draw_demo_card(ui, rect, scale, palette, [&](auto&) {
        ui.label("CONTACT").font(dp(16.0f)).height(dp(18.0f)).draw();
        ui.spacer(dp(8.0f));
        ui.row()
            .items({ui.flex(1.0f), ui.flex(1.0f)})
            .gap(dp(8.0f))
            .align_center()
            .begin([&] {
                ui.readonly("", state.handle_text).height(dp(38.0f)).muted().value_scale(1.0f).draw();
                ui.input("", state.email_text).height(dp(38.0f)).placeholder("sudoevolve@gmail.com").draw();
            });
    });
}

void draw_editor_card(UI& ui, BasicDemoState& state, const Rect& rect, float scale, const BasicPalette& palette) {
    const auto dp = [scale](float value) { return value * scale; };

    draw_demo_card(ui, rect, scale, palette, [&](auto& card) {
        ui.label("TEXT EDITOR").font(dp(16.0f)).height(dp(18.0f)).draw();
        ui.spacer(dp(6.0f));
        const float editor_height = std::max(dp(108.0f), card.content().h - dp(30.0f));
        if (ui.text_area("STORY", state.story_text).height(editor_height).draw()) {
            state.last_action = "Story text updated.";
        }
    });
}

void draw_icon_demo_card(UI& ui, BasicDemoState& state, const Rect& rect, float scale, const BasicPalette& palette) {
    const auto dp = [scale](float value) { return value * scale; };
    static constexpr std::array<const char*, 6> kIconButtons{{
        u8"\uF0C9  MENU",
        u8"\uF060  BACK",
        u8"\uF061  FORWARD",
        u8"\uF002  SEARCH",
        u8"\uF013  SETTINGS",
        u8"\uF0E0  MAIL",
    }};

    draw_demo_card(ui, rect, scale, palette, [&](auto&) {
        ui.label("ICON DEMO").font(dp(16.0f)).height(dp(18.0f)).draw();
        ui.spacer(dp(6.0f));

        for (std::size_t row = 0; row < kIconButtons.size(); row += 2u) {
            ui.row()
                .items({ui.flex(1.0f), ui.flex(1.0f)})
                .gap(dp(8.0f))
                .align_center()
                .begin([&] {
                    if (ui.button(kIconButtons[row]).secondary().height(dp(34.0f)).draw()) {
                        state.last_action = std::string("Pressed ") + kIconButtons[row] + ".";
                    }
                    if (ui.button(kIconButtons[row + 1u]).secondary().height(dp(34.0f)).draw()) {
                        state.last_action = std::string("Pressed ") + kIconButtons[row + 1u] + ".";
                    }
                });
            if (row + 2u < kIconButtons.size()) {
                ui.spacer(dp(6.0f));
            }
        }
    });
}

void draw_tabs_card(UI& ui, BasicDemoState& state, const Rect& rect, float scale, const BasicPalette& palette) {
    const auto dp = [scale](float value) { return value * scale; };
    static constexpr std::array<const char*, 3> kSingleTabs{{"EFFICIENCY", "QUALITY", "SPEED"}};
    static constexpr std::array<const char*, 3> kMultiTabs{{"REACT", "VUE", "SVELTE"}};

    draw_demo_card(ui, rect, scale, palette, [&](auto&) {
        ui.label("TAB SELECTION").font(dp(16.0f)).height(dp(18.0f)).draw();
        ui.spacer(dp(4.0f));
        ui.label("SINGLE SELECT (RADIO)").font(dp(11.0f)).muted().height(dp(14.0f)).draw();
        ui.spacer(dp(6.0f));

        ui.row()
            .items({ui.flex(1.0f), ui.flex(1.0f), ui.flex(1.0f)})
            .gap(dp(6.0f))
            .align_center()
            .begin([&] {
                for (int index = 0; index < 3; ++index) {
                    if (ui.tab(kSingleTabs[static_cast<std::size_t>(index)], state.single_tab == index)
                            .height(dp(36.0f))
                            .draw()) {
                        state.single_tab = index;
                        state.last_action = std::string("Single tab set to ") + kSingleTabs[static_cast<std::size_t>(index)] + ".";
                    }
                }
            });

        ui.spacer(dp(8.0f));
        ui.label("MULTI SELECT (CHECKBOX)").font(dp(11.0f)).muted().height(dp(14.0f)).draw();
        ui.spacer(dp(6.0f));
        ui.row()
            .items({ui.flex(1.0f), ui.flex(1.0f), ui.flex(1.0f)})
            .gap(dp(6.0f))
            .align_center()
            .begin([&] {
                for (int index = 0; index < 3; ++index) {
                    const std::size_t slot = static_cast<std::size_t>(index);
                    if (ui.tab(kMultiTabs[slot], state.multi_tabs[slot]).height(dp(36.0f)).draw()) {
                        state.multi_tabs[slot] = !state.multi_tabs[slot];
                        state.last_action = std::string("Toggled ") + kMultiTabs[slot] + ".";
                    }
                }
            });
    });
}

void draw_slider_card(UI& ui, BasicDemoState& state, const Rect& rect, float scale, const BasicPalette& palette) {
    const auto dp = [scale](float value) { return value * scale; };

    draw_demo_card(ui, rect, scale, palette, [&](auto&) {
        ui.label("SLIDERS").font(dp(16.0f)).height(dp(18.0f)).draw();
        ui.spacer(dp(6.0f));
        if (ui.slider("OPACITY", state.opacity).range(0.0f, 20.0f).decimals(0).height(dp(40.0f)).draw()) {
            state.last_action = "Opacity slider adjusted.";
        }
        ui.spacer(dp(4.0f));
        if (ui.slider("EXPOSURE", state.exposure).range(0.0f, 255.0f).decimals(0).height(dp(40.0f)).draw()) {
            state.last_action = "Exposure slider adjusted.";
        }
    });
}

void draw_buttons_card(UI& ui, BasicDemoState& state, const Rect& rect, float scale, const BasicPalette& palette) {
    const auto dp = [scale](float value) { return value * scale; };

    draw_demo_card(ui, rect, scale, palette, [&](auto&) {
        ui.label("BUTTONS").font(dp(16.0f)).height(dp(18.0f)).draw();
        ui.spacer(dp(6.0f));
        ui.row()
            .items({ui.flex(1.0f), ui.flex(1.0f), ui.flex(1.0f)})
            .gap(dp(8.0f))
            .align_center()
            .begin([&] {
                if (ui.button("PRIMARY ACTION").primary().height(dp(36.0f)).draw()) {
                    state.last_action = "Primary action fired.";
                }
                if (ui.button("SECONDARY").secondary().height(dp(36.0f)).draw()) {
                    state.last_action = "Secondary action fired.";
                }
                if (ui.button("GHOST").ghost().height(dp(36.0f)).draw()) {
                    state.last_action = "Ghost action fired.";
                }
            });
    });
}

void draw_dropdown_card(UI& ui, BasicDemoState& state, const Rect& rect, float scale, const BasicPalette& palette) {
    const auto dp = [scale](float value) { return value * scale; };

    draw_demo_card(ui, rect, scale, palette, [&](auto& card) {
        ui.label("DROPDOWN PANEL").font(dp(16.0f)).height(dp(18.0f)).draw();
        ui.spacer(dp(8.0f));

        const float max_body_height = std::max(dp(96.0f), card.content().h - dp(54.0f));
        const float body_height = std::clamp(card.content().h - dp(76.0f), dp(96.0f), max_body_height);
        ui.dropdown("ADVANCED SETTINGS", state.advanced_open)
            .body_height(body_height)
            .padding(dp(12.0f))
            .begin([&](auto&) {
                const float scroll_height = std::max(dp(72.0f), body_height - dp(16.0f));
                ui.scroll_area("advanced-assets")
                    .height(scroll_height)
                    .padding(dp(5.0f))
                    .scrollbar_width(dp(7.0f))
                    .min_thumb_height(dp(24.0f))
                    .wheel_step(dp(26.0f))
                    .begin([&](auto&) {
                        for (std::size_t index = 0; index < kAssetPresets.size(); ++index) {
                            const eui::ButtonStyle style =
                                (state.selected_asset == index) ? eui::ButtonStyle::Primary : eui::ButtonStyle::Secondary;
                            auto button = ui.button(kAssetPresets[index].title);
                            button.height(dp(38.0f));
                            if (style == eui::ButtonStyle::Primary) {
                                button.primary();
                            } else {
                                button.secondary();
                            }
                            if (button.draw()) {
                                state.selected_asset = index;
                                state.last_action = std::string("Selected ") + kAssetPresets[index].title + ".";
                            }
                            if (index + 1u < kAssetPresets.size()) {
                                ui.spacer(dp(6.0f));
                            }
                        }
                    });
            });
    });
}

void draw_input_progress_card(UI& ui, BasicDemoState& state, const Rect& rect, float scale,
                              const BasicPalette& palette) {
    const auto dp = [scale](float value) { return value * scale; };

    draw_demo_card(ui, rect, scale, palette, [&](auto&) {
        ui.label("INPUT + PROGRESS").font(dp(16.0f)).height(dp(18.0f)).draw();
        ui.spacer(dp(6.0f));
        ui.row()
            .items({ui.flex(1.0f), ui.fixed(dp(118.0f))})
            .gap(dp(8.0f))
            .align_center()
            .begin([&] {
                if (ui.input_float("GAMMA", state.gamma).range(0.50f, 4.00f).decimals(2).height(dp(36.0f)).draw()) {
                    state.last_action = "Gamma input updated.";
                }

                const std::string button_label = playback_button_label(state);
                auto button = ui.button(button_label);
                button.height(dp(36.0f));
                if (state.playback_active) {
                    button.primary();
                } else {
                    button.secondary();
                }
                if (button.draw()) {
                    if (state.playback_active) {
                        state.playback_active = false;
                        state.last_action = "Playback paused.";
                    } else {
                        if (state.loading_assets >= 0.999f) {
                            state.loading_assets = 0.0f;
                        }
                        state.playback_active = true;
                        state.last_action = "Playback started.";
                    }
                }
            });
        ui.spacer(dp(8.0f));
        ui.progress("LOADING ASSETS...", state.loading_assets).height(dp(10.0f)).draw();
        ui.spacer(dp(8.0f));
        ui.readonly("STATUS", playback_status(state)).height(dp(36.0f)).value_scale(0.95f).muted(false).draw();
    });
}

}  // namespace

int main() {
    BasicDemoState state{};

    eui::app::AppOptions options{};
    options.title = "EUI Basic Widgets Demo";
    options.width = 1180;
    options.height = 780;
    options.vsync = true;
    options.continuous_render = false;
    options.max_fps = 120.0;
    options.text_font_family = "Segoe UI";
    options.text_font_weight = 600;
    options.icon_font_family = "Font Awesome 7 Free Solid";
    options.icon_font_file = "include/Font Awesome 7 Free-Solid-900.otf";
    options.enable_icon_font_fallback = true;

    return eui::app::run(
        [&](eui::app::FrameContext frame) {
            auto& ctx = frame.context();
            const auto metrics = frame.window_metrics();
            const float scale = std::max(1.0f, metrics.dpi_scale);
            const auto dp = [scale](float value) { return value * scale; };
            const BasicPalette palette = make_basic_palette(state.dark_mode);

            advance_loading(state, frame.delta_seconds_f32());

            ctx.set_theme_mode(state.dark_mode ? eui::ThemeMode::Dark : eui::ThemeMode::Light);
            ctx.set_primary_color(state.dark_mode ? eui::rgba(0.31f, 0.83f, 0.86f, 1.0f)
                                                  : eui::rgba(0.16f, 0.63f, 0.68f, 1.0f));
            ctx.set_corner_radius(dp(12.0f));

            UI ui(ctx);
            const float margin = dp(14.0f);
            const Rect frame_rect{
                margin,
                margin,
                std::max(0.0f, static_cast<float>(metrics.framebuffer_w) - margin * 2.0f),
                std::max(0.0f, static_cast<float>(metrics.framebuffer_h) - margin * 2.0f),
            };

            ui.panel()
                .in(frame_rect)
                .padding(dp(14.0f))
                .radius(dp(22.0f))
                .gradient(palette.shell_top, palette.shell_bottom)
                .stroke(palette.shell_stroke, 1.0f)
                .begin([&](auto& root) {
                    const auto columns = root.split_h_ratio(root.content(), 0.50f, dp(12.0f));
                    ui.scope(columns.first, [&](auto& left) {
                        const Rect column = left.content();
                        const float gap = dp(10.0f);
                        const float overview_height = std::clamp(column.h * 0.14f, dp(96.0f), dp(118.0f));
                        const float editor_height = std::clamp(column.h * 0.21f, dp(142.0f), dp(188.0f));
                        const float tabs_height = std::clamp(column.h * 0.27f, dp(182.0f), dp(224.0f));

                        const auto split1 = ui.split_v(column, overview_height, gap);
                        const auto split2 = ui.split_v(split1.second, editor_height, gap);
                        const auto split3 = ui.split_v(split2.second, tabs_height, gap);

                        draw_overview_card(ui, state, split1.first, scale, metrics.dpi_scale, palette);
                        draw_editor_card(ui, state, split2.first, scale, palette);
                        draw_tabs_card(ui, state, split3.first, scale, palette);
                        draw_dropdown_card(ui, state, split3.second, scale, palette);
                    });

                    ui.scope(columns.second, [&](auto& right) {
                        const float gap = dp(10.0f);
                        const Rect contact = right.dock_top(dp(92.0f), gap);
                        const Rect progress = right.dock_bottom(dp(176.0f), gap);
                        const Rect buttons = right.dock_bottom(dp(90.0f), gap);
                        const Rect sliders = right.dock_bottom(dp(144.0f), gap);
                        const Rect icons = right.content();

                        draw_contact_card(ui, state, contact, scale, palette);
                        draw_icon_demo_card(ui, state, icons, scale, palette);
                        draw_slider_card(ui, state, sliders, scale, palette);
                        draw_buttons_card(ui, state, buttons, scale, palette);
                        draw_input_progress_card(ui, state, progress, scale, palette);
                    });
                });

            if (state.playback_active) {
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
