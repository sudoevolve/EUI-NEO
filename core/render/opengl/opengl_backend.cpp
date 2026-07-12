#include "core/render/opengl/opengl_backend.h"

#include "core/render/render_types.h"

#include <algorithm>
#include <cmath>
#include <glad/glad.h>

#if defined(EUI_WINDOW_BACKEND_SDL2)
#include <SDL.h>
#else
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>
#endif

namespace core::render::opengl {

namespace {

bool& gladLoaded() {
    static bool loaded = false;
    return loaded;
}

#if defined(EUI_WINDOW_BACKEND_SDL2)
bool loadOpenGLFunctions() {
    if (gladLoaded()) {
        return true;
    }
    gladLoaded() = gladLoadGLLoader(reinterpret_cast<GLADloadproc>(SDL_GL_GetProcAddress)) != 0;
    return gladLoaded();
}
#else
bool loadOpenGLFunctions() {
    if (gladLoaded()) {
        return true;
    }
    gladLoaded() = gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)) != 0;
    return gladLoaded();
}
#endif

bool platformRequiresConservativeBackbufferSync() {
#if defined(__linux__) || defined(_WIN32)
    // The backbuffer contents and swap order are not guaranteed after presenting.
    // Use full cache blits instead of assuming a preserved two-buffer swap chain.
    return true;
#else
    return false;
#endif
}

} // namespace

OpenGLRenderBackend::OpenGLRenderBackend(core::window::Handle window, RenderBackend* shareContext)
    : window_(window), shareContext_(shareContext) {}

OpenGLRenderBackend::~OpenGLRenderBackend() {
    if (!initialized_) {
        return;
    }
    makeCurrent();
    releaseRenderCache();
    releasePrimitiveResources();
    releasePolygonResources();
    releaseTextResources();
    releaseImageResources();
#if defined(EUI_WINDOW_BACKEND_SDL2)
    if (context_ != nullptr) {
        SDL_GL_DeleteContext(context_);
        context_ = nullptr;
    }
#endif
    initialized_ = false;
}

bool OpenGLRenderBackend::initialize() {
    if (window_ == nullptr) {
        return false;
    }

#if defined(EUI_WINDOW_BACKEND_SDL2)
    if (shareContext_ != nullptr) {
        shareContext_->makeCurrent();
    }
    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, shareContext_ != nullptr ? 1 : 0);
    context_ = SDL_GL_CreateContext(static_cast<SDL_Window*>(window_));
    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 0);
    if (context_ == nullptr) {
        return false;
    }
    makeCurrent();
    SDL_GL_SetSwapInterval(0);
#else
    context_ = window_;
    makeCurrent();
    glfwSwapInterval(0);
#endif

    if (!loadOpenGLFunctions()) {
#if defined(EUI_WINDOW_BACKEND_SDL2)
        SDL_GL_DeleteContext(context_);
        context_ = nullptr;
#endif
        return false;
    }

    damagePresentSupported_ = false;
    initialized_ = true;
    return true;
}

bool OpenGLRenderBackend::valid() const {
    return initialized_ && window_ != nullptr;
}

void OpenGLRenderBackend::resetStateCache() {
    stateCacheValid_ = false;
    blendEnabled_ = false;
    alphaBlendSet_ = false;
    scissorEnabledState_ = false;
    scissorX_ = 0;
    scissorY_ = 0;
    scissorWidth_ = 0;
    scissorHeight_ = 0;
    currentProgram_ = 0;
    currentVao_ = 0;
    currentArrayBuffer_ = 0;
    currentTextureUnit_ = 0;
    for (unsigned int& texture : currentTexture2D_) {
        texture = 0;
    }
}

void OpenGLRenderBackend::useProgram(unsigned int program) {
    if (!stateCacheValid_ || currentProgram_ != program) {
        glUseProgram(program);
        currentProgram_ = program;
        stateCacheValid_ = true;
    }
}

void OpenGLRenderBackend::bindVertexArray(unsigned int vao) {
    if (!stateCacheValid_ || currentVao_ != vao) {
        glBindVertexArray(vao);
        currentVao_ = vao;
        stateCacheValid_ = true;
    }
}

void OpenGLRenderBackend::bindArrayBuffer(unsigned int buffer) {
    if (!stateCacheValid_ || currentArrayBuffer_ != buffer) {
        glBindBuffer(GL_ARRAY_BUFFER, buffer);
        currentArrayBuffer_ = buffer;
        stateCacheValid_ = true;
    }
}

void OpenGLRenderBackend::activeTextureUnit(unsigned int unit) {
    if (unit >= 8) {
        return;
    }
    if (!stateCacheValid_ || currentTextureUnit_ != unit) {
        glActiveTexture(GL_TEXTURE0 + unit);
        currentTextureUnit_ = unit;
        stateCacheValid_ = true;
    }
}

void OpenGLRenderBackend::bindTexture2D(unsigned int texture) {
    if (currentTextureUnit_ >= 8) {
        glBindTexture(GL_TEXTURE_2D, texture);
        resetStateCache();
        return;
    }
    if (!stateCacheValid_ || currentTexture2D_[currentTextureUnit_] != texture) {
        glBindTexture(GL_TEXTURE_2D, texture);
        currentTexture2D_[currentTextureUnit_] = texture;
        stateCacheValid_ = true;
    }
}

void OpenGLRenderBackend::setBlendEnabled(bool enabled) {
    if (!stateCacheValid_ || blendEnabled_ != enabled) {
        if (enabled) {
            glEnable(GL_BLEND);
        } else {
            glDisable(GL_BLEND);
        }
        blendEnabled_ = enabled;
        stateCacheValid_ = true;
    }
}

void OpenGLRenderBackend::setStandardAlphaBlend() {
    setBlendEnabled(true);
    if (!stateCacheValid_ || !alphaBlendSet_) {
        glBlendFuncSeparate(GL_SRC_ALPHA,
                            GL_ONE_MINUS_SRC_ALPHA,
                            GL_ONE,
                            GL_ONE_MINUS_SRC_ALPHA);
        alphaBlendSet_ = true;
        stateCacheValid_ = true;
    }
}

void OpenGLRenderBackend::setPremultipliedAlphaBlend() {
    setBlendEnabled(true);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    alphaBlendSet_ = false;
    stateCacheValid_ = true;
}

void OpenGLRenderBackend::makeCurrent() {
    if (window_ == nullptr) {
        return;
    }
#if defined(EUI_WINDOW_BACKEND_SDL2)
    if (context_ != nullptr) {
        SDL_GL_MakeCurrent(static_cast<SDL_Window*>(window_), context_);
    }
#else
    glfwMakeContextCurrent(static_cast<GLFWwindow*>(window_));
#endif
    resetStateCache();
}

void OpenGLRenderBackend::beginFrame(const RenderSurface& surface) {
    makeCurrent();
    framebufferWidth_ = std::max(0, surface.framebufferWidth);
    framebufferHeight_ = std::max(0, surface.framebufferHeight);
    glViewport(0, 0, surface.framebufferWidth, surface.framebufferHeight);
}

void OpenGLRenderBackend::present() {
    if (window_ == nullptr) {
        return;
    }
#if defined(EUI_WINDOW_BACKEND_SDL2)
    SDL_GL_SwapWindow(static_cast<SDL_Window*>(window_));
#else
    glfwSwapBuffers(static_cast<GLFWwindow*>(window_));
#endif
    core::render::RenderFrameStats& stats = core::render::currentRenderFrameStats();
    ++stats.backendPresents;
    stats.backendPresentPixels += static_cast<std::uint64_t>(std::max(0, framebufferWidth_)) *
                                  static_cast<std::uint64_t>(std::max(0, framebufferHeight_));
    stats.backendIncrementalPresentSupported = damagePresentSupported_ ? 1 : 0;
    if (!backbufferCacheGenerations_.empty()) {
        currentBackbuffer_ = (currentBackbuffer_ + 1) % backbufferCacheGenerations_.size();
    }
}

bool OpenGLRenderBackend::ensureRenderCache(int width, int height) {
    cacheRecreated_ = false;
    width = std::max(1, width);
    height = std::max(1, height);
    if (cacheFramebuffer_ != 0 && cacheTexture_ != 0 && cacheWidth_ == width && cacheHeight_ == height) {
        return true;
    }

    releaseRenderCache();

    glGenFramebuffers(1, &cacheFramebuffer_);
    glGenTextures(1, &cacheTexture_);
    glBindTexture(GL_TEXTURE_2D, cacheTexture_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindFramebuffer(GL_FRAMEBUFFER, cacheFramebuffer_);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, cacheTexture_, 0);
    const bool complete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);
    resetStateCache();

    if (!complete) {
        releaseRenderCache();
        return false;
    }

    cacheWidth_ = width;
    cacheHeight_ = height;
    cacheRecreated_ = true;
    invalidateRenderCacheSync();
    return true;
}

bool OpenGLRenderBackend::renderCacheWasRecreated() const {
    return cacheRecreated_;
}

void OpenGLRenderBackend::releaseRenderCache() {
    makeCurrent();
    if (cacheTexture_ != 0) {
        glDeleteTextures(1, &cacheTexture_);
        cacheTexture_ = 0;
    }
    if (cacheFramebuffer_ != 0) {
        glDeleteFramebuffers(1, &cacheFramebuffer_);
        cacheFramebuffer_ = 0;
    }
    cacheWidth_ = 0;
    cacheHeight_ = 0;
    invalidateRenderCacheSync();
    resetStateCache();
}

void OpenGLRenderBackend::beginRenderCacheFrame(int width, int height, const std::vector<core::Rect>& repaintRects) {
    cacheRenderArea_ = fullRenderRect(width, height);
    std::vector<core::Rect> renderRects = mergeRenderRects(clampRenderRects(repaintRects, width, height));
    if (!renderRects.empty()) {
        core::Rect area = renderRects.front();
        for (std::size_t i = 1; i < renderRects.size(); ++i) {
            area = unionRenderRect(area, renderRects[i]);
        }
        cacheRenderArea_ = area;
    }
    core::render::RenderFrameStats& stats = core::render::currentRenderFrameStats();
    ++stats.backendRenderPasses;
    stats.backendRenderPassPixels += renderRectAreaPixels(cacheRenderArea_);
    glBindFramebuffer(GL_FRAMEBUFFER, cacheFramebuffer_);
    glViewport(0, 0, width, height);
}

void OpenGLRenderBackend::endRenderCacheFrame() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OpenGLRenderBackend::blitRenderCache(int width,
                                          int height,
                                          RenderCacheBlitMode mode,
                                          const std::vector<core::Rect>& dirtyRects) {
    std::vector<core::Rect> blitRects = resolveRenderCacheBlitRects(width, height, mode, dirtyRects);
    if (blitRects.empty()) {
        return;
    }
    core::render::RenderFrameStats& stats = core::render::currentRenderFrameStats();
    stats.cacheBlits += static_cast<int>(blitRects.size());
    stats.backendCopyRegions += static_cast<int>(blitRects.size());
    stats.blitPixels += renderRectAreaPixels(blitRects);

    setScissor(false, {}, height);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, cacheFramebuffer_);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    for (const core::Rect& rect : blitRects) {
        const GLint left = static_cast<GLint>(rect.x);
        const GLint right = static_cast<GLint>(rect.x + rect.width);
        const GLint top = static_cast<GLint>(rect.y);
        const GLint bottom = static_cast<GLint>(rect.y + rect.height);
        glBlitFramebuffer(left, height - bottom, right, height - top,
                          left, height - bottom, right, height - top,
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if (currentBackbuffer_ < backbufferCacheGenerations_.size()) {
        backbufferCacheGenerations_[currentBackbuffer_] = renderCacheGeneration_;
    }
}

std::vector<core::Rect> OpenGLRenderBackend::resolveRenderCacheBlitRects(int width,
                                                                         int height,
                                                                         RenderCacheBlitMode mode,
                                                                         const std::vector<core::Rect>& dirtyRects) {
    constexpr std::size_t kMaxBlitRects = 16;
    constexpr double kMaxBlitAreaRatio = 0.65;

    width = std::min(std::max(1, width), std::max(1, cacheWidth_));
    height = std::min(std::max(1, height), std::max(1, cacheHeight_));
    const core::Rect fullRect = fullRenderRect(width, height);
    auto useFull = [&] {
        return std::vector<core::Rect>{fullRect};
    };

    const bool cacheChanged = mode == RenderCacheBlitMode::Full || mode == RenderCacheBlitMode::Dirty;
    if (cacheChanged) {
        ++renderCacheGeneration_;
        const bool updateFull = mode == RenderCacheBlitMode::Full;
        const std::vector<core::Rect> updateRects = updateFull
            ? std::vector<core::Rect>{fullRect}
            : clampRenderRects(dirtyRects, width, height);
        recordRenderCacheBlitHistory(renderCacheGeneration_, updateFull || updateRects.empty(), updateRects);
    } else if (renderCacheGeneration_ == 0) {
        renderCacheGeneration_ = 1;
        recordRenderCacheBlitHistory(renderCacheGeneration_, true, {fullRect});
    }

    if (mode == RenderCacheBlitMode::Full || currentBackbuffer_ >= backbufferCacheGenerations_.size()) {
        return useFull();
    }

    if (platformRequiresConservativeBackbufferSync()) {
        return useFull();
    }

    const std::uint64_t syncedGeneration = backbufferCacheGenerations_[currentBackbuffer_];
    if (syncedGeneration == renderCacheGeneration_) {
        return {};
    }
    if (syncedGeneration == 0 || syncedGeneration > renderCacheGeneration_) {
        return useFull();
    }

    std::vector<core::Rect> requiredRects;
    for (std::uint64_t generation = syncedGeneration + 1; generation <= renderCacheGeneration_; ++generation) {
        const auto found = std::find_if(renderCacheHistory_.begin(), renderCacheHistory_.end(), [&](const RenderCacheHistoryEntry& entry) {
            return entry.generation == generation;
        });
        if (found == renderCacheHistory_.end() || found->full) {
            return useFull();
        }
        requiredRects.insert(requiredRects.end(), found->rects.begin(), found->rects.end());
    }

    requiredRects = mergeRenderRects(clampRenderRects(requiredRects, width, height));
    const double framePixels = static_cast<double>(std::max(1, width)) * static_cast<double>(std::max(1, height));
    if (requiredRects.empty() ||
        requiredRects.size() > kMaxBlitRects ||
        static_cast<double>(renderRectAreaPixels(requiredRects)) > framePixels * kMaxBlitAreaRatio) {
        return useFull();
    }
    return requiredRects;
}

void OpenGLRenderBackend::recordRenderCacheBlitHistory(std::uint64_t generation,
                                                       bool fullSync,
                                                       const std::vector<core::Rect>& rects) {
    constexpr std::size_t kMaxHistoryEntries = 32;
    RenderCacheHistoryEntry entry;
    entry.generation = generation;
    entry.full = fullSync;
    entry.rects = fullSync ? std::vector<core::Rect>{} : rects;
    renderCacheHistory_.push_back(std::move(entry));
    while (renderCacheHistory_.size() > kMaxHistoryEntries) {
        renderCacheHistory_.erase(renderCacheHistory_.begin());
    }
}

void OpenGLRenderBackend::invalidateRenderCacheSync() {
    renderCacheGeneration_ = 0;
    renderCacheHistory_.clear();
    backbufferCacheGenerations_.assign(2, 0);
    currentBackbuffer_ = 0;
}

void OpenGLRenderBackend::clear(const core::Color& color) {
    glClearColor(color.r, color.g, color.b, color.a);
    glClear(GL_COLOR_BUFFER_BIT);
}

void OpenGLRenderBackend::setScissor(bool enabled, const core::Rect& rect, int framebufferHeight) {
    if (!enabled) {
        if (!stateCacheValid_ || scissorEnabledState_) {
            glDisable(GL_SCISSOR_TEST);
            scissorEnabledState_ = false;
            stateCacheValid_ = true;
        }
        return;
    }

    const float left = std::floor(rect.x);
    const float right = std::ceil(rect.x + rect.width);
    const float top = std::floor(rect.y);
    const float bottom = std::ceil(rect.y + rect.height);
    const GLint x = static_cast<GLint>(left);
    const GLint y = static_cast<GLint>(std::floor(static_cast<float>(framebufferHeight) - bottom));
    const GLsizei width = static_cast<GLsizei>(std::max(1.0f, right - left));
    const GLsizei height = static_cast<GLsizei>(std::max(1.0f, bottom - top));
    const GLint safeY = std::max<GLint>(0, y);
    const GLsizei safeWidth = std::max<GLsizei>(1, width);
    const GLsizei safeHeight = std::max<GLsizei>(1, height);
    if (!stateCacheValid_ || !scissorEnabledState_) {
        glEnable(GL_SCISSOR_TEST);
        scissorEnabledState_ = true;
        stateCacheValid_ = true;
    }
    if (!stateCacheValid_ ||
        scissorX_ != x ||
        scissorY_ != safeY ||
        scissorWidth_ != safeWidth ||
        scissorHeight_ != safeHeight) {
        glScissor(x, safeY, safeWidth, safeHeight);
        scissorX_ = x;
        scissorY_ = safeY;
        scissorWidth_ = safeWidth;
        scissorHeight_ = safeHeight;
        stateCacheValid_ = true;
    }
}

} // namespace core::render::opengl
