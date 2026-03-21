#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#if EUI_ENABLE_STB_TRUETYPE
#include "stb_truetype.h"
#endif

#include "eui/core/foundation.h"

namespace eui::detail {

#if EUI_ENABLE_STB_TRUETYPE
struct ContextStbMeasureFont {
    std::shared_ptr<std::vector<unsigned char>> data_blob{};
    stbtt_fontinfo info{};
    float scale{0.0f};
    bool valid{false};
};
#endif

struct ContextTextMeasureState {
#if defined(_WIN32)
    HDC hdc{nullptr};
    std::unordered_map<std::string, HFONT> runtime_fonts{};
#endif
    TextMeasureBackend backend{TextMeasureBackend::Approx};
    std::string font_family{"Segoe UI"};
    int font_weight{600};
    std::string font_file{};
    std::string icon_font_family{"Font Awesome 7 Free Solid"};
    std::string icon_font_file{};
    bool enable_icon_fallback{true};
#if EUI_ENABLE_STB_TRUETYPE
    std::unordered_map<std::string, ContextStbMeasureFont> stb_fonts{};
#endif
};

inline bool context_private_use_codepoint_measure(std::uint32_t cp) {
    return cp >= 0xE000u && cp <= 0xF8FFu;
}

inline void context_release_stb_measure_cache(ContextTextMeasureState& state) {
#if EUI_ENABLE_STB_TRUETYPE
    state.stb_fonts.clear();
#else
    (void)state;
#endif
}

inline void context_release_text_measure_cache(ContextTextMeasureState& state) {
#if defined(_WIN32)
    for (auto& pair : state.runtime_fonts) {
        if (pair.second != nullptr) {
            DeleteObject(pair.second);
        }
    }
    state.runtime_fonts.clear();
    if (state.hdc != nullptr) {
        DeleteDC(state.hdc);
        state.hdc = nullptr;
    }
#else
    (void)state;
#endif
}

inline void context_configure_text_measure(ContextTextMeasureState& state, TextMeasureBackend backend,
                                           std::string_view family, int weight, std::string_view font_file = {},
                                           std::string_view icon_family = {}, std::string_view icon_file = {},
                                           bool enable_icon_fallback = true) {
    const std::string resolved_family =
        family.empty() ? std::string("Segoe UI") : std::string(family);
    const int resolved_weight = std::clamp(weight, 100, 900);
    const std::string resolved_file = std::string(font_file);
    const std::string resolved_icon_family =
        icon_family.empty() ? std::string("Font Awesome 7 Free Solid") : std::string(icon_family);
    const std::string resolved_icon_file = std::string(icon_file);

    const bool backend_changed = state.backend != backend;
    const bool family_changed = state.font_family != resolved_family;
    const bool weight_changed = state.font_weight != resolved_weight;
    const bool file_changed = state.font_file != resolved_file;
    const bool icon_family_changed = state.icon_font_family != resolved_icon_family;
    const bool icon_file_changed = state.icon_font_file != resolved_icon_file;
    const bool icon_fallback_changed = state.enable_icon_fallback != enable_icon_fallback;

    state.backend = backend;
    state.font_family = resolved_family;
    state.font_weight = resolved_weight;
    state.font_file = resolved_file;
    state.icon_font_family = resolved_icon_family;
    state.icon_font_file = resolved_icon_file;
    state.enable_icon_fallback = enable_icon_fallback;

    if (backend_changed || family_changed || weight_changed || icon_family_changed || icon_fallback_changed) {
        context_release_text_measure_cache(state);
    }
    if (backend_changed || file_changed || icon_file_changed || icon_fallback_changed) {
        context_release_stb_measure_cache(state);
    }
}

#if defined(_WIN32)
inline std::wstring context_utf8_to_wide_measure(std::string_view text) {
    if (text.empty()) {
        return {};
    }
    const int needed =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (needed <= 0) {
        std::wstring fallback;
        fallback.reserve(text.size());
        for (unsigned char ch : text) {
            fallback.push_back(static_cast<wchar_t>(ch));
        }
        return fallback;
    }
    std::wstring wide(static_cast<std::size_t>(needed), L'\0');
    const int written = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                                            static_cast<int>(text.size()), wide.data(), needed);
    if (written <= 0) {
        return {};
    }
    return wide;
}

inline HDC context_ensure_text_measure_hdc(ContextTextMeasureState& state) {
    if (state.hdc == nullptr) {
        state.hdc = CreateCompatibleDC(nullptr);
    }
    return state.hdc;
}

inline int context_measure_codepoint_to_utf16(std::uint32_t cp, wchar_t out[2]) {
    if (cp <= 0xFFFFu) {
        out[0] = static_cast<wchar_t>(cp);
        out[1] = L'\0';
        return 1;
    }
    out[0] = L'\0';
    out[1] = L'\0';
    return 0;
}

inline HFONT context_ensure_text_measure_font(ContextTextMeasureState& state, bool icon_font, int px) {
    const int clamped_px = std::max(8, px);
    const std::string& family_name = icon_font ? state.icon_font_family : state.font_family;
    const int font_weight = icon_font ? 400 : state.font_weight;
    const std::string key =
        std::string(icon_font ? "icon:" : "text:") + family_name + "#" + std::to_string(font_weight) + "#" +
        std::to_string(clamped_px);
    auto it = state.runtime_fonts.find(key);
    if (it != state.runtime_fonts.end()) {
        return it->second;
    }

    std::wstring family = context_utf8_to_wide_measure(family_name);
    if (family.empty()) {
        family = icon_font ? L"Font Awesome 7 Free Solid" : L"Segoe UI";
    }
    HFONT font = CreateFontW(-clamped_px, 0, 0, 0, font_weight, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_DONTCARE, family.c_str());
    if (font == nullptr) {
        font = CreateFontW(-clamped_px, 0, 0, 0, font_weight, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                           DEFAULT_PITCH | FF_DONTCARE, icon_font ? L"Segoe UI Symbol" : L"Segoe UI");
    }
    if (font == nullptr) {
        font = CreateFontW(-clamped_px, 0, 0, 0, font_weight, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                           DEFAULT_PITCH | FF_DONTCARE, L"Arial");
    }
    state.runtime_fonts[key] = font;
    return font;
}

inline bool context_measure_font_has_glyph(HDC hdc, HFONT font, std::uint32_t cp) {
    if (hdc == nullptr || font == nullptr) {
        return false;
    }
    wchar_t wide[2] = {L'\0', L'\0'};
    const int wide_len = context_measure_codepoint_to_utf16(cp, wide);
    if (wide_len <= 0) {
        return false;
    }
    HGDIOBJ old = SelectObject(hdc, font);
    WORD glyph_index = 0xFFFFu;
    const DWORD glyph_query = GetGlyphIndicesW(hdc, wide, wide_len, &glyph_index, GGI_MARK_NONEXISTING_GLYPHS);
    if (old != nullptr) {
        SelectObject(hdc, old);
    }
    return glyph_query != GDI_ERROR && glyph_index != 0xFFFFu;
}
#endif

inline float context_measure_text_width_runtime(ContextTextMeasureState& state, std::uint32_t cp, float font_size) {
#if defined(_WIN32)
    HDC hdc = context_ensure_text_measure_hdc(state);
    if (hdc == nullptr) {
        return 0.0f;
    }

    const int text_font_px = std::max(8, static_cast<int>(std::round(font_size * 1.20f)));
    const int icon_font_px = std::max(8, static_cast<int>(std::round(text_font_px * k_icon_visual_scale)));
    HFONT text_font = context_ensure_text_measure_font(state, false, text_font_px);
    HFONT icon_font = state.enable_icon_fallback ? context_ensure_text_measure_font(state, true, icon_font_px)
                                                 : nullptr;

    HFONT measure_font = text_font;
    const bool prefer_icon =
        state.enable_icon_fallback && icon_font != nullptr && context_private_use_codepoint_measure(cp);
    if (prefer_icon && context_measure_font_has_glyph(hdc, icon_font, cp)) {
        measure_font = icon_font;
    } else if (text_font == nullptr || !context_measure_font_has_glyph(hdc, text_font, cp)) {
        if (state.enable_icon_fallback && icon_font != nullptr && context_measure_font_has_glyph(hdc, icon_font, cp)) {
            measure_font = icon_font;
        }
    }

    if (measure_font == nullptr) {
        return 0.0f;
    }

    wchar_t wide[2] = {L'\0', L'\0'};
    const int wide_len = context_measure_codepoint_to_utf16(cp, wide);
    if (wide_len <= 0) {
        return 0.0f;
    }

    HGDIOBJ old = SelectObject(hdc, measure_font);
    SIZE size{};
    if (GetTextExtentPoint32W(hdc, wide, wide_len, &size) == FALSE) {
        size.cx = 0;
    }
    if (old != nullptr) {
        SelectObject(hdc, old);
    }
    return std::max(0.0f, static_cast<float>(size.cx));
#else
    (void)state;
    (void)cp;
    (void)font_size;
    return 0.0f;
#endif
}

inline int context_quantize_text_measure_font_px(int px) {
    int clamped = std::max(8, px);
    if (clamped <= 32) {
        clamped = ((clamped + 1) / 2) * 2;
    } else if (clamped <= 72) {
        clamped = ((clamped + 2) / 4) * 4;
    } else {
        clamped = ((clamped + 4) / 8) * 8;
    }
    return std::clamp(clamped, 8, 200);
}

inline std::vector<unsigned char> context_read_file_bytes(const std::string& path) {
    if (path.empty()) {
        return {};
    }
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) {
        return {};
    }
    ifs.seekg(0, std::ios::end);
    const std::streamoff size = ifs.tellg();
    if (size <= 0) {
        return {};
    }
    ifs.seekg(0, std::ios::beg);
    std::vector<unsigned char> bytes(static_cast<std::size_t>(size));
    ifs.read(reinterpret_cast<char*>(bytes.data()), size);
    if (!ifs) {
        return {};
    }
    return bytes;
}

inline std::shared_ptr<std::vector<unsigned char>> context_load_shared_font_blob(const std::string& path) {
#if EUI_ENABLE_STB_TRUETYPE
    if (path.empty()) {
        return {};
    }

    static std::unordered_map<std::string, std::weak_ptr<std::vector<unsigned char>>> shared_blobs{};
    auto it = shared_blobs.find(path);
    if (it != shared_blobs.end()) {
        auto data = it->second.lock();
        if (data != nullptr && !data->empty()) {
            return data;
        }
        shared_blobs.erase(it);
    }

    auto data = std::make_shared<std::vector<unsigned char>>(context_read_file_bytes(path));
    if (data->empty()) {
        return {};
    }
    shared_blobs[path] = data;
    return data;
#else
    (void)path;
    return {};
#endif
}

inline const ContextStbMeasureFont* context_ensure_stb_measure_font(ContextTextMeasureState& state, bool icon_font,
                                                                    int px) {
#if EUI_ENABLE_STB_TRUETYPE
    const std::string& font_file = icon_font ? state.icon_font_file : state.font_file;
    if (font_file.empty()) {
        return nullptr;
    }

    const int quantized_px = context_quantize_text_measure_font_px(px);
    const std::string key =
        std::string(icon_font ? "icon:" : "text:") + font_file + "#" + std::to_string(quantized_px);

    auto it = state.stb_fonts.find(key);
    if (it == state.stb_fonts.end()) {
        ContextStbMeasureFont font{};
        font.data_blob = context_load_shared_font_blob(font_file);
        if (font.data_blob != nullptr && !font.data_blob->empty()) {
            const int offset = stbtt_GetFontOffsetForIndex(font.data_blob->data(), 0);
            if (offset >= 0 && stbtt_InitFont(&font.info, font.data_blob->data(), offset) != 0) {
                font.scale = stbtt_ScaleForPixelHeight(&font.info, static_cast<float>(quantized_px));
                font.valid = font.scale > 0.0f;
            }
        }
        it = state.stb_fonts.emplace(key, std::move(font)).first;
    }
    return it->second.valid ? &it->second : nullptr;
#else
    (void)state;
    (void)icon_font;
    (void)px;
    return nullptr;
#endif
}

inline float context_measure_text_width_stb(ContextTextMeasureState& state, std::uint32_t cp, float font_size) {
#if EUI_ENABLE_STB_TRUETYPE
    const int text_font_px = std::max(8, static_cast<int>(std::round(font_size * 1.20f)));
    const int icon_font_px = std::max(8, static_cast<int>(std::round(text_font_px * k_icon_visual_scale)));
    const ContextStbMeasureFont* text_font = context_ensure_stb_measure_font(state, false, text_font_px);
    const ContextStbMeasureFont* icon_font =
        state.enable_icon_fallback ? context_ensure_stb_measure_font(state, true, icon_font_px) : nullptr;

    auto glyph_index_for = [&](const ContextStbMeasureFont* font) -> int {
        if (font == nullptr || !font->valid) {
            return 0;
        }
        return stbtt_FindGlyphIndex(&font->info, static_cast<int>(cp));
    };

    const ContextStbMeasureFont* measure_font = text_font;
    int glyph_index = glyph_index_for(text_font);
    const bool prefer_icon =
        state.enable_icon_fallback && icon_font != nullptr && context_private_use_codepoint_measure(cp);
    if (prefer_icon) {
        const int icon_glyph_index = glyph_index_for(icon_font);
        if (icon_glyph_index != 0) {
            measure_font = icon_font;
            glyph_index = icon_glyph_index;
        }
    } else if ((measure_font == nullptr || glyph_index == 0) && state.enable_icon_fallback) {
        const int icon_glyph_index = glyph_index_for(icon_font);
        if (icon_font != nullptr && icon_glyph_index != 0) {
            measure_font = icon_font;
            glyph_index = icon_glyph_index;
        }
    }

    if (measure_font == nullptr || !measure_font->valid) {
        return 0.0f;
    }
    if (glyph_index == 0 && cp != 0u) {
        glyph_index = stbtt_FindGlyphIndex(&measure_font->info, static_cast<int>('?'));
    }
    if (glyph_index == 0) {
        return 0.0f;
    }
    int advance = 0;
    int lsb = 0;
    stbtt_GetGlyphHMetrics(&measure_font->info, glyph_index, &advance, &lsb);
    (void)lsb;
    return std::max(0.0f, static_cast<float>(advance) * measure_font->scale);
#else
    (void)state;
    (void)cp;
    (void)font_size;
    return 0.0f;
#endif
}

inline float context_approx_char_width(float font_size) {
    return std::max(5.0f, font_size * 0.58f);
}

inline bool context_decode_utf8_at(std::string_view text, std::size_t index, std::size_t limit, std::uint32_t& cp,
                                   std::size_t& next) {
    cp = 0u;
    next = index;
    if (index >= text.size() || index >= limit) {
        return false;
    }

    const unsigned char lead = static_cast<unsigned char>(text[index]);
    if (lead < 0x80u) {
        cp = static_cast<std::uint32_t>(lead);
        next = index + 1u;
        return true;
    }

    if ((lead >> 5u) == 0x6u && index + 1u < limit) {
        const unsigned char b1 = static_cast<unsigned char>(text[index + 1u]);
        if ((b1 & 0xC0u) == 0x80u) {
            cp = (static_cast<std::uint32_t>(lead & 0x1Fu) << 6u) |
                 static_cast<std::uint32_t>(b1 & 0x3Fu);
            next = index + 2u;
            return true;
        }
    }
    if ((lead >> 4u) == 0xEu && index + 2u < limit) {
        const unsigned char b1 = static_cast<unsigned char>(text[index + 1u]);
        const unsigned char b2 = static_cast<unsigned char>(text[index + 2u]);
        if ((b1 & 0xC0u) == 0x80u && (b2 & 0xC0u) == 0x80u) {
            cp = (static_cast<std::uint32_t>(lead & 0x0Fu) << 12u) |
                 (static_cast<std::uint32_t>(b1 & 0x3Fu) << 6u) |
                 static_cast<std::uint32_t>(b2 & 0x3Fu);
            next = index + 3u;
            return true;
        }
    }
    if ((lead >> 3u) == 0x1Eu && index + 3u < limit) {
        const unsigned char b1 = static_cast<unsigned char>(text[index + 1u]);
        const unsigned char b2 = static_cast<unsigned char>(text[index + 2u]);
        const unsigned char b3 = static_cast<unsigned char>(text[index + 3u]);
        if ((b1 & 0xC0u) == 0x80u && (b2 & 0xC0u) == 0x80u && (b3 & 0xC0u) == 0x80u) {
            cp = (static_cast<std::uint32_t>(lead & 0x07u) << 18u) |
                 (static_cast<std::uint32_t>(b1 & 0x3Fu) << 12u) |
                 (static_cast<std::uint32_t>(b2 & 0x3Fu) << 6u) |
                 static_cast<std::uint32_t>(b3 & 0x3Fu);
            next = index + 4u;
            return true;
        }
    }

    cp = static_cast<std::uint32_t>(lead);
    next = index + 1u;
    return false;
}

inline float context_approx_codepoint_advance(std::uint32_t cp, float font_size) {
    constexpr float k_measure_correction = 0.92f;
    const float base = std::max(5.0f, font_size);
    if (cp <= 0x7Fu) {
        const char ch = static_cast<char>(cp);
        float factor = 0.52f;
        if (ch == ' ') {
            factor = 0.30f;
        } else if (std::strchr(".,:;'`!|", ch) != nullptr) {
            factor = 0.28f;
        } else if (std::strchr("()[]{}ilIjtfr", ch) != nullptr) {
            factor = 0.36f;
        } else if (std::strchr("mwMW@#%&", ch) != nullptr) {
            factor = 0.78f;
        } else if (ch >= '0' && ch <= '9') {
            factor = 0.52f;
        } else if (ch >= 'A' && ch <= 'Z') {
            factor = 0.58f;
        } else if (ch >= 'a' && ch <= 'z') {
            factor = 0.50f;
        } else {
            factor = 0.50f;
        }
        return std::max(3.0f, base * factor * k_measure_correction);
    }

    if ((cp >= 0x4E00u && cp <= 0x9FFFu) || (cp >= 0x3400u && cp <= 0x4DBFu) ||
        (cp >= 0xAC00u && cp <= 0xD7AFu) || (cp >= 0x3040u && cp <= 0x30FFu)) {
        return std::max(6.0f, base * 1.0f * k_measure_correction);
    }
    if (cp >= 0x1F300u && cp <= 0x1FAFFu) {
        return std::max(6.0f, base * 1.05f * k_measure_correction);
    }
    return context_approx_char_width(font_size) * k_measure_correction;
}

inline float context_measure_codepoint_advance(ContextTextMeasureState& state, std::uint32_t cp, float font_size) {
    float measured = 0.0f;
    if (state.backend == TextMeasureBackend::Win32) {
        measured = context_measure_text_width_runtime(state, cp, font_size);
    } else if (state.backend == TextMeasureBackend::Stb) {
        measured = context_measure_text_width_stb(state, cp, font_size);
    }
    if (measured <= 0.0f) {
        measured = context_approx_codepoint_advance(cp, font_size);
    }
    return std::max(1.0f, measured);
}

inline float context_text_width_until(ContextTextMeasureState& state, std::string_view text, std::size_t byte_index,
                                      float font_size) {
    const std::size_t target = std::min(byte_index, text.size());
    std::size_t index = 0u;
    float width = 0.0f;
    while (index < target) {
        std::size_t next = index;
        std::uint32_t cp = 0u;
        context_decode_utf8_at(text, index, target, cp, next);
        width += context_measure_codepoint_advance(state, cp, font_size);
        index = next;
    }
    return width;
}

}  // namespace eui::detail
