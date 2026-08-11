#include "core/render/shadertoy_primitive.h"

#include "core/render/primitive_geometry.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>

namespace core {
namespace {

std::array<float, 4> localDate() {
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#ifdef _WIN32
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    return {
        static_cast<float>(local.tm_year + 1900),
        static_cast<float>(local.tm_mon + 1),
        static_cast<float>(local.tm_mday),
        static_cast<float>(local.tm_hour * 3600 + local.tm_min * 60 + local.tm_sec)
    };
}

void buildVertices(const Rect& bounds,
                   const TransformMatrix& matrix,
                   float* vertices) {
    const Vec3 screen[4] = {
        transformPointWithW(matrix, bounds.x, bounds.y),
        transformPointWithW(matrix, bounds.x + bounds.width, bounds.y),
        transformPointWithW(matrix, bounds.x + bounds.width, bounds.y + bounds.height),
        transformPointWithW(matrix, bounds.x, bounds.y + bounds.height)
    };
    const Vec2 local[4] = {
        {bounds.x, bounds.y},
        {bounds.x + bounds.width, bounds.y},
        {bounds.x + bounds.width, bounds.y + bounds.height},
        {bounds.x, bounds.y + bounds.height}
    };
    const Vec2 uv[4] = {{0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f}};
    constexpr int order[6] = {0, 1, 2, 0, 2, 3};
    for (int index = 0; index < 6; ++index) {
        const int vertex = order[index];
        const int offset = index * 7;
        vertices[offset + 0] = screen[vertex].x;
        vertices[offset + 1] = screen[vertex].y;
        vertices[offset + 2] = screen[vertex].z;
        vertices[offset + 3] = local[vertex].x;
        vertices[offset + 4] = local[vertex].y;
        vertices[offset + 5] = uv[vertex].x;
        vertices[offset + 6] = uv[vertex].y;
    }
}

} // namespace

struct ShaderToyPrimitive::Impl {
    render::ShaderToyGraph graph;
    render::ShaderToyGraph activeGraph;
    std::string elementId;
    std::uint64_t resourceHash = 0;
    std::uint64_t activeResourceHash = 0;
    render::RenderBackend::ShaderToyHandle handle = nullptr;
    render::RenderBackend* backend = nullptr;
    Rect boundsValue{};
    TransformMatrix transformMatrix{};
    render::ShaderToyFrameData frame{};
    render::ShaderToyError lastError{};
    Vec2 lastClickPosition{};
    Vec2 pointerPosition{};
    float radius = 0.0f;
    float opacity = 1.0f;
    float resolutionScale = 1.0f;
    float timeScale = 1.0f;
    bool paused = false;
    bool resetRequested = true;
    bool pointerHasClicked = false;
    bool pointerCaptured = false;
    bool frameRendered = false;
    std::uint64_t lastUpdateToken = 0;

    void resetTargets(bool resetClock) {
        resetRequested = true;
        if (resetClock) {
            frame = {};
            lastUpdateToken = 0;
        } else {
            frame.frame = 0;
        }
        frameRendered = false;
    }

    void release() {
        if (handle != nullptr && backend != nullptr) {
            backend->destroyShaderToy(handle);
        }
        handle = nullptr;
        backend = nullptr;
        activeGraph = {};
        activeResourceHash = 0;
    }
};

ShaderToyPrimitive::ShaderToyPrimitive() : impl_(std::make_unique<Impl>()) {}
ShaderToyPrimitive::~ShaderToyPrimitive() { destroy(); }
ShaderToyPrimitive::ShaderToyPrimitive(ShaderToyPrimitive&&) noexcept = default;
ShaderToyPrimitive& ShaderToyPrimitive::operator=(ShaderToyPrimitive&& other) noexcept {
    if (this != &other) {
        destroy();
        impl_ = std::move(other.impl_);
    }
    return *this;
}

bool ShaderToyPrimitive::initialize() { return true; }
void ShaderToyPrimitive::destroy() {
    if (impl_) impl_->release();
}
void ShaderToyPrimitive::setElementId(std::string id) { impl_->elementId = std::move(id); }

void ShaderToyPrimitive::setGraph(const render::ShaderToyGraph& graph) {
    const std::uint64_t hash = render::shaderToyResourceHash(graph);
    impl_->resourceHash = hash;
    impl_->graph = graph;
}

void ShaderToyPrimitive::setBounds(float x, float y, float width, float height) {
    const Rect next{x, y, width, height};
    if (next.width != impl_->boundsValue.width || next.height != impl_->boundsValue.height) {
        impl_->resetTargets(false);
    }
    impl_->boundsValue = next;
}
void ShaderToyPrimitive::setCornerRadius(float radius) { impl_->radius = std::max(0.0f, radius); }
void ShaderToyPrimitive::setOpacity(float opacity) { impl_->opacity = std::clamp(opacity, 0.0f, 1.0f); }
void ShaderToyPrimitive::setTransformMatrix(const TransformMatrix& matrix) { impl_->transformMatrix = matrix; }
void ShaderToyPrimitive::setResolutionScale(float scale) {
    const float next = std::clamp(scale, 0.125f, 4.0f);
    if (next != impl_->resolutionScale) {
        impl_->resolutionScale = next;
        impl_->resetTargets(false);
    }
}
void ShaderToyPrimitive::setPaused(bool paused) { impl_->paused = paused; }
void ShaderToyPrimitive::setTimeScale(float scale) { impl_->timeScale = std::max(0.0f, scale); }
void ShaderToyPrimitive::requestReset() {
    impl_->resetTargets(true);
}

void ShaderToyPrimitive::update(float deltaSeconds,
                                const Vec2& localPointer,
                                bool pointerDown,
                                bool pressedThisFrame,
                                bool releasedThisFrame,
                                std::uint64_t frameToken,
                                bool pointerInside) {
    if (frameToken == impl_->lastUpdateToken) {
        return;
    }
    impl_->lastUpdateToken = frameToken;
    const float scaledDelta = impl_->paused ? 0.0f : std::max(0.0f, deltaSeconds) * impl_->timeScale;
    impl_->frame.deltaTime = scaledDelta;
    impl_->frame.time += scaledDelta;
    impl_->frame.frameRate = scaledDelta > 0.0f ? 1.0f / scaledDelta : 0.0f;
    if (!impl_->paused && impl_->frameRendered) {
        ++impl_->frame.frame;
    }
    impl_->frame.date = localDate();
    impl_->frame.channelTime.fill(impl_->frame.time);
    impl_->frame.frameToken = frameToken;

    const Vec2 bottomLeft{
        std::clamp(localPointer.x, 0.0f, impl_->boundsValue.width),
        std::clamp(impl_->boundsValue.height - localPointer.y, 0.0f, impl_->boundsValue.height)
    };
    if (pointerInside || impl_->pointerCaptured) {
        impl_->pointerPosition = bottomLeft;
    }
    if (pressedThisFrame && pointerInside) {
        impl_->lastClickPosition = bottomLeft;
        impl_->pointerHasClicked = true;
        impl_->pointerCaptured = true;
    }
    const bool capturedRelease = releasedThisFrame && impl_->pointerCaptured;
    if (pointerDown && impl_->pointerCaptured) {
        impl_->frame.mouse = {impl_->pointerPosition.x, impl_->pointerPosition.y,
                              impl_->pointerPosition.x, impl_->pointerPosition.y};
    } else if (impl_->pointerHasClicked || capturedRelease) {
        impl_->frame.mouse = {impl_->pointerPosition.x, impl_->pointerPosition.y,
                              -std::fabs(impl_->lastClickPosition.x),
                              -std::fabs(impl_->lastClickPosition.y)};
    } else {
        impl_->frame.mouse = {impl_->pointerPosition.x,
                              impl_->pointerPosition.y, 0.0f, 0.0f};
    }
    if (capturedRelease || (!pointerDown && impl_->pointerCaptured)) {
        impl_->pointerCaptured = false;
    }
}

bool ShaderToyPrimitive::isAnimating() const { return !impl_->paused; }
const Rect& ShaderToyPrimitive::bounds() const { return impl_->boundsValue; }
const render::ShaderToyFrameData& ShaderToyPrimitive::frameData() const { return impl_->frame; }
const render::ShaderToyError& ShaderToyPrimitive::error() const { return impl_->lastError; }

void ShaderToyPrimitive::render(int windowWidth, int windowHeight) {
    if (impl_->boundsValue.width <= 0.0f || impl_->boundsValue.height <= 0.0f ||
        impl_->opacity <= 0.001f || impl_->graph.passes.empty()) {
        return;
    }
    render::RenderBackend* active = render::activeRenderBackend();
    if (active == nullptr) {
        return;
    }
    if (impl_->backend != active) {
        impl_->release();
        impl_->backend = active;
    }
    if (impl_->handle == nullptr) {
        const render::ShaderToyValidationResult validation = render::validateShaderToyGraph(impl_->graph);
        if (!validation.valid()) {
            impl_->lastError = validation.errors.front();
            impl_->lastError.elementId = impl_->elementId;
            return;
        }
        impl_->handle = active->createShaderToy(impl_->graph, &impl_->lastError);
        if (impl_->handle == nullptr) {
            impl_->lastError.elementId = impl_->elementId;
            return;
        }
        impl_->activeGraph = impl_->graph;
        impl_->activeResourceHash = impl_->resourceHash;
        impl_->resetRequested = true;
    } else if (impl_->activeResourceHash != impl_->resourceHash) {
        render::ShaderToyError replacementError;
        render::RenderBackend::ShaderToyHandle replacement = nullptr;
        const render::ShaderToyValidationResult validation =
            render::validateShaderToyGraph(impl_->graph);
        if (validation.valid()) {
            replacement = active->createShaderToy(impl_->graph,
                                                   &replacementError);
        } else {
            replacementError = validation.errors.front();
        }
        if (replacement != nullptr) {
            const int width = std::max(
                1, static_cast<int>(std::ceil(
                       impl_->boundsValue.width * impl_->resolutionScale)));
            const int height = std::max(
                1, static_cast<int>(std::ceil(
                       impl_->boundsValue.height * impl_->resolutionScale)));
            render::RenderBackend::TextureHandle replacementTexture =
                active->renderShaderToy(
                    replacement, impl_->graph, width, height, impl_->frame,
                    impl_->paused, true, &replacementError);
            if (replacementTexture != nullptr) {
                render::RenderBackend::ShaderToyHandle previous = impl_->handle;
                impl_->handle = replacement;
                impl_->activeGraph = impl_->graph;
                impl_->activeResourceHash = impl_->resourceHash;
                impl_->lastError = replacementError;
                impl_->resetRequested = false;
                impl_->frameRendered = true;

                float vertices[42]{};
                buildVertices(impl_->boundsValue, impl_->transformMatrix,
                              vertices);
                active->drawTexture(
                    replacementTexture, vertices, 42,
                    {1.0f, 1.0f, 1.0f, impl_->opacity},
                    impl_->boundsValue, impl_->radius, 0.0f,
                    windowWidth, windowHeight);
                active->destroyShaderToy(previous);
                return;
            }
            active->destroyShaderToy(replacement);
        }
        replacementError.elementId = impl_->elementId;
        impl_->lastError = std::move(replacementError);
    } else {
        impl_->activeGraph = impl_->graph;
    }

    const int targetWidth = std::max(1, static_cast<int>(std::ceil(impl_->boundsValue.width * impl_->resolutionScale)));
    const int targetHeight = std::max(1, static_cast<int>(std::ceil(impl_->boundsValue.height * impl_->resolutionScale)));
    render::ShaderToyError activeError;
    render::ShaderToyError* renderError =
        impl_->activeResourceHash != impl_->resourceHash
            ? &activeError
            : &impl_->lastError;
    render::RenderBackend::TextureHandle texture = active->renderShaderToy(
        impl_->handle, impl_->activeGraph, targetWidth, targetHeight, impl_->frame,
        impl_->paused, impl_->resetRequested, renderError);
    if (texture == nullptr) {
        if (activeError) impl_->lastError = activeError;
        impl_->lastError.elementId = impl_->elementId;
        return;
    }
    if (activeError) impl_->lastError = activeError;
    if (impl_->lastError) {
        impl_->lastError.elementId = impl_->elementId;
    }
    impl_->resetRequested = false;
    impl_->frameRendered = true;

    float vertices[42]{};
    buildVertices(impl_->boundsValue, impl_->transformMatrix, vertices);
    active->drawTexture(texture, vertices, 42,
                        {1.0f, 1.0f, 1.0f, impl_->opacity},
                        impl_->boundsValue, impl_->radius, 0.0f,
                        windowWidth, windowHeight);
}

} // namespace core
