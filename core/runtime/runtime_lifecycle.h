#pragma once

namespace core::dsl {

inline bool Runtime::initialize() {
    return true;
}

inline bool Runtime::initialize(core::window::Handle window) {
    installInputCallbacks(window);
    return true;
}

template <typename ComposeFn>
inline void Runtime::compose(const std::string& pageId, float logicalWidth, float logicalHeight, ComposeFn&& composeFn) {
    const std::vector<runtime::ElementSnapshot> previousStructure = elementStructure_;
    const Screen screen{logicalWidth, logicalHeight};
    ui_.begin(pageId);
    ui_.setFocusedId(focusedId_);
    composeFn(ui_, screen);
    ui_.end();
    ui_.layout(screen);
    elementStructure_ = collectElementStructure();
    syncScrollStateBindings();
    for (const std::string& scope : ui_.consumeReleasedStateScopes()) {
        const std::string childPrefix = scope + ".";
        if (focusedId_ == scope || focusedId_.rfind(childPrefix, 0) == 0) {
            focusedId_.clear();
            ui_.setFocusedId(focusedId_);
        }
    }
    if (!focusedId_.empty() && isElementInDisabledTree(focusedId_)) {
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
        pruneInstancesRequested_ = true;
    }
    fullTreeUpdateRequested_ = true;
    logicalWidth_ = logicalWidth;
    logicalHeight_ = logicalHeight;
}

inline bool Runtime::update(core::window::Handle window, float deltaSeconds, float pointerScale, float dpiScale, bool inputEnabled) {
    PointerEvent event = readPointerEvent(window, pointerScale);
    const auto inputEvents = consumeInputEvents(window);
    KeyboardEvent keyboardEvent = inputEvents.first;
    ScrollEvent scrollEvent = inputEvents.second;
    if (!inputEnabled) {
        event.x = -1000000.0;
        event.y = -1000000.0;
        event.deltaX = 0.0;
        event.deltaY = 0.0;
        event.down = false;
        event.pressedThisFrame = false;
        event.releasedThisFrame = true;
        event.rightDown = false;
        event.rightPressedThisFrame = false;
        event.rightReleasedThisFrame = false;
        keyboardEvent = {};
        scrollEvent = {};
    }
    animating_ = false;
    composeRequested_ = false;
    wantsHandCursor_ = false;
    if (pruneInstancesRequested_) {
        markInstancesUnseen();
    }
    markTimersUnseen();
    if (ImagePrimitive::consumeRemoteImageReady()) {
        fullPaintRequested_ = true;
        paintRequested_ = true;
    }

    syncScrollStateBindings();
    if (scrollEvent.active()) {
        updateScroll(scrollEvent, hitTestScrollable(event, dpiScale));
        hoverTargetCacheValid_ = false;
    }
    updateScrollMotion(deltaSeconds);

    if (event.pressedThisFrame) {
        setFocusedId(hitTestFocusable(event, dpiScale));
    }

    const std::string hoverTargetId = resolveHoverTarget(event, dpiScale, inputEnabled);
    updateElementTree(event, deltaSeconds, dpiScale, hoverTargetId);
    updateDependentVisualDirtyRegions(dpiScale);

    if (keyboardEvent.hasInput()) {
        updateTextInput(keyboardEvent);
    }
    releaseUnseenTimers();
    updateImeCursorRect(window, dpiScale);
    applyCursor(window);

    promoteBackdropBlurDirtyRegions(dpiScale);
    if (pruneInstancesRequested_) {
        releaseUnseenInstances();
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
    core::render::RenderFrameStats& stats = core::render::currentRenderFrameStats();

    const bool hasRenderableContent = !ui_.roots().empty();
    if (!hasRenderableContent) {
        ++stats.clearCalls;
        renderBackend->clear(clearColor);
        dirtyRects_.clear();
        fullPaintRequested_ = false;
        core::render::publishRenderFrameStats();
        return;
    }

    if (!renderBackend->ensureRenderCache(windowWidth, windowHeight)) {
        ++stats.clearCalls;
        renderBackend->clear(clearColor);
        ++stats.renderDirectPasses;
        renderDirect(*renderBackend, windowWidth, windowHeight, dpiScale);
        dirtyRects_.clear();
        fullPaintRequested_ = false;
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
        renderDirect(*renderBackend, windowWidth, windowHeight, dpiScale);
    } else {
        for (const Rect& dirty : dirtyRects) {
            renderBackend->setScissor(true, dirty, windowHeight);
            ++stats.clearCalls;
            renderBackend->clear(clearColor);
            ++stats.renderDirectPasses;
            renderDirect(*renderBackend, windowWidth, windowHeight, dpiScale, &dirty);
        }
        renderBackend->setScissor(false, {}, windowHeight);
    }

    renderBackend->endRenderCacheFrame();
    renderBackend->blitRenderCache(windowWidth,
                                   windowHeight,
                                   fullPaintRequested_ ? core::render::RenderCacheBlitMode::Full
                                                       : core::render::RenderCacheBlitMode::Dirty,
                                   dirtyRects);
    const bool retainedLayerRebuilt = stats.retainedLayerRebuilds > 0;
    dirtyRects_.clear();
    fullPaintRequested_ = retainedLayerRebuilt;
    paintRequested_ = retainedLayerRebuilt;
    core::render::publishRenderFrameStats();
}

inline void Runtime::render(int windowWidth, int windowHeight, float dpiScale) {
    core::render::RenderBackend* renderBackend = core::render::activeRenderBackend();
    if (renderBackend == nullptr) {
        return;
    }

    const RenderTransform identity;
    const std::vector<const Element*>& roots = orderedElements(ui_);
    for (const Element* root : roots) {
        prepareTextElement(*root, windowWidth, windowHeight, dpiScale, identity);
    }
    for (const Element* root : roots) {
        renderElement(*renderBackend, *root, windowWidth, windowHeight, dpiScale, identity);
    }
}

inline void Runtime::shutdown(bool releaseCachedImageTextures) {
    releaseGraphicsResources(releaseCachedImageTextures);
    rects_.clear();
    polygons_.clear();
    texts_.clear();
    images_.clear();
    interactions_.clear();
    dirtyKeys_.clear();
    layouts_.clear();
    scrollStates_.clear();
    sliderStates_.clear();
    timers_.clear();
    dependentVisualStates_.clear();
    frameTargets_.clear();
    paintBounds_.clear();
    retainedLayers_.clear();
    elementStructure_.clear();
    hoverTargetCacheValid_ = false;
    ui_.clearState();
}

inline void Runtime::releaseGraphicsResources(bool releaseCachedImageTextures) {
    for (auto& item : rects_) {
        if (item.second.initialized) {
            item.second.primitive->destroy();
            item.second.initialized = false;
        }
    }
    for (auto& item : polygons_) {
        if (item.second.initialized) {
            item.second.primitive->destroy();
            item.second.initialized = false;
        }
    }
    for (auto& item : texts_) {
        if (item.second.initialized) {
            item.second.primitive->destroy();
            item.second.initialized = false;
        }
    }
    for (auto& item : images_) {
        if (item.second.initialized) {
            item.second.primitive->destroy();
            item.second.initialized = false;
        }
    }
    if (releaseCachedImageTextures) {
        ImagePrimitive::releaseCachedTextures();
    }
    core::render::RenderBackend* renderBackend = core::render::activeRenderBackend();
    if (renderBackend != nullptr) {
        for (auto& item : retainedLayers_) {
            if (item.second.handle != nullptr) {
                renderBackend->destroyLayer(item.second.handle);
                item.second.handle = nullptr;
            }
            item.second.valid = false;
        }
    }
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
