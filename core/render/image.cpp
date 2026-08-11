#include "core/render/image.h"

#include "core/platform/platform.h"
#include "core/render/image_source.h"
#include "core/render/render_backend.h"
#include "core/window/window_backend.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace core {

namespace {

struct TextureRecord {
    render::RenderBackend::TextureHandle texture = nullptr;
    int width = 0;
    int height = 0;
    std::size_t byteCount = 0;
    std::size_t references = 0;
    std::uint64_t lastUse = 0;
};

struct TextureCache {
    std::unordered_map<std::string, TextureRecord> records;
    std::size_t totalBytes = 0;
    std::uint64_t useSerial = 0;
};

std::unordered_map<render::RenderBackend*, TextureCache> gTextureCaches;

constexpr std::size_t kMaxIdleTextureEntries = 32;
constexpr std::size_t kMaxTextureCacheBytes = 64u * 1024u * 1024u;
constexpr std::size_t kMaxRemoteTextureUploadsPerFrame = 2;
constexpr std::size_t kMaxRemoteTextureUploadBytesPerFrame = 8u * 1024u * 1024u;

struct TextureUploadBudget {
    std::size_t uploads = 0;
    std::size_t bytes = 0;
};

thread_local TextureUploadBudget gTextureUploadBudget;

bool reserveRemoteTextureUpload(std::size_t byteCount) {
    if (gTextureUploadBudget.uploads >= kMaxRemoteTextureUploadsPerFrame ||
        (gTextureUploadBudget.uploads > 0 &&
         gTextureUploadBudget.bytes + byteCount > kMaxRemoteTextureUploadBytesPerFrame)) {
        return false;
    }
    ++gTextureUploadBudget.uploads;
    gTextureUploadBudget.bytes += byteCount;
    return true;
}

void pruneIdleTextures(render::RenderBackend& backend, TextureCache& cache) {
    while (true) {
        std::size_t idleEntries = 0;
        auto oldest = cache.records.end();
        for (auto it = cache.records.begin(); it != cache.records.end(); ++it) {
            if (it->second.references != 0) {
                continue;
            }
            ++idleEntries;
            if (oldest == cache.records.end() || it->second.lastUse < oldest->second.lastUse) {
                oldest = it;
            }
        }

        if ((cache.totalBytes <= kMaxTextureCacheBytes && idleEntries <= kMaxIdleTextureEntries) ||
            oldest == cache.records.end()) {
            return;
        }
        if (oldest->second.texture != nullptr) {
            backend.destroyTexture(oldest->second.texture);
        }
        cache.totalBytes -= oldest->second.byteCount;
        cache.records.erase(oldest);
    }
}

int gifFrameDelayMs(const render::image::GifFrameData& frames, int frameIndex) {
    if (frames.delays.empty()) {
        return 100;
    }
    const std::size_t index = static_cast<std::size_t>(
        std::clamp(frameIndex, 0, static_cast<int>(frames.delays.size()) - 1));
    return std::max(10, frames.delays[index]);
}

} // namespace

struct ImagePrimitive::Impl {
    bool initialize() { return true; }
    void destroy();
    void setSource(const std::string& source) {
        source_ = source;
        svgKey_.clear();
        svgSource_.clear();
        textureUploadDeferred_ = false;
    }
    void setSvgSource(const std::string& key, const std::string& svg) {
        source_.clear();
        svgKey_ = key;
        svgSource_ = svg;
        textureUploadDeferred_ = false;
    }
    void setFlipVertically(bool value) { flipVertically_ = value; }
    void setBounds(float x, float y, float width, float height) { bounds_ = {x, y, width, height}; }
    void setTint(const Color& tint) { tint_ = tint; }
    void setCornerRadius(float radius) { radius_ = std::max(0.0f, radius); }
    void setBlur(float blur) { blur_ = std::max(0.0f, blur); }
    void setOpacity(float opacity) { opacity_ = std::clamp(opacity, 0.0f, 1.0f); }
    void setTransform(const Transform& transform) { transform_ = transform; hasTransformMatrix_ = false; }
    void setTransformMatrix(const TransformMatrix& matrix) { transformMatrix_ = matrix; hasTransformMatrix_ = true; }
    void setFit(ImageFit fit) { fit_ = fit; }
    void setCoverViewport(bool enabled, const Vec2& canvasSize, const Vec2& viewportOffset) {
        hasCoverViewport_ = enabled;
        coverViewportSize_ = canvasSize;
        coverViewportOffset_ = viewportOffset;
    }

    bool updateTexture();
    bool hasPendingLoad() const { return pendingLoad_; }
    bool isAnimating() const { return gifFrameCount_ > 1; }
    void render(int windowWidth, int windowHeight);

    static bool isSourceReady(const std::string& source) {
        return render::image::isSourceReady(source);
    }
    static bool hasSourceFailed(const std::string& source) {
        return render::image::hasSourceFailed(source);
    }
    static bool retrySource(const std::string& source) {
        return render::image::retrySource(source);
    }
    static bool consumeRemoteImageReady() { return render::image::consumeRemoteImageReady(); }
    static void releaseCachedTextures();

    void releaseTexture();
    bool ensureStaticPixels();
    static render::RenderBackend::TextureHandle acquireCachedTexture(render::RenderBackend& backend,
                                                                     const std::string& cacheKey,
                                                                     const unsigned char* pixels,
                                                                     int width,
                                                                     int height,
                                                                     bool throttleUpload,
                                                                     bool* uploadDeferred);
    static void releaseCachedTexture(render::RenderBackend& backend, const std::string& cacheKey);
    Vec3 transformPoint(float x, float y) const;
    void rebuildVertices(float* vertices) const;

    std::string source_;
    std::string svgKey_;
    std::string svgSource_;
    std::string loadedSource_;
    std::string loadedSvgKey_;
    std::string loadedSvgSource_;
    std::string loadedStaticPath_;
    bool flipVertically_ = false;
    bool loadedFlipVertically_ = false;
    bool pendingLoad_ = false;
    Rect bounds_;
    Color tint_ = {1.0f, 1.0f, 1.0f, 1.0f};
    float radius_ = 0.0f;
    float blur_ = 0.0f;
    float opacity_ = 1.0f;
    Transform transform_;
    TransformMatrix transformMatrix_;
    bool hasTransformMatrix_ = false;
    ImageFit fit_ = ImageFit::Cover;
    bool hasCoverViewport_ = false;
    Vec2 coverViewportSize_;
    Vec2 coverViewportOffset_;
    render::RenderBackend::TextureHandle texture_ = nullptr;
    render::RenderBackend* textureBackend_ = nullptr;
    int textureWidth_ = 0;
    int textureHeight_ = 0;
    bool textureDirty_ = false;
    bool textureUploadDeferred_ = false;
    bool staticContentLoaded_ = false;
    std::string desiredTextureCacheKey_;
    std::string loadedTextureCacheKey_;
    std::shared_ptr<const render::image::StaticImageData> staticImage_;
    std::shared_ptr<const render::image::GifFrameData> gifFrames_;
    std::string loadedGifPath_;
    bool loadedGifFlipVertically_ = false;
    int gifFrameCount_ = 0;
    int gifFrameIndex_ = 0;
    double gifNextFrameTime_ = 0.0;
};

void ImagePrimitive::Impl::destroy() {
    releaseTexture();
    staticImage_.reset();
    gifFrames_.reset();
    desiredTextureCacheKey_.clear();
    loadedTextureCacheKey_.clear();
    loadedGifPath_.clear();
    loadedSource_.clear();
    loadedSvgKey_.clear();
    loadedSvgSource_.clear();
    loadedStaticPath_.clear();
    staticContentLoaded_ = false;
    textureUploadDeferred_ = false;
}

bool ImagePrimitive::Impl::ensureStaticPixels() {
    if (staticImage_) {
        return true;
    }

    if (!loadedSvgKey_.empty() || !loadedSvgSource_.empty()) {
        staticImage_ = render::image::loadStaticSvg(loadedSvgKey_, loadedSvgSource_, loadedFlipVertically_);
    } else if (!loadedStaticPath_.empty()) {
        staticImage_ = render::image::loadStaticImageFromPath(loadedStaticPath_, loadedFlipVertically_);
    }
    if (!staticImage_) {
        return false;
    }
    textureWidth_ = staticImage_->width;
    textureHeight_ = staticImage_->height;
    return true;
}

bool ImagePrimitive::Impl::updateTexture() {
    bool changed = false;
    bool pending = false;
    if (!svgKey_.empty() || !svgSource_.empty()) {
        pendingLoad_ = false;
        if (loadedSvgKey_ == svgKey_ && loadedSvgSource_ == svgSource_ &&
            loadedFlipVertically_ == flipVertically_ && staticContentLoaded_) {
            return false;
        }

        std::shared_ptr<const render::image::StaticImageData> image =
            render::image::loadStaticSvg(svgKey_, svgSource_, flipVertically_);
        if (!image) {
            staticImage_.reset();
            staticContentLoaded_ = false;
            desiredTextureCacheKey_.clear();
            textureWidth_ = 0;
            textureHeight_ = 0;
            textureDirty_ = true;
            return false;
        }

        gifFrames_.reset();
        loadedGifPath_.clear();
        gifFrameCount_ = 0;
        staticImage_ = std::move(image);
        staticContentLoaded_ = true;
        textureWidth_ = staticImage_->width;
        textureHeight_ = staticImage_->height;
        loadedSource_.clear();
        loadedStaticPath_.clear();
        loadedSvgKey_ = svgKey_;
        loadedSvgSource_ = svgSource_;
        loadedFlipVertically_ = flipVertically_;
        desiredTextureCacheKey_ = "svg-string:" + svgKey_ + "#" +
                                  std::to_string(std::hash<std::string>{}(svgSource_)) +
                                  (flipVertically_ ? "#flip" : "#noflip");
        textureDirty_ = true;
        return true;
    }

    if (!loadedSvgKey_.empty() || !loadedSvgSource_.empty()) {
        loadedSvgKey_.clear();
        loadedSvgSource_.clear();
        staticImage_.reset();
        staticContentLoaded_ = false;
        desiredTextureCacheKey_.clear();
        textureDirty_ = true;
    }

    const std::string resolvedPath = render::image::resolveImagePath(source_, &pending);
    pendingLoad_ = pending;
    const bool isGif = !resolvedPath.empty() && render::image::isGifPath(resolvedPath);

    if (isGif) {
        if (loadedGifPath_ != resolvedPath || loadedGifFlipVertically_ != flipVertically_) {
            gifFrames_ = render::image::loadGifFrames(resolvedPath, flipVertically_);
            if (!gifFrames_) {
                return false;
            }
            staticImage_.reset();
            staticContentLoaded_ = false;
            loadedStaticPath_.clear();
            loadedGifPath_ = resolvedPath;
            loadedGifFlipVertically_ = flipVertically_;
            gifFrameCount_ = gifFrames_->frameCount;
            gifFrameIndex_ = 0;
            gifNextFrameTime_ = window::timeSeconds() + static_cast<double>(gifFrameDelayMs(*gifFrames_, 0)) / 1000.0;
            textureWidth_ = gifFrames_->width;
            textureHeight_ = gifFrames_->height;
            loadedSource_ = source_;
            loadedFlipVertically_ = flipVertically_;
            desiredTextureCacheKey_.clear();
            textureDirty_ = true;
            pendingLoad_ = false;
            changed = true;
        } else if (gifFrameCount_ > 1 && gifFrames_) {
            const double now = window::timeSeconds();
            if (now >= gifNextFrameTime_) {
                int guard = 0;
                do {
                    gifFrameIndex_ = (gifFrameIndex_ + 1) % gifFrameCount_;
                    gifNextFrameTime_ += static_cast<double>(gifFrameDelayMs(*gifFrames_, gifFrameIndex_)) / 1000.0;
                    ++guard;
                } while (now >= gifNextFrameTime_ && guard < gifFrameCount_);
                textureDirty_ = true;
                changed = true;
            }
        }
        return changed;
    }

    if (loadedSource_ == source_ && loadedFlipVertically_ == flipVertically_ && staticContentLoaded_) {
        const bool uploadDeferred = textureUploadDeferred_;
        textureUploadDeferred_ = false;
        return uploadDeferred;
    }

    if (resolvedPath.empty()) {
        pendingLoad_ = pending;
        if (source_.empty() || loadedSource_ != source_ || loadedFlipVertically_ != flipVertically_) {
            releaseTexture();
            staticImage_.reset();
            gifFrames_.reset();
            staticContentLoaded_ = false;
            loadedStaticPath_.clear();
            loadedGifPath_.clear();
            gifFrameCount_ = 0;
            desiredTextureCacheKey_.clear();
            loadedSource_.clear();
            textureWidth_ = 0;
            textureHeight_ = 0;
            textureDirty_ = true;
        }
        return false;
    }

    bool decodePending = false;
    std::shared_ptr<const render::image::StaticImageData> image =
        render::image::isRemoteSource(source_)
            ? render::image::requestStaticImageFromPath(resolvedPath, flipVertically_, &decodePending)
            : render::image::loadStaticImageFromPath(resolvedPath, flipVertically_);
    pendingLoad_ = pending || decodePending;
    if (!image) {
        staticImage_.reset();
        staticContentLoaded_ = false;
        loadedStaticPath_.clear();
        desiredTextureCacheKey_.clear();
        loadedSource_.clear();
        textureWidth_ = 0;
        textureHeight_ = 0;
        textureDirty_ = true;
        return false;
    }

    gifFrames_.reset();
    loadedGifPath_.clear();
    gifFrameCount_ = 0;
    staticImage_ = std::move(image);
    staticContentLoaded_ = true;
    textureWidth_ = staticImage_->width;
    textureHeight_ = staticImage_->height;
    loadedSource_ = source_;
    loadedStaticPath_ = resolvedPath;
    loadedFlipVertically_ = flipVertically_;
    desiredTextureCacheKey_ = render::image::imageCacheKey(resolvedPath, flipVertically_);
    textureDirty_ = true;
    pendingLoad_ = false;
    return true;
}

void ImagePrimitive::Impl::render(int windowWidth, int windowHeight) {
    if (bounds_.width <= 0.0f || bounds_.height <= 0.0f || opacity_ <= 0.001f) {
        return;
    }
    auto* backend = render::activeRenderBackend();
    if (backend == nullptr) {
        return;
    }
    if (texture_ != nullptr && textureBackend_ != backend) {
        releaseTexture();
    }

    const unsigned char* pixels = nullptr;
    if (gifFrames_ && gifFrameCount_ > 0) {
        const std::size_t frameBytes = static_cast<std::size_t>(gifFrames_->width) *
                                       static_cast<std::size_t>(gifFrames_->height) * 4u;
        pixels = gifFrames_->pixels.data() + frameBytes * static_cast<std::size_t>(gifFrameIndex_);
    } else if (staticImage_) {
        pixels = staticImage_->pixels.get();
    }

    const bool wantsCachedTexture = staticContentLoaded_ && !desiredTextureCacheKey_.empty();
    if (wantsCachedTexture) {
        const bool throttleUpload = render::image::isRemoteSource(loadedSource_);
        if (loadedTextureCacheKey_ != desiredTextureCacheKey_ || textureBackend_ != backend) {
            releaseTexture();
            texture_ = acquireCachedTexture(*backend, desiredTextureCacheKey_, pixels,
                                            textureWidth_, textureHeight_, throttleUpload,
                                            &textureUploadDeferred_);
            if (texture_ == nullptr && pixels == nullptr && ensureStaticPixels()) {
                pixels = staticImage_->pixels.get();
                texture_ = acquireCachedTexture(*backend, desiredTextureCacheKey_, pixels,
                                                textureWidth_, textureHeight_, throttleUpload,
                                                &textureUploadDeferred_);
            }
            if (texture_ != nullptr) {
                textureBackend_ = backend;
                loadedTextureCacheKey_ = desiredTextureCacheKey_;
                textureDirty_ = false;
                textureUploadDeferred_ = false;
            }
        }
        if (texture_ != nullptr) {
            staticImage_.reset();
        }
    } else {
        if (pixels == nullptr || textureWidth_ <= 0 || textureHeight_ <= 0) {
            releaseTexture();
            return;
        }
        if (!loadedTextureCacheKey_.empty()) {
            releaseTexture();
        }
        if (texture_ == nullptr) {
            texture_ = backend->createTexture(pixels, textureWidth_, textureHeight_);
            textureBackend_ = texture_ != nullptr ? backend : nullptr;
            textureDirty_ = texture_ == nullptr;
        } else if (textureDirty_) {
            if (backend->updateTexture(texture_, pixels, textureWidth_, textureHeight_)) {
                textureDirty_ = false;
            }
        }
    }
    if (texture_ == nullptr) {
        return;
    }

    float vertices[42] = {};
    rebuildVertices(vertices);
    Color tint = tint_;
    tint.a *= opacity_;
    backend->drawTexture(texture_, vertices, 42, tint, bounds_, radius_, blur_, windowWidth, windowHeight);
}

void ImagePrimitive::Impl::releaseTexture() {
    if (texture_ != nullptr) {
        render::RenderBackend* backend = textureBackend_ != nullptr ? textureBackend_ : render::activeRenderBackend();
        if (backend == nullptr) {
            return;
        }
        if (!loadedTextureCacheKey_.empty()) {
            releaseCachedTexture(*backend, loadedTextureCacheKey_);
        } else {
            backend->destroyTexture(texture_);
        }
        texture_ = nullptr;
    }
    textureBackend_ = nullptr;
    loadedTextureCacheKey_.clear();
    textureDirty_ = false;
}

render::RenderBackend::TextureHandle ImagePrimitive::Impl::acquireCachedTexture(render::RenderBackend& backend,
                                                                                 const std::string& cacheKey,
                                                                                 const unsigned char* pixels,
                                                                                 int width,
                                                                                 int height,
                                                                                 bool throttleUpload,
                                                                                 bool* uploadDeferred) {
    if (cacheKey.empty() || width <= 0 || height <= 0) {
        return nullptr;
    }
    TextureCache& cache = gTextureCaches[&backend];
    const auto cached = cache.records.find(cacheKey);
    if (cached != cache.records.end()) {
        ++cached->second.references;
        cached->second.lastUse = ++cache.useSerial;
        return cached->second.texture;
    }
    if (pixels == nullptr) {
        return nullptr;
    }

    const std::size_t byteCount = static_cast<std::size_t>(width) *
                                  static_cast<std::size_t>(height) * 4u;
    if (throttleUpload && !reserveRemoteTextureUpload(byteCount)) {
        if (uploadDeferred != nullptr) {
            *uploadDeferred = true;
        }
        core::platform::requestUiUpdate();
        return nullptr;
    }

    render::RenderBackend::TextureHandle texture = backend.createTexture(pixels, width, height);
    if (texture != nullptr) {
        cache.totalBytes += byteCount;
        cache.records[cacheKey] = {texture, width, height, byteCount, 1, ++cache.useSerial};
    }
    return texture;
}

void ImagePrimitive::Impl::releaseCachedTexture(render::RenderBackend& backend, const std::string& cacheKey) {
    const auto cacheIt = gTextureCaches.find(&backend);
    if (cacheIt == gTextureCaches.end()) {
        return;
    }
    TextureCache& cache = cacheIt->second;
    const auto cached = cache.records.find(cacheKey);
    if (cached == cache.records.end()) {
        return;
    }
    if (cached->second.references > 0) {
        --cached->second.references;
    }
    cached->second.lastUse = ++cache.useSerial;
    pruneIdleTextures(backend, cache);
    if (cache.records.empty()) {
        gTextureCaches.erase(cacheIt);
    }
}

void ImagePrimitive::Impl::releaseCachedTextures() {
    auto* backend = render::activeRenderBackend();
    if (backend == nullptr) {
        return;
    }
    const auto cacheIt = gTextureCaches.find(backend);
    if (cacheIt == gTextureCaches.end()) {
        return;
    }
    for (auto& item : cacheIt->second.records) {
        if (item.second.texture != nullptr) {
            backend->destroyTexture(item.second.texture);
        }
    }
    gTextureCaches.erase(cacheIt);
}

Vec3 ImagePrimitive::Impl::transformPoint(float x, float y) const {
    if (hasTransformMatrix_) {
        return core::transformPointWithW(transformMatrix_, x, y);
    }
    const Vec2 origin = {
        bounds_.x + bounds_.width * transform_.origin.x,
        bounds_.y + bounds_.height * transform_.origin.y
    };
    const float scaledX = (x - origin.x) * transform_.scale.x;
    const float scaledY = (y - origin.y) * transform_.scale.y;
    const float cosine = std::cos(transform_.rotate);
    const float sine = std::sin(transform_.rotate);
    return {
        origin.x + scaledX * cosine - scaledY * sine + transform_.translate.x,
        origin.y + scaledX * sine + scaledY * cosine + transform_.translate.y,
        1.0f
    };
}

void ImagePrimitive::Impl::rebuildVertices(float* vertices) const {
    Rect drawRect = bounds_;
    if (fit_ == ImageFit::Contain && textureWidth_ > 0 && textureHeight_ > 0 &&
        bounds_.width > 0.0f && bounds_.height > 0.0f) {
        const float rectAspect = bounds_.width / bounds_.height;
        const float imageAspect = static_cast<float>(textureWidth_) / static_cast<float>(textureHeight_);
        if (imageAspect > rectAspect) {
            drawRect.height = bounds_.width / imageAspect;
            drawRect.y = bounds_.y + (bounds_.height - drawRect.height) * 0.5f;
        } else if (imageAspect < rectAspect) {
            drawRect.width = bounds_.height * imageAspect;
            drawRect.x = bounds_.x + (bounds_.width - drawRect.width) * 0.5f;
        }
    }

    const Vec3 screen[4] = {
        transformPoint(drawRect.x, drawRect.y),
        transformPoint(drawRect.x + drawRect.width, drawRect.y),
        transformPoint(drawRect.x + drawRect.width, drawRect.y + drawRect.height),
        transformPoint(drawRect.x, drawRect.y + drawRect.height)
    };
    const Vec2 local[4] = {
        {drawRect.x, drawRect.y},
        {drawRect.x + drawRect.width, drawRect.y},
        {drawRect.x + drawRect.width, drawRect.y + drawRect.height},
        {drawRect.x, drawRect.y + drawRect.height}
    };
    float u0 = 0.0f;
    float v0 = 0.0f;
    float u1 = 1.0f;
    float v1 = 1.0f;
    if (fit_ != ImageFit::Stretch && textureWidth_ > 0 && textureHeight_ > 0 &&
        bounds_.width > 0.0f && bounds_.height > 0.0f) {
        const bool useCoverViewport = fit_ == ImageFit::Cover &&
                                      hasCoverViewport_ &&
                                      coverViewportSize_.x > 0.0f &&
                                      coverViewportSize_.y > 0.0f;
        const float sampleWidth = useCoverViewport ? coverViewportSize_.x : bounds_.width;
        const float sampleHeight = useCoverViewport ? coverViewportSize_.y : bounds_.height;
        const float rectAspect = sampleWidth / sampleHeight;
        const float imageAspect = static_cast<float>(textureWidth_) / static_cast<float>(textureHeight_);
        if (fit_ == ImageFit::Cover) {
            if (imageAspect > rectAspect) {
                const float visible = std::clamp(rectAspect / imageAspect, 0.0f, 1.0f);
                u0 = (1.0f - visible) * 0.5f;
                u1 = 1.0f - u0;
            } else if (imageAspect < rectAspect) {
                const float visible = std::clamp(imageAspect / rectAspect, 0.0f, 1.0f);
                v0 = (1.0f - visible) * 0.5f;
                v1 = 1.0f - v0;
            }
            if (useCoverViewport) {
                const float left = std::clamp(coverViewportOffset_.x / sampleWidth, 0.0f, 1.0f);
                const float top = std::clamp(coverViewportOffset_.y / sampleHeight, 0.0f, 1.0f);
                const float right = std::clamp((coverViewportOffset_.x + bounds_.width) / sampleWidth, left, 1.0f);
                const float bottom = std::clamp((coverViewportOffset_.y + bounds_.height) / sampleHeight, top, 1.0f);
                const float fullU0 = u0;
                const float fullV0 = v0;
                const float fullU1 = u1;
                const float fullV1 = v1;
                u0 = fullU0 + (fullU1 - fullU0) * left;
                u1 = fullU0 + (fullU1 - fullU0) * right;
                v0 = fullV0 + (fullV1 - fullV0) * top;
                v1 = fullV0 + (fullV1 - fullV0) * bottom;
            }
        }
    }
    const Vec2 uv[4] = {{u0, v0}, {u1, v0}, {u1, v1}, {u0, v1}};
    const int order[6] = {0, 1, 2, 0, 2, 3};
    for (int i = 0; i < 6; ++i) {
        const int index = order[i];
        const int offset = i * 7;
        vertices[offset + 0] = screen[index].x;
        vertices[offset + 1] = screen[index].y;
        vertices[offset + 2] = screen[index].z;
        vertices[offset + 3] = local[index].x;
        vertices[offset + 4] = local[index].y;
        vertices[offset + 5] = uv[index].x;
        vertices[offset + 6] = uv[index].y;
    }
}

ImagePrimitive::ImagePrimitive()
    : impl_(std::make_unique<Impl>()) {}

ImagePrimitive::~ImagePrimitive() = default;
ImagePrimitive::ImagePrimitive(ImagePrimitive&&) noexcept = default;
ImagePrimitive& ImagePrimitive::operator=(ImagePrimitive&&) noexcept = default;

bool ImagePrimitive::initialize() { return impl_->initialize(); }
void ImagePrimitive::destroy() { impl_->destroy(); }
void ImagePrimitive::setSource(const std::string& source) { impl_->setSource(source); }
void ImagePrimitive::setSvgSource(const std::string& key, const std::string& svg) { impl_->setSvgSource(key, svg); }
void ImagePrimitive::setFlipVertically(bool value) { impl_->setFlipVertically(value); }
void ImagePrimitive::setBounds(float x, float y, float width, float height) { impl_->setBounds(x, y, width, height); }
void ImagePrimitive::setTint(const Color& tint) { impl_->setTint(tint); }
void ImagePrimitive::setCornerRadius(float radius) { impl_->setCornerRadius(radius); }
void ImagePrimitive::setBlur(float blur) { impl_->setBlur(blur); }
void ImagePrimitive::setOpacity(float opacity) { impl_->setOpacity(opacity); }
void ImagePrimitive::setTransform(const Transform& transform) { impl_->setTransform(transform); }
void ImagePrimitive::setTransformMatrix(const TransformMatrix& matrix) { impl_->setTransformMatrix(matrix); }
void ImagePrimitive::setFit(ImageFit fit) { impl_->setFit(fit); }
void ImagePrimitive::setCoverViewport(bool enabled, const Vec2& canvasSize, const Vec2& viewportOffset) {
    impl_->setCoverViewport(enabled, canvasSize, viewportOffset);
}
bool ImagePrimitive::updateTexture() { return impl_->updateTexture(); }
bool ImagePrimitive::hasPendingLoad() const { return impl_->hasPendingLoad(); }
bool ImagePrimitive::isAnimating() const { return impl_->isAnimating(); }
void ImagePrimitive::render(int windowWidth, int windowHeight) { impl_->render(windowWidth, windowHeight); }
bool ImagePrimitive::isSourceReady(const std::string& source) { return Impl::isSourceReady(source); }
bool ImagePrimitive::hasSourceFailed(const std::string& source) { return Impl::hasSourceFailed(source); }
bool ImagePrimitive::retrySource(const std::string& source) { return Impl::retrySource(source); }
bool ImagePrimitive::consumeRemoteImageReady() { return Impl::consumeRemoteImageReady(); }
void ImagePrimitive::beginRenderFrame() { gTextureUploadBudget = {}; }
void ImagePrimitive::releaseCachedTextures() { Impl::releaseCachedTextures(); }

} // namespace core
