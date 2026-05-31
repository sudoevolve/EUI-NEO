#pragma once

#include "core/render/primitive_geometry.h"
#include "core/render/render_surface.h"
#include "core/window/window_types.h"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace core {
struct Color;
struct Rect;
} // namespace core

namespace core::render {

enum class TextAtlasPageKind {
    Gray,
    Color
};

struct TextAtlasPageData {
    TextAtlasPageKind kind = TextAtlasPageKind::Gray;
    int width = 0;
    int height = 0;
    int channels = 0;
    std::uint64_t generation = 0;
    const unsigned char* pixels = nullptr;
};

struct TextDrawCommand {
    const float* vertices = nullptr;
    std::size_t vertexFloatCount = 0;
    core::Color color{};
    TextAtlasPageData grayAtlas{};
    TextAtlasPageData colorAtlas{};
};

class RenderBackend {
public:
    using TextureHandle = void*;

    virtual ~RenderBackend() = default;

    virtual bool initialize() = 0;
    virtual bool valid() const = 0;
    virtual void makeCurrent() = 0;
    virtual void beginFrame(const RenderSurface& surface) = 0;
    virtual void present() = 0;
    virtual bool ensureRenderCache(int width, int height) = 0;
    virtual bool renderCacheWasRecreated() const = 0;
    virtual void releaseRenderCache() = 0;
    virtual void beginRenderCacheFrame(int width, int height) = 0;
    virtual void endRenderCacheFrame() = 0;
    virtual void blitRenderCache(int width, int height) = 0;
    virtual void clear(const core::Color& color) = 0;
    virtual void setScissor(bool enabled, const core::Rect& rect, int framebufferHeight) = 0;
    virtual void prepareBackdropBlur(const core::Rect& bounds, float blur, int windowWidth, int windowHeight) = 0;
    virtual void drawRoundedRect(const RoundedRectDrawCommand& command, int windowWidth, int windowHeight) = 0;
    virtual void drawText(const TextDrawCommand& command, int windowWidth, int windowHeight) = 0;
    virtual TextureHandle createTexture(const unsigned char* pixels, int width, int height) {
        (void)pixels;
        (void)width;
        (void)height;
        return nullptr;
    }
    virtual bool updateTexture(TextureHandle handle, const unsigned char* pixels, int width, int height) {
        (void)handle;
        (void)pixels;
        (void)width;
        (void)height;
        return false;
    }
    virtual void destroyTexture(TextureHandle handle) {
        (void)handle;
    }
    virtual void drawTexture(TextureHandle handle,
                             const float* vertices,
                             std::size_t vertexFloatCount,
                             const core::Color& tint,
                             const core::Rect& rect,
                             float radius,
                             int windowWidth,
                             int windowHeight) {
        (void)handle;
        (void)vertices;
        (void)vertexFloatCount;
        (void)tint;
        (void)rect;
        (void)radius;
        (void)windowWidth;
        (void)windowHeight;
    }
};

std::unique_ptr<RenderBackend> createRenderBackend(core::window::Handle window, RenderBackend* shareBackend = nullptr);
core::window::RenderApi windowRenderApi();
void initializeRenderBackendLoader();

inline RenderBackend*& activeRenderBackendSlot() {
    static thread_local RenderBackend* backend = nullptr;
    return backend;
}

inline RenderBackend* activeRenderBackend() {
    return activeRenderBackendSlot();
}

class ScopedRenderBackend {
public:
    explicit ScopedRenderBackend(RenderBackend& backend)
        : previous_(activeRenderBackendSlot()) {
        activeRenderBackendSlot() = &backend;
    }

    ~ScopedRenderBackend() {
        activeRenderBackendSlot() = previous_;
    }

    ScopedRenderBackend(const ScopedRenderBackend&) = delete;
    ScopedRenderBackend& operator=(const ScopedRenderBackend&) = delete;

private:
    RenderBackend* previous_ = nullptr;
};

} // namespace core::render
