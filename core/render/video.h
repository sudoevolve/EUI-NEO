#pragma once

#include "core/render/image_types.h"
#include "core/render/render_types.h"

#include <memory>
#include <string>

namespace core {

class VideoPrimitive {
public:
    VideoPrimitive();
    ~VideoPrimitive();

    VideoPrimitive(const VideoPrimitive&) = delete;
    VideoPrimitive& operator=(const VideoPrimitive&) = delete;
    VideoPrimitive(VideoPrimitive&&) noexcept;
    VideoPrimitive& operator=(VideoPrimitive&&) noexcept;

    bool initialize();
    void destroy();

    void setSource(const std::string& source);
    void setBounds(float x, float y, float width, float height);
    void setTint(const Color& tint);
    void setCornerRadius(float radius);
    void setOpacity(float opacity);
    void setTransform(const Transform& transform);
    void setTransformMatrix(const TransformMatrix& matrix);
    void setFit(ImageFit fit);
    void setCoverViewport(bool enabled, const Vec2& canvasSize, const Vec2& viewportOffset);

    bool updateFrame();
    bool hasFrame() const;
    void render(int windowWidth, int windowHeight);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace core
