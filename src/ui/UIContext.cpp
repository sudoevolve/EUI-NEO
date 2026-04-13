#include "UIContext.h"
#include <algorithm>
#include <cstdint>
#include <unordered_set>

namespace EUINEO {

namespace {

UIClipRect IntersectClipRect(const UIClipRect& lhs, const UIClipRect& rhs) {
    const float x1 = std::max(lhs.x, rhs.x);
    const float y1 = std::max(lhs.y, rhs.y);
    const float x2 = std::min(lhs.x + lhs.width, rhs.x + rhs.width);
    const float y2 = std::min(lhs.y + lhs.height, rhs.y + rhs.height);

    UIClipRect clipped;
    clipped.x = x1;
    clipped.y = y1;
    clipped.width = std::max(0.0f, x2 - x1);
    clipped.height = std::max(0.0f, y2 - y1);
    return clipped;
}

int LayerDrawPriority(RenderLayer layer) {
    switch (layer) {
        case RenderLayer::Backdrop:
            return 0;
        case RenderLayer::Content:
            return 1;
        case RenderLayer::Chrome:
            return 2;
        case RenderLayer::Popup:
            return 3;
        default:
            return 0;
    }
}

bool NodeDrawBefore(const UINode* lhs, const UINode* rhs) {
    const int lhsLayer = LayerDrawPriority(lhs->renderLayer());
    const int rhsLayer = LayerDrawPriority(rhs->renderLayer());
    if (lhsLayer != rhsLayer) {
        return lhsLayer < rhsLayer;
    }
    const bool lhsExplicitZ = lhs->hasExplicitZIndex();
    const bool rhsExplicitZ = rhs->hasExplicitZIndex();
    if (lhsExplicitZ || rhsExplicitZ) {
        if (lhs->zIndex() != rhs->zIndex()) {
            return lhs->zIndex() < rhs->zIndex();
        }
    }
    return false;
}

float ClampDimension(float value) {
    return std::max(0.0f, value);
}

float MainMarginBefore(const LayoutBuildInfo& build, FlexDirection direction) {
    return direction == FlexDirection::Row ? build.marginLeft : build.marginTop;
}

float MainMarginAfter(const LayoutBuildInfo& build, FlexDirection direction) {
    return direction == FlexDirection::Row ? build.marginRight : build.marginBottom;
}

float CrossMarginBefore(const LayoutBuildInfo& build, FlexDirection direction) {
    return direction == FlexDirection::Row ? build.marginTop : build.marginLeft;
}

float CrossMarginAfter(const LayoutBuildInfo& build, FlexDirection direction) {
    return direction == FlexDirection::Row ? build.marginBottom : build.marginRight;
}

float ResolveMainOffset(MainAxisAlignment alignment, float availableMain, float occupiedMain) {
    const float freeSpace = std::max(0.0f, availableMain - occupiedMain);
    if (alignment == MainAxisAlignment::Center) {
        return freeSpace * 0.5f;
    }
    if (alignment == MainAxisAlignment::End) {
        return freeSpace;
    }
    return 0.0f;
}

float ResolveCrossOffset(FlexDirection direction, CrossAxisAlignment alignment, float availableCross, float crossSize) {
    CrossAxisAlignment effective = alignment;
    if (effective == CrossAxisAlignment::Auto) {
        effective = direction == FlexDirection::Row ? CrossAxisAlignment::Center : CrossAxisAlignment::Start;
    }
    const float freeSpace = std::max(0.0f, availableCross - crossSize);
    if (effective == CrossAxisAlignment::Center) {
        return freeSpace * 0.5f;
    }
    if (effective == CrossAxisAlignment::End) {
        return freeSpace;
    }
    return 0.0f;
}

void HashCombine(std::uint64_t& seed, std::uint64_t value) {
    seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
}

bool HasFrameInputActivity() {
    if (State.pointerMoved || State.mouseClicked || State.mouseReleased ||
        State.mouseRightClicked || State.mouseRightReleased ||
        State.mouseDown || State.mouseRightDown) {
        return true;
    }
    if (State.scrollDeltaX != 0.0f || State.scrollDeltaY != 0.0f) {
        return true;
    }
    if (!State.textInput.empty()) {
        return true;
    }
    for (bool pressed : State.keysPressed) {
        if (pressed) {
            return true;
        }
    }
    return false;
}

bool IsPointerMoveOnlyActivity() {
    if (!State.pointerMoved) {
        return false;
    }
    if (State.mouseClicked || State.mouseReleased ||
        State.mouseRightClicked || State.mouseRightReleased ||
        State.mouseDown || State.mouseRightDown) {
        return false;
    }
    if (State.scrollDeltaX != 0.0f || State.scrollDeltaY != 0.0f) {
        return false;
    }
    if (!State.textInput.empty()) {
        return false;
    }
    for (bool pressed : State.keysPressed) {
        if (pressed) {
            return false;
        }
    }
    return true;
}

} // namespace

void FinalizeUIBuild(UIContext& context, UINode& node, const LayoutBuildInfo& info) {
    context.finalizeBuild(node, info);
}

void UIContext::begin(const std::string& pageId) {
    pageId_ = pageId;
    ++composeStamp_;

    // Garbage collection for Immediate Mode UI
    // Remove nodes that haven't been seen in the last 120 frames (approx 1-2 seconds)
    // This is crucial for Virtual Lists and dynamic data where IDs change frequently
    bool removedNode = false;
    for (auto it = nodes_.begin(); it != nodes_.end(); ) {
        if (it->second && !it->second->composedIn(composeStamp_) && 
           (it->second->composedIn(0) || composeStamp_ - it->second->composeStamp() > 120)) {
            Renderer::ReleaseCachedSurface(it->second->key());
            removedNode = true;
            it = nodes_.erase(it);
        } else {
            ++it;
        }
    }

    order_.clear();
    if (removedNode) {
        drawOrder_.clear();
        drawOrderInitialized_ = false;
        drawOrderSignature_ = 0;
        pointerHotNodes_.clear();
    }
    clipStack_.clear();
    offsetStack_.clear();
    scrollScopeStack_.clear();
    ownedLayouts_.clear();
    layoutStack_.clear();
    baseContextOffset_.clear();
    scrollBindings_.clear();
    currentOffsetX_ = 0.0f;
    currentOffsetY_ = 0.0f;
    treeChanged_ = false;
    needsRecompose_ = false;
}

void UIContext::end() {
    bool dirtyLayers[static_cast<std::size_t>(RenderLayer::Count)] = {};

    for (UINode* node : order_) {
        if (!node->cacheDirty()) {
            continue;
        }
        if (node->visible() && node->primitive().blur <= 0.0f && node->usesCachedSurface()) {
            const RectFrame bounds = node->paintBounds();
            if (bounds.width <= 0.0f || bounds.height <= 0.0f) {
                continue;
            }
        }
        const std::size_t layerIndex = static_cast<std::size_t>(node->renderLayer());
        if (layerIndex < static_cast<std::size_t>(RenderLayer::Count)) {
            dirtyLayers[layerIndex] = true;
        }
    }

    bool anyDirty = treeChanged_;
    for (std::size_t index = 0; index < static_cast<std::size_t>(RenderLayer::Count); ++index) {
        if (!dirtyLayers[index]) {
            continue;
        }
        anyDirty = true;
        Renderer::InvalidateLayer(static_cast<RenderLayer>(index));
    }

    if (anyDirty) {
        Renderer::RequestRepaint();
    }
}

void UIContext::pushClip(float x, float y, float width, float height) {
    UIClipRect clip;
    clip.x = x;
    clip.y = y;
    clip.width = width;
    clip.height = height;

    if (!clipStack_.empty()) {
        clip = IntersectClipRect(clipStack_.back(), clip);
    }

    clipStack_.push_back(clip);
}

void UIContext::popClip() {
    if (!clipStack_.empty()) {
        clipStack_.pop_back();
    }
}

float UIContext::pushScrollArea(const std::string& id, float x, float y, float width, float height,
                                float contentHeight, float scrollStep, RenderLayer scrollbarLayer) {
    ScrollAreaNode& node = acquire<ScrollAreaNode>(id);
    UIPrimitive& primitive = node.primitive();
    primitive.x = x;
    primitive.y = y;
    primitive.width = width;
    primitive.height = height;
    primitive.renderLayer = scrollbarLayer;

    node.trackComposeValue("contentHeight", contentHeight);
    node.trackComposeValue("scrollStep", scrollStep);
    node.setContentHeight(contentHeight);
    node.setScrollStep(scrollStep);
    node.finishCompose();

    const float scrollOffsetY = node.scrollOffsetY();
    pushClip(x, y, width, height);
    scrollScopeStack_.push_back(&node);
    return scrollOffsetY;
}

void UIContext::popScrollArea() {
    if (!scrollScopeStack_.empty()) {
        scrollScopeStack_.pop_back();
    }
    popClip();
}

void UIContext::applyRuntimeContext(UINode* node) {
    if (node == nullptr) {
        return;
    }
    auto baseIt = baseContextOffset_.find(node);
    if (baseIt == baseContextOffset_.end()) {
        return;
    }
    float scrollOffsetY = 0.0f;
    auto bindingIt = scrollBindings_.find(node);
    if (bindingIt != scrollBindings_.end()) {
        for (ScrollAreaNode* area : bindingIt->second) {
            if (area != nullptr) {
                scrollOffsetY += area->scrollOffsetY();
            }
        }
    }
    UIPrimitive& primitive = node->primitive();
    primitive.contextOffsetX = baseIt->second.x;
    primitive.contextOffsetY = baseIt->second.y - scrollOffsetY;
}

UIContext::LayoutState* UIContext::createLayout(FlexDirection direction) {
    auto layout = std::make_unique<LayoutState>();
    layout->direction = direction;
    LayoutState* raw = layout.get();
    ownedLayouts_.push_back(std::move(layout));
    return raw;
}

void UIContext::beginLayout(LayoutState* layout) {
    if (layout == nullptr) {
        return;
    }

    if (!layoutStack_.empty()) {
        LayoutItem item;
        item.container = layout;
        item.build = layout->build;
        layoutStack_.back()->children.push_back(item);
    }

    layoutStack_.push_back(layout);
}

void UIContext::endLayout(LayoutState* layout) {
    if (layout == nullptr || layoutStack_.empty()) {
        return;
    }

    if (layoutStack_.back() == layout) {
        layoutStack_.pop_back();
    }

    if (layoutStack_.empty()) {
        resolveLayout(*layout, resolveLayoutFrame(*layout));
    }
}

void UIContext::finalizeBuild(UINode& node, const LayoutBuildInfo& info) {
    if (layoutStack_.empty()) {
        node.finishCompose();
        return;
    }
    registerLayoutChild(node, info);
}

void UIContext::registerLayoutChild(UINode& node, const LayoutBuildInfo& info) {
    LayoutItem item;
    item.node = &node;
    item.build = info;
    layoutStack_.back()->children.push_back(item);
}

RectFrame UIContext::resolveLayoutFrame(const LayoutState& layout) const {
    return RectFrame{
        layout.build.hasX ? layout.build.x : 0.0f,
        layout.build.hasY ? layout.build.y : 0.0f,
        layout.build.hasWidth ? layout.build.width : 0.0f,
        layout.build.hasHeight ? layout.build.height : 0.0f
    };
}

float UIContext::preferredMainSize(const LayoutItem& item, FlexDirection direction) const {
    const bool isRow = direction == FlexDirection::Row;
    const float explicitSize = isRow ? item.build.width : item.build.height;
    const bool hasExplicitSize = isRow ? item.build.hasWidth : item.build.hasHeight;
    if (hasExplicitSize) {
        return ClampDimension(explicitSize);
    }

    if (item.container != nullptr) {
        return ClampDimension(explicitSize);
    }

    if (item.node == nullptr) {
        return 0.0f;
    }

    const UIPrimitive& primitive = item.node->primitive();
    const float primitiveSize = isRow ? primitive.width : primitive.height;
    if (primitiveSize > 0.0f) {
        return primitiveSize;
    }

    const RectFrame bounds = item.node->layoutBounds();
    return ClampDimension(isRow ? bounds.width : bounds.height);
}

float UIContext::resolveMainSize(const LayoutItem& item, FlexDirection direction,
                                 float flexSpace, float totalFlex) const {
    if (item.build.flex > 0.0f && totalFlex > 0.0f) {
        return ClampDimension(flexSpace * (item.build.flex / totalFlex));
    }
    return preferredMainSize(item, direction);
}

float UIContext::resolveCrossSize(const LayoutItem& item, FlexDirection direction, float availableCross) const {
    const bool isRow = direction == FlexDirection::Row;
    const float explicitSize = isRow ? item.build.height : item.build.width;
    const bool hasExplicitSize = isRow ? item.build.hasHeight : item.build.hasWidth;
    if (hasExplicitSize) {
        return ClampDimension(explicitSize);
    }

    if (item.container != nullptr) {
        return ClampDimension(availableCross);
    }

    if (item.node == nullptr) {
        return ClampDimension(availableCross);
    }

    const UIPrimitive& primitive = item.node->primitive();
    const float primitiveSize = isRow ? primitive.height : primitive.width;
    if (!isRow) {
        return ClampDimension(availableCross);
    }
    if (primitiveSize > 0.0f) {
        return std::min(primitiveSize, ClampDimension(availableCross));
    }

    const RectFrame bounds = item.node->layoutBounds();
    const float intrinsicCross = ClampDimension(bounds.height);
    if (intrinsicCross > 0.0f) {
        return std::min(intrinsicCross, ClampDimension(availableCross));
    }
    return ClampDimension(availableCross);
}

RectFrame UIContext::resolveItemFrame(const LayoutItem& item, FlexDirection direction,
                                      float cursor, float crossStart,
                                      float mainSize, float crossSize) const {
    if (direction == FlexDirection::Row) {
        return RectFrame{
            cursor + (item.build.hasX ? item.build.x : 0.0f),
            crossStart + (item.build.hasY ? item.build.y : 0.0f),
            ClampDimension(mainSize),
            ClampDimension(crossSize)
        };
    }

    return RectFrame{
        crossStart + (item.build.hasX ? item.build.x : 0.0f),
        cursor + (item.build.hasY ? item.build.y : 0.0f),
        ClampDimension(crossSize),
        ClampDimension(mainSize)
    };
}

void UIContext::applyResolvedFrame(UINode& node, const RectFrame& frame) {
    UIPrimitive& primitive = node.primitive();
    primitive.width = frame.width;
    primitive.height = frame.height;
    if (primitive.minWidth > 0.0f) {
        primitive.width = std::max(primitive.width, primitive.minWidth);
    }
    if (primitive.minHeight > 0.0f) {
        primitive.height = std::max(primitive.height, primitive.minHeight);
    }
    if (primitive.maxWidth > 0.0f) {
        primitive.width = std::min(primitive.width, primitive.maxWidth);
    }
    if (primitive.maxHeight > 0.0f) {
        primitive.height = std::min(primitive.height, primitive.maxHeight);
    }
    const RectFrame layoutBounds = node.layoutBounds();
    primitive.x = frame.x - layoutBounds.x;
    primitive.y = frame.y - layoutBounds.y;
    node.finishCompose();
}

void UIContext::resolveLayout(LayoutState& layout, const RectFrame& frame) {
    const float innerX = frame.x + layout.paddingLeft;
    const float innerY = frame.y + layout.paddingTop;
    const float innerWidth = ClampDimension(frame.width - layout.paddingLeft - layout.paddingRight);
    const float innerHeight = ClampDimension(frame.height - layout.paddingTop - layout.paddingBottom);
    const float availableMain = layout.direction == FlexDirection::Row ? innerWidth : innerHeight;
    const float availableCross = layout.direction == FlexDirection::Row ? innerHeight : innerWidth;
    const std::size_t childCount = layout.children.size();
    const float gapTotal = childCount > 1 ? layout.gap * static_cast<float>(childCount - 1) : 0.0f;

    float fixedMain = 0.0f;
    float totalFlex = 0.0f;
    float totalMargins = 0.0f;
    for (const LayoutItem& item : layout.children) {
        totalMargins += MainMarginBefore(item.build, layout.direction) + MainMarginAfter(item.build, layout.direction);
        if (item.build.flex > 0.0f) {
            totalFlex += item.build.flex;
        } else {
            fixedMain += preferredMainSize(item, layout.direction);
        }
    }

    const float flexSpace = std::max(0.0f, availableMain - fixedMain - totalMargins - gapTotal);
    float occupiedMain = totalMargins + gapTotal;
    for (const LayoutItem& item : layout.children) {
        occupiedMain += resolveMainSize(item, layout.direction, flexSpace, totalFlex);
    }
    float cursor = (layout.direction == FlexDirection::Row ? innerX : innerY)
        + ResolveMainOffset(layout.justifyContent, availableMain, occupiedMain);
    const float crossStart = layout.direction == FlexDirection::Row ? innerY : innerX;

    for (LayoutItem& item : layout.children) {
        const float mainMarginBefore = MainMarginBefore(item.build, layout.direction);
        const float mainMarginAfter = MainMarginAfter(item.build, layout.direction);
        const float crossMarginBefore = CrossMarginBefore(item.build, layout.direction);
        const float crossMarginAfter = CrossMarginAfter(item.build, layout.direction);
        const float mainSize = resolveMainSize(item, layout.direction, flexSpace, totalFlex);
        const float crossAvailable = std::max(0.0f, availableCross - crossMarginBefore - crossMarginAfter);
        const float crossSize = resolveCrossSize(
            item,
            layout.direction,
            crossAvailable
        );
        const float crossOffset = ResolveCrossOffset(layout.direction, layout.alignItems, crossAvailable, crossSize);
        const RectFrame childFrame = resolveItemFrame(
            item,
            layout.direction,
            cursor + mainMarginBefore,
            crossStart + crossMarginBefore + crossOffset,
            mainSize,
            crossSize
        );

        if (item.node != nullptr) {
            applyResolvedFrame(*item.node, childFrame);
        } else if (item.container != nullptr) {
            resolveLayout(*item.container, childFrame);
        }

        cursor += mainMarginBefore + mainSize + mainMarginAfter + layout.gap;
    }
}

std::uint64_t UIContext::computeDrawOrderSignature() const {
    std::uint64_t signature = 1469598103934665603ull;
    for (const UINode* node : order_) {
        HashCombine(signature, static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(node)));
        HashCombine(signature, static_cast<std::uint64_t>(LayerDrawPriority(node->renderLayer())));
        HashCombine(signature, static_cast<std::uint64_t>(node->zIndex()));
        HashCombine(signature, node->hasExplicitZIndex() ? 1ull : 0ull);
    }
    return signature;
}

void UIContext::ensureDrawOrder() {
    const std::uint64_t signature = computeDrawOrderSignature();
    if (!drawOrderInitialized_ || signature != drawOrderSignature_) {
        drawOrder_ = order_;
        std::stable_sort(drawOrder_.begin(), drawOrder_.end(), NodeDrawBefore);
        drawOrderSignature_ = signature;
        drawOrderInitialized_ = true;
    }
}

void UIContext::update() {
    ensureDrawOrder();

    State.inputBlockedByPopup = false;
    for (const UINode* node : drawOrder_) {
        if (node->visible() && node->blocksUnderlyingInput()) {
            State.inputBlockedByPopup = true;
            break;
        }
    }

    const bool hasInputActivity = HasFrameInputActivity();
    const bool pointerMoveOnly = IsPointerMoveOnlyActivity();
    std::unordered_set<UINode*> pointerHotNodeSet;
    if (pointerMoveOnly) {
        std::vector<UINode*> currentPointerHotNodes;
        currentPointerHotNodes.reserve(2);
        for (auto it = drawOrder_.rbegin(); it != drawOrder_.rend(); ++it) {
            UINode* node = *it;
            applyRuntimeContext(node);
            if (!node->visible()) {
                continue;
            }
            const UIPrimitive& primitive = node->primitive();
            if (!primitive.enabled) {
                continue;
            }
            if (State.inputBlockedByPopup && primitive.renderLayer != RenderLayer::Popup) {
                continue;
            }
            if (!PrimitiveContains(primitive, State.mouseX, State.mouseY)) {
                continue;
            }
            currentPointerHotNodes.push_back(node);
            if (State.inputPriorityByZ) {
                break;
            }
        }
        for (UINode* node : currentPointerHotNodes) {
            pointerHotNodeSet.insert(node);
        }
        for (UINode* node : pointerHotNodes_) {
            pointerHotNodeSet.insert(node);
        }
        pointerHotNodes_ = std::move(currentPointerHotNodes);
    } else {
        pointerHotNodes_.clear();
    }

    bool dirtyLayers[static_cast<std::size_t>(RenderLayer::Count)] = {};
    auto updateNode = [this, &dirtyLayers, hasInputActivity, pointerMoveOnly, &pointerHotNodeSet](UINode* node) {
        applyRuntimeContext(node);
        const bool isVisible = node->visible();
        const bool pointerHot = pointerMoveOnly &&
            pointerHotNodeSet.find(node) != pointerHotNodeSet.end();
        const bool shouldRunUpdate = pointerMoveOnly
            ? (pointerHot || node->wantsContinuousUpdate() || node->cacheDirty())
            : (hasInputActivity || node->wantsContinuousUpdate() || node->cacheDirty());
        if (isVisible && shouldRunUpdate) {
            node->update();
        } else {
            if (!isVisible) {
                node->clearCacheDirty();
            }
        }
        if (isVisible && node->cacheDirty() && node->primitive().blur <= 0.0f && node->usesCachedSurface()) {
            const RectFrame bounds = node->paintBounds();
            if (bounds.width <= 0.0f || bounds.height <= 0.0f) {
                Renderer::ReleaseCachedSurface(node->key());
                node->clearCacheDirty();
            }
        }
        if (isVisible && node->cacheDirty()) {
            const std::size_t layerIndex = static_cast<std::size_t>(node->renderLayer());
            if (layerIndex < static_cast<std::size_t>(RenderLayer::Count)) {
                dirtyLayers[layerIndex] = true;
            }
        }
        if (node->consumeRecomposeRequest()) {
            needsRecompose_ = true;
        }
    };

    if (State.inputPriorityByZ) {
        for (auto it = drawOrder_.rbegin(); it != drawOrder_.rend(); ++it) {
            updateNode(*it);
        }
    } else {
        for (UINode* node : order_) {
            updateNode(node);
        }
    }

    for (std::size_t index = 0; index < static_cast<std::size_t>(RenderLayer::Count); ++index) {
        if (dirtyLayers[index]) {
            Renderer::InvalidateLayer(static_cast<RenderLayer>(index));
        }
    }
}

void UIContext::render() {
    Renderer::CompositeLayers(CurrentTheme->background);
    Renderer::BeginFrame();
    draw();
}

void UIContext::draw() {
    ensureDrawOrder();

    for (UINode* node : drawOrder_) {
        applyRuntimeContext(node);
        if (!node->visible()) {
            continue;
        }
        if (node->primitive().blur <= 0.0f && node->usesCachedSurface()) {
            const RectFrame bounds = node->paintBounds();
            if (bounds.width > 0.0f && bounds.height > 0.0f) {
                Renderer::DrawCachedSurface(node->key(), bounds, node->cacheDirty(), [node]() {
                    node->draw();
                });
            } else {
                Renderer::ReleaseCachedSurface(node->key());
            }
            node->clearCacheDirty();
        } else {
            node->draw();
            node->clearCacheDirty();
        }
    }
}

bool UIContext::wantsContinuousUpdate() const {
    for (const UINode* node : order_) {
        if (node->visible() && node->wantsContinuousUpdate()) {
            return true;
        }
    }
    return false;
}

bool UIContext::consumeRecomposeRequest() {
    const bool requested = needsRecompose_;
    needsRecompose_ = false;
    return requested;
}

void UIContext::markAllNodesDirty() {
    for (auto& entry : nodes_) {
        if (entry.second) {
            entry.second->forceComposeDirty();
        }
    }
    Renderer::RequestRepaint();
}

void UIContext::requestVisualRefresh(float duration) {
    Renderer::RequestRepaint(duration);
}

void UIContext::requestThemeRefresh(float duration) {
    markAllNodesDirty();
    Renderer::InvalidateAll();
    Renderer::RequestRepaint(duration);
}

} // namespace EUINEO
