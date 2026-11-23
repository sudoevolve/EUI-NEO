#pragma once

#include "components/contextmenu.h"
#include "components/layout.h"
#include "core/render/text.h"

namespace components {

/** @brief 顶层菜单，子项复用 ContextMenuItem，支持级联菜单、禁用项、分隔线和动作。 */
struct MenuBarMenu {
    std::string text;
    std::vector<ContextMenuItem> items;
};

/** @brief 客户区菜单栏；screen 使用 compose 的逻辑尺寸，默认放在窗口左上角。 */
class MenuBarBuilder : public ComponentLayout<MenuBarBuilder> {
public:
    MenuBarBuilder(core::dsl::Ui& ui, std::string id) : ui_(ui), id_(std::move(id)) {}
    MenuBarBuilder& screen(float w, float h) { screenWidth_ = std::max(0.0f, w); screenHeight_ = std::max(0.0f, h); return *this; }
    MenuBarBuilder& position(float x, float y) { x_ = x; y_ = y; return *this; }
    MenuBarBuilder& x(float value) { x_ = value; return *this; }
    MenuBarBuilder& y(float value) { y_ = value; return *this; }
    MenuBarBuilder& height(float value) { height_ = std::max(1.0f, value); return *this; }
    MenuBarBuilder& menuWidth(float value) { menuWidth_ = std::max(1.0f, value); return *this; }
    MenuBarBuilder& menus(std::vector<MenuBarMenu> value) { menus_ = std::move(value); return *this; }
    MenuBarBuilder& theme(const theme::ThemeColorTokens& value) { tokens_ = value; return *this; }
    MenuBarBuilder& onSelect(std::function<void(int, const std::vector<int>&)> value) { onSelect_ = std::move(value); return *this; }
    void build() {
        auto& state = ui_.state<State>(id_ + ".menuState");
        if (state.open >= static_cast<int>(menus_.size())) state.open = -1;
        const float width = std::max(0.0f, screenWidth_ - x_);
        const float h = height_ > 0 ? height_ : tokens_.metrics.control.control;
        const int count = static_cast<int>(menus_.size());
        std::vector<bool> enabled;
        for (const auto& menu : menus_) enabled.push_back(!menu.items.empty());
        if (state.open >= 0 && !enabled[static_cast<std::size_t>(state.open)]) state.open = -1;
        const auto navigate = [&state, count, enabled](const core::KeyEvent& key) {
            if (!key.isDown() || count == 0) return false;
            if (key.key == core::InputKey::Escape) { state.open = -1; return true; }
            if (key.key != core::InputKey::Left && key.key != core::InputKey::Right) return false;
            for (int step = 0; step < count; ++step) {
                state.open = (state.open + (key.key == core::InputKey::Left ? count - 1 : 1)) % count;
                if (enabled[static_cast<std::size_t>(state.open)]) return true;
            }
            state.open = -1; return true;
        };
        auto root = ui_.stack(id_).position(x_, y_).size(width, h)
            .zIndex(state.open >= 0 ? layer::Menu : layer::Content).stackingContext();
        root = layoutRoot(root);
        root.content([&] {
            ui_.rect(id_ + ".bg").size(width, h).color(tokens_.surface).build();
            float x = 0;
            for (int i = 0; i < count; ++i) {
                const auto& menu = menus_[static_cast<std::size_t>(i)];
                const float w = std::min(std::max(0.0f, width - x),
                    core::TextPrimitive::measureTextWidth(menu.text, {}, tokens_.metrics.typography.body, 400) + tokens_.metrics.spacing.panel * 2);
                const auto header = id_ + ".header." + std::to_string(i);
                ui_.rect(header).x(x).size(w, h).accessible(core::AccessibleRole::Button, menu.text)
                    .accessibleExpanded(state.open == i).focusGroup(id_ + ".headers").activateOnFocus(false)
                    .disabled(menu.items.empty())
                    .states(state.open == i ? tokens_.surfaceActive : tokens_.surface, tokens_.surfaceHover, tokens_.surfaceActive)
                    .onClick([&state, i] { state.open = state.open == i ? -1 : i; })
                    .onHover([&state, i](bool hovered) { if (hovered && state.open >= 0) state.open = i; })
                    .onKeyEvent([&state, i, navigate](const core::KeyEvent& key) {
                        if (!key.isDown()) return false;
                        if (key.key == core::InputKey::Down || key.key == core::InputKey::Enter || key.key == core::InputKey::Space) {
                            state.open = i; return true;
                        }
                        return state.open >= 0 ? navigate(key) : false;
                    }).build();
                ui_.text(header + ".label").x(x).size(w, h).text(menu.text).fontSize(tokens_.metrics.typography.body)
                    .color(tokens_.text).horizontalAlign(core::HorizontalAlign::Center).verticalAlign(core::VerticalAlign::Center).build();
                {
                    contextMenu(ui_, id_ + ".popup." + std::to_string(i)).screen(width, std::max(0.0f, screenHeight_ - y_))
                        .position(x, h).size(menuWidth_, tokens_.metrics.control.segmented).items(menu.items)
                        .theme(tokens_).open(state.open == i).dismissExclusion({0, 0, width, h})
                        .onOpenChange([&state](bool open) { if (!open) state.open = -1; })
                        .onNavigate(navigate).onSelectPath([onSelect = onSelect_, i](const auto& path) { if (onSelect) onSelect(i, path); }).build();
                }
                x += w;
            }
        }).build();
    }
private:
    struct State { int open = -1; };
    core::dsl::Ui& ui_;
    std::string id_;
    std::vector<MenuBarMenu> menus_;
    theme::ThemeColorTokens tokens_ = theme::dark();
    std::function<void(int, const std::vector<int>&)> onSelect_;
    float x_ = 0, y_ = 0, screenWidth_ = 800, screenHeight_ = 600, height_ = 0, menuWidth_ = 220;
};

inline MenuBarBuilder menuBar(core::dsl::Ui& ui, const std::string& id) { return MenuBarBuilder(ui, id); }

} // namespace components
