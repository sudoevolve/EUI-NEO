inline bool font_file_exists(const std::string& path) {
    if (path.empty()) {
        return false;
    }
    std::ifstream ifs(path, std::ios::binary);
    return static_cast<bool>(ifs);
}

inline std::string resolve_bundled_icon_font_file() {
    static constexpr const char* kBundledFont = "Font Awesome 7 Free-Solid-900.otf";
    static constexpr const char* kCandidates[] = {
        "include/Font Awesome 7 Free-Solid-900.otf",
        "./include/Font Awesome 7 Free-Solid-900.otf",
        "../include/Font Awesome 7 Free-Solid-900.otf",
        "../../include/Font Awesome 7 Free-Solid-900.otf",
    };
    for (const char* candidate : kCandidates) {
        if (candidate != nullptr && font_file_exists(candidate)) {
            return std::string(candidate);
        }
    }
    (void)kBundledFont;
    return {};
}

inline std::string resolve_icon_font_file_path(const char* explicit_file) {
    if (explicit_file != nullptr && explicit_file[0] != '\0') {
        const std::string explicit_path = explicit_file;
        if (font_file_exists(explicit_path)) {
            return explicit_path;
        }
    }
    const std::string bundled = resolve_bundled_icon_font_file();
    if (!bundled.empty()) {
        return bundled;
    }
    if (explicit_file != nullptr && explicit_file[0] != '\0') {
        return std::string(explicit_file);
    }
    return {};
}

#ifndef _WIN32
inline bool utf8_next(std::string_view text, std::size_t& index, std::uint32_t& codepoint) {
    if (index >= text.size()) {
        return false;
    }
    const unsigned char lead = static_cast<unsigned char>(text[index]);
    if (lead < 0x80u) {
        codepoint = lead;
        index += 1u;
        return true;
    }
    if ((lead >> 5u) == 0x6u && index + 1u < text.size()) {
        const unsigned char b1 = static_cast<unsigned char>(text[index + 1u]);
        if ((b1 & 0xC0u) == 0x80u) {
            codepoint = (static_cast<std::uint32_t>(lead & 0x1Fu) << 6u) |
                        static_cast<std::uint32_t>(b1 & 0x3Fu);
            index += 2u;
            return true;
        }
    }
    if ((lead >> 4u) == 0xEu && index + 2u < text.size()) {
        const unsigned char b1 = static_cast<unsigned char>(text[index + 1u]);
        const unsigned char b2 = static_cast<unsigned char>(text[index + 2u]);
        if ((b1 & 0xC0u) == 0x80u && (b2 & 0xC0u) == 0x80u) {
            codepoint = (static_cast<std::uint32_t>(lead & 0x0Fu) << 12u) |
                        (static_cast<std::uint32_t>(b1 & 0x3Fu) << 6u) |
                        static_cast<std::uint32_t>(b2 & 0x3Fu);
            index += 3u;
            return true;
        }
    }
    if ((lead >> 3u) == 0x1Eu && index + 3u < text.size()) {
        const unsigned char b1 = static_cast<unsigned char>(text[index + 1u]);
        const unsigned char b2 = static_cast<unsigned char>(text[index + 2u]);
        const unsigned char b3 = static_cast<unsigned char>(text[index + 3u]);
        if ((b1 & 0xC0u) == 0x80u && (b2 & 0xC0u) == 0x80u && (b3 & 0xC0u) == 0x80u) {
            codepoint = (static_cast<std::uint32_t>(lead & 0x07u) << 18u) |
                        (static_cast<std::uint32_t>(b1 & 0x3Fu) << 12u) |
                        (static_cast<std::uint32_t>(b2 & 0x3Fu) << 6u) |
                        static_cast<std::uint32_t>(b3 & 0x3Fu);
            index += 4u;
            return true;
        }
    }
    codepoint = static_cast<std::uint32_t>(lead);
    index += 1u;
    return true;
}

#endif
#endif

#ifdef _WIN32
inline std::wstring utf8_to_wide(std::string_view text) {
    if (text.empty()) {
        return std::wstring{};
    }
    const int count =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (count <= 0) {
        std::wstring fallback;
        fallback.reserve(text.size());
        for (char ch : text) {
            fallback.push_back(static_cast<unsigned char>(ch));
        }
        return fallback;
    }
    std::wstring out(static_cast<std::size_t>(count), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), out.data(),
                        count);
    return out;
}

inline bool utf8_next(std::string_view text, std::size_t& index, std::uint32_t& codepoint) {
    if (index >= text.size()) {
        return false;
    }
    const unsigned char lead = static_cast<unsigned char>(text[index]);
    if (lead < 0x80u) {
        codepoint = lead;
        index += 1u;
        return true;
    }
    if ((lead >> 5u) == 0x6u && index + 1u < text.size()) {
        const unsigned char b1 = static_cast<unsigned char>(text[index + 1u]);
        if ((b1 & 0xC0u) == 0x80u) {
            codepoint = (static_cast<std::uint32_t>(lead & 0x1Fu) << 6u) |
                        static_cast<std::uint32_t>(b1 & 0x3Fu);
            index += 2u;
            return true;
        }
    }
    if ((lead >> 4u) == 0xEu && index + 2u < text.size()) {
        const unsigned char b1 = static_cast<unsigned char>(text[index + 1u]);
        const unsigned char b2 = static_cast<unsigned char>(text[index + 2u]);
        if ((b1 & 0xC0u) == 0x80u && (b2 & 0xC0u) == 0x80u) {
            codepoint = (static_cast<std::uint32_t>(lead & 0x0Fu) << 12u) |
                        (static_cast<std::uint32_t>(b1 & 0x3Fu) << 6u) |
                        static_cast<std::uint32_t>(b2 & 0x3Fu);
            index += 3u;
            return true;
        }
    }
    if ((lead >> 3u) == 0x1Eu && index + 3u < text.size()) {
        const unsigned char b1 = static_cast<unsigned char>(text[index + 1u]);
        const unsigned char b2 = static_cast<unsigned char>(text[index + 2u]);
        const unsigned char b3 = static_cast<unsigned char>(text[index + 3u]);
        if ((b1 & 0xC0u) == 0x80u && (b2 & 0xC0u) == 0x80u && (b3 & 0xC0u) == 0x80u) {
            codepoint = (static_cast<std::uint32_t>(lead & 0x07u) << 18u) |
                        (static_cast<std::uint32_t>(b1 & 0x3Fu) << 12u) |
                        (static_cast<std::uint32_t>(b2 & 0x3Fu) << 6u) |
                        static_cast<std::uint32_t>(b3 & 0x3Fu);
            index += 4u;
            return true;
        }
    }
    codepoint = static_cast<std::uint32_t>(lead);
    index += 1u;
    return true;
}

