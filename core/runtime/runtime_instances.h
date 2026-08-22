#pragma once

#include <core/render/shadertoy_primitive.h>

#include "core/dsl.h"
#include "core/render/image.h"
#include "core/render/primitive.h"
#include "core/render/text.h"
#include "core/runtime/runtime_geometry.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace core::dsl::runtime {

struct ElementSnapshot {
    std::string id;
    ElementKind kind = ElementKind::Stack;
    int zIndex = 0;
    bool clip = false;
    std::size_t childCount = 0;

    bool operator==(const ElementSnapshot& other) const {
        return id == other.id &&
               kind == other.kind &&
               zIndex == other.zIndex &&
               clip == other.clip &&
               childCount == other.childCount;
    }

    bool operator!=(const ElementSnapshot& other) const {
        return !(*this == other);
    }
};

struct RectInstance {
    std::unique_ptr<RoundedRectPrimitive> primitive = std::make_unique<RoundedRectPrimitive>();
    InteractionState interaction;
    bool initialized = false;
    bool seen = false;
    SmoothedValue<float> hoverBlend;
    SmoothedValue<float> pressBlend;
    AnimatedValue<LayoutRect> frame;
    AnimatedValue<Color> color;
    Gradient gradient;
    AnimatedValue<float> radius;
    AnimatedValue<float> blur;
    AnimatedValue<float> opacity;
    AnimatedValue<Border> border;
    AnimatedValue<Shadow> shadow;
    AnimatedValue<Transform> transform;
};

struct PolygonInstance {
    std::unique_ptr<PolygonPrimitive> primitive = std::make_unique<PolygonPrimitive>();
    InteractionState interaction;
    bool initialized = false;
    bool seen = false;
    SmoothedValue<float> hoverBlend;
    SmoothedValue<float> pressBlend;
    AnimatedValue<LayoutRect> frame;
    AnimatedValue<Color> color;
    AnimatedValue<float> radius;
    AnimatedValue<float> opacity;
    AnimatedValue<Transform> transform;
    std::vector<Vec2> points;
};

struct TextInstance {
    std::unique_ptr<TextPrimitive> primitive = std::make_unique<TextPrimitive>();
    bool initialized = false;
    bool seen = false;
    AnimatedValue<LayoutRect> frame;
    AnimatedValue<Color> color;
    AnimatedValue<float> opacity;
    AnimatedValue<Transform> transform;
    std::string text;
    std::string fontFamily;
    float fontSize = 16.0f;
    int fontWeight = 400;
    float maxWidth = 0.0f;
    bool wrap = false;
    HorizontalAlign horizontalAlign = HorizontalAlign::Left;
    VerticalAlign verticalAlign = VerticalAlign::Top;
    float lineHeight = 0.0f;
    std::string contentDirtyKey;
};

struct ImageInstance {
    std::unique_ptr<ImagePrimitive> primitive = std::make_unique<ImagePrimitive>();
    bool initialized = false;
    bool seen = false;
    AnimatedValue<LayoutRect> frame;
    AnimatedValue<Color> tint;
    AnimatedValue<float> radius;
    AnimatedValue<float> blur;
    AnimatedValue<float> opacity;
    AnimatedValue<Transform> transform;
    std::string source;
    std::string svgSource;
    bool flipVertically = false;
    ImageFit fit = ImageFit::Cover;
    bool hasCoverViewport = false;
    Vec2 coverViewportSize;
    Vec2 coverViewportOffset;
};

struct ShaderToyInstance {
    std::unique_ptr<ShaderToyPrimitive> primitive = std::make_unique<ShaderToyPrimitive>();
    bool initialized = false;
    bool seen = false;
    AnimatedValue<LayoutRect> frame;
    AnimatedValue<float> radius;
    AnimatedValue<float> opacity;
    AnimatedValue<Transform> transform;
    std::uint64_t graphHash = 0;
    std::uint64_t resetKey = 0;
    std::uint64_t reportedErrorHash = 0;
};

struct InteractionInstance {
    InteractionState state;
    bool contextActive = false;
    bool contextDragged = false;
    double contextStartX = 0.0;
    double contextStartY = 0.0;
    bool seen = false;
};

struct DirtyKeyInstance {
    std::string key;
    Rect rect;
    bool initialized = false;
    bool seen = false;
};

struct LayoutInstance {
    AnimatedValue<Transform> transform;
    AnimatedValue<float> opacity;
    bool seen = false;
};

struct ScrollStateInstance {
    float offset = 0.0f;
    float maxOffset = 0.0f;
    float step = 48.0f;
    float velocity = 0.0f;
    float dragStartOffset = 0.0f;
    Rect dirtyRect;
    bool hasDirtyRect = false;
    bool initialized = false;
    bool seen = false;
};

struct SliderStateInstance {
    float value = 0.0f;
    float width = 0.0f;
    float knobSize = 0.0f;
    Rect dirtyRect;
    bool hasDirtyRect = false;
    bool initialized = false;
    bool dragging = false;
    bool seen = false;
};

struct TimerInstance {
    float seconds = 0.0f;
    float elapsed = 0.0f;
    bool seen = false;
    bool active = false;
};

struct FrameTargetInstance {
    LayoutRect frame;
    bool initialized = false;
    bool seen = false;
};

struct PaintBoundsInstance {
    Rect own;
    Rect subtree;
    int drawCost = 0;
    bool hasOwn = false;
    bool hasSubtree = false;
    bool subtreeAnimating = false;
    bool seen = false;
};

struct RetainedLayerInstance {
    render::RenderBackend::LayerHandle handle = nullptr;
    Rect bounds;
    std::uint64_t signature = 0;
    int width = 0;
    int height = 0;
    Rect pendingBounds;
    std::uint64_t pendingSignature = 0;
    int pendingWidth = 0;
    int pendingHeight = 0;
    int pendingStableFrames = 0;
    bool valid = false;
    bool seen = false;
};

struct DependentVisualState {
    Rect rect;
    float opacity = 1.0f;
    float scale = 1.0f;
    bool seen = false;
};

template <typename Map>
inline void markEntriesUnseen(Map& entries) {
    for (auto& item : entries) {
        item.second.seen = false;
    }
}

template <typename Map, typename OnRemove>
inline void releaseUnseenEntries(Map& entries, OnRemove&& onRemove) {
    for (auto item = entries.begin(); item != entries.end(); ) {
        if (!item->second.seen) {
            onRemove(item->second);
            item = entries.erase(item);
        } else {
            ++item;
        }
    }
}

class InstanceStore {
public:
    RectInstance& rect(const std::string& id) {
        RectInstance& instance = rects.try_emplace(id).first->second;
        instance.seen = true;
        return instance;
    }

    PolygonInstance& polygon(const std::string& id) {
        PolygonInstance& instance = polygons.try_emplace(id).first->second;
        instance.seen = true;
        return instance;
    }

    TextInstance& text(const std::string& id) {
        TextInstance& instance = texts.try_emplace(id).first->second;
        instance.seen = true;
        return instance;
    }

    ImageInstance& image(const std::string& id) {
        ImageInstance& instance = images.try_emplace(id).first->second;
        instance.seen = true;
        return instance;
    }

    ShaderToyInstance& shaderToy(const std::string& id) {
        ShaderToyInstance& instance = shaderToys.try_emplace(id).first->second;
        instance.seen = true;
        return instance;
    }

    InteractionInstance& interaction(const std::string& id) {
        InteractionInstance& instance = interactions.try_emplace(id).first->second;
        instance.seen = true;
        return instance;
    }

    DirtyKeyInstance& dirtyKey(const std::string& id) {
        DirtyKeyInstance& instance = dirtyKeys.try_emplace(id).first->second;
        instance.seen = true;
        return instance;
    }

    LayoutInstance& layout(const std::string& id) {
        LayoutInstance& instance = layouts.try_emplace(id).first->second;
        instance.seen = true;
        return instance;
    }

    ScrollStateInstance& scrollState(const std::string& id) {
        ScrollStateInstance& instance = scrollStates.try_emplace(id).first->second;
        instance.seen = true;
        return instance;
    }

    SliderStateInstance& sliderState(const std::string& id) {
        SliderStateInstance& instance = sliderStates.try_emplace(id).first->second;
        instance.seen = true;
        return instance;
    }

    TimerInstance& timer(const std::string& id) {
        return timers.try_emplace(id).first->second;
    }

    RetainedLayerInstance& retainedLayer(const std::string& id) {
        RetainedLayerInstance& instance = retainedLayers.try_emplace(id).first->second;
        instance.seen = true;
        return instance;
    }

    bool hoverBlend(const std::string& id, float& value) const {
        const auto rect = rects.find(id);
        if (rect != rects.end()) {
            value = rect->second.hoverBlend.value();
            return true;
        }
        const auto polygon = polygons.find(id);
        if (polygon != polygons.end()) {
            value = polygon->second.hoverBlend.value();
            return true;
        }
        return false;
    }

    bool pressBlend(const std::string& id, float& value, LayoutRect& frame) const {
        const auto rect = rects.find(id);
        if (rect != rects.end()) {
            value = rect->second.pressBlend.value();
            frame = rect->second.frame.value();
            return true;
        }
        const auto polygon = polygons.find(id);
        if (polygon != polygons.end()) {
            value = polygon->second.pressBlend.value();
            frame = polygon->second.frame.value();
            return true;
        }
        return false;
    }

    RenderTransform renderTransform(const Element& element,
                                    float dpiScale,
                                    const RenderTransform& inherited) const {
        RenderTransform result = inherited;

        if (element.kind == ElementKind::Row ||
            element.kind == ElementKind::Column ||
            element.kind == ElementKind::Stack ||
            element.kind == ElementKind::Flow) {
            const auto layout = layouts.find(element.id);
            if (layout != layouts.end()) {
                const Transform local = layout->second.transform.value();
                const float opacity = layout->second.opacity.value();
                if (!isIdentityTransform(local) || !closeEnough(opacity, 1.0f)) {
                    const Transform scaled = scaleTransform(local, dpiScale);
                    result = appendRenderMatrix(
                        result,
                        matrixForTransform(toPixelRect(element.frame, dpiScale), scaled));
                    result.opacity *= opacity;
                }
            }
        }

        if (!element.hoverOpacitySourceId.empty()) {
            float hover = 0.0f;
            if (hoverBlend(element.hoverOpacitySourceId, hover)) {
                hover = std::clamp(hover, 0.0f, 1.0f);
                result.opacity *= lerpValue(
                    element.hoverHiddenOpacity,
                    element.hoverVisibleOpacity,
                    hover);
            } else {
                result.opacity *= element.hoverHiddenOpacity;
            }
        }

        if (!element.visualStateSourceId.empty()) {
            float press = 0.0f;
            LayoutRect sourceFrame;
            if (pressBlend(element.visualStateSourceId, press, sourceFrame)) {
                const float scale = 1.0f - (1.0f - element.pressedScale) * press;
                if (std::fabs(scale - 1.0f) > 0.0001f) {
                    result = appendRenderMatrix(
                        result,
                        matrixForScaleAround(toPixelRect(sourceFrame, dpiScale), scale));
                }
            }
        }
        return result;
    }

    void markInstancesUnseen() {
        markEntriesUnseen(rects);
        markEntriesUnseen(polygons);
        markEntriesUnseen(texts);
        markEntriesUnseen(images);
        markEntriesUnseen(shaderToys);
        markEntriesUnseen(interactions);
        markEntriesUnseen(dirtyKeys);
        markEntriesUnseen(layouts);
        markEntriesUnseen(scrollStates);
        markEntriesUnseen(sliderStates);
        markEntriesUnseen(frameTargets);
        markEntriesUnseen(paintBounds);
        markEntriesUnseen(retainedLayers);
    }

    void releaseUnseenInstances() {
        auto releasePrimitive = [](auto& instance) {
            if (instance.initialized) {
                instance.primitive->destroy();
                instance.initialized = false;
            }
        };
        auto noop = [](auto&) {};
        auto releaseLayer = [](RetainedLayerInstance& instance) {
            core::render::RenderBackend* renderBackend = core::render::activeRenderBackend();
            if (renderBackend != nullptr && instance.handle != nullptr) {
                renderBackend->destroyLayer(instance.handle);
            }
            instance.handle = nullptr;
            instance.valid = false;
        };

        releaseUnseenEntries(rects, releasePrimitive);
        releaseUnseenEntries(polygons, releasePrimitive);
        releaseUnseenEntries(texts, releasePrimitive);
        releaseUnseenEntries(images, releasePrimitive);
        releaseUnseenEntries(shaderToys, releasePrimitive);
        releaseUnseenEntries(interactions, noop);
        releaseUnseenEntries(dirtyKeys, noop);
        releaseUnseenEntries(layouts, noop);
        releaseUnseenEntries(scrollStates, noop);
        releaseUnseenEntries(sliderStates, noop);
        releaseUnseenEntries(frameTargets, noop);
        releaseUnseenEntries(paintBounds, noop);
        releaseUnseenEntries(retainedLayers, releaseLayer);
    }

    void markTimersUnseen() {
        markEntriesUnseen(timers);
    }

    void releaseUnseenTimers() {
        releaseUnseenEntries(timers, [](TimerInstance&) {});
    }

    void releaseGraphicsResources(bool releaseCachedImageTextures) {
        auto releasePrimitive = [](auto& entries) {
            for (auto& item : entries) {
                if (item.second.initialized) {
                    item.second.primitive->destroy();
                    item.second.initialized = false;
                }
            }
        };
        releasePrimitive(rects);
        releasePrimitive(polygons);
        releasePrimitive(texts);
        releasePrimitive(images);
        releasePrimitive(shaderToys);

        if (releaseCachedImageTextures) {
            ImagePrimitive::releaseCachedTextures();
        }
        core::render::RenderBackend* renderBackend = core::render::activeRenderBackend();
        if (renderBackend != nullptr) {
            for (auto& item : retainedLayers) {
                if (item.second.handle != nullptr) {
                    renderBackend->destroyLayer(item.second.handle);
                    item.second.handle = nullptr;
                }
                item.second.valid = false;
            }
        }
    }

    void clear() {
        rects.clear();
        polygons.clear();
        texts.clear();
        images.clear();
        shaderToys.clear();
        interactions.clear();
        dirtyKeys.clear();
        layouts.clear();
        scrollStates.clear();
        sliderStates.clear();
        timers.clear();
        dependentVisualStates.clear();
        frameTargets.clear();
        paintBounds.clear();
        retainedLayers.clear();
    }

    std::unordered_map<std::string, RectInstance> rects;
    std::unordered_map<std::string, PolygonInstance> polygons;
    std::unordered_map<std::string, TextInstance> texts;
    std::unordered_map<std::string, ImageInstance> images;
    std::unordered_map<std::string, ShaderToyInstance> shaderToys;
    std::unordered_map<std::string, InteractionInstance> interactions;
    std::unordered_map<std::string, DirtyKeyInstance> dirtyKeys;
    std::unordered_map<std::string, LayoutInstance> layouts;
    std::unordered_map<std::string, ScrollStateInstance> scrollStates;
    std::unordered_map<std::string, SliderStateInstance> sliderStates;
    std::unordered_map<std::string, TimerInstance> timers;
    std::unordered_map<std::string, DependentVisualState> dependentVisualStates;
    std::unordered_map<std::string, FrameTargetInstance> frameTargets;
    std::unordered_map<std::string, PaintBoundsInstance> paintBounds;
    std::unordered_map<std::string, RetainedLayerInstance> retainedLayers;
};

} // namespace core::dsl::runtime
