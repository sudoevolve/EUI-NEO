#pragma once

#include "core/primitive.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace core {

enum class HorizontalAlign {
    Left,
    Center,
    Right
};

enum class VerticalAlign {
    Top,
    Center,
    Bottom
};

struct TextStyle {
    std::string text;
    std::string fontFamily;
    float fontSize = 16.0f;
    int fontWeight = 400;
    Color color = {1.0f, 1.0f, 1.0f, 1.0f};
    float maxWidth = 0.0f;
    bool wrap = false;
    HorizontalAlign horizontalAlign = HorizontalAlign::Left;
    VerticalAlign verticalAlign = VerticalAlign::Top;
    float lineHeight = 0.0f;
};

class TextPrimitive {
public:
    struct Glyph {
        float advance = 0.0f;
        float xOffset = 0.0f;
        float yOffset = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
        float u0 = 0.0f;
        float v0 = 0.0f;
        float u1 = 0.0f;
        float v1 = 0.0f;
        bool useSdf = true;
    };

    TextPrimitive() = default;
    TextPrimitive(float x, float y) : position_{x, y} {}

    bool initialize();
    void destroy();

    void setPosition(float x, float y);
    void setText(const std::string& text);
    void setFontFamily(const std::string& fontFamily);
    void setFontSize(float fontSize);
    void setFontWeight(int fontWeight);
    void setColor(const Color& color);
    void setMaxWidth(float maxWidth);
    void setWrap(bool wrap);
    void setHorizontalAlign(HorizontalAlign align);
    void setVerticalAlign(VerticalAlign align);
    void setLineHeight(float lineHeight);
    void setStyle(const TextStyle& style);
    void setVisualScale(float originX, float originY, float scale);

    const TextStyle& style() const;
    Vec2 position() const;
    Vec2 measuredSize();

    void render(int windowWidth, int windowHeight);

private:
    struct LaidOutGlyph {
        Glyph glyph;
        float x = 0.0f;
        float y = 0.0f;
    };

    struct Line {
        std::vector<LaidOutGlyph> glyphs;
        float width = 0.0f;
    };

    bool loadFont();
    bool ensureGlyph(unsigned int codepoint);
    void invalidateLayout();
    void rebuildLayout();
    std::vector<unsigned int> decodeUtf8(const std::string& text) const;
    float glyphAdvance(unsigned int codepoint);
    void appendCodepointToLine(Line& line, unsigned int codepoint, float& cursorX);

    static unsigned int readCodepoint(const std::string& text, size_t& index);
    static std::string resolveFontPath(const std::string& fontFamily, int fontWeight);
    static GLuint compileShader(GLenum type, const char* source);

    Vec2 position_;
    Vec2 visualScaleOrigin_;
    float visualScale_ = 1.0f;
    TextStyle style_;
    std::shared_ptr<std::vector<unsigned char>> fontData_;
    void* fontInfoStorage_ = nullptr;
    float scale_ = 1.0f;
    float ascent_ = 0.0f;
    float descent_ = 0.0f;
    float lineGap_ = 0.0f;

    std::unordered_map<unsigned int, Glyph> glyphs_;

    std::vector<Line> lines_;
    Vec2 measuredSize_;
    bool layoutDirty_ = true;
    bool fontDirty_ = true;

    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint shaderProgram_ = 0;
    GLint windowSizeLocation_ = -1;
    GLint colorLocation_ = -1;
    GLint textureLocation_ = -1;
};

} // namespace core
