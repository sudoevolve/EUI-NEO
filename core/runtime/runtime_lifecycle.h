#pragma once

namespace core::dsl {

inline bool Runtime::initialize() {
    return true;
}

inline bool Runtime::initialize(core::window::Handle window) {
    core::window::installInputCallbacks(window);
    return true;
}

template <typename ComposeFn>
inline void Runtime::compose(const std::string& pageId, float logicalWidth, float logicalHeight, ComposeFn&& composeFn) {
    const std::vector<runtime::ElementSnapshot> previousStructure = elementStructure_;
    const Screen screen{logicalWidth, logicalHeight};
    ui_.begin(pageId);
    ui_.setFocusedId(focusedId_);
    ui_.setFocusVisible(focusVisible_);
    composeFn(ui_, screen);
    ui_.end();
    ui_.layout(screen);
    const auto composedFocus = focusedId_;
    synchronizeFocusScope();
    if (!ui_.focusRequest_.empty()) {
        const auto request = std::move(ui_.focusRequest_);
        ui_.focusRequest_.clear();
        const auto order = focusOrder();
        if (std::find(order.begin(), order.end(), request) != order.end()) setFocusedId(request);
    }
    if (focusedId_ != composedFocus) {
        ui_.end();
        ui_.layout(screen);
    }
    elementStructure_ = collectElementStructure();
    syncScrollStateBindings();
    for (const std::string& scope : ui_.consumeReleasedStateScopes()) {
        const std::string childPrefix = scope + ".";
        if (focusedId_ == scope || focusedId_.rfind(childPrefix, 0) == 0) {
            focusedId_.clear();
            ui_.setFocusedId(focusedId_);
        }
    }
    if (!focusedId_.empty() && (!ui_.find(focusedId_) || isElementInDisabledTree(focusedId_))) {
        setFocusedId({});
    }

    if (elementStructure_ != previousStructure) {
        paintRequested_ = true;
        fullPaintRequested_ = true;
        pruneInstancesRequested_ = true;
    }

    if (logicalWidth_ != logicalWidth || logicalHeight_ != logicalHeight) {
        paintRequested_ = true;
        fullPaintRequested_ = true;
    }
    fullTreeUpdateRequested_ = true;
    logicalWidth_ = logicalWidth;
    logicalHeight_ = logicalHeight;
}

inline bool Runtime::update(core::window::Handle window, float deltaSeconds, float pointerScale, float dpiScale, bool inputEnabled) {
    ++updateFrameToken_;
    if (updateFrameToken_ == 0) {
        ++updateFrameToken_;
    }
    if (inputEnabled) {
        core::window::pollTouchInput(window, pointerScale);
    } else {
        cancelInput(window);
        touchScrollMoved_ = false;
        touchScrollDistance_ = 0.0;
    }
    std::vector<std::uint64_t> pointerSequences;
    std::vector<std::uint64_t> gestureSequences;
    std::vector<PointerEvent> pointerEvents = consumePointerEvents(window, pointerScale, &pointerSequences);
    std::vector<KeyEvent> keyEvents = consumeKeyEvents(window);
    TextInputEvent textInputEvent = consumeTextInput(window);
    ScrollEvent scrollEvent = consumeScrollInput(window);
    std::vector<GestureEvent> gestureEvents = consumeGestureInput(window, pointerScale, &gestureSequences);
    for (const PointerEvent& event : pointerEvents) {
        if (event.touch && event.isPress(PointerButton::Left)) {
            touchScrollMoved_ = false;
            touchScrollDistance_ = 0.0;
        }
    }
    if (scrollEvent.pixelX != 0.0 || scrollEvent.pixelY != 0.0) {
        touchScrollDistance_ += std::hypot(scrollEvent.pixelX, scrollEvent.pixelY);
        touchScrollMoved_ = touchScrollDistance_ >= 6.0 * (dpiScale > 0.0f ? dpiScale : 1.0f);
    }
    if (!inputEnabled) {
        for (PointerEvent& event : pointerEvents) {
            event.x = -1000000.0;
            event.y = -1000000.0;
            event.deltaX = 0.0;
            event.deltaY = 0.0;
            event.modifiers = {};
        }
        keyEvents.clear();
        textInputEvent = {};
        scrollEvent = {};
    }
    animating_ = false;
    composeRequested_ = false;
    wantsHandCursor_ = false;
    if (pruneInstancesRequested_) {
        instances_.markInstancesUnseen();
    }
    instances_.markTimersUnseen();
    if (ImagePrimitive::consumeRemoteImageReady()) {
        fullPaintRequested_ = true;
        paintRequested_ = true;
    }

    syncScrollStateBindings();
    if (scrollEvent.active()) {
        updateScroll(scrollEvent, hitTestScrollable(pointerEvents.back(), dpiScale), dpiScale);
        hoverTargetCacheValid_ = false;
    }
    updateScrollMotion(deltaSeconds);

    std::size_t pointerIndex = 0;
    std::size_t gestureIndex = 0;
    while (pointerIndex < pointerEvents.size() || gestureIndex < gestureEvents.size()) {
        if (gestureIndex < gestureEvents.size() &&
            (pointerIndex == pointerEvents.size() ||
             gestureSequences[gestureIndex] < pointerSequences[pointerIndex])) {
            const GestureEvent& event = gestureEvents[gestureIndex++];
            if (inputEnabled || event.phase == GesturePhase::Cancel) {
                updateGesture(event, dpiScale);
            }
            continue;
        }
        const PointerEvent& event = pointerEvents[pointerIndex];
        if (inputEnabled && event.action == PointerAction::Press) {
            setFocusVisible(false);
            dismissOutsidePress(hitTestInteractive(event, dpiScale));
        }
        if (event.isPress(PointerButton::Left)) {
            setFocusedId(hitTestFocusable(event, dpiScale));
        }
        const std::string hoverTargetId = resolveHoverTarget(event, dpiScale, inputEnabled);
        const float eventDeltaSeconds = pointerIndex + 1 == pointerEvents.size() ? deltaSeconds : 0.0f;
        updateElementTree(event, eventDeltaSeconds, dpiScale, hoverTargetId);
        ++pointerIndex;
    }
    for (const PointerEvent& event : pointerEvents) {
        if (event.touch && (event.action == PointerAction::Release || event.action == PointerAction::Cancel)) {
            touchScrollMoved_ = false;
            touchScrollDistance_ = 0.0;
            break;
        }
    }
    updateDependentVisualDirtyRegions(dpiScale);

    if (!keyEvents.empty()) {
        updateKeyInput(keyEvents);
    }
    if (textInputEvent.hasInput()) {
        updateTextInput(textInputEvent);
    }
    instances_.releaseUnseenTimers();
    updateImeCursorRect(window, dpiScale);
    applyCursor(window);

    promoteBackdropBlurDirtyRegions(dpiScale);
    if (pruneInstancesRequested_) {
        instances_.releaseUnseenInstances();
        pruneInstancesRequested_ = false;
    }
    fullTreeUpdateRequested_ = false;
    previousFrameAnimating_ = animating_;

    const bool result = paintRequested_;
    paintRequested_ = false;
    return result;
}

inline bool Runtime::isAnimating() const {
    return animating_;
}

inline bool Runtime::composeRequested() const {
    return composeRequested_;
}

inline bool Runtime::paintRequested() const {
    return paintRequested_;
}

inline void Runtime::requestFullPaint() {
    fullPaintRequested_ = true;
    paintRequested_ = true;
}

inline void Runtime::render(int windowWidth, int windowHeight, float dpiScale, const Color& clearColor) {
    core::render::RenderBackend* renderBackend = core::render::activeRenderBackend();
    if (renderBackend == nullptr) {
        return;
    }

    core::render::beginRenderFrameStats(windowWidth, windowHeight);
    ImagePrimitive::beginRenderFrame();
    core::render::RenderFrameStats& stats = core::render::currentRenderFrameStats();

    const bool hasRenderableContent = !ui_.roots().empty();
    const auto releasePrunedRetainedLayers = [&] {
        instances_.releaseUnseenRetainedLayers();
    };
    if (!hasRenderableContent) {
        ++stats.clearCalls;
        renderBackend->clear(clearColor);
        dirtyRects_.clear();
        fullPaintRequested_ = false;
        releasePrunedRetainedLayers();
        core::render::publishRenderFrameStats();
        return;
    }

    if (!renderBackend->ensureRenderCache(windowWidth, windowHeight)) {
        ++stats.clearCalls;
        renderBackend->clear(clearColor);
        ++stats.renderDirectPasses;
        RuntimeRenderer(ui_, instances_).renderDirect(
            *renderBackend, windowWidth, windowHeight, dpiScale);
        dirtyRects_.clear();
        fullPaintRequested_ = false;
        releasePrunedRetainedLayers();
        core::render::publishRenderFrameStats();
        return;
    }
    stats.usedRenderCache = true;
    if (renderBackend->renderCacheWasRecreated()) {
        fullPaintRequested_ = true;
        stats.renderCacheRecreated = true;
    }

    if (!fullPaintRequested_ && dirtyRects_.empty()) {
        renderBackend->blitRenderCache(windowWidth, windowHeight, core::render::RenderCacheBlitMode::Existing);
        releasePrunedRetainedLayers();
        core::render::publishRenderFrameStats();
        return;
    }

    stats.fullPaint = fullPaintRequested_;
    const std::vector<Rect> dirtyRects = fullPaintRequested_
        ? std::vector<Rect>{}
        : core::dsl::resolveDirtyRects(dirtyRects_, windowWidth, windowHeight, dpiScale);
    if (!fullPaintRequested_ && dirtyRects.empty()) {
        dirtyRects_.clear();
        renderBackend->blitRenderCache(windowWidth, windowHeight, core::render::RenderCacheBlitMode::Existing);
        releasePrunedRetainedLayers();
        core::render::publishRenderFrameStats();
        return;
    }
    stats.dirtyRectCount = static_cast<int>(dirtyRects.size());
    for (const Rect& dirty : dirtyRects) {
        const float width = std::max(0.0f, dirty.width);
        const float height = std::max(0.0f, dirty.height);
        stats.dirtyPixels += static_cast<std::uint64_t>(width * height);
    }

    renderBackend->beginRenderCacheFrame(windowWidth, windowHeight, dirtyRects);

    if (fullPaintRequested_) {
        renderBackend->setScissor(false, {}, windowHeight);
        ++stats.clearCalls;
        renderBackend->clear(clearColor);
        ++stats.renderDirectPasses;
        RuntimeRenderer(ui_, instances_).renderDirect(
            *renderBackend, windowWidth, windowHeight, dpiScale);
    } else {
        for (const Rect& dirty : dirtyRects) {
            renderBackend->setScissor(true, dirty, windowHeight);
            ++stats.clearCalls;
            renderBackend->clear(clearColor);
            ++stats.renderDirectPasses;
            RuntimeRenderer(ui_, instances_).renderDirect(
                *renderBackend, windowWidth, windowHeight, dpiScale, &dirty);
        }
        renderBackend->setScissor(false, {}, windowHeight);
    }

    renderBackend->endRenderCacheFrame();
    renderBackend->blitRenderCache(windowWidth,
                                   windowHeight,
                                   fullPaintRequested_ ? core::render::RenderCacheBlitMode::Full
                                                       : core::render::RenderCacheBlitMode::Dirty,
                                   dirtyRects);
    const bool retainedLayerWarmupNeeded =
        stats.retainedLayerMisses > 0 && stats.retainedLayerRebuilds == 0;
    dirtyRects_.clear();
    fullPaintRequested_ = retainedLayerWarmupNeeded;
    paintRequested_ = retainedLayerWarmupNeeded;
    releasePrunedRetainedLayers();
    core::render::publishRenderFrameStats();
}

inline void Runtime::render(int windowWidth, int windowHeight, float dpiScale) {
    core::render::RenderBackend* renderBackend = core::render::activeRenderBackend();
    if (renderBackend == nullptr) {
        return;
    }

    ImagePrimitive::beginRenderFrame();

    RuntimeRenderer(ui_, instances_).renderDirect(
        *renderBackend, windowWidth, windowHeight, dpiScale);
    instances_.releaseUnseenRetainedLayers();
}

inline void Runtime::shutdown(bool releaseCachedImageTextures) {
    focusScopes_.clear();
    focusVisible_ = false;
    ui_.setFocusVisible(false);
    releaseGraphicsResources(releaseCachedImageTextures);
    instances_.clear();
    elementStructure_.clear();
    hoverTargetCacheValid_ = false;
    gestureTargetId_.clear();
    // 元素与回调也可能持有外部 GPU 资源，不能留到设备销毁后的 Runtime 析构。
    ui_.begin();
    ui_.end();
    ui_.clearState();
    keyEventHandler_ = {};
}

inline void Runtime::releaseGraphicsResources(bool releaseCachedImageTextures) {
    instances_.releaseGraphicsResources(releaseCachedImageTextures);
    destroyCursors();
    fullPaintRequested_ = true;
    paintRequested_ = true;
}

inline void Runtime::applyCursor(core::window::Handle window) {
    if (!arrowCursor_) {
        arrowCursor_ = core::window::createStandardCursor(core::window::CursorType::Arrow);
    }
    if (!handCursor_) {
        handCursor_ = core::window::createStandardCursor(core::window::CursorType::Hand);
    }

    core::window::CursorHandle target = wantsHandCursor_ && handCursor_ ? handCursor_ : arrowCursor_;
    if (target != currentCursor_) {
        core::window::setCursor(window, target);
        currentCursor_ = target;
    }
}

inline void Runtime::destroyCursors() {
    if (arrowCursor_) {
        core::window::destroyCursor(arrowCursor_);
        arrowCursor_ = nullptr;
    }
    if (handCursor_) {
        core::window::destroyCursor(handCursor_);
        handCursor_ = nullptr;
    }
    currentCursor_ = nullptr;
}

} // namespace core::dsl
