#include "core/render/primitive.h"

#include "core/render/primitive_geometry.h"
#include "core/render/render_backend.h"

#include <algorithm>

namespace core {

namespace {

core::render::RenderBackend* activeBackend() {
    return core::render::activeRenderBackend();
}

Rect transformedBoundsForRect(const Rect& bounds,
                              const Transform& transform,
                              const TransformMatrix& matrix,
                              bool hasTransformMatrix) {
    const Vec3 p0 = core::render::transformPrimitivePoint(bounds, transform, matrix, hasTransformMatrix, bounds.x, bounds.y);
    const Vec3 p1 = core::render::transformPrimitivePoint(bounds, transform, matrix, hasTransformMatrix, bounds.x + bounds.width, bounds.y);
    const Vec3 p2 = core::render::transformPrimitivePoint(bounds, transform, matrix, hasTransformMatrix, bounds.x + bounds.width, bounds.y + bounds.height);
    const Vec3 p3 = core::render::transformPrimitivePoint(bounds, transform, matrix, hasTransformMatrix, bounds.x, bounds.y + bounds.height);
    const float left = std::min(std::min(p0.x, p1.x), std::min(p2.x, p3.x));
    const float top = std::min(std::min(p0.y, p1.y), std::min(p2.y, p3.y));
    const float right = std::max(std::max(p0.x, p1.x), std::max(p2.x, p3.x));
    const float bottom = std::max(std::max(p0.y, p1.y), std::max(p2.y, p3.y));
    return {left, top, right - left, bottom - top};
}

} // namespace

struct RoundedRectPrimitive::Impl {
    explicit Impl(float x = 0.0f, float y = 0.0f, float width = 0.0f, float height = 0.0f)
        : bounds{x, y, width, height} {}

    Rect bounds{};
    Color color{};
    Gradient gradient{};
    Border border{};
    Shadow shadow{};
    Transform transform{};
    TransformMatrix transformMatrix{};
    float blur = 0.0f;
    float cornerRadius = 0.0f;
    float opacity = 1.0f;
    bool hasTransformMatrix = false;

    void drawLayer(int windowWidth,
                   int windowHeight,
                   const Rect& geometryBounds,
                   const Rect& sdfBounds,
                   const Rect& backdropBounds,
                   bool shadowPass,
                   bool insetShadowPass,
                   const Color& layerColor,
                   float layerBlur,
                   const Vec2& layerShadowOffset = {},
                   float layerShadowSpread = 0.0f) const {
        auto* backend = activeBackend();
        if (backend == nullptr) {
            return;
        }

        const auto vertices = core::render::roundedRectGeometryVertices(
            bounds, transform, transformMatrix, hasTransformMatrix, geometryBounds);

        core::render::RoundedRectDrawCommand command{};
        command.vertices.assign(vertices.begin(), vertices.end());
        command.fillColor = shadowPass ? layerColor : color;
        command.gradient = gradient;
        command.border = shadowPass ? Border{} : border;
        command.rect = sdfBounds;
        command.radius = core::render::clampedPrimitiveRadius(cornerRadius, sdfBounds);
        command.border.width = core::render::clampedPrimitiveBorderWidth(command.border.width, sdfBounds);
        command.opacity = opacity;
        command.shadowBlur = layerBlur;
        command.shadowOffset = layerShadowOffset;
        command.shadowSpread = layerShadowSpread;
        command.backdropBlur = shadowPass ? 0.0f : blur;
        command.backdropOffset = {
            backdropBounds.x - bounds.x,
            backdropBounds.y - bounds.y
        };
        command.shadowPass = shadowPass;
        command.insetShadowPass = insetShadowPass;
        backend->drawRoundedRect(command, windowWidth, windowHeight);
    }

    void drawShadow(int windowWidth, int windowHeight) const {
        if (bounds.width <= 0.0f || bounds.height <= 0.0f ||
            opacity <= 0.001f || shadow.color.a <= 0.001f) {
            return;
        }

        const Rect shadowShape = core::render::primitiveShadowShape(bounds, shadow);
        const float shadowBlur = core::render::primitiveShadowBlur(shadow);
        const float shadowExtent = core::render::primitiveShadowExtent(shadow, shadowBlur);
        drawLayer(windowWidth, windowHeight, core::render::expandPrimitiveRect(shadowShape, shadowExtent), shadowShape, shadowShape,
                  true, false, core::render::scalePrimitiveAlpha(shadow.color, 0.74f), shadowBlur);
    }

    void drawInsetShadow(int windowWidth, int windowHeight) const {
        if (bounds.width <= 0.0f || bounds.height <= 0.0f ||
            opacity <= 0.001f || shadow.color.a <= 0.001f) {
            return;
        }

        drawLayer(windowWidth, windowHeight, bounds, bounds, bounds, true, true,
                  core::render::scalePrimitiveAlpha(shadow.color, 0.86f),
                  core::render::primitiveShadowBlur(shadow),
                  shadow.offset,
                  shadow.spread);
    }
};

RoundedRectPrimitive::RoundedRectPrimitive()
    : impl_(std::make_unique<Impl>()) {}

RoundedRectPrimitive::RoundedRectPrimitive(float x, float y, float width, float height)
    : impl_(std::make_unique<Impl>(x, y, width, height)) {}

RoundedRectPrimitive::~RoundedRectPrimitive() = default;
RoundedRectPrimitive::RoundedRectPrimitive(RoundedRectPrimitive&&) noexcept = default;
RoundedRectPrimitive& RoundedRectPrimitive::operator=(RoundedRectPrimitive&&) noexcept = default;

bool RoundedRectPrimitive::initialize() { return true; }
void RoundedRectPrimitive::destroy() {}
void RoundedRectPrimitive::setBounds(float x, float y, float width, float height) { impl_->bounds = {x, y, width, height}; }
void RoundedRectPrimitive::setColor(const Color& color) { impl_->color = color; }
void RoundedRectPrimitive::setGradient(const Gradient& gradient) { impl_->gradient = gradient; }
void RoundedRectPrimitive::setCornerRadius(float radius) { impl_->cornerRadius = radius; }
void RoundedRectPrimitive::setOpacity(float opacity) { impl_->opacity = std::clamp(opacity, 0.0f, 1.0f); }
void RoundedRectPrimitive::setBorder(const Border& border) { impl_->border = border; }
void RoundedRectPrimitive::setShadow(const Shadow& shadow) { impl_->shadow = shadow; }
void RoundedRectPrimitive::setBlur(float blur) { impl_->blur = std::max(0.0f, blur); }
void RoundedRectPrimitive::setTranslate(float x, float y) {
    impl_->transform.translate = {x, y};
    impl_->hasTransformMatrix = false;
}
void RoundedRectPrimitive::setScale(float x, float y) {
    impl_->transform.scale = {x, y};
    impl_->hasTransformMatrix = false;
}
void RoundedRectPrimitive::setRotate(float radians) {
    impl_->transform.rotate = radians;
    impl_->hasTransformMatrix = false;
}
void RoundedRectPrimitive::setTransformOrigin(float x, float y) {
    impl_->transform.origin = {x, y};
    impl_->hasTransformMatrix = false;
}
void RoundedRectPrimitive::setTransform(const Transform& transform) {
    impl_->transform = transform;
    impl_->hasTransformMatrix = false;
}
void RoundedRectPrimitive::setTransformMatrix(const TransformMatrix& matrix) {
    impl_->transformMatrix = matrix;
    impl_->hasTransformMatrix = true;
}
const Rect& RoundedRectPrimitive::bounds() const { return impl_->bounds; }
const Color& RoundedRectPrimitive::color() const { return impl_->color; }
const Gradient& RoundedRectPrimitive::gradient() const { return impl_->gradient; }
const Border& RoundedRectPrimitive::border() const { return impl_->border; }
const Shadow& RoundedRectPrimitive::shadow() const { return impl_->shadow; }
float RoundedRectPrimitive::blur() const { return impl_->blur; }
const Transform& RoundedRectPrimitive::transform() const { return impl_->transform; }
float RoundedRectPrimitive::cornerRadius() const { return impl_->cornerRadius; }
float RoundedRectPrimitive::opacity() const { return impl_->opacity; }
void RoundedRectPrimitive::render(int windowWidth, int windowHeight) const {
    if (impl_->bounds.width <= 0.0f || impl_->bounds.height <= 0.0f || impl_->opacity <= 0.001f) {
        return;
    }
    auto* backend = activeBackend();
    if (backend == nullptr) {
        return;
    }
    const Rect backdropBounds = transformedBoundsForRect(impl_->bounds,
                                                        impl_->transform,
                                                        impl_->transformMatrix,
                                                        impl_->hasTransformMatrix);
    if (impl_->blur > 0.0f) {
        backend->prepareBackdropBlur(backdropBounds, impl_->blur, windowWidth, windowHeight);
    }
    if (impl_->shadow.enabled && !impl_->shadow.inset) {
        impl_->drawShadow(windowWidth, windowHeight);
    }
    impl_->drawLayer(windowWidth, windowHeight, impl_->bounds, impl_->bounds, backdropBounds, false, false, impl_->color, impl_->shadow.blur);
    if (impl_->shadow.enabled && impl_->shadow.inset) {
        impl_->drawInsetShadow(windowWidth, windowHeight);
    }
}

struct PolygonPrimitive::Impl {
    Rect bounds{};
    std::vector<Vec2> points;
    float strokeWidth = 0.0f;
    Color color{};
    Transform transform{};
    TransformMatrix transformMatrix{};
    float radius = 0.0f;
    float opacity = 1.0f;
    bool hasTransformMatrix = false;
};

PolygonPrimitive::PolygonPrimitive()
    : impl_(std::make_unique<Impl>()) {}

PolygonPrimitive::~PolygonPrimitive() = default;
PolygonPrimitive::PolygonPrimitive(PolygonPrimitive&&) noexcept = default;
PolygonPrimitive& PolygonPrimitive::operator=(PolygonPrimitive&&) noexcept = default;

bool PolygonPrimitive::initialize() { return true; }
void PolygonPrimitive::destroy() {}
void PolygonPrimitive::setBounds(float x, float y, float width, float height) { impl_->bounds = {x, y, width, height}; }
void PolygonPrimitive::setPoints(const std::vector<Vec2>& points) { impl_->points = points; }
void PolygonPrimitive::setStrokeWidth(float width) { impl_->strokeWidth = std::max(0.0f, width); }
void PolygonPrimitive::setRadius(float radius) { impl_->radius = std::max(0.0f, radius); }
void PolygonPrimitive::setColor(const Color& color) { impl_->color = color; }
void PolygonPrimitive::setOpacity(float opacity) { impl_->opacity = std::clamp(opacity, 0.0f, 1.0f); }
void PolygonPrimitive::setTransform(const Transform& transform) {
    impl_->transform = transform;
    impl_->hasTransformMatrix = false;
}
void PolygonPrimitive::setTransformMatrix(const TransformMatrix& matrix) {
    impl_->transformMatrix = matrix;
    impl_->hasTransformMatrix = true;
}
void PolygonPrimitive::render(int windowWidth, int windowHeight) const {
    if (impl_->points.size() < (impl_->strokeWidth > 0 ? 2u : 3u) || impl_->opacity <= 0.001f || impl_->color.a <= 0.001f) {
        return;
    }
    auto* backend = activeBackend();
    if (backend == nullptr) {
        return;
    }

    if (impl_->strokeWidth > 0.0f) {
        // 同一折线只占一个 Runtime 节点；胶囊线段复用两后端已有的 rounded rect batch。
        const float radius = impl_->strokeWidth * 0.5f;
        core::render::RoundedRectDrawCommand command;
        command.fillColor = impl_->color;
        command.opacity = impl_->opacity;
        command.radius = radius;
        command.vertices.resize(6);
        for (std::size_t i = 1; i < impl_->points.size(); ++i) {
            const auto a = impl_->points[i - 1], b = impl_->points[i];
            if (!std::isfinite(a.x) || !std::isfinite(a.y) || !std::isfinite(b.x) || !std::isfinite(b.y)) continue;
            const float dx = b.x - a.x, dy = b.y - a.y;
            const float length = std::hypot(dx, dy);
            if (length <= 0.0001f) continue;
            command.rect = {-radius, -radius, length + radius * 2, radius * 2};
            const auto vertices = core::render::roundedRectGeometryVertices(command.rect, {}, {}, false,
                core::render::expandPrimitiveRect(command.rect, 2));
            for (std::size_t v = 0; v < vertices.size(); ++v) {
                const auto p = vertices[v].local;
                command.vertices[v] = {core::render::transformPrimitivePoint(impl_->bounds, impl_->transform,
                    impl_->transformMatrix, impl_->hasTransformMatrix,
                    impl_->bounds.x + a.x + (dx * p.x - dy * p.y) / length,
                    impl_->bounds.y + a.y + (dy * p.x + dx * p.y) / length), p};
            }
            backend->drawRoundedRect(command, windowWidth, windowHeight);
        }
        return;
    }

    const std::vector<Vec2> renderPoints = core::render::roundedPolygonPoints(impl_->points, impl_->radius);
    if (renderPoints.size() < 3) {
        return;
    }

    constexpr float polygonAntialiasExtent = 2.0f;
    const Rect geometryBounds = core::render::expandPrimitiveRect(impl_->bounds, polygonAntialiasExtent);
    core::render::PolygonDrawCommand command{};
    const auto vertices = core::render::polygonGeometryVertices(
        impl_->bounds, impl_->transform, impl_->transformMatrix, impl_->hasTransformMatrix, geometryBounds);
    command.vertices.assign(vertices.begin(), vertices.end());
    command.edges.reserve(renderPoints.size());
    for (std::size_t index = 0; index < renderPoints.size(); ++index) {
        const Vec2& from = renderPoints[index];
        const Vec2& to = renderPoints[(index + 1u) % renderPoints.size()];
        command.edges.push_back({
            {impl_->bounds.x + from.x, impl_->bounds.y + from.y},
            {impl_->bounds.x + to.x, impl_->bounds.y + to.y}
        });
    }
    command.fillColor = impl_->color;
    command.opacity = impl_->opacity;
    backend->drawPolygon(command, windowWidth, windowHeight);
}

} // namespace core
