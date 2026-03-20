#define EUI_BACKEND_OPENGL
#define EUI_PLATFORM_GLFW
#include "EUI.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <regex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <shellapi.h>
#include <windows.h>
#endif

namespace {

using eui::Rect;
using eui::quick::UI;

struct GalleryState {
    int selected_page{0};
    int selected_animation_demo{0};
    int selected_control_icon{0};
    float control_slider{0.64f};
    float progress_ratio{0.72f};
    float layout_gap{18.0f};
    float layout_radius{18.0f};
    float settings_blur{18.0f};
    bool light_mode{false};
    bool controls_dropdown_open{false};
    bool design_icon_library_open{false};
    int controls_mode{0};
    int design_icon_page{0};
    int accent_index{0};
    float custom_accent_r{96.0f};
    float custom_accent_g{165.0f};
    float custom_accent_b{250.0f};
    std::array<bool, 3> controls_multi_select{{true, false, true}};
    std::string search_text{"Search gallery"};
    std::string notes_text{
        "Panels, text, blur and motion all run through the same immediate-mode renderer.\n\n"
        "Use this editor to test multiline input, line wrapping and longer form content while you tune the surrounding layout.\n\n"
        "The gallery is meant to double as a developer reference, so the default text is intentionally longer than a placeholder.\n\n"
        "Try editing this copy, pasting multiple paragraphs, and scrolling through the document while sliders and theme settings update around it.\n\n"
        "A healthy default body here is useful because developers can immediately validate caret movement, selection painting, wheel scrolling, clipboard flow and long-form wrapping without seeding data first.\n\n"
        "This extra block is only here to guarantee the scroll bar stays active during demo startup."
    };
};

struct DemoDescriptor {
    const char* title;
    const char* summary;
    const char* api_label;
};

struct PageDescriptor {
    const char* title;
    const char* summary;
    const char* api_label;
};

struct IconDescriptor {
    const char* label;
    const char* usage;
    std::uint32_t codepoint;
};

struct IconLibraryEntry {
    std::string label;
    std::string usage;
    std::uint32_t codepoint{0};
};

struct GalleryPalette {
    bool light{false};
    std::uint32_t shell_top{};
    std::uint32_t shell_bottom{};
    std::uint32_t surface{};
    std::uint32_t surface_alt{};
    std::uint32_t surface_deep{};
    std::uint32_t border{};
    std::uint32_t border_soft{};
    std::uint32_t text{};
    std::uint32_t muted{};
    std::uint32_t accent{};
    std::uint32_t accent_soft{};
    std::uint32_t grid{};
};

struct Point2 {
    float x{0.0f};
    float y{0.0f};
};

enum GalleryPageIndex {
    kPageBasicControls = 0,
    kPageDesign = 1,
    kPageLayout = 2,
    kPageAnimation = 3,
    kPageDashboard = 4,
    kPageSettings = 5,
    kPageAbout = 6,
};

static constexpr std::array<DemoDescriptor, 9> kDemos{{
    {"Translate", "Move a rounded rect on one axis", "translate() + ease_in_out"},
    {"Scale", "Grow and shrink around the center", "scale() + eased scalar"},
    {"Rotate", "Rotate a single rounded rect in place", "rotate() + linear loop"},
    {"Arc Motion", "Move on a curved arc instead of a line", "curve path + ease_out"},
    {"Bezier Path", "Follow a cubic Bezier trajectory", "Bezier path + custom cubic"},
    {"Color Shift", "Crossfade the actor between two palettes", "gradient() + color lerp"},
    {"Opacity", "Fade between solid and ghosted states", "opacity() + ease_in_out"},
    {"Blur", "Animate backdrop blur on the actor", "blur() + eased scalar"},
    {"Combo", "Translation, scale, rotate, color, opacity and blur", "combined channels"},
}};

static constexpr std::array<PageDescriptor, 7> kPages{{
    {"Basic Controls", "Buttons, sliders, inputs and text surfaces.", "buttons + inputs"},
    {"Design", "Typography, icon ids and palette tokens for fast UI work.", "fonts + icons + colors"},
    {"Layout", "Split, dock and nested panel composition.", "layout primitives"},
    {"Animation", "Motion lives here instead of in the main sidebar.", "motion showcase"},
    {"Dashboard Example", "A dashboard-style composition preview.", "dashboard preview"},
    {"Settings", "Theme mode, accent color and gallery tuning.", "theme + accent"},
    {"About", "Project identity, repository link and contact details.", "project + license"},
}};

static constexpr std::array<std::uint32_t, 6> kAccentHexes{{
    0x60A5FA,
    0x22C55E,
    0xF97316,
    0xA855F7,
    0xEAB308,
    0x14B8A6,
}};

constexpr int custom_accent_slot() {
    return static_cast<int>(kAccentHexes.size());
}

static constexpr std::array<std::uint32_t, 7> kPageIcons{{
    0xF1DEu,
    0xF1FCu,
    0xF0DBu,
    0xF061u,
    0xF080u,
    0xF013u,
    0xF05Au,
}};

float clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

float lerp(float from, float to, float t) {
    return from + (to - from) * clamp01(t);
}

Rect inset_rect(const Rect& rect, float padding_x, float padding_y) {
    return Rect{
        rect.x + padding_x,
        rect.y + padding_y,
        std::max(0.0f, rect.w - padding_x * 2.0f),
        std::max(0.0f, rect.h - padding_y * 2.0f),
    };
}

Rect inset_rect(const Rect& rect, float padding) {
    return inset_rect(rect, padding, padding);
}

std::uint32_t mix_hex(std::uint32_t lhs, std::uint32_t rhs, float t) {
    const float clamped = clamp01(t);
    const auto mix_channel = [&](int shift) {
        const float a = static_cast<float>((lhs >> shift) & 0xffu);
        const float b = static_cast<float>((rhs >> shift) & 0xffu);
        return static_cast<std::uint32_t>(std::lround(a + (b - a) * clamped));
    };
    return (mix_channel(16) << 16) | (mix_channel(8) << 8) | mix_channel(0);
}

eui::Color color_from_hex(std::uint32_t hex, float alpha = 1.0f) {
    const float r = static_cast<float>((hex >> 16) & 0xffu) / 255.0f;
    const float g = static_cast<float>((hex >> 8) & 0xffu) / 255.0f;
    const float b = static_cast<float>(hex & 0xffu) / 255.0f;
    return eui::rgba(r, g, b, alpha);
}

std::string format_percent(float ratio) {
    char buffer[32]{};
    std::snprintf(buffer, sizeof(buffer), "%.0f%%", clamp01(ratio) * 100.0f);
    return std::string(buffer);
}

std::string format_pixels(float value) {
    char buffer[32]{};
    std::snprintf(buffer, sizeof(buffer), "%.0f px", value);
    return std::string(buffer);
}

std::string format_precise_pixels(float value) {
    char buffer[32]{};
    if (std::fabs(value - std::round(value)) < 0.05f) {
        std::snprintf(buffer, sizeof(buffer), "%.0f px", value);
    } else {
        std::snprintf(buffer, sizeof(buffer), "%.1f px", value);
    }
    return std::string(buffer);
}

std::string format_hex_color(std::uint32_t hex) {
    char buffer[32]{};
    std::snprintf(buffer, sizeof(buffer), "0x%06X", hex & 0xFFFFFFu);
    return std::string(buffer);
}

std::string format_codepoint(std::uint32_t codepoint) {
    char buffer[32]{};
    std::snprintf(buffer, sizeof(buffer), "0x%04X", codepoint & 0xFFFFu);
    return std::string(buffer);
}

void open_external_uri(std::string_view uri) {
#if defined(_WIN32)
    const std::wstring wide_uri(uri.begin(), uri.end());
    ShellExecuteW(nullptr, L"open", wide_uri.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#elif defined(__APPLE__)
    const std::string command = "open \"" + std::string(uri) + "\"";
    static_cast<void>(std::system(command.c_str()));
#else
    const std::string command = "xdg-open \"" + std::string(uri) + "\" >/dev/null 2>&1";
    static_cast<void>(std::system(command.c_str()));
#endif
}

std::uint32_t pack_rgb_hex(float red, float green, float blue) {
    const auto to_u8 = [](float value) -> std::uint32_t {
        return static_cast<std::uint32_t>(std::lround(std::clamp(value, 0.0f, 255.0f)));
    };
    return (to_u8(red) << 16) | (to_u8(green) << 8) | to_u8(blue);
}

bool accent_uses_custom_rgb(const GalleryState& state) {
    return state.accent_index >= custom_accent_slot();
}

std::uint32_t custom_accent_hex(const GalleryState& state) {
    return pack_rgb_hex(state.custom_accent_r, state.custom_accent_g, state.custom_accent_b);
}

std::filesystem::path find_icon_library_json() {
    namespace fs = std::filesystem;
    fs::path current = fs::current_path();
    for (int depth = 0; depth < 6; ++depth) {
        const fs::path candidate = current / "examples" / "EUI_gallery_icons.json";
        if (fs::exists(candidate)) {
            return candidate;
        }
        if (!current.has_parent_path()) {
            break;
        }
        current = current.parent_path();
    }
    return {};
}

std::vector<IconLibraryEntry> fallback_icon_library() {
    return {
        {"Search", "Query / filter", 0xF002u},
        {"Heart", "Favorite / like", 0xF004u},
        {"Star", "Rating / featured", 0xF005u},
        {"User", "Profile / identity", 0xF007u},
        {"Home", "Landing / dashboard", 0xF015u},
        {"Clock", "Time / recent", 0xF017u},
        {"Camera", "Capture / media", 0xF030u},
        {"Image", "Artwork / preview", 0xF03Eu},
        {"Video", "Playback / motion", 0xF03Du},
        {"Check", "Confirm / success", 0xF00Cu},
        {"Close", "Dismiss / remove", 0xF00Du},
        {"Arrow", "Forward / next", 0xF061u},
    };
}

std::vector<IconLibraryEntry> load_icon_library_entries() {
    const std::filesystem::path path = find_icon_library_json();
    if (path.empty()) {
        return fallback_icon_library();
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return fallback_icon_library();
    }

    const std::string content{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    const std::regex item_re(
        R"ICON(\{\s*"label"\s*:\s*"([^"]+)"\s*,\s*"usage"\s*:\s*"([^"]+)"\s*,\s*"codepoint"\s*:\s*"(0x[0-9A-Fa-f]+)"\s*\})ICON");
    std::vector<IconLibraryEntry> result;
    for (std::sregex_iterator it(content.begin(), content.end(), item_re), end; it != end; ++it) {
        IconLibraryEntry entry{};
        entry.label = (*it)[1].str();
        entry.usage = (*it)[2].str();
        entry.codepoint = static_cast<std::uint32_t>(std::stoul((*it)[3].str(), nullptr, 16));
        result.push_back(std::move(entry));
    }
    return result.empty() ? fallback_icon_library() : result;
}

const std::vector<IconLibraryEntry>& icon_library_entries() {
    static const std::vector<IconLibraryEntry> entries = load_icon_library_entries();
    return entries;
}

std::uint32_t accent_hex(const GalleryState& state) {
    if (accent_uses_custom_rgb(state)) {
        return custom_accent_hex(state);
    }
    return kAccentHexes[static_cast<std::size_t>(std::clamp(state.accent_index, 0, static_cast<int>(kAccentHexes.size() - 1)))];
}

GalleryPalette make_gallery_palette(const GalleryState& state) {
    const std::uint32_t accent = accent_hex(state);
    if (state.light_mode) {
        return GalleryPalette{
            true,
            0xEEF4FA,
            0xE1EBF5,
            0xFBFDFF,
            0xF3F8FC,
            0xEAF1F7,
            0xC8D5E1,
            0xD6E0E8,
            0x102033,
            0x5F738A,
            accent,
            mix_hex(0xF3F8FC, accent, 0.20f),
            0xD3DEE7,
        };
    }
    return GalleryPalette{
        false,
        0x060D17,
        0x0C1625,
        0x07111C,
        0x09131F,
        0x0F1B2A,
        0x20324A,
        0x26364E,
        0xF5FAFF,
        0x8EA0BA,
        accent,
        mix_hex(0x0E1A2A, accent, 0.18f),
        0x203247,
    };
}

std::uint32_t panel_shadow_hex(const GalleryPalette& palette) {
    return palette.light ? 0x93A8BC : 0x020617;
}

std::uint32_t actor_primary_top_hex(const GalleryPalette& palette) {
    return palette.light ? mix_hex(0xFFFFFF, palette.accent, 0.34f) : mix_hex(0x72B2FF, palette.accent, 0.18f);
}

std::uint32_t actor_primary_bottom_hex(const GalleryPalette& palette) {
    return palette.light ? mix_hex(0xDCEAFE, palette.accent, 0.76f) : mix_hex(0x2563EB, palette.accent, 0.60f);
}

std::uint32_t actor_focus_top_hex(const GalleryPalette& palette) {
    return palette.light ? mix_hex(0xF8FCFF, palette.accent, 0.22f) : mix_hex(0xA7E2FF, palette.accent, 0.18f);
}

std::uint32_t actor_focus_bottom_hex(const GalleryPalette& palette) {
    return palette.light ? mix_hex(0xE2EEFF, palette.accent, 0.58f) : mix_hex(0x2563EB, palette.accent, 0.72f);
}

std::uint32_t actor_glass_top_hex(const GalleryPalette& palette) {
    return palette.light ? 0xFFFFFF : 0xF8FBFF;
}

std::uint32_t actor_glass_bottom_hex(const GalleryPalette& palette) {
    return palette.light ? mix_hex(0xEEF5FD, palette.accent, 0.22f) : mix_hex(0xD7EAFE, palette.accent, 0.16f);
}

std::uint32_t actor_stroke_hex(const GalleryPalette& palette) {
    return palette.light ? mix_hex(0xD7E6F7, palette.accent, 0.18f) : mix_hex(0xDBECFF, palette.accent, 0.16f);
}

std::uint32_t actor_inner_hex(const GalleryPalette& palette) {
    return palette.light ? 0xFFFFFF : 0xEFF6FF;
}

std::uint32_t actor_outline_hex(const GalleryPalette& palette) {
    return mix_hex(palette.border_soft, palette.accent, palette.light ? 0.24f : 0.18f);
}

std::uint32_t demo_track_hex(const GalleryPalette& palette) {
    return mix_hex(palette.surface_deep, palette.accent, palette.light ? 0.12f : 0.18f);
}

std::uint32_t demo_axis_hex(const GalleryPalette& palette) {
    return mix_hex(palette.border, palette.accent, palette.light ? 0.18f : 0.24f);
}

std::uint32_t demo_curve_hex(const GalleryPalette& palette) {
    return mix_hex(palette.border_soft, palette.accent, palette.light ? 0.42f : 0.22f);
}

std::uint32_t demo_curve_soft_hex(const GalleryPalette& palette) {
    return mix_hex(palette.border_soft, palette.accent, palette.light ? 0.34f : 0.16f);
}

std::uint32_t demo_anchor_hex(const GalleryPalette& palette) {
    return palette.light ? mix_hex(0xFFFFFF, palette.accent, 0.32f) : mix_hex(0x7DD3FC, palette.accent, 0.22f);
}

std::uint32_t demo_anchor_end_hex(const GalleryPalette& palette) {
    return mix_hex(palette.accent, 0x38BDF8, palette.light ? 0.24f : 0.52f);
}

std::uint32_t demo_anchor_muted_hex(const GalleryPalette& palette) {
    return mix_hex(palette.surface_deep, palette.text, palette.light ? 0.18f : 0.28f);
}

std::uint32_t metric_surface_top_hex(const GalleryPalette& palette) {
    return mix_hex(palette.surface_deep, palette.accent, palette.light ? 0.08f : 0.10f);
}

std::uint32_t metric_surface_bottom_hex(const GalleryPalette& palette) {
    return mix_hex(palette.surface_alt, palette.accent, palette.light ? 0.18f : 0.16f);
}

std::uint32_t metric_surface_border_hex(const GalleryPalette& palette) {
    return mix_hex(palette.border_soft, palette.accent, palette.light ? 0.34f : 0.32f);
}

std::uint32_t nav_selected_fill_hex(const GalleryPalette& palette) {
    return mix_hex(palette.surface_deep, palette.accent, palette.light ? 0.18f : 0.24f);
}

std::uint32_t nav_idle_fill_hex(const GalleryPalette& palette) {
    return palette.light ? mix_hex(palette.surface_deep, palette.border, 0.18f) : palette.surface_alt;
}

std::uint32_t preview_backdrop_hex(const GalleryPalette& palette) {
    return mix_hex(palette.surface_deep, palette.accent, palette.light ? 0.10f : 0.06f);
}

std::array<std::uint32_t, 5> blur_reference_hexes(const GalleryPalette& palette) {
    return {{
        mix_hex(palette.accent, 0x22C55E, palette.light ? 0.34f : 0.16f),
        mix_hex(palette.accent, 0xF97316, palette.light ? 0.56f : 0.74f),
        mix_hex(palette.accent, 0xA855F7, palette.light ? 0.62f : 0.76f),
        mix_hex(palette.accent, 0x38BDF8, palette.light ? 0.36f : 0.58f),
        mix_hex(palette.accent, 0xEAB308, palette.light ? 0.58f : 0.72f),
    }};
}

void draw_icon(UI& ui, std::uint32_t codepoint, const Rect& rect, std::uint32_t hex, float alpha = 1.0f) {
    ui.glyph(codepoint).in(rect).tint(hex, alpha).draw();
}

bool hovered(const eui::InputState& input, const Rect& rect) {
    return rect.contains(input.mouse_x, input.mouse_y);
}

bool clicked(const eui::InputState& input, const Rect& rect) {
    return hovered(input, rect) && input.mouse_pressed;
}

void draw_fill(UI& ui, const Rect& rect, std::uint32_t hex, float radius, float alpha = 1.0f) {
    ui.shape().in(rect).radius(radius).fill(hex, alpha).draw();
}

void draw_stroke(UI& ui, const Rect& rect, std::uint32_t hex, float radius, float width = 1.0f, float alpha = 1.0f) {
    ui.shape().in(rect).radius(radius).no_fill().stroke(hex, width, alpha).draw();
}

void draw_text_left(UI& ui, std::string_view text, const Rect& rect, float font_size, std::uint32_t hex,
                    float alpha = 1.0f) {
    ui.text(text).in(rect).font(font_size).color(hex, alpha).align(eui::TextAlign::Left).draw();
}

void draw_text_center(UI& ui, std::string_view text, const Rect& rect, float font_size, std::uint32_t hex,
                      float alpha = 1.0f) {
    ui.text(text).in(rect).font(font_size).color(hex, alpha).align(eui::TextAlign::Center).draw();
}

void draw_text_right(UI& ui, std::string_view text, const Rect& rect, float font_size, std::uint32_t hex,
                     float alpha = 1.0f) {
    ui.text(text).in(rect).font(font_size).color(hex, alpha).align(eui::TextAlign::Right).draw();
}

float font_display(float scale) {
    return 22.0f * scale;
}

float font_heading(float scale) {
    return 16.0f * scale;
}

float font_body(float scale) {
    return 14.0f * scale;
}

float font_meta(float scale) {
    return 12.2f * scale;
}

float loop_progress(double time_seconds, float duration_seconds) {
    const float safe_duration = std::max(1e-4f, duration_seconds);
    float phase = std::fmod(static_cast<float>(time_seconds), safe_duration);
    if (phase < 0.0f) {
        phase += safe_duration;
    }
    return phase / safe_duration;
}

float timeline_ping_pong(UI& ui, double time_seconds, float duration_seconds, eui::animation::EasingPreset preset) {
    eui::animation::TimelineClip clip{};
    clip.scalar.from = 0.0f;
    clip.scalar.to = 1.0f;
    clip.duration_seconds = std::max(1e-4f, duration_seconds);
    clip.easing = eui::animation::preset_bezier(preset);

    const float loop = loop_progress(time_seconds, clip.duration_seconds * 2.0f) * clip.duration_seconds * 2.0f;
    if (loop <= clip.duration_seconds) {
        return ui.animate(clip, loop);
    }
    return 1.0f - ui.animate(clip, loop - clip.duration_seconds);
}

float timeline_ping_pong(UI& ui, double time_seconds, float duration_seconds, const eui::animation::CubicBezier& bezier) {
    eui::animation::TimelineClip clip{};
    clip.scalar.from = 0.0f;
    clip.scalar.to = 1.0f;
    clip.duration_seconds = std::max(1e-4f, duration_seconds);
    clip.easing = bezier;

    const float loop = loop_progress(time_seconds, clip.duration_seconds * 2.0f) * clip.duration_seconds * 2.0f;
    if (loop <= clip.duration_seconds) {
        return ui.animate(clip, loop);
    }
    return 1.0f - ui.animate(clip, loop - clip.duration_seconds);
}

Point2 cubic_bezier_point(const Point2& p0, const Point2& p1, const Point2& p2, const Point2& p3, float t) {
    t = clamp01(t);
    return Point2{
        eui::animation::cubic_bezier_component(p0.x, p1.x, p2.x, p3.x, t),
        eui::animation::cubic_bezier_component(p0.y, p1.y, p2.y, p3.y, t),
    };
}

void draw_stage_background(UI& ui, const Rect& rect, float scale, const GalleryPalette& palette) {
    draw_fill(ui, rect, palette.surface_alt, 22.0f * scale, 1.0f);
    draw_stroke(ui, rect, palette.border, 22.0f * scale, 1.0f, 0.92f);

    const float gap = 34.0f * scale;
    for (float x = rect.x + gap; x < rect.x + rect.w; x += gap) {
        ui.shape().in(Rect{x, rect.y, 1.0f, rect.h}).fill(palette.grid, palette.light ? 0.36f : 0.46f).draw();
    }
    for (float y = rect.y + gap; y < rect.y + rect.h; y += gap) {
        ui.shape().in(Rect{rect.x, y, rect.w, 1.0f}).fill(palette.grid, palette.light ? 0.36f : 0.46f).draw();
    }
}

void draw_actor(UI& ui, const Rect& rect, float scale, const GalleryPalette& palette, std::uint32_t top_hex,
                std::uint32_t bottom_hex, float alpha = 1.0f, float blur_radius = 0.0f,
                float backdrop_blur = 0.0f, float translate_x = 0.0f, float translate_y = 0.0f,
                float actor_scale = 1.0f, float angle_deg = 0.0f, float fill_alpha = 1.0f) {
    auto outer = ui.shape()
                     .in(rect)
                     .radius(18.0f * scale)
                     .gradient(top_hex, bottom_hex, fill_alpha)
                     .stroke(actor_stroke_hex(palette), 1.0f, 0.92f)
                     .shadow(0.0f, 8.0f * scale, 20.0f * scale, panel_shadow_hex(palette), palette.light ? 0.08f : 0.12f)
                     .opacity(alpha)
                     .translate(translate_x, translate_y)
                     .scale(actor_scale)
                     .rotate(angle_deg)
                     .origin_center();
    if (blur_radius > 0.0f || backdrop_blur > 0.0f) {
        outer.blur(blur_radius, backdrop_blur);
    }
    outer.draw();

    auto inner = ui.shape()
                     .in(inset_rect(rect, 10.0f * scale))
                     .radius(10.0f * scale)
                     .fill(actor_inner_hex(palette), (palette.light ? 0.32f : 0.18f) * fill_alpha)
                     .opacity(alpha)
                     .translate(translate_x, translate_y)
                     .scale(actor_scale)
                     .rotate(angle_deg)
                     .origin_center();
    if (blur_radius > 0.0f || backdrop_blur > 0.0f) {
        inner.blur(blur_radius * 0.35f, backdrop_blur * 0.35f);
    }
    inner.draw();
}

void draw_actor(UI& ui, const Rect& rect, float scale, const GalleryPalette& palette, float alpha = 1.0f,
                float blur_radius = 0.0f, float backdrop_blur = 0.0f, float translate_x = 0.0f,
                float translate_y = 0.0f, float actor_scale = 1.0f, float angle_deg = 0.0f,
                float fill_alpha = 1.0f) {
    draw_actor(ui, rect, scale, palette, actor_primary_top_hex(palette), actor_primary_bottom_hex(palette), alpha,
               blur_radius, backdrop_blur, translate_x, translate_y, actor_scale, angle_deg, fill_alpha);
}

void draw_blur_reference(UI& ui, const Rect& rect, float scale, const GalleryPalette& palette) {
    const float cx = rect.x + rect.w * 0.5f;
    const float cy = rect.y + rect.h * 0.5f;
    const auto colors = blur_reference_hexes(palette);

    draw_fill(ui, Rect{cx - 240.0f * scale, cy - 94.0f * scale, 122.0f * scale, 122.0f * scale},
              colors[0], 61.0f * scale, 0.90f);
    draw_fill(ui, Rect{cx + 116.0f * scale, cy - 86.0f * scale, 130.0f * scale, 130.0f * scale},
              colors[1], 65.0f * scale, 0.88f);
    draw_fill(ui, Rect{cx - 214.0f * scale, cy + 72.0f * scale, 168.0f * scale, 22.0f * scale},
              colors[2], 11.0f * scale, 0.82f);
    draw_fill(ui, Rect{cx + 34.0f * scale, cy + 62.0f * scale, 150.0f * scale, 22.0f * scale},
              colors[3], 11.0f * scale, 0.84f);
    draw_fill(ui, Rect{cx - 9.0f * scale, cy - 116.0f * scale, 18.0f * scale, 220.0f * scale},
              colors[4], 9.0f * scale, 0.78f);
}

void draw_translate_demo(UI& ui, const Rect& rect, float scale, double time_seconds, const GalleryPalette& palette) {
    draw_stage_background(ui, rect, scale, palette);

    const Rect track{rect.x + 48.0f * scale, rect.y + rect.h * 0.5f - 3.0f * scale, rect.w - 96.0f * scale, 6.0f * scale};
    draw_fill(ui, track, demo_track_hex(palette), 3.0f * scale, 0.86f);

    const Rect start_dot{track.x - 8.0f * scale, track.y - 8.0f * scale, 16.0f * scale, 16.0f * scale};
    const Rect end_dot{track.x + track.w - 8.0f * scale, track.y - 8.0f * scale, 16.0f * scale, 16.0f * scale};
    draw_fill(ui, start_dot, demo_anchor_hex(palette), 8.0f * scale, 1.0f);
    draw_fill(ui, end_dot, demo_anchor_end_hex(palette), 8.0f * scale, 1.0f);

    const float travel = track.w - 72.0f * scale;
    const float t = timeline_ping_pong(ui, time_seconds, 1.35f, eui::animation::EasingPreset::ease_in_out);
    draw_actor(ui, Rect{track.x + travel * t, rect.y + rect.h * 0.5f - 36.0f * scale, 72.0f * scale, 72.0f * scale}, scale,
               palette);
}

void draw_scale_demo(UI& ui, const Rect& rect, float scale, double time_seconds, const GalleryPalette& palette) {
    draw_stage_background(ui, rect, scale, palette);

    const float t = timeline_ping_pong(ui, time_seconds, 1.55f, eui::animation::EasingPreset::ease_in_out);
    const float actor_scale = lerp(0.62f, 1.28f, t);
    const Rect actor{
        rect.x + rect.w * 0.5f - 72.0f * scale,
        rect.y + rect.h * 0.5f - 72.0f * scale,
        144.0f * scale,
        144.0f * scale,
    };
    draw_actor(ui, actor, scale, palette, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, actor_scale);
}

void draw_rotate_demo(UI& ui, const Rect& rect, float scale, double time_seconds, const GalleryPalette& palette) {
    draw_stage_background(ui, rect, scale, palette);

    const float t = loop_progress(time_seconds, 2.40f);
    const float angle = 360.0f * t;
    const float cx = rect.x + rect.w * 0.5f;
    const float cy = rect.y + rect.h * 0.5f;
    const Rect actor{cx - 92.0f * scale, cy - 42.0f * scale, 184.0f * scale, 84.0f * scale};
    draw_stroke(ui, Rect{cx - 132.0f * scale, cy - 132.0f * scale, 264.0f * scale, 264.0f * scale},
                demo_axis_hex(palette), 132.0f * scale, 1.0f * scale, 0.42f);
    ui.shape()
        .in(Rect{cx - 1.0f, rect.y + 48.0f * scale, 2.0f, rect.h - 96.0f * scale})
        .fill(demo_axis_hex(palette), 0.66f)
        .draw();
    ui.shape()
        .in(Rect{rect.x + 48.0f * scale, cy - 1.0f, rect.w - 96.0f * scale, 2.0f})
        .fill(demo_axis_hex(palette), 0.66f)
        .draw();
    draw_actor(ui, actor, scale, palette, actor_focus_top_hex(palette), actor_focus_bottom_hex(palette), 1.0f, 0.0f,
               0.0f, 0.0f, 0.0f, 1.0f, angle);
    draw_fill(ui, Rect{cx - 7.0f * scale, cy - 7.0f * scale, 14.0f * scale, 14.0f * scale},
              actor_glass_top_hex(palette), 7.0f * scale, 0.98f);
}

void draw_arc_motion_demo(UI& ui, const Rect& rect, float scale, double time_seconds, const GalleryPalette& palette) {
    draw_stage_background(ui, rect, scale, palette);

    const float t = timeline_ping_pong(ui, time_seconds, 1.75f, eui::animation::EasingPreset::ease_out);
    const float left = rect.x + 52.0f * scale;
    const float right = rect.x + rect.w - 52.0f * scale;
    const float base_y = rect.y + rect.h * 0.70f;
    const float height = rect.h * 0.34f;

    for (int i = 0; i <= 30; ++i) {
        const float sample = static_cast<float>(i) / 30.0f;
        const float x = lerp(left, right, sample);
        const float y = base_y - std::sin(sample * 3.14159265f) * height;
        draw_fill(ui, Rect{x - 3.0f * scale, y - 3.0f * scale, 6.0f * scale, 6.0f * scale}, demo_curve_soft_hex(palette),
                  3.0f * scale, 0.70f);
    }

    const float x = lerp(left, right, t);
    const float y = base_y - std::sin(t * 3.14159265f) * height;
    draw_actor(ui, Rect{x - 36.0f * scale, y - 36.0f * scale, 72.0f * scale, 72.0f * scale}, scale, palette);
}

void draw_bezier_demo(UI& ui, const Rect& rect, float scale, double time_seconds, const GalleryPalette& palette) {
    draw_stage_background(ui, rect, scale, palette);

    const Point2 p0{rect.x + 58.0f * scale, rect.y + rect.h - 74.0f * scale};
    const Point2 p1{rect.x + rect.w * 0.30f, rect.y + 38.0f * scale};
    const Point2 p2{rect.x + rect.w * 0.68f, rect.y + rect.h - 42.0f * scale};
    const Point2 p3{rect.x + rect.w - 58.0f * scale, rect.y + 74.0f * scale};

    for (int i = 0; i <= 40; ++i) {
        const float sample = static_cast<float>(i) / 40.0f;
        const Point2 point = cubic_bezier_point(p0, p1, p2, p3, sample);
        draw_fill(ui, Rect{point.x - 3.0f * scale, point.y - 3.0f * scale, 6.0f * scale, 6.0f * scale},
                  demo_curve_hex(palette), 3.0f * scale, sample < 0.5f ? 0.72f : 0.90f);
    }

    draw_fill(ui, Rect{p0.x - 6.0f * scale, p0.y - 6.0f * scale, 12.0f * scale, 12.0f * scale}, demo_anchor_hex(palette),
              6.0f * scale, 1.0f);
    draw_fill(ui, Rect{p1.x - 6.0f * scale, p1.y - 6.0f * scale, 12.0f * scale, 12.0f * scale}, demo_anchor_muted_hex(palette),
              6.0f * scale, 0.92f);
    draw_fill(ui, Rect{p2.x - 6.0f * scale, p2.y - 6.0f * scale, 12.0f * scale, 12.0f * scale}, demo_anchor_muted_hex(palette),
              6.0f * scale, 0.92f);
    draw_fill(ui, Rect{p3.x - 6.0f * scale, p3.y - 6.0f * scale, 12.0f * scale, 12.0f * scale}, demo_anchor_end_hex(palette),
              6.0f * scale, 1.0f);

    const eui::animation::CubicBezier velocity{0.20f, 0.0f, 0.10f, 1.0f};
    const float t = timeline_ping_pong(ui, time_seconds, 2.10f, velocity);
    const Point2 actor = cubic_bezier_point(p0, p1, p2, p3, t);
    draw_actor(ui, Rect{actor.x - 34.0f * scale, actor.y - 34.0f * scale, 68.0f * scale, 68.0f * scale}, scale, palette);
}

void draw_color_demo(UI& ui, const Rect& rect, float scale, double time_seconds, const GalleryPalette& palette) {
    draw_stage_background(ui, rect, scale, palette);

    const float t = timeline_ping_pong(ui, time_seconds, 1.80f, eui::animation::EasingPreset::ease_in_out);
    const std::uint32_t top = mix_hex(actor_primary_top_hex(palette), mix_hex(palette.accent, 0xF59E0B, 0.72f), t);
    const std::uint32_t bottom = mix_hex(actor_primary_bottom_hex(palette), mix_hex(palette.accent, 0xEF4444, 0.68f), t);
    const Rect actor{
        rect.x + rect.w * 0.5f - 88.0f * scale,
        rect.y + rect.h * 0.5f - 52.0f * scale,
        176.0f * scale,
        104.0f * scale,
    };
    draw_actor(ui, actor, scale, palette, top, bottom);

    draw_fill(ui, Rect{rect.x + 58.0f * scale, rect.y + rect.h - 78.0f * scale, 84.0f * scale, 18.0f * scale},
              actor_primary_top_hex(palette), 9.0f * scale, 0.92f);
    draw_fill(ui, Rect{rect.x + 154.0f * scale, rect.y + rect.h - 78.0f * scale, 84.0f * scale, 18.0f * scale},
              mix_hex(actor_primary_top_hex(palette), mix_hex(palette.accent, 0xF59E0B, 0.72f), 0.5f), 9.0f * scale, 0.92f);
    draw_fill(ui, Rect{rect.x + 250.0f * scale, rect.y + rect.h - 78.0f * scale, 84.0f * scale, 18.0f * scale},
              mix_hex(palette.accent, 0xEF4444, 0.68f), 9.0f * scale, 0.92f);
}

void draw_opacity_demo(UI& ui, const Rect& rect, float scale, double time_seconds, const GalleryPalette& palette) {
    draw_stage_background(ui, rect, scale, palette);

    const Rect actor{
        rect.x + rect.w * 0.5f - 88.0f * scale,
        rect.y + rect.h * 0.5f - 52.0f * scale,
        176.0f * scale,
        104.0f * scale,
    };
    const float alpha = lerp(0.20f, 1.0f, timeline_ping_pong(ui, time_seconds, 1.60f, eui::animation::EasingPreset::ease_in_out));

    draw_stroke(ui, actor, actor_outline_hex(palette), 18.0f * scale, 1.0f, 0.26f);
    draw_actor(ui, actor, scale, palette, alpha);
}

void draw_blur_demo(UI& ui, const Rect& rect, float scale, double time_seconds, const GalleryPalette& palette) {
    draw_stage_background(ui, rect, scale, palette);
    draw_blur_reference(ui, inset_rect(rect, 18.0f * scale), scale, palette);

    const float t = timeline_ping_pong(ui, time_seconds, 1.85f, eui::animation::EasingPreset::ease_in_out);
    const float blur_value = lerp(0.0f, 22.0f * scale, t);
    const Rect left_actor{
        rect.x + rect.w * 0.5f - 214.0f * scale,
        rect.y + rect.h * 0.5f - 58.0f * scale,
        188.0f * scale,
        116.0f * scale,
    };
    const Rect right_actor{
        rect.x + rect.w * 0.5f + 26.0f * scale,
        rect.y + rect.h * 0.5f - 58.0f * scale,
        188.0f * scale,
        116.0f * scale,
    };
    draw_text_center(ui, "No blur", Rect{left_actor.x, left_actor.y - 34.0f * scale, left_actor.w, 18.0f * scale},
                     font_body(scale), palette.muted, 0.96f);
    draw_text_center(ui, "Animated blur", Rect{right_actor.x, right_actor.y - 34.0f * scale, right_actor.w, 18.0f * scale},
                     font_body(scale), palette.muted, 0.96f);

    draw_actor(ui, left_actor, scale, palette, actor_glass_top_hex(palette), actor_glass_bottom_hex(palette), 1.0f, 0.0f,
               0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.12f);
    draw_actor(ui, right_actor, scale, palette, actor_glass_top_hex(palette), actor_glass_bottom_hex(palette), 1.0f,
               blur_value, blur_value, 0.0f, 0.0f, 1.0f, 0.0f, 0.12f);
}

void draw_combo_demo(UI& ui, const Rect& rect, float scale, double time_seconds, const GalleryPalette& palette) {
    draw_stage_background(ui, rect, scale, palette);
    draw_blur_reference(ui, inset_rect(rect, 24.0f * scale), scale, palette);

    const float translate_t = timeline_ping_pong(ui, time_seconds, 1.65f, eui::animation::EasingPreset::ease_in_out);
    const float scale_t = timeline_ping_pong(ui, time_seconds + 0.28, 1.65f, eui::animation::EasingPreset::spring_soft);
    const float rotate_t = loop_progress(time_seconds, 2.60f);
    const float color_t = timeline_ping_pong(ui, time_seconds + 0.44, 2.00f, eui::animation::EasingPreset::ease_in_out);
    const float alpha_t = timeline_ping_pong(ui, time_seconds + 0.18, 1.70f, eui::animation::EasingPreset::ease_in_out);
    const float blur_t = timeline_ping_pong(ui, time_seconds + 0.33, 1.90f, eui::animation::EasingPreset::ease_in_out);

    const float dx = lerp(-rect.w * 0.18f, rect.w * 0.18f, translate_t);
    const float actor_scale = lerp(0.72f, 1.18f, scale_t);
    const float angle = lerp(-28.0f, 332.0f, rotate_t);
    const float alpha = lerp(0.42f, 1.0f, alpha_t);
    const float blur_value = lerp(0.0f, 16.0f * scale, blur_t);
    const std::uint32_t top = mix_hex(actor_primary_top_hex(palette), mix_hex(palette.accent, 0xA855F7, 0.62f), color_t);
    const std::uint32_t bottom = mix_hex(actor_primary_bottom_hex(palette), mix_hex(palette.accent, 0xF97316, 0.72f), color_t);
    const Rect actor{
        rect.x + rect.w * 0.5f - 94.0f * scale,
        rect.y + rect.h * 0.5f - 58.0f * scale,
        188.0f * scale,
        116.0f * scale,
    };
    draw_actor(ui, actor, scale, palette, top, bottom, alpha, blur_value, blur_value, dx, 0.0f, actor_scale, angle, 0.24f);
}

void draw_demo_scene(UI& ui, int demo_index, const Rect& rect, float scale, double time_seconds, const GalleryPalette& palette) {
    switch (demo_index) {
        case 0:
            draw_translate_demo(ui, rect, scale, time_seconds, palette);
            break;
        case 1:
            draw_scale_demo(ui, rect, scale, time_seconds, palette);
            break;
        case 2:
            draw_rotate_demo(ui, rect, scale, time_seconds, palette);
            break;
        case 3:
            draw_arc_motion_demo(ui, rect, scale, time_seconds, palette);
            break;
        case 4:
            draw_bezier_demo(ui, rect, scale, time_seconds, palette);
            break;
        case 5:
            draw_color_demo(ui, rect, scale, time_seconds, palette);
            break;
        case 6:
            draw_opacity_demo(ui, rect, scale, time_seconds, palette);
            break;
        case 7:
            draw_blur_demo(ui, rect, scale, time_seconds, palette);
            break;
        case 8:
        default:
            draw_combo_demo(ui, rect, scale, time_seconds, palette);
            break;
    }
}

void draw_design_page(UI& ui, const eui::InputState& input, GalleryState& state, const Rect& rect, float scale) {
    const GalleryPalette palette = make_gallery_palette(state);
    const auto& library_icons = icon_library_entries();
    const float gap = 18.0f * scale;
    const auto left_split = ui.split_h_ratio(rect, 0.33f, gap);
    const auto mid_split = ui.split_h_ratio(left_split.second, 0.50f, gap);
    const Rect typography_rect = left_split.first;
    const Rect color_rect = mid_split.first;
    const Rect icon_rect = mid_split.second;

    ui.card("Typography")
        .in(typography_rect)
        .title_font(font_heading(scale))
        .radius(22.0f * scale)
        .fill(palette.surface_alt)
        .stroke(palette.border, 1.0f)
        .begin([&](auto& card) {
            struct TypeRow {
                const char* name;
                const char* usage;
                const char* sample;
                float size;
            };

            const std::array<TypeRow, 4> rows{{
                {"font_display", "Page title / shell header", "Display Title", font_display(scale)},
                {"font_heading", "Card / section heading", "Section Heading", font_heading(scale)},
                {"font_body", "Navigation / readable text", "Readable Body", font_body(scale)},
                {"font_meta", "Summary / caption / helper", "Meta Caption", font_meta(scale)},
            }};

            const float row_h = 72.0f * scale;
            const float row_gap = 8.0f * scale;
            const Rect code_rect = card.dock_bottom(92.0f * scale, 0.0f);
            const Rect content = card.content();
            float y = content.y;
            for (const auto& row_info : rows) {
                const Rect row{content.x, y, content.w, row_h};
                draw_fill(ui, row, palette.surface_deep, 14.0f * scale, 0.94f);
                draw_stroke(ui, row, palette.border_soft, 14.0f * scale, 1.0f, 0.86f);
                draw_text_left(ui, row_info.name,
                               Rect{row.x + 12.0f * scale, row.y + 8.0f * scale, row.w - 132.0f * scale, 18.0f * scale},
                               font_body(scale), palette.text, 0.98f);
                draw_text_right(ui, format_precise_pixels(row_info.size / std::max(1.0f, scale)),
                                Rect{row.x + row.w - 120.0f * scale, row.y + 8.0f * scale, 108.0f * scale, 18.0f * scale},
                                font_meta(scale), palette.muted, 0.98f);
                draw_text_left(ui, row_info.usage,
                               Rect{row.x + 12.0f * scale, row.y + 28.0f * scale, row.w - 24.0f * scale, 16.0f * scale},
                               font_meta(scale), palette.muted, 0.96f);
                draw_text_left(ui, row_info.sample,
                               Rect{row.x + 12.0f * scale, row.y + 44.0f * scale, row.w - 24.0f * scale, 24.0f * scale},
                               row_info.size, mix_hex(palette.text, palette.accent, 0.18f), 0.98f);
                y += row_h + row_gap;
            }
            draw_fill(ui, code_rect, palette.surface_deep, 16.0f * scale, 0.92f);
            draw_stroke(ui, code_rect, palette.border_soft, 16.0f * scale, 1.0f, 0.86f);
            const std::array<std::string_view, 4> code_lines{{
                "float font_display(float scale) { return 22.0f * scale; }",
                "float font_heading(float scale) { return 16.0f * scale; }",
                "float font_body(float scale) { return 14.0f * scale; }",
                "float font_meta(float scale) { return 12.2f * scale; }",
            }};
            for (std::size_t i = 0; i < code_lines.size(); ++i) {
                draw_text_left(ui, code_lines[i],
                               Rect{code_rect.x + 12.0f * scale, code_rect.y + 10.0f * scale + static_cast<float>(i) * 20.0f * scale,
                                    code_rect.w - 24.0f * scale, 18.0f * scale},
                               font_meta(scale), palette.muted, 0.98f);
            }
        });

    ui.card("Color System")
        .in(color_rect)
        .title_font(font_heading(scale))
        .radius(22.0f * scale)
        .fill(palette.surface_alt)
        .stroke(palette.border, 1.0f)
        .begin([&](auto& card) {
            const auto sections = ui.split_v_ratio(card.content(), 0.24f, 12.0f * scale);
            const Rect accents = sections.first;
            const Rect tokens_area = sections.second;

            draw_fill(ui, accents, palette.surface_deep, 18.0f * scale, 0.92f);
            draw_stroke(ui, accents, palette.border_soft, 18.0f * scale, 1.0f, 0.90f);
            draw_text_left(ui, "Built-in Accent Presets",
                           Rect{accents.x + 14.0f * scale, accents.y + 12.0f * scale, accents.w - 28.0f * scale, 18.0f * scale},
                           font_body(scale), palette.text, 0.98f);
            const float accent_gap = 10.0f * scale;
            const float accent_tile_w = (accents.w - 48.0f * scale - accent_gap * 2.0f) / 3.0f;
            const float accent_grid_y = accents.y + 42.0f * scale;
            const float accent_available_h = std::max(42.0f * scale, accents.y + accents.h - accent_grid_y - 12.0f * scale);
            const float accent_tile_h = std::max(34.0f * scale, (accent_available_h - accent_gap) * 0.5f);
            for (std::size_t i = 0; i < kAccentHexes.size(); ++i) {
                const int row = static_cast<int>(i) / 3;
                const int col = static_cast<int>(i) % 3;
                const Rect tile{
                    accents.x + 14.0f * scale + static_cast<float>(col) * (accent_tile_w + accent_gap),
                    accent_grid_y + static_cast<float>(row) * (accent_tile_h + accent_gap),
                    accent_tile_w,
                    accent_tile_h,
                };
                draw_fill(ui, tile, palette.surface_alt, 14.0f * scale, 0.96f);
                draw_stroke(ui, tile, palette.border_soft, 14.0f * scale, 1.0f, 0.86f);
                draw_fill(ui, Rect{tile.x + 10.0f * scale, tile.y + 10.0f * scale, tile.w - 20.0f * scale, 14.0f * scale},
                          kAccentHexes[i], 7.0f * scale, 0.98f);
                draw_text_left(ui, format_hex_color(kAccentHexes[i]),
                               Rect{tile.x + 10.0f * scale, tile.y + 30.0f * scale, tile.w - 20.0f * scale, 14.0f * scale},
                               font_meta(scale), palette.text, 0.98f);
            }

            draw_fill(ui, tokens_area, palette.surface_deep, 18.0f * scale, 0.92f);
            draw_stroke(ui, tokens_area, palette.border_soft, 18.0f * scale, 1.0f, 0.90f);
            draw_text_left(ui, state.light_mode ? "Current Theme Tokens: Light" : "Current Theme Tokens: Dark",
                           Rect{tokens_area.x + 14.0f * scale, tokens_area.y + 12.0f * scale, tokens_area.w - 28.0f * scale, 18.0f * scale},
                           font_body(scale), palette.text, 0.98f);

            const std::array<std::pair<const char*, std::uint32_t>, 10> tokens{{
                {"shell_top", palette.shell_top},
                {"shell_bottom", palette.shell_bottom},
                {"surface", palette.surface},
                {"surface_alt", palette.surface_alt},
                {"surface_deep", palette.surface_deep},
                {"border", palette.border},
                {"border_soft", palette.border_soft},
                {"accent", palette.accent},
                {"accent_soft", palette.accent_soft},
                {"muted", palette.muted},
            }};

            const Rect token_area{
                tokens_area.x + 14.0f * scale,
                tokens_area.y + 42.0f * scale,
                tokens_area.w - 28.0f * scale,
                tokens_area.h - 56.0f * scale,
            };
            const float token_gap = 8.0f * scale;
            const float token_w = (token_area.w - token_gap) * 0.5f;
            const float token_h = std::max(38.0f * scale, (token_area.h - token_gap * 4.0f) / 5.0f);
            for (std::size_t i = 0; i < tokens.size(); ++i) {
                const int row = static_cast<int>(i) / 2;
                const int col = static_cast<int>(i) % 2;
                const Rect tile{
                    token_area.x + static_cast<float>(col) * (token_w + token_gap),
                    token_area.y + static_cast<float>(row) * (token_h + token_gap),
                    token_w,
                    token_h,
                };
                draw_fill(ui, tile, palette.surface_alt, 14.0f * scale, 0.96f);
                draw_stroke(ui, tile, palette.border_soft, 14.0f * scale, 1.0f, 0.86f);
                const Rect swatch{
                    tile.x + 10.0f * scale,
                    tile.y + 10.0f * scale,
                    16.0f * scale,
                    std::max(14.0f * scale, tile.h - 20.0f * scale),
                };
                draw_fill(ui, swatch, tokens[i].second, std::min(swatch.w * 0.5f, 8.0f * scale), 0.98f);
                draw_text_left(ui, tokens[i].first,
                               Rect{tile.x + 34.0f * scale, tile.y + 10.0f * scale, tile.w - 46.0f * scale, 16.0f * scale},
                               font_body(scale), palette.text, 0.98f);
                draw_text_left(ui, format_hex_color(tokens[i].second),
                               Rect{tile.x + 34.0f * scale, tile.y + tile.h - 22.0f * scale, tile.w - 46.0f * scale, 14.0f * scale},
                               font_meta(scale), palette.muted, 0.96f);
            }
        });

    ui.card("Icon Reference")
        .in(icon_rect)
        .title_font(font_heading(scale))
        .radius(22.0f * scale)
        .fill(palette.surface_alt)
        .stroke(palette.border, 1.0f)
        .begin([&](auto& card) {
            const Rect action_row = card.dock_top(38.0f * scale, 12.0f * scale);
            const bool open_hovered = hovered(input, action_row);
            const float open_mix =
                ui.presence(ui.id("gallery/design/open-icon-sidebar"), open_hovered || state.design_icon_library_open, 18.0f, 12.0f);
            draw_fill(ui, action_row,
                      state.design_icon_library_open ? nav_selected_fill_hex(palette)
                                                     : mix_hex(palette.surface_deep, palette.accent, 0.10f * open_mix),
                      action_row.h * 0.5f, 0.96f);
            draw_stroke(ui, action_row,
                        state.design_icon_library_open ? palette.accent
                                                       : mix_hex(palette.border_soft, palette.accent, 0.18f * open_mix),
                        action_row.h * 0.5f, 1.0f, 0.88f);
            draw_icon(ui, 0xF03Au,
                      Rect{action_row.x + 12.0f * scale, action_row.y + (action_row.h - 16.0f * scale) * 0.5f,
                           16.0f * scale, 16.0f * scale},
                      state.design_icon_library_open ? palette.accent : palette.text, 0.98f);
            draw_text_left(ui, "Open Icon Sidebar",
                           Rect{action_row.x + 34.0f * scale, action_row.y + 10.0f * scale, action_row.w - 46.0f * scale, 18.0f * scale},
                           font_body(scale), state.design_icon_library_open ? palette.text : palette.muted, 0.98f);
            if (clicked(input, action_row)) {
                state.design_icon_library_open = true;
            }

            const Rect content = card.content();
            const float icon_gap = 10.0f * scale;
            const float tile_w = (content.w - icon_gap * 2.0f) / 3.0f;
            const float tile_h = (content.h - icon_gap * 3.0f) / 4.0f;
            const std::size_t preview_count = std::min<std::size_t>(12, library_icons.size());

            for (std::size_t i = 0; i < preview_count; ++i) {
                const int row = static_cast<int>(i) / 3;
                const int col = static_cast<int>(i) % 3;
                const Rect tile{
                    content.x + static_cast<float>(col) * (tile_w + icon_gap),
                    content.y + static_cast<float>(row) * (tile_h + icon_gap),
                    tile_w,
                    tile_h,
                };
                draw_fill(ui, tile, palette.surface_deep, 16.0f * scale, 0.94f);
                draw_stroke(ui, tile, i == 0 ? palette.accent : palette.border_soft, 16.0f * scale, 1.0f, 0.90f);
                draw_icon(ui, library_icons[i].codepoint,
                          Rect{tile.x + 12.0f * scale, tile.y + 12.0f * scale, 18.0f * scale, 18.0f * scale},
                          i == 0 ? palette.accent : palette.text, 0.98f);
                draw_text_left(ui, library_icons[i].label,
                               Rect{tile.x + 38.0f * scale, tile.y + 10.0f * scale, tile.w - 50.0f * scale, 18.0f * scale},
                               font_body(scale), palette.text, 0.98f);
                draw_text_left(ui, format_codepoint(library_icons[i].codepoint),
                               Rect{tile.x + 12.0f * scale, tile.y + 38.0f * scale, tile.w - 24.0f * scale, 16.0f * scale},
                               font_meta(scale), palette.accent, 0.98f);
                draw_text_left(ui, library_icons[i].usage,
                               Rect{tile.x + 12.0f * scale, tile.y + 56.0f * scale, tile.w - 24.0f * scale, tile.h - 60.0f * scale},
                               font_meta(scale), palette.muted, 0.96f);
            }
        });

    const std::size_t page_size = 12;
    const int icon_page_count = static_cast<int>((library_icons.size() + page_size - 1) / page_size);
    state.design_icon_page = std::clamp(state.design_icon_page, 0, std::max(0, icon_page_count - 1));
    const float sidebar_w = std::min(rect.w * 0.38f, 400.0f * scale);
    const std::uint64_t sidebar_motion_id = ui.id("gallery/design/icon-sidebar");
    const float sidebar_closed_x = rect.x + rect.w + 18.0f * scale;
    if (ui.motion_value(sidebar_motion_id, -10000.0f) < -9999.0f) {
        ui.reset_motion(sidebar_motion_id, sidebar_closed_x);
    }
    const float sidebar_target_x =
        state.design_icon_library_open ? (rect.x + rect.w - sidebar_w) : sidebar_closed_x;
    const float sidebar_x = ui.motion(sidebar_motion_id, sidebar_target_x, 18.0f);
    if (sidebar_x < rect.x + rect.w - 1.0f) {
        ui.panel("Icon Library")
            .in(Rect{sidebar_x, rect.y, sidebar_w, rect.h})
            .title_font(font_heading(scale))
            .radius(24.0f * scale)
            .fill(palette.surface_alt)
            .stroke(palette.border, 1.0f)
            .shadow(0.0f, 12.0f * scale, 24.0f * scale, panel_shadow_hex(palette), palette.light ? 0.10f : 0.18f)
            .begin([&](auto& panel) {
                const std::string page_text =
                    "Page " + std::to_string(state.design_icon_page + 1) + " / " + std::to_string(std::max(1, icon_page_count));
                const Rect header_info = panel.dock_top(18.0f * scale, 4.0f * scale);
                draw_text_left(ui, "Font Awesome JSON Catalog", header_info, font_body(scale), palette.text, 0.98f);
                const Rect page_row = panel.dock_top(34.0f * scale, 8.0f * scale);
                draw_fill(ui, page_row, palette.surface_deep, page_row.h * 0.5f, 0.96f);
                draw_stroke(ui, page_row, palette.border_soft, page_row.h * 0.5f, 1.0f, 0.88f);
                draw_text_center(ui, page_text, page_row, font_meta(scale), palette.text, 0.98f);

                const Rect actions = panel.dock_top(34.0f * scale, 8.0f * scale);
                const auto action_split = ui.split_h_ratio(actions, 0.5f, 8.0f * scale);
                draw_fill(ui, action_split.first, palette.surface_deep, actions.h * 0.5f, 0.96f);
                draw_stroke(ui, action_split.first, palette.border_soft, actions.h * 0.5f, 1.0f, 0.88f);
                draw_text_center(ui, "Prev Page", action_split.first, font_meta(scale), palette.text, 0.98f);
                if (clicked(input, action_split.first)) {
                    state.design_icon_page = std::max(0, state.design_icon_page - 1);
                }
                draw_fill(ui, action_split.second, palette.surface_deep, actions.h * 0.5f, 0.96f);
                draw_stroke(ui, action_split.second, palette.border_soft, actions.h * 0.5f, 1.0f, 0.88f);
                draw_text_center(ui, "Next Page", action_split.second, font_meta(scale), palette.text, 0.98f);
                if (clicked(input, action_split.second)) {
                    state.design_icon_page = std::min(std::max(0, icon_page_count - 1), state.design_icon_page + 1);
                }

                const Rect close_row = panel.dock_top(36.0f * scale, 10.0f * scale);
                draw_fill(ui, close_row, nav_selected_fill_hex(palette), close_row.h * 0.5f, 0.96f);
                draw_stroke(ui, close_row, palette.accent, close_row.h * 0.5f, 1.0f, 0.92f);
                draw_text_center(ui, "Close Sidebar", close_row, font_body(scale), palette.text, 0.98f);
                if (clicked(input, close_row)) {
                    state.design_icon_library_open = false;
                }

                const Rect grid = panel.content();
                const float gap = 10.0f * scale;
                const float tile_w = (grid.w - gap * 2.0f) / 3.0f;
                const float tile_h = (grid.h - gap * 3.0f) / 4.0f;
                const std::size_t start = static_cast<std::size_t>(state.design_icon_page) * page_size;
                const std::size_t end = std::min(library_icons.size(), start + page_size);
                for (std::size_t i = start; i < end; ++i) {
                    const std::size_t local = i - start;
                    const int row = static_cast<int>(local) / 3;
                    const int col = static_cast<int>(local) % 3;
                    const Rect tile{
                        grid.x + static_cast<float>(col) * (tile_w + gap),
                        grid.y + static_cast<float>(row) * (tile_h + gap),
                        tile_w,
                        tile_h,
                    };
                    draw_fill(ui, tile, palette.surface_deep, 16.0f * scale, 0.96f);
                    draw_stroke(ui, tile, palette.border_soft, 16.0f * scale, 1.0f, 0.88f);
                    draw_icon(ui, library_icons[i].codepoint,
                              Rect{tile.x + 12.0f * scale, tile.y + 10.0f * scale, 18.0f * scale, 18.0f * scale},
                              palette.accent, 0.98f);
                    draw_text_left(ui, library_icons[i].label,
                                   Rect{tile.x + 38.0f * scale, tile.y + 9.0f * scale, tile.w - 50.0f * scale, 18.0f * scale},
                                   font_body(scale), palette.text, 0.98f);
                    draw_text_left(ui, format_codepoint(library_icons[i].codepoint),
                                   Rect{tile.x + 12.0f * scale, tile.y + 34.0f * scale, tile.w - 24.0f * scale, 16.0f * scale},
                                   font_meta(scale), palette.accent, 0.98f);
                    draw_text_left(ui, library_icons[i].usage,
                                   Rect{tile.x + 12.0f * scale, tile.y + 52.0f * scale, tile.w - 24.0f * scale,
                                        tile.h - 58.0f * scale},
                                   font_meta(scale), palette.muted, 0.96f);
                }
            });
    }
}

void draw_basic_controls_page(UI& ui, const eui::InputState& input, GalleryState& state, const Rect& rect, float scale) {
    const GalleryPalette palette = make_gallery_palette(state);
    const float page_gap = 18.0f * scale;
    const float column_w = std::max(0.0f, (rect.w - page_gap * 3.0f) * 0.25f);
    const Rect col_one{rect.x, rect.y, column_w, rect.h};
    const Rect col_two{col_one.x + column_w + page_gap, rect.y, column_w, rect.h};
    const Rect col_three{col_two.x + column_w + page_gap, rect.y, column_w, rect.h};
    const Rect col_four{col_three.x + column_w + page_gap, rect.y, column_w, rect.h};
    const std::array<std::string_view, 3> control_modes{{"Compact", "Balanced", "Comfortable"}};
    const std::array<std::string_view, 3> control_toggles{{"Toolbar", "Cards", "Blur"}};
    const std::array<IconDescriptor, 3> control_icons{{
        {"Search", "Query / filter", 0xF002u},
        {"Layout", "Panels / splits", 0xF0DBu},
        {"Motion", "Animation / easing", 0xF061u},
    }};

    ui.card("Buttons")
        .in(col_one)
        .title_font(font_heading(scale))
        .radius(22.0f * scale)
        .fill(palette.surface_alt)
        .stroke(palette.border, 1.0f)
        .begin([&](auto& card) {
            const Rect icon_row = card.dock_top(72.0f * scale, 12.0f * scale);
            const Rect summary_row = card.dock_bottom(34.0f * scale, 0.0f);
            const float icon_gap = 8.0f * scale;
            const float icon_w = (icon_row.w - icon_gap * 2.0f) / 3.0f;
            for (std::size_t i = 0; i < control_icons.size(); ++i) {
                const Rect chip{
                    icon_row.x + static_cast<float>(i) * (icon_w + icon_gap),
                    icon_row.y,
                    icon_w,
                    icon_row.h,
                };
                const bool is_selected = state.selected_control_icon == static_cast<int>(i);
                const bool is_hovered = hovered(input, chip);
                const float mix = ui.presence(ui.id("gallery/basic/icon-chip", static_cast<std::uint64_t>(i + 1u)),
                                              is_hovered || is_selected, 18.0f, 12.0f);
                draw_fill(ui, chip, is_selected ? nav_selected_fill_hex(palette) : mix_hex(palette.surface_deep, palette.accent, 0.10f * mix),
                          16.0f * scale, 0.96f);
                draw_stroke(ui, chip, is_selected ? palette.accent : mix_hex(palette.border_soft, palette.accent, 0.18f * mix),
                            16.0f * scale, 1.0f, 0.88f);
                draw_icon(ui, control_icons[i].codepoint,
                          Rect{chip.x + chip.w * 0.5f - 8.0f * scale, chip.y + 12.0f * scale,
                               16.0f * scale, 16.0f * scale},
                          is_selected ? palette.accent : palette.text, 0.98f);
                draw_text_center(ui, control_icons[i].label,
                                 Rect{chip.x + 6.0f * scale, chip.y + 38.0f * scale, chip.w - 12.0f * scale, 18.0f * scale},
                                 font_meta(scale), is_selected ? palette.text : palette.muted, 0.98f);
                if (clicked(input, chip)) {
                    state.selected_control_icon = static_cast<int>(i);
                }
            }

            if (ui.button("Primary Action").primary().height(36.0f * scale).draw()) {
                state.progress_ratio = std::min(1.0f, state.progress_ratio + 0.08f);
            }
            if (ui.button("Reset Progress").ghost().height(34.0f * scale).draw()) {
                state.progress_ratio = 0.18f;
            }
            ui.readonly("Selected", std::string(control_icons[static_cast<std::size_t>(std::clamp(state.selected_control_icon, 0, 2))].label))
                .height(32.0f * scale)
                .draw();
            draw_text_left(ui, "Compact button sizing keeps the page readable.", summary_row, font_meta(scale), palette.muted, 0.96f);
        });

    ui.card("Selection")
        .in(col_two)
        .title_font(font_heading(scale))
        .radius(22.0f * scale)
        .fill(palette.surface_alt)
        .stroke(palette.border, 1.0f)
        .begin([&](auto&) {
            ui.readonly("Single", "Exclusive tab group").height(32.0f * scale).draw();
            for (int i = 0; i < static_cast<int>(control_modes.size()); ++i) {
                if (ui.tab(control_modes[static_cast<std::size_t>(i)], state.controls_mode == i).height(32.0f * scale).draw()) {
                    state.controls_mode = i;
                }
            }
            ui.readonly("Multi", "Independent toggles").height(32.0f * scale).draw();
            for (int i = 0; i < static_cast<int>(control_toggles.size()); ++i) {
                if (ui.tab(control_toggles[static_cast<std::size_t>(i)],
                           state.controls_multi_select[static_cast<std::size_t>(i)]).height(32.0f * scale).draw()) {
                    state.controls_multi_select[static_cast<std::size_t>(i)] =
                        !state.controls_multi_select[static_cast<std::size_t>(i)];
                }
            }
            const std::string enabled_summary =
                std::string(state.controls_multi_select[0] ? "Toolbar " : "") +
                (state.controls_multi_select[1] ? "Cards " : "") +
                (state.controls_multi_select[2] ? "Blur" : "");
            ui.readonly("Enabled", enabled_summary.empty() ? "None" : enabled_summary).height(32.0f * scale).draw();
        });

    ui.card("Inputs")
        .in(col_three)
        .title_font(font_heading(scale))
        .radius(22.0f * scale)
        .fill(palette.surface_alt)
        .stroke(palette.border, 1.0f)
        .begin([&](auto&) {
            const std::string spacing_text = format_pixels(state.layout_gap);
            const std::string density_text = std::string(control_modes[static_cast<std::size_t>(std::clamp(state.controls_mode, 0, 2))]);
            ui.input("Search", state.search_text).placeholder("Type to filter").height(38.0f * scale).draw();
            std::string dropdown_label = "Density: ";
            dropdown_label += density_text;
            ui.dropdown(dropdown_label, state.controls_dropdown_open).body_height(144.0f * scale).padding(14.0f * scale).begin([&](auto&) {
                for (int i = 0; i < static_cast<int>(control_modes.size()); ++i) {
                    if (ui.button(control_modes[static_cast<std::size_t>(i)]).ghost().height(36.0f * scale).draw()) {
                        state.controls_mode = i;
                        state.controls_dropdown_open = false;
                    }
                }
            });
            ui.slider("Progress", state.progress_ratio).range(0.0f, 1.0f).decimals(2).height(36.0f * scale).draw();
            ui.slider("Live Value", state.control_slider).range(0.0f, 1.0f).decimals(2).height(36.0f * scale).draw();
            ui.progress("Completion", state.progress_ratio).height(10.0f * scale).draw();
            ui.readonly("Gap / Accent", spacing_text + " / " + format_hex_color(accent_hex(state))).height(32.0f * scale).draw();
            if (ui.button("Jump To Animation Page").secondary().height(36.0f * scale).draw()) {
                state.selected_page = kPageAnimation;
            }
        });

    ui.card("Editor")
        .in(col_four)
        .title_font(font_heading(scale))
        .radius(22.0f * scale)
        .fill(palette.surface_alt)
        .stroke(palette.border, 1.0f)
        .begin([&](auto& card) {
            ui.readonly("Purpose", "Multiline input, wrapping and scroll behavior").height(32.0f * scale).draw();
            const float notes_h = std::max(160.0f * scale, card.content().h - 40.0f * scale);
            ui.text_area("Notes", state.notes_text).height(notes_h).draw();
        });
}

void draw_layout_page(UI& ui, GalleryState& state, const Rect& rect, float scale) {
    const std::uint32_t accent = accent_hex(state);
    const GalleryPalette palette = make_gallery_palette(state);
    const auto split = ui.split_v_ratio(rect, 0.30f, 18.0f * scale);

    ui.card("Layout Controls")
        .in(split.first)
        .title_font(font_heading(scale))
        .radius(22.0f * scale)
        .fill(palette.surface_alt)
        .stroke(palette.border, 1.0f)
        .begin([&](auto&) {
            ui.slider("Gap", state.layout_gap).range(8.0f, 28.0f).decimals(0).height(36.0f * scale).draw();
            ui.slider("Radius", state.layout_radius).range(8.0f, 28.0f).decimals(0).height(36.0f * scale).draw();
            ui.readonly("Core APIs", "split_h_ratio, split_v_ratio, dock_top, content()").height(36.0f * scale).draw();
        });

    const float gap = state.layout_gap * scale;
    const float radius = state.layout_radius * scale;
    draw_fill(ui, split.second, palette.surface_alt, 22.0f * scale, 1.0f);
    draw_stroke(ui, split.second, palette.border, 22.0f * scale, 1.0f, 1.0f);

    const Rect shell = inset_rect(split.second, 18.0f * scale);
    const Rect toolbar{shell.x, shell.y, shell.w, 52.0f * scale};
    const Rect body{shell.x, toolbar.y + toolbar.h + gap, shell.w, shell.h - toolbar.h - gap};
    const auto columns = ui.split_h_ratio(body, 0.22f, gap);
    const auto workspace = ui.split_h_ratio(columns.second, 0.70f, gap);
    const auto canvas_split = ui.split_v_ratio(workspace.first, 0.74f, gap);

    draw_fill(ui, toolbar, palette.surface_deep, radius, 0.98f);
    draw_stroke(ui, toolbar, accent, radius, 1.0f, 0.52f);
    draw_text_left(ui, "Toolbar / Filters", inset_rect(toolbar, 16.0f * scale, 14.0f * scale), font_heading(scale), palette.text, 0.98f);

    draw_fill(ui, columns.first, palette.surface_deep, radius, 0.98f);
    draw_stroke(ui, columns.first, palette.border_soft, radius, 1.0f, 0.96f);
    draw_text_left(ui, "Sidebar", inset_rect(columns.first, 14.0f * scale, 14.0f * scale), font_heading(scale), palette.text, 0.98f);

    draw_fill(ui, workspace.first, palette.surface, radius, 0.98f);
    draw_stroke(ui, workspace.first, palette.border_soft, radius, 1.0f, 0.96f);
    draw_text_left(ui, "Canvas Region", inset_rect(canvas_split.first, 18.0f * scale, 16.0f * scale), font_heading(scale), palette.text, 0.98f);

    draw_fill(ui, canvas_split.second, palette.surface_deep, radius, 0.98f);
    draw_stroke(ui, canvas_split.second, palette.border_soft, radius, 1.0f, 0.96f);
    draw_text_left(ui, "Footer Tools", inset_rect(canvas_split.second, 18.0f * scale, 16.0f * scale), font_heading(scale), palette.text, 0.98f);

    draw_fill(ui, workspace.second, palette.surface_deep, radius, 0.98f);
    draw_stroke(ui, workspace.second, mix_hex(palette.border_soft, accent, 0.26f), radius, 1.0f, 0.96f);
    draw_text_left(ui, "Inspector", inset_rect(workspace.second, 14.0f * scale, 14.0f * scale), font_heading(scale), palette.text, 0.98f);
}

void draw_animation_page(UI& ui, const eui::InputState& input, GalleryState& state, const Rect& rect, float scale,
                         double time_seconds) {
    const GalleryPalette palette = make_gallery_palette(state);
    const std::uint32_t accent = accent_hex(state);
    const Rect selector_rect{rect.x, rect.y, rect.w, 154.0f * scale};
    const Rect stage_rect{rect.x, rect.y + 166.0f * scale, rect.w, rect.h - 166.0f * scale};
    const DemoDescriptor& current =
        kDemos[static_cast<std::size_t>(std::clamp(state.selected_animation_demo, 0, static_cast<int>(kDemos.size() - 1)))];

    draw_fill(ui, selector_rect, palette.surface_alt, 20.0f * scale, 1.0f);
    draw_stroke(ui, selector_rect, palette.border, 20.0f * scale, 1.0f, 1.0f);

    const float grid_gap = 10.0f * scale;
    const float item_h = 28.0f * scale;
    const float item_w = (selector_rect.w - grid_gap * 4.0f) / 3.0f;
    const float start_x = selector_rect.x + grid_gap;
    const float start_y = selector_rect.y + 12.0f * scale;
    for (std::size_t i = 0; i < kDemos.size(); ++i) {
        const int row = static_cast<int>(i) / 3;
        const int col = static_cast<int>(i) % 3;
        const Rect item{
            start_x + static_cast<float>(col) * (item_w + grid_gap),
            start_y + static_cast<float>(row) * (item_h + grid_gap),
            item_w,
            item_h,
        };
        const bool is_selected = state.selected_animation_demo == static_cast<int>(i);
        const bool is_hovered = hovered(input, item);
        if (is_selected || is_hovered) {
            draw_fill(ui, item, is_selected ? mix_hex(palette.surface_deep, accent, 0.28f) : palette.surface_deep, item.h * 0.5f, is_selected ? 0.98f : 0.92f);
            draw_stroke(ui, item, is_selected ? accent : palette.border_soft, item.h * 0.5f, 1.0f, is_selected ? 0.96f : 0.88f);
        } else {
            draw_fill(ui, item, palette.surface_deep, item.h * 0.5f, 0.84f);
        }
        draw_text_center(ui, kDemos[i].title, item, font_body(scale), is_selected ? palette.text : palette.muted, 0.98f);
        if (clicked(input, item)) {
            state.selected_animation_demo = static_cast<int>(i);
        }
    }

    const float info_y = selector_rect.y + 116.0f * scale;
    draw_text_left(ui, current.summary,
                   Rect{selector_rect.x + 14.0f * scale, info_y,
                        selector_rect.w - 280.0f * scale, 16.0f * scale},
                   font_meta(scale), palette.muted, 0.96f);
    draw_fill(ui, Rect{selector_rect.x + selector_rect.w - 236.0f * scale, info_y - 4.0f * scale,
                       222.0f * scale, 22.0f * scale},
              palette.surface_deep, 11.0f * scale, 0.96f);
    draw_stroke(ui, Rect{selector_rect.x + selector_rect.w - 236.0f * scale, info_y - 4.0f * scale,
                         222.0f * scale, 22.0f * scale},
                accent, 11.0f * scale, 1.0f, 0.90f);
    draw_text_center(ui, current.api_label,
                     Rect{selector_rect.x + selector_rect.w - 236.0f * scale, info_y - 4.0f * scale,
                          222.0f * scale, 22.0f * scale},
                     font_meta(scale), mix_hex(palette.text, accent, 0.40f), 0.98f);

    for (std::size_t i = 0; i < kDemos.size(); ++i) {
        const float mix =
            ui.presence(ui.id("gallery/animation/view", static_cast<std::uint64_t>(i + 1u)),
                        state.selected_animation_demo == static_cast<int>(i), 12.0f, 12.0f);
        if (mix <= 0.01f) {
            continue;
        }
        const float alpha = mix * mix * (3.0f - 2.0f * mix);
        ui.ctx().set_global_alpha(alpha);
        ui.clip(stage_rect, [&] {
            const Rect scene_rect{stage_rect.x + (1.0f - alpha) * 14.0f * scale, stage_rect.y, stage_rect.w, stage_rect.h};
            draw_demo_scene(ui, static_cast<int>(i), scene_rect, scale, time_seconds, palette);
        });
        ui.ctx().set_global_alpha(1.0f);
    }
}

void draw_components_page(UI& ui, GalleryState& state, const Rect& rect, float scale) {
    const std::uint32_t accent = accent_hex(state);
    const GalleryPalette palette = make_gallery_palette(state);
    const auto top_bottom = ui.split_v_ratio(rect, 0.40f, 18.0f * scale);
    const float gap = 14.0f * scale;

    const auto top_cols = ui.split_h_ratio(top_bottom.first, 0.5f, gap);
    const auto top_left = ui.split_h_ratio(top_cols.first, 0.5f, gap);
    const auto top_right = ui.split_h_ratio(top_cols.second, 0.5f, gap);

    ui.metric("Build", "Header-only")
        .in(top_left.first)
        .tag("core")
        .caption("One include tree, one renderer path")
        .label_font(font_body(scale))
        .value_font(font_heading(scale))
        .caption_font(font_body(scale))
        .tag_font(font_meta(scale))
        .gradient(metric_surface_top_hex(palette), metric_surface_bottom_hex(palette))
        .stroke(metric_surface_border_hex(palette), 1.0f)
        .draw();
    ui.metric("Backends", "GLFW / SDL2")
        .in(top_left.second)
        .tag("window")
        .caption("Selected by source macros")
        .label_font(font_body(scale))
        .value_font(font_heading(scale))
        .caption_font(font_body(scale))
        .tag_font(font_meta(scale))
        .gradient(metric_surface_top_hex(palette), metric_surface_bottom_hex(palette))
        .stroke(metric_surface_border_hex(palette), 1.0f)
        .draw();
    ui.metric("Theme", state.light_mode ? "Light" : "Dark")
        .in(top_right.first)
        .tag("runtime")
        .caption("Gallery shell reads live settings")
        .label_font(font_body(scale))
        .value_font(font_heading(scale))
        .caption_font(font_body(scale))
        .tag_font(font_meta(scale))
        .gradient(metric_surface_top_hex(palette), metric_surface_bottom_hex(palette))
        .stroke(metric_surface_border_hex(palette), 1.0f)
        .draw();
    ui.metric("Progress", format_percent(state.progress_ratio))
        .in(top_right.second)
        .tag("state")
        .caption("Shared state can feed multiple widgets")
        .label_font(font_body(scale))
        .value_font(font_heading(scale))
        .caption_font(font_body(scale))
        .tag_font(font_meta(scale))
        .gradient(metric_surface_top_hex(palette), metric_surface_bottom_hex(palette))
        .stroke(metric_surface_border_hex(palette), 1.0f)
        .draw();

    const auto bottom_cols = ui.split_h_ratio(top_bottom.second, 0.50f, 18.0f * scale);
    ui.card("Readonly, Progress and Button")
        .in(bottom_cols.first)
        .title_font(font_heading(scale))
        .radius(22.0f * scale)
        .fill(palette.surface_alt)
        .stroke(palette.border, 1.0f)
        .begin([&](auto&) {
            ui.readonly("Renderer", "Modern OpenGL").height(36.0f * scale).draw();
            ui.readonly("Accent Radius", format_pixels(state.layout_radius)).height(36.0f * scale).draw();
            ui.progress("Shared Progress", state.progress_ratio).height(10.0f * scale).draw();
            if (ui.button("Open Settings").secondary().height(38.0f * scale).draw()) {
                state.selected_page = kPageSettings;
            }
        });

    const std::string component_text =
        "Metric, readonly, progress, card and panel builders are exposed directly from eui::quick::UI.\n\n"
        "This page groups the higher-level building blocks so the left navigation stays clean and top-level.";
    ui.card("Text Surface")
        .in(bottom_cols.second)
        .title_font(font_heading(scale))
        .radius(22.0f * scale)
        .fill(palette.surface_alt)
        .stroke(palette.border, 1.0f)
        .begin([&](auto&) {
            ui.text_area_readonly("Component Notes", component_text).height(236.0f * scale).draw();
        });
}

void draw_dashboard_page(UI& ui, const GalleryState& state, const Rect& rect, float scale) {
    const std::uint32_t accent = accent_hex(state);
    const GalleryPalette palette = make_gallery_palette(state);
    const auto split = ui.split_v_ratio(rect, 0.66f, 18.0f * scale);
    const Rect preview = split.first;
    const Rect stats = split.second;

    draw_fill(ui, preview, palette.surface_alt, 22.0f * scale, 1.0f);
    draw_stroke(ui, preview, palette.border, 22.0f * scale, 1.0f, 1.0f);

    const Rect shell = inset_rect(preview, 18.0f * scale);
    const Rect sidebar{shell.x, shell.y, 120.0f * scale, shell.h};
    const Rect main{sidebar.x + sidebar.w + 16.0f * scale, shell.y, shell.w - sidebar.w - 16.0f * scale, shell.h};
    const Rect hero{main.x, main.y, main.w, 76.0f * scale};
    const Rect cards_row{main.x, hero.y + hero.h + 14.0f * scale, main.w, 94.0f * scale};
    const Rect activity{main.x, cards_row.y + cards_row.h + 14.0f * scale, main.w, main.h - hero.h - cards_row.h - 28.0f * scale};

    draw_fill(ui, sidebar, palette.surface_deep, 18.0f * scale, 0.98f);
    draw_stroke(ui, sidebar, palette.border_soft, 18.0f * scale, 1.0f, 0.96f);
    draw_text_left(ui, "Dashboard", Rect{sidebar.x + 14.0f * scale, sidebar.y + 16.0f * scale, sidebar.w - 28.0f * scale, 18.0f * scale},
                   font_heading(scale), palette.text, 1.0f);
    for (int i = 0; i < 4; ++i) {
        const Rect item{sidebar.x + 12.0f * scale, sidebar.y + 48.0f * scale + static_cast<float>(i) * 42.0f * scale,
                        sidebar.w - 24.0f * scale, 30.0f * scale};
        draw_fill(ui, item, i == 1 ? nav_selected_fill_hex(palette) : nav_idle_fill_hex(palette), 15.0f * scale,
                  i == 1 ? 0.98f : 0.84f);
        draw_text_left(ui, i == 0 ? "Overview" : i == 1 ? "Orders" : i == 2 ? "Activity" : "Settings",
                       Rect{item.x + 12.0f * scale, item.y + 7.0f * scale, item.w - 24.0f * scale, 16.0f * scale},
                       font_body(scale), i == 1 ? palette.text : palette.muted, 0.98f);
    }

    draw_fill(ui, hero, palette.surface_deep, 18.0f * scale, 0.98f);
    draw_stroke(ui, hero, palette.border_soft, 18.0f * scale, 1.0f, 0.96f);
    draw_text_left(ui, "Dashboard Preview", Rect{hero.x + 18.0f * scale, hero.y + 14.0f * scale, hero.w - 36.0f * scale, 18.0f * scale},
                   font_heading(scale), palette.text, 1.0f);
    draw_text_left(ui, "The full standalone example remains available as reference_dashboard_demo.cpp",
                   Rect{hero.x + 18.0f * scale, hero.y + 40.0f * scale, hero.w - 36.0f * scale, 16.0f * scale},
                   font_meta(scale), palette.muted, 0.96f);

    const auto cards = ui.split_h_ratio(cards_row, 0.33f, 12.0f * scale);
    const auto cards_right = ui.split_h_ratio(cards.second, 0.5f, 12.0f * scale);
    ui.metric("Revenue", "$128k").in(cards.first).tag("up").caption("Month over month +18%")
        .label_font(font_body(scale)).value_font(font_heading(scale)).caption_font(font_body(scale)).tag_font(font_meta(scale))
        .gradient(mix_hex(palette.surface_deep, accent, 0.10f), mix_hex(palette.surface_alt, accent, 0.18f)).draw();
    ui.metric("Orders", "1,284").in(cards_right.first).tag("live").caption("Refreshed from shared state")
        .label_font(font_body(scale)).value_font(font_heading(scale)).caption_font(font_body(scale)).tag_font(font_meta(scale))
        .gradient(mix_hex(palette.surface_deep, accent, 0.10f), mix_hex(palette.surface_alt, accent, 0.18f)).draw();
    ui.metric("Retention", "92%").in(cards_right.second).tag("stable").caption("Reusable metric surface")
        .label_font(font_body(scale)).value_font(font_heading(scale)).caption_font(font_body(scale)).tag_font(font_meta(scale))
        .gradient(mix_hex(palette.surface_deep, accent, 0.10f), mix_hex(palette.surface_alt, accent, 0.18f)).draw();

    draw_fill(ui, activity, palette.surface_deep, 18.0f * scale, 0.98f);
    draw_stroke(ui, activity, palette.border_soft, 18.0f * scale, 1.0f, 0.96f);
    draw_text_left(ui, "Activity", Rect{activity.x + 18.0f * scale, activity.y + 14.0f * scale, 120.0f * scale, 18.0f * scale},
                   font_heading(scale), palette.text, 1.0f);
    const float plot_bottom = activity.y + activity.h - 26.0f * scale;
    float bar_x = activity.x + 24.0f * scale;
    for (int i = 0; i < 7; ++i) {
        const float height = (0.28f + static_cast<float>((i * 37) % 53) / 100.0f) * (activity.h - 66.0f * scale);
        draw_fill(ui, Rect{bar_x, plot_bottom - height, 18.0f * scale, height},
                  i == 5 ? accent : demo_track_hex(palette), 9.0f * scale, i == 5 ? 0.98f : 0.86f);
        bar_x += 28.0f * scale;
    }

    const auto stats_cols = ui.split_h_ratio(stats, 0.5f, 18.0f * scale);
    ui.card("What This Proves")
        .in(stats_cols.first)
        .title_font(font_heading(scale))
        .radius(22.0f * scale)
        .fill(palette.surface_alt)
        .stroke(palette.border, 1.0f)
        .begin([&](auto&) {
            ui.readonly("Composition", "Sidebar + cards + chart + metrics").height(36.0f * scale).draw();
            ui.readonly("Reuse", "Same primitives power the standalone dashboard").height(36.0f * scale).draw();
        });
    ui.card("Usage")
        .in(stats_cols.second)
        .title_font(font_heading(scale))
        .radius(22.0f * scale)
        .fill(palette.surface_alt)
        .stroke(palette.border, 1.0f)
        .begin([&](auto&) {
            ui.text_area_readonly("Standalone File", "reference_dashboard_demo.cpp remains as the dedicated dashboard example target.")
                .height(120.0f * scale)
                .draw();
        });
}

void draw_settings_page(UI& ui, const eui::InputState& input, GalleryState& state, const Rect& rect, float scale) {
    const GalleryPalette palette = make_gallery_palette(state);
    const auto split = ui.split_h_ratio(rect, 0.54f, 18.0f * scale);

    ui.card("Gallery Settings")
        .in(split.first)
        .title_font(font_heading(scale))
        .radius(22.0f * scale)
        .fill(palette.surface_alt)
        .stroke(palette.border, 1.0f)
        .begin([&](auto& card) {
            const Rect mode_label = card.dock_top(14.0f * scale, 6.0f * scale);
            const Rect mode_row = card.dock_top(38.0f * scale, 14.0f * scale);
            const Rect accent_label = card.dock_top(14.0f * scale, 6.0f * scale);
            const Rect accent_row = card.dock_top(40.0f * scale, 16.0f * scale);
            const Rect custom_label = card.dock_top(14.0f * scale, 6.0f * scale);
            const Rect custom_row = card.dock_top(42.0f * scale, 12.0f * scale);

            draw_text_left(ui, "Theme Mode", mode_label, font_meta(scale), palette.muted, 0.96f);
            const auto mode_split = ui.split_h_ratio(mode_row, 0.5f, 10.0f * scale);
            const Rect dark_rect = mode_split.first;
            const Rect light_rect = mode_split.second;
            const std::uint32_t accent = accent_hex(state);
            draw_fill(ui, dark_rect, state.light_mode ? palette.surface_deep : nav_selected_fill_hex(palette), dark_rect.h * 0.5f, 0.98f);
            draw_stroke(ui, dark_rect, state.light_mode ? palette.border_soft : accent, dark_rect.h * 0.5f, 1.0f, 0.96f);
            draw_text_center(ui, "Dark", dark_rect, font_body(scale), state.light_mode ? palette.muted : palette.text, 0.98f);
            draw_fill(ui, light_rect, state.light_mode ? nav_selected_fill_hex(palette) : palette.surface_deep, light_rect.h * 0.5f, 0.98f);
            draw_stroke(ui, light_rect, state.light_mode ? accent : palette.border_soft, light_rect.h * 0.5f, 1.0f, 0.96f);
            draw_text_center(ui, "Light", light_rect, font_body(scale), state.light_mode ? palette.text : palette.muted, 0.98f);
            if (clicked(input, dark_rect)) {
                state.light_mode = false;
            }
            if (clicked(input, light_rect)) {
                state.light_mode = true;
            }

            draw_text_left(ui, "Accent Color", accent_label, font_meta(scale), palette.muted, 0.96f);
            const float swatch = 28.0f * scale;
            const float swatch_gap = 10.0f * scale;
            float swatch_x = accent_row.x;
            const int active_accent = accent_uses_custom_rgb(state) ? custom_accent_slot() : state.accent_index;
            for (int i = 0; i <= custom_accent_slot(); ++i) {
                const std::uint32_t swatch_hex =
                    i == custom_accent_slot() ? custom_accent_hex(state) : kAccentHexes[static_cast<std::size_t>(i)];
                const Rect chip{swatch_x, accent_row.y + 6.0f * scale, swatch, swatch};
                draw_fill(ui, chip, swatch_hex, chip.w * 0.5f, 0.98f);
                if (i == custom_accent_slot()) {
                    draw_stroke(ui, Rect{chip.x + 4.0f * scale, chip.y + 4.0f * scale, chip.w - 8.0f * scale, chip.h - 8.0f * scale},
                                palette.surface_alt, (chip.w - 8.0f * scale) * 0.5f, 1.0f, 0.90f);
                }
                if (active_accent == i) {
                    draw_stroke(ui, Rect{chip.x - 3.0f, chip.y - 3.0f, chip.w + 6.0f, chip.h + 6.0f},
                                palette.text, (chip.w + 6.0f) * 0.5f, 1.0f, 0.92f);
                }
                if (clicked(input, Rect{chip.x - 4.0f, chip.y - 4.0f, chip.w + 8.0f, chip.h + 8.0f})) {
                    state.accent_index = i;
                }
                swatch_x += swatch + swatch_gap;
            }

            draw_text_left(ui, "Custom RGB", custom_label, font_meta(scale), palette.muted, 0.96f);
            const auto custom_split = ui.split_h_ratio(custom_row, 0.24f, 10.0f * scale);
            const Rect custom_chip = custom_split.first;
            const Rect custom_info = custom_split.second;
            const std::uint32_t custom_hex = custom_accent_hex(state);
            draw_fill(ui, custom_chip, custom_hex, 14.0f * scale, 0.98f);
            draw_stroke(ui, custom_chip,
                        active_accent == custom_accent_slot() ? palette.text : palette.border_soft,
                        14.0f * scale, 1.0f, active_accent == custom_accent_slot() ? 0.94f : 0.86f);
            draw_fill(ui, custom_info, palette.surface_deep, custom_info.h * 0.5f, 0.96f);
            draw_stroke(ui, custom_info, palette.border_soft, custom_info.h * 0.5f, 1.0f, 0.86f);
            draw_text_left(ui, "Live custom accent",
                           Rect{custom_info.x + 12.0f * scale, custom_info.y + 6.0f * scale,
                                custom_info.w - 24.0f * scale, 14.0f * scale},
                           font_meta(scale), palette.muted, 0.96f);
            draw_text_left(ui, format_hex_color(custom_hex),
                           Rect{custom_info.x + 12.0f * scale, custom_info.y + 20.0f * scale,
                                custom_info.w - 24.0f * scale, 16.0f * scale},
                           font_body(scale), palette.text, 0.98f);
            if (clicked(input, custom_row)) {
                state.accent_index = custom_accent_slot();
            }

            if (ui.slider("Red", state.custom_accent_r).range(0.0f, 255.0f).decimals(0).height(36.0f * scale).draw()) {
                state.accent_index = custom_accent_slot();
            }
            if (ui.slider("Green", state.custom_accent_g).range(0.0f, 255.0f).decimals(0).height(36.0f * scale).draw()) {
                state.accent_index = custom_accent_slot();
            }
            if (ui.slider("Blue", state.custom_accent_b).range(0.0f, 255.0f).decimals(0).height(36.0f * scale).draw()) {
                state.accent_index = custom_accent_slot();
            }
            ui.slider("Corner Radius", state.layout_radius).range(8.0f, 28.0f).decimals(0).height(36.0f * scale).draw();
            ui.slider("Glass Blur", state.settings_blur).range(0.0f, 28.0f).decimals(0).height(36.0f * scale).draw();
        });

    ui.card("Live Preview")
        .in(split.second)
        .title_font(font_heading(scale))
        .radius(22.0f * scale)
        .fill(palette.surface_alt)
        .stroke(palette.border, 1.0f)
        .begin([&](auto& card) {
            const std::uint32_t accent = accent_hex(state);
            const float preview_radius = state.layout_radius * scale;
            const Rect preview = inset_rect(card.content(), 6.0f * scale);
            ui.clip(preview, [&] {
                draw_fill(ui, preview, preview_backdrop_hex(palette), preview_radius, 1.0f);
                draw_stroke(ui, preview, accent, preview_radius, 1.0f, 0.76f);
                const Rect inner = inset_rect(preview, 18.0f * scale);
                draw_stage_background(ui, inner, scale, make_gallery_palette(state));
                draw_blur_reference(ui, inset_rect(inner, 34.0f * scale), scale * 0.82f, palette);
                draw_actor(ui,
                           Rect{inner.x + inner.w * 0.5f - 80.0f * scale, inner.y + inner.h * 0.5f - 50.0f * scale,
                                160.0f * scale, 100.0f * scale},
                           scale, palette, actor_glass_top_hex(palette), actor_glass_bottom_hex(palette), 1.0f,
                           state.settings_blur * scale, state.settings_blur * scale, 0.0f, 0.0f, 1.0f, 0.0f, 0.18f);
            });
        });
}

void draw_about_page(UI& ui, const eui::InputState& input, const GalleryState& state, const Rect& rect, float scale) {
    const GalleryPalette palette = make_gallery_palette(state);
    const std::uint32_t accent = accent_hex(state);

    draw_fill(ui, rect, palette.surface_alt, 24.0f * scale, 1.0f);
    draw_stroke(ui, rect, palette.border, 24.0f * scale, 1.0f, 1.0f);

    const Rect content = inset_rect(rect, 32.0f * scale);
    const float hero_w = std::min(content.w, 720.0f * scale);
    const float hero_h = std::min(content.h * 0.44f, 236.0f * scale);
    const Rect hero{
        content.x + (content.w - hero_w) * 0.5f,
        content.y + 6.0f * scale,
        hero_w,
        hero_h,
    };

    draw_fill(ui, hero, palette.surface_deep, 24.0f * scale, 0.96f);
    draw_stroke(ui, hero, mix_hex(palette.border_soft, accent, 0.18f), 24.0f * scale, 1.0f, 0.90f);
    draw_text_center(ui, "EUI", Rect{hero.x, hero.y + 20.0f * scale, hero.w, 22.0f * scale},
                     font_heading(scale) * 2.0f, mix_hex(palette.text, accent, 0.24f), 0.98f);
    draw_text_center(ui, "Created by SudoEvolve",
                     Rect{hero.x, hero.y + 50.0f * scale, hero.w, 18.0f * scale},
                     font_body(scale), palette.text, 0.98f);
    draw_text_center(ui,
                     "Immediate-mode GUI with crisp text, modern surfaces, reusable controls and fast iteration for desktop tooling.",
                     Rect{hero.x + 28.0f * scale, hero.y + 60.0f * scale, hero.w - 56.0f * scale, 38.0f * scale},
                     font_body(scale), palette.muted, 0.98f);

    const float buttons_w = std::min(hero.w - 64.0f * scale, 420.0f * scale);
    const Rect buttons_row{
        hero.x + (hero.w - buttons_w) * 0.5f,
        hero.y + hero.h - 80.0f * scale,
        buttons_w,
        42.0f * scale,
    };
    const auto button_split = ui.split_h_ratio(buttons_row, 0.5f, 16.0f * scale);
    const Rect github_button = button_split.first;
    const Rect mail_button = button_split.second;

    const bool github_hovered = hovered(input, github_button);
    const bool mail_hovered = hovered(input, mail_button);
    const float github_mix = ui.presence(ui.id("gallery/about/github"), github_hovered, 18.0f, 12.0f);
    const float mail_mix = ui.presence(ui.id("gallery/about/mail"), mail_hovered, 18.0f, 12.0f);

    draw_fill(ui, github_button, mix_hex(palette.accent, 0xFFFFFF, palette.light ? 0.14f : 0.04f * github_mix),
              github_button.h * 0.5f, 0.98f);
    draw_stroke(ui, github_button, mix_hex(palette.accent, palette.text, 0.12f), github_button.h * 0.5f, 1.0f, 0.94f);
    draw_icon(ui, 0xF121u,
              Rect{github_button.x + 24.0f * scale, github_button.y + 13.0f * scale, 18.0f * scale, 18.0f * scale},
              palette.light ? 0x0F172A : 0xFFFFFF, 0.98f);
    draw_text_left(ui, "GitHub",
                   Rect{github_button.x + 52.0f * scale, github_button.y + 13.0f * scale,
                        github_button.w - 68.0f * scale, 18.0f * scale},
                   font_body(scale), palette.light ? 0x0F172A : 0xFFFFFF, 0.98f);

    draw_fill(ui, mail_button, palette.surface_deep, mail_button.h * 0.5f, 0.96f);
    draw_stroke(ui, mail_button, mix_hex(palette.border_soft, accent, 0.20f + 0.18f * mail_mix), mail_button.h * 0.5f, 1.0f, 0.90f);
    draw_icon(ui, 0xF0E0u,
              Rect{mail_button.x + 24.0f * scale, mail_button.y + 13.0f * scale, 18.0f * scale, 18.0f * scale},
              palette.text, 0.98f);
    draw_text_left(ui, "Email",
                   Rect{mail_button.x + 52.0f * scale, mail_button.y + 13.0f * scale, mail_button.w - 68.0f * scale, 18.0f * scale},
                   font_body(scale), palette.text, 0.98f);

    if (clicked(input, github_button)) {
        open_external_uri("https://github.com/sudoevolve");
    }
    if (clicked(input, mail_button)) {
        open_external_uri("mailto:sudoevolve@gmail.com");
    }

    const Rect info_area{
        content.x,
        hero.y + hero.h + 18.0f * scale,
        content.w,
        content.h - hero.h - 24.0f * scale,
    };
    const auto info_split = ui.split_h_ratio(info_area, 0.5f, 16.0f * scale);

    draw_fill(ui, info_split.first, palette.surface_deep, 22.0f * scale, 0.96f);
    draw_stroke(ui, info_split.first, palette.border_soft, 22.0f * scale, 1.0f, 0.88f);
    draw_text_left(ui, "Project", Rect{info_split.first.x + 20.0f * scale, info_split.first.y + 18.0f * scale,
                                       info_split.first.w - 40.0f * scale, 18.0f * scale},
                   font_heading(scale), palette.text, 0.98f);
    draw_text_left(ui, "Name: EUI", Rect{info_split.first.x + 20.0f * scale, info_split.first.y + 52.0f * scale,
                                         info_split.first.w - 40.0f * scale, 16.0f * scale},
                   font_body(scale), palette.text, 0.98f);
    draw_text_left(ui, "Author: SudoEvolve", Rect{info_split.first.x + 20.0f * scale, info_split.first.y + 78.0f * scale,
                                                  info_split.first.w - 40.0f * scale, 16.0f * scale},
                   font_body(scale), palette.text, 0.98f);
    draw_text_left(ui, "Renderer: Modern OpenGL", Rect{info_split.first.x + 20.0f * scale, info_split.first.y + 104.0f * scale,
                                                       info_split.first.w - 40.0f * scale, 16.0f * scale},
                   font_meta(scale), palette.muted, 0.98f);
    draw_text_left(ui, "Repository: github.com/sudoevolve", Rect{info_split.first.x + 20.0f * scale, info_split.first.y + 130.0f * scale,
                                                                 info_split.first.w - 40.0f * scale, 16.0f * scale},
                   font_meta(scale), palette.muted, 0.98f);

    draw_fill(ui, info_split.second, palette.surface_deep, 22.0f * scale, 0.96f);
    draw_stroke(ui, info_split.second, palette.border_soft, 22.0f * scale, 1.0f, 0.88f);
    draw_text_left(ui, "License & Contact", Rect{info_split.second.x + 20.0f * scale, info_split.second.y + 18.0f * scale,
                                                 info_split.second.w - 40.0f * scale, 18.0f * scale},
                   font_heading(scale), palette.text, 0.98f);
    draw_text_left(ui, "MIT License", Rect{info_split.second.x + 20.0f * scale, info_split.second.y + 52.0f * scale,
                                           info_split.second.w - 40.0f * scale, 16.0f * scale},
                   font_body(scale), palette.text, 0.98f);
    draw_text_left(ui, "Copyright (c) 2026 SudoEvolve", Rect{info_split.second.x + 20.0f * scale, info_split.second.y + 78.0f * scale,
                                                             info_split.second.w - 40.0f * scale, 16.0f * scale},
                   font_meta(scale), palette.muted, 0.98f);
    draw_text_left(ui, "Email: sudoevolve@gmail.com", Rect{info_split.second.x + 20.0f * scale, info_split.second.y + 104.0f * scale,
                                                           info_split.second.w - 40.0f * scale, 16.0f * scale},
                   font_body(scale), palette.text, 0.98f);
    draw_text_left(ui, "GitHub button opens the profile directly in your browser.", Rect{info_split.second.x + 20.0f * scale,
                                                                                           info_split.second.y + 132.0f * scale,
                                                                                           info_split.second.w - 40.0f * scale,
                                                                                           28.0f * scale},
                   font_meta(scale), palette.muted, 0.96f);
}

void draw_sidebar(UI& ui, const eui::InputState& input, GalleryState& state, const Rect& rect, float scale) {
    const GalleryPalette palette = make_gallery_palette(state);
    const std::uint32_t accent = accent_hex(state);
    draw_fill(ui, rect, palette.surface, 26.0f * scale, 1.0f);
    draw_stroke(ui, rect, palette.border, 26.0f * scale, 1.0f, 1.0f);

    const Rect inner = inset_rect(rect, 18.0f * scale);
    draw_text_left(ui, "EUI Gallery", Rect{inner.x, inner.y, inner.w, 22.0f * scale}, font_display(scale), palette.text, 1.0f);
    draw_text_left(ui, "Controls, design, layout, motion, dashboard, settings and about.",
                   Rect{inner.x, inner.y + 26.0f * scale, inner.w, 18.0f * scale}, font_meta(scale), palette.muted, 0.96f);

    const float row_h = 42.0f * scale;
    const float row_gap = 8.0f * scale;
    const float list_y = inner.y + 62.0f * scale;
    const float indicator_y_target = list_y + static_cast<float>(state.selected_page) * (row_h + row_gap);
    const std::uint64_t indicator_id = ui.id("gallery/sidebar-indicator");
    if (ui.motion_value(indicator_id, -1.0f) < 0.0f) {
        ui.reset_motion(indicator_id, indicator_y_target);
    }
    const float indicator_y = ui.motion(indicator_id, indicator_y_target, 18.0f);
    draw_fill(ui, Rect{inner.x, indicator_y, inner.w, row_h}, nav_selected_fill_hex(palette), 18.0f * scale, 0.96f);
    draw_stroke(ui, Rect{inner.x, indicator_y, inner.w, row_h}, accent, 18.0f * scale, 1.0f, 0.72f);
    draw_fill(ui, Rect{inner.x, indicator_y, 4.0f * scale, row_h}, accent, 2.0f * scale, 0.98f);

    for (std::size_t i = 0; i < kPages.size(); ++i) {
        const Rect row{
            inner.x,
            list_y + static_cast<float>(i) * (row_h + row_gap),
            inner.w,
            row_h,
        };
        const bool is_hovered = hovered(input, row);
        const bool is_selected = state.selected_page == static_cast<int>(i);
        const float hover_mix =
            ui.presence(ui.id("gallery/sidebar-hover", static_cast<std::uint64_t>(i + 1u)), is_hovered, 18.0f, 12.0f);

        if (!is_selected && is_hovered) {
            draw_fill(ui, row, palette.surface_deep, 18.0f * scale, 0.92f * hover_mix);
            draw_stroke(ui, row, palette.border_soft, 18.0f * scale, 1.0f, 0.88f * hover_mix);
        }
        if (clicked(input, row)) {
            state.selected_page = static_cast<int>(i);
        }

        draw_icon(ui, kPageIcons[i],
                  Rect{row.x + 14.0f * scale, row.y + (row.h - 14.0f * scale) * 0.5f, 14.0f * scale, 14.0f * scale},
                  is_selected ? accent : palette.muted, 0.98f);
        draw_text_left(ui, kPages[i].title,
                       Rect{row.x + 38.0f * scale, row.y + 12.0f * scale, row.w - 54.0f * scale, 16.0f * scale},
                       font_body(scale), is_selected ? palette.text : mix_hex(palette.text, palette.muted, 0.22f), 0.98f);
    }
}

void draw_stage(UI& ui, const eui::InputState& input, GalleryState& state, const Rect& rect, float scale, double time_seconds) {
    const GalleryPalette palette = make_gallery_palette(state);
    const PageDescriptor& page = kPages[static_cast<std::size_t>(std::clamp(state.selected_page, 0, static_cast<int>(kPages.size() - 1)))];

    draw_fill(ui, rect, palette.surface, 26.0f * scale, 1.0f);
    draw_stroke(ui, rect, palette.border, 26.0f * scale, 1.0f, 1.0f);

    const Rect inner = inset_rect(rect, 22.0f * scale);
    const Rect title_rect{inner.x, inner.y, inner.w, 24.0f * scale};
    const Rect summary_rect{inner.x, inner.y + 26.0f * scale, inner.w, 18.0f * scale};
    const Rect chip_rect{inner.x, inner.y + 56.0f * scale, 224.0f * scale, 30.0f * scale};
    const Rect stage_rect{inner.x, inner.y + 102.0f * scale, inner.w, inner.h - 102.0f * scale};

    draw_text_left(ui, page.title, title_rect, font_display(scale), palette.text, 1.0f);
    draw_text_left(ui, page.summary, summary_rect, font_meta(scale), palette.muted, 0.96f);
    draw_fill(ui, chip_rect, palette.surface_deep, chip_rect.h * 0.5f, 0.96f);
    draw_stroke(ui, chip_rect, palette.border_soft, chip_rect.h * 0.5f, 1.0f, 0.90f);
    draw_text_center(ui, page.api_label, chip_rect, font_meta(scale), mix_hex(palette.text, palette.accent, 0.40f), 0.98f);

    switch (state.selected_page) {
        case kPageBasicControls:
            draw_basic_controls_page(ui, input, state, stage_rect, scale);
            break;
        case kPageDesign:
            draw_design_page(ui, input, state, stage_rect, scale);
            break;
        case kPageLayout:
            draw_layout_page(ui, state, stage_rect, scale);
            break;
        case kPageAnimation:
            draw_animation_page(ui, input, state, stage_rect, scale, time_seconds);
            break;
        case kPageDashboard:
            draw_dashboard_page(ui, state, stage_rect, scale);
            break;
        case kPageSettings:
            draw_settings_page(ui, input, state, stage_rect, scale);
            break;
        case kPageAbout:
        default:
            draw_about_page(ui, input, state, stage_rect, scale);
            break;
    }
}

}  // namespace

int main() {
    GalleryState state{};

    eui::app::AppOptions options{};
    options.title = "EUI Gallery";
    options.width = 1420;
    options.height = 860;
    options.vsync = true;
    options.continuous_render = false;
    options.max_fps = 240.0;
    options.text_font_family = "Segoe UI";
    options.text_font_weight = 600;
    options.icon_font_family = "Font Awesome 7 Free Solid";
    options.icon_font_file = "include/Font Awesome 7 Free-Solid-900.otf";

    return eui::app::run(
        [&](eui::app::FrameContext frame) {
            auto& ctx = frame.context();
            const auto metrics = frame.window_metrics();
            const auto& input = ctx.input_state();
            const float scale = std::max(1.0f, metrics.dpi_scale);
            const double time_seconds = input.time_seconds;

            const std::uint32_t accent = accent_hex(state);
            const GalleryPalette palette = make_gallery_palette(state);
            ctx.set_theme_mode(state.light_mode ? eui::ThemeMode::Light : eui::ThemeMode::Dark);
            ctx.set_primary_color(color_from_hex(accent));
            ctx.set_corner_radius(std::max(8.0f, state.layout_radius * scale * 0.7f));

            UI ui(ctx);
            const float margin = 18.0f * scale;
            const Rect frame_rect{
                margin,
                margin,
                std::max(0.0f, static_cast<float>(metrics.framebuffer_w) - margin * 2.0f),
                std::max(0.0f, static_cast<float>(metrics.framebuffer_h) - margin * 2.0f),
            };

            ui.panel()
                .in(frame_rect)
                .padding(18.0f * scale)
                .radius(30.0f * scale)
                .gradient(palette.shell_top, palette.shell_bottom)
                .stroke(palette.border, 1.0f)
                .shadow(0.0f, 14.0f * scale, 28.0f * scale, panel_shadow_hex(palette), palette.light ? 0.10f : 0.18f)
                .begin([&](auto& root) {
                    const Rect header = root.dock_top(68.0f * scale, 16.0f * scale);
                    draw_fill(ui, header, palette.surface_alt, 22.0f * scale, 0.96f);
                    draw_stroke(ui, header, palette.border, 22.0f * scale, 1.0f, 1.0f);
                    draw_text_left(ui, "EUI Gallery",
                                   Rect{header.x + 22.0f * scale, header.y + 14.0f * scale, header.w - 44.0f * scale, 20.0f * scale},
                                   font_display(scale), palette.text, 1.0f);
                    draw_text_left(ui, "Left side is now top-level navigation only. Animation detail lives inside the Animation page.",
                                   Rect{header.x + 22.0f * scale, header.y + 36.0f * scale, header.w - 44.0f * scale, 16.0f * scale},
                                   font_meta(scale), palette.muted, 0.96f);

                    const auto split = root.split_h_ratio(root.content(), 0.22f, 18.0f * scale);
                    draw_sidebar(ui, input, state, split.first, scale);
                    draw_stage(ui, input, state, split.second, scale, time_seconds);
                });

            frame.request_next_frame();
        },
        options);
}

#if defined(_WIN32)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    return main();
}
#endif
