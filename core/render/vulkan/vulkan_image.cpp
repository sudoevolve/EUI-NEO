#include "core/render/vulkan/vulkan_backend.h"
#include "core/render/vulkan/vulkan_image_shaders.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>

namespace core::render::vulkan {

namespace {

constexpr std::size_t kImageVertexFloatCapacity = 65536;

struct ImagePushConstants {
    float windowSize[4] = {};
    float tint[4] = {};
    float rect[4] = {};
    float flags[4] = {};
    float yuvMatrix0[4] = {};
    float yuvMatrix1[4] = {};
    float yuvMatrix2[4] = {};
    float yuvOffset[4] = {};
};

bool isVideoFormat(ImagePixelFormat format) {
    return format == ImagePixelFormat::NV12 ||
           format == ImagePixelFormat::I420 ||
           format == ImagePixelFormat::P010;
}

void writeYuvConversion(ImagePixelFormat format,
                        ImageColorSpace colorSpace,
                        ImageColorRange colorRange,
                        ImagePushConstants& constants) {
    const bool limited = colorRange == ImageColorRange::Limited;
    const bool p010 = format == ImagePixelFormat::P010;
    const float normalization = p010 ? 65535.0f : 255.0f;
    const float sampleScale = p010 ? 64.0f : 1.0f;
    const float maxCode = p010 ? 1023.0f : 255.0f;
    const float lumaRange = p010 ? 876.0f : 219.0f;
    const float chromaRange = p010 ? 896.0f : 224.0f;
    const float yScale = limited
        ? normalization / (lumaRange * sampleScale)
        : normalization / (maxCode * sampleScale);
    const float chromaScale = limited
        ? normalization / (chromaRange * sampleScale)
        : normalization / (maxCode * sampleScale);
    float rv = 1.5748f;
    float gu = 0.187324f;
    float gv = 0.468124f;
    float bu = 1.8556f;
    if (colorSpace == ImageColorSpace::BT601) {
        rv = 1.4020f;
        gu = 0.344136f;
        gv = 0.714136f;
        bu = 1.7720f;
    } else if (colorSpace == ImageColorSpace::BT2020) {
        rv = 1.4746f;
        gu = 0.16455f;
        gv = 0.57135f;
        bu = 1.8814f;
    }
    constants.yuvMatrix0[0] = yScale;
    constants.yuvMatrix0[2] = rv * chromaScale;
    constants.yuvMatrix1[0] = yScale;
    constants.yuvMatrix1[1] = -gu * chromaScale;
    constants.yuvMatrix1[2] = -gv * chromaScale;
    constants.yuvMatrix2[0] = yScale;
    constants.yuvMatrix2[1] = bu * chromaScale;
    constants.yuvOffset[0] = limited ? (p010 ? 4096.0f / normalization : 16.0f / 255.0f) : 0.0f;
    constants.yuvOffset[1] = p010 ? 32768.0f / normalization : 0.5f;
    constants.yuvOffset[2] = constants.yuvOffset[1];
}

} // namespace

VulkanRenderBackend::TextureHandle VulkanRenderBackend::createTexture(const unsigned char* pixels, int width, int height) {
    if (pixels == nullptr || width <= 0 || height <= 0 || !frameActive_) {
        return nullptr;
    }
    auto* texture = new TextureResource();
    if (!updateTexture(texture, pixels, width, height)) {
        delete texture;
        return nullptr;
    }
    return texture;
}

bool VulkanRenderBackend::updateTexture(TextureHandle handle, const unsigned char* pixels, int width, int height) {
    auto* texture = static_cast<TextureResource*>(handle);
    if (texture == nullptr || pixels == nullptr || width <= 0 || height <= 0 || !frameActive_) {
        return false;
    }
    if (renderPassActive_) {
        endActiveRenderPass();
    }

    constexpr VkFormat kTextureFormat = VK_FORMAT_R8G8B8A8_UNORM;
    if (texture->image == VK_NULL_HANDLE || texture->width != width ||
        texture->height != height || texture->channels != 4 ||
        texture->format != kTextureFormat) {
        destroyTextureResource(*texture);

        if (!createTargetImage(*texture,
                               width,
                               height,
                               kTextureFormat,
                               VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                   VK_IMAGE_USAGE_SAMPLED_BIT) ||
            !ensureTextureSampler(*texture)) {
            destroyTextureResource(*texture);
            return false;
        }
    }

    const VkDeviceSize uploadSize = static_cast<VkDeviceSize>(width) * static_cast<VkDeviceSize>(height) * 4u;
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceSize stagingOffset = 0;
    void* mapped = nullptr;
    if (!allocateUploadRegion(uploadSize, stagingBuffer, stagingOffset, mapped)) {
        return false;
    }
    std::memcpy(mapped, pixels, static_cast<std::size_t>(uploadSize));

    VkCommandBuffer commandBuffer = currentCommandBuffer();
    transitionImageLayout(commandBuffer, texture->image, texture->layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    texture->layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    VkBufferImageCopy copyRegion{};
    copyRegion.bufferOffset = stagingOffset;
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageExtent = {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), 1};
    vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    transitionImageLayout(commandBuffer, texture->image,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    texture->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    ++texture->generation;
    beginLoadPass();
    return true;
}

VulkanRenderBackend::TextureHandle VulkanRenderBackend::createDynamicTexture(const ImageFrame& frame) {
    if (!frameActive_ || !frame.valid() || !isVideoFormat(frame.format)) {
        return nullptr;
    }
    auto* texture = new VideoTextureResource();
    texture->kind = TextureKind::Video;
    if (!updateDynamicTexture(texture, frame)) {
        destroyVideoTextureResource(*texture);
        delete texture;
        return nullptr;
    }
    return texture;
}

bool VulkanRenderBackend::updateDynamicTexture(TextureHandle handle, const ImageFrame& frame) {
    auto* texture = static_cast<TextureResource*>(handle);
    if (texture == nullptr || texture->kind != TextureKind::Video || !frameActive_ ||
        !frame.valid() || !isVideoFormat(frame.format)) {
        return false;
    }
    if (renderPassActive_) {
        endActiveRenderPass();
    }

    auto& video = *static_cast<VideoTextureResource*>(texture);
    const int width = static_cast<int>(frame.width);
    const int height = static_cast<int>(frame.height);
    const int chromaWidth = static_cast<int>((frame.width + 1u) / 2u);
    const int chromaHeight = static_cast<int>((frame.height + 1u) / 2u);
    const bool p010 = frame.format == ImagePixelFormat::P010;
    const VkFormat singleFormat = p010 ? VK_FORMAT_R16_UNORM : VK_FORMAT_R8_UNORM;
    const VkFormat dualFormat = p010 ? VK_FORMAT_R16G16_UNORM : VK_FORMAT_R8G8_UNORM;
    const int sampleBytes = p010 ? 2 : 1;
    const bool layoutChanged = video.pixelFormat != frame.format ||
        video.width != width || video.height != height;
    if (layoutChanged) {
        destroyVideoTextureResource(video);
        video.kind = TextureKind::Video;
    }
    const bool planesSupported = frame.format == ImagePixelFormat::I420
        ? supportsVideoFormat(VK_FORMAT_R8_UNORM)
        : supportsVideoFormat(singleFormat) && supportsVideoFormat(dualFormat);
    if (!planesSupported ||
        !uploadTexturePlane(video, frame.pixels->data(), width, height, frame.stride,
                            sampleBytes, singleFormat)) {
        return false;
    }
    if (frame.format == ImagePixelFormat::I420) {
        if (!uploadTexturePlane(video.plane1, frame.plane1->data(), chromaWidth, chromaHeight,
                                frame.stride1, 1, VK_FORMAT_R8_UNORM) ||
            !uploadTexturePlane(video.plane2, frame.plane2->data(), chromaWidth, chromaHeight,
                                frame.stride2, 1, VK_FORMAT_R8_UNORM)) {
            return false;
        }
    } else if (!uploadTexturePlane(video.plane1, frame.plane1->data(), chromaWidth, chromaHeight,
                                   frame.stride1, sampleBytes * 2, dualFormat)) {
        return false;
    }
    if (frame.format != ImagePixelFormat::I420) {
        destroyTextureResource(video.plane2);
    }
    video.pixelFormat = frame.format;
    video.colorSpace = frame.colorSpace;
    video.colorRange = frame.colorRange;
    video.width = width;
    video.height = height;
    ++video.generation;
    beginLoadPass();
    return true;
}

bool VulkanRenderBackend::supportsVideoFormat(VkFormat format) const {
    if (physicalDevice_ == VK_NULL_HANDLE) {
        return false;
    }
    VkFormatProperties properties{};
    vkGetPhysicalDeviceFormatProperties(physicalDevice_, format, &properties);
    const VkFormatFeatureFlags required = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
                                          VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT |
                                          VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
    return (properties.optimalTilingFeatures & required) == required;
}

bool VulkanRenderBackend::uploadTexturePlane(TextureResource& texture,
                                             const std::uint8_t* pixels,
                                             int width,
                                             int height,
                                             std::uint32_t stride,
                                             int bytesPerPixel,
                                             VkFormat format) {
    if (pixels == nullptr || width <= 0 || height <= 0 || bytesPerPixel <= 0 || !frameActive_ ||
        stride < static_cast<std::uint32_t>(width * bytesPerPixel)) {
        return false;
    }
    if (texture.image == VK_NULL_HANDLE || texture.width != width || texture.height != height ||
        texture.format != format) {
        const TextureKind kind = texture.kind;
        destroyTextureResource(texture);
        texture.kind = kind;
        if (!createTargetImage(texture, width, height, format,
                               VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT) ||
            !ensureTextureSampler(texture)) {
            destroyTextureResource(texture);
            texture.kind = kind;
            return false;
        }
    }

    const VkDeviceSize uploadSize = static_cast<VkDeviceSize>(width) *
                                    static_cast<VkDeviceSize>(height) * bytesPerPixel;
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceSize stagingOffset = 0;
    void* mapped = nullptr;
    if (!allocateUploadRegion(uploadSize, stagingBuffer, stagingOffset, mapped)) {
        return false;
    }
    auto* destination = static_cast<std::uint8_t*>(mapped);
    const std::size_t rowBytes = static_cast<std::size_t>(width) * bytesPerPixel;
    for (int row = 0; row < height; ++row) {
        std::memcpy(destination + static_cast<std::size_t>(row) * rowBytes,
                    pixels + static_cast<std::size_t>(row) * stride,
                    rowBytes);
    }

    VkCommandBuffer commandBuffer = currentCommandBuffer();
    transitionImageLayout(commandBuffer, texture.image, texture.layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    texture.layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    VkBufferImageCopy copyRegion{};
    copyRegion.bufferOffset = stagingOffset;
    copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.layerCount = 1;
    copyRegion.imageExtent = {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height), 1};
    vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, texture.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
    transitionImageLayout(commandBuffer, texture.image,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    texture.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    ++texture.generation;
    return true;
}

void VulkanRenderBackend::destroyTexture(TextureHandle handle) {
    auto* texture = static_cast<TextureResource*>(handle);
    if (texture == nullptr) {
        return;
    }
    if (!frameActive_ && device_ != VK_NULL_HANDLE && inFlight_ != VK_NULL_HANDLE &&
        vkGetFenceStatus(device_, inFlight_) == VK_SUCCESS) {
        if (texture->kind == TextureKind::Video) {
            auto* video = static_cast<VideoTextureResource*>(texture);
            destroyVideoTextureResource(*video);
            delete video;
        } else {
            destroyTextureResource(*texture);
            delete texture;
        }
        return;
    }
    pendingTextureDeletes_.push_back(texture);
}

void VulkanRenderBackend::drawTexture(TextureHandle handle,
                                      const float* vertices,
                                      std::size_t vertexFloatCount,
                                      const core::Color& tint,
                                      const core::Rect& rect,
                                      float radius,
                                      float blur,
                                      int windowWidth,
                                      int windowHeight) {
    auto* texture = static_cast<TextureResource*>(handle);
    if (!frameActive_ || texture == nullptr || vertices == nullptr ||
        vertexFloatCount < 42 || tint.a <= 0.001f || windowWidth <= 0 || windowHeight <= 0) {
        return;
    }
    if (texture->kind == TextureKind::Rgba &&
        (texture->view == VK_NULL_HANDLE || texture->sampler == VK_NULL_HANDLE)) {
        return;
    }
    flushRoundedRectBatch();
    if (!ensureImagePipeline(false)) {
        return;
    }
    if (!frameRecorded_) {
        recordClearPass(clearColor_);
    }
    if (!renderPassActive_) {
        beginLoadPass();
    }
    if (!renderPassActive_ || !ensureImageDescriptor(*texture) || !ensureImageVertexBuffer()) {
        return;
    }

    if (imageVertices_.used + vertexFloatCount > imageVertices_.capacity) {
        return;
    }
    const std::size_t floatOffset = imageVertices_.used;
    auto* mappedImageVertices = static_cast<float*>(imageVertices_.mapped);
    std::memcpy(mappedImageVertices + floatOffset, vertices, vertexFloatCount * sizeof(float));
    imageVertices_.used += vertexFloatCount;

    VkCommandBuffer commandBuffer = currentCommandBuffer();
    if (!applyDrawViewportAndScissor(windowWidth, windowHeight)) {
        return;
    }

    ImagePushConstants constants{};
    constants.windowSize[0] = static_cast<float>(windowWidth);
    constants.windowSize[1] = static_cast<float>(windowHeight);
    writeColor(constants.tint, tint);
    constants.rect[0] = rect.x;
    constants.rect[1] = rect.y;
    constants.rect[2] = rect.width;
    constants.rect[3] = rect.height;
    constants.flags[0] = radius;
    constants.flags[1] = blur;
    if (texture->kind == TextureKind::Video) {
        const auto& video = *static_cast<const VideoTextureResource*>(texture);
        constants.flags[2] = video.pixelFormat == ImagePixelFormat::I420 ? 2.0f : 1.0f;
        writeYuvConversion(video.pixelFormat, video.colorSpace, video.colorRange, constants);
    }

    const VkDeviceSize offset = static_cast<VkDeviceSize>(floatOffset * sizeof(float));
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, imagePipeline_);
    vkCmdBindDescriptorSets(commandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            imagePipelineLayout_,
                            0,
                            1,
                            &texture->descriptorSet,
                            0,
                            nullptr);
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &imageVertices_.buffer, &offset);
    vkCmdPushConstants(commandBuffer,
                       imagePipelineLayout_,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0,
                       sizeof(constants),
                       &constants);
    vkCmdDraw(commandBuffer, static_cast<std::uint32_t>(vertexFloatCount / 7), 1, 0, 0);
}

VulkanRenderBackend::LayerHandle VulkanRenderBackend::createLayer(int width, int height) {
    if (width <= 0 || height <= 0) {
        return nullptr;
    }
    auto* layer = new LayerResource();
    if (!ensureLayerResource(*layer, width, height)) {
        delete layer;
        return nullptr;
    }
    layers_.push_back(layer);
    return layer;
}

bool VulkanRenderBackend::resizeLayer(LayerHandle handle, int width, int height) {
    auto* layer = static_cast<LayerResource*>(handle);
    return layer != nullptr && ensureLayerResource(*layer, width, height);
}

void VulkanRenderBackend::destroyLayer(LayerHandle handle) {
    auto* layer = static_cast<LayerResource*>(handle);
    if (layer == nullptr) {
        return;
    }
    if (device_ != VK_NULL_HANDLE && frameActive_) {
        endActiveRenderPass();
    } else if (device_ != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device_);
    }
    if (activeLayer_ == layer) {
        activeLayer_ = nullptr;
        renderTarget_ = RenderTarget::Swapchain;
        renderingToCache_ = false;
    }
    layers_.erase(std::remove(layers_.begin(), layers_.end(), layer), layers_.end());
    destroyLayerResource(*layer);
    delete layer;
}

bool VulkanRenderBackend::beginLayerFrame(LayerHandle handle, int width, int height) {
    auto* layer = static_cast<LayerResource*>(handle);
    if (!frameActive_ || layer == nullptr || !ensureLayerResource(*layer, width, height)) {
        return false;
    }

    endActiveRenderPass();
    previousLayerTarget_ = renderTarget_;
    renderTarget_ = RenderTarget::Layer;
    renderingToCache_ = false;
    activeLayer_ = layer;
    return true;
}

void VulkanRenderBackend::endLayerFrame() {
    if (renderTarget_ != RenderTarget::Layer || activeLayer_ == nullptr) {
        return;
    }
    endActiveRenderPass();
    transitionLayerImage(*activeLayer_, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    activeLayer_ = nullptr;
    renderTarget_ = previousLayerTarget_;
    renderingToCache_ = renderTarget_ == RenderTarget::RenderCache;
    previousLayerTarget_ = RenderTarget::Swapchain;
    beginLoadPass();
}

VulkanRenderBackend::TextureHandle VulkanRenderBackend::layerTexture(LayerHandle handle) {
    auto* layer = static_cast<LayerResource*>(handle);
    if (layer == nullptr || layer->texture.view == VK_NULL_HANDLE) {
        return nullptr;
    }
    return &layer->texture;
}

void VulkanRenderBackend::drawLayerTexture(TextureHandle handle,
                                           const float* vertices,
                                           std::size_t vertexFloatCount,
                                           const core::Rect& rect,
                                           int windowWidth,
                                           int windowHeight) {
    auto* texture = static_cast<TextureResource*>(handle);
    if (!frameActive_ || texture == nullptr || texture->view == VK_NULL_HANDLE || vertices == nullptr ||
        vertexFloatCount < 42 || windowWidth <= 0 || windowHeight <= 0) {
        return;
    }
    flushRoundedRectBatch();
    if (texture->layout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        endActiveRenderPass();
        transitionImageLayout(currentCommandBuffer(),
                              texture->image,
                              texture->layout,
                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        texture->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    if (!ensureImagePipeline(true)) {
        return;
    }
    if (!frameRecorded_) {
        recordClearPass(clearColor_);
    }
    if (!renderPassActive_) {
        beginLoadPass();
    }
    if (!renderPassActive_ || !ensureImageDescriptor(*texture) || !ensureImageVertexBuffer()) {
        return;
    }

    if (imageVertices_.used + vertexFloatCount > imageVertices_.capacity) {
        return;
    }
    const std::size_t floatOffset = imageVertices_.used;
    auto* mappedImageVertices = static_cast<float*>(imageVertices_.mapped);
    std::array<float, 42> layerVertices{};
    if (vertexFloatCount > layerVertices.size()) {
        return;
    }
    std::memcpy(layerVertices.data(), vertices, vertexFloatCount * sizeof(float));
    // Retained layers are color attachments; their V origin differs from uploaded image textures.
    for (std::size_t offset = 6; offset < vertexFloatCount; offset += 7) {
        layerVertices[offset] = 1.0f - layerVertices[offset];
    }
    std::memcpy(mappedImageVertices + floatOffset, layerVertices.data(), vertexFloatCount * sizeof(float));
    imageVertices_.used += vertexFloatCount;

    VkCommandBuffer commandBuffer = currentCommandBuffer();
    if (!applyDrawViewportAndScissor(windowWidth, windowHeight)) {
        return;
    }

    ImagePushConstants constants{};
    constants.windowSize[0] = static_cast<float>(windowWidth);
    constants.windowSize[1] = static_cast<float>(windowHeight);
    constants.tint[0] = 1.0f;
    constants.tint[1] = 1.0f;
    constants.tint[2] = 1.0f;
    constants.tint[3] = 1.0f;
    constants.rect[0] = rect.x;
    constants.rect[1] = rect.y;
    constants.rect[2] = rect.width;
    constants.rect[3] = rect.height;
    constants.flags[0] = 0.0f;

    const VkDeviceSize offset = static_cast<VkDeviceSize>(floatOffset * sizeof(float));
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, imagePremultipliedPipeline_);
    vkCmdBindDescriptorSets(commandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            imagePipelineLayout_,
                            0,
                            1,
                            &texture->descriptorSet,
                            0,
                            nullptr);
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &imageVertices_.buffer, &offset);
    vkCmdPushConstants(commandBuffer,
                       imagePipelineLayout_,
                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                       0,
                       sizeof(constants),
                       &constants);
    vkCmdDraw(commandBuffer, static_cast<std::uint32_t>(vertexFloatCount / 7), 1, 0, 0);
}

bool VulkanRenderBackend::ensureImagePipeline(bool premultipliedAlpha) {
    VkPipeline& targetPipeline = premultipliedAlpha ? imagePremultipliedPipeline_ : imagePipeline_;
    if (targetPipeline != VK_NULL_HANDLE) {
        return true;
    }
    if (device_ == VK_NULL_HANDLE || renderPass_ == VK_NULL_HANDLE) {
        return false;
    }
    if (imageDescriptorSetLayout_ == VK_NULL_HANDLE) {
        std::array<VkDescriptorSetLayoutBinding, 3> bindings{};
        for (std::uint32_t index = 0; index < bindings.size(); ++index) {
            bindings[index].binding = index;
            bindings[index].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            bindings[index].descriptorCount = 1;
            bindings[index].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        }

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();
        if (vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr, &imageDescriptorSetLayout_) != VK_SUCCESS) {
            return false;
        }
    }

    VkShaderModule vertexShader = createShaderModule(device_, shaders::kImageVertexSpirv, shaders::kImageVertexSpirvSize);
    VkShaderModule fragmentShader = createShaderModule(device_, shaders::kImageFragmentSpirv, shaders::kImageFragmentSpirvSize);
    if (vertexShader == VK_NULL_HANDLE || fragmentShader == VK_NULL_HANDLE) {
        if (vertexShader != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device_, vertexShader, nullptr);
        }
        if (fragmentShader != VK_NULL_HANDLE) {
            vkDestroyShaderModule(device_, fragmentShader, nullptr);
        }
        return false;
    }

    VkPipelineShaderStageCreateInfo shaderStages[2]{};
    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].module = vertexShader;
    shaderStages[0].pName = "main";
    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].module = fragmentShader;
    shaderStages[1].pName = "main";

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(float) * 7;
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 3> attributes{};
    attributes[0].binding = 0;
    attributes[0].location = 0;
    attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[0].offset = 0;
    attributes[1].binding = 0;
    attributes[1].location = 1;
    attributes[1].format = VK_FORMAT_R32G32_SFLOAT;
    attributes[1].offset = sizeof(float) * 3;
    attributes[2].binding = 0;
    attributes[2].location = 2;
    attributes[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributes[2].offset = sizeof(float) * 5;

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size());
    vertexInput.pVertexAttributeDescriptions = attributes.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = premultipliedAlpha ? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                          VK_COLOR_COMPONENT_G_BIT |
                                          VK_COLOR_COMPONENT_B_BIT |
                                          VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    std::array<VkDynamicState, 2> dynamicStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPushConstantRange pushConstant{};
    pushConstant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstant.offset = 0;
    pushConstant.size = sizeof(ImagePushConstants);

    if (imagePipelineLayout_ == VK_NULL_HANDLE) {
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &imageDescriptorSetLayout_;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstant;
        if (vkCreatePipelineLayout(device_, &pipelineLayoutInfo, nullptr, &imagePipelineLayout_) != VK_SUCCESS) {
            vkDestroyShaderModule(device_, fragmentShader, nullptr);
            vkDestroyShaderModule(device_, vertexShader, nullptr);
            return false;
        }
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = imagePipelineLayout_;
    pipelineInfo.renderPass = renderPass_;
    pipelineInfo.subpass = 0;

    const bool created = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &targetPipeline) == VK_SUCCESS;
    vkDestroyShaderModule(device_, fragmentShader, nullptr);
    vkDestroyShaderModule(device_, vertexShader, nullptr);
    if (!created) {
        if (premultipliedAlpha) {
            if (imagePremultipliedPipeline_ != VK_NULL_HANDLE) {
                vkDestroyPipeline(device_, imagePremultipliedPipeline_, nullptr);
                imagePremultipliedPipeline_ = VK_NULL_HANDLE;
            }
        } else {
            destroyImagePipeline();
        }
    }
    return created;
}

bool VulkanRenderBackend::ensureImageDescriptor(TextureResource& texture) {
    if (imageDescriptorSetLayout_ == VK_NULL_HANDLE) {
        return false;
    }
    if (imageDescriptorPool_ == VK_NULL_HANDLE || imageDescriptorPoolUsed_ >= imageDescriptorPoolCapacity_) {
        constexpr std::uint32_t kInitialImageDescriptorPoolSize = 16;
        const std::uint32_t nextCapacity = imageDescriptorPoolCapacity_ == 0
            ? kInitialImageDescriptorPoolSize
            : imageDescriptorPoolCapacity_ * 2;

        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = nextCapacity * 3;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets = nextCapacity;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        VkDescriptorPool pool = VK_NULL_HANDLE;
        if (vkCreateDescriptorPool(device_, &poolInfo, nullptr, &pool) != VK_SUCCESS) {
            return false;
        }
        imageDescriptorPools_.push_back(pool);
        imageDescriptorPool_ = pool;
        imageDescriptorPoolUsed_ = 0;
        imageDescriptorPoolCapacity_ = nextCapacity;
    }
    if (texture.descriptorSet == VK_NULL_HANDLE) {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = imageDescriptorPool_;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &imageDescriptorSetLayout_;
        if (vkAllocateDescriptorSets(device_, &allocInfo, &texture.descriptorSet) != VK_SUCCESS) {
            return false;
        }
        texture.descriptorPool = imageDescriptorPool_;
        ++imageDescriptorPoolUsed_;
    }

    const TextureResource* plane1 = &texture;
    const TextureResource* plane2 = &texture;
    if (texture.kind == TextureKind::Video) {
        const auto& video = *static_cast<const VideoTextureResource*>(&texture);
        if (video.plane1.view == VK_NULL_HANDLE || video.plane1.sampler == VK_NULL_HANDLE ||
            (video.pixelFormat == ImagePixelFormat::I420 &&
             (video.plane2.view == VK_NULL_HANDLE || video.plane2.sampler == VK_NULL_HANDLE))) {
            return false;
        }
        plane1 = &video.plane1;
        plane2 = video.pixelFormat == ImagePixelFormat::I420 ? &video.plane2 : &video.plane1;
    } else if (texture.view == VK_NULL_HANDLE || texture.sampler == VK_NULL_HANDLE) {
        return false;
    }
    std::array<VkDescriptorImageInfo, 3> imageInfos{};
    const std::array<const TextureResource*, 3> textures{&texture, plane1, plane2};
    std::array<VkWriteDescriptorSet, 3> writes{};
    for (std::uint32_t index = 0; index < writes.size(); ++index) {
        imageInfos[index].sampler = textures[index]->sampler;
        imageInfos[index].imageView = textures[index]->view;
        imageInfos[index].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        writes[index].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[index].dstSet = texture.descriptorSet;
        writes[index].dstBinding = index;
        writes[index].descriptorCount = 1;
        writes[index].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[index].pImageInfo = &imageInfos[index];
    }
    vkUpdateDescriptorSets(device_, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
    return true;
}

bool VulkanRenderBackend::ensureImageVertexBuffer() {
    if (imageVertices_.buffer == VK_NULL_HANDLE) {
        const VkDeviceSize size = sizeof(float) * kImageVertexFloatCapacity;
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(device_, &bufferInfo, nullptr, &imageVertices_.buffer) != VK_SUCCESS) {
            return false;
        }

        VkMemoryRequirements memoryRequirements{};
        vkGetBufferMemoryRequirements(device_, imageVertices_.buffer, &memoryRequirements);
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memoryRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memoryRequirements.memoryTypeBits,
                                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (allocInfo.memoryTypeIndex == std::numeric_limits<std::uint32_t>::max() ||
            vkAllocateMemory(device_, &allocInfo, nullptr, &imageVertices_.memory) != VK_SUCCESS ||
            vkBindBufferMemory(device_, imageVertices_.buffer, imageVertices_.memory, 0) != VK_SUCCESS ||
            vkMapMemory(device_, imageVertices_.memory, 0, size, 0, &imageVertices_.mapped) != VK_SUCCESS) {
            destroyImageResources();
            return false;
        }
        imageVertices_.capacity = kImageVertexFloatCapacity;
    }
    return imageVertices_.mapped != nullptr && imageVertices_.capacity >= 42;
}

void VulkanRenderBackend::destroyImagePipeline() {
    if (imagePipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, imagePipeline_, nullptr);
        imagePipeline_ = VK_NULL_HANDLE;
    }
    if (imagePremultipliedPipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, imagePremultipliedPipeline_, nullptr);
        imagePremultipliedPipeline_ = VK_NULL_HANDLE;
    }
    if (imagePipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, imagePipelineLayout_, nullptr);
        imagePipelineLayout_ = VK_NULL_HANDLE;
    }
}

void VulkanRenderBackend::destroyImageResources() {
    if (device_ == VK_NULL_HANDLE) {
        imageVertices_.buffer = VK_NULL_HANDLE;
        imageVertices_.memory = VK_NULL_HANDLE;
        imageVertices_.mapped = nullptr;
        imageVertices_.capacity = 0;
        imageVertices_.used = 0;
        return;
    }
    if (imageVertices_.memory != VK_NULL_HANDLE && imageVertices_.mapped != nullptr) {
        vkUnmapMemory(device_, imageVertices_.memory);
        imageVertices_.mapped = nullptr;
    }
    if (imageVertices_.buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_, imageVertices_.buffer, nullptr);
        imageVertices_.buffer = VK_NULL_HANDLE;
    }
    if (imageVertices_.memory != VK_NULL_HANDLE) {
        vkFreeMemory(device_, imageVertices_.memory, nullptr);
        imageVertices_.memory = VK_NULL_HANDLE;
    }
    imageVertices_.capacity = 0;
    imageVertices_.used = 0;
}

} // namespace core::render::vulkan
