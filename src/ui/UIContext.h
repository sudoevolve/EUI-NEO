#pragma once

#include "UIBuilder.h"
#include "../components/Button.h"
#include "../components/ComboBox.h"
#include "../components/InputBox.h"
#include "../components/Label.h"
#include "../components/Panel.h"
#include "../components/ProgressBar.h"
#include "../components/SegmentedControl.h"
#include "../components/Sidebar.h"
#include "../components/Slider.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace EUINEO {

class UIContext {
public:
    void begin(const std::string& pageId);
    void end();
    void update();
    void draw();
    void draw(RenderLayer layer);
    RectFrame layerBounds(RenderLayer layer) const;
    bool wantsContinuousUpdate() const;
    void markAllNodesDirty();
    void pushClip(float x, float y, float width, float height);
    void popClip();

    template <typename NodeT>
    GenericNodeBuilder<NodeT> node(const std::string& id) {
        NodeT& instance = acquire<NodeT>(id);
        return GenericNodeBuilder<NodeT>(*this, instance);
    }

    template <typename NodeT>
    typename NodeT::Builder component(const std::string& id) {
        NodeT& node = acquire<NodeT>(id);
        return typename NodeT::Builder(*this, node);
    }

#define EUI_UI_COMPONENT(name, type) \
    typename type::Builder name(const std::string& id) { \
        return component<type>(id); \
    }
#include "UIComponents.def"
#undef EUI_UI_COMPONENT

private:
    template <typename NodeT>
    NodeT& acquire(const std::string& id) {
        const std::string fullKey = pageId_.empty() ? id : pageId_ + "." + id;
        auto it = nodes_.find(fullKey);
        if (it == nodes_.end() || std::string(it->second->typeName()) != NodeT::StaticTypeName()) {
            treeChanged_ = true;
            auto replacement = std::make_unique<NodeT>(fullKey);
            UINode* raw = replacement.get();
            it = nodes_.insert_or_assign(fullKey, std::move(replacement)).first;
            raw->beginCompose(composeStamp_);
            applyCurrentClip(raw->primitive());
            order_.push_back(raw);
            return static_cast<NodeT&>(*raw);
        }

        UINode* node = it->second.get();
        if (!node->composedIn(composeStamp_)) {
            node->beginCompose(composeStamp_);
            order_.push_back(node);
        }
        applyCurrentClip(node->primitive());
        return static_cast<NodeT&>(*node);
    }

    void applyCurrentClip(UIPrimitive& primitive) {
        if (clipStack_.empty()) {
            primitive.hasClipRect = false;
            primitive.clipRect = UIClipRect{};
            return;
        }
        primitive.hasClipRect = true;
        primitive.clipRect = clipStack_.back();
    }

    void refreshLayerBounds();

    std::string pageId_;
    std::uint64_t composeStamp_ = 0;
    std::unordered_map<std::string, std::unique_ptr<UINode>> nodes_;
    std::vector<UINode*> order_;
    std::vector<UINode*> drawOrder_;
    std::vector<UIClipRect> clipStack_;
    mutable std::vector<RectFrame> layerBounds_;
    bool treeChanged_ = false;
    std::uint64_t drawOrderStamp_ = 0;
    std::uint64_t layerBoundsStamp_ = 0;
};

} // namespace EUINEO
