#pragma once

#include "core/dsl.h"
#include "core/render/image.h"
#include "core/render/primitive.h"
#include "core/render/text.h"
#include "core/render/video.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
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

struct VideoInstance {
    std::unique_ptr<VideoPrimitive> primitive = std::make_unique<VideoPrimitive>();
    bool initialized = false;
    bool seen = false;
    AnimatedValue<LayoutRect> frame;
    AnimatedValue<Color> tint;
    AnimatedValue<float> radius;
    AnimatedValue<float> opacity;
    AnimatedValue<Transform> transform;
    std::string source;
    ImageFit fit = ImageFit::Cover;
    bool hasCoverViewport = false;
    Vec2 coverViewportSize;
    Vec2 coverViewportOffset;
};

struct InteractionInstance {
    InteractionState state;
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
    int stableFrames = 0;
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

} // namespace core::dsl::runtime
