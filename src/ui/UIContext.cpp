#include "UIContext.h"
#include <algorithm>

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

RectFrame UnionFrame(const RectFrame& lhs, const RectFrame& rhs) {
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

} // namespace

void UIContext::begin(const std::string& pageId) {
    pageId_ = pageId;
    ++composeStamp_;
    order_.clear();
    drawOrder_.clear();
    drawOrderStamp_ = 0;
    clipStack_.clear();
    treeChanged_ = false;
    layerBoundsStamp_ = 0;
    if (layerBounds_.size() != static_cast<std::size_t>(RenderLayer::Count)) {
        layerBounds_.assign(static_cast<std::size_t>(RenderLayer::Count), RectFrame{});
    }
}

void UIContext::end() {
    bool backdropDirty = false;
    for (auto& entry : nodes_) {
        UINode* node = entry.second.get();
        if (node == nullptr) {
            continue;
        }
        if (!node->composedIn(composeStamp_)) {
            treeChanged_ = true;
            if (node->renderLayer() == RenderLayer::Backdrop) {
                backdropDirty = true;
            }
        }
    }

    bool anyDirty = treeChanged_;
    for (UINode* node : order_) {
        if (!node->cacheDirty()) {
            continue;
        }
        anyDirty = true;
        if (node->renderLayer() == RenderLayer::Backdrop) {
            backdropDirty = true;
        }
    }

    if (backdropDirty) {
        Renderer::InvalidateLayer(RenderLayer::Backdrop);
    } else if (anyDirty) {
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

void UIContext::update() {
    for (UINode* node : order_) {
        if (node->visible()) {
            node->update();
        }
    }
    refreshLayerBounds();
}

void UIContext::draw() {
    if (drawOrderStamp_ != composeStamp_) {
        drawOrder_ = order_;
        std::stable_sort(drawOrder_.begin(), drawOrder_.end(), [](const UINode* lhs, const UINode* rhs) {
            return lhs->zIndex() < rhs->zIndex();
        });
        drawOrderStamp_ = composeStamp_;
    }

    for (UINode* node : drawOrder_) {
        if (!node->visible() || node->renderLayer() == RenderLayer::Backdrop) {
            continue;
        }
        const RectFrame bounds = node->paintBounds();
        const bool dirty = node->cacheDirty();
        Renderer::DrawCachedSurface(node->key(), bounds, dirty, [node]() {
            node->draw();
        });
        node->clearCacheDirty();
    }
}

void UIContext::draw(RenderLayer layer) {
    if (drawOrderStamp_ != composeStamp_) {
        drawOrder_ = order_;
        std::stable_sort(drawOrder_.begin(), drawOrder_.end(), [](const UINode* lhs, const UINode* rhs) {
            return lhs->zIndex() < rhs->zIndex();
        });
        drawOrderStamp_ = composeStamp_;
    }

    for (UINode* node : drawOrder_) {
        if (node->visible() && node->renderLayer() == layer) {
            node->draw();
        }
    }
}

RectFrame UIContext::layerBounds(RenderLayer layer) const {
    const std::size_t index = static_cast<std::size_t>(layer);
    if (index >= layerBounds_.size()) {
        return RectFrame{};
    }
    return layerBounds_[index];
}

bool UIContext::wantsContinuousUpdate() const {
    for (const UINode* node : order_) {
        if (node->visible() && node->wantsContinuousUpdate()) {
            return true;
        }
    }
    return false;
}

void UIContext::markAllNodesDirty() {
    for (auto& entry : nodes_) {
        if (entry.second) {
            entry.second->forceComposeDirty();
        }
    }
    Renderer::RequestRepaint();
}

void UIContext::refreshLayerBounds() {
    if (layerBounds_.size() != static_cast<std::size_t>(RenderLayer::Count)) {
        layerBounds_.assign(static_cast<std::size_t>(RenderLayer::Count), RectFrame{});
    } else {
        std::fill(layerBounds_.begin(), layerBounds_.end(), RectFrame{});
    }

    for (UINode* node : order_) {
        if (!node->visible()) {
            continue;
        }
        const std::size_t index = static_cast<std::size_t>(node->renderLayer());
        if (index >= layerBounds_.size()) {
            continue;
        }
        layerBounds_[index] = UnionFrame(layerBounds_[index], node->paintBounds());
    }

    layerBoundsStamp_ = composeStamp_;
}

} // namespace EUINEO
