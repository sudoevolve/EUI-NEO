#include "core/render/video.h"

#include "core/render/render_backend.h"
#include "core/render/video_source.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace core {

struct VideoPrimitive::Impl {
    bool initialize() { return true; }
    void destroy();
    void setSource(const std::string& source) { source_ = source; }
    void setBounds(float x, float y, float width, float height) { bounds_ = {x, y, width, height}; }
    void setTint(const Color& tint) { tint_ = tint; }
    void setCornerRadius(float radius) { radius_ = std::max(0.0f, radius); }
    void setOpacity(float opacity) { opacity_ = std::clamp(opacity, 0.0f, 1.0f); }
    void setTransform(const Transform& transform) { transform_ = transform; hasTransformMatrix_ = false; }
    void setTransformMatrix(const TransformMatrix& matrix) { transformMatrix_ = matrix; hasTransformMatrix_ = true; }
    void setFit(ImageFit fit) { fit_ = fit; }
    void setCoverViewport(bool enabled, const Vec2& canvasSize, const Vec2& viewportOffset) {
        hasCoverViewport_ = enabled;
        coverViewportSize_ = canvasSize;
        coverViewportOffset_ = viewportOffset;
    }

    bool updateFrame();
    bool hasFrame() const { return pixels_ != nullptr && textureWidth_ > 0 && textureHeight_ > 0; }
    void render(int windowWidth, int windowHeight);
    void releaseTexture();
    Vec3 transformPoint(float x, float y) const;
    void rebuildVertices(float* vertices) const;

    std::string source_;
    Rect bounds_;
    Color tint_ = {1.0f, 1.0f, 1.0f, 1.0f};
    float radius_ = 0.0f;
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
    std::uint64_t loadedGeneration_ = 0;
    std::shared_ptr<const std::vector<unsigned char>> pixels_;
};

void VideoPrimitive::Impl::destroy() {
    releaseTexture();
    pixels_.reset();
    textureWidth_ = 0;
    textureHeight_ = 0;
    loadedGeneration_ = 0;
    textureDirty_ = false;
}

bool VideoPrimitive::Impl::updateFrame() {
    if (source_.empty()) {
        if (pixels_ || textureWidth_ > 0 || textureHeight_ > 0 || loadedGeneration_ != 0) {
            pixels_.reset();
            textureWidth_ = 0;
            textureHeight_ = 0;
            loadedGeneration_ = 0;
            textureDirty_ = true;
            return true;
        }
        return false;
    }

    core::video::Frame frame;
    if (!core::video::latestFrame(source_, frame)) {
        if (pixels_ || textureWidth_ > 0 || textureHeight_ > 0 || loadedGeneration_ != 0) {
            pixels_.reset();
            textureWidth_ = 0;
            textureHeight_ = 0;
            loadedGeneration_ = 0;
            textureDirty_ = true;
            return true;
        }
        return false;
    }
    if (frame.format != core::video::PixelFormat::Rgba8 ||
        !frame.pixels ||
        frame.width <= 0 ||
        frame.height <= 0 ||
        frame.stride != frame.width * 4) {
        if (pixels_ || textureWidth_ > 0 || textureHeight_ > 0 || loadedGeneration_ != 0) {
            pixels_.reset();
            textureWidth_ = 0;
            textureHeight_ = 0;
            loadedGeneration_ = 0;
            textureDirty_ = true;
            return true;
        }
        return false;
    }
    if (frame.generation == loadedGeneration_) {
        return false;
    }

    pixels_ = std::move(frame.pixels);
    textureWidth_ = frame.width;
    textureHeight_ = frame.height;
    loadedGeneration_ = frame.generation;
    textureDirty_ = true;
    return true;
}

void VideoPrimitive::Impl::render(int windowWidth, int windowHeight) {
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
    if (!pixels_) {
        releaseTexture();
        return;
    }
    if (textureWidth_ <= 0 || textureHeight_ <= 0 || pixels_->empty()) {
        releaseTexture();
        return;
    }

    const unsigned char* pixels = pixels_->data();
    if (texture_ == nullptr) {
        texture_ = backend->createTexture(pixels, textureWidth_, textureHeight_);
        textureBackend_ = texture_ != nullptr ? backend : nullptr;
        textureDirty_ = texture_ == nullptr;
    } else if (textureDirty_) {
        if (backend->updateTexture(texture_, pixels, textureWidth_, textureHeight_)) {
            textureDirty_ = false;
        }
    }
    if (texture_ == nullptr) {
        return;
    }

    float vertices[42] = {};
    rebuildVertices(vertices);
    Color tint = tint_;
    tint.a *= opacity_;
    backend->drawTexture(texture_, vertices, 42, tint, bounds_, radius_, windowWidth, windowHeight);
}

void VideoPrimitive::Impl::releaseTexture() {
    if (texture_ != nullptr) {
        render::RenderBackend* backend = textureBackend_ != nullptr ? textureBackend_ : render::activeRenderBackend();
        if (backend == nullptr) {
            return;
        }
        backend->destroyTexture(texture_);
        texture_ = nullptr;
    }
    textureBackend_ = nullptr;
    textureDirty_ = false;
}

Vec3 VideoPrimitive::Impl::transformPoint(float x, float y) const {
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

void VideoPrimitive::Impl::rebuildVertices(float* vertices) const {
    Rect drawRect = bounds_;
    if (fit_ == ImageFit::Contain && textureWidth_ > 0 && textureHeight_ > 0 &&
        bounds_.width > 0.0f && bounds_.height > 0.0f) {
        const float rectAspect = bounds_.width / bounds_.height;
        const float videoAspect = static_cast<float>(textureWidth_) / static_cast<float>(textureHeight_);
        if (videoAspect > rectAspect) {
            drawRect.height = bounds_.width / videoAspect;
            drawRect.y = bounds_.y + (bounds_.height - drawRect.height) * 0.5f;
        } else if (videoAspect < rectAspect) {
            drawRect.width = bounds_.height * videoAspect;
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
        const float videoAspect = static_cast<float>(textureWidth_) / static_cast<float>(textureHeight_);
        if (fit_ == ImageFit::Cover) {
            if (videoAspect > rectAspect) {
                const float visible = std::clamp(rectAspect / videoAspect, 0.0f, 1.0f);
                u0 = (1.0f - visible) * 0.5f;
                u1 = 1.0f - u0;
            } else if (videoAspect < rectAspect) {
                const float visible = std::clamp(videoAspect / rectAspect, 0.0f, 1.0f);
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

VideoPrimitive::VideoPrimitive()
    : impl_(std::make_unique<Impl>()) {}

VideoPrimitive::~VideoPrimitive() = default;
VideoPrimitive::VideoPrimitive(VideoPrimitive&&) noexcept = default;
VideoPrimitive& VideoPrimitive::operator=(VideoPrimitive&&) noexcept = default;

bool VideoPrimitive::initialize() { return impl_->initialize(); }
void VideoPrimitive::destroy() { impl_->destroy(); }
void VideoPrimitive::setSource(const std::string& source) { impl_->setSource(source); }
void VideoPrimitive::setBounds(float x, float y, float width, float height) { impl_->setBounds(x, y, width, height); }
void VideoPrimitive::setTint(const Color& tint) { impl_->setTint(tint); }
void VideoPrimitive::setCornerRadius(float radius) { impl_->setCornerRadius(radius); }
void VideoPrimitive::setOpacity(float opacity) { impl_->setOpacity(opacity); }
void VideoPrimitive::setTransform(const Transform& transform) { impl_->setTransform(transform); }
void VideoPrimitive::setTransformMatrix(const TransformMatrix& matrix) { impl_->setTransformMatrix(matrix); }
void VideoPrimitive::setFit(ImageFit fit) { impl_->setFit(fit); }
void VideoPrimitive::setCoverViewport(bool enabled, const Vec2& canvasSize, const Vec2& viewportOffset) {
    impl_->setCoverViewport(enabled, canvasSize, viewportOffset);
}
bool VideoPrimitive::updateFrame() { return impl_->updateFrame(); }
bool VideoPrimitive::hasFrame() const { return impl_->hasFrame(); }
void VideoPrimitive::render(int windowWidth, int windowHeight) { impl_->render(windowWidth, windowHeight); }

} // namespace core
