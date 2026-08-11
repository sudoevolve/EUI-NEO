#pragma once

#include "core/render/render_backend.h"
#include "core/render/shadertoy.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace core {

class ShaderToyPrimitive {
public:
    ShaderToyPrimitive();
    ~ShaderToyPrimitive();

    ShaderToyPrimitive(const ShaderToyPrimitive&) = delete;
    ShaderToyPrimitive& operator=(const ShaderToyPrimitive&) = delete;
    ShaderToyPrimitive(ShaderToyPrimitive&&) noexcept;
    ShaderToyPrimitive& operator=(ShaderToyPrimitive&&) noexcept;

    bool initialize();
    void destroy();
    void setElementId(std::string id);
    void setGraph(const render::ShaderToyGraph& graph);
    void setBounds(float x, float y, float width, float height);
    void setCornerRadius(float radius);
    void setOpacity(float opacity);
    void setTransformMatrix(const TransformMatrix& matrix);
    void setResolutionScale(float scale);
    void setPaused(bool paused);
    void setTimeScale(float scale);
    void requestReset();
    void update(float deltaSeconds,
                const Vec2& localPointer,
                bool pointerDown,
                bool pressedThisFrame,
                bool releasedThisFrame,
                std::uint64_t frameToken,
                bool pointerInside = true);
    bool isAnimating() const;
    const Rect& bounds() const;
    const render::ShaderToyFrameData& frameData() const;
    const render::ShaderToyError& error() const;
    void render(int windowWidth, int windowHeight);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace core
