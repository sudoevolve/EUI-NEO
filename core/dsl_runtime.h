#pragma once

#include "core/dsl.h"
#include "core/platform/platform.h"
#include "core/input/input_state.h"
#include "core/render/image.h"
#include "core/render/primitive.h"
#include "core/render/render_backend.h"
#include "core/render/text.h"
#include "core/runtime/runtime_animation.h"
#include "core/runtime/runtime_dirty.h"
#include "core/runtime/runtime_geometry.h"
#include "core/runtime/runtime_hit_test.h"
#include "core/runtime/runtime_instances.h"
#include "core/runtime/runtime_state_bindings.h"
#include "core/window/window_backend.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <unordered_map>
#include <vector>

namespace core::dsl {

class Runtime {
public:
    bool initialize();

    bool initialize(core::window::Handle window);

    void setKeyEventHandler(std::function<void(const KeyEvent&)> handler) {
        keyEventHandler_ = std::move(handler);
    }

    template <typename ComposeFn>
    void compose(const std::string& pageId, float logicalWidth, float logicalHeight, ComposeFn&& composeFn);

    bool update(core::window::Handle window, float deltaSeconds, float pointerScale, float dpiScale, bool inputEnabled = true);

    bool isAnimating() const;

    bool composeRequested() const;

    bool paintRequested() const;

    void requestFullPaint();

    void render(int windowWidth, int windowHeight, float dpiScale, const Color& clearColor);

    void render(int windowWidth, int windowHeight, float dpiScale);

    void shutdown(bool releaseCachedImageTextures = true);

    void releaseGraphicsResources(bool releaseCachedImageTextures = true);

private:
    template <typename Fn>
    void forEachElement(Fn&& fn) const;

    template <typename Fn>
    static void forEachElement(const Element& element, Fn&& fn);

    std::vector<runtime::ElementSnapshot> collectElementStructure() const;

    void applyCursor(core::window::Handle window);

    void destroyCursors();

    void addDirtyRect(const Rect& rect);

    void addDirtyUnion(const Rect& before, const Rect& after);

    void promoteBackdropBlurDirtyRegions(float dpiScale);

    void expandBackdropBlurDirtyRegions(const Element& element,
                                        float dpiScale,
                                        const RenderTransform& inheritedTransform,
                                        Rect& mergedDirty,
                                        bool& expanded);

    Transform pointerRuntimeTransform(const Element& element,
                                      const PointerEvent& event,
                                      float dpiScale,
                                      const std::string& hoverTargetId) const;

    bool updateFrameTarget(const Element& element);

    Rect visualDirtyRectForElement(const Element& element,
                                   float dpiScale,
                                   const RenderTransform& inheritedTransform) const;

    void updateExplicitDirtyKey(const Element& element,
                                float dpiScale,
                                const RenderTransform& inheritedTransform);

    void updateElementTree(const PointerEvent& event,
                           float deltaSeconds,
                           float dpiScale,
                           const std::string& hoverTargetId);

    bool canReuseStaticSubtree(const Element& element,
                               const PointerEvent& event,
                               float dpiScale,
                               const RenderTransform& inheritedTransform,
                               bool ancestorFrameChanged,
                               bool ancestorDisabled) const;

    bool elementHasActiveAnimation(const Element& element) const;

    runtime::PaintBoundsInstance updateElementTree(const Element& element,
                                                   const PointerEvent& event,
                                                   float deltaSeconds,
                                                   float dpiScale,
                                                   const std::string& hoverTargetId,
                                                   const RenderTransform& inheritedTransform,
                                                   bool ancestorFrameChanged,
                                                   bool ancestorDisabled);

    std::string capturedInteractionId() const;

    bool isElementInDisabledTree(const std::string& id) const;

    bool findElementDisabledState(const Element& element,
                                  const std::string& id,
                                  bool ancestorDisabled,
                                  bool& disabledTree) const;

    std::string hitTestInteractive(const PointerEvent& event, float dpiScale) const;

    std::string hitTestFocusable(const PointerEvent& event, float dpiScale) const;

    std::string hitTestScrollable(const PointerEvent& event, float dpiScale) const;

    std::string resolveHoverTarget(const PointerEvent& event, float dpiScale, bool inputEnabled);

    bool canReuseHoverTarget(const PointerEvent& event, float dpiScale) const;

    template <typename Predicate>
    std::string hitTest(const PointerEvent& event, float dpiScale, Predicate&& predicate) const;

    template <typename Predicate>
    bool hitTestElement(const Element& element,
                        const PointerEvent& event,
                        float dpiScale,
                        const RenderTransform& inheritedTransform,
                        Predicate& predicate,
                        bool hasClip,
                        const Rect& clipRect,
                        bool ancestorDisabled,
                        std::string& targetId) const;

    bool hitTestFocusableElement(const Element& element,
                                 const PointerEvent& event,
                                 float dpiScale,
                                 const RenderTransform& inheritedTransform,
                                 bool hasClip,
                                 const Rect& clipRect,
                                 bool ancestorDisabled,
                                 std::string& targetId) const;

    void setFocusedId(const std::string& id);

    void updateScroll(const ScrollEvent& event, const std::string& targetId);

    void updateTextInput(const TextInputEvent& event);

    void updateKeyInput(const std::vector<KeyEvent>& events);

    void updateImeCursorRect(core::window::Handle window, float dpiScale);

    void updateInteraction(const Element& element,
                           const PointerEvent& event,
                           float dpiScale,
                           const std::string& hoverTargetId,
                           const RenderTransform& inheritedTransform);

    Transform currentElementTransform(const Element& element) const;

    TransformMatrix hitMatrixForElement(const Element& element, float dpiScale, const Rect& bounds, const RenderTransform& renderTransform) const;

    bool hitContains(const Element& element,
                     const PointerEvent& event,
                     float dpiScale,
                     const Rect& bounds,
                     const RenderTransform& renderTransform) const;

    void updateTimer(const Element& element, float deltaSeconds);

    void updateFrameCallback(const Element& element, float deltaSeconds);

    void updateLayoutElement(const Element& element,
                             float deltaSeconds,
                             float dpiScale,
                             const RenderTransform& inheritedTransform,
                             const PointerEvent& event,
                             const std::string& hoverTargetId);

    void updateRect(const Element& element,
                    float deltaSeconds,
                    float dpiScale,
                    const RenderTransform& inheritedTransform,
                    bool snapFrame);

    void updatePolygon(const Element& element,
                       float deltaSeconds,
                       float dpiScale,
                       const RenderTransform& inheritedTransform,
                       bool snapFrame);

    void updateText(const Element& element,
                    float deltaSeconds,
                    float dpiScale,
                    const RenderTransform& inheritedTransform,
                    bool snapFrame);

    void updateImage(const Element& element,
                     float deltaSeconds,
                     float dpiScale,
                     const RenderTransform& inheritedTransform,
                     bool snapFrame);

    void updateShaderToy(const Element& element,
                         const PointerEvent& event,
                         float deltaSeconds,
                         float dpiScale,
                         const RenderTransform& inheritedTransform,
                         bool snapFrame);

    runtime::DependentVisualState dependentVisualStateForElement(const Element& element,
                                                        float dpiScale,
                                                        const RenderTransform& inheritedTransform) const;

    void updateDependentVisualDirtyRegions(float dpiScale);

    void updateDependentVisualDirtyRegions(const Element& element,
                                           float dpiScale,
                                           const RenderTransform& inheritedTransform);

    void syncScrollStateElement(const Element& element);

    void syncSliderStateElement(const Element& element);

    void syncScrollStateBindings();

    float scrollStepFor(const Element& element) const;

    void addScrollDirtyRect(const runtime::ScrollStateInstance& instance);

    void addSliderDirtyRect(const runtime::SliderStateInstance& instance);

    void setScrollOffset(const std::string& stateId, float offset);

    void applyRuntimeScroll(const Element& element, float delta);

    void updateScrollMotion(float deltaSeconds);

    void beginRuntimeScrollDrag(const Element& element);

    void updateRuntimeScrollDrag(const Element& element, double dragDeltaY, float dpiScale);

    float sliderValueFromPointer(const Element& element, double pointerX, float dpiScale) const;

    void setSliderValue(const std::string& stateId, float value, bool dragging);

    void updateRuntimeSlider(const Element& element, double pointerX, float dpiScale, bool dragging);

    Ui ui_;
    runtime::InstanceStore instances_;
    std::vector<runtime::ElementSnapshot> elementStructure_;
    std::vector<runtime::LogicalDirtyRect> dirtyRects_;
    bool paintRequested_ = true;
    bool animating_ = false;
    bool composeRequested_ = false;
    bool fullPaintRequested_ = true;
    bool wantsHandCursor_ = false;
    bool fullTreeUpdateRequested_ = true;
    bool pruneInstancesRequested_ = true;
    bool previousFrameAnimating_ = false;
    bool hoverTargetCacheValid_ = false;
    PointerEvent hoverTargetCacheEvent_;
    float hoverTargetCacheDpiScale_ = 0.0f;
    std::string hoverTargetCacheId_;
    std::string focusedId_;
    std::function<void(const KeyEvent&)> keyEventHandler_;
    RenderTransform focusedElementRenderTransform_;
    bool focusedElementRenderTransformValid_ = false;
    float logicalWidth_ = 0.0f;
    float logicalHeight_ = 0.0f;
    std::uint64_t updateFrameToken_ = 0;
    core::window::CursorHandle arrowCursor_ = nullptr;
    core::window::CursorHandle handCursor_ = nullptr;
    core::window::CursorHandle currentCursor_ = nullptr;
    core::window::Handle imeCursorWindow_ = nullptr;
    Rect imeCursorRect_;
    bool imeCursorRectValid_ = false;
};

} // namespace core::dsl

#include "core/runtime/runtime_render.h"
#include "core/runtime/runtime_lifecycle.h"
#include "core/runtime/runtime_input.h"
#include "core/runtime/runtime_update.h"
