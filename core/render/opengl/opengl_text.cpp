#include "core/render/opengl/opengl_backend.h"

#include "core/window/window_backend.h"

#include <glad/glad.h>

#include <algorithm>
#include <cstdint>
#include <unordered_map>

namespace core::render::opengl {

namespace {

struct TextAtlasTexture {
    GLuint texture = 0;
    int width = 0;
    int height = 0;
    int channels = 0;
    std::uint64_t generation = 0;
};

struct TextRenderResources {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint shaderProgram = 0;
    GLint windowSizeLocation = -1;
    GLint colorLocation = -1;
    GLint grayTextureLocation = -1;
    GLint colorTextureLocation = -1;
    TextAtlasTexture gray;
    TextAtlasTexture color;
};

std::unordered_map<window::ContextKey, TextRenderResources>& textResourcesByContext() {
    static std::unordered_map<window::ContextKey, TextRenderResources> resourcesByContext;
    return resourcesByContext;
}

TextRenderResources& textResources() {
    return textResourcesByContext()[window::currentContextKey()];
}

GLuint compileShader(GLenum type, const char* source) {
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

void destroyAtlasTexture(TextAtlasTexture& atlas) {
    if (atlas.texture != 0) {
        glDeleteTextures(1, &atlas.texture);
    }
    atlas = {};
}

void destroyTextRenderResources(TextRenderResources& resources) {
    if (resources.vbo != 0) {
        glDeleteBuffers(1, &resources.vbo);
    }
    if (resources.vao != 0) {
        glDeleteVertexArrays(1, &resources.vao);
    }
    if (resources.shaderProgram != 0) {
        glDeleteProgram(resources.shaderProgram);
    }
    destroyAtlasTexture(resources.gray);
    destroyAtlasTexture(resources.color);
    resources = {};
}

bool ensureTextRenderResources(TextRenderResources& resources) {
    if (resources.shaderProgram != 0 && resources.vao != 0 && resources.vbo != 0) {
        return true;
    }

    const char* vertexSource =
        "#version 330 core\n"
        "layout(location = 0) in vec2 aPos;\n"
        "layout(location = 1) in vec2 aUv;\n"
        "layout(location = 2) in float aColored;\n"
        "uniform vec2 uWindowSize;\n"
        "out vec2 vUv;\n"
        "out float vColored;\n"
        "void main() {\n"
        "    vUv = aUv;\n"
        "    vColored = aColored;\n"
        "    vec2 ndc = vec2((aPos.x / uWindowSize.x) * 2.0 - 1.0,\n"
        "                    1.0 - (aPos.y / uWindowSize.y) * 2.0);\n"
        "    gl_Position = vec4(ndc, 0.0, 1.0);\n"
        "}\n";

    const char* fragmentSource =
        "#version 330 core\n"
        "in vec2 vUv;\n"
        "in float vColored;\n"
        "out vec4 FragColor;\n"
        "uniform sampler2D uGrayAtlas;\n"
        "uniform sampler2D uColorAtlas;\n"
        "uniform vec4 uColor;\n"
        "void main() {\n"
        "    if (vColored > 0.5) {\n"
        "        vec4 sampleColor = texture(uColorAtlas, vUv);\n"
        "        if (sampleColor.a <= 0.0) discard;\n"
        "        FragColor = sampleColor * uColor.a;\n"
        "    } else {\n"
        "        float alpha = texture(uGrayAtlas, vUv).r;\n"
        "        if (alpha <= 0.0) discard;\n"
        "        FragColor = vec4(uColor.rgb, uColor.a * alpha);\n"
        "    }\n"
        "}\n";

    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (vertexShader == 0 || fragmentShader == 0) {
        if (vertexShader != 0) {
            glDeleteShader(vertexShader);
        }
        if (fragmentShader != 0) {
            glDeleteShader(fragmentShader);
        }
        return false;
    }

    resources.shaderProgram = glCreateProgram();
    glAttachShader(resources.shaderProgram, vertexShader);
    glAttachShader(resources.shaderProgram, fragmentShader);
    glLinkProgram(resources.shaderProgram);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLint linked = 0;
    glGetProgramiv(resources.shaderProgram, GL_LINK_STATUS, &linked);
    if (!linked) {
        glDeleteProgram(resources.shaderProgram);
        resources.shaderProgram = 0;
        return false;
    }

    resources.windowSizeLocation = glGetUniformLocation(resources.shaderProgram, "uWindowSize");
    resources.colorLocation = glGetUniformLocation(resources.shaderProgram, "uColor");
    resources.grayTextureLocation = glGetUniformLocation(resources.shaderProgram, "uGrayAtlas");
    resources.colorTextureLocation = glGetUniformLocation(resources.shaderProgram, "uColorAtlas");

    glGenVertexArrays(1, &resources.vao);
    glGenBuffers(1, &resources.vbo);
    glBindVertexArray(resources.vao);
    glBindBuffer(GL_ARRAY_BUFFER, resources.vbo);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 5, nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 5, reinterpret_cast<void*>(sizeof(float) * 2));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(float) * 5, reinterpret_cast<void*>(sizeof(float) * 4));
    glEnableVertexAttribArray(2);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return resources.shaderProgram != 0 && resources.vao != 0 && resources.vbo != 0;
}

bool ensureAtlasTexture(TextAtlasTexture& texture, const TextAtlasPageData& page) {
    if (page.pixels == nullptr || page.width <= 0 || page.height <= 0 || page.channels <= 0) {
        return false;
    }

    const bool recreate = texture.texture == 0 ||
                          texture.width != page.width ||
                          texture.height != page.height ||
                          texture.channels != page.channels;
    if (!recreate && texture.generation == page.generation) {
        return true;
    }

    if (recreate) {
        destroyAtlasTexture(texture);
        glGenTextures(1, &texture.texture);
        texture.width = page.width;
        texture.height = page.height;
        texture.channels = page.channels;
    }

    glBindTexture(GL_TEXTURE_2D, texture.texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (page.channels == 4) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, page.width, page.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, page.pixels);
    } else {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, page.width, page.height, 0, GL_RED, GL_UNSIGNED_BYTE, page.pixels);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    texture.generation = page.generation;
    return texture.texture != 0;
}

} // namespace

void OpenGLRenderBackend::drawText(const TextDrawCommand& command, int windowWidth, int windowHeight) {
    if (command.vertices == nullptr || command.vertexFloatCount == 0 || windowWidth <= 0 || windowHeight <= 0) {
        return;
    }
    flushRoundedRectBatch();

    TextRenderResources& resources = textResources();
    const bool hadResources = resources.shaderProgram != 0 && resources.vao != 0 && resources.vbo != 0;
    const bool grayAtlasUpload =
        command.grayAtlas.pixels != nullptr &&
        command.grayAtlas.width > 0 &&
        command.grayAtlas.height > 0 &&
        command.grayAtlas.channels > 0 &&
        (resources.gray.texture == 0 ||
         resources.gray.width != command.grayAtlas.width ||
         resources.gray.height != command.grayAtlas.height ||
         resources.gray.channels != command.grayAtlas.channels ||
         resources.gray.generation != command.grayAtlas.generation);
    const bool colorAtlasUpload =
        command.colorAtlas.pixels != nullptr &&
        command.colorAtlas.width > 0 &&
        command.colorAtlas.height > 0 &&
        command.colorAtlas.channels > 0 &&
        (resources.color.texture == 0 ||
         resources.color.width != command.colorAtlas.width ||
         resources.color.height != command.colorAtlas.height ||
         resources.color.channels != command.colorAtlas.channels ||
         resources.color.generation != command.colorAtlas.generation);
    if (!ensureTextRenderResources(resources) ||
        !ensureAtlasTexture(resources.gray, command.grayAtlas)) {
        return;
    }
    if (command.colorAtlas.pixels != nullptr && command.colorAtlas.width > 0 && command.colorAtlas.height > 0) {
        ensureAtlasTexture(resources.color, command.colorAtlas);
    }
    if (!hadResources || grayAtlasUpload || colorAtlasUpload) {
        resetStateCache();
    }

    setStandardAlphaBlend();

    useProgram(resources.shaderProgram);
    glUniform2f(resources.windowSizeLocation, static_cast<float>(windowWidth), static_cast<float>(windowHeight));
    glUniform4f(resources.colorLocation, command.color.r, command.color.g, command.color.b, command.color.a);
    glUniform1i(resources.grayTextureLocation, 0);
    glUniform1i(resources.colorTextureLocation, 1);

    activeTextureUnit(0);
    bindTexture2D(resources.gray.texture);
    activeTextureUnit(1);
    bindTexture2D(resources.color.texture != 0 ? resources.color.texture : resources.gray.texture);
    activeTextureUnit(0);

    bindVertexArray(resources.vao);
    bindArrayBuffer(resources.vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(command.vertexFloatCount * sizeof(float)),
                 command.vertices,
                 GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(command.vertexFloatCount / 5));
}

void OpenGLRenderBackend::releaseTextResources() {
    flushRoundedRectBatch();
    auto& resourcesByContext = textResourcesByContext();
    const auto current = window::currentContextKey();
    const auto item = resourcesByContext.find(current);
    if (item != resourcesByContext.end()) {
        destroyTextRenderResources(item->second);
        resourcesByContext.erase(item);
    }
    resetStateCache();
}

} // namespace core::render::opengl
