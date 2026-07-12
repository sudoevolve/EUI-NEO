#pragma once

#include "components/theme.h"
#include "core/dsl.h"
#include "eui/signal.h"

#include <algorithm>
#include <functional>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>

namespace components {

struct ContextMenuStyle {
    ContextMenuStyle() : ContextMenuStyle(theme::dark()) {}

    explicit ContextMenuStyle(const theme::ThemeColorTokens& tokens) {
        background = tokens.dark
            ? core::mixColor(tokens.surface, theme::color(0.0f, 0.0f, 0.0f), 0.16f)
            : tokens.surface;
        hover = tokens.surfaceHover;
        pressed = tokens.surfaceActive;
        text = tokens.text;
        mutedText = theme::withOpacity(tokens.text, 0.54f);
        border = theme::withOpacity(tokens.border, 0.82f);
        shadow = theme::popupShadow(tokens);
    }

    core::Color background;
    core::Color hover;
    core::Color pressed;
    core::Color text;
    core::Color mutedText;
    core::Color border;
    core::Shadow shadow;
    float radius = 12.0f;
};

struct ContextMenuItem {
    std::string text;
    std::vector<ContextMenuItem> children;

    ContextMenuItem(std::string value) : text(std::move(value)) {}
    ContextMenuItem(std::string value, std::vector<ContextMenuItem> childItems)
        : text(std::move(value)), children(std::move(childItems)) {}

    bool hasChildren() const { return !children.empty(); }
};

namespace context_menu_detail {

inline float menuHeight(std::size_t itemCount, float itemHeight, float inset) {
    return itemHeight * static_cast<float>(itemCount) + inset * 2.0f;
}

inline float clampMenuY(float desiredY, float height, float screenHeight) {
    return std::clamp(desiredY, 8.0f, std::max(8.0f, screenHeight - height - 8.0f));
}

inline float childMenuX(float parentX, float parentWidth, float childWidth, float screenWidth, float gap) {
    const float right = parentX + parentWidth + gap;
    if (right + childWidth + 8.0f <= screenWidth) {
        return right;
    }
    return std::max(8.0f, parentX - childWidth - gap);
}

} // namespace context_menu_detail

class ContextMenuBuilder {
public:
    ContextMenuBuilder(core::dsl::Ui& ui, std::string id)
        : ui_(ui), id_(std::move(id)) {}

    ContextMenuBuilder& open(bool value = true) { open_ = value; return *this; }
    ContextMenuBuilder& bindOpen(eui::Signal<bool>& signal) {
        open(signal.get());
        onOpenChange([&signal](bool value) { signal.set(value); });
        return *this;
    }
    ContextMenuBuilder& screen(float width, float height) { screenWidth_ = width; screenHeight_ = height; return *this; }
    ContextMenuBuilder& position(float x, float y) { x_ = x; y_ = y; return *this; }
    ContextMenuBuilder& size(float width, float itemHeight) { width_ = width; itemHeight_ = itemHeight; return *this; }
    ContextMenuBuilder& items(std::vector<std::string> value) {
        items_.clear();
        items_.reserve(value.size());
        for (std::string& text : value) {
            items_.emplace_back(std::move(text));
        }
        return *this;
    }
    ContextMenuBuilder& items(std::initializer_list<std::string> value) {
        return items(std::vector<std::string>(value));
    }
    ContextMenuBuilder& items(std::vector<ContextMenuItem> value) { items_ = std::move(value); return *this; }
    ContextMenuBuilder& style(const ContextMenuStyle& value) { style_ = value; return *this; }
    ContextMenuBuilder& theme(const theme::ThemeColorTokens& tokens) { style_ = ContextMenuStyle(tokens); return *this; }
    ContextMenuBuilder& transition(const core::Transition& value) { transition_ = value; return *this; }
    ContextMenuBuilder& zIndex(int value) { zIndex_ = value; return *this; }
    ContextMenuBuilder& onSelect(std::function<void(int)> callback) { onSelect_ = std::move(callback); return *this; }
    ContextMenuBuilder& onSelectPath(std::function<void(const std::vector<int>&)> callback) { onSelectPath_ = std::move(callback); return *this; }
    ContextMenuBuilder& onOpenChange(std::function<void(bool)> callback) { onOpenChange_ = std::move(callback); return *this; }

    void build() {
        if (items_.empty()) {
            return;
        }

        const float inset = 6.0f;
        const float width = std::min(width_, std::max(0.0f, screenWidth_ - 16.0f));
        const float height = context_menu_detail::menuHeight(items_.size(), itemHeight_, inset);
        const float x = std::clamp(x_, 8.0f, std::max(8.0f, screenWidth_ - width - 8.0f));
        const float y = context_menu_detail::clampMenuY(y_, height, screenHeight_);
        const std::function<void()> requestDismiss = dismissCallback();
        const std::function<void(int)> onSelect = onSelect_;
        const std::function<void(const std::vector<int>&)> onSelectPath = onSelectPath_;
        CascadeState* cascade = &ui_.state<CascadeState>(id_ + ".cascade");
        if (!open_) {
            cascade->openPath.clear();
        }

        ui_.stack(id_)
            .size(screenWidth_, screenHeight_)
            .zIndex(zIndex_)
            .content([&] {
                if (open_) {
                    ui_.rect(id_ + ".dismiss")
                        .size(screenWidth_, screenHeight_)
                        .states(theme::color(0.0f, 0.0f, 0.0f, 0.0f),
                                theme::color(0.0f, 0.0f, 0.0f, 0.0f),
                                theme::color(0.0f, 0.0f, 0.0f, 0.0f))
                        .onClick(requestDismiss)
                        .onScroll([](const core::ScrollEvent&) {})
                        .build();
                }

                ui_.stack(id_ + ".menu")
                    .size(screenWidth_, screenHeight_)
                    .content([&] {
                        renderMenus(x, y, width, inset, *cascade, onSelect, onSelectPath, requestDismiss);
                    })
                    .build();
            })
            .build();
    }

private:
    struct CascadeState {
        std::vector<int> openPath;
        std::vector<int> renderedPath;
    };

    static int maximumDepth(const std::vector<ContextMenuItem>& items) {
        int depth = 1;
        for (const ContextMenuItem& item : items) {
            if (item.hasChildren()) {
                depth = std::max(depth, 1 + maximumDepth(item.children));
            }
        }
        return depth;
    }

    void renderMenus(float rootX, float rootY, float width, float inset, CascadeState& cascade,
                     const std::function<void(int)>& onSelect,
                     const std::function<void(const std::vector<int>&)>& onSelectPath,
                     const std::function<void()>& requestDismiss) {
        const std::vector<ContextMenuItem>* levelItems = &items_;
        std::vector<int> prefix;
        float menuX = rootX;
        float menuY = rootY;
        const int depthCount = maximumDepth(items_);
        int renderedLevels = 0;

        for (int depth = 0; depth < depthCount; ++depth) {
            if (depth > 0) {
                if (depth > static_cast<int>(cascade.renderedPath.size())) {
                    break;
                }
                const int parentIndex = cascade.renderedPath[depth - 1];
                if (parentIndex < 0 || parentIndex >= static_cast<int>(levelItems->size()) ||
                    !(*levelItems)[parentIndex].hasChildren()) {
                    break;
                }
                prefix.push_back(parentIndex);
                const float desiredY = menuY + static_cast<float>(parentIndex) * itemHeight_;
                levelItems = &(*levelItems)[parentIndex].children;
                menuX = context_menu_detail::childMenuX(menuX, width, width, screenWidth_, 4.0f);
                menuY = context_menu_detail::clampMenuY(
                    desiredY, context_menu_detail::menuHeight(levelItems->size(), itemHeight_, inset), screenHeight_);
            }

            const bool visible = open_ && (depth == 0 || depth <= static_cast<int>(cascade.openPath.size()));
            renderLevel(*levelItems, prefix, depth, menuX, menuY, width, inset, visible, cascade,
                        onSelect, onSelectPath, requestDismiss);
            renderedLevels = depth + 1;
        }

        for (int depth = renderedLevels; depth < depthCount; ++depth) {
            renderHiddenLevel(depth, width);
        }
    }

    void renderLevel(const std::vector<ContextMenuItem>& levelItems, const std::vector<int>& prefix,
                     int depth, float x, float y, float width, float inset, bool visible, CascadeState& cascade,
                     const std::function<void(int)>& onSelect,
                     const std::function<void(const std::vector<int>&)>& onSelectPath,
                     const std::function<void()>& requestDismiss) {
        const std::string levelId = id_ + ".level." + std::to_string(depth);
        const float height = context_menu_detail::menuHeight(levelItems.size(), itemHeight_, inset);
        const float opacity = visible ? 1.0f : 0.0f;
        const float scale = visible ? 1.0f : 0.94f;
        const float offsetY = visible ? 0.0f : -4.0f;

        ui_.stack(levelId)
            .x(x).y(y).size(width, height)
            .opacity(opacity)
            .translateY(offsetY)
            .scale(scale)
            .transformOrigin(0.0f, 0.0f)
            .transition(transition_)
            .animate(core::AnimProperty::Opacity | core::AnimProperty::Transform)
            .content([&] {
                ui_.rect(levelId + ".bg").size(width, height).color(style_.background)
                    .radius(style_.radius).border(1.0f, style_.border).shadow(style_.shadow).build();
                ui_.rect(levelId + ".hit").size(width, height)
                    .states(theme::color(0, 0, 0, 0), theme::color(0, 0, 0, 0), theme::color(0, 0, 0, 0))
                    .disabled(!visible).onClick([] {}).build();

                for (int index = 0; index < static_cast<int>(levelItems.size()); ++index) {
                    const ContextMenuItem& item = levelItems[index];
                    const float itemY = inset + static_cast<float>(index) * itemHeight_;
                    std::vector<int> path = prefix;
                    path.push_back(index);
                    ui_.rect(levelId + ".item." + std::to_string(index))
                        .x(inset).y(itemY).size(std::max(0.0f, width - inset * 2.0f), itemHeight_)
                        .states(theme::color(0, 0, 0, 0), style_.hover, style_.pressed)
                        .radius(std::max(4.0f, style_.radius - 4.0f)).instantStates()
                        .disabled(!visible)
                        .onHover([cascadePtr = &cascade, path, hasChildren = item.hasChildren()](bool hovered) {
                            if (!hovered) return;
                            cascadePtr->openPath = path;
                            if (hasChildren) {
                                cascadePtr->renderedPath = path;
                            } else {
                                cascadePtr->openPath.pop_back();
                            }
                        })
                        .onClick([onSelect, onSelectPath, requestDismiss, path, index,
                                  hasChildren = item.hasChildren(), cascadePtr = &cascade] {
                            if (hasChildren) {
                                cascadePtr->openPath = path;
                                cascadePtr->renderedPath = path;
                                return;
                            }
                            if (onSelectPath) onSelectPath(path);
                            if (onSelect) onSelect(index);
                            requestDismiss();
                        }).build();
                    ui_.text(levelId + ".label." + std::to_string(index))
                        .x(inset + 12.0f).y(itemY + std::max(0.0f, (itemHeight_ - 18.0f) * 0.5f))
                        .size(std::max(0.0f, width - inset * 2.0f - (item.hasChildren() ? 42.0f : 24.0f)), 20.0f)
                        .text(item.text).fontSize(15.0f).lineHeight(18.0f).color(style_.text).build();
                    if (item.hasChildren()) {
                        ui_.text(levelId + ".arrow." + std::to_string(index))
                            .x(width - inset - 26.0f).y(itemY).size(18.0f, itemHeight_)
                            .icon(0xF054).fontSize(11.0f).lineHeight(14.0f).color(style_.mutedText)
                            .horizontalAlign(core::HorizontalAlign::Center)
                            .verticalAlign(core::VerticalAlign::Center)
                            .build();
                    }
                }
            })
            .build();
    }

    void renderHiddenLevel(int depth, float width) {
        ui_.stack(id_ + ".level." + std::to_string(depth))
            .size(width, 0.0f)
            .opacity(0.0f)
            .translateY(-4.0f)
            .scale(0.94f)
            .transformOrigin(0.0f, 0.0f)
            .transition(transition_)
            .animate(core::AnimProperty::Opacity | core::AnimProperty::Transform)
            .build();
    }

    std::function<void()> dismissCallback() const {
        const std::function<void(bool)> onOpenChange = onOpenChange_;
        return [onOpenChange] {
            if (onOpenChange) {
                onOpenChange(false);
            }
        };
    }

    core::dsl::Ui& ui_;
    std::string id_;
    std::vector<ContextMenuItem> items_;
    ContextMenuStyle style_;
    core::Transition transition_ = core::Transition::make(0.12f, core::Ease::OutCubic);
    std::function<void(int)> onSelect_;
    std::function<void(const std::vector<int>&)> onSelectPath_;
    std::function<void(bool)> onOpenChange_;
    bool open_ = false;
    float screenWidth_ = 800.0f;
    float screenHeight_ = 600.0f;
    float x_ = 0.0f;
    float y_ = 0.0f;
    float width_ = 190.0f;
    float itemHeight_ = 36.0f;
    int zIndex_ = 1050;
};

inline ContextMenuBuilder contextMenu(core::dsl::Ui& ui, const std::string& id) {
    return ContextMenuBuilder(ui, id);
}

} // namespace components
