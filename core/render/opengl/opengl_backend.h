#pragma once

#include "core/render/render_backend.h"
#include "core/window/window_types.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace core::render::opengl {

class OpenGLRenderBackend final : public RenderBackend {
public:
    explicit OpenGLRenderBackend(core::window::Handle window, RenderBackend* shareContext = nullptr);
    ~OpenGLRenderBackend() override;

    OpenGLRenderBackend(const OpenGLRenderBackend&) = delete;
    OpenGLRenderBackend& operator=(const OpenGLRenderBackend&) = delete;

    bool initialize() override;
    bool valid() const override;

    void makeCurrent() override;
    void beginFrame(const RenderSurface& surface) override;
    void present() override;
    bool ensureRenderCache(int width, int height) override;
    bool renderCacheWasRecreated() const override;
    void releaseRenderCache() override;
    void beginRenderCacheFrame(int width,
                               int height,
                               const std::vector<core::Rect>& repaintRects = {}) override;
    void endRenderCacheFrame() override;
    void blitRenderCache(int width,
                         int height,
                         RenderCacheBlitMode mode = RenderCacheBlitMode::Full,
                         const std::vector<core::Rect>& dirtyRects = {}) override;
    LayerHandle createLayer(int width, int height) override;
    bool resizeLayer(LayerHandle layer, int width, int height) override;
    void destroyLayer(LayerHandle layer) override;
    bool beginLayerFrame(LayerHandle layer, int width, int height) override;
    void endLayerFrame() override;
    TextureHandle layerTexture(LayerHandle layer) override;
    void clear(const core::Color& color) override;
    void setScissor(bool enabled, const core::Rect& rect, int framebufferHeight) override;
    void prepareBackdropBlur(const core::Rect& bounds, float blur, int windowWidth, int windowHeight) override;
    void drawRoundedRect(const RoundedRectDrawCommand& command, int windowWidth, int windowHeight) override;
    void drawPolygon(const PolygonDrawCommand& command, int windowWidth, int windowHeight) override;
    void drawText(const TextDrawCommand& command, int windowWidth, int windowHeight) override;
    TextureHandle createTexture(const unsigned char* pixels, int width, int height) override;
    bool updateTexture(TextureHandle handle, const unsigned char* pixels, int width, int height) override;
    void destroyTexture(TextureHandle handle) override;
    void drawTexture(TextureHandle handle,
                     const float* vertices,
                     std::size_t vertexFloatCount,
                     const core::Color& tint,
                     const core::Rect& rect,
                     float radius,
                     float blur,
                     int windowWidth,
                     int windowHeight) override;
    void drawLayerTexture(TextureHandle handle,
                          const float* vertices,
                          std::size_t vertexFloatCount,
                          const core::Rect& rect,
                          int windowWidth,
                          int windowHeight) override;
    ShaderToyHandle createShaderToy(const ShaderToyGraph& graph, ShaderToyError* error) override;
    TextureHandle renderShaderToy(ShaderToyHandle handle,
                                  const ShaderToyGraph& graph,
                                  int width,
                                  int height,
                                  const ShaderToyFrameData& frame,
                                  bool paused,
                                  bool reset,
                                  ShaderToyError* error) override;
    void destroyShaderToy(ShaderToyHandle handle) override;
    bool readShaderToyPixel(ShaderToyHandle handle, float* rgba) override;
    bool readShaderToyPixels(ShaderToyHandle handle,
                             float* rgba,
                             std::size_t floatCount) override;

private:
    void flushRoundedRectBatch();
    void releaseShaderToys();
    void releasePrimitiveResources();
    void releasePolygonResources();
    void releaseTextResources();
    bool ensureImageResources();
    unsigned int compileImageShader(unsigned int type, const char* source) const;
    void releaseImageResources();
    void resetStateCache();
    void useProgram(unsigned int program);
    void bindVertexArray(unsigned int vao);
    void bindArrayBuffer(unsigned int buffer);
    void activeTextureUnit(unsigned int unit);
    void bindTexture2D(unsigned int texture);
    void setBlendEnabled(bool enabled);
    void setStandardAlphaBlend();
    void setPremultipliedAlphaBlend();
    std::vector<core::Rect> resolveRenderCacheBlitRects(int width,
                                                        int height,
                                                        RenderCacheBlitMode mode,
                                                        const std::vector<core::Rect>& dirtyRects);
    void recordRenderCacheBlitHistory(std::uint64_t generation, bool fullSync, const std::vector<core::Rect>& rects);
    void invalidateRenderCacheSync();

    core::window::Handle window_ = nullptr;
    RenderBackend* shareContext_ = nullptr;
    void* context_ = nullptr;
    bool initialized_ = false;
    bool damagePresentSupported_ = false;
    unsigned int cacheFramebuffer_ = 0;
    unsigned int cacheTexture_ = 0;
    int cacheWidth_ = 0;
    int cacheHeight_ = 0;
    int cacheCapacityWidth_ = 0;
    int cacheCapacityHeight_ = 0;
    bool cacheRecreated_ = false;
    int framebufferWidth_ = 0;
    int framebufferHeight_ = 0;
    int layerPreviousFramebuffer_ = 0;
    int layerPreviousViewport_[4] = {};
    bool layerFrameActive_ = false;
    core::Rect cacheRenderArea_{};
    struct RenderCacheHistoryEntry {
        std::uint64_t generation = 0;
        bool full = false;
        std::vector<core::Rect> rects;
    };
    std::uint64_t renderCacheGeneration_ = 0;
    std::vector<std::uint64_t> backbufferCacheGenerations_{0, 0};
    std::vector<RenderCacheHistoryEntry> renderCacheHistory_;
    std::vector<ShaderToyHandle> shaderToys_;
    std::vector<float> roundedRectBatchVertices_;
    int roundedRectBatchWindowWidth_ = 0;
    int roundedRectBatchWindowHeight_ = 0;
    std::size_t currentBackbuffer_ = 0;
    unsigned int imageVao_ = 0;
    unsigned int imageVbo_ = 0;
    unsigned int imageShaderProgram_ = 0;
    int imageWindowSizeLocation_ = -1;
    int imageTextureLocation_ = -1;
    int imageTintLocation_ = -1;
    int imageRectLocation_ = -1;
    int imageRadiusLocation_ = -1;
    int imageBlurLocation_ = -1;
    bool programStateValid_ = false;
    bool vaoStateValid_ = false;
    bool arrayBufferStateValid_ = false;
    bool textureUnitStateValid_ = false;
    bool texture2DStateValid_[8] = {};
    bool blendEnabledStateValid_ = false;
    bool blendFunctionStateValid_ = false;
    bool scissorEnabledStateValid_ = false;
    bool scissorRectStateValid_ = false;
    bool blendEnabled_ = false;
    bool alphaBlendSet_ = false;
    bool scissorEnabledState_ = false;
    int scissorX_ = 0;
    int scissorY_ = 0;
    int scissorWidth_ = 0;
    int scissorHeight_ = 0;
    unsigned int currentProgram_ = 0;
    unsigned int currentVao_ = 0;
    unsigned int currentArrayBuffer_ = 0;
    unsigned int currentTextureUnit_ = 0;
    unsigned int currentTexture2D_[8] = {};
};

} // namespace core::render::opengl
