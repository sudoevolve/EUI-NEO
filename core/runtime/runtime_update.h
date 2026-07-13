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
        const std::vector<const Element*>& roots = orderedElements(ui_);
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

    const RenderTransform renderTransform = resolveRenderTransform(element, dpiScale, inheritedTransform);
    if (element.kind == ElementKind::Rect) {
        const auto instance = rects_.find(element.id);
        const LayoutRect frame = instance != rects_.end() ? instance->second.frame.value() : element.frame;
        const Transform transform = instance != rects_.end() ? instance->second.transform.value() : element.transform;
        const float blur = std::max(element.blur, instance != rects_.end() ? instance->second.blur.value() : element.blur);
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

    const std::vector<const Element*>& children = orderedElements(element);
    for (const Element* child : children) {
        expandBackdropBlurDirtyRegions(*child, dpiScale, renderTransform, mergedDirty, expanded);
    }
}

inline Rect Runtime::visualDirtyRectForElement(
    const Element& element,
    float dpiScale,
    const RenderTransform& inheritedTransform) const {
    const RenderTransform renderTransform = resolveRenderTransform(element, dpiScale, inheritedTransform);
    Rect local{element.frame.x, element.frame.y, element.frame.width, element.frame.height};
    if (element.kind == ElementKind::Rect) {
        const auto instance = rects_.find(element.id);
        if (instance != rects_.end()) {
            local = visualRect(instance->second.frame.value(),
                               instance->second.shadow.value(),
                               instance->second.blur.value(),
                               instance->second.transform.value());
        } else {
            local = visualRect(element.frame, element.shadow, element.blur, element.transform);
        }
    } else if (element.kind == ElementKind::Polygon) {
        const auto instance = polygons_.find(element.id);
        const LayoutRect frame = instance != polygons_.end() ? instance->second.frame.value() : element.frame;
        const Transform transform = instance != polygons_.end() ? instance->second.transform.value() : element.transform;
        local = transformRect({frame.x, frame.y, frame.width, frame.height}, frame, transform);
    } else if (element.kind == ElementKind::Text) {
        const auto instance = texts_.find(element.id);
        const LayoutRect frame = instance != texts_.end() ? instance->second.frame.value() : element.frame;
        const Transform transform = instance != texts_.end() ? instance->second.transform.value() : element.transform;
        local = transformRect({frame.x, frame.y, frame.width, frame.height}, frame, transform);
    } else if (element.kind == ElementKind::Image || element.kind == ElementKind::Svg) {
        const auto instance = images_.find(element.id);
        const LayoutRect frame = instance != images_.end() ? instance->second.frame.value() : element.frame;
        const Transform transform = instance != images_.end() ? instance->second.transform.value() : element.transform;
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

    runtime::DirtyKeyInstance& instance = dirtyKeyInstance(element.id);
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
    state.rect = inflateRect(visualDirtyRectForElement(element, dpiScale, inheritedTransform),
                             dependentVisualPadding());

    if (!element.hoverOpacitySourceId.empty()) {
        float hover = 0.0f;
        if (hoverBlendForSource(element.hoverOpacitySourceId, hover)) {
            hover = std::clamp(hover, 0.0f, 1.0f);
            state.opacity *= lerpValue(element.hoverHiddenOpacity, element.hoverVisibleOpacity, hover);
        } else {
            state.opacity *= element.hoverHiddenOpacity;
        }
    }

    if (!element.visualStateSourceId.empty()) {
        float press = 0.0f;
        LayoutRect sourceFrame;
        if (pressBlendForSource(element.visualStateSourceId, press, sourceFrame)) {
            (void)sourceFrame;
            state.scale = 1.0f - (1.0f - element.pressedScale) * press;
        }
    }

    state.seen = true;
    return state;
}

inline void Runtime::updateDependentVisualDirtyRegions(float dpiScale) {
    if (!ui_.hasDependentVisuals()) {
        for (const auto& item : dependentVisualStates_) {
            addDirtyRect(item.second.rect);
        }
        dependentVisualStates_.clear();
        return;
    }

    for (auto& item : dependentVisualStates_) {
        item.second.seen = false;
    }

    const RenderTransform identity;
    const std::vector<const Element*>& roots = orderedElements(ui_);
    for (const Element* root : roots) {
        updateDependentVisualDirtyRegions(*root, dpiScale, identity);
    }

    for (auto item = dependentVisualStates_.begin(); item != dependentVisualStates_.end(); ) {
        if (item->second.seen) {
            ++item;
            continue;
        }
        addDirtyRect(item->second.rect);
        item = dependentVisualStates_.erase(item);
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
        auto item = dependentVisualStates_.find(element.id);
        if (item == dependentVisualStates_.end()) {
            dependentVisualStates_.emplace(element.id, current);
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

    const RenderTransform renderTransform = resolveRenderTransform(element, dpiScale, inheritedTransform);
    const std::vector<const Element*>& children = orderedElements(element);
    for (const Element* child : children) {
        updateDependentVisualDirtyRegions(*child, dpiScale, renderTransform);
    }
}

template <typename Fn>
inline void Runtime::forEachElement(Fn&& fn) const {
    const std::vector<const Element*>& roots = orderedElements(ui_);
    for (const Element* root : roots) {
        forEachElement(*root, fn);
    }
}

template <typename Fn>
inline void Runtime::forEachElement(const Element& element, Fn&& fn) {
    fn(element);
    const std::vector<const Element*>& children = orderedElements(element);
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
        !focusedId_.empty()) {
        return false;
    }

    const auto cached = paintBounds_.find(element.id);
    if (cached == paintBounds_.end() || !cached->second.hasSubtree) {
        return false;
    }

    const Rect subtree = toPixelRect(cached->second.subtree, dpiScale);
    if (subtree.contains(event.x, event.y)) {
        return false;
    }

    const RenderTransform renderTransform = resolveRenderTransform(element, dpiScale, inheritedTransform);
    return !renderTransform.active && closeEnough(renderTransform.opacity, 1.0f);
}

inline bool Runtime::elementHasActiveAnimation(const Element& element) const {
    if (element.kind == ElementKind::Row ||
        element.kind == ElementKind::Column ||
        element.kind == ElementKind::Stack ||
        element.kind == ElementKind::Flow) {
        const auto item = layouts_.find(element.id);
        return item != layouts_.end() && isLayoutAnimating(item->second);
    }
    if (element.kind == ElementKind::Rect) {
        const auto item = rects_.find(element.id);
        return item != rects_.end() && isRectAnimating(element, item->second);
    }
    if (element.kind == ElementKind::Polygon) {
        const auto item = polygons_.find(element.id);
        return item != polygons_.end() && isPolygonAnimating(element, item->second);
    }
    if (element.kind == ElementKind::Text) {
        const auto item = texts_.find(element.id);
        return item != texts_.end() && isTextAnimating(item->second);
    }
    if (element.kind == ElementKind::Image || element.kind == ElementKind::Svg) {
        const auto item = images_.find(element.id);
        return item != images_.end() && isImageAnimating(item->second);
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
    runtime::FrameTargetInstance& instance = frameTargets_.try_emplace(element.id).first->second;
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

inline runtime::RectInstance& Runtime::rectInstance(const std::string& id) {
    runtime::RectInstance& instance = rects_.try_emplace(id).first->second;
    instance.seen = true;
    return instance;
}

inline runtime::PolygonInstance& Runtime::polygonInstance(const std::string& id) {
    runtime::PolygonInstance& instance = polygons_.try_emplace(id).first->second;
    instance.seen = true;
    return instance;
}

inline runtime::TextInstance& Runtime::textInstance(const std::string& id) {
    runtime::TextInstance& instance = texts_.try_emplace(id).first->second;
    instance.seen = true;
    return instance;
}

inline runtime::ImageInstance& Runtime::imageInstance(const std::string& id) {
    runtime::ImageInstance& instance = images_.try_emplace(id).first->second;
    instance.seen = true;
    return instance;
}

inline runtime::InteractionInstance& Runtime::interactionInstance(const std::string& id) {
    runtime::InteractionInstance& instance = interactions_.try_emplace(id).first->second;
    instance.seen = true;
    return instance;
}

inline runtime::DirtyKeyInstance& Runtime::dirtyKeyInstance(const std::string& id) {
    runtime::DirtyKeyInstance& instance = dirtyKeys_.try_emplace(id).first->second;
    instance.seen = true;
    return instance;
}

inline runtime::LayoutInstance& Runtime::layoutInstance(const std::string& id) {
    runtime::LayoutInstance& instance = layouts_.try_emplace(id).first->second;
    instance.seen = true;
    return instance;
}

inline runtime::ScrollStateInstance& Runtime::scrollStateInstance(const std::string& id) {
    runtime::ScrollStateInstance& instance = scrollStates_.try_emplace(id).first->second;
    instance.seen = true;
    return instance;
}

inline runtime::SliderStateInstance& Runtime::sliderStateInstance(const std::string& id) {
    runtime::SliderStateInstance& instance = sliderStates_.try_emplace(id).first->second;
    instance.seen = true;
    return instance;
}

inline runtime::TimerInstance& Runtime::timerInstance(const std::string& id) {
    return timers_.try_emplace(id).first->second;
}

inline void Runtime::markInstancesUnseen() {
    runtime::markEntriesUnseen(rects_);
    runtime::markEntriesUnseen(polygons_);
    runtime::markEntriesUnseen(texts_);
    runtime::markEntriesUnseen(images_);
    runtime::markEntriesUnseen(interactions_);
    runtime::markEntriesUnseen(dirtyKeys_);
    runtime::markEntriesUnseen(layouts_);
    runtime::markEntriesUnseen(scrollStates_);
    runtime::markEntriesUnseen(sliderStates_);
    runtime::markEntriesUnseen(frameTargets_);
    runtime::markEntriesUnseen(paintBounds_);
    runtime::markEntriesUnseen(retainedLayers_);
}

inline void Runtime::releaseUnseenInstances() {
    auto releasePrimitive = [](auto& instance) {
        if (instance.initialized) {
            instance.primitive->destroy();
            instance.initialized = false;
        }
    };
    auto noop = [](auto&) {};
    auto releaseLayer = [](runtime::RetainedLayerInstance& instance) {
        core::render::RenderBackend* renderBackend = core::render::activeRenderBackend();
        if (renderBackend != nullptr && instance.handle != nullptr) {
            renderBackend->destroyLayer(instance.handle);
        }
        instance.handle = nullptr;
        instance.valid = false;
    };

    runtime::releaseUnseenEntries(rects_, releasePrimitive);
    runtime::releaseUnseenEntries(polygons_, releasePrimitive);
    runtime::releaseUnseenEntries(texts_, releasePrimitive);
    runtime::releaseUnseenEntries(images_, releasePrimitive);
    runtime::releaseUnseenEntries(interactions_, noop);
    runtime::releaseUnseenEntries(dirtyKeys_, noop);
    runtime::releaseUnseenEntries(layouts_, noop);
    runtime::releaseUnseenEntries(scrollStates_, noop);
    runtime::releaseUnseenEntries(sliderStates_, noop);
    runtime::releaseUnseenEntries(frameTargets_, noop);
    runtime::releaseUnseenEntries(paintBounds_, noop);
    runtime::releaseUnseenEntries(retainedLayers_, releaseLayer);
}

inline void Runtime::markTimersUnseen() {
    for (auto& item : timers_) {
        item.second.seen = false;
    }
}

inline void Runtime::releaseUnseenTimers() {
    for (auto item = timers_.begin(); item != timers_.end(); ) {
        if (!item->second.seen) {
            item = timers_.erase(item);
        } else {
            ++item;
        }
    }
}

inline void Runtime::updateTimer(const Element& element, float deltaSeconds) {
    if (!element.onTimer || element.timerSeconds <= 0.0f) {
        return;
    }

    runtime::TimerInstance& instance = timerInstance(element.id);
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

inline bool Runtime::hoverBlendForSource(const std::string& id, float& value) const {
    const auto rect = rects_.find(id);
    if (rect != rects_.end()) {
        value = rect->second.hoverBlend.value();
        return true;
    }
    const auto polygon = polygons_.find(id);
    if (polygon != polygons_.end()) {
        value = polygon->second.hoverBlend.value();
        return true;
    }
    return false;
}

inline bool Runtime::pressBlendForSource(const std::string& id, float& value, LayoutRect& frame) const {
    const auto rect = rects_.find(id);
    if (rect != rects_.end()) {
        value = rect->second.pressBlend.value();
        frame = rect->second.frame.value();
        return true;
    }
    const auto polygon = polygons_.find(id);
    if (polygon != polygons_.end()) {
        value = polygon->second.pressBlend.value();
        frame = polygon->second.frame.value();
        return true;
    }
    return false;
}

inline RenderTransform Runtime::resolveRenderTransform(const Element& element, float dpiScale, const RenderTransform& inherited) const {
    RenderTransform result = inherited;

    if (element.kind == ElementKind::Row ||
        element.kind == ElementKind::Column ||
        element.kind == ElementKind::Stack ||
        element.kind == ElementKind::Flow) {
        const auto layout = layouts_.find(element.id);
        if (layout != layouts_.end()) {
            const Transform local = layout->second.transform.value();
            const float opacity = layout->second.opacity.value();
            const bool hasTransform = !isIdentityTransform(local);
            const bool hasOpacity = !closeEnough(opacity, 1.0f);
            if (hasTransform || hasOpacity) {
                Transform scaled = scaleTransform(local, dpiScale);
                result = appendRenderMatrix(result, matrixForTransform(toPixelRect(element.frame, dpiScale), scaled));
                result.opacity *= opacity;
            }
        }
    }

    if (!element.hoverOpacitySourceId.empty()) {
        float hover = 0.0f;
        if (hoverBlendForSource(element.hoverOpacitySourceId, hover)) {
            hover = std::clamp(hover, 0.0f, 1.0f);
            result.opacity *= lerpValue(element.hoverHiddenOpacity, element.hoverVisibleOpacity, hover);
        } else {
            result.opacity *= element.hoverHiddenOpacity;
        }
    }

    if (!element.visualStateSourceId.empty()) {
        float press = 0.0f;
        LayoutRect sourceFrame;
        if (pressBlendForSource(element.visualStateSourceId, press, sourceFrame)) {
            const float scale = 1.0f - (1.0f - element.pressedScale) * press;
            if (std::fabs(scale - 1.0f) > 0.0001f) {
                result = appendRenderMatrix(result, matrixForScaleAround(toPixelRect(sourceFrame, dpiScale), scale));
            }
        }
    }
    return result;
}

inline void Runtime::syncScrollStateElement(const Element& element) {
    if (element.scrollStateId.empty()) {
        return;
    }

    runtime::ScrollStateInstance& instance = scrollStateInstance(element.scrollStateId);
    if (!ownsScrollState(element)) {
        return;
    }

    syncOwnedScrollState(element, instance);
}

inline void Runtime::syncSliderStateElement(const Element& element) {
    if (element.sliderStateId.empty()) {
        return;
    }

    runtime::SliderStateInstance& instance = sliderStateInstance(element.sliderStateId);
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
    const auto state = scrollStates_.find(element.scrollStateId);
    if (state != scrollStates_.end()) {
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
    auto state = scrollStates_.find(stateId);
    if (state == scrollStates_.end()) {
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
    auto state = scrollStates_.find(element.scrollStateId);
    if (state == scrollStates_.end()) {
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
    for (auto& entry : scrollStates_) {
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
    auto state = scrollStates_.find(element.scrollDragSourceId);
    if (state == scrollStates_.end()) {
        return;
    }
    state->second.velocity = 0.0f;
    state->second.dragStartOffset = state->second.offset;
}

inline void Runtime::updateRuntimeScrollDrag(const Element& element, double dragDeltaY, float dpiScale) {
    if (element.scrollDragSourceId.empty() || element.scrollDragTravel <= 0.0f) {
        return;
    }
    const auto state = scrollStates_.find(element.scrollDragSourceId);
    if (state == scrollStates_.end() || state->second.maxOffset <= 0.0f) {
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
    const auto state = sliderStates_.find(element.sliderInputSourceId);
    if (state == sliderStates_.end()) {
        return 0.0f;
    }

    const Element* owner = ui_.find(element.sliderInputSourceId);
    if (owner == nullptr) {
        return 0.0f;
    }

    return core::dsl::sliderValueFromPointer(*owner, pointerX, dpiScale);
}

inline void Runtime::setSliderValue(const std::string& stateId, float value, bool dragging) {
    auto state = sliderStates_.find(stateId);
    if (state == sliderStates_.end()) {
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
    runtime::markEntriesUnseen(paintBounds_);
    focusedElementRenderTransformValid_ = false;
    const RenderTransform identity;
    const std::vector<const Element*>& roots = orderedElements(ui_);
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
        runtime::PaintBoundsInstance cached = paintBounds_[element.id];
        cached.seen = true;
        paintBounds_[element.id] = cached;
        return cached;
    }

    const bool frameTargetChanged = updateFrameTarget(element);
    updateExplicitDirtyKey(element, dpiScale, inheritedTransform);
    if (disabledTree) {
        interactionInstance(element.id).state.update({}, event, false, false);
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
    }

    const bool childAncestorFrameChanged = ancestorFrameChanged || frameTargetChanged;
    const RenderTransform renderTransform = resolveRenderTransform(element, dpiScale, inheritedTransform);
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

    const std::vector<const Element*>& children = orderedElements(element);
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

    paintBounds_[element.id] = bounds;
    return bounds;
}

inline void Runtime::updateLayoutElement(
    const Element& element,
    float deltaSeconds,
    float dpiScale,
    const RenderTransform& inheritedTransform,
    const PointerEvent& event,
    const std::string& hoverTargetId) {
    runtime::LayoutInstance& instance = layoutInstance(element.id);
    const Rect beforeRect = applyRenderTransformToLogicalRect(inflateRect(
        transformRect({element.frame.x, element.frame.y, element.frame.width, element.frame.height},
                      element.frame,
                      instance.transform.value()),
        64.0f), dpiScale, inheritedTransform);

    bool changed = false;
    Transform targetTransform = pointerRuntimeTransform(element, event, dpiScale, hoverTargetId);
    targetTransform = core::dsl::runtimeTransformForElement(element, scrollStates_, sliderStates_, targetTransform);
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
    runtime::RectInstance& instance = rectInstance(element.id);
    instance.interaction = interactionInstance(element.id).state;
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
        const auto state = sliderStates_.find(element.sliderFillSourceId);
        if (state != sliderStates_.end()) {
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
    changed = instance.transform.setTarget(core::dsl::runtimeTransformForElement(element, scrollStates_, sliderStates_, element.transform), element.transition, shouldAnimate(element, AnimProperty::Transform)) || changed;

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
    runtime::PolygonInstance& instance = polygonInstance(element.id);
    instance.interaction = interactionInstance(element.id).state;
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
    changed = instance.transform.setTarget(core::dsl::runtimeTransformForElement(element, scrollStates_, sliderStates_, element.transform), element.transition, shouldAnimate(element, AnimProperty::Transform)) || changed;

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
    runtime::TextInstance& instance = textInstance(element.id);
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
    changed = instance.transform.setTarget(core::dsl::runtimeTransformForElement(element, scrollStates_, sliderStates_, element.transform), element.transition, shouldAnimate(element, AnimProperty::Transform)) || changed;

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
    runtime::ImageInstance& instance = imageInstance(element.id);
    const Rect beforeRect = applyRenderTransformToLogicalRect(
        imageVisualRect(instance.frame.value(), instance.transform.value()),
        dpiScale,
        inheritedTransform);

    bool changed = false;
    changed = instance.frame.setTarget(element.frame, element.transition, !snapFrame && shouldAnimateFrame(element)) || changed;
    changed = instance.tint.setTarget(element.color, element.transition, shouldAnimate(element, AnimProperty::Color)) || changed;
    changed = instance.radius.setTarget(element.radius, element.transition, shouldAnimate(element, AnimProperty::Radius)) || changed;
    changed = instance.opacity.setTarget(element.opacity, element.transition, shouldAnimate(element, AnimProperty::Opacity)) || changed;
    changed = instance.transform.setTarget(core::dsl::runtimeTransformForElement(element, scrollStates_, sliderStates_, element.transform), element.transition, shouldAnimate(element, AnimProperty::Transform)) || changed;

    changed = instance.frame.tick(deltaSeconds) || changed;
    changed = instance.tint.tick(deltaSeconds) || changed;
    changed = instance.radius.tick(deltaSeconds) || changed;
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

} // namespace core::dsl
