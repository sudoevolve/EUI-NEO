#include "core/text.h"

#include <glad/glad.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "3rd/stb_truetype.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace core {

namespace {

constexpr int SdfPadding = 5;

struct FontFace {
    std::string path;
    std::shared_ptr<std::vector<unsigned char>> data;
    stbtt_fontinfo info;
    float scale = 1.0f;
    float ascent = 0.0f;
    float descent = 0.0f;
    float lineGap = 0.0f;
    bool useSdf = true;
};

struct FontInfoHolder {
    std::vector<FontFace> faces;
};

struct SharedTextAtlas {
    GLuint texture = 0;
    int width = 1024;
    int height = 1024;
    int x = 1;
    int y = 1;
    int rowHeight = 0;
    int references = 0;
    std::unordered_map<std::string, TextPrimitive::Glyph> glyphs;
};

SharedTextAtlas& sharedTextAtlas() {
    static SharedTextAtlas atlas;
    return atlas;
}

bool retainSharedTextAtlas() {
    SharedTextAtlas& atlas = sharedTextAtlas();
    ++atlas.references;
    if (atlas.texture != 0) {
        return true;
    }

    glGenTextures(1, &atlas.texture);
    glBindTexture(GL_TEXTURE_2D, atlas.texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, atlas.width, atlas.height, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    return atlas.texture != 0;
}

void releaseSharedTextAtlas() {
    SharedTextAtlas& atlas = sharedTextAtlas();
    atlas.references = std::max(0, atlas.references - 1);
    if (atlas.references > 0 || atlas.texture == 0) {
        return;
    }

    glDeleteTextures(1, &atlas.texture);
    atlas.texture = 0;
    atlas.x = 1;
    atlas.y = 1;
    atlas.rowHeight = 0;
    atlas.glyphs.clear();
}

std::string glyphCacheKey(const FontFace& face, float fontSize, unsigned int codepoint) {
    return face.path + "#" +
           std::to_string(static_cast<int>(std::round(fontSize * 64.0f))) + "#" +
           std::to_string(codepoint);
}

std::string existingPath(const std::filesystem::path& path) {
    std::error_code error;
    if (std::filesystem::exists(path, error)) {
        return path.string();
    }
    return {};
}

std::string resolveProjectAssetPath(const std::string& filename) {
    const std::filesystem::path sourceRoot = std::filesystem::path(__FILE__).parent_path().parent_path();
    const std::filesystem::path candidates[] = {
        std::filesystem::path("assets") / filename,
        std::filesystem::path("..") / "assets" / filename,
        std::filesystem::path("..") / ".." / "assets" / filename,
        sourceRoot / "assets" / filename
    };

    for (const auto& candidate : candidates) {
        if (const std::string path = existingPath(candidate); !path.empty()) {
            return path;
        }
    }
    return (sourceRoot / "assets" / filename).string();
}

std::string resolveFontFilePath(const std::string& path) {
    const std::filesystem::path raw(path);
    if (const std::string existing = existingPath(raw); !existing.empty()) {
        return existing;
    }

    const std::filesystem::path sourceRoot = std::filesystem::path(__FILE__).parent_path().parent_path();
    const std::filesystem::path candidates[] = {
        std::filesystem::path("assets") / raw.filename(),
        std::filesystem::path("..") / "assets" / raw.filename(),
        std::filesystem::path("..") / ".." / "assets" / raw.filename(),
        sourceRoot / "assets" / raw.filename()
    };

    for (const auto& candidate : candidates) {
        if (const std::string existing = existingPath(candidate); !existing.empty()) {
            return existing;
        }
    }
    return path;
}

bool isFontAwesomePath(const std::string& path) {
    return std::filesystem::path(path).filename().string().find("Font Awesome") != std::string::npos;
}

std::shared_ptr<std::vector<unsigned char>> loadSharedFontData(const std::string& fontPath) {
    static std::unordered_map<std::string, std::weak_ptr<std::vector<unsigned char>>> cache;

    if (auto cached = cache[fontPath].lock()) {
        return cached;
    }

    std::ifstream file(fontPath, std::ios::binary);
    if (!file) {
        return {};
    }

    auto data = std::make_shared<std::vector<unsigned char>>(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>());

    if (data->empty()) {
        return {};
    }

    cache[fontPath] = data;
    return data;
}

bool loadFontFace(const std::string& path, float fontSize, bool useSdf, FontFace& face) {
    face.path = path;
    face.useSdf = useSdf;
    face.data = loadSharedFontData(path);
    if (!face.data || face.data->empty()) {
        return false;
    }

    const int offset = stbtt_GetFontOffsetForIndex(face.data->data(), 0);
    if (!stbtt_InitFont(&face.info, face.data->data(), offset)) {
        return false;
    }

    face.scale = stbtt_ScaleForPixelHeight(&face.info, fontSize);
    int ascent = 0;
    int descent = 0;
    int lineGap = 0;
    stbtt_GetFontVMetrics(&face.info, &ascent, &descent, &lineGap);
    face.ascent = static_cast<float>(ascent) * face.scale;
    face.descent = static_cast<float>(descent) * face.scale;
    face.lineGap = static_cast<float>(lineGap) * face.scale;
    return true;
}

} // namespace

bool TextPrimitive::initialize() {
    const char* vertexSource =
        "#version 330 core\n"
        "layout(location = 0) in vec2 aPos;\n"
        "layout(location = 1) in vec2 aUv;\n"
        "layout(location = 2) in float aUseSdf;\n"
        "uniform vec2 uWindowSize;\n"
        "out vec2 vUv;\n"
        "out float vUseSdf;\n"
        "void main() {\n"
        "    vUv = aUv;\n"
        "    vUseSdf = aUseSdf;\n"
        "    vec2 ndc = vec2((aPos.x / uWindowSize.x) * 2.0 - 1.0,\n"
        "                    1.0 - (aPos.y / uWindowSize.y) * 2.0);\n"
        "    gl_Position = vec4(ndc, 0.0, 1.0);\n"
        "}\n";

    const char* fragmentSource =
        "#version 330 core\n"
        "in vec2 vUv;\n"
        "in float vUseSdf;\n"
        "out vec4 FragColor;\n"
        "uniform sampler2D uAtlas;\n"
        "uniform vec4 uColor;\n"
        "void main() {\n"
        "    float sdf = texture(uAtlas, vUv).r;\n"
        "    float width = fwidth(sdf);\n"
        "    float alpha = vUseSdf > 0.5 ? smoothstep(0.5 - width, 0.5 + width, sdf) : sdf;\n"
        "    if (alpha <= 0.0) discard;\n"
        "    FragColor = vec4(uColor.rgb, uColor.a * alpha);\n"
        "}\n";

    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (!vertexShader || !fragmentShader) {
        return false;
    }

    shaderProgram_ = glCreateProgram();
    glAttachShader(shaderProgram_, vertexShader);
    glAttachShader(shaderProgram_, fragmentShader);
    glLinkProgram(shaderProgram_);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLint linked = 0;
    glGetProgramiv(shaderProgram_, GL_LINK_STATUS, &linked);
    if (!linked) {
        glDeleteProgram(shaderProgram_);
        shaderProgram_ = 0;
        return false;
    }

    windowSizeLocation_ = glGetUniformLocation(shaderProgram_, "uWindowSize");
    colorLocation_ = glGetUniformLocation(shaderProgram_, "uColor");
    textureLocation_ = glGetUniformLocation(shaderProgram_, "uAtlas");

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 5, nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 5, reinterpret_cast<void*>(sizeof(float) * 2));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(float) * 5, reinterpret_cast<void*>(sizeof(float) * 4));
    glEnableVertexAttribArray(2);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    if (!loadFont() || !retainSharedTextAtlas()) {
        destroy();
        return false;
    }
    return true;
}

void TextPrimitive::destroy() {
    releaseSharedTextAtlas();
    if (vbo_) {
        glDeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }
    if (vao_) {
        glDeleteVertexArrays(1, &vao_);
        vao_ = 0;
    }
    if (shaderProgram_) {
        glDeleteProgram(shaderProgram_);
        shaderProgram_ = 0;
    }
    delete static_cast<FontInfoHolder*>(fontInfoStorage_);
    fontInfoStorage_ = nullptr;
}

void TextPrimitive::setPosition(float x, float y) {
    if (position_.x == x && position_.y == y) {
        return;
    }
    position_ = {x, y};
}

void TextPrimitive::setText(const std::string& text) {
    if (style_.text == text) {
        return;
    }
    style_.text = text;
    invalidateLayout();
}

void TextPrimitive::setFontFamily(const std::string& fontFamily) {
    if (style_.fontFamily == fontFamily) {
        return;
    }
    style_.fontFamily = fontFamily;
    fontDirty_ = true;
    invalidateLayout();
}

void TextPrimitive::setFontSize(float fontSize) {
    if (style_.fontSize == fontSize) {
        return;
    }
    style_.fontSize = fontSize;
    fontDirty_ = true;
    invalidateLayout();
}

void TextPrimitive::setFontWeight(int fontWeight) {
    if (style_.fontWeight == fontWeight) {
        return;
    }
    style_.fontWeight = fontWeight;
    fontDirty_ = true;
    invalidateLayout();
}

void TextPrimitive::setColor(const Color& color) {
    style_.color = color;
}

void TextPrimitive::setMaxWidth(float maxWidth) {
    if (style_.maxWidth == maxWidth) {
        return;
    }
    style_.maxWidth = maxWidth;
    invalidateLayout();
}

void TextPrimitive::setWrap(bool wrap) {
    if (style_.wrap == wrap) {
        return;
    }
    style_.wrap = wrap;
    invalidateLayout();
}

void TextPrimitive::setHorizontalAlign(HorizontalAlign align) {
    style_.horizontalAlign = align;
}

void TextPrimitive::setVerticalAlign(VerticalAlign align) {
    style_.verticalAlign = align;
}

void TextPrimitive::setLineHeight(float lineHeight) {
    if (style_.lineHeight == lineHeight) {
        return;
    }
    style_.lineHeight = lineHeight;
    invalidateLayout();
}

void TextPrimitive::setVisualScale(float originX, float originY, float scale) {
    visualScaleOrigin_ = {originX, originY};
    visualScale_ = std::max(0.01f, scale);
}

void TextPrimitive::setStyle(const TextStyle& style) {
    const bool fontChanged = style.fontFamily != style_.fontFamily ||
                             style.fontSize != style_.fontSize ||
                             style.fontWeight != style_.fontWeight;
    style_ = style;
    fontDirty_ = fontDirty_ || fontChanged;
    invalidateLayout();
}

const TextStyle& TextPrimitive::style() const {
    return style_;
}

Vec2 TextPrimitive::position() const {
    return position_;
}

Vec2 TextPrimitive::measuredSize() {
    if (layoutDirty_) {
        rebuildLayout();
    }
    return measuredSize_;
}

void TextPrimitive::render(int windowWidth, int windowHeight) {
    if (!shaderProgram_ || !vao_ || !vbo_) {
        return;
    }

    if (layoutDirty_) {
        rebuildLayout();
    }

    std::vector<float> vertices;
    const float lineHeight = style_.lineHeight > 0.0f ? style_.lineHeight : style_.fontSize * 1.2f;
    float blockYOffset = 0.0f;
    if (style_.verticalAlign == VerticalAlign::Center) {
        blockYOffset = -measuredSize_.y * 0.5f;
    } else if (style_.verticalAlign == VerticalAlign::Bottom) {
        blockYOffset = -measuredSize_.y;
    }

    for (size_t lineIndex = 0; lineIndex < lines_.size(); ++lineIndex) {
        const Line& line = lines_[lineIndex];
        float lineX = position_.x;
        if (style_.horizontalAlign == HorizontalAlign::Center) {
            lineX -= line.width * 0.5f;
        } else if (style_.horizontalAlign == HorizontalAlign::Right) {
            lineX -= line.width;
        }

        const float lineY = position_.y + blockYOffset + static_cast<float>(lineIndex) * lineHeight;
        for (const LaidOutGlyph& laidOut : line.glyphs) {
            const Glyph& glyph = laidOut.glyph;
            const float x0 = lineX + laidOut.x + glyph.xOffset;
            const float y0 = lineY + laidOut.y + glyph.yOffset;
            const float x1 = x0 + glyph.width;
            const float y1 = y0 + glyph.height;
            const float useSdf = glyph.useSdf ? 1.0f : 0.0f;
            Vec2 p0{x0, y0};
            Vec2 p1{x1, y0};
            Vec2 p2{x1, y1};
            Vec2 p3{x0, y1};

            if (std::fabs(visualScale_ - 1.0f) > 0.0001f) {
                auto scalePoint = [&](Vec2 point) {
                    return Vec2{
                        visualScaleOrigin_.x + (point.x - visualScaleOrigin_.x) * visualScale_,
                        visualScaleOrigin_.y + (point.y - visualScaleOrigin_.y) * visualScale_
                    };
                };
                p0 = scalePoint(p0);
                p1 = scalePoint(p1);
                p2 = scalePoint(p2);
                p3 = scalePoint(p3);
            }

            vertices.insert(vertices.end(), {
                p0.x, p0.y, glyph.u0, glyph.v0, useSdf,
                p1.x, p1.y, glyph.u1, glyph.v0, useSdf,
                p2.x, p2.y, glyph.u1, glyph.v1, useSdf,
                p0.x, p0.y, glyph.u0, glyph.v0, useSdf,
                p2.x, p2.y, glyph.u1, glyph.v1, useSdf,
                p3.x, p3.y, glyph.u0, glyph.v1, useSdf
            });
        }
    }

    if (vertices.empty()) {
        return;
    }

    const GLboolean blendEnabled = glIsEnabled(GL_BLEND);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(shaderProgram_);
    glUniform2f(windowSizeLocation_, static_cast<float>(windowWidth), static_cast<float>(windowHeight));
    glUniform4f(colorLocation_, style_.color.r, style_.color.g, style_.color.b, style_.color.a);
    glUniform1i(textureLocation_, 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sharedTextAtlas().texture);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(float)), vertices.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(vertices.size() / 5));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);

    if (!blendEnabled) {
        glDisable(GL_BLEND);
    }
}

bool TextPrimitive::loadFont() {
    const std::string fontPath = resolveFontPath(style_.fontFamily, style_.fontWeight);
    auto* holder = static_cast<FontInfoHolder*>(fontInfoStorage_);
    if (!holder) {
        holder = new FontInfoHolder();
        fontInfoStorage_ = holder;
    }

    holder->faces.clear();

    FontFace primary;
    if (!loadFontFace(fontPath, style_.fontSize, !isFontAwesomePath(fontPath), primary)) {
        return false;
    }

    fontData_ = primary.data;
    holder->faces.push_back(std::move(primary));

    const std::string fallbackPaths[] = {
        resolveFontPath("FontAwesome", 400),
        resolveFontPath("YouSheBiaoTiHei", 400),
#ifdef _WIN32
        resolveFontPath("Microsoft YaHei", 400)
#else
        resolveFontPath("", 400)
#endif
    };

    for (const std::string& fallbackPath : fallbackPaths) {
        if (fallbackPath.empty() || fallbackPath == fontPath) {
            continue;
        }

        FontFace fallback;
        if (loadFontFace(fallbackPath, style_.fontSize, !isFontAwesomePath(fallbackPath), fallback)) {
            holder->faces.push_back(std::move(fallback));
        }
    }

    scale_ = holder->faces.front().scale;
    ascent_ = holder->faces.front().ascent;
    descent_ = holder->faces.front().descent;
    lineGap_ = holder->faces.front().lineGap;

    glyphs_.clear();

    fontDirty_ = false;
    return true;
}

bool TextPrimitive::ensureGlyph(unsigned int codepoint) {
    if (glyphs_.find(codepoint) != glyphs_.end()) {
        return true;
    }

    if (fontDirty_ && !loadFont()) {
        return false;
    }

    auto* holder = static_cast<FontInfoHolder*>(fontInfoStorage_);
    if (!holder || holder->faces.empty()) {
        return false;
    }

    const FontFace* face = &holder->faces.front();
    if (codepoint != ' ' && codepoint != '\t') {
        for (const FontFace& candidate : holder->faces) {
            if (stbtt_FindGlyphIndex(&candidate.info, static_cast<int>(codepoint)) != 0) {
                face = &candidate;
                break;
            }
        }

        if (stbtt_FindGlyphIndex(&face->info, static_cast<int>(codepoint)) == 0) {
            Glyph missingGlyph;
            missingGlyph.advance = style_.fontSize * 0.5f;
            glyphs_[codepoint] = missingGlyph;
            return true;
        }
    }

    int advance = 0;
    int leftSideBearing = 0;
    stbtt_GetCodepointHMetrics(&face->info, static_cast<int>(codepoint), &advance, &leftSideBearing);

    Glyph glyph;
    glyph.advance = static_cast<float>(advance) * face->scale;
    glyph.useSdf = face->useSdf;

    if (codepoint == ' ' || codepoint == '\t') {
        glyphs_[codepoint] = glyph;
        return true;
    }

    const std::string cacheKey = glyphCacheKey(*face, style_.fontSize, codepoint);
    SharedTextAtlas& atlas = sharedTextAtlas();
    if (const auto cached = atlas.glyphs.find(cacheKey); cached != atlas.glyphs.end()) {
        glyphs_[codepoint] = cached->second;
        return true;
    }

    int width = 0;
    int height = 0;
    int xoff = 0;
    int yoff = 0;
    unsigned char* bitmap = face->useSdf
        ? stbtt_GetCodepointSDF(&face->info, face->scale, static_cast<int>(codepoint),
                                SdfPadding, 128, 32.0f, &width, &height, &xoff, &yoff)
        : stbtt_GetCodepointBitmap(&face->info, face->scale, face->scale, static_cast<int>(codepoint),
                                   &width, &height, &xoff, &yoff);
    if (!bitmap || width <= 0 || height <= 0) {
        if (bitmap && face->useSdf) {
            stbtt_FreeSDF(bitmap, nullptr);
        } else if (bitmap) {
            stbtt_FreeBitmap(bitmap, nullptr);
        }
        glyphs_[codepoint] = glyph;
        return true;
    }

    if (atlas.x + width + 1 >= atlas.width) {
        atlas.x = 1;
        atlas.y += atlas.rowHeight + 1;
        atlas.rowHeight = 0;
    }
    if (atlas.y + height + 1 >= atlas.height) {
        if (face->useSdf) {
            stbtt_FreeSDF(bitmap, nullptr);
        } else {
            stbtt_FreeBitmap(bitmap, nullptr);
        }
        return false;
    }

    glBindTexture(GL_TEXTURE_2D, atlas.texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, atlas.x, atlas.y, width, height, GL_RED, GL_UNSIGNED_BYTE, bitmap);
    glBindTexture(GL_TEXTURE_2D, 0);

    glyph.xOffset = static_cast<float>(xoff);
    glyph.yOffset = static_cast<float>(yoff) + face->ascent;
    glyph.width = static_cast<float>(width);
    glyph.height = static_cast<float>(height);
    glyph.u0 = static_cast<float>(atlas.x) / static_cast<float>(atlas.width);
    glyph.v0 = static_cast<float>(atlas.y) / static_cast<float>(atlas.height);
    glyph.u1 = static_cast<float>(atlas.x + width) / static_cast<float>(atlas.width);
    glyph.v1 = static_cast<float>(atlas.y + height) / static_cast<float>(atlas.height);
    atlas.glyphs[cacheKey] = glyph;
    glyphs_[codepoint] = glyph;

    atlas.x += width + 1;
    atlas.rowHeight = std::max(atlas.rowHeight, height);

    if (face->useSdf) {
        stbtt_FreeSDF(bitmap, nullptr);
    } else {
        stbtt_FreeBitmap(bitmap, nullptr);
    }
    return true;
}

void TextPrimitive::invalidateLayout() {
    layoutDirty_ = true;
}

void TextPrimitive::rebuildLayout() {
    if (fontDirty_ && !loadFont()) {
        layoutDirty_ = false;
        return;
    }

    lines_.clear();
    measuredSize_ = {};

    Line currentLine;
    float cursorX = 0.0f;
    const float lineHeight = style_.lineHeight > 0.0f ? style_.lineHeight : style_.fontSize * 1.2f;
    const float maxWidth = style_.maxWidth > 0.0f ? style_.maxWidth : 0.0f;

    const std::vector<unsigned int> codepoints = decodeUtf8(style_.text);
    for (unsigned int codepoint : codepoints) {
        if (codepoint == '\r') {
            continue;
        }
        if (codepoint == '\n') {
            measuredSize_.x = std::max(measuredSize_.x, currentLine.width);
            lines_.push_back(currentLine);
            currentLine = {};
            cursorX = 0.0f;
            continue;
        }

        const float advance = glyphAdvance(codepoint);
        if (style_.wrap && maxWidth > 0.0f && cursorX > 0.0f && cursorX + advance > maxWidth) {
            measuredSize_.x = std::max(measuredSize_.x, currentLine.width);
            lines_.push_back(currentLine);
            currentLine = {};
            cursorX = 0.0f;
        }

        appendCodepointToLine(currentLine, codepoint, cursorX);
    }

    measuredSize_.x = std::max(measuredSize_.x, currentLine.width);
    lines_.push_back(currentLine);
    measuredSize_.y = lines_.empty() ? 0.0f : static_cast<float>(lines_.size()) * lineHeight;
    layoutDirty_ = false;
}

std::vector<unsigned int> TextPrimitive::decodeUtf8(const std::string& text) const {
    std::vector<unsigned int> codepoints;
    size_t index = 0;
    while (index < text.size()) {
        codepoints.push_back(readCodepoint(text, index));
    }
    return codepoints;
}

float TextPrimitive::glyphAdvance(unsigned int codepoint) {
    if (!ensureGlyph(codepoint)) {
        return style_.fontSize * 0.5f;
    }
    return glyphs_[codepoint].advance;
}

void TextPrimitive::appendCodepointToLine(Line& line, unsigned int codepoint, float& cursorX) {
    if (!ensureGlyph(codepoint)) {
        return;
    }

    const Glyph& glyph = glyphs_[codepoint];
    if (codepoint != ' ' && codepoint != '\t') {
        line.glyphs.push_back({glyph, cursorX, 0.0f});
    }
    cursorX += glyph.advance;
    line.width = cursorX;
}

unsigned int TextPrimitive::readCodepoint(const std::string& text, size_t& index) {
    const unsigned char first = static_cast<unsigned char>(text[index++]);
    if (first < 0x80) {
        return first;
    }
    if ((first >> 5) == 0x6 && index < text.size()) {
        return ((first & 0x1F) << 6) | (static_cast<unsigned char>(text[index++]) & 0x3F);
    }
    if ((first >> 4) == 0xE && index + 1 < text.size()) {
        unsigned int cp = (first & 0x0F) << 12;
        cp |= (static_cast<unsigned char>(text[index++]) & 0x3F) << 6;
        cp |= static_cast<unsigned char>(text[index++]) & 0x3F;
        return cp;
    }
    if ((first >> 3) == 0x1E && index + 2 < text.size()) {
        unsigned int cp = (first & 0x07) << 18;
        cp |= (static_cast<unsigned char>(text[index++]) & 0x3F) << 12;
        cp |= (static_cast<unsigned char>(text[index++]) & 0x3F) << 6;
        cp |= static_cast<unsigned char>(text[index++]) & 0x3F;
        return cp;
    }
    return '?';
}

std::string TextPrimitive::resolveFontPath(const std::string& fontFamily, int fontWeight) {
    if (!fontFamily.empty() && fontFamily.find('.') != std::string::npos) {
        return resolveFontFilePath(fontFamily);
    }

    if (fontFamily == "YouSheBiaoTiHei" || fontFamily == "YouShe" || fontFamily == "Title") {
        return resolveProjectAssetPath("YouSheBiaoTiHei-2.ttf");
    }

    if (fontFamily == "FontAwesome" || fontFamily == "Font Awesome" ||
        fontFamily == "Font Awesome 7 Free" || fontFamily == "Icon") {
        return resolveProjectAssetPath("Font Awesome 7 Free-Solid-900.otf");
    }

#ifdef _WIN32
    if (fontFamily == "Microsoft YaHei" || fontFamily == "YaHei") {
        return "C:/Windows/Fonts/msyh.ttc";
    }
    if (fontFamily == "SimHei") {
        return "C:/Windows/Fonts/simhei.ttf";
    }
    if (fontWeight >= 600) {
        return "C:/Windows/Fonts/segoeuib.ttf";
    }
    return "C:/Windows/Fonts/segoeui.ttf";
#else
    (void)fontWeight;
    return "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
#endif
}

GLuint TextPrimitive::compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

} // namespace core
