#pragma once

#include "core/render/render_types.h"

#include <memory>
#include <string>

namespace core {

class ImagePrimitive {
public:
    ImagePrimitive();
    ~ImagePrimitive();

    ImagePrimitive(const ImagePrimitive&) = delete;
    ImagePrimitive& operator=(const ImagePrimitive&) = delete;
    ImagePrimitive(ImagePrimitive&&) noexcept;
    ImagePrimitive& operator=(ImagePrimitive&&) noexcept;

    bool initialize();
    void destroy();

    void setSource(const std::string& source);
    void setSvgSource(const std::string& key, const std::string& svg);
    void setFlipVertically(bool value);
    void setBounds(float x, float y, float width, float height);
    void setTint(const Color& tint);
    void setCornerRadius(float radius);
    void setBlur(float blur);
    void setOpacity(float opacity);
    void setTransform(const Transform& transform);
    void setTransformMatrix(const TransformMatrix& matrix);
    void setFit(ImageFit fit);
    void setCoverViewport(bool enabled, const Vec2& canvasSize, const Vec2& viewportOffset);

    bool updateTexture();
    bool hasPendingLoad() const;
    bool isAnimating() const;
    void render(int windowWidth, int windowHeight);

    static bool isSourceReady(const std::string& source);
    static bool hasSourceFailed(const std::string& source);
    static bool retrySource(const std::string& source);
    static bool consumeRemoteImageReady();
    static void beginRenderFrame();
    static void releaseCachedTextures();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace core
