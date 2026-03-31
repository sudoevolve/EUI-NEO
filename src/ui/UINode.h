#pragma once

#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <type_traits>
#include <utility>
#include "UIPrimitive.h"
#include <string>
#include <vector>

namespace EUINEO {

class UINode {
public:
    explicit UINode(std::string key) : key_(std::move(key)) {}
    virtual ~UINode() = default;

    const std::string& key() const {
        return key_;
    }

    void beginCompose(std::uint64_t composeStamp) {
        composeStamp_ = composeStamp;
        composeBuildHash_ = ComposeHashSeed();
        recomposeRequested_ = false;
        resetDefaults();
    }

    void finishCompose() {
        const bool primitiveChanged = !PrimitiveEq(primitive_, composedPrimitive_);
        const bool composeHashChanged = composeBuildHash_ != composedHash_;
        if (!hasComposeSnapshot_ || primitiveChanged || composeHashChanged) {
            cacheDirty_ = true;
        }

        composedPrimitive_ = primitive_;
        composedHash_ = composeBuildHash_;
        hasComposeSnapshot_ = true;
    }

    bool composedIn(std::uint64_t composeStamp) const {
        return composeStamp_ == composeStamp;
    }

    std::uint64_t composeStamp() const {
        return composeStamp_;
    }

    UIPrimitive& primitive() {
        return primitive_;
    }

    const UIPrimitive& primitive() const {
        return primitive_;
    }

    int zIndex() const {
        return primitive_.zIndex;
    }

    bool hasExplicitZIndex() const {
        return primitive_.hasExplicitZIndex;
    }

    RenderLayer renderLayer() const {
        return primitive_.renderLayer;
    }

    bool visible() const {
        return primitive_.visible;
    }

    bool cacheDirty() const {
        return cacheDirty_;
    }

    void clearCacheDirty() {
        cacheDirty_ = false;
    }

    bool consumeRecomposeRequest() {
        if (!recomposeRequested_) {
            return false;
        }
        recomposeRequested_ = false;
        return true;
    }

    void trackComposeMarker(const char* tag) {
        HashCString(composeBuildHash_, tag);
    }

    template <typename T>
    bool trackComposeValue(const char* tag, const T& value) {
        HashCString(composeBuildHash_, tag);
        return HashComposeValue(composeBuildHash_, value);
    }

    void forceComposeDirty() {
        cacheDirty_ = true;
    }

    virtual RectFrame paintBounds() const {
        return measurePrimitivePaintBounds();
    }

    virtual RectFrame layoutBounds() const {
        return RectFrame{0.0f, 0.0f, primitive_.width, primitive_.height};
    }

    virtual bool wantsContinuousUpdate() const {
        return false;
    }

    virtual bool usesCachedSurface() const {
        return true;
    }

    virtual const char* typeName() const = 0;
    virtual void update() = 0;
    virtual void draw() = 0;

protected:
    virtual void resetDefaults() = 0;

    RectFrame clipPaintBounds(const RectFrame& bounds) const {
        RectFrame result = bounds;
        if (primitive_.hasClipRect) {
            const float x1 = std::max(result.x, primitive_.clipRect.x);
            const float y1 = std::max(result.y, primitive_.clipRect.y);
            const float x2 = std::min(result.x + result.width, primitive_.clipRect.x + primitive_.clipRect.width);
            const float y2 = std::min(result.y + result.height, primitive_.clipRect.y + primitive_.clipRect.height);
            result.x = x1;
            result.y = y1;
            result.width = std::max(0.0f, x2 - x1);
            result.height = std::max(0.0f, y2 - y1);
        }
        return result;
    }

    RectFrame measurePrimitivePaintBounds() const {
        const RectFrame frame = PrimitiveFrame(primitive_);
        const RectStyle style = MakeStyle(primitive_);
        return measurePaintBounds(frame, style);
    }

    RectFrame measurePaintBounds(const RectFrame& frame, const RectStyle& style, bool clip = true) const {
        const RectBounds bounds = Renderer::MeasureRectBounds(frame.x, frame.y, frame.width, frame.height, style);
        RectFrame result{bounds.x, bounds.y, bounds.w, bounds.h};
        return clip ? clipPaintBounds(result) : result;
    }

    RectFrame expandPaintBounds(const RectFrame& bounds, float left, float top, float right, float bottom) const {
        return clipPaintBounds(RectFrame{
            bounds.x - left,
            bounds.y - top,
            bounds.width + left + right,
            bounds.height + top + bottom
        });
    }

    RectFrame expandPrimitivePaintBounds(float left, float top, float right, float bottom) const {
        return expandPaintBounds(PrimitiveFrame(primitive_), left, top, right, bottom);
    }

    RectFrame unionPaintBounds(const RectFrame& lhs, const RectFrame& rhs) const {
        if (lhs.width <= 0.0f || lhs.height <= 0.0f) {
            return rhs;
        }
        if (rhs.width <= 0.0f || rhs.height <= 0.0f) {
            return lhs;
        }

        const float x1 = std::min(lhs.x, rhs.x);
        const float y1 = std::min(lhs.y, rhs.y);
        const float x2 = std::max(lhs.x + lhs.width, rhs.x + rhs.width);
        const float y2 = std::max(lhs.y + lhs.height, rhs.y + rhs.height);
        return RectFrame{x1, y1, x2 - x1, y2 - y1};
    }

    RectFrame stackedPaintBoundsBelow(const RectFrame& anchorBounds, float stackedHeight,
                                      float overlap = 0.0f, bool clipStacked = true) const {
        if (stackedHeight <= 0.0f) {
            return anchorBounds;
        }

        RectFrame stackedBounds{
            anchorBounds.x,
            anchorBounds.y + anchorBounds.height - overlap,
            anchorBounds.width,
            stackedHeight + overlap
        };
        if (clipStacked) {
            stackedBounds = clipPaintBounds(stackedBounds);
        }
        return unionPaintBounds(anchorBounds, stackedBounds);
    }

    bool containsPoint(float x, float y) const {
        return PrimitiveContains(primitive_, x, y);
    }

    bool hovered() const {
        return primitive_.enabled && containsPoint(State.mouseX, State.mouseY);
    }

    bool animateTowards(float& value, float target, float speed,
                        float settleEpsilon = 0.01f, float compareEpsilon = 0.001f) const {
        if (std::abs(value - target) <= compareEpsilon) {
            return false;
        }

        const float previous = value;
        value = Lerp(value, target, speed);
        if (std::abs(value - target) < settleEpsilon) {
            value = target;
        }
        return std::abs(previous - value) > 0.0001f;
    }

    void applyPopupPresentationDefaults(int zIndex = 220) {
        primitive_.renderLayer = RenderLayer::Popup;
        primitive_.clipToParent = false;
        primitive_.zIndex = std::max(primitive_.zIndex, zIndex);
        primitive_.hasExplicitZIndex = true;
    }

    void requestVisualRepaint(float duration = 0.0f) {
        cacheDirty_ = true;
        Renderer::RequestRepaint(duration);
    }

    void requestComposeRebuild(float duration = 0.0f) {
        recomposeRequested_ = true;
        requestVisualRepaint(duration);
    }

    UIPrimitive primitive_;

private:
    static constexpr float ComposeEpsilon() {
        return 0.0001f;
    }

    static std::uint64_t ComposeHashSeed() {
        return 1469598103934665603ull;
    }

    static void HashBytes(std::uint64_t& hash, const void* data, std::size_t size) {
        const unsigned char* bytes = static_cast<const unsigned char*>(data);
        for (std::size_t index = 0; index < size; ++index) {
            hash ^= static_cast<std::uint64_t>(bytes[index]);
            hash *= 1099511628211ull;
        }
    }

    static void HashCString(std::uint64_t& hash, const char* value) {
        if (value == nullptr) {
            const std::uint64_t marker = 0;
            HashBytes(hash, &marker, sizeof(marker));
            return;
        }
        const std::size_t length = std::char_traits<char>::length(value);
        HashBytes(hash, &length, sizeof(length));
        HashBytes(hash, value, length);
    }

    template <typename T>
    static std::enable_if_t<std::is_integral_v<T> || std::is_enum_v<T>, bool>
    HashComposeValue(std::uint64_t& hash, const T& value) {
        HashBytes(hash, &value, sizeof(value));
        return true;
    }

    static bool HashComposeValue(std::uint64_t& hash, const float& value) {
        std::uint32_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        HashBytes(hash, &bits, sizeof(bits));
        return true;
    }

    static bool HashComposeValue(std::uint64_t& hash, const double& value) {
        std::uint64_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        HashBytes(hash, &bits, sizeof(bits));
        return true;
    }

    static bool HashComposeValue(std::uint64_t& hash, const std::string& value) {
        const std::size_t length = value.size();
        HashBytes(hash, &length, sizeof(length));
        if (!value.empty()) {
            HashBytes(hash, value.data(), value.size());
        }
        return true;
    }

    static bool HashComposeValue(std::uint64_t& hash, const char* value) {
        HashCString(hash, value);
        return true;
    }

    static bool HashComposeValue(std::uint64_t& hash, const Color& value) {
        return HashComposeValue(hash, value.r) &&
               HashComposeValue(hash, value.g) &&
               HashComposeValue(hash, value.b) &&
               HashComposeValue(hash, value.a);
    }

    static bool HashComposeValue(std::uint64_t& hash, const RectGradient& value) {
        return HashComposeValue(hash, value.enabled) &&
               HashComposeValue(hash, value.topLeft) &&
               HashComposeValue(hash, value.topRight) &&
               HashComposeValue(hash, value.bottomLeft) &&
               HashComposeValue(hash, value.bottomRight);
    }

    static bool HashComposeValue(std::uint64_t& hash, const RectTransform& value) {
        return HashComposeValue(hash, value.translateX) &&
               HashComposeValue(hash, value.translateY) &&
               HashComposeValue(hash, value.scaleX) &&
               HashComposeValue(hash, value.scaleY) &&
               HashComposeValue(hash, value.rotationDegrees);
    }

    static bool HashComposeValue(std::uint64_t& hash, const RectFrame& value) {
        return HashComposeValue(hash, value.x) &&
               HashComposeValue(hash, value.y) &&
               HashComposeValue(hash, value.width) &&
               HashComposeValue(hash, value.height);
    }

    template <typename T>
    static bool HashComposeValue(std::uint64_t& hash, const std::vector<T>& values) {
        const std::size_t count = values.size();
        HashBytes(hash, &count, sizeof(count));
        for (const T& value : values) {
            if (!HashComposeValue(hash, value)) {
                return false;
            }
        }
        return true;
    }

    template <typename T>
    static std::enable_if_t<!std::is_integral_v<T> && !std::is_enum_v<T>, bool>
    HashComposeValue(std::uint64_t& hash, const T& value) {
        (void)hash;
        (void)value;
        return false;
    }

    static bool FloatEq(float lhs, float rhs, float epsilon = ComposeEpsilon()) {
        return std::abs(lhs - rhs) <= epsilon;
    }

    static bool ColorEq(const Color& lhs, const Color& rhs, float epsilon = ComposeEpsilon()) {
        return FloatEq(lhs.r, rhs.r, epsilon) &&
               FloatEq(lhs.g, rhs.g, epsilon) &&
               FloatEq(lhs.b, rhs.b, epsilon) &&
               FloatEq(lhs.a, rhs.a, epsilon);
    }

    static bool GradientEq(const RectGradient& lhs, const RectGradient& rhs, float epsilon = ComposeEpsilon()) {
        return lhs.enabled == rhs.enabled &&
               ColorEq(lhs.topLeft, rhs.topLeft, epsilon) &&
               ColorEq(lhs.topRight, rhs.topRight, epsilon) &&
               ColorEq(lhs.bottomLeft, rhs.bottomLeft, epsilon) &&
               ColorEq(lhs.bottomRight, rhs.bottomRight, epsilon);
    }

    static bool ShadowEq(const UIShadow& lhs, const UIShadow& rhs, float epsilon = ComposeEpsilon()) {
        return FloatEq(lhs.blur, rhs.blur, epsilon) &&
               FloatEq(lhs.offsetX, rhs.offsetX, epsilon) &&
               FloatEq(lhs.offsetY, rhs.offsetY, epsilon) &&
               ColorEq(lhs.color, rhs.color, epsilon);
    }

    static bool ClipEq(const UIClipRect& lhs, const UIClipRect& rhs, float epsilon = ComposeEpsilon()) {
        return FloatEq(lhs.x, rhs.x, epsilon) &&
               FloatEq(lhs.y, rhs.y, epsilon) &&
               FloatEq(lhs.width, rhs.width, epsilon) &&
               FloatEq(lhs.height, rhs.height, epsilon);
    }

    static bool PrimitiveEq(const UIPrimitive& lhs, const UIPrimitive& rhs, float epsilon = ComposeEpsilon()) {
        return FloatEq(lhs.x, rhs.x, epsilon) &&
               FloatEq(lhs.y, rhs.y, epsilon) &&
               FloatEq(lhs.contextOffsetX, rhs.contextOffsetX, epsilon) &&
               FloatEq(lhs.contextOffsetY, rhs.contextOffsetY, epsilon) &&
               FloatEq(lhs.width, rhs.width, epsilon) &&
               FloatEq(lhs.height, rhs.height, epsilon) &&
               FloatEq(lhs.minWidth, rhs.minWidth, epsilon) &&
               FloatEq(lhs.minHeight, rhs.minHeight, epsilon) &&
               FloatEq(lhs.maxWidth, rhs.maxWidth, epsilon) &&
               FloatEq(lhs.maxHeight, rhs.maxHeight, epsilon) &&
               FloatEq(lhs.scaleX, rhs.scaleX, epsilon) &&
               FloatEq(lhs.scaleY, rhs.scaleY, epsilon) &&
               FloatEq(lhs.rotation, rhs.rotation, epsilon) &&
               FloatEq(lhs.translateX, rhs.translateX, epsilon) &&
               FloatEq(lhs.translateY, rhs.translateY, epsilon) &&
               lhs.anchor == rhs.anchor &&
               FloatEq(lhs.rounding, rhs.rounding, epsilon) &&
               ColorEq(lhs.background, rhs.background, epsilon) &&
               GradientEq(lhs.gradient, rhs.gradient, epsilon) &&
               FloatEq(lhs.borderWidth, rhs.borderWidth, epsilon) &&
               ColorEq(lhs.borderColor, rhs.borderColor, epsilon) &&
               FloatEq(lhs.blur, rhs.blur, epsilon) &&
               ShadowEq(lhs.shadow, rhs.shadow, epsilon) &&
               FloatEq(lhs.opacity, rhs.opacity, epsilon) &&
               lhs.visible == rhs.visible &&
               lhs.enabled == rhs.enabled &&
               lhs.renderLayer == rhs.renderLayer &&
               lhs.zIndex == rhs.zIndex &&
               lhs.hasExplicitZIndex == rhs.hasExplicitZIndex &&
               lhs.clipToParent == rhs.clipToParent &&
               lhs.hasClipRect == rhs.hasClipRect &&
               (!lhs.hasClipRect || ClipEq(lhs.clipRect, rhs.clipRect, epsilon));
    }

    std::string key_;
    std::uint64_t composeStamp_ = 0;
    std::uint64_t composeBuildHash_ = ComposeHashSeed();
    std::uint64_t composedHash_ = 0;
    UIPrimitive composedPrimitive_;
    bool hasComposeSnapshot_ = false;
    bool cacheDirty_ = true;
    bool recomposeRequested_ = false;
};

} // namespace EUINEO
