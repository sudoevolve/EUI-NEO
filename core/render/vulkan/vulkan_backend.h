#pragma once

#include "core/render/render_backend.h"
#include "core/render/render_types.h"

#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace core::render::vulkan {

class VulkanRenderBackend final : public RenderBackend {
public:
    explicit VulkanRenderBackend(core::window::Handle window, RenderBackend* shareBackend = nullptr);
    ~VulkanRenderBackend() override;

    VulkanRenderBackend(const VulkanRenderBackend&) = delete;
    VulkanRenderBackend& operator=(const VulkanRenderBackend&) = delete;

    bool initialize() override;
    bool valid() const override;
    void makeCurrent() override;
    void resetSurface() override;
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
                     int windowWidth,
                     int windowHeight) override;
    LayerHandle createLayer(int width, int height) override;
    bool resizeLayer(LayerHandle layer, int width, int height) override;
    void destroyLayer(LayerHandle layer) override;
    bool beginLayerFrame(LayerHandle layer, int width, int height) override;
    void endLayerFrame() override;
    TextureHandle layerTexture(LayerHandle layer) override;
    void drawLayerTexture(TextureHandle handle,
                          const float* vertices,
                          std::size_t vertexFloatCount,
                          const core::Rect& rect,
                          int windowWidth,
                          int windowHeight) override;

private:
    struct TextureResource {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkFormat format = VK_FORMAT_UNDEFINED;
        int width = 0;
        int height = 0;
        int channels = 0;
        std::uint64_t generation = 0;
    };

    struct LayerResource {
        TextureResource texture;
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        VkExtent2D extent{};
    };

    enum class RenderTarget {
        Swapchain,
        RenderCache,
        Layer
    };

    struct MappedBuffer {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        void* mapped = nullptr;
        std::size_t capacity = 0;
        std::size_t used = 0;
    };

    struct UploadArena {
        VkBuffer buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        void* mapped = nullptr;
        VkDeviceSize capacity = 0;
        VkDeviceSize used = 0;
        std::vector<VkBuffer> pendingBuffers;
        std::vector<VkDeviceMemory> pendingMemories;
    };

    struct RenderCacheHistoryEntry {
        std::uint64_t generation = 0;
        bool full = false;
        std::vector<core::Rect> rects;
    };

    bool createInstance();
    bool createSurface();
    bool pickDevice();
    bool createDevice();
    bool recreateSwapchain(const RenderSurface& surface);
    void destroySwapchain();
    void destroy();
    void recordClearPass(const core::Color& color);
    void beginLoadPass();
    bool createTargetImage(TextureResource& texture,
                           int width,
                           int height,
                           VkFormat format,
                           VkImageUsageFlags usage);
    bool ensureTextureSampler(TextureResource& texture);
    bool ensureLayerResource(LayerResource& layer, int width, int height);
    void destroyLayerResource(LayerResource& layer);
    void releaseAllLayerFramebuffers();
    void releaseAllLayerResources();
    void destroyRenderCacheResources();
    void transitionRenderCacheImage(VkImageLayout newLayout);
    void transitionLayerImage(LayerResource& layer, VkImageLayout newLayout);
    bool ensureRenderCacheResolvePipeline();
    bool ensureRenderCacheResolveDescriptor();
    void destroyRenderCacheResolvePipeline();
    void destroyRenderCacheResolveResources();
    bool drawRenderCacheResolve(int width, int height);
    std::vector<core::Rect> resolveRenderCacheBlitRects(int width,
                                                        int height,
                                                        RenderCacheBlitMode mode,
                                                        const std::vector<core::Rect>& dirtyRects);
    void recordRenderCacheBlitHistory(std::uint64_t generation, bool fullSync, const std::vector<core::Rect>& rects);
    void invalidateRenderCacheSync();
    void setPresentDirtyRects(const std::vector<core::Rect>& rects);
    VkExtent2D currentRenderExtent() const;
    VkRect2D currentRenderArea() const;
    VkFramebuffer currentFramebuffer() const;
    VkImage currentRenderImage() const;
    VkImageLayout currentRenderImageLayout() const;
    void setCurrentRenderImageLayout(VkImageLayout layout);
    VkCommandBuffer currentCommandBuffer() const;
    bool hasCurrentCommandBuffer() const;
    void endActiveRenderPass();
    bool applyDrawViewportAndScissor(int windowWidth, int windowHeight);
    void writeColor(float (&target)[4], const core::Color& color);
    VkShaderModule createShaderModule(VkDevice device, const std::uint32_t* code, std::size_t codeSize);
    void transitionImageLayout(VkCommandBuffer commandBuffer,
                               VkImage image,
                               VkImageLayout oldLayout,
                               VkImageLayout newLayout);
    VkRect2D clampScissor(const core::Rect& rect, int windowWidth, int windowHeight) const;
    bool ensureRoundedRectPipeline();
    bool ensurePolygonPipeline();
    bool ensurePolygonEdgeBuffer(std::size_t edgeCount);
    bool ensureBackdropResources(std::uint32_t width, std::uint32_t height);
    bool ensureBackdropDescriptor();
    void initializeBackdropImageIfNeeded();
    bool ensurePrimitiveVertexBuffer(std::size_t vertexCount);
    bool ensureTextPipeline();
    bool textAtlasNeedsUpload(const TextAtlasPageData& page) const;
    bool ensureTextAtlas(const TextAtlasPageData& page);
    bool ensureTextDescriptor();
    bool ensureTextVertexBuffer(std::size_t floatCount);
    bool ensureImagePipeline(bool premultipliedAlpha = false);
    bool ensureImageDescriptor(TextureResource& texture);
    bool ensureImageVertexBuffer();
    bool allocateUploadRegion(VkDeviceSize size, VkBuffer& buffer, VkDeviceSize& offset, void*& mapped);
    bool createUploadBuffer(VkDeviceSize capacity);
    void destroyUploadBuffer();
    void destroyRoundedRectPipeline();
    void destroyPolygonPipeline();
    void destroyPolygonEdgeBuffer();
    void destroyBackdropResources();
    void destroyBackdropDescriptorPool();
    void destroyPrimitiveVertexBuffer();
    void destroyTextPipeline();
    void destroyTextResources();
    void destroyImagePipeline();
    void destroyImageResources();
    void destroyTextureResource(TextureResource& texture);
    void releasePendingTextureDeletes();
    void releasePendingUploads();
    void transitionSwapchainImage(VkImageLayout newLayout);
    std::uint32_t findMemoryType(std::uint32_t filter, VkMemoryPropertyFlags properties) const;

    core::window::Handle window_ = nullptr;
    VkInstance instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
    VkQueue presentQueue_ = VK_NULL_HANDLE;
    std::uint32_t graphicsFamily_ = 0;
    std::uint32_t presentFamily_ = 0;
    bool initialized_ = false;

    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapchainFormat_ = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent_{};
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;
    std::vector<VkImageLayout> swapchainImageLayouts_;
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers_;
    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers_;
    VkSemaphore imageAvailable_ = VK_NULL_HANDLE;
    VkSemaphore renderFinished_ = VK_NULL_HANDLE;
    VkFence inFlight_ = VK_NULL_HANDLE;
    std::uint32_t currentImage_ = 0;
    bool frameActive_ = false;
    bool frameRecorded_ = false;
    bool renderPassActive_ = false;
    bool scissorEnabled_ = false;
    bool swapchainTransferSrcSupported_ = false;
    bool swapchainTransferDstSupported_ = false;
    bool incrementalPresentSupported_ = false;
    bool backdropReady_ = false;
    RenderTarget renderTarget_ = RenderTarget::Swapchain;
    RenderTarget previousLayerTarget_ = RenderTarget::Swapchain;
    LayerResource* activeLayer_ = nullptr;
    core::Rect scissorRect_{};
    core::Rect cacheRenderArea_{};
    core::Color clearColor_{0.0f, 0.0f, 0.0f, 1.0f};

    VkDescriptorSetLayout roundedRectDescriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool roundedRectDescriptorPool_ = VK_NULL_HANDLE;
    VkDescriptorSet roundedRectDescriptorSet_ = VK_NULL_HANDLE;
    VkPipelineLayout roundedRectPipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline roundedRectPipeline_ = VK_NULL_HANDLE;
    VkImage backdropImage_ = VK_NULL_HANDLE;
    VkDeviceMemory backdropImageMemory_ = VK_NULL_HANDLE;
    VkImageView backdropImageView_ = VK_NULL_HANDLE;
    VkSampler backdropSampler_ = VK_NULL_HANDLE;
    VkImageLayout backdropImageLayout_ = VK_IMAGE_LAYOUT_UNDEFINED;
    VkExtent2D backdropExtent_{};
    MappedBuffer primitiveVertices_;
    VkDescriptorSetLayout polygonDescriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool polygonDescriptorPool_ = VK_NULL_HANDLE;
    VkDescriptorSet polygonDescriptorSet_ = VK_NULL_HANDLE;
    VkPipelineLayout polygonPipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline polygonPipeline_ = VK_NULL_HANDLE;
    MappedBuffer polygonEdges_;

    VkImage renderCacheImage_ = VK_NULL_HANDLE;
    VkDeviceMemory renderCacheMemory_ = VK_NULL_HANDLE;
    VkImageView renderCacheView_ = VK_NULL_HANDLE;
    VkFramebuffer renderCacheFramebuffer_ = VK_NULL_HANDLE;
    VkImageLayout renderCacheLayout_ = VK_IMAGE_LAYOUT_UNDEFINED;
    VkExtent2D renderCacheExtent_{};
    bool renderCacheRecreated_ = false;
    bool renderingToCache_ = false;
    std::uint64_t renderCacheGeneration_ = 0;
    std::vector<std::uint64_t> swapchainImageCacheGenerations_;
    std::vector<RenderCacheHistoryEntry> renderCacheHistory_;
    std::vector<core::Rect> presentDirtyRects_;
    VkDescriptorSetLayout renderCacheResolveDescriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool renderCacheResolveDescriptorPool_ = VK_NULL_HANDLE;
    VkDescriptorSet renderCacheResolveDescriptorSet_ = VK_NULL_HANDLE;
    VkPipelineLayout renderCacheResolvePipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline renderCacheResolvePipeline_ = VK_NULL_HANDLE;
    VkSampler renderCacheResolveSampler_ = VK_NULL_HANDLE;

    VkDescriptorSetLayout textDescriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool textDescriptorPool_ = VK_NULL_HANDLE;
    VkDescriptorSet textDescriptorSet_ = VK_NULL_HANDLE;
    VkPipelineLayout textPipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline textPipeline_ = VK_NULL_HANDLE;
    TextureResource textGrayAtlas_;
    TextureResource textColorAtlas_;
    bool textDescriptorDirty_ = true;
    MappedBuffer textVertices_;
    VkDescriptorSetLayout imageDescriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool imageDescriptorPool_ = VK_NULL_HANDLE;
    std::vector<VkDescriptorPool> imageDescriptorPools_;
    std::uint32_t imageDescriptorPoolUsed_ = 0;
    std::uint32_t imageDescriptorPoolCapacity_ = 0;
    VkPipelineLayout imagePipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline imagePipeline_ = VK_NULL_HANDLE;
    VkPipeline imagePremultipliedPipeline_ = VK_NULL_HANDLE;
    MappedBuffer imageVertices_;
    UploadArena uploadArena_;
    std::vector<TextureResource*> pendingTextureDeletes_;
    std::vector<LayerResource*> layers_;

};

} // namespace core::render::vulkan
