#pragma once

#include "components/theme.h"
#include "components/layout.h"
#include "core/dsl.h"
#include "eui/signal.h"

#include <algorithm>
#include <functional>
#include <initializer_list>
#include <string>
#include <utility>
#include <vector>
#include <type_traits>

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
        radius = tokens.metrics.radius.card;
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
    /** @brief 叶子项被点击或键盘激活时同步调用；随后发送选择回调并关闭菜单。 */
    std::function<void()> action;
    /** @brief 禁用项不可点击，也不参与键盘导航。 */
    bool enabled = true;
    /** @brief 分隔线不占普通行高，不可交互；通常通过 separatorItem 创建。 */
    bool separator = false;

    ContextMenuItem(std::string value) : text(std::move(value)) {}
    ContextMenuItem(std::string value, std::vector<ContextMenuItem> childItems)
        : text(std::move(value)), children(std::move(childItems)) {}
    template <typename Callback, std::enable_if_t<std::is_invocable_r_v<void, Callback&>, int> = 0>
    ContextMenuItem(std::string value, Callback callback)
        : text(std::move(value)), action(std::move(callback)) {}
    static ContextMenuItem separatorItem() { ContextMenuItem item(""); item.separator = true; return item; }

    bool hasChildren() const { return !children.empty(); }
};

namespace context_menu_detail {

inline float menuHeight(std::size_t itemCount, float itemHeight, float inset) {
    return itemHeight * static_cast<float>(itemCount) + inset * 2.0f;
}

inline float itemOffset(const std::vector<ContextMenuItem>& items, std::size_t index, float itemHeight, float inset) {
    float result = 0;
    for (std::size_t i = 0; i < std::min(index, items.size()); ++i) {
        result += items[i].separator ? std::max(4.0f, inset) : itemHeight;
    }
    return result;
}

inline float menuHeight(const std::vector<ContextMenuItem>& items, float itemHeight, float inset) {
    return itemOffset(items, items.size(), itemHeight, inset) + inset * 2;
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
    /** @brief 外部菜单栏保留可交互区域；区域之外的点击关闭菜单。坐标相对当前父容器。 */
    ContextMenuBuilder& dismissExclusion(const core::Rect& rect) { dismissExclusion_ = rect; hasExclusion_ = true; return *this; }
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
    ContextMenuBuilder& theme(const theme::ThemeColorTokens& tokens) {
        style_ = ContextMenuStyle(tokens);
        metrics_ = tokens.metrics;
        return *this;
    }
    ContextMenuBuilder& transition(const core::Transition& value) { transition_ = value; return *this; }
    ContextMenuBuilder& zIndex(int value) { zIndex_ = value; return *this; }
    ContextMenuBuilder& onSelect(std::function<void(int)> callback) { onSelect_ = std::move(callback); return *this; }
    ContextMenuBuilder& onSelectPath(std::function<void(const std::vector<int>&)> callback) { onSelectPath_ = std::move(callback); return *this; }
    ContextMenuBuilder& onOpenChange(std::function<void(bool)> callback) { onOpenChange_ = std::move(callback); return *this; }
    ContextMenuBuilder& onNavigate(std::function<bool(const core::KeyEvent&)> callback) { onNavigate_ = std::move(callback); return *this; }

    void build() {
        if (items_.empty()) {
            return;
        }

        const float inset = metrics_.spacing.small;
        const float itemHeight = resolvedItemHeight();
        const float width = std::min(width_, std::max(0.0f, screenWidth_ - metrics_.spacing.section));
        const float xInset = metrics_.spacing.compact;
        const float height = context_menu_detail::menuHeight(items_, itemHeight, inset);
        const float x = std::clamp(x_, xInset, std::max(xInset, screenWidth_ - width - xInset));
        const float y = context_menu_detail::clampMenuY(y_, height, screenHeight_);
        const std::function<void()> requestDismiss = dismissCallback();
        const std::function<void(int)> onSelect = onSelect_;
        const std::function<void(const std::vector<int>&)> onSelectPath = onSelectPath_;
        CascadeState* cascade = &ui_.state<CascadeState>(id_ + ".cascade");
        if (!open_) {
            cascade->openPath.clear();
        }

        ui_.stack(id_)
            .accessibleHidden(!open_).focusScope(open_).onDismiss(requestDismiss)
            .size(screenWidth_, screenHeight_)
            .zIndex(zIndex_)
            .content([&] {
                if (open_) {
                    const auto dismiss = [&](const std::string& suffix, float x, float y, float w, float h) {
                        if (w <= 0 || h <= 0) return;
                        ui_.rect(id_ + ".dismiss" + suffix).position(x, y)
                        .size(w, h)
                        .states(theme::color(0.0f, 0.0f, 0.0f, 0.0f),
                                theme::color(0.0f, 0.0f, 0.0f, 0.0f),
                                theme::color(0.0f, 0.0f, 0.0f, 0.0f))
                        .onClick(requestDismiss).tabStop(false).preserveFocusOnPress()
                        .onScroll([](const core::ScrollEvent&) {})
                        .build();
                    };
                    if (!hasExclusion_) dismiss("", 0, 0, screenWidth_, screenHeight_);
                    else {
                        const float left = std::clamp(dismissExclusion_.x, 0.0f, screenWidth_);
                        const float top = std::clamp(dismissExclusion_.y, 0.0f, screenHeight_);
                        const float right = std::clamp(dismissExclusion_.x + dismissExclusion_.width, left, screenWidth_);
                        const float bottom = std::clamp(dismissExclusion_.y + dismissExclusion_.height, top, screenHeight_);
                        dismiss(".top", 0, 0, screenWidth_, top);
                        dismiss(".bottom", 0, bottom, screenWidth_, screenHeight_ - bottom);
                        dismiss(".left", 0, top, left, bottom - top);
                        dismiss(".right", right, top, screenWidth_ - right, bottom - top);
                    }
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

    float resolvedItemHeight() const {
        return itemHeight_ > 0.0f ? itemHeight_ : metrics_.control.segmented;
    }

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
        const float itemHeight = resolvedItemHeight();
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
                const float desiredY = menuY + context_menu_detail::itemOffset(*levelItems, static_cast<std::size_t>(parentIndex), itemHeight, inset);
                levelItems = &(*levelItems)[parentIndex].children;
                menuX = context_menu_detail::childMenuX(menuX, width, width, screenWidth_, metrics_.spacing.tiny);
                menuY = context_menu_detail::clampMenuY(
                    desiredY, context_menu_detail::menuHeight(*levelItems, itemHeight, inset), screenHeight_);
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
        const float itemHeight = resolvedItemHeight();
        const float height = context_menu_detail::menuHeight(levelItems, itemHeight, inset);
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
                    .radius(style_.radius).border(metrics_.spacing.hairline, style_.border).shadow(style_.shadow).build();
                ui_.rect(levelId + ".hit").size(width, height)
                    .states(theme::color(0, 0, 0, 0), theme::color(0, 0, 0, 0), theme::color(0, 0, 0, 0))
                    .disabled(!visible).blockPointer().build();

                float itemY = inset;
                for (int index = 0; index < static_cast<int>(levelItems.size()); ++index) {
                    const ContextMenuItem& item = levelItems[index];
                    std::vector<int> path = prefix;
                    path.push_back(index);
                    int firstChild = -1;
                    for (int child = 0; child < static_cast<int>(item.children.size()); ++child) {
                        if (item.children[child].enabled && !item.children[child].separator) { firstChild = child; break; }
                    }
                    const std::string childFocus = firstChild < 0 ? std::string{} :
                        id_ + ".level." + std::to_string(depth + 1) + ".item." + std::to_string(firstChild);
                    const std::string parentFocus = prefix.empty() ? std::string{} :
                        id_ + ".level." + std::to_string(depth - 1) + ".item." + std::to_string(prefix.back());
                    ui_.rect(levelId + ".item." + std::to_string(index))
                        .accessible(core::AccessibleRole::ListItem, item.text).accessibleExpanded(item.hasChildren())
                        .focusGroup(levelId).activateOnFocus(false)
                        .x(inset).y(itemY).size(std::max(0.0f, width - inset * 2.0f), itemHeight)
                        .states(theme::color(0, 0, 0, 0), style_.hover, style_.pressed)
                        .radius(std::max(metrics_.radius.tiny, style_.radius - metrics_.radius.tiny)).instantStates()
                        .disabled(!visible || !item.enabled || item.separator)
                        .onKeyEvent([navigate = onNavigate_, cascadePtr = &cascade, path, depth, ui = &ui_, childFocus, parentFocus,
                                     hasChildren = item.hasChildren()](const core::KeyEvent& key) {
                            if (!key.isDown()) return false;
                            if (key.key == core::InputKey::Right && hasChildren) {
                                cascadePtr->openPath = path; cascadePtr->renderedPath = path;
                                if (!childFocus.empty()) ui->requestFocus(childFocus);
                                return true;
                            }
                            if (key.key == core::InputKey::Left && depth > 0) {
                                cascadePtr->openPath.resize(static_cast<std::size_t>(depth - 1));
                                ui->requestFocus(parentFocus); return true;
                            }
                            return navigate ? navigate(key) : false;
                        })
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
                                  hasChildren = item.hasChildren(), action = item.action, cascadePtr = &cascade, ui = &ui_, childFocus] {
                            if (hasChildren) {
                                cascadePtr->openPath = path;
                                cascadePtr->renderedPath = path;
                                if (!childFocus.empty()) ui->requestFocus(childFocus);
                                return;
                            }
                            if (action) action();
                            if (onSelectPath) onSelectPath(path);
                            if (onSelect) onSelect(index);
                            requestDismiss();
                        }).build();
                    if (item.separator) {
                        ui_.rect(levelId + ".separator." + std::to_string(index))
                            .position(inset, itemY + std::max(4.0f, inset) * .5f).size(std::max(0.0f, width - inset * 2), 1)
                            .color(style_.border).build();
                    }
                    ui_.text(levelId + ".label." + std::to_string(index))
                        .x(inset + metrics_.spacing.content)
                        .y(itemY)
                        .size(std::max(0.0f, width - inset * 2.0f -
                                      (item.hasChildren() ? metrics_.control.control : metrics_.spacing.panel)),
                              itemHeight)
                        .text(item.text)
                        .fontSize(metrics_.typography.option)
                        .lineHeight(metrics_.typography.option + metrics_.typography.lineGapTight)
                        .verticalAlign(core::VerticalAlign::Center)
                        .color(item.enabled ? style_.text : style_.mutedText).build();
                    if (item.hasChildren()) {
                        ui_.text(levelId + ".arrow." + std::to_string(index))
                            .x(width - inset - metrics_.control.compact + metrics_.spacing.micro)
                            .y(itemY).size(metrics_.typography.control, itemHeight)
                            .icon(0xF054)
                            .fontSize(metrics_.typography.micro)
                            .lineHeight(metrics_.typography.micro + metrics_.typography.lineGapTight)
                            .color(style_.mutedText)
                            .horizontalAlign(core::HorizontalAlign::Center)
                            .verticalAlign(core::VerticalAlign::Center)
                            .build();
                    }
                    itemY += item.separator ? std::max(4.0f, inset) : itemHeight;
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
    theme::ThemeMetricTokens metrics_;
    core::Transition transition_ = core::Transition::make(0.12f, core::Ease::OutCubic);
    std::function<void(int)> onSelect_;
    std::function<void(const std::vector<int>&)> onSelectPath_;
    std::function<void(bool)> onOpenChange_;
    std::function<bool(const core::KeyEvent&)> onNavigate_;
    core::Rect dismissExclusion_{};
    bool hasExclusion_ = false;
    bool open_ = false;
    float screenWidth_ = 800.0f;
    float screenHeight_ = 600.0f;
    float x_ = 0.0f;
    float y_ = 0.0f;
    float width_ = 190.0f;
    float itemHeight_ = 0.0f;
    int zIndex_ = layer::Menu;
};

inline ContextMenuBuilder contextMenu(core::dsl::Ui& ui, const std::string& id) {
    return ContextMenuBuilder(ui, id);
}

} // namespace components
