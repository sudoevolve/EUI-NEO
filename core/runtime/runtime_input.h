#pragma once

namespace core::dsl {

inline std::string Runtime::capturedInteractionId() const {
    for (const auto& item : instances_.interactions) {
        if (item.second.state.active && ui_.find(item.first) && !isElementInDisabledTree(item.first)) {
            return item.first;
        }
    }
    return {};
}

inline bool Runtime::isElementInDisabledTree(const std::string& id) const {
    if (id.empty()) {
        return false;
    }

    bool disabledTree = false;
    const std::string resolvedId = ui_.resolveId(id);
    const std::vector<const Element*>& roots = ui_.orderedRoots();
    for (const Element* root : roots) {
        if (findElementDisabledState(*root, resolvedId, false, disabledTree)) {
            return disabledTree;
        }
    }
    return false;
}

inline bool Runtime::findElementDisabledState(
    const Element& element,
    const std::string& id,
    bool ancestorDisabled,
    bool& disabledTree) const {
    const bool currentDisabledTree = ancestorDisabled || element.disabled;
    if (element.id == id) {
        disabledTree = currentDisabledTree;
        return true;
    }

    for (const auto& child : element.children) {
        if (findElementDisabledState(*child, id, currentDisabledTree, disabledTree)) {
            return true;
        }
    }
    return false;
}

inline std::string Runtime::hitTestInteractive(const PointerEvent& event, float dpiScale) const {
    return hitTest(event, dpiScale, [](const Element& element) {
        return element.interactive && !element.disabled;
    });
}

inline std::string Runtime::hitTestFocusable(const PointerEvent& event, float dpiScale) const {
    std::string targetId;
    const RenderTransform identity;
    const std::vector<const Element*>& roots = ui_.orderedRoots();
    for (auto it = roots.rbegin(); it != roots.rend(); ++it) {
        if (hitTestFocusableElement(**it, event, dpiScale, identity, false, {}, false, targetId)) {
            break;
        }
    }
    return targetId;
}

inline std::string Runtime::hitTestScrollable(const PointerEvent& event, float dpiScale) const {
    return hitTest(event, dpiScale, [](const Element& element) {
        return (!element.scrollStateId.empty() || static_cast<bool>(element.onScroll)) && !element.disabled;
    });
}

inline std::string Runtime::resolveHoverTarget(const PointerEvent& event, float dpiScale, bool inputEnabled) {
    const std::string capturedId = capturedInteractionId();
    if (!capturedId.empty()) {
        hoverTargetCacheValid_ = false;
        return capturedId;
    }

    if (inputEnabled && canReuseHoverTarget(event, dpiScale)) {
        return hoverTargetCacheId_;
    }

    const std::string targetId = inputEnabled ? hitTestInteractive(event, dpiScale) : std::string{};
    hoverTargetCacheValid_ = inputEnabled;
    hoverTargetCacheEvent_ = event;
    hoverTargetCacheDpiScale_ = dpiScale;
    hoverTargetCacheId_ = targetId;
    return targetId;
}

inline bool Runtime::canReuseHoverTarget(const PointerEvent& event, float dpiScale) const {
    if (!hoverTargetCacheValid_ ||
        fullTreeUpdateRequested_ ||
        pruneInstancesRequested_ ||
        previousFrameAnimating_ ||
        !closeEnough(hoverTargetCacheDpiScale_, dpiScale) ||
        hoverTargetCacheEvent_.x != event.x ||
        hoverTargetCacheEvent_.y != event.y ||
        event.deltaX != 0.0 ||
        event.deltaY != 0.0 ||
        hoverTargetCacheEvent_.buttons != event.buttons ||
        event.action != PointerAction::Move) {
        return false;
    }

    return hoverTargetCacheId_.empty() || ui_.find(hoverTargetCacheId_) != nullptr;
}

template <typename Predicate>
inline std::string Runtime::hitTest(const PointerEvent& event, float dpiScale, Predicate&& predicate) const {
    std::string targetId;
    const RenderTransform identity;
    const std::vector<const Element*>& roots = ui_.orderedRoots();
    for (auto it = roots.rbegin(); it != roots.rend(); ++it) {
        if (hitTestElement(**it, event, dpiScale, identity, predicate, false, {}, false, targetId)) {
            break;
        }
    }
    return targetId;
}

template <typename Predicate>
inline bool Runtime::hitTestElement(
    const Element& element,
    const PointerEvent& event,
    float dpiScale,
    const RenderTransform& inheritedTransform,
    Predicate& predicate,
    bool hasClip,
    const Rect& clipRect,
    bool ancestorDisabled,
    std::string& targetId) const {
    const bool disabledTree = ancestorDisabled || element.disabled;
    const RenderTransform renderTransform = instances_.renderTransform(element, dpiScale, inheritedTransform);
    Rect effectiveClip = clipRect;
    bool effectiveHasClip = hasClip;
    const Rect bounds = toPixelRect(element.frame, dpiScale);
    if (element.clip) {
        const Rect clipBounds = applyRenderTransform(bounds, renderTransform);
        if (effectiveHasClip) {
            if (!intersectRect(effectiveClip, clipBounds, effectiveClip)) {
                return false;
            }
        } else {
            effectiveClip = clipBounds;
            effectiveHasClip = true;
        }
    }

    if (effectiveHasClip && !effectiveClip.contains(event.x, event.y)) {
        return false;
    }

    const std::vector<const Element*>& children = element.orderedChildren;
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        if (hitTestElement(**it, event, dpiScale, renderTransform, predicate, effectiveHasClip, effectiveClip, disabledTree, targetId)) {
            return true;
        }
    }

    if (!disabledTree && predicate(element) && hitContains(element, event, dpiScale, bounds, renderTransform)) {
        targetId = element.id;
        return true;
    }
    return false;
}

inline bool Runtime::hitTestFocusableElement(
    const Element& element,
    const PointerEvent& event,
    float dpiScale,
    const RenderTransform& inheritedTransform,
    bool hasClip,
    const Rect& clipRect,
    bool ancestorDisabled,
    std::string& targetId) const {
    const bool disabledTree = ancestorDisabled || element.disabled;
    const RenderTransform renderTransform = instances_.renderTransform(element, dpiScale, inheritedTransform);
    Rect effectiveClip = clipRect;
    bool effectiveHasClip = hasClip;
    const Rect bounds = toPixelRect(element.frame, dpiScale);
    if (element.clip) {
        const Rect clipBounds = applyRenderTransform(bounds, renderTransform);
        if (effectiveHasClip) {
            if (!intersectRect(effectiveClip, clipBounds, effectiveClip)) {
                return false;
            }
        } else {
            effectiveClip = clipBounds;
            effectiveHasClip = true;
        }
    }

    if (effectiveHasClip && !effectiveClip.contains(event.x, event.y)) {
        return false;
    }

    const std::vector<const Element*>& children = element.orderedChildren;
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        if (hitTestFocusableElement(**it, event, dpiScale, renderTransform, effectiveHasClip, effectiveClip, disabledTree, targetId)) {
            return true;
        }
    }

    if (disabledTree) {
        return false;
    }
    if (!hitContains(element, event, dpiScale, bounds, renderTransform)) {
        return false;
    }
    if (element.preserveFocusOnPress) {
        targetId = focusedId_;
        return true;
    }
    if (element.focusable && !element.disabled) {
        targetId = element.id;
        return true;
    }
    if (element.interactive && !element.disabled) {
        targetId.clear();
        return true;
    }
    return false;
}

inline void Runtime::setFocusedId(const std::string& id) {
    if (focusedId_ == id) {
        return;
    }

    const std::string oldId = focusedId_;
    focusedId_ = id;
    imeCursorRectValid_ = false;
    ui_.setFocusedId(focusedId_);

    if (const Element* oldElement = ui_.find(oldId)) {
        if (oldElement->onFocusChanged) {
            oldElement->onFocusChanged(false);
        }
    }
    if (const Element* newElement = ui_.find(focusedId_)) {
        if (newElement->onFocusChanged) {
            newElement->onFocusChanged(true);
        }
    }
    composeRequested_ = true;
    paintRequested_ = true;
}

inline void Runtime::updateScroll(const ScrollEvent& event, const std::string& targetId) {
    const Element* element = targetId.empty() ? nullptr : ui_.find(targetId);
    const std::string activeScrollStateId = element != nullptr && !element->disabled
        ? element->scrollStateId
        : std::string{};
    for (auto& entry : instances_.scrollStates) {
        if (entry.first != activeScrollStateId) {
            entry.second.velocity = 0.0f;
        }
    }

    if (element != nullptr) {
        if (!element->scrollStateId.empty() && !element->disabled) {
            applyRuntimeScroll(*element, -static_cast<float>(event.y) * scrollStepFor(*element));
            return;
        }
        if (element->onScroll && !element->disabled) {
            element->onScroll(event);
            composeRequested_ = true;
            paintRequested_ = true;
        }
    }
}

inline void Runtime::updateTextInput(const TextInputEvent& event) {
    if (focusedId_.empty()) {
        return;
    }

    if (isElementInDisabledTree(focusedId_)) {
        setFocusedId({});
        return;
    }

    if (const Element* element = ui_.find(focusedId_)) {
        if (element->onTextInput && !element->disabled) {
            element->onTextInput(event);
            composeRequested_ = true;
            paintRequested_ = true;
        }
    }
}

inline void Runtime::updateKeyInput(const std::vector<KeyEvent>& events) {
    const Element* focused = focusedId_.empty() ? nullptr : ui_.find(focusedId_);
    if (focused != nullptr && (focused->disabled || isElementInDisabledTree(focusedId_))) {
        setFocusedId({});
        focused = nullptr;
    }

    for (const KeyEvent& key : events) {
        bool handled = false;
        if (focused != nullptr && focused->onKeyEvent) {
            handled = focused->onKeyEvent(key);
            if (handled) {
                composeRequested_ = true;
                paintRequested_ = true;
            }
        }
        if (!handled && keyEventHandler_) {
            keyEventHandler_(key);
            composeRequested_ = true;
            paintRequested_ = true;
        }
    }
}

inline void Runtime::updateImeCursorRect(core::window::Handle window, float dpiScale) {
    if (window == nullptr) {
        imeCursorRectValid_ = false;
        return;
    }
    if (focusedId_.empty()) {
        imeCursorRectValid_ = false;
        return;
    }

    const Element* element = ui_.find(focusedId_);
    if (element == nullptr || isElementInDisabledTree(focusedId_) || !element->hasImeRect) {
        imeCursorRectValid_ = false;
        return;
    }

    if (!focusedElementRenderTransformValid_) {
        imeCursorRectValid_ = false;
        return;
    }

    const Rect elementBounds = toPixelRect(element->frame, dpiScale);
    const TransformMatrix transform = hitMatrixForElement(
        *element,
        dpiScale,
        elementBounds,
        focusedElementRenderTransform_);
    const Rect pixelRect = imeCursorPixelRect(*element, dpiScale, transform);
    if (imeCursorRectValid_ &&
        imeCursorWindow_ == window &&
        closeEnough(imeCursorRect_, pixelRect)) {
        return;
    }
    imeCursorWindow_ = window;
    imeCursorRect_ = pixelRect;
    imeCursorRectValid_ = true;
    core::platform::setImeCursorRect(
        window,
        pixelRect.x,
        pixelRect.y,
        pixelRect.width,
        pixelRect.height);
}

inline void Runtime::updateInteraction(
    const Element& element,
    const PointerEvent& event,
    float dpiScale,
    const std::string& hoverTargetId,
    const RenderTransform& inheritedTransform) {
    if (!element.interactive && instances_.interactions.find(element.id) == instances_.interactions.end()) {
        return;
    }

    runtime::InteractionInstance& instance = instances_.interaction(element.id);
    const Rect bounds = toPixelRect(element.frame, dpiScale);
    const RenderTransform renderTransform = instances_.renderTransform(element, dpiScale, inheritedTransform);
    const bool enabled = element.interactive && !element.disabled;
    const bool topmostHover = enabled && element.id == hoverTargetId;
    const bool wasHover = instance.state.hover;
    const Rect interactionBounds = applyTransformMatrix(
        bounds,
        hitMatrixForElement(element, dpiScale, bounds, renderTransform));
    instance.state.update(interactionBounds, event, topmostHover,
                          element.acceptedButtons, element.dragThreshold, enabled);

    if (enabled && wasHover != instance.state.hover && element.onHoverChanged) {
        element.onHoverChanged(instance.state.hover);
        composeRequested_ = true;
        paintRequested_ = true;
    }

    if (enabled && instance.state.hover && element.cursor == CursorShape::Hand) {
        wantsHandCursor_ = true;
    }

    if (enabled && topmostHover && element.onMove &&
        (event.deltaX != 0.0 || event.deltaY != 0.0 || wasHover != instance.state.hover)) {
        PointerEvent logicalEvent = event;
        logicalEvent.x /= dpiScale;
        logicalEvent.y /= dpiScale;
        logicalEvent.deltaX /= dpiScale;
        logicalEvent.deltaY /= dpiScale;
        const Rect logicalBounds{
            interactionBounds.x / dpiScale,
            interactionBounds.y / dpiScale,
            interactionBounds.width / dpiScale,
            interactionBounds.height / dpiScale
        };
        if (element.onMove(logicalEvent, logicalBounds)) {
            composeRequested_ = true;
            paintRequested_ = true;
        }
    }

    if (enabled && topmostHover && event.isPress(PointerButton::Right) && element.onContextMenu) {
        instance.contextActive = true;
        instance.contextDragged = false;
        instance.contextStartX = event.x;
        instance.contextStartY = event.y;
    }
    if (instance.contextActive && event.isDown(PointerButton::Right) &&
        (std::fabs(event.x - instance.contextStartX) > element.dragThreshold ||
         std::fabs(event.y - instance.contextStartY) > element.dragThreshold)) {
        instance.contextDragged = true;
    }
    const bool contextEnded = instance.contextActive && event.button == PointerButton::Right &&
        (event.action == PointerAction::Release || event.action == PointerAction::Cancel);
    if (enabled && topmostHover && contextEnded && event.action == PointerAction::Release &&
        !instance.contextDragged && element.onContextMenu) {
        PointerEvent logicalEvent = event;
        logicalEvent.x /= dpiScale;
        logicalEvent.y /= dpiScale;
        logicalEvent.deltaX /= dpiScale;
        logicalEvent.deltaY /= dpiScale;
        const Rect logicalBounds{
            interactionBounds.x / dpiScale,
            interactionBounds.y / dpiScale,
            interactionBounds.width / dpiScale,
            interactionBounds.height / dpiScale
        };
        element.onContextMenu(logicalEvent, logicalBounds);
        composeRequested_ = true;
        paintRequested_ = true;
    }
    if (contextEnded) {
        instance.contextActive = false;
        instance.contextDragged = false;
    }

    if (enabled && instance.state.pressStarted && !element.scrollDragSourceId.empty()) {
        beginRuntimeScrollDrag(element);
        paintRequested_ = true;
    }

    if (enabled && instance.state.pressStarted && !element.sliderInputSourceId.empty()) {
        updateRuntimeSlider(element, event.x, dpiScale, true);
        paintRequested_ = true;
        return;
    }

    if (enabled && instance.state.pressStarted && element.onPress) {
        element.onPress(event, interactionBounds);
        composeRequested_ = true;
        paintRequested_ = true;
    }

    if (enabled && instance.state.clicked && element.onClick) {
        element.onClick();
        composeRequested_ = true;
        paintRequested_ = true;
    }

    if (enabled && instance.state.released && element.onRelease) {
        element.onRelease(event, interactionBounds);
        composeRequested_ = true;
        paintRequested_ = true;
    }

    if (enabled && instance.state.released && !element.sliderInputSourceId.empty()) {
        if (auto state = instances_.sliderStates.find(element.sliderInputSourceId); state != instances_.sliderStates.end()) {
            state->second.dragging = false;
        }
        composeRequested_ = true;
        paintRequested_ = true;
        return;
    }

    if (enabled && instance.state.pressed && !element.scrollDragSourceId.empty() &&
        (event.deltaX != 0.0 || event.deltaY != 0.0 || instance.state.drag)) {
        updateRuntimeScrollDrag(element, instance.state.dragDeltaY, dpiScale);
        return;
    }

    if (enabled && instance.state.pressed && !element.sliderInputSourceId.empty() &&
        (event.deltaX != 0.0 || event.deltaY != 0.0 || instance.state.drag)) {
        updateRuntimeSlider(element, event.x, dpiScale, true);
        return;
    }

    if (enabled && instance.state.pressed && element.onDrag &&
        (event.deltaX != 0.0 || event.deltaY != 0.0 || instance.state.drag)) {
        element.onDrag({
            event.x,
            event.y,
            event.deltaX,
            event.deltaY,
            instance.state.dragDeltaX,
            instance.state.dragDeltaY,
            instance.state.activeButton,
            event.buttons,
            event.modifiers
        });
        composeRequested_ = true;
        paintRequested_ = true;
    }
}

inline Transform Runtime::currentElementTransform(const Element& element) const {
    if (element.kind == ElementKind::Rect) {
        const auto instance = instances_.rects.find(element.id);
        if (instance != instances_.rects.end()) {
            return instance->second.transform.value();
        }
    } else if (element.kind == ElementKind::Polygon) {
        const auto instance = instances_.polygons.find(element.id);
        if (instance != instances_.polygons.end()) {
            return instance->second.transform.value();
        }
    } else if (element.kind == ElementKind::Text) {
        const auto instance = instances_.texts.find(element.id);
        if (instance != instances_.texts.end()) {
            return instance->second.transform.value();
        }
    } else if (element.kind == ElementKind::Image || element.kind == ElementKind::Svg) {
        const auto instance = instances_.images.find(element.id);
        if (instance != instances_.images.end()) {
            return instance->second.transform.value();
        }
    } else if (element.kind == ElementKind::Shadertoy) {
        const auto instance = instances_.shaderToys.find(element.id);
        if (instance != instances_.shaderToys.end()) {
            return instance->second.transform.value();
        }
    } else if (element.kind == ElementKind::Row ||
               element.kind == ElementKind::Column ||
               element.kind == ElementKind::Stack) {
        const auto instance = instances_.layouts.find(element.id);
        if (instance != instances_.layouts.end()) {
            return instance->second.transform.value();
        }
    }
    return element.transform;
}

inline TransformMatrix Runtime::hitMatrixForElement(const Element& element, float dpiScale, const Rect& bounds, const RenderTransform& renderTransform) const {
    if (element.kind == ElementKind::Rect ||
        element.kind == ElementKind::Polygon ||
        element.kind == ElementKind::Text ||
        element.kind == ElementKind::Image || element.kind == ElementKind::Svg ||
        element.kind == ElementKind::Shadertoy) {
        return combinedPrimitiveMatrix(renderTransform, bounds, scaleTransform(currentElementTransform(element), dpiScale));
    }
    return renderTransform.matrix;
}

inline bool Runtime::hitContains(
    const Element& element,
    const PointerEvent& event,
    float dpiScale,
    const Rect& bounds,
    const RenderTransform& renderTransform) const {
    if (element.hitTestMode == HitTestMode::None) {
        return false;
    }
    if (element.hitTestMode == HitTestMode::Transformed || renderTransform.active) {
        TransformMatrix inverse;
        if (!inverseMatrix(hitMatrixForElement(element, dpiScale, bounds, renderTransform), inverse)) {
            return false;
        }
        PointerEvent localEvent = event;
        const Vec2 local = core::transformPoint(inverse, static_cast<float>(event.x), static_cast<float>(event.y));
        localEvent.x = local.x;
        localEvent.y = local.y;
        if (element.kind == ElementKind::Polygon) {
            return polygonContains(element, localEvent.x, localEvent.y, dpiScale, bounds);
        }
        return bounds.contains(localEvent.x, localEvent.y);
    }
    if (element.kind == ElementKind::Polygon) {
        return polygonContains(element, event.x, event.y, dpiScale, bounds);
    }
    return bounds.contains(event.x, event.y);
}

} // namespace core::dsl
