#include "modules/plot/gpu_scene3d.h"
#include "modules/plot/gpu_scene3d_shader.h"
#include "modules/plot/volume.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>

#if defined(EUI_RENDER_BACKEND_OPENGL)
#include <glad/glad.h>
#endif

namespace modules::plot {
#if defined(EUI_RENDER_BACKEND_OPENGL)
namespace {
// compose runs between core renderer calls, so its cached GL state must remain valid.
struct GlState {
    GLint program, vao, draw, read, active, buffer, unpack, pack, viewport[4];
    GLint textures[4][3], samplers[4], stores[10];
    GLboolean mask[4], enabled[7];
    static constexpr GLenum caps[] = {
        GL_BLEND,        GL_DEPTH_TEST,       GL_CULL_FACE,         GL_SCISSOR_TEST,
        GL_STENCIL_TEST, GL_FRAMEBUFFER_SRGB, GL_RASTERIZER_DISCARD};
    static constexpr GLenum pixelStores[] = {
        GL_UNPACK_ALIGNMENT,    GL_UNPACK_ROW_LENGTH,  GL_UNPACK_SKIP_ROWS, GL_UNPACK_SKIP_PIXELS,
        GL_UNPACK_IMAGE_HEIGHT, GL_UNPACK_SKIP_IMAGES, GL_PACK_ALIGNMENT,   GL_PACK_ROW_LENGTH,
        GL_PACK_SKIP_ROWS,      GL_PACK_SKIP_PIXELS};
    GlState() {
        glGetIntegerv(GL_CURRENT_PROGRAM, &program);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
        glGetIntegerv(GL_TEXTURE_BUFFER, &buffer);
        glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpack);
        glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING, &pack);
        glGetIntegerv(GL_VIEWPORT, viewport);
        glGetBooleanv(GL_COLOR_WRITEMASK, mask);
        for (int i = 0; i < 7; ++i)
            enabled[i] = glIsEnabled(caps[i]);
        for (int i = 0; i < 10; ++i)
            glGetIntegerv(pixelStores[i], &stores[i]);
        for (int i = 0; i < 4; ++i) {
            glActiveTexture(GL_TEXTURE0 + i);
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &textures[i][0]);
            glGetIntegerv(GL_TEXTURE_BINDING_3D, &textures[i][1]);
            glGetIntegerv(GL_TEXTURE_BINDING_BUFFER, &textures[i][2]);
            glGetIntegerv(GL_SAMPLER_BINDING, &samplers[i]);
        }
    }
    ~GlState() {
        glUseProgram(program);
        glBindVertexArray(vao);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, read);
        glBindBuffer(GL_TEXTURE_BUFFER, buffer);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, unpack);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, pack);
        glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
        glColorMask(mask[0], mask[1], mask[2], mask[3]);
        for (int i = 0; i < 7; ++i) {
            if (enabled[i])
                glEnable(caps[i]);
            else
                glDisable(caps[i]);
        }
        for (int i = 0; i < 10; ++i)
            glPixelStorei(pixelStores[i], stores[i]);
        for (int i = 0; i < 4; ++i) {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, textures[i][0]);
            glBindTexture(GL_TEXTURE_3D, textures[i][1]);
            glBindTexture(GL_TEXTURE_BUFFER, textures[i][2]);
            glBindSampler(i, samplers[i]);
        }
        glActiveTexture(active);
    }
};
struct Texture {
    GLuint value = 0;
    ~Texture() {
        if (value)
            glDeleteTextures(1, &value);
    }
};
struct Program {
    GLuint value = 0;
    ~Program() {
        if (value)
            glDeleteProgram(value);
    }
};
GLuint compile(GLenum type, const char* source) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &source, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096]{};
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        glDeleteShader(s);
        throw std::runtime_error(std::string("plot: spatial shader: ") + log);
    }
    return s;
}
std::shared_ptr<Program> program(std::uint64_t device) {
    // Weak ownership: all GL objects die before the device; no static GL destructor.
    static thread_local std::map<std::uint64_t, std::weak_ptr<Program>> cache;
    if (auto p = cache[device].lock())
        return p;
    auto p = std::make_shared<Program>();
    GLuint vs = compile(GL_VERTEX_SHADER, detail::sceneVertexShader), fs = 0;
    try {
        fs = compile(GL_FRAGMENT_SHADER, detail::sceneFragmentShader);
        p->value = glCreateProgram();
        glAttachShader(p->value, vs);
        glAttachShader(p->value, fs);
        glLinkProgram(p->value);
        GLint ok = 0;
        glGetProgramiv(p->value, GL_LINK_STATUS, &ok);
        if (!ok) {
            char log[4096]{};
            glGetProgramInfoLog(p->value, sizeof(log), nullptr, log);
            throw std::runtime_error(std::string("plot: spatial shader link: ") + log);
        }
    } catch (...) {
        glDeleteShader(vs);
        if (fs)
            glDeleteShader(fs);
        throw;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    cache[device] = p;
    return p;
}
void textureParameters(GLenum target) {
    glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if (target == GL_TEXTURE_3D)
        glTexParameteri(target, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
}
} // namespace
#endif

struct GpuSceneRenderer3D::Impl {
    Budget budget;
    std::shared_ptr<const eui::GpuImage> image;
    std::uint64_t revision = 0;
    std::size_t uploads = 0;
#if defined(EUI_RENDER_BACKEND_OPENGL)
    std::uint64_t device = 0, sceneRevision = 0, volumeRevision = 0;
    bool prepared = false;
    detail::GpuSceneData data;
    std::shared_ptr<Program> shader;
    GLuint vao = 0, framebuffer = 0, depth = 0, buffers[3]{}, textures[3]{}, volume = 0;
    std::shared_ptr<VolumeData> volumeSource;
    double volumeOffset = 0, volumeScale = 1;
    ~Impl() {
        image.reset();
        if (vao)
            glDeleteVertexArrays(1, &vao);
        if (framebuffer)
            glDeleteFramebuffers(1, &framebuffer);
        if (depth)
            glDeleteTextures(1, &depth);
        if (volume)
            glDeleteTextures(1, &volume);
        if (buffers[0])
            glDeleteBuffers(3, buffers);
        if (textures[0])
            glDeleteTextures(3, textures);
    }
#endif
};
GpuSceneRenderer3D::GpuSceneRenderer3D(Budget b) : impl_(std::make_unique<Impl>()) { impl_->budget = b; }
GpuSceneRenderer3D::~GpuSceneRenderer3D() = default;
std::shared_ptr<const eui::GpuImage> GpuSceneRenderer3D::image() const { return impl_->image; }
std::uint64_t GpuSceneRenderer3D::revision() const { return impl_->revision; }
std::size_t GpuSceneRenderer3D::uploads() const { return impl_->uploads; }
void GpuSceneRenderer3D::release() {
    auto b = impl_->budget;
    auto r = impl_->revision;
    impl_ = std::make_unique<Impl>();
    impl_->budget = b;
    impl_->revision = r;
}

bool GpuSceneRenderer3D::render(const SceneRenderer3D& source, std::uint64_t sceneRevision,
                                const Camera3D& camera, std::uint32_t width, std::uint32_t height) {
#if defined(EUI_RENDER_BACKEND_OPENGL)
    camera.validate();
    if (!width || !height || width > 32768 || height > 32768)
        throw std::invalid_argument("plot: invalid GPU viewport");
    auto& s = *impl_;
    const auto device = eui::image::gpuDevice();
    if (device.api != eui::GpuApi::OpenGL || !device.identity || (s.device && s.device != device.identity))
        throw std::runtime_error("plot: spatial rendering requires the owning OpenGL device");
    GLint maxTexture = 0, maxBuffer = 0, maxVolume = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexture);
    glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE, &maxBuffer);
    glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &maxVolume);
    if (width > std::uint32_t(maxTexture) || height > std::uint32_t(maxTexture) ||
        std::uint64_t(width) * height * 8 > s.budget.textureBytes)
        throw std::length_error("plot: GPU viewport budget exceeded");
    const bool changed = !s.prepared || s.sceneRevision != sceneRevision;
    if (changed) {
        s.data = source.gpuData();
        s.prepared = false;
    }
    auto& d = s.data;
    if (!d.representable)
        return false;
    const std::size_t geometryBytes = (d.nodes.size() + d.primitives.size()) * 16;
    if (d.nodes.size() > std::size_t(maxBuffer) || d.primitives.size() > std::size_t(maxBuffer) ||
        d.primitives.size() / 12 >= 16777216 || d.nodes.size() / 3 >= 16777216 ||
        geometryBytes > s.budget.vertexBytes)
        return false;
    // After floating-origin conversion, refuse cameras whose float roundoff approaches a pixel.
    const auto relativeEye = (camera.eye() - d.origin) / d.scale;
    const double pixelSpan =
        (camera.projection == Projection3D::Orthographic ? camera.verticalSpan : camera.distance) / d.scale /
        height;
    if (!finite(relativeEye) || length(relativeEye) * 1.2e-7 > pixelSpan * 0.05)
        return false;
    const auto layer = d.scene.volume;
    double offset = 0, scale = 1;
    if (layer) {
        const auto& layout = layer->data->layout();
        const auto& stops = layer->transfer.stops();
        const auto remaining = s.budget.vertexBytes - geometryBytes;
        if (stops.size() > remaining / 32 || stops.size() > std::size_t(maxBuffer) / 2 ||
            layout.sampleCount() > (remaining - stops.size() * 32) / 8)
            return false;
        for (auto n : layout.dimensions)
            if (n > std::size_t(maxVolume))
                return false;
        if (length(layout.bounds().max - layout.bounds().min) / d.settings.volumeStep +
                double(d.primitives.size() / 12) + 1 >
            double(d.settings.maxStepsPerRay))
            return false;
        offset = stops.front().value;
        scale = stops.back().value - offset;
        if (scale == 0)
            scale = 1;
        if (!std::isfinite(scale))
            return false;
    }
    GlState saved;
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    for (int i = 0; i < 6; ++i)
        glPixelStorei(GlState::pixelStores[i], i == 0 ? 1 : 0);
    for (int i = 0; i < 4; ++i)
        glBindSampler(i, 0);
    if (!s.shader) {
        s.shader = program(device.identity);
        s.device = device.identity;
        glGenVertexArrays(1, &s.vao);
        glGenFramebuffers(1, &s.framebuffer);
        glGenTextures(1, &s.depth);
        glGenBuffers(3, s.buffers);
        glGenTextures(3, s.textures);
        glGenTextures(1, &s.volume);
    }
    auto upload = [&](int index, const std::vector<std::array<float, 4>>& data) {
        glActiveTexture(GL_TEXTURE0 + index);
        glBindBuffer(GL_TEXTURE_BUFFER, s.buffers[index]);
        const std::array<float, 4> zero{};
        glBufferData(GL_TEXTURE_BUFFER, std::max<std::size_t>(16, data.size() * 16),
                     data.empty() ? zero.data() : data.front().data(), GL_STATIC_DRAW);
        glBindTexture(GL_TEXTURE_BUFFER, s.textures[index]);
        glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, s.buffers[index]);
    };
    if (changed) {
        upload(0, d.nodes);
        upload(1, d.primitives);
        ++s.uploads;
        if (!layer && s.volumeSource) {
            glDeleteTextures(1, &s.volume);
            glGenTextures(1, &s.volume);
            s.volumeSource.reset();
            upload(2, {});
        }
    }
    if (layer) {
        if (changed) {
            std::vector<std::array<float, 4>> stops;
            for (const auto& knot : layer->transfer.stops()) {
                stops.push_back({float((knot.value - offset) / scale), 0, 0, 0});
                stops.push_back(knot.rgba);
            }
            upload(2, stops);
        }
        if (s.volumeSource != layer->data || s.volumeRevision != layer->data->revision() ||
            s.volumeOffset != offset || s.volumeScale != scale) {
            const auto& layout = layer->data->layout();
            auto n = layout.dimensions;
            glActiveTexture(GL_TEXTURE3);
            glBindTexture(GL_TEXTURE_3D, s.volume);
            textureParameters(GL_TEXTURE_3D);
            glTexImage3D(GL_TEXTURE_3D, 0, GL_RG32F, GLsizei(n[0]), GLsizei(n[1]), GLsizei(n[2]), 0, GL_RG,
                         GL_FLOAT, nullptr);
            // A plane staging buffer bounds CPU memory; loader caches remain on the owning UI thread.
            std::vector<std::array<float, 2>> plane(n[0] * n[1]);
            for (std::size_t z = 0; z < n[2]; ++z) {
                for (std::size_t y = 0; y < n[1]; ++y)
                    for (std::size_t x = 0; x < n[0]; ++x) {
                        double value = layer->data->value(x, y, z);
                        if (std::isfinite(value) && !std::isfinite(float((value - offset) / scale)))
                            return false;
                        plane[y * n[0] + x] = std::isfinite(value)
                                                  ? std::array<float, 2>{float((value - offset) / scale), 1}
                                                  : std::array<float, 2>{0, 0};
                    }
                glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, GLint(z), GLsizei(n[0]), GLsizei(n[1]), 1, GL_RG,
                                GL_FLOAT, plane.data());
            }
            s.volumeSource = layer->data;
            s.volumeRevision = layer->data->revision();
            s.volumeOffset = offset;
            s.volumeScale = scale;
        }
    }
    glActiveTexture(GL_TEXTURE0);
    if (!s.image || s.image->descriptor().width != int(width) ||
        s.image->descriptor().height != int(height)) {
        auto owner = std::make_shared<Texture>();
        glGenTextures(1, &owner->value);
        glBindTexture(GL_TEXTURE_2D, owner->value);
        textureParameters(GL_TEXTURE_2D);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        eui::GpuImageDescriptor descriptor;
        descriptor.device = device;
        descriptor.width = int(width);
        descriptor.height = int(height);
        descriptor.texture = owner->value;
        auto imported = eui::image::importGpuImage(descriptor, owner);
        if (!imported)
            throw std::runtime_error("plot: spatial image import failed");
        s.image = std::move(imported);
        glBindTexture(GL_TEXTURE_2D, s.depth);
        textureParameters(GL_TEXTURE_2D);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, width, height, 0, GL_RED, GL_FLOAT, nullptr);
    }
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s.framebuffer);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           s.image->descriptor().texture, 0);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, s.depth, 0);
    const GLenum attachments[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
    glDrawBuffers(2, attachments);
    if (glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        throw std::runtime_error("plot: spatial framebuffer incomplete");
    glViewport(0, 0, width, height);
    for (auto cap : GlState::caps)
        glDisable(cap);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glUseProgram(s.shader->value);
    glBindVertexArray(s.vao);
    auto location = [&](const char* name) { return glGetUniformLocation(s.shader->value, name); };
    auto integer = [&](const char* name, int value) { glUniform1i(location(name), value); };
    auto scalar = [&](const char* name, double value) { glUniform1f(location(name), float(value)); };
    auto vector = [&](const char* name, Vec3 v) {
        glUniform3f(location(name), float(v.x), float(v.y), float(v.z));
    };
    for (int i = 0; i < 3; ++i) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_BUFFER, s.textures[i]);
    }
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_3D, s.volume);
    integer("nodes", 0);
    integer("primitives", 1);
    integer("transfer", 2);
    integer("volume", 3);
    integer("nodeCount", int(d.nodes.size() / 3));
    integer("primitiveCount", int(d.primitives.size() / 12));
    integer("hasVolume", layer ? 1 : 0);
    integer("clipVolume", d.scene.clipToBounds ? 1 : 0);
    integer("transferCount", layer ? int(layer->transfer.stops().size()) : 0);
    integer("perspective", camera.projection == Projection3D::Perspective ? 1 : 0);
    glUniform2f(location("viewport"), float(width), float(height));
    const Vec3 backward{std::cos(camera.pitch) * std::sin(camera.yaw), std::sin(camera.pitch),
                        std::cos(camera.pitch) * std::cos(camera.yaw)};
    const Vec3 right{std::cos(camera.yaw), 0, -std::sin(camera.yaw)};
    vector("eye", relativeEye);
    vector("backward", backward);
    vector("rightAxis", right);
    vector("upAxis", cross(backward, right));
    vector("boundsMin", (d.scene.bounds.min - d.origin) / d.scale);
    vector("boundsMax", (d.scene.bounds.max - d.origin) / d.scale);
    vector("light", normalized(d.scene.lightDirection));
    scalar("halfSpan", camera.projection == Projection3D::Orthographic
                           ? camera.verticalSpan / (2 * d.scale)
                           : std::tan(camera.fieldOfView * 3.141592653589793 / 360));
    scalar("nearPlane", camera.nearPlane / d.scale);
    scalar("farPlane", camera.farPlane / d.scale);
    scalar("worldScale", d.scale);
    glUniform4fv(location("background"), 1, d.settings.background.data());
    if (layer) {
        auto bounds = layer->data->layout().bounds();
        vector("volumeMin", (bounds.min - d.origin) / d.scale);
        vector("volumeMax", (bounds.max - d.origin) / d.scale);
        scalar("sampleStep", d.settings.volumeStep / d.scale);
        scalar("referenceStep", layer->referenceStep / d.scale);
    }
    glDrawArrays(GL_TRIANGLES, 0, 3);
    s.prepared = true;
    s.sceneRevision = sceneRevision;
    ++s.revision;
    return true;
#else
    (void)source;
    (void)sceneRevision;
    (void)camera;
    (void)width;
    (void)height;
    return false;
#endif
}
double GpuSceneRenderer3D::depthAt(std::uint32_t x, std::uint32_t y) const {
#if defined(EUI_RENDER_BACKEND_OPENGL)
    auto& s = *impl_;
    if (!s.image || x >= std::uint32_t(s.image->descriptor().width) ||
        y >= std::uint32_t(s.image->descriptor().height))
        return std::numeric_limits<double>::infinity();
    GlState saved;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, s.framebuffer);
    glReadBuffer(GL_COLOR_ATTACHMENT1);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    for (int i = 6; i < 10; ++i)
        glPixelStorei(GlState::pixelStores[i], i == 6 ? 1 : 0);
    float depth = 0;
    glReadPixels(x, y, 1, 1, GL_RED, GL_FLOAT, &depth);
    return depth;
#else
    (void)x;
    (void)y;
    return std::numeric_limits<double>::infinity();
#endif
}
} // namespace modules::plot
