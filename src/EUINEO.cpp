#include "EUINEO.h"
#include <algorithm>
#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

namespace EUINEO {

// --- 工具与数学 ---
Color Lerp(const Color& a, const Color& b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return Color(
        a.r + (b.r - a.r) * t,
        a.g + (b.g - a.g) * t,
        a.b + (b.b - a.b) * t,
        a.a + (b.a - a.a) * t
    );
}
float Lerp(float a, float b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return a + (b - a) * t;
}

// --- Theme Tokens ---
Theme LightTheme = {
    Color(0.95f, 0.95f, 0.97f), // bg
    Color(0.2f, 0.5f, 0.9f),    // primary (Blue)
    Color(1.0f, 1.0f, 1.0f),    // surface
    Color(0.9f, 0.9f, 0.9f),    // surfaceHover
    Color(0.8f, 0.8f, 0.8f),    // surfaceActive
    Color(0.0f, 0.0f, 0.0f),    // text (black)
    Color(0.8f, 0.8f, 0.8f)     // border
};

Theme DarkTheme = {
    Color(0.1f, 0.1f, 0.12f),   // bg
    Color(0.3f, 0.6f, 1.0f),    // primary (Light Blue)
    Color(0.15f, 0.15f, 0.18f), // surface
    Color(0.25f, 0.25f, 0.28f), // surfaceHover
    Color(0.35f, 0.35f, 0.38f), // surfaceActive
    Color(1.0f, 1.0f, 1.0f),    // text (white)
    Color(0.3f, 0.3f, 0.3f)     // border
};

Theme* CurrentTheme = &DarkTheme;
UIState State;

// --- 渲染器 (极简正交投影 + 单纯色 Shader) ---
static GLuint VAO, VBO, ShaderProgram;
static GLint ProjLoc, ColorLoc, PosLoc, SizeLoc, RoundingLoc;

static const char* vShaderStr = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
out vec2 vPos;
uniform mat4 projection;
uniform vec2 uPos;
uniform vec2 uSize;
void main() {
    vec2 pos = (aPos * uSize) + uPos;
    vPos = pos;
    gl_Position = projection * vec4(pos, 0.0, 1.0);
}
)";

static const char* fShaderStr = R"(
#version 330 core
in vec2 vPos;
uniform vec4 uColor;
uniform vec2 uPos;
uniform vec2 uSize;
uniform float uRounding;
out vec4 FragColor;

float roundedBoxSDF(vec2 CenterPosition, vec2 Size, float Radius) {
    return length(max(abs(CenterPosition)-Size+Radius,0.0))-Radius;
}

void main() {
    if (uRounding > 0.0) {
        vec2 center = uPos + uSize * 0.5;
        vec2 p = vPos - center;
        float d = roundedBoxSDF(p, uSize * 0.5, uRounding);
        
        // 抗锯齿边缘
        float alpha = 1.0 - smoothstep(-1.0, 1.0, d);
        FragColor = vec4(uColor.rgb, uColor.a * alpha);
    } else {
        FragColor = uColor;
    }
}
)";

static GLuint CompileShader(GLenum type, const char* source) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &source, nullptr);
    glCompileShader(s);
    return s;
}

// --- 字体渲染相关变量 ---
struct Character {
    GLuint TextureID;
    int Size[2];
    int Bearing[2];
    unsigned int Advance;
};
static std::unordered_map<unsigned int, Character> Characters; // 支持 Unicode
static GLuint TextVAO, TextVBO;
static GLuint TextShaderProgram;
static GLint TextProjLoc, TextColorLoc;

static const char* textVShaderStr = R"(
#version 330 core
layout(location = 0) in vec4 vertex; // <vec2 pos, vec2 tex>
out vec2 TexCoords;
uniform mat4 projection;
void main() {
    gl_Position = projection * vec4(vertex.xy, 0.0, 1.0);
    TexCoords = vertex.zw;
}
)";

static const char* textFShaderStr = R"(
#version 330 core
in vec2 TexCoords;
out vec4 color;
uniform sampler2D text;
uniform vec4 textColor;
void main() {
    vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, TexCoords).r);
    color = textColor * sampled;
}
)";

void Renderer::Init() {
    // ... 前面的代码不变
    GLuint vs = CompileShader(GL_VERTEX_SHADER, vShaderStr);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fShaderStr);
    ShaderProgram = glCreateProgram();
    glAttachShader(ShaderProgram, vs);
    glAttachShader(ShaderProgram, fs);
    glLinkProgram(ShaderProgram);
    glDeleteShader(vs);
    glDeleteShader(fs);

    ProjLoc = glGetUniformLocation(ShaderProgram, "projection");
    ColorLoc = glGetUniformLocation(ShaderProgram, "uColor");
    PosLoc = glGetUniformLocation(ShaderProgram, "uPos");
    SizeLoc = glGetUniformLocation(ShaderProgram, "uSize");
    RoundingLoc = glGetUniformLocation(ShaderProgram, "uRounding");

    float vertices[] = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 1.0f
    };
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // 初始化文本着色器
    GLuint tvs = CompileShader(GL_VERTEX_SHADER, textVShaderStr);
    GLuint tfs = CompileShader(GL_FRAGMENT_SHADER, textFShaderStr);
    TextShaderProgram = glCreateProgram();
    glAttachShader(TextShaderProgram, tvs);
    glAttachShader(TextShaderProgram, tfs);
    glLinkProgram(TextShaderProgram);
    glDeleteShader(tvs);
    glDeleteShader(tfs);
    
    TextProjLoc = glGetUniformLocation(TextShaderProgram, "projection");
    TextColorLoc = glGetUniformLocation(TextShaderProgram, "textColor");

    glGenVertexArrays(1, &TextVAO);
    glGenBuffers(1, &TextVBO);
    glBindVertexArray(TextVAO);
    glBindBuffer(GL_ARRAY_BUFFER, TextVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void Renderer::Shutdown() {
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(ShaderProgram);

    glDeleteVertexArrays(1, &TextVAO);
    glDeleteBuffers(1, &TextVBO);
    glDeleteProgram(TextShaderProgram);
}

void Renderer::BeginFrame() {
    // 基础 Shader 设置正交投影矩阵 (左, 右, 下, 上)
    // 映射到屏幕像素坐标 (0,0) 在左上角
    float L = 0.0f, R = State.screenW, B = State.screenH, T = 0.0f;
    float proj[16] = {
        2.0f/(R-L), 0, 0, 0,
        0, 2.0f/(T-B), 0, 0,
        0, 0, -1, 0,
        -(R+L)/(R-L), -(T+B)/(T-B), 0, 1
    };

    glUseProgram(ShaderProgram);
    glUniformMatrix4fv(ProjLoc, 1, GL_FALSE, proj);
    
    // 文本 Shader 设置正交投影矩阵
    glUseProgram(TextShaderProgram);
    glUniformMatrix4fv(TextProjLoc, 1, GL_FALSE, proj);
    
    // 开启混合支持透明度和字体渲染
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
}

void Renderer::DrawRect(float x, float y, float w, float h, const Color& color, float rounding) {
    glUseProgram(ShaderProgram);
    glUniform2f(PosLoc, x, y);
    glUniform2f(SizeLoc, w, h);
    glUniform4f(ColorLoc, color.r, color.g, color.b, color.a);
    glUniform1f(RoundingLoc, rounding);
    
    // 如果有圆角，需要启用混合
    if (rounding > 0.0f) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

bool Renderer::LoadFont(const std::string& fontPath, float fontSize, unsigned int startChar, unsigned int endChar) {
    std::ifstream file(fontPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<unsigned char> buffer(size);
    if (!file.read((char*)buffer.data(), size)) return false;

    stbtt_fontinfo font;
    if (!stbtt_InitFont(&font, buffer.data(), 0)) return false;

    float scale = stbtt_ScaleForPixelHeight(&font, fontSize);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    
    // 创建独立纹理，释放位图内存，确保只加载需要的字形，最大程度节省内存
    for (unsigned int c = startChar; c < endChar; c++) {
        // 如果该字符已经加载过，跳过
        if (Characters.find(c) != Characters.end()) continue;
        
        // 检查字体是否包含该字形，如果不包含则跳过，避免生成无用的空白纹理
        if (stbtt_FindGlyphIndex(&font, c) == 0) continue;

        int width, height, xoff, yoff;
        unsigned char* bitmap = stbtt_GetCodepointBitmap(&font, 0, scale, c, &width, &height, &xoff, &yoff);
        if (!bitmap) continue;

        int advance, lsb;
        stbtt_GetCodepointHMetrics(&font, c, &advance, &lsb);

        GLuint texture;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, width, height, 0, GL_RED, GL_UNSIGNED_BYTE, bitmap);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        Character character = {
            texture,
            {width, height},
            {xoff, yoff},
            (unsigned int)(advance * scale)
        };
        Characters[c] = character;
        stbtt_FreeBitmap(bitmap, nullptr);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    return true;
}

// 简易 UTF-8 解码器
static std::vector<unsigned int> DecodeUTF8(const std::string& text) {
    std::vector<unsigned int> codepoints;
    size_t i = 0;
    while (i < text.length()) {
        unsigned char c = text[i];
        if (c <= 0x7F) {
            codepoints.push_back(c);
            i += 1;
        } else if ((c & 0xE0) == 0xC0) {
            if (i + 1 < text.length()) {
                codepoints.push_back(((c & 0x1F) << 6) | (text[i+1] & 0x3F));
                i += 2;
            } else break;
        } else if ((c & 0xF0) == 0xE0) {
            if (i + 2 < text.length()) {
                codepoints.push_back(((c & 0x0F) << 12) | ((text[i+1] & 0x3F) << 6) | (text[i+2] & 0x3F));
                i += 3;
            } else break;
        } else if ((c & 0xF8) == 0xF0) {
            if (i + 3 < text.length()) {
                codepoints.push_back(((c & 0x07) << 18) | ((text[i+1] & 0x3F) << 12) | ((text[i+2] & 0x3F) << 6) | (text[i+3] & 0x3F));
                i += 4;
            } else break;
        } else {
            i += 1;
        }
    }
    return codepoints;
}

void Renderer::DrawTextStr(const std::string& text, float x, float y, const Color& color, float scale) {
    glUseProgram(TextShaderProgram);
    glUniform4f(TextColorLoc, color.r, color.g, color.b, color.a);
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(TextVAO);
    
    // 启用混合以正确显示字体的 alpha 通道
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    std::vector<unsigned int> codepoints = DecodeUTF8(text);
    for (unsigned int c : codepoints) {
        // 特殊处理空格字符，如果空格没有加载到位图中，我们可以手动增加它的步进
        if (c == ' ') {
            // 一个简单的经验值：空格宽度大约是字体高度的 0.3 倍
            x += 24.0f * 0.3f * scale; // 假设基础 fontSize 是 24.0f
            continue;
        }

        if (Characters.find(c) == Characters.end()) continue;
        Character ch = Characters[c];

        float xpos = x + ch.Bearing[0] * scale;
        float ypos = y + ch.Bearing[1] * scale;

        float w = ch.Size[0] * scale;
        float h = ch.Size[1] * scale;

        float vertices[6][4] = {
            { xpos,     ypos + h,   0.0f, 1.0f }, 
            { xpos,     ypos,       0.0f, 0.0f }, 
            { xpos + w, ypos,       1.0f, 0.0f }, 

            { xpos,     ypos + h,   0.0f, 1.0f }, 
            { xpos + w, ypos,       1.0f, 0.0f }, 
            { xpos + w, ypos + h,   1.0f, 1.0f }  
        };

        glBindTexture(GL_TEXTURE_2D, ch.TextureID);
        glBindBuffer(GL_ARRAY_BUFFER, TextVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glDrawArrays(GL_TRIANGLES, 0, 6);
        x += ch.Advance * scale;
    }
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

float Renderer::MeasureTextWidth(const std::string& text, float scale) {
    float width = 0;
    std::vector<unsigned int> codepoints = DecodeUTF8(text);
    for (unsigned int c : codepoints) {
        if (Characters.find(c) != Characters.end()) {
            width += Characters[c].Advance * scale;
        }
    }
    return width;
}

void Renderer::RequestRepaint(float duration) {
    State.needsRepaint = true;
    if (duration > State.animationTimeLeft) {
        State.animationTimeLeft = duration;
    }
}

bool Renderer::ShouldRepaint() {
    if (State.needsRepaint) {
        State.needsRepaint = false;
        return true;
    }
    if (State.animationTimeLeft > 0) {
        State.animationTimeLeft -= State.deltaTime;
        return true;
    }
    return false;
}

// --- Widget 基础类 ---
void Widget::GetAbsoluteBounds(float& outX, float& outY) {
    outX = x; outY = y;
    // 简易锚点计算
    switch(anchor) {
        case Anchor::TopCenter: outX += State.screenW/2 - width/2; break;
        case Anchor::TopRight: outX += State.screenW - width; break;
        case Anchor::CenterLeft: outY += State.screenH/2 - height/2; break;
        case Anchor::Center: outX += State.screenW/2 - width/2; outY += State.screenH/2 - height/2; break;
        case Anchor::CenterRight: outX += State.screenW - width; outY += State.screenH/2 - height/2; break;
        case Anchor::BottomLeft: outY += State.screenH - height; break;
        case Anchor::BottomCenter: outX += State.screenW/2 - width/2; outY += State.screenH - height; break;
        case Anchor::BottomRight: outX += State.screenW - width; outY += State.screenH - height; break;
        case Anchor::TopLeft: default: break;
    }
}

bool Widget::IsHovered() {
    float absX, absY;
    GetAbsoluteBounds(absX, absY);
    return State.mouseX >= absX && State.mouseX <= absX + width &&
           State.mouseY >= absY && State.mouseY <= absY + height;
}

} // namespace EUINEO