#include "modules/plot/renderer.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

#if defined(EUI_RENDER_BACKEND_OPENGL)
#include <glad/glad.h>
#endif

namespace modules::plot {

#if defined(EUI_RENDER_BACKEND_OPENGL)
namespace {
// 模块在 compose 中绘制，恢复全部所触及状态以保持核心 renderer 的状态缓存有效。
struct GlState {
    GLint program, vao, buffer, drawFramebuffer, readFramebuffer, activeTexture, texture, unpackBuffer;
    GLint viewport[4], blendSrcRgb, blendDstRgb, blendSrcAlpha, blendDstAlpha, blendRgb, blendAlpha;
    GLboolean blend, depth, cull, scissor, stencil, srgb, rasterizerDiscard, colorMask[4];
    GLfloat clearColor[4];
    GlState() {
        glGetIntegerv(GL_CURRENT_PROGRAM, &program);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &buffer);
        glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpackBuffer);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &drawFramebuffer);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &readFramebuffer);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &activeTexture);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture);
        glGetIntegerv(GL_VIEWPORT, viewport);
        glGetIntegerv(GL_BLEND_SRC_RGB, &blendSrcRgb);
        glGetIntegerv(GL_BLEND_DST_RGB, &blendDstRgb);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &blendSrcAlpha);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &blendDstAlpha);
        glGetIntegerv(GL_BLEND_EQUATION_RGB, &blendRgb);
        glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &blendAlpha);
        glGetFloatv(GL_COLOR_CLEAR_VALUE, clearColor);
        glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
        blend = glIsEnabled(GL_BLEND);
        depth = glIsEnabled(GL_DEPTH_TEST);
        cull = glIsEnabled(GL_CULL_FACE);
        scissor = glIsEnabled(GL_SCISSOR_TEST);
        stencil = glIsEnabled(GL_STENCIL_TEST);
        srgb = glIsEnabled(GL_FRAMEBUFFER_SRGB);
        rasterizerDiscard = glIsEnabled(GL_RASTERIZER_DISCARD);
    }
    ~GlState() {
        glUseProgram(program);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, buffer);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, unpackBuffer);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, drawFramebuffer);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, readFramebuffer);
        glActiveTexture(activeTexture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        glBlendFuncSeparate(blendSrcRgb, blendDstRgb, blendSrcAlpha, blendDstAlpha);
        glBlendEquationSeparate(blendRgb, blendAlpha);
        glClearColor(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
        glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
        const auto restore = [](GLenum capability, GLboolean enabled) {
            if (enabled)
                glEnable(capability);
            else
                glDisable(capability);
        };
        restore(GL_BLEND, blend);
        restore(GL_DEPTH_TEST, depth);
        restore(GL_CULL_FACE, cull);
        restore(GL_SCISSOR_TEST, scissor);
        restore(GL_STENCIL_TEST, stencil);
        restore(GL_FRAMEBUFFER_SRGB, srgb);
        restore(GL_RASTERIZER_DISCARD, rasterizerDiscard);
    }
};
struct Texture {
    GLuint texture = 0;
    ~Texture() {
        if (texture)
            glDeleteTextures(1, &texture);
    }
};
GLuint shader(GLenum type, const char* source) {
    const GLuint value = glCreateShader(type);
    glShaderSource(value, 1, &source, nullptr);
    glCompileShader(value);
    GLint compiled = 0;
    glGetShaderiv(value, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char log[2048]{};
        glGetShaderInfoLog(value, sizeof(log), nullptr, log);
        glDeleteShader(value);
        throw std::runtime_error(std::string("plot: shader compilation: ") + log);
    }
    return value;
}
GLuint program() {
    const auto vertex =
        shader(GL_VERTEX_SHADER,
               "#version 330 core\nlayout(location=0) in vec2 position; uniform vec2 size;"
               "void main(){gl_Position=vec4(position.x/size.x*2.0-1.0,1.0-position.y/size.y*2.0,0,1);}");
    GLuint fragment = 0, value = 0;
    try {
        fragment = shader(GL_FRAGMENT_SHADER,
                          "#version 330 core\nuniform vec4 color; out vec4 pixel; void main(){pixel=color;}");
        value = glCreateProgram();
        glAttachShader(value, vertex);
        glAttachShader(value, fragment);
        glLinkProgram(value);
        GLint linked = 0;
        glGetProgramiv(value, GL_LINK_STATUS, &linked);
        if (!linked)
            throw std::runtime_error("plot: shader link failed");
    } catch (...) {
        glDeleteShader(vertex);
        if (fragment)
            glDeleteShader(fragment);
        if (value)
            glDeleteProgram(value);
        throw;
    }
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    return value;
}
} // namespace
#endif

struct Renderer::Impl {
    Budget budget;
    std::shared_ptr<const eui::GpuImage> image;
    std::uint64_t revision = 0;
#if defined(EUI_RENDER_BACKEND_OPENGL)
    GLuint program = 0, vao = 0, vbo = 0, framebuffer = 0;
    std::size_t capacity = 0;
    std::vector<Vertex> uploaded;
    std::uint64_t device = 0;
    ~Impl() {
        image.reset();
        if (framebuffer)
            glDeleteFramebuffers(1, &framebuffer);
        if (vbo)
            glDeleteBuffers(1, &vbo);
        if (vao)
            glDeleteVertexArrays(1, &vao);
        if (program)
            glDeleteProgram(program);
    }
#endif
};
Renderer::Renderer(Budget budget) : impl_(std::make_unique<Impl>()) { impl_->budget = budget; }
Renderer::~Renderer() = default;
std::shared_ptr<const eui::GpuImage> Renderer::image() const { return impl_->image; }
std::uint64_t Renderer::revision() const noexcept { return impl_->revision; }
void Renderer::release() {
    const auto budget = impl_->budget;
    const auto revision = impl_->revision;
    impl_ = std::make_unique<Impl>();
    impl_->budget = budget;
    impl_->revision = revision;
}
void Renderer::render(const std::vector<Batch>& batches, double width, double height, double dpi) {
#if defined(EUI_RENDER_BACKEND_OPENGL)
    const auto device = eui::image::gpuDevice();
    if (device.api != eui::GpuApi::OpenGL || !device.identity)
        throw std::runtime_error("plot: render requires an active OpenGL window");
    if (impl_->device && impl_->device != device.identity)
        throw std::runtime_error("plot: renderer cannot cross window devices");
    if (!std::isfinite(width) || !std::isfinite(height) || !std::isfinite(dpi) || width <= 0 || height <= 0 ||
        dpi <= 0 || width * dpi > 32768 || height * dpi > 32768)
        throw std::invalid_argument("plot: invalid render dimensions");
    const int pixelsX = static_cast<int>(std::ceil(width * dpi));
    const int pixelsY = static_cast<int>(std::ceil(height * dpi));
    if (std::size_t(pixelsX) * pixelsY > impl_->budget.textureBytes / 4)
        throw std::length_error("plot: texture budget exceeded");
    std::size_t count = 0;
    for (const auto& batch : batches) {
        if (batch.vertices.size() % 3 != 0)
            throw std::invalid_argument("plot: incomplete triangle");
        if (batch.vertices.size() > impl_->budget.vertexBytes / sizeof(Vertex) - count)
            throw std::length_error("plot: vertex budget exceeded");
        count += batch.vertices.size();
        for (float value : batch.color)
            if (!std::isfinite(value) || value < 0 || value > 1)
                throw std::invalid_argument("plot: invalid batch color");
        for (const auto& vertex : batch.vertices)
            if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y))
                throw std::invalid_argument("plot: nonfinite vertex");
    }
    if (count > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max()))
        throw std::length_error("plot: draw vertex count exceeded");
    GlState state;
    impl_->device = device.identity;
    GLint maximum = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximum);
    if (pixelsX > maximum || pixelsY > maximum)
        throw std::length_error("plot: GPU texture limit exceeded");
    if (!impl_->program)
        impl_->program = program();
    if (!impl_->vao)
        glGenVertexArrays(1, &impl_->vao);
    if (!impl_->vbo)
        glGenBuffers(1, &impl_->vbo);
    if (!impl_->framebuffer)
        glGenFramebuffers(1, &impl_->framebuffer);
    if (!impl_->image || impl_->image->descriptor().width != pixelsX ||
        impl_->image->descriptor().height != pixelsY) {
        auto owner = std::make_shared<Texture>();
        glGenTextures(1, &owner->texture);
        glBindTexture(GL_TEXTURE_2D, owner->texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, pixelsX, pixelsY, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        eui::GpuImageDescriptor descriptor;
        descriptor.device = device;
        descriptor.width = pixelsX;
        descriptor.height = pixelsY;
        descriptor.texture = owner->texture;
        auto image = eui::image::importGpuImage(descriptor, owner);
        if (!image)
            throw std::runtime_error("plot: GPU image import failed");
        impl_->image = std::move(image);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, impl_->framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           impl_->image->descriptor().texture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        throw std::runtime_error("plot: framebuffer incomplete");
    glViewport(0, 0, pixelsX, pixelsY);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_FRAMEBUFFER_SRGB);
    glDisable(GL_RASTERIZER_DISCARD);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    // 不透明绘图区使颜色保持 straight alpha，避免 UI 再合成时重复预乘。
    glClearColor(0.04f, 0.05f, 0.07f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(impl_->program);
    glUniform2f(glGetUniformLocation(impl_->program, "size"), float(width), float(height));
    glBindVertexArray(impl_->vao);
    glBindBuffer(GL_ARRAY_BUFFER, impl_->vbo);
    if (count * sizeof(Vertex) > impl_->capacity) {
        impl_->capacity = count * sizeof(Vertex);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(impl_->capacity), nullptr, GL_DYNAMIC_DRAW);
        impl_->uploaded.clear();
    }
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
    glEnableVertexAttribArray(0);
    std::size_t offset = 0;
    for (const auto& batch : batches) {
        if (batch.vertices.empty())
            continue;
        // 固定大小块比较，数据局部变化且几何布局不变时只上传受影响区间。
        constexpr std::size_t chunkVertices = 4096;
        for (std::size_t start = 0; start < batch.vertices.size(); start += chunkVertices) {
            const auto length = std::min(chunkVertices, batch.vertices.size() - start);
            const auto first = offset + start;
            const bool changed = first + length > impl_->uploaded.size() ||
                                 std::memcmp(impl_->uploaded.data() + first, batch.vertices.data() + start,
                                             length * sizeof(Vertex)) != 0;
            if (changed) {
                glBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>(first * sizeof(Vertex)),
                                static_cast<GLsizeiptr>(length * sizeof(Vertex)),
                                batch.vertices.data() + start);
                if (first + length > impl_->uploaded.size())
                    impl_->uploaded.resize(first + length);
                std::copy_n(batch.vertices.data() + start, length, impl_->uploaded.data() + first);
            }
        }
        glUniform4fv(glGetUniformLocation(impl_->program, "color"), 1, batch.color.data());
        glDrawArrays(GL_TRIANGLES, static_cast<GLint>(offset), static_cast<GLsizei>(batch.vertices.size()));
        offset += batch.vertices.size();
    }
    ++impl_->revision;
#else
    (void)batches;
    (void)width;
    (void)height;
    (void)dpi;
    throw std::runtime_error("plot: Vulkan rendering is not implemented");
#endif
}
} // namespace modules::plot
