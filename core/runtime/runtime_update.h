#pragma once

namespace core::dsl {

inline void Runtime::addDirtyRect(const Rect& rect) {
    if (rect.width <= 0.0f || rect.height <= 0.0f) {
        return;
    }
    dirtyRects_.push_back({rect.x, rect.y, rect.width, rect.height});
    paintRequested_ = true;
}

inline void Runtime::addDirtyUnion(const Rect& before, const Rect& after) {
    addDirtyRect(unionRect(before, after));
}

inline void Runtime::promoteBackdropBlurDirtyRegions(float dpiScale) {
    if (fullPaintRequested_ || dirtyRects_.empty() || !ui_.hasBackdropBlur()) {
        return;
    }

    Rect mergedDirty{};
    bool hasMergedDirty = false;
    for (const runtime::LogicalDirtyRect& dirty : dirtyRects_) {
        const Rect dirtyRect{dirty.x, dirty.y, dirty.width, dirty.height};
        mergedDirty = hasMergedDirty ? unionRect(mergedDirty, dirtyRect) : dirtyRect;
        hasMergedDirty = true;
    }
    if (!hasMergedDirty) {
        return;
    }

    bool expandedAny = false;
    bool expandedThisPass = false;
    const RenderTransform identity;
    do {
        expandedThisPass = false;
        const std::vector<const Element*>& roots = ui_.orderedRoots();
        for (const Element* root : roots) {
            expandBackdropBlurDirtyRegions(*root, dpiScale, identity, mergedDirty, expandedThisPass);
        }
        expandedAny = expandedAny || expandedThisPass;
    } while (expandedThisPass);

    if (expandedAny) {
        dirtyRects_.clear();
        dirtyRects_.push_back({mergedDirty.x, mergedDirty.y, mergedDirty.width, mergedDirty.height});
        paintRequested_ = true;
    }
}

inline void Runtime::expandBackdropBlurDirtyRegions(
    const Element& element,
    float dpiScale,
    const RenderTransform& inheritedTransform,
    Rect& mergedDirty,
    bool& expanded) {
    if (!element.subtreeHasBackdropBlur) {
        return;
    }

    const RenderTransform renderTransform = instances_.renderTransform(element, dpiScale, inheritedTransform);
    if (element.kind == ElementKind::Rect) {
        const auto instance = instances_.rects.find(element.id);
        const LayoutRect frame = instance != instances_.rects.end() ? instance->second.frame.value() : element.frame;
        const Transform transform = instance != instances_.rects.end() ? instance->second.transform.value() : element.transform;
        const float blur = std::max(element.blur, instance != instances_.rects.end() ? instance->second.blur.value() : element.blur);
        if (blur > 0.0f) {
            const Rect captureRect = applyRenderTransformToLogicalRect(
                backdropCaptureRect(frame, blur, transform),
                dpiScale,
                renderTransform);
            if (intersects(captureRect, mergedDirty) && !containsRect(mergedDirty, captureRect)) {
                mergedDirty = unionRect(mergedDirty, captureRect);
                expanded = true;
            }
        }
    }

    const std::vector<const Element*>& children = element.orderedChildren;
    for (const Element* child : children) {
        expandBackdropBlurDirtyRegions(*child, dpiScale, renderTransform, mergedDirty, expanded);
    }
}

inline Rect Runtime::visualDirtyRectForElement(
    const Element& element,
    float dpiScale,
    const RenderTransform& inheritedTransform) const {
    const RenderTransform renderTransform = instances_.renderTransform(element, dpiScale, inheritedTransform);
    Rect local{element.frame.x, element.frame.y, element.frame.width, element.frame.height};
    if (element.kind == ElementKind::Rect) {
        const auto instance = instances_.rects.find(element.id);
        if (instance != instances_.rects.end()) {
            local = visualRect(instance->second.frame.value(),
                               instance->second.shadow.value(),
                               instance->second.blur.value(),
                               instance->second.transform.value());
        } else {
            local = visualRect(element.frame, element.shadow, element.blur, element.transform);
        }
    } else if (element.kind == ElementKind::Polygon) {
        const auto instance = instances_.polygons.find(element.id);
        const LayoutRect frame = instance != instances_.polygons.end() ? instance->second.frame.value() : element.frame;
        const Transform transform = instance != instances_.polygons.end() ? instance->second.transform.value() : element.transform;
        local = transformRect({frame.x, frame.y, frame.width, frame.height}, frame, transform);
    } else if (element.kind == ElementKind::Text) {
        const auto instance = instances_.texts.find(element.id);
        const LayoutRect frame = instance != instances_.texts.end() ? instance->second.frame.value() : element.frame;
        const Transform transform = instance != instances_.texts.end() ? instance->second.transform.value() : element.transform;
        local = transformRect({frame.x, frame.y, frame.width, frame.height}, frame, transform);
    } else if (element.kind == ElementKind::Image || element.kind == ElementKind::Svg) {
        const auto instance = instances_.images.find(element.id);
        const LayoutRect frame = instance != instances_.images.end() ? instance->second.frame.value() : element.frame;
        const Transform transform = instance != instances_.images.end() ? instance->second.transform.value() : element.transform;
        local = imageVisualRect(frame, transform);
    } else if (element.kind == ElementKind::Shadertoy) {
        const auto instance = instances_.shaderToys.find(element.id);
        const LayoutRect frame = instance != instances_.shaderToys.end() ? instance->second.frame.value() : element.frame;
        const Transform transform = instance != instances_.shaderToys.end() ? instance->second.transform.value() : element.transform;
        local = imageVisualRect(frame, transform);
    }
    return applyRenderTransformToLogicalRect(local, dpiScale, renderTransform);
}

inline void Runtime::updateExplicitDirtyKey(
    const Element& element,
    float dpiScale,
    const RenderTransform& inheritedTransform) {
    if (element.dirtyKey.empty()) {
        return;
    }

    runtime::DirtyKeyInstance& instance = instances_.dirtyKey(element.id);
    const Rect current = visualDirtyRectForElement(element, dpiScale, inheritedTransform);
    if (!instance.initialized) {
        instance.key = element.dirtyKey;
        instance.rect = current;
        instance.initialized = true;
        return;
    }

    if (instance.key != element.dirtyKey) {
        addDirtyUnion(instance.rect, current);
        instance.key = element.dirtyKey;
    }
    instance.rect = current;
}

inline runtime::DependentVisualState Runtime::dependentVisualStateForElement(
    const Element& element,
    float dpiScale,
    const RenderTransform& inheritedTransform) const {
    runtime::DependentVisualState state;
    const auto paintBounds = instances_.paintBounds.find(element.id);
    const Rect visualBounds = paintBounds != instances_.paintBounds.end() &&
                              paintBounds->second.hasSubtree
        ? paintBounds->second.subtree
        : visualDirtyRectForElement(element, dpiScale, inheritedTransform);
    state.rect = inflateRect(visualBounds, dependentVisualPadding());

    if (!element.hoverOpacitySourceId.empty()) {
        float hover = 0.0f;
        if (instances_.hoverBlend(element.hoverOpacitySourceId, hover)) {
            hover = std::clamp(hover, 0.0f, 1.0f);
            state.opacity *= lerpValue(element.hoverHiddenOpacity, element.hoverVisibleOpacity, hover);
        } else {
            state.opacity *= element.hoverHiddenOpacity;
        }
    }

    if (!element.visualStateSourceId.empty()) {
        float press = 0.0f;
        LayoutRect sourceFrame;
        if (instances_.pressBlend(element.visualStateSourceId, press, sourceFrame)) {
            (void)sourceFrame;
            state.scale = 1.0f - (1.0f - element.pressedScale) * press;
        }
    }

    state.seen = true;
    return state;
}

inline void Runtime::updateDependentVisualDirtyRegions(float dpiScale) {
    if (!ui_.hasDependentVisuals()) {
        for (const auto& item : instances_.dependentVisualStates) {
            addDirtyRect(item.second.rect);
        }
        instances_.dependentVisualStates.clear();
        return;
    }

    for (auto& item : instances_.dependentVisualStates) {
        item.second.seen = false;
    }

    const RenderTransform identity;
    const std::vector<const Element*>& roots = ui_.orderedRoots();
    for (const Element* root : roots) {
        updateDependentVisualDirtyRegions(*root, dpiScale, identity);
    }

    for (auto item = instances_.dependentVisualStates.begin(); item != instances_.dependentVisualStates.end(); ) {
        if (item->second.seen) {
            ++item;
            continue;
        }
        addDirtyRect(item->second.rect);
        item = instances_.dependentVisualStates.erase(item);
    }
}

inline void Runtime::updateDependentVisualDirtyRegions(
    const Element& element,
    float dpiScale,
    const RenderTransform& inheritedTransform) {
    if (!element.subtreeHasDependentVisuals) {
        return;
    }

    if (!element.hoverOpacitySourceId.empty() || !element.visualStateSourceId.empty()) {
        const runtime::DependentVisualState current = dependentVisualStateForElement(element, dpiScale, inheritedTransform);
        auto item = instances_.dependentVisualStates.find(element.id);
        if (item == instances_.dependentVisualStates.end()) {
            instances_.dependentVisualStates.emplace(element.id, current);
            if (!fullPaintRequested_ && current.opacity > 0.001f) {
                addDirtyRect(current.rect);
            }
        } else {
            runtime::DependentVisualState& previous = item->second;
            previous.seen = true;
            const bool changed =
                !closeEnough(previous.rect, current.rect) ||
                !closeEnough(previous.opacity, current.opacity) ||
                !closeEnough(previous.scale, current.scale);
            if (changed) {
                addDirtyUnion(previous.rect, current.rect);
                previous.rect = current.rect;
                previous.opacity = current.opacity;
                previous.scale = current.scale;
            }
        }
    }

    const RenderTransform renderTransform = instances_.renderTransform(element, dpiScale, inheritedTransform);
    const std::vector<const Element*>& children = element.orderedChildren;
    for (const Element* child : children) {
        updateDependentVisualDirtyRegions(*child, dpiScale, renderTransform);
    }
}

template <typename Fn>
inline void Runtime::forEachElement(Fn&& fn) const {
    const std::vector<const Element*>& roots = ui_.orderedRoots();
    for (const Element* root : roots) {
        forEachElement(*root, fn);
    }
}

template <typename Fn>
inline void Runtime::forEachElement(const Element& element, Fn&& fn) {
    fn(element);
    const std::vector<const Element*>& children = element.orderedChildren;
    for (const Element* child : children) {
        forEachElement(*child, fn);
    }
}

inline std::vector<runtime::ElementSnapshot> Runtime::collectElementStructure() const {
    std::vector<runtime::ElementSnapshot> result;
    forEachElement([&](const Element& element) {
        result.push_back({
            element.id,
            element.kind,
            element.zIndex,
            element.clip,
            element.children.size()
        });
    });
    return result;
}

inline bool Runtime::canReuseStaticSubtree(
    const Element& element,
    const PointerEvent& event,
    float dpiScale,
    const RenderTransform& inheritedTransform,
    bool ancestorFrameChanged,
    bool ancestorDisabled) const {
    if (fullTreeUpdateRequested_ ||
        pruneInstancesRequested_ ||
        ancestorFrameChanged ||
        ancestorDisabled ||
        element.subtreeNeedsUpdate ||
        event.action != PointerAction::Move ||
        !event.buttons.empty() ||
        !focusedId_.empty()) {
        return false;
    }

    const auto cached = instances_.paintBounds.find(element.id);
    if (cached == instances_.paintBounds.end() || !cached->second.hasSubtree) {
        return false;
    }

    const Rect subtree = toPixelRect(cached->second.subtree, dpiScale);
    if (subtree.contains(event.x, event.y)) {
        return false;
    }

    const RenderTransform renderTransform = instances_.renderTransform(element, dpiScale, inheritedTransform);
    return !renderTransform.active && closeEnough(renderTransform.opacity, 1.0f);
}

inline bool Runtime::elementHasActiveAnimation(const Element& element) const {
    if (element.kind == ElementKind::Row ||
        element.kind == ElementKind::Column ||
        element.kind == ElementKind::Stack ||
        element.kind == ElementKind::Flow) {
        const auto item = instances_.layouts.find(element.id);
        return item != instances_.layouts.end() && isLayoutAnimating(item->second);
    }
    if (element.kind == ElementKind::Rect) {
        const auto item = instances_.rects.find(element.id);
        return item != instances_.rects.end() && isRectAnimating(element, item->second);
    }
    if (element.kind == ElementKind::Polygon) {
        const auto item = instances_.polygons.find(element.id);
        return item != instances_.polygons.end() && isPolygonAnimating(element, item->second);
    }
    if (element.kind == ElementKind::Text) {
        const auto item = instances_.texts.find(element.id);
        return item != instances_.texts.end() && isTextAnimating(item->second);
    }
    if (element.kind == ElementKind::Image || element.kind == ElementKind::Svg) {
        const auto item = instances_.images.find(element.id);
        return item != instances_.images.end() && isImageAnimating(item->second);
    }
    if (element.kind == ElementKind::Shadertoy) {
        const auto item = instances_.shaderToys.find(element.id);
        return item != instances_.shaderToys.end() &&
               (item->second.primitive->isAnimating() ||
                item->second.frame.isActive() ||
                item->second.radius.isActive() ||
                item->second.opacity.isActive() ||
                item->second.transform.isActive());
    }
    return false;
}

inline Transform Runtime::pointerRuntimeTransform(
    const Element& element,
    const PointerEvent& event,
    float dpiScale,
    const std::string& hoverTargetId) const {
    return pointerRuntimeTransformForElement(element,
                                            ui_.find(element.pointerRuntimeSourceId),
                                            event.x,
                                            event.y,
                                            dpiScale,
                                            hoverTargetId);
}

inline bool Runtime::updateFrameTarget(const Element& element) {
    runtime::FrameTargetInstance& instance = instances_.frameTargets.try_emplace(element.id).first->second;
    instance.seen = true;
    if (!instance.initialized) {
        instance.frame = element.frame;
        instance.initialized = true;
        return false;
    }

    const bool changed = !closeEnough(instance.frame, element.frame);
    instance.frame = element.frame;
    return changed;
}

inline void Runtime::updateTimer(const Element& element, float deltaSeconds) {
    if (!element.onTimer || element.timerSeconds <= 0.0f) {
        return;
    }

    runtime::TimerInstance& instance = instances_.timer(element.id);
    instance.seen = true;
    if (!instance.active || !closeEnough(instance.seconds, element.timerSeconds)) {
        instance.seconds = element.timerSeconds;
        instance.elapsed = 0.0f;
        instance.active = true;
    }

    if (!instance.active) {
        return;
    }

    instance.elapsed += std::max(0.0f, deltaSeconds);
    if (instance.elapsed >= instance.seconds) {
        instance.active = false;
        element.onTimer();
        composeRequested_ = true;
        paintRequested_ = true;
    } else {
        animating_ = true;
    }
}

inline void Runtime::updateFrameCallback(const Element& element, float deltaSeconds) {
    if (!element.onFrame) {
        return;
    }
    element.onFrame(std::max(0.0f, deltaSeconds));
    composeRequested_ = true;
    paintRequested_ = true;
    animating_ = true;
}

inline void Runtime::syncScrollStateElement(const Element& element) {
    if (element.scrollStateId.empty()) {
        return;
    }

    runtime::ScrollStateInstance& instance = instances_.scrollState(element.scrollStateId);
    if (!ownsScrollState(element)) {
        return;
    }

    syncOwnedScrollState(element, instance);
}

inline void Runtime::syncSliderStateElement(const Element& element) {
    if (element.sliderStateId.empty()) {
        return;
    }

    runtime::SliderStateInstance& instance = instances_.sliderState(element.sliderStateId);
    if (!ownsSliderState(element)) {
        return;
    }

    syncOwnedSliderState(element, instance);
}

inline void Runtime::syncScrollStateBindings() {
    forEachElement([&](const Element& element) {
        syncScrollStateElement(element);
        syncSliderStateElement(element);
    });
}

inline float Runtime::scrollStepFor(const Element& element) const {
    const auto state = instances_.scrollStates.find(element.scrollStateId);
    if (state != instances_.scrollStates.end()) {
        return state->second.step;
    }
    return std::max(1.0f, element.scrollStep);
}

inline void Runtime::addScrollDirtyRect(const runtime::ScrollStateInstance& instance) {
    if (instance.hasDirtyRect) {
        addDirtyRect(instance.dirtyRect);
    }
}

inline void Runtime::addSliderDirtyRect(const runtime::SliderStateInstance& instance) {
    if (instance.hasDirtyRect) {
        addDirtyRect(instance.dirtyRect);
    }
}

inline void Runtime::setScrollOffset(const std::string& stateId, float offset) {
    auto state = instances_.scrollStates.find(stateId);
    if (state == instances_.scrollStates.end()) {
        return;
    }

    runtime::ScrollStateInstance& instance = state->second;
    const float next = std::clamp(offset, 0.0f, instance.maxOffset);
    if (closeEnough(next, instance.offset)) {
        return;
    }

    instance.offset = next;
    addScrollDirtyRect(instance);
    if (const Element* owner = ui_.find(stateId)) {
        if (owner->onScrollOffsetChanged && !owner->disabled) {
            owner->onScrollOffsetChanged(instance.offset);
        }
        if (owner->composeOnScrollOffsetChange && !owner->disabled) {
            composeRequested_ = true;
        }
    }
}

inline void Runtime::applyRuntimeScroll(const Element& element, float delta) {
    if (element.scrollStateId.empty()) {
        return;
    }
    auto state = instances_.scrollStates.find(element.scrollStateId);
    if (state == instances_.scrollStates.end()) {
        return;
    }
    runtime::ScrollStateInstance& instance = state->second;
    if (instance.maxOffset <= 0.0f ||
        (instance.offset <= 0.0f && delta < 0.0f) ||
        (instance.offset >= instance.maxOffset && delta > 0.0f)) {
        instance.velocity = 0.0f;
        return;
    }

    instance.velocity = addScrollImpulse(instance.velocity, delta);
    if (scrollMotionActive(instance.velocity)) {
        animating_ = true;
        paintRequested_ = true;
    }
}

inline void Runtime::updateScrollMotion(float deltaSeconds) {
    for (auto& entry : instances_.scrollStates) {
        runtime::ScrollStateInstance& instance = entry.second;
        const ScrollMotionStep motion = advanceScrollMotion(
            instance.offset,
            instance.maxOffset,
            instance.velocity,
            deltaSeconds);
        instance.velocity = motion.velocity;
        if (!closeEnough(motion.offset, instance.offset)) {
            setScrollOffset(entry.first, motion.offset);
        }
        animating_ = animating_ || motion.active;
    }
}

inline void Runtime::beginRuntimeScrollDrag(const Element& element) {
    if (element.scrollDragSourceId.empty()) {
        return;
    }
    auto state = instances_.scrollStates.find(element.scrollDragSourceId);
    if (state == instances_.scrollStates.end()) {
        return;
    }
    state->second.velocity = 0.0f;
    state->second.dragStartOffset = state->second.offset;
}

inline void Runtime::updateRuntimeScrollDrag(const Element& element, double dragDeltaY, float dpiScale) {
    if (element.scrollDragSourceId.empty() || element.scrollDragTravel <= 0.0f) {
        return;
    }
    const auto state = instances_.scrollStates.find(element.scrollDragSourceId);
    if (state == instances_.scrollStates.end() || state->second.maxOffset <= 0.0f) {
        return;
    }
    state->second.velocity = 0.0f;
    const float logicalDeltaY = static_cast<float>(dragDeltaY) / std::max(0.001f, dpiScale);
    const float next = state->second.dragStartOffset +
                       logicalDeltaY * (state->second.maxOffset / element.scrollDragTravel);
    setScrollOffset(element.scrollDragSourceId, next);
}

inline float Runtime::sliderValueFromPointer(const Element& element, double pointerX, float dpiScale) const {
    if (element.sliderInputSourceId.empty()) {
        return 0.0f;
    }
    const auto state = instances_.sliderStates.find(element.sliderInputSourceId);
    if (state == instances_.sliderStates.end()) {
        return 0.0f;
    }

    const Element* owner = ui_.find(element.sliderInputSourceId);
    if (owner == nullptr) {
        return 0.0f;
    }

    return core::dsl::sliderValueFromPointer(*owner, pointerX, dpiScale);
}

inline void Runtime::setSliderValue(const std::string& stateId, float value, bool dragging) {
    auto state = instances_.sliderStates.find(stateId);
    if (state == instances_.sliderStates.end()) {
        return;
    }

    runtime::SliderStateInstance& instance = state->second;
    const float next = std::clamp(value, 0.0f, 1.0f);
    const bool changed = !closeEnough(next, instance.value);
    instance.dragging = dragging;
    if (!changed) {
        return;
    }

    instance.value = next;
    addSliderDirtyRect(instance);
    paintRequested_ = true;
    if (const Element* owner = ui_.find(stateId)) {
        if (owner->onSliderValueChanged && !owner->disabled) {
            owner->onSliderValueChanged(instance.value);
        }
    }
}

inline void Runtime::updateRuntimeSlider(const Element& element, double pointerX, float dpiScale, bool dragging) {
    if (element.sliderInputSourceId.empty()) {
        return;
    }
    setSliderValue(element.sliderInputSourceId, sliderValueFromPointer(element, pointerX, dpiScale), dragging);
}

inline void Runtime::updateElementTree(
    const PointerEvent& event,
    float deltaSeconds,
    float dpiScale,
    const std::string& hoverTargetId) {
    runtime::markEntriesUnseen(instances_.paintBounds);
    focusedElementRenderTransformValid_ = false;
    const RenderTransform identity;
    const std::vector<const Element*>& roots = ui_.orderedRoots();
    for (const Element* root : roots) {
        updateElementTree(*root, event, deltaSeconds, dpiScale, hoverTargetId, identity, false, false);
    }
}

inline runtime::PaintBoundsInstance Runtime::updateElementTree(
    const Element& element,
    const PointerEvent& event,
    float deltaSeconds,
    float dpiScale,
    const std::string& hoverTargetId,
    const RenderTransform& inheritedTransform,
    bool ancestorFrameChanged,
    bool ancestorDisabled) {
    const bool disabledTree = ancestorDisabled || element.disabled;
    if (canReuseStaticSubtree(element, event, dpiScale, inheritedTransform, ancestorFrameChanged, disabledTree)) {
        runtime::PaintBoundsInstance cached = instances_.paintBounds[element.id];
        cached.seen = true;
        instances_.paintBounds[element.id] = cached;
        return cached;
    }

    const bool frameTargetChanged = updateFrameTarget(element);
    updateExplicitDirtyKey(element, dpiScale, inheritedTransform);
    if (disabledTree) {
        instances_.interaction(element.id).state.update({}, event, false, {}, 0.0, false);
    } else {
        updateInteraction(element, event, dpiScale, hoverTargetId, inheritedTransform);
    }
    updateTimer(element, deltaSeconds);
    updateFrameCallback(element, deltaSeconds);

    if (element.kind == ElementKind::Row ||
        element.kind == ElementKind::Column ||
        element.kind == ElementKind::Stack ||
        element.kind == ElementKind::Flow) {
        updateLayoutElement(element, deltaSeconds, dpiScale, inheritedTransform, event, hoverTargetId);
    } else if (element.kind == ElementKind::Rect) {
        updateRect(element, deltaSeconds, dpiScale, inheritedTransform, ancestorFrameChanged);
    } else if (element.kind == ElementKind::Polygon) {
        updatePolygon(element, deltaSeconds, dpiScale, inheritedTransform, ancestorFrameChanged);
    } else if (element.kind == ElementKind::Text) {
        updateText(element, deltaSeconds, dpiScale, inheritedTransform, ancestorFrameChanged);
    } else if (element.kind == ElementKind::Image || element.kind == ElementKind::Svg) {
        updateImage(element, deltaSeconds, dpiScale, inheritedTransform, ancestorFrameChanged);
    } else if (element.kind == ElementKind::Shadertoy) {
        updateShaderToy(element, event, deltaSeconds, dpiScale, inheritedTransform, ancestorFrameChanged);
    }

    const bool childAncestorFrameChanged = ancestorFrameChanged || frameTargetChanged;
    const RenderTransform renderTransform = instances_.renderTransform(element, dpiScale, inheritedTransform);
    if (element.id == focusedId_) {
        focusedElementRenderTransform_ = renderTransform;
        focusedElementRenderTransformValid_ = true;
    }
    runtime::PaintBoundsInstance bounds;
    bounds.seen = true;
    bounds.subtreeAnimating = elementHasActiveAnimation(element);
    if (renderTransform.opacity > 0.001f &&
        element.kind != ElementKind::Row &&
        element.kind != ElementKind::Column &&
        element.kind != ElementKind::Stack &&
        element.kind != ElementKind::Flow) {
        bounds.own = visualDirtyRectForElement(element, dpiScale, inheritedTransform);
        bounds.subtree = bounds.own;
        bounds.hasOwn = bounds.own.width > 0.0f && bounds.own.height > 0.0f;
        bounds.hasSubtree = bounds.hasOwn;
        bounds.drawCost = bounds.hasOwn ? 1 : 0;
    }

    const std::vector<const Element*>& children = element.orderedChildren;
    for (const Element* child : children) {
        const runtime::PaintBoundsInstance childBounds =
            updateElementTree(*child, event, deltaSeconds, dpiScale, hoverTargetId, renderTransform, childAncestorFrameChanged, disabledTree);
        bounds.subtreeAnimating = bounds.subtreeAnimating || childBounds.subtreeAnimating;
        if (!childBounds.hasSubtree) {
            continue;
        }
        bounds.subtree = bounds.hasSubtree ? unionRect(bounds.subtree, childBounds.subtree) : childBounds.subtree;
        bounds.hasSubtree = true;
        bounds.drawCost += childBounds.drawCost;
    }

    if (bounds.hasSubtree && element.clip) {
        Rect clipped{};
        const Rect clipFrame = applyRenderTransformToLogicalRect(
            {element.frame.x, element.frame.y, element.frame.width, element.frame.height},
            dpiScale,
            renderTransform);
        bounds.hasSubtree = intersectRect(bounds.subtree, clipFrame, clipped);
        bounds.subtree = clipped;
        if (bounds.hasOwn) {
            Rect clippedOwn{};
            bounds.hasOwn = intersectRect(bounds.own, clipFrame, clippedOwn);
            bounds.own = clippedOwn;
        }
    }

    instances_.paintBounds[element.id] = bounds;
    return bounds;
}

inline void Runtime::updateLayoutElement(
    const Element& element,
    float deltaSeconds,
    float dpiScale,
    const RenderTransform& inheritedTransform,
    const PointerEvent& event,
    const std::string& hoverTargetId) {
    runtime::LayoutInstance& instance = instances_.layout(element.id);
    const Rect beforeRect = applyRenderTransformToLogicalRect(inflateRect(
        transformRect({element.frame.x, element.frame.y, element.frame.width, element.frame.height},
                      element.frame,
                      instance.transform.value()),
        64.0f), dpiScale, inheritedTransform);

    bool changed = false;
    Transform targetTransform = pointerRuntimeTransform(element, event, dpiScale, hoverTargetId);
    targetTransform = core::dsl::runtimeTransformForElement(element, instances_.scrollStates, instances_.sliderStates, targetTransform);
    changed = instance.transform.setTarget(targetTransform, element.transition, shouldAnimate(element, AnimProperty::Transform)) || changed;
    changed = instance.opacity.setTarget(element.opacity, element.transition, shouldAnimate(element, AnimProperty::Opacity)) || changed;

    changed = instance.transform.tick(deltaSeconds) || changed;
    changed = instance.opacity.tick(deltaSeconds) || changed;

    if (changed) {
        const Rect afterRect = applyRenderTransformToLogicalRect(inflateRect(
            transformRect({element.frame.x, element.frame.y, element.frame.width, element.frame.height},
                          element.frame,
                          instance.transform.value()),
            64.0f), dpiScale, inheritedTransform);
        addDirtyUnion(beforeRect, afterRect);
    }
    animating_ = animating_ || isLayoutAnimating(instance);
}

inline void Runtime::updateRect(
    const Element& element,
    float deltaSeconds,
    float dpiScale,
    const RenderTransform& inheritedTransform,
    bool snapFrame) {
    runtime::RectInstance& instance = instances_.rect(element.id);
    instance.interaction = instances_.interaction(element.id).state;
    const Rect beforeRect = applyRenderTransformToLogicalRect(
        visualRect(instance.frame.value(), instance.shadow.value(), instance.blur.value(), instance.transform.value()),
        dpiScale,
        inheritedTransform);

    const bool interactive = element.interactive && !element.disabled;
    const bool stateColorsVisible = element.hasStateColors &&
        (!closeEnough(element.color, element.hoverColor) || !closeEnough(element.color, element.pressedColor));
    const float hoverSpeed = element.smoothStateColors ? 12.0f : 0.0f;
    const float pressSpeed = element.smoothStateColors ? 20.0f : 0.0f;
    const bool hoverChanged = instance.hoverBlend.update(interactive && stateColorsVisible && instance.interaction.hover ? 1.0f : 0.0f,
                                                         hoverSpeed,
                                                         deltaSeconds);
    const bool pressChanged = instance.pressBlend.update(interactive && stateColorsVisible && instance.interaction.pressed ? 1.0f : 0.0f,
                                                         pressSpeed,
                                                         deltaSeconds);
    const float hover = instance.hoverBlend.value();
    const float press = instance.pressBlend.value();
    const Color hoverColor = stateColorsVisible ? mixColor(element.color, element.hoverColor, hover) : element.color;
    const Color currentColor = stateColorsVisible ? mixColor(hoverColor, element.pressedColor, press) : element.color;
    const bool gradientChanged = !sameGradient(instance.gradient, element.gradient);
    if (gradientChanged) {
        instance.gradient = element.gradient;
    }

    LayoutRect targetFrame = element.frame;
    if (!element.sliderFillSourceId.empty()) {
        const auto state = instances_.sliderStates.find(element.sliderFillSourceId);
        if (state != instances_.sliderStates.end()) {
            targetFrame.width = std::max(0.0f, state->second.width * state->second.value);
        }
    }

    bool changed = hoverChanged || pressChanged || gradientChanged;
    changed = instance.frame.setTarget(targetFrame, element.transition, !snapFrame && shouldAnimateFrame(element)) || changed;
    changed = instance.color.setTarget(currentColor, element.transition, shouldAnimate(element, AnimProperty::Color)) || changed;
    changed = instance.radius.setTarget(element.radius, element.transition, shouldAnimate(element, AnimProperty::Radius)) || changed;
    changed = instance.blur.setTarget(element.blur, element.transition, shouldAnimate(element, AnimProperty::Blur)) || changed;
    changed = instance.opacity.setTarget(element.opacity, element.transition, shouldAnimate(element, AnimProperty::Opacity)) || changed;
    changed = instance.border.setTarget(element.border, element.transition, shouldAnimate(element, AnimProperty::Border)) || changed;
    changed = instance.shadow.setTarget(element.shadow, element.transition, shouldAnimate(element, AnimProperty::Shadow)) || changed;
    changed = instance.transform.setTarget(core::dsl::runtimeTransformForElement(element, instances_.scrollStates, instances_.sliderStates, element.transform), element.transition, shouldAnimate(element, AnimProperty::Transform)) || changed;

    changed = instance.frame.tick(deltaSeconds) || changed;
    changed = instance.color.tick(deltaSeconds) || changed;
    changed = instance.radius.tick(deltaSeconds) || changed;
    changed = instance.blur.tick(deltaSeconds) || changed;
    changed = instance.opacity.tick(deltaSeconds) || changed;
    changed = instance.border.tick(deltaSeconds) || changed;
    changed = instance.shadow.tick(deltaSeconds) || changed;
    changed = instance.transform.tick(deltaSeconds) || changed;

    if (changed) {
        const Rect afterRect = applyRenderTransformToLogicalRect(
            visualRect(instance.frame.value(), instance.shadow.value(), instance.blur.value(), instance.transform.value()),
            dpiScale,
            inheritedTransform);
        addDirtyUnion(beforeRect, afterRect);
    }
    animating_ = animating_ || isRectAnimating(element, instance);
}

inline void Runtime::updatePolygon(
    const Element& element,
    float deltaSeconds,
    float dpiScale,
    const RenderTransform& inheritedTransform,
    bool snapFrame) {
    runtime::PolygonInstance& instance = instances_.polygon(element.id);
    instance.interaction = instances_.interaction(element.id).state;
    const Rect beforeRect = applyRenderTransformToLogicalRect(
        transformRect({instance.frame.value().x, instance.frame.value().y, instance.frame.value().width, instance.frame.value().height},
                      instance.frame.value(),
                      instance.transform.value()),
        dpiScale,
        inheritedTransform);

    const bool interactive = element.interactive && !element.disabled;
    const bool stateColorsVisible = element.hasStateColors &&
        (!closeEnough(element.color, element.hoverColor) || !closeEnough(element.color, element.pressedColor));
    const float hoverSpeed = element.smoothStateColors ? 12.0f : 0.0f;
    const float pressSpeed = element.smoothStateColors ? 20.0f : 0.0f;
    const bool hoverChanged = instance.hoverBlend.update(interactive && stateColorsVisible && instance.interaction.hover ? 1.0f : 0.0f,
                                                         hoverSpeed,
                                                         deltaSeconds);
    const bool pressChanged = instance.pressBlend.update(interactive && stateColorsVisible && instance.interaction.pressed ? 1.0f : 0.0f,
                                                         pressSpeed,
                                                         deltaSeconds);
    const float hover = instance.hoverBlend.value();
    const float press = instance.pressBlend.value();
    const Color hoverColor = stateColorsVisible ? mixColor(element.color, element.hoverColor, hover) : element.color;
    const Color currentColor = stateColorsVisible ? mixColor(hoverColor, element.pressedColor, press) : element.color;
    const bool pointsChanged = !samePoints(instance.points, element.polygonPoints);
    if (pointsChanged) {
        instance.points = element.polygonPoints;
    }

    bool changed = hoverChanged || pressChanged || pointsChanged;
    changed = instance.frame.setTarget(element.frame, element.transition, !snapFrame && shouldAnimateFrame(element)) || changed;
    changed = instance.color.setTarget(currentColor, element.transition, shouldAnimate(element, AnimProperty::Color)) || changed;
    changed = instance.radius.setTarget(element.radius, element.transition, shouldAnimate(element, AnimProperty::Radius)) || changed;
    changed = instance.opacity.setTarget(element.opacity, element.transition, shouldAnimate(element, AnimProperty::Opacity)) || changed;
    changed = instance.transform.setTarget(core::dsl::runtimeTransformForElement(element, instances_.scrollStates, instances_.sliderStates, element.transform), element.transition, shouldAnimate(element, AnimProperty::Transform)) || changed;

    changed = instance.frame.tick(deltaSeconds) || changed;
    changed = instance.color.tick(deltaSeconds) || changed;
    changed = instance.radius.tick(deltaSeconds) || changed;
    changed = instance.opacity.tick(deltaSeconds) || changed;
    changed = instance.transform.tick(deltaSeconds) || changed;

    if (changed) {
        const Rect afterRect = applyRenderTransformToLogicalRect(
            transformRect({instance.frame.value().x, instance.frame.value().y, instance.frame.value().width, instance.frame.value().height},
                          instance.frame.value(),
                          instance.transform.value()),
            dpiScale,
            inheritedTransform);
        addDirtyUnion(beforeRect, afterRect);
    }
    animating_ = animating_ || isPolygonAnimating(element, instance);
}

inline void Runtime::updateText(
    const Element& element,
    float deltaSeconds,
    float dpiScale,
    const RenderTransform& inheritedTransform,
    bool snapFrame) {
    runtime::TextInstance& instance = instances_.text(element.id);
    const Rect beforeRect = applyRenderTransformToLogicalRect(
        transformRect({instance.frame.value().x,
                       instance.frame.value().y,
                       instance.frame.value().width,
                       instance.frame.value().height},
                      instance.frame.value(),
                      instance.transform.value()),
        dpiScale,
        inheritedTransform);

    const bool keyedContent = !element.dirtyKey.empty();
    const bool textChanged = keyedContent
        ? instance.contentDirtyKey != element.dirtyKey
        : instance.text != element.text;
    const bool contentChanged =
        textChanged ||
        instance.fontFamily != element.fontFamily ||
        instance.fontSize != element.fontSize ||
        instance.fontWeight != element.fontWeight ||
        instance.maxWidth != element.maxWidth ||
        instance.wrap != element.wrap ||
        instance.horizontalAlign != element.horizontalAlign ||
        instance.verticalAlign != element.verticalAlign ||
        instance.lineHeight != element.lineHeight;
    if (contentChanged) {
        instance.text = element.text;
        instance.contentDirtyKey = element.dirtyKey;
        instance.fontFamily = element.fontFamily;
        instance.fontSize = element.fontSize;
        instance.fontWeight = element.fontWeight;
        instance.maxWidth = element.maxWidth;
        instance.wrap = element.wrap;
        instance.horizontalAlign = element.horizontalAlign;
        instance.verticalAlign = element.verticalAlign;
        instance.lineHeight = element.lineHeight;
    }

    bool changed = false;
    changed = instance.frame.setTarget(element.frame, element.transition, !snapFrame && shouldAnimateFrame(element)) || changed;
    changed = instance.color.setTarget(element.textColor, element.transition, shouldAnimate(element, AnimProperty::TextColor)) || changed;
    changed = instance.opacity.setTarget(element.opacity, element.transition, shouldAnimate(element, AnimProperty::Opacity)) || changed;
    changed = instance.transform.setTarget(core::dsl::runtimeTransformForElement(element, instances_.scrollStates, instances_.sliderStates, element.transform), element.transition, shouldAnimate(element, AnimProperty::Transform)) || changed;

    changed = instance.frame.tick(deltaSeconds) || changed;
    changed = instance.color.tick(deltaSeconds) || changed;
    changed = instance.opacity.tick(deltaSeconds) || changed;
    changed = instance.transform.tick(deltaSeconds) || changed;

    if (changed || contentChanged) {
        const Rect afterRect = applyRenderTransformToLogicalRect(
            transformRect({instance.frame.value().x,
                           instance.frame.value().y,
                           instance.frame.value().width,
                           instance.frame.value().height},
                          instance.frame.value(),
                          instance.transform.value()),
            dpiScale,
            inheritedTransform);
        addDirtyUnion(beforeRect, afterRect);
    }
    animating_ = animating_ || isTextAnimating(instance);
}

inline void Runtime::updateImage(
    const Element& element,
    float deltaSeconds,
    float dpiScale,
    const RenderTransform& inheritedTransform,
    bool snapFrame) {
    runtime::ImageInstance& instance = instances_.image(element.id);
    const Rect beforeRect = applyRenderTransformToLogicalRect(
        imageVisualRect(instance.frame.value(), instance.transform.value()),
        dpiScale,
        inheritedTransform);

    bool changed = false;
    changed = instance.frame.setTarget(element.frame, element.transition, !snapFrame && shouldAnimateFrame(element)) || changed;
    changed = instance.tint.setTarget(element.color, element.transition, shouldAnimate(element, AnimProperty::Color)) || changed;
    changed = instance.radius.setTarget(element.radius, element.transition, shouldAnimate(element, AnimProperty::Radius)) || changed;
    changed = instance.blur.setTarget(element.blur, element.transition, shouldAnimate(element, AnimProperty::Blur)) || changed;
    changed = instance.opacity.setTarget(element.opacity, element.transition, shouldAnimate(element, AnimProperty::Opacity)) || changed;
    changed = instance.transform.setTarget(core::dsl::runtimeTransformForElement(element, instances_.scrollStates, instances_.sliderStates, element.transform), element.transition, shouldAnimate(element, AnimProperty::Transform)) || changed;

    changed = instance.frame.tick(deltaSeconds) || changed;
    changed = instance.tint.tick(deltaSeconds) || changed;
    changed = instance.radius.tick(deltaSeconds) || changed;
    changed = instance.blur.tick(deltaSeconds) || changed;
    changed = instance.opacity.tick(deltaSeconds) || changed;
    changed = instance.transform.tick(deltaSeconds) || changed;

    if (instance.hasCoverViewport != element.imageHasCoverViewport ||
        !closeEnough(instance.coverViewportSize, element.imageCoverViewportSize) ||
        !closeEnough(instance.coverViewportOffset, element.imageCoverViewportOffset)) {
        instance.hasCoverViewport = element.imageHasCoverViewport;
        instance.coverViewportSize = element.imageCoverViewportSize;
        instance.coverViewportOffset = element.imageCoverViewportOffset;
        changed = true;
    }

    const bool sourceChanged = instance.source != element.imageSource ||
                               instance.svgSource != element.svgSource ||
                               instance.flipVertically != element.imageFlipVertically ||
                               instance.fit != element.imageFit;
    if (sourceChanged) {
        instance.source = element.imageSource;
        instance.svgSource = element.svgSource;
        instance.flipVertically = element.imageFlipVertically;
        instance.fit = element.imageFit;
        if (element.kind == ElementKind::Svg) {
            instance.primitive->setSvgSource(element.id, instance.svgSource);
        } else {
            instance.primitive->setSource(instance.source);
        }
        instance.primitive->setFlipVertically(instance.flipVertically);
        instance.primitive->setFit(instance.fit);
        changed = true;
    }

    const LayoutRect frame = instance.frame.value();
    instance.primitive->setBounds(frame.x, frame.y, frame.width, frame.height);
    if (instance.primitive->updateTexture()) {
        changed = true;
    }

    if (changed) {
        const Rect afterRect = applyRenderTransformToLogicalRect(
            imageVisualRect(instance.frame.value(), instance.transform.value()),
            dpiScale,
            inheritedTransform);
        addDirtyUnion(beforeRect, afterRect);
    }
    animating_ = animating_ || isImageAnimating(instance);
}

inline void Runtime::updateShaderToy(
    const Element& element,
    const PointerEvent& event,
    float deltaSeconds,
    float dpiScale,
    const RenderTransform& inheritedTransform,
    bool snapFrame) {
    runtime::ShaderToyInstance& instance = instances_.shaderToy(element.id);
    const Rect beforeRect = applyRenderTransformToLogicalRect(
        imageVisualRect(instance.frame.value(), instance.transform.value()),
        dpiScale, inheritedTransform);

    bool changed = false;
    changed = instance.frame.setTarget(element.frame, element.transition,
                                       !snapFrame && shouldAnimateFrame(element)) || changed;
    changed = instance.radius.setTarget(element.radius, element.transition,
                                        shouldAnimate(element, AnimProperty::Radius)) || changed;
    changed = instance.opacity.setTarget(element.opacity, element.transition,
                                         shouldAnimate(element, AnimProperty::Opacity)) || changed;
    changed = instance.transform.setTarget(
        core::dsl::runtimeTransformForElement(element, instances_.scrollStates, instances_.sliderStates, element.transform),
        element.transition, shouldAnimate(element, AnimProperty::Transform)) || changed;
    changed = instance.frame.tick(deltaSeconds) || changed;
    changed = instance.radius.tick(deltaSeconds) || changed;
    changed = instance.opacity.tick(deltaSeconds) || changed;
    changed = instance.transform.tick(deltaSeconds) || changed;

    const std::uint64_t graphHash = core::render::shaderToyGraphHash(element.shaderToyGraph);
    if (instance.graphHash != graphHash) {
        instance.graphHash = graphHash;
        instance.primitive->setGraph(element.shaderToyGraph);
        changed = true;
    }
    if (instance.resetKey != element.shaderToyResetKey) {
        instance.resetKey = element.shaderToyResetKey;
        instance.primitive->requestReset();
        changed = true;
    }

    const LayoutRect frame = instance.frame.value();
    const Rect pixelFrame = toPixelRect(frame, dpiScale);
    const Transform localTransform = scaleTransform(instance.transform.value(), dpiScale);
    const RenderTransform elementTransform = instances_.renderTransform(element, dpiScale, inheritedTransform);
    const TransformMatrix matrix = combinedPrimitiveMatrix(elementTransform, pixelFrame, localTransform);
    TransformMatrix inverse;
    Vec2 pointerPixels{static_cast<float>(event.x), static_cast<float>(event.y)};
    const bool invertible = inverseMatrix(matrix, inverse);
    if (invertible) {
        pointerPixels = core::transformPoint(inverse, pointerPixels.x, pointerPixels.y);
    }
    const Vec2 localPointer{pointerPixels.x - pixelFrame.x, pointerPixels.y - pixelFrame.y};
    const bool pointerInside = invertible &&
        localPointer.x >= 0.0f && localPointer.y >= 0.0f &&
        localPointer.x <= pixelFrame.width && localPointer.y <= pixelFrame.height;

    instance.primitive->setElementId(element.id);
    instance.primitive->setBounds(pixelFrame.x, pixelFrame.y, pixelFrame.width, pixelFrame.height);
    instance.primitive->setCornerRadius(toPixels(instance.radius.value(), dpiScale));
    instance.primitive->setOpacity(instance.opacity.value() * elementTransform.opacity);
    instance.primitive->setTransformMatrix(matrix);
    instance.primitive->setResolutionScale(element.shaderToyResolutionScale);
    instance.primitive->setTimeScale(element.shaderToyTimeScale);
    instance.primitive->setPaused(element.shaderToyPaused);
    instance.primitive->update(deltaSeconds, localPointer,
                               event.isDown(PointerButton::Left),
                               event.isPress(PointerButton::Left),
                               event.isRelease(PointerButton::Left),
                               updateFrameToken_, pointerInside);

    const bool active = instance.primitive->isAnimating() || instance.frame.isActive() ||
                        instance.radius.isActive() || instance.opacity.isActive() ||
                        instance.transform.isActive();
    if (changed || active) {
        const Rect afterRect = applyRenderTransformToLogicalRect(
            imageVisualRect(instance.frame.value(), instance.transform.value()),
            dpiScale, inheritedTransform);
        addDirtyUnion(beforeRect, afterRect);
    }
    animating_ = animating_ || active;
}

} // namespace core::dsl
