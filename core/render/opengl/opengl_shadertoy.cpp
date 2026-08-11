#include "core/render/opengl/opengl_backend.h"

#include "core/render/image_source.h"

#include <glad/glad.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

namespace core::render::opengl {
namespace {

constexpr std::uint64_t kInvalidFrameToken = std::numeric_limits<std::uint64_t>::max();

struct ShaderToyTextureHeader {
    GLuint texture = 0;
};

struct ShaderToyImage {
    GLuint texture = 0;
    int width = 1;
    int height = 1;
};

struct ShaderToyPassResource {
    GLuint program = 0;
    std::array<GLuint, 2> textures{};
    std::array<GLuint, 2> framebuffers{};
    std::filesystem::file_time_type sourceTime{};
    bool sourceTimeValid = false;
    int width = 0;
    int height = 0;
};

struct OpenGLShaderToy {
    std::vector<ShaderToyPassResource> passes;
    std::unordered_map<std::string, ShaderToyImage> images;
    ShaderToyImage empty;
    ShaderToyTextureHeader output;
    GLuint vao = 0;
    int width = 0;
    int height = 0;
    int currentIndex = 0;
    bool hasOutput = false;
    std::uint64_t lastFrameToken = kInvalidFrameToken;
    ShaderToyError hotReloadError;
};

struct OpenGLStateScope {
    GLint drawFramebuffer = 0;
    GLint readFramebuffer = 0;
    GLint viewport[4]{};
    GLint scissorBox[4]{};
    GLint program = 0;
    GLint vao = 0;
    GLint arrayBuffer = 0;
    GLint activeTexture = 0;
    GLint texture2DBindings[4]{};
    GLint samplerBindings[4]{};
    GLint blendSrcRgb = 0;
    GLint blendDstRgb = 0;
    GLint blendSrcAlpha = 0;
    GLint blendDstAlpha = 0;
    GLfloat clearColor[4]{};
    GLboolean scissor = GL_FALSE;
    GLboolean blend = GL_FALSE;
    GLboolean framebufferSrgb = GL_FALSE;

    OpenGLStateScope() {
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer);
        glGetIntegerv(GL_VIEWPORT, viewport);
        glGetIntegerv(GL_SCISSOR_BOX, scissorBox);
        glGetIntegerv(GL_CURRENT_PROGRAM, &program);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &arrayBuffer);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
        glGetIntegerv(GL_BLEND_SRC_RGB, &blendSrcRgb);
        glGetIntegerv(GL_BLEND_DST_RGB, &blendDstRgb);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrcAlpha);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDstAlpha);
        glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColor);
        scissor = glIsEnabled(GL_SCISSOR_TEST);
        blend = glIsEnabled(GL_BLEND);
        framebufferSrgb = glIsEnabled(GL_FRAMEBUFFER_SRGB);
        for (int unit = 0; unit < 4; ++unit) {
            glActiveTexture(GL_TEXTURE0 + unit);
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture2DBindings[unit]);
            glGetIntegerv(GL_SAMPLER_BINDING, &samplerBindings[unit]);
        }
        glActiveTexture(activeTexture);
    }

    void discardBindings(const OpenGLShaderToy& toy) {
        if (vao == static_cast<GLint>(toy.vao)) vao = 0;
        if (drawFramebuffer != 0 || readFramebuffer != 0 || program != 0) {
            for (const ShaderToyPassResource& pass : toy.passes) {
                if (drawFramebuffer == static_cast<GLint>(pass.framebuffers[0]) ||
                    drawFramebuffer == static_cast<GLint>(pass.framebuffers[1])) {
                    drawFramebuffer = 0;
                }
                if (readFramebuffer == static_cast<GLint>(pass.framebuffers[0]) ||
                    readFramebuffer == static_cast<GLint>(pass.framebuffers[1])) {
                    readFramebuffer = 0;
                }
                if (program == static_cast<GLint>(pass.program)) program = 0;
            }
        }
        for (GLint& binding : texture2DBindings) {
            if (binding == static_cast<GLint>(toy.empty.texture)) binding = 0;
            for (const ShaderToyPassResource& pass : toy.passes) {
                if (binding == static_cast<GLint>(pass.textures[0]) ||
                    binding == static_cast<GLint>(pass.textures[1])) {
                    binding = 0;
                }
            }
            for (const auto& entry : toy.images) {
                if (binding == static_cast<GLint>(entry.second.texture)) binding = 0;
            }
        }
    }

    ~OpenGLStateScope() {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(drawFramebuffer));
        glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(readFramebuffer));
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        glScissor(scissorBox[0], scissorBox[1], scissorBox[2], scissorBox[3]);
        scissor ? glEnable(GL_SCISSOR_TEST) : glDisable(GL_SCISSOR_TEST);
        blend ? glEnable(GL_BLEND) : glDisable(GL_BLEND);
        framebufferSrgb ? glEnable(GL_FRAMEBUFFER_SRGB) : glDisable(GL_FRAMEBUFFER_SRGB);
        glBlendFuncSeparate(blendSrcRgb, blendDstRgb, blendSrcAlpha, blendDstAlpha);
        glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
        glUseProgram(static_cast<GLuint>(program));
        glBindVertexArray(static_cast<GLuint>(vao));
        glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(arrayBuffer));
        for (int unit = 0; unit < 4; ++unit) {
            glActiveTexture(GL_TEXTURE0 + unit);
            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture2DBindings[unit]));
            glBindSampler(static_cast<GLuint>(unit),
                          static_cast<GLuint>(samplerBindings[unit]));
        }
        glActiveTexture(activeTexture);
    }
};

const char* kVertexShader = R"GLSL(#version 330 core
out vec2 euiTexCoord;
const vec2 euiPositions[6] = vec2[6](
    vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(1.0, 1.0),
    vec2(-1.0, -1.0), vec2(1.0, 1.0), vec2(-1.0, 1.0)
);
void main() {
    vec2 position = euiPositions[gl_VertexID];
    euiTexCoord = position * 0.5 + 0.5;
    gl_Position = vec4(position, 0.0, 1.0);
}
)GLSL";

void setError(ShaderToyError* error,
              ShaderToyErrorCode code,
              const ShaderToyPass* pass,
              const char* stage,
              std::string message) {
    if (error == nullptr) {
        return;
    }
    *error = {code, {}, pass != nullptr ? pass->name : std::string{},
              stage != nullptr ? stage : std::string{},
              pass != nullptr ? pass->fragmentPath : std::string{}, 0, std::move(message)};
}

int shaderSourceLine(const std::string& log) {
    for (std::size_t index = 0; index + 2 < log.size(); ++index) {
        if (log[index] != '0' || (log[index + 1] != '(' && log[index + 1] != ':')) {
            continue;
        }
        std::size_t cursor = index + 2;
        if (cursor >= log.size() || !std::isdigit(static_cast<unsigned char>(log[cursor]))) {
            continue;
        }
        int line = 0;
        while (cursor < log.size() &&
               std::isdigit(static_cast<unsigned char>(log[cursor]))) {
            line = line * 10 + (log[cursor] - '0');
            ++cursor;
        }
        if (line > 0) return line;
    }
    return 0;
}

GLuint compileShader(GLenum type,
                     const std::string& source,
                     const ShaderToyPass* pass,
                     ShaderToyError* error) {
    const GLuint shader = glCreateShader(type);
    const char* text = source.c_str();
    glShaderSource(shader, 1, &text, nullptr);
    glCompileShader(shader);
    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_TRUE) {
        return shader;
    }
    GLint length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    std::string log(static_cast<std::size_t>(std::max(1, length)), '\0');
    glGetShaderInfoLog(shader, length, nullptr, log.data());
    glDeleteShader(shader);
    setError(error, ShaderToyErrorCode::ShaderCompileFailed, pass,
             type == GL_VERTEX_SHADER ? "vertex" : "fragment", std::move(log));
    if (error != nullptr) {
        error->line = shaderSourceLine(error->message);
    }
    return 0;
}

GLuint buildProgram(const ShaderToyPass& pass,
                    const std::vector<ShaderToyUniform>& uniforms,
                    ShaderToyError* error) {
    std::string source;
    ShaderToyError readError;
    if (!resolveShaderToySource(pass, source, readError)) {
        if (error != nullptr) {
            *error = std::move(readError);
        }
        return 0;
    }
    const GLuint vertex = compileShader(GL_VERTEX_SHADER, kVertexShader, &pass, error);
    const GLuint fragment = compileShader(GL_FRAGMENT_SHADER,
                                          wrapShaderToyOpenGL(source, uniforms),
                                          &pass, error);
    if (vertex == 0 || fragment == 0) {
        if (vertex != 0) glDeleteShader(vertex);
        if (fragment != 0) glDeleteShader(fragment);
        return 0;
    }
    const GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked == GL_TRUE) {
        return program;
    }
    GLint length = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    std::string log(static_cast<std::size_t>(std::max(1, length)), '\0');
    glGetProgramInfoLog(program, length, nullptr, log.data());
    glDeleteProgram(program);
    setError(error, ShaderToyErrorCode::ShaderLinkFailed, &pass, "link", std::move(log));
    return 0;
}

void deleteTargets(ShaderToyPassResource& pass) {
    glDeleteFramebuffers(2, pass.framebuffers.data());
    glDeleteTextures(2, pass.textures.data());
    pass.framebuffers = {};
    pass.textures = {};
}

void destroyToy(OpenGLShaderToy& toy) {
    for (ShaderToyPassResource& pass : toy.passes) {
        deleteTargets(pass);
        if (pass.program != 0) {
            glDeleteProgram(pass.program);
        }
    }
    for (auto& entry : toy.images) {
        if (entry.second.texture != 0) {
            glDeleteTextures(1, &entry.second.texture);
        }
    }
    if (toy.empty.texture != 0) {
        glDeleteTextures(1, &toy.empty.texture);
    }
    if (toy.vao != 0) {
        glDeleteVertexArrays(1, &toy.vao);
    }
}

bool createGlTexture(GLuint& texture,
                     int width,
                     int height,
                     GLint internalFormat,
                     GLenum type,
                     const void* pixels,
                     GLint wrap = GL_CLAMP_TO_EDGE) {
    glGenTextures(1, &texture);
    if (texture == 0) return false;
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0,
                 GL_RGBA, type, pixels);
    return glGetError() == GL_NO_ERROR;
}

bool createPassTargets(ShaderToyPassResource& pass,
                       int width,
                       int height) {
    for (int index = 0; index < 2; ++index) {
        if (!createGlTexture(pass.textures[index], width, height,
                             GL_RGBA32F, GL_FLOAT, nullptr)) {
            deleteTargets(pass);
            return false;
        }
        glGenFramebuffers(1, &pass.framebuffers[index]);
        glBindFramebuffer(GL_FRAMEBUFFER, pass.framebuffers[index]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                               GL_TEXTURE_2D, pass.textures[index], 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            deleteTargets(pass);
            return false;
        }
    }
    pass.width = width;
    pass.height = height;
    return true;
}

bool resizeTargets(OpenGLShaderToy& toy,
                   const ShaderToyGraph& graph,
                   int width,
                   int height,
                   ShaderToyError* error) {
    bool matches = toy.width == width && toy.height == height &&
                   toy.passes.size() == graph.passes.size();
    for (std::size_t index = 0; matches && index < toy.passes.size(); ++index) {
        matches = toy.passes[index].textures[0] != 0 &&
                  toy.passes[index].width == width &&
                  toy.passes[index].height == height;
    }
    if (matches) {
        return true;
    }
    GLint maxTextureSize = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
    if (width > maxTextureSize || height > maxTextureSize) {
        setError(error, ShaderToyErrorCode::ResourceCreationFailed, nullptr,
                 "framebuffer", "Shadertoy target exceeds the OpenGL texture size limit.");
        return false;
    }
    std::vector<ShaderToyPassResource> replacements(toy.passes.size());
    for (std::size_t index = 0; index < replacements.size(); ++index) {
        ShaderToyPassResource& pass = replacements[index];
        if (!createPassTargets(pass, width, height)) {
            for (ShaderToyPassResource& created : replacements) {
                deleteTargets(created);
            }
            setError(error, ShaderToyErrorCode::ResourceCreationFailed, nullptr,
                     "framebuffer", "Unable to create RGBA32F Shadertoy targets.");
            return false;
        }
    }
    for (std::size_t index = 0; index < toy.passes.size(); ++index) {
        deleteTargets(toy.passes[index]);
        toy.passes[index].textures = replacements[index].textures;
        toy.passes[index].framebuffers = replacements[index].framebuffers;
        toy.passes[index].width = replacements[index].width;
        toy.passes[index].height = replacements[index].height;
        replacements[index].textures = {};
        replacements[index].framebuffers = {};
    }
    toy.width = width;
    toy.height = height;
    toy.currentIndex = 0;
    toy.hasOutput = false;
    toy.lastFrameToken = kInvalidFrameToken;
    return true;
}

std::string imageKey(const ShaderToyChannel& channel) {
    return channel.source;
}

bool uploadImage(const ShaderToyChannel& channel,
                 ShaderToyImage& target,
                 ShaderToyError* error) {
    bool pending = false;
    const std::shared_ptr<const image::StaticImageData> data =
        image::loadStaticImage(channel.source, true, &pending);
    if (!data || !data->pixels || data->width <= 0 || data->height <= 0) {
        setError(error, ShaderToyErrorCode::SourceReadFailed, nullptr, "image",
                 pending ? "Image channel is not ready." :
                           "Unable to decode image channel: " + channel.source);
        if (error != nullptr) {
            error->sourcePath = channel.source;
        }
        return false;
    }
    if (!createGlTexture(target.texture, data->width, data->height,
                         GL_RGBA8, GL_UNSIGNED_BYTE, data->pixels.get(),
                         GL_REPEAT)) {
        setError(error, ShaderToyErrorCode::ResourceCreationFailed, nullptr,
                 "image", "Unable to upload image channel: " + channel.source);
        return false;
    }
    target.width = data->width;
    target.height = data->height;
    return true;
}

std::filesystem::file_time_type sourceTime(const std::string& path, bool& valid) {
    std::error_code fileError;
    const auto result = std::filesystem::last_write_time(
        std::filesystem::u8path(path), fileError);
    valid = !fileError;
    return result;
}

std::filesystem::file_time_type sourceTime(const ShaderToyPass& pass,
                                           bool& valid) {
    if (pass.sourceKind == ShaderToySourceKind::Inline) {
        valid = false;
        return {};
    }
    return sourceTime(pass.fragmentPath, valid);
}

void reloadChangedPrograms(OpenGLShaderToy& toy,
                           const ShaderToyGraph& graph) {
    for (std::size_t index = 0; index < toy.passes.size(); ++index) {
        bool valid = false;
        const auto nextTime = sourceTime(graph.passes[index], valid);
        ShaderToyPassResource& pass = toy.passes[index];
        if ((valid && pass.sourceTimeValid && pass.sourceTime == nextTime) ||
            (!valid && !pass.sourceTimeValid)) {
            continue;
        }
        ShaderToyError reloadError;
        const GLuint replacement = buildProgram(graph.passes[index], graph.uniforms, &reloadError);
        pass.sourceTime = nextTime;
        pass.sourceTimeValid = valid;
        if (replacement != 0) {
            glDeleteProgram(pass.program);
            pass.program = replacement;
            if (toy.hotReloadError.passName == graph.passes[index].name) {
                toy.hotReloadError = {};
            }
        } else if (reloadError) {
            toy.hotReloadError = std::move(reloadError);
        }
    }
}

void setCustomUniform(GLuint program, const ShaderToyUniform& uniform) {
    const GLint location = glGetUniformLocation(program, uniform.name.c_str());
    if (location < 0) return;
    switch (uniform.kind) {
    case ShaderToyUniformKind::Float: glUniform1f(location, uniform.values[0]); break;
    case ShaderToyUniformKind::Vec2: glUniform2fv(location, 1, uniform.values.data()); break;
    case ShaderToyUniformKind::Vec3: glUniform3fv(location, 1, uniform.values.data()); break;
    case ShaderToyUniformKind::Vec4: glUniform4fv(location, 1, uniform.values.data()); break;
    case ShaderToyUniformKind::Int: glUniform1i(location, static_cast<int>(uniform.values[0])); break;
    }
}

} // namespace

OpenGLRenderBackend::ShaderToyHandle OpenGLRenderBackend::createShaderToy(
    const ShaderToyGraph& graph,
    ShaderToyError* error) {
    flushRoundedRectBatch();
    if (error != nullptr) *error = {};
    const ShaderToyValidationResult validation = validateShaderToyGraph(graph);
    if (!validation.valid()) {
        if (error != nullptr) *error = validation.errors.front();
        return nullptr;
    }
    auto toy = std::make_unique<OpenGLShaderToy>();
    toy->passes.resize(graph.passes.size());
    OpenGLStateScope state;

    glGenVertexArrays(1, &toy->vao);
    const std::array<unsigned char, 4> black{0, 0, 0, 255};
    if (toy->vao == 0 ||
        !createGlTexture(toy->empty.texture, 1, 1, GL_RGBA8, GL_UNSIGNED_BYTE, black.data())) {
        setError(error, ShaderToyErrorCode::ResourceCreationFailed, nullptr,
                 "resource", "Unable to create Shadertoy quad resources.");
        destroyToy(*toy);
        resetStateCache();
        return nullptr;
    }

    for (std::size_t index = 0; index < graph.passes.size(); ++index) {
        ShaderToyPassResource& pass = toy->passes[index];
        pass.program = buildProgram(graph.passes[index], graph.uniforms, error);
        pass.sourceTime = sourceTime(graph.passes[index], pass.sourceTimeValid);
        if (pass.program == 0) {
            destroyToy(*toy);
            resetStateCache();
            return nullptr;
        }
    }

    for (const ShaderToyPass& pass : graph.passes) {
        for (const ShaderToyChannel& channel : pass.channels) {
            if (channel.kind != ShaderToyChannelKind::Image) {
                continue;
            }
            const std::string key = imageKey(channel);
            if (toy->images.find(key) != toy->images.end()) continue;
            ShaderToyImage image;
            if (!uploadImage(channel, image, error)) {
                destroyToy(*toy);
                resetStateCache();
                return nullptr;
            }
            toy->images.emplace(key, std::move(image));
        }
    }

    OpenGLShaderToy* handle = toy.release();
    shaderToys_.push_back(handle);
    resetStateCache();
    return handle;
}

OpenGLRenderBackend::TextureHandle OpenGLRenderBackend::renderShaderToy(
    ShaderToyHandle handle,
    const ShaderToyGraph& graph,
    int width,
    int height,
    const ShaderToyFrameData& frame,
    bool paused,
    bool reset,
    ShaderToyError* error) {
    flushRoundedRectBatch();
    auto* toy = static_cast<OpenGLShaderToy*>(handle);
    if (toy == nullptr || width <= 0 || height <= 0 || graph.passes.size() != toy->passes.size()) {
        setError(error, ShaderToyErrorCode::ResourceCreationFailed, nullptr,
                 "render", "Invalid OpenGL Shadertoy resource.");
        return nullptr;
    }
    OpenGLStateScope state;
    reloadChangedPrograms(*toy, graph);
    if (error != nullptr) *error = toy->hotReloadError;
    if (!resizeTargets(*toy, graph, width, height, error)) {
        resetStateCache();
        return toy->hasOutput ? &toy->output : nullptr;
    }
    if (reset) {
        toy->currentIndex = 0;
        toy->hasOutput = false;
        toy->lastFrameToken = kInvalidFrameToken;
        for (ShaderToyPassResource& pass : toy->passes) {
            for (GLuint framebuffer : pass.framebuffers) {
                glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
                glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
                glClear(GL_COLOR_BUFFER_BIT);
            }
        }
    }
    if ((paused && toy->hasOutput) ||
        (toy->lastFrameToken == frame.frameToken && toy->hasOutput)) {
        resetStateCache();
        return &toy->output;
    }

    const int previousIndex = toy->currentIndex;
    const int targetIndex = 1 - previousIndex;
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_FRAMEBUFFER_SRGB);
    glBindVertexArray(toy->vao);

    for (std::size_t passIndex = 0; passIndex < toy->passes.size(); ++passIndex) {
        ShaderToyPassResource& pass = toy->passes[passIndex];
        const ShaderToyPass& passShape = graph.passes[passIndex];
        glViewport(0, 0, pass.width, pass.height);
        glUseProgram(pass.program);
        glUniform3f(glGetUniformLocation(pass.program, "iResolution"),
                    static_cast<float>(pass.width),
                    static_cast<float>(pass.height), 1.0f);
        glUniform1f(glGetUniformLocation(pass.program, "iTime"), frame.time);
        glUniform1f(glGetUniformLocation(pass.program, "iTimeDelta"), frame.deltaTime);
        glUniform1i(glGetUniformLocation(pass.program, "iFrame"), frame.frame);
        glUniform1f(glGetUniformLocation(pass.program, "iFrameRate"), frame.frameRate);
        glUniform4fv(glGetUniformLocation(pass.program, "iDate"), 1, frame.date.data());
        const float mouseScaleX = static_cast<float>(pass.width) /
                                   static_cast<float>(width);
        const float mouseScaleY = static_cast<float>(pass.height) /
                                  static_cast<float>(height);
        const std::array<float, 4> passMouse{
            frame.mouse[0] * mouseScaleX,
            frame.mouse[1] * mouseScaleY,
            frame.mouse[2] * mouseScaleX,
            frame.mouse[3] * mouseScaleY
        };
        glUniform4fv(glGetUniformLocation(pass.program, "iMouse"),
                     1, passMouse.data());
        glUniform1fv(glGetUniformLocation(pass.program, "iChannelTime"),
                     static_cast<GLsizei>(frame.channelTime.size()),
                     frame.channelTime.data());
        glUniform1f(glGetUniformLocation(pass.program, "iSampleRate"),
                    frame.sampleRate);
        for (const ShaderToyUniform& uniform : graph.uniforms) {
            setCustomUniform(pass.program, uniform);
        }

        for (std::size_t channelIndex = 0;
             channelIndex < kShaderToyChannelCount;
             ++channelIndex) {
            const ShaderToyChannel& channel = passShape.channels[channelIndex];
            const ShaderToyImage* image = &toy->empty;
            GLuint texture = toy->empty.texture;
            int channelWidth = image->width;
            int channelHeight = image->height;
            if (channel.kind == ShaderToyChannelKind::Image) {
                const auto found = toy->images.find(imageKey(channel));
                if (found != toy->images.end()) {
                    image = &found->second;
                    texture = image->texture;
                    channelWidth = image->width;
                    channelHeight = image->height;
                }
            } else if (channel.kind == ShaderToyChannelKind::Self) {
                if (toy->hasOutput) {
                    texture = pass.textures[previousIndex];
                    image = nullptr;
                    channelWidth = pass.width;
                    channelHeight = pass.height;
                }
            } else if (channel.kind == ShaderToyChannelKind::Buffer) {
                const ShaderToyPass* sourcePass = graph.findPass(channel.source);
                if (sourcePass != nullptr) {
                    const std::size_t sourceIndex =
                        static_cast<std::size_t>(sourcePass - graph.passes.data());
                    if (sourceIndex < passIndex || toy->hasOutput) {
                        const int readIndex = sourceIndex < passIndex ? targetIndex : previousIndex;
                        texture = toy->passes[sourceIndex].textures[readIndex];
                        image = nullptr;
                        channelWidth = toy->passes[sourceIndex].width;
                        channelHeight = toy->passes[sourceIndex].height;
                    }
                }
            }
            glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(channelIndex));
            glBindTexture(GL_TEXTURE_2D, texture);
            glBindSampler(static_cast<GLuint>(channelIndex), 0);
            const std::string samplerName = "iChannel" + std::to_string(channelIndex);
            glUniform1i(glGetUniformLocation(pass.program, samplerName.c_str()),
                        static_cast<GLint>(channelIndex));
            const std::string resolutionName =
                "iChannelResolution[" + std::to_string(channelIndex) + "]";
            glUniform3f(glGetUniformLocation(pass.program, resolutionName.c_str()),
                        static_cast<float>(channelWidth),
                        static_cast<float>(channelHeight), 1.0f);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, pass.framebuffers[targetIndex]);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    toy->currentIndex = targetIndex;
    toy->hasOutput = true;
    toy->lastFrameToken = frame.frameToken;
    toy->output.texture = toy->passes.back().textures[targetIndex];
    if (error != nullptr) *error = toy->hotReloadError;
    resetStateCache();
    return &toy->output;
}

void OpenGLRenderBackend::destroyShaderToy(ShaderToyHandle handle) {
    flushRoundedRectBatch();
    auto* toy = static_cast<OpenGLShaderToy*>(handle);
    if (toy == nullptr) return;
    OpenGLStateScope state;
    const auto found = std::find(shaderToys_.begin(), shaderToys_.end(), handle);
    if (found != shaderToys_.end()) {
        shaderToys_.erase(found);
    }
    state.discardBindings(*toy);
    destroyToy(*toy);
    delete toy;
    resetStateCache();
}

bool OpenGLRenderBackend::readShaderToyPixel(ShaderToyHandle handle, float* rgba) {
    flushRoundedRectBatch();
    auto* toy = static_cast<OpenGLShaderToy*>(handle);
    if (toy == nullptr || rgba == nullptr) return false;
    std::vector<float> pixels(
        static_cast<std::size_t>(toy->passes.back().width) *
        static_cast<std::size_t>(toy->passes.back().height) * 4u);
    if (!readShaderToyPixels(handle, pixels.data(), pixels.size())) return false;
    std::copy_n(pixels.data(), 4, rgba);
    return true;
}

bool OpenGLRenderBackend::readShaderToyPixels(ShaderToyHandle handle,
                                              float* rgba,
                                              std::size_t floatCount) {
    flushRoundedRectBatch();
    auto* toy = static_cast<OpenGLShaderToy*>(handle);
    if (toy == nullptr || rgba == nullptr || !toy->hasOutput || toy->output.texture == 0) {
        return false;
    }
    const std::size_t required =
        static_cast<std::size_t>(toy->passes.back().width) *
        static_cast<std::size_t>(toy->passes.back().height) * 4u;
    if (floatCount < required) return false;
    OpenGLStateScope state;
    glBindTexture(GL_TEXTURE_2D, toy->output.texture);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, rgba);
    const bool success = glGetError() == GL_NO_ERROR;
    resetStateCache();
    return success;
}

void OpenGLRenderBackend::releaseShaderToys() {
    flushRoundedRectBatch();
    std::vector<ShaderToyHandle> resources = std::move(shaderToys_);
    shaderToys_.clear();
    for (ShaderToyHandle handle : resources) {
        auto* toy = static_cast<OpenGLShaderToy*>(handle);
        if (toy != nullptr) {
            destroyToy(*toy);
            delete toy;
        }
    }
    resetStateCache();
}

} // namespace core::render::opengl
