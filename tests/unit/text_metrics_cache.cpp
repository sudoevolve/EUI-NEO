#include "core/render/text.h"
#include <ft2build.h>
#include FT_FREETYPE_H

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {
void appendUtf8(std::string &text, unsigned int code) {
    if (code < 0x80)
        text += static_cast<char>(code);
    else if (code < 0x800) {
        text += static_cast<char>(0xc0 | (code >> 6));
        text += static_cast<char>(0x80 | (code & 63));
    } else {
        text += static_cast<char>(0xe0 | (code >> 12));
        text += static_cast<char>(0x80 | ((code >> 6) & 63));
        text += static_cast<char>(0x80 | (code & 63));
    }
}
std::uint64_t fingerprint(const core::TextPrimitive::TextMetrics &metrics) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (std::size_t i = 0; i < metrics.caretX.size(); ++i) {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &metrics.caretX[i], sizeof(bits));
        hash = (hash ^ bits) * 1099511628211ULL;
        hash = (hash ^ static_cast<unsigned int>(metrics.byteIndices[i])) * 1099511628211ULL;
    }
    return hash;
}
} // namespace

int main(int argc, char **argv) {
    const bool benchmark = argc > 1 && std::string(argv[1]) == "--benchmark";
    const char *customFont = std::getenv("EUI_TEXT_TEST_FONT");
    const auto root = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
    const std::string font =
        customFont ? customFont
                   : (root / "assets/JingNanJunJunTi-JinNanJunJunTi-Bold-2.ttf").string();
    core::TextPrimitive::setDefaultFontFiles(font, {});
    FT_Library library = nullptr;
    FT_Face face = nullptr;
    if (FT_Init_FreeType(&library) || FT_New_Face(library, font.c_str(), 0, &face))
        return 1;
    if (!FT_IS_SCALABLE(face))
        return 1;
    const auto flags = FT_LOAD_DEFAULT | FT_LOAD_COLOR | FT_LOAD_NO_SVG | FT_LOAD_TARGET_LIGHT;
    int checks = 0;
    // Independent FreeType oracle: many unique glyphs, size churn, then the
    // same glyphs in reverse order exercise eviction, collisions and face moves.
    for (const float size : {12.f, 20.272f, 40.f, 20.272f}) {
        const float scaled =
            std::max(1.f, size * face->units_per_EM / (face->ascender - face->descender));
        if (FT_Set_Char_Size(face, 0, static_cast<FT_F26Dot6>(std::lround(scaled * 64)), 72, 72))
            return 1;
        for (int pass = 0; pass < 2; ++pass) {
            std::string text;
            float expected = 0;
            for (unsigned int i = 0; i < 3000; ++i) {
                const auto code = 0x4e00 + (pass ? 2999 - i : i);
                const auto glyph = FT_Get_Char_Index(face, code);
                if (!glyph)
                    continue;
                appendUtf8(text, code);
                if (FT_Load_Glyph(face, glyph, flags))
                    return 1;
                expected += static_cast<float>(face->glyph->linearHoriAdvance) / 65536.f;
            }
            for (int repeat = 0; repeat < 2; ++repeat) {
                const auto actual = core::TextPrimitive::measureTextMetrics(text, font, size);
                if (std::fabs(actual.width - expected) > .01f) {
                    std::cerr << "FreeType advance mismatch: " << actual.width << " != " << expected
                              << '\n';
                    return 1;
                }
                ++checks;
            }
        }
    }
    FT_Done_Face(face);
    FT_Done_FreeType(library);
    if (!benchmark) {
        std::cout << checks << " FreeType oracle checks passed\n";
        return 0;
    }
    using Clock = std::chrono::steady_clock;
    std::string diverse;
    for (unsigned int i = 0; i < 3000; ++i)
        appendUtf8(diverse, 0x4e00 + i);
    std::string mixed;
    while (mixed.size() < 8192)
        mixed += "中文输入 English 123，标点。";
    const std::vector<std::pair<std::string, std::string>> cases = {
        {"latin-short", "A quick brown fox 0123456789"},
        {"mixed-short", "中文输入 English 123，标点。"},
        {"emoji-combining", "A e\xcc\x81 \xf0\x9f\x98\x80 \xf0\x9f\x9a\x80 \t中"},
        {"mixed-long", mixed},
        {"cjk-diverse", diverse},
        {"latin-long", std::string(8192, 'W')}};
    for (const float size : {20.272f, 46.336f}) {
        for (auto scenario : cases) {
            // No whole-string metric cache: changing the suffix models typing.
            std::vector<double> samples;
            std::uint64_t digest = 0;
            for (int i = 0; i < 65; ++i) {
                scenario.second += 'x';
                const auto start = Clock::now();
                const auto metrics =
                    core::TextPrimitive::measureTextMetrics(scenario.second, font, size);
                const auto duration =
                    std::chrono::duration<double, std::milli>(Clock::now() - start).count();
                digest ^= fingerprint(metrics);
                if (i >= 5)
                    samples.push_back(duration);
            }
            std::sort(samples.begin(), samples.end());
            std::cout << "{\"case\":\"" << scenario.first << "\",\"size\":" << size
                      << ",\"p50_ms\":" << samples[30] << ",\"p95_ms\":" << samples[57]
                      << ",\"digest\":" << digest << "}\n";
        }
    }
    return 0;
}
