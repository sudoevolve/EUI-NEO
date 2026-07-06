#pragma once

#include "core/render/primitive_geometry.h"
#include "core/render/render_surface.h"
#include "core/window/window_types.h"

#include <cstddef>
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

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

struct RenderFrameStats {
    bool rendered = false;
    bool fullPaint = false;
    bool usedRenderCache = false;
    bool renderCacheRecreated = false;
    int framebufferWidth = 0;
    int framebufferHeight = 0;
    int dirtyRectCount = 0;
    std::uint64_t dirtyPixels = 0;
    std::uint64_t blitPixels = 0;
    int clearCalls = 0;
    int renderDirectPasses = 0;
    int cacheBlits = 0;
    int backendRenderPasses = 0;
    std::uint64_t backendRenderPassPixels = 0;
    int backendCopyRegions = 0;
    int backendBarriers = 0;
    int backendSubmits = 0;
    int backendPresents = 0;
    std::uint64_t backendPresentPixels = 0;
    int backendIncrementalPresents = 0;
    int backendIncrementalPresentSupported = 0;
    int backendResolveDraws = 0;
    int rectDraws = 0;
    int polygonDraws = 0;
    int textPrepares = 0;
    int textDraws = 0;
    int imageDraws = 0;
    int retainedLayerHits = 0;
    int retainedLayerMisses = 0;
    int retainedLayerDraws = 0;
    int retainedLayerRebuilds = 0;
};

enum class RenderCacheBlitMode {
    Full,
    Dirty,
    Existing
};

inline core::Rect clampRenderRect(const core::Rect& rect, int width, int height) {
    const float maxWidth = static_cast<float>(std::max(0, width));
    const float maxHeight = static_cast<float>(std::max(0, height));
    const float left = std::clamp(std::floor(rect.x), 0.0f, maxWidth);
    const float top = std::clamp(std::floor(rect.y), 0.0f, maxHeight);
    const float right = std::clamp(std::ceil(rect.x + rect.width), left, maxWidth);
    const float bottom = std::clamp(std::ceil(rect.y + rect.height), top, maxHeight);
    return {left, top, right - left, bottom - top};
}

inline std::vector<core::Rect> clampRenderRects(const std::vector<core::Rect>& rects, int width, int height) {
    std::vector<core::Rect> result;
    result.reserve(rects.size());
    for (const core::Rect& rect : rects) {
        const core::Rect clamped = clampRenderRect(rect, width, height);
        if (clamped.width > 0.0f && clamped.height > 0.0f) {
            result.push_back(clamped);
        }
    }
    return result;
}

inline std::uint64_t renderRectAreaPixels(const core::Rect& rect) {
    return static_cast<std::uint64_t>(std::max(0.0f, rect.width)) *
           static_cast<std::uint64_t>(std::max(0.0f, rect.height));
}

inline std::uint64_t renderRectAreaPixels(const std::vector<core::Rect>& rects) {
    std::uint64_t pixels = 0;
    for (const core::Rect& rect : rects) {
        pixels += renderRectAreaPixels(rect);
    }
    return pixels;
}

inline bool renderRectsIntersect(const core::Rect& a, const core::Rect& b) {
    return a.x < b.x + b.width &&
           a.x + a.width > b.x &&
           a.y < b.y + b.height &&
           a.y + a.height > b.y;
}

inline core::Rect unionRenderRect(const core::Rect& a, const core::Rect& b) {
    const float left = std::min(a.x, b.x);
    const float top = std::min(a.y, b.y);
    const float right = std::max(a.x + a.width, b.x + b.width);
    const float bottom = std::max(a.y + a.height, b.y + b.height);
    return {left, top, right - left, bottom - top};
}

inline bool shouldMergeRenderRects(const core::Rect& a, const core::Rect& b) {
    if (renderRectsIntersect(a, b)) {
        return true;
    }
    const std::uint64_t separateArea = renderRectAreaPixels(a) + renderRectAreaPixels(b);
    if (separateArea == 0) {
        return true;
    }
    const std::uint64_t mergedArea = renderRectAreaPixels(unionRenderRect(a, b));
    return static_cast<double>(mergedArea) <= static_cast<double>(separateArea) * 1.25;
}

inline std::vector<core::Rect> mergeRenderRects(std::vector<core::Rect> rects) {
    bool changed = true;
    while (changed) {
        changed = false;
        for (std::size_t i = 0; i < rects.size() && !changed; ++i) {
            for (std::size_t j = i + 1; j < rects.size(); ++j) {
                if (shouldMergeRenderRects(rects[i], rects[j])) {
                    rects[i] = unionRenderRect(rects[i], rects[j]);
                    rects.erase(rects.begin() + static_cast<std::ptrdiff_t>(j));
                    changed = true;
                    break;
                }
            }
        }
    }
    return rects;
}

inline core::Rect fullRenderRect(int width, int height) {
    return {0.0f, 0.0f, static_cast<float>(std::max(0, width)), static_cast<float>(std::max(0, height))};
}

class RenderBackend {
public:
    using TextureHandle = void*;
    using LayerHandle = void*;

    virtual ~RenderBackend() = default;

    virtual bool initialize() = 0;
    virtual bool valid() const = 0;
    virtual void makeCurrent() = 0;
    virtual void resetSurface() {}
    virtual void beginFrame(const RenderSurface& surface) = 0;
    virtual void present() = 0;
    virtual bool ensureRenderCache(int width, int height) = 0;
    virtual bool renderCacheWasRecreated() const = 0;
    virtual void releaseRenderCache() = 0;
    virtual void beginRenderCacheFrame(int width,
                                       int height,
                                       const std::vector<core::Rect>& repaintRects = {}) = 0;
    virtual void endRenderCacheFrame() = 0;
    virtual void blitRenderCache(int width,
                                 int height,
                                 RenderCacheBlitMode mode = RenderCacheBlitMode::Full,
                                 const std::vector<core::Rect>& dirtyRects = {}) = 0;
    virtual LayerHandle createLayer(int width, int height) {
        (void)width;
        (void)height;
        return nullptr;
    }
    virtual bool resizeLayer(LayerHandle layer, int width, int height) {
        (void)layer;
        (void)width;
        (void)height;
        return false;
    }
    virtual void destroyLayer(LayerHandle layer) {
        (void)layer;
    }
    virtual bool beginLayerFrame(LayerHandle layer, int width, int height) {
        (void)layer;
        (void)width;
        (void)height;
        return false;
    }
    virtual void endLayerFrame() {}
    virtual TextureHandle layerTexture(LayerHandle layer) {
        (void)layer;
        return nullptr;
    }
    virtual void clear(const core::Color& color) = 0;
    virtual void setScissor(bool enabled, const core::Rect& rect, int framebufferHeight) = 0;
    virtual void prepareBackdropBlur(const core::Rect& bounds, float blur, int windowWidth, int windowHeight) = 0;
    virtual void drawRoundedRect(const RoundedRectDrawCommand& command, int windowWidth, int windowHeight) = 0;
    virtual void drawPolygon(const PolygonDrawCommand& command, int windowWidth, int windowHeight) = 0;
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
    virtual void drawLayerTexture(TextureHandle handle,
                                  const float* vertices,
                                  std::size_t vertexFloatCount,
                                  const core::Rect& rect,
                                  int windowWidth,
                                  int windowHeight) {
        drawTexture(handle,
                    vertices,
                    vertexFloatCount,
                    {1.0f, 1.0f, 1.0f, 1.0f},
                    rect,
                    0.0f,
                    windowWidth,
                    windowHeight);
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

inline RenderFrameStats& currentRenderFrameStats() {
    static thread_local RenderFrameStats stats;
    return stats;
}

inline RenderFrameStats& lastRenderFrameStatsSlot() {
    static thread_local RenderFrameStats stats;
    return stats;
}

inline void beginRenderFrameStats(int framebufferWidth, int framebufferHeight) {
    RenderFrameStats& stats = currentRenderFrameStats();
    stats = {};
    stats.rendered = true;
    stats.framebufferWidth = framebufferWidth;
    stats.framebufferHeight = framebufferHeight;
}

inline void publishRenderFrameStats() {
    lastRenderFrameStatsSlot() = currentRenderFrameStats();
}

inline const RenderFrameStats& lastRenderFrameStats() {
    return lastRenderFrameStatsSlot();
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
