#pragma once

#include "components/button.h"
#include "components/mousearea.h"
#include "components/theme.h"
#include "core/dsl.h"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace modules::keyboard {

enum class KeyboardMode {
    Letters,
    Symbols,
    MoreSymbols
};

enum class KeyboardTheme {
    Light,
    Night
};

enum class KeyboardAction {
    Backspace,
    Enter,
    Space,
    ToggleCase,
    ToggleLetters,
    ToggleSymbols,
    Dismiss
};

struct KeyboardStyle {
    static KeyboardStyle light(core::Color accent = components::theme::defaultPrimary()) {
        KeyboardStyle style;
        style.background = core::mixColor(components::theme::color(0.95f, 0.97f, 1.0f), accent, 0.06f);
        style.key = core::mixColor(components::theme::color(0.91f, 0.95f, 1.0f), accent, 0.10f);
        style.keyHover = core::mixColor(components::theme::color(0.86f, 0.92f, 1.0f), accent, 0.13f);
        style.keyPressed = core::mixColor(components::theme::color(0.78f, 0.88f, 1.0f), accent, 0.18f);
        style.actionKey = core::mixColor(style.key, accent, 0.25f);
        style.actionKeyHover = core::mixColor(style.keyHover, accent, 0.32f);
        style.actionKeyPressed = core::mixColor(style.keyPressed, accent, 0.42f);
        style.primaryKey = accent;
        style.primaryKeyHover = core::mixColor(accent, components::theme::color(1.0f, 1.0f, 1.0f), 0.12f);
        style.primaryKeyPressed = core::mixColor(accent, components::theme::color(0.0f, 0.0f, 0.0f), 0.18f);
        style.text = components::theme::color(0.02f, 0.03f, 0.05f);
        style.primaryText = components::theme::color(0.96f, 0.98f, 1.0f);
        return style;
    }

    static KeyboardStyle night(core::Color accent = components::theme::defaultPrimary()) {
        KeyboardStyle style;
        style.background = core::mixColor(components::theme::color(0.08f, 0.09f, 0.11f), accent, 0.07f);
        style.key = core::mixColor(components::theme::color(0.16f, 0.18f, 0.22f), accent, 0.13f);
        style.keyHover = core::mixColor(components::theme::color(0.21f, 0.24f, 0.30f), accent, 0.16f);
        style.keyPressed = core::mixColor(components::theme::color(0.26f, 0.30f, 0.38f), accent, 0.22f);
        style.actionKey = core::mixColor(style.key, accent, 0.22f);
        style.actionKeyHover = core::mixColor(style.keyHover, accent, 0.28f);
        style.actionKeyPressed = core::mixColor(style.keyPressed, accent, 0.20f);
        style.primaryKey = accent;
        style.primaryKeyHover = core::mixColor(accent, components::theme::color(1.0f, 1.0f, 1.0f), 0.16f);
        style.primaryKeyPressed = core::mixColor(accent, components::theme::color(0.0f, 0.0f, 0.0f), 0.24f);
        style.text = components::theme::color(0.95f, 0.97f, 1.0f);
        style.primaryText = components::theme::color(0.98f, 0.99f, 1.0f);
        return style;
    }

    core::Color background;
    core::Color key;
    core::Color keyHover;
    core::Color keyPressed;
    core::Color actionKey;
    core::Color actionKeyHover;
    core::Color actionKeyPressed;
    core::Color primaryKey;
    core::Color primaryKeyHover;
    core::Color primaryKeyPressed;
    core::Color text;
    core::Color primaryText;
    float radius = 12.0f;
    float padding = 10.0f;
    float gap = 8.0f;
    float keyHeight = 80.0f;
    float fontSize = 31.0f;
};

struct KeyboardOptions {
    KeyboardMode mode = KeyboardMode::Letters;
    KeyboardTheme theme = KeyboardTheme::Night;
    bool uppercase = true;
    core::Color accent = components::theme::defaultPrimary();
    std::function<void(const std::string&)> onText;
    std::function<void(KeyboardAction)> onAction;
};

struct KeyboardPanelFrame {
    float x = 0.0f;
    float y = 0.0f;
    float width = 760.0f;
    float height = 350.0f;
};

struct KeyboardPanelConfig {
    float width = 0.0f;
    float height = 0.0f;
    float x = 0.0f;
    float y = 0.0f;
    bool hasWidth = false;
    bool hasHeight = false;
    bool hasPosition = false;
    float minWidth = 460.0f;
    float minHeight = 260.0f;
    float margin = 16.0f;
};

struct KeyboardPanelStyle {
    static KeyboardPanelStyle light(core::Color accent = components::theme::defaultPrimary()) {
        KeyboardPanelStyle style;
        style.panel = {0.93f, 0.965f, 1.0f, 0.98f};
        style.border = {0.72f, 0.78f, 0.88f, 0.72f};
        style.shadow = {true, {0.0f, 12.0f}, 30.0f, 0.0f, {0.10f, 0.14f, 0.22f, 0.16f}};
        style.handle = {0.18f, 0.23f, 0.32f, 0.48f};
        style.resizeHint = core::mixColor(style.handle, accent, 0.35f);
        return style;
    }

    static KeyboardPanelStyle night(core::Color accent = components::theme::defaultPrimary()) {
        KeyboardPanelStyle style;
        style.panel = {0.045f, 0.052f, 0.066f, 0.98f};
        style.border = {0.35f, 0.42f, 0.52f, 0.58f};
        style.shadow = {true, {0.0f, 12.0f}, 30.0f, 0.0f, {0.0f, 0.0f, 0.0f, 0.30f}};
        style.handle = {0.94f, 0.97f, 1.0f, 0.92f};
        style.resizeHint = core::mixColor(style.handle, accent, 0.25f);
        return style;
    }

    core::Color panel;
    core::Color border;
    core::Color handle;
    core::Color resizeHint;
    core::Shadow shadow;
    float radius = 22.0f;
    float padding = 10.0f;
    float handleZone = 38.0f;
};

struct KeyboardInputBinding {
    KeyboardInputBinding(std::string inputId, std::string& inputValue, bool multilineInput = false)
        : id(std::move(inputId)), value(&inputValue), multiline(multilineInput) {}

    std::string id;
    std::string* value = nullptr;
    bool multiline = false;
};

class KeyboardBuilder {
public:
    KeyboardBuilder(core::dsl::Ui& ui, std::string id)
        : ui_(ui), id_(std::move(id)) {}

    KeyboardBuilder& size(float width, float height) { width_ = width; height_ = height; return *this; }
    KeyboardBuilder& mode(KeyboardMode value) { mode_ = value; return *this; }
    KeyboardBuilder& uppercase(bool value) { uppercase_ = value; return *this; }
    KeyboardBuilder& theme(KeyboardTheme value) {
        return theme(value, components::theme::defaultPrimary());
    }
    KeyboardBuilder& theme(KeyboardTheme value, core::Color accent) {
        style_ = value == KeyboardTheme::Night ? KeyboardStyle::night(accent) : KeyboardStyle::light(accent);
        return *this;
    }
    KeyboardBuilder& style(const KeyboardStyle& value) { style_ = value; return *this; }
    KeyboardBuilder& transition(const core::Transition& value) { transition_ = value; return *this; }
    KeyboardBuilder& onText(std::function<void(const std::string&)> callback) {
        onText_ = std::move(callback);
        return *this;
    }
    KeyboardBuilder& onAction(std::function<void(KeyboardAction)> callback) {
        onAction_ = std::move(callback);
        return *this;
    }

    void build() {
        const auto rows = mode_ == KeyboardMode::Letters
            ? letterRows()
            : mode_ == KeyboardMode::Symbols ? symbolRows() : moreSymbolRows();
        const float rowCount = static_cast<float>(std::max<std::size_t>(1, rows.size()));
        const float padding = std::max(0.0f, style_.padding);
        const float gap = std::max(0.0f, style_.gap);
        const float availableHeight = std::max(0.0f, height_ - padding * 2.0f - gap * (rowCount - 1.0f));
        const float keyHeight = std::min(style_.keyHeight, availableHeight / rowCount);
        const float startY = padding + std::max(0.0f, (availableHeight / rowCount - keyHeight) * 0.5f);

        ui_.stack(id_)
            .size(width_, height_)
            .content([&] {
                ui_.rect(id_ + ".background")
                    .size(width_, height_)
                    .color(style_.background)
                    .radius(std::max(18.0f, style_.radius + 8.0f))
                    .transition(transition_)
                    .animate(core::AnimProperty::Color)
                    .build();

                for (std::size_t row = 0; row < rows.size(); ++row) {
                    renderRow(rows[row], static_cast<int>(row), startY + static_cast<float>(row) * (keyHeight + gap), keyHeight);
                }
            })
            .build();
    }

private:
    struct RepeatState {
        bool pressed = false;
        float elapsed = 0.0f;
        float sinceRepeat = 0.0f;
    };

    struct KeySpec {
        std::string label;
        std::string text;
        KeyboardAction action = KeyboardAction::Space;
        float weight = 1.0f;
        bool sendsText = true;
        bool primary = false;
        bool actionKey = false;
    };

    std::vector<std::vector<KeySpec>> letterRows() const {
        const std::vector<std::string> firstRow = uppercase_
            ? std::vector<std::string>{"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P"}
            : std::vector<std::string>{"q", "w", "e", "r", "t", "y", "u", "i", "o", "p"};
        const std::vector<std::string> secondRow = uppercase_
            ? std::vector<std::string>{"A", "S", "D", "F", "G", "H", "J", "K", "L"}
            : std::vector<std::string>{"a", "s", "d", "f", "g", "h", "j", "k", "l"};
        const std::vector<std::string> thirdRow = uppercase_
            ? std::vector<std::string>{"Z", "X", "C", "V", "B", "N", "M"}
            : std::vector<std::string>{"z", "x", "c", "v", "b", "n", "m"};

        return {
            textRow(firstRow),
            textRow(secondRow),
            {
                action(uppercase_ ? "ABC" : "abc", KeyboardAction::ToggleCase, 1.4f),
                text(thirdRow[0]), text(thirdRow[1]), text(thirdRow[2]), text(thirdRow[3]),
                text(thirdRow[4]), text(thirdRow[5]), text(thirdRow[6]),
                action("back", KeyboardAction::Backspace, 1.4f)
            },
            {
                action("123", KeyboardAction::ToggleSymbols, 1.2f),
                action("Space", KeyboardAction::Space, 4.3f),
                action("Enter", KeyboardAction::Enter, 1.4f, true),
                action("Hide", KeyboardAction::Dismiss, 1.2f)
            }
        };
    }

    std::vector<std::vector<KeySpec>> symbolRows() const {
        return {
            textRow({"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"}),
            {
                text("@"), text("#"), text("$"), text("_"), text("&"),
                text("-"), text("+"), text("("), text(")"), text("/")
            },
            {
                action("=\\<", KeyboardAction::ToggleSymbols, 1.55f),
                text("*"), text("\""), text("'"), text(":"), text(";"), text("!"), text("?"),
                action("back", KeyboardAction::Backspace, 1.55f)
            },
            {
                action("ABC", KeyboardAction::ToggleLetters, 1.2f),
                action("Space", KeyboardAction::Space, 4.3f),
                action("Enter", KeyboardAction::Enter, 1.4f, true),
                action("Hide", KeyboardAction::Dismiss, 1.2f)
            }
        };
    }

    std::vector<std::vector<KeySpec>> moreSymbolRows() const {
        return {
            {
                text("~"), text("`"), text("|"), text(core::dsl::utf8(0x2022)),
                text(core::dsl::utf8(0x221A)), text(core::dsl::utf8(0x03C0)),
                text(core::dsl::utf8(0x00F7)), text(core::dsl::utf8(0x00D7)),
                text(core::dsl::utf8(0x00A7)), text(core::dsl::utf8(0x0394))
            },
            {
                text(core::dsl::utf8(0x00A3)), text(core::dsl::utf8(0x00A2)),
                text(core::dsl::utf8(0x20AC)), text(core::dsl::utf8(0x00A5)),
                text("^"), text(core::dsl::utf8(0x00B0)), text("="), text("{"), text("}"), text("\\")
            },
            {
                action("?123", KeyboardAction::ToggleSymbols, 1.55f),
                text("%"), text(core::dsl::utf8(0x00A9)), text(core::dsl::utf8(0x00AE)),
                text("TM", core::dsl::utf8(0x2122)), text(core::dsl::utf8(0x2713)),
                text("["), text("]"),
                action("back", KeyboardAction::Backspace, 1.55f)
            },
            {
                action("ABC", KeyboardAction::ToggleLetters, 1.2f),
                action("Space", KeyboardAction::Space, 4.3f),
                action("Enter", KeyboardAction::Enter, 1.4f, true),
                action("Hide", KeyboardAction::Dismiss, 1.2f)
            }
        };
    }

    static std::vector<KeySpec> textRow(const std::vector<std::string>& labels) {
        std::vector<KeySpec> result;
        result.reserve(labels.size());
        for (const std::string& label : labels) {
            result.push_back(text(label));
        }
        return result;
    }

    static KeySpec text(std::string value, float weight = 1.0f) {
        return {value, value, KeyboardAction::Space, weight, true, false, false};
    }

    static KeySpec text(std::string label, std::string value, float weight = 1.0f) {
        return {std::move(label), std::move(value), KeyboardAction::Space, weight, true, false, false};
    }

    static KeySpec action(std::string label, KeyboardAction action, float weight, bool primary = false) {
        return {label, {}, action, weight, false, primary, true};
    }

    static bool repeats(const KeySpec& key) {
        return key.sendsText ||
               key.action == KeyboardAction::Backspace ||
               key.action == KeyboardAction::Space;
    }

    static void dispatchKey(const KeySpec& key,
                            const std::string& textValue,
                            KeyboardAction actionValue,
                            const std::function<void(const std::string&)>& onText,
                            const std::function<void(KeyboardAction)>& onAction) {
        if (key.sendsText) {
            if (onText) {
                onText(textValue);
            }
            return;
        }
        if (onAction) {
            onAction(actionValue);
        }
    }

    void renderRow(const std::vector<KeySpec>& keys, int row, float y, float keyHeight) {
        if (keys.empty()) {
            return;
        }

        float totalWeight = 0.0f;
        for (const KeySpec& key : keys) {
            totalWeight += std::max(0.1f, key.weight);
        }

        const float padding = std::max(0.0f, style_.padding);
        const float gap = std::max(0.0f, style_.gap);
        const float availableWidth = std::max(0.0f, width_ - padding * 2.0f - gap * static_cast<float>(keys.size() - 1));
        const float unit = totalWeight > 0.0f ? availableWidth / totalWeight : availableWidth;
        float x = padding;

        for (std::size_t index = 0; index < keys.size(); ++index) {
            const KeySpec key = keys[index];
            const float keyWidth = std::max(0.0f, unit * std::max(0.1f, key.weight));
            renderKey(key, row, static_cast<int>(index), x, y, keyWidth, keyHeight);
            x += keyWidth + gap;
        }
    }

    void renderKey(const KeySpec& key, int row, int index, float x, float y, float width, float height) {
        components::ButtonStyle buttonStyle;
        buttonStyle.normal = key.primary ? style_.primaryKey : key.actionKey ? style_.actionKey : style_.key;
        buttonStyle.hover = key.primary ? style_.primaryKeyHover : key.actionKey ? style_.actionKeyHover : style_.keyHover;
        buttonStyle.pressed = key.primary ? style_.primaryKeyPressed : key.actionKey ? style_.actionKeyPressed : style_.keyPressed;
        buttonStyle.text = key.primary ? style_.primaryText : style_.text;
        buttonStyle.icon = buttonStyle.text;
        buttonStyle.border = {0.0f, components::theme::color(0.0f, 0.0f, 0.0f, 0.0f)};
        buttonStyle.shadow = {false, {}, 0.0f, 0.0f, {}};
        buttonStyle.radius = style_.radius;
        buttonStyle.pressScale = 0.985f;

        const std::function<void(const std::string&)> onText = onText_;
        const std::function<void(KeyboardAction)> onAction = onAction_;
        const std::string textValue = key.text;
        const KeyboardAction actionValue = key.action;
        const bool repeatable = repeats(key);

        const std::string keyId = id_ + ".key." + std::to_string(row) + "." + std::to_string(index);
        RepeatState* repeat = &ui_.state<RepeatState>(keyId + ".repeat");
        std::function<void(float)> onRepeatFrame;
        if (repeatable && repeat->pressed) {
            onRepeatFrame = [repeat, key, textValue, actionValue, onText, onAction](float deltaSeconds) {
                if (!repeat->pressed) {
                    return;
                }
                constexpr float initialDelay = 0.34f;
                constexpr float repeatInterval = 0.055f;
                repeat->elapsed += std::max(0.0f, deltaSeconds);
                if (repeat->elapsed < initialDelay) {
                    return;
                }
                repeat->sinceRepeat += std::max(0.0f, deltaSeconds);
                while (repeat->sinceRepeat >= repeatInterval) {
                    repeat->sinceRepeat -= repeatInterval;
                    dispatchKey(key, textValue, actionValue, onText, onAction);
                }
            };
        }
        ui_.stack(keyId)
            .x(x)
            .y(y)
            .size(width, height)
            .content([&] {
                components::button(ui_, keyId + ".button")
                    .size(width, height)
                    .text(key.label)
                    .fontSize(style_.fontSize)
                    .style(buttonStyle)
                    .preserveFocusOnPress()
                    .transition(transition_)
                    .onPress([repeat, key, textValue, actionValue, onText, onAction] {
                        repeat->pressed = true;
                        repeat->elapsed = 0.0f;
                        repeat->sinceRepeat = 0.0f;
                        dispatchKey(key, textValue, actionValue, onText, onAction);
                    })
                    .onRelease([repeat] {
                        repeat->pressed = false;
                    })
                    .onFrame(onRepeatFrame)
                    .build();
            })
            .build();
    }

    core::dsl::Ui& ui_;
    std::string id_;
    KeyboardStyle style_ = KeyboardStyle::night();
    core::Transition transition_ = core::Transition::make(0.10f, core::Ease::OutCubic);
    std::function<void(const std::string&)> onText_;
    std::function<void(KeyboardAction)> onAction_;
    KeyboardMode mode_ = KeyboardMode::Letters;
    bool uppercase_ = true;
    float width_ = 640.0f;
    float height_ = 390.0f;
};

class KeyboardController {
public:
    void show(KeyboardOptions options = {}) {
        options_ = std::move(options);
        visible_ = true;
    }

    void hide() {
        visible_ = false;
    }

    bool visible() const {
        return visible_;
    }

    KeyboardMode mode() const {
        return options_.mode;
    }

    bool uppercase() const {
        return options_.uppercase;
    }

    void setMode(KeyboardMode value) {
        options_.mode = value;
    }

    void setUppercase(bool value) {
        options_.uppercase = value;
    }

    void setAppearance(KeyboardTheme theme, core::Color accent = components::theme::defaultPrimary()) {
        options_.theme = theme;
        options_.accent = accent;
    }

    void compose(core::dsl::Ui& ui, const std::string& id, float width, float height) {
        if (!visible_) {
            return;
        }

        KeyboardOptions& options = options_;
        KeyboardBuilder(ui, id)
            .size(width, height)
            .mode(options.mode)
            .uppercase(options.uppercase)
            .theme(options.theme, options.accent)
            .onText(options.onText)
            .onAction([this, &options](KeyboardAction action) {
                if (action == KeyboardAction::ToggleCase) {
                    options.uppercase = !options.uppercase;
                } else if (action == KeyboardAction::ToggleLetters) {
                    options.mode = KeyboardMode::Letters;
                } else if (action == KeyboardAction::ToggleSymbols) {
                    options.mode = options.mode == KeyboardMode::Symbols ? KeyboardMode::MoreSymbols : KeyboardMode::Symbols;
                } else if (action == KeyboardAction::Dismiss) {
                    hide();
                }

                if (options.onAction) {
                    options.onAction(action);
                }
            })
            .build();
    }

private:
    KeyboardOptions options_;
    bool visible_ = false;
};

class KeyboardPanelController {
public:
    KeyboardPanelController() = default;

    KeyboardPanelController(std::initializer_list<KeyboardInputBinding> bindings) {
        for (const KeyboardInputBinding& binding : bindings) {
            if (binding.value != nullptr) {
                bindInput(binding.id, *binding.value, binding.multiline);
            }
        }
    }

    KeyboardPanelController& size(float width, float height) {
        config_.width = width;
        config_.height = height;
        config_.hasWidth = true;
        config_.hasHeight = true;
        frame_.width = width;
        frame_.height = height;
        return *this;
    }

    KeyboardPanelController& position(float x, float y) {
        config_.x = x;
        config_.y = y;
        config_.hasPosition = true;
        frame_.x = x;
        frame_.y = y;
        return *this;
    }

    KeyboardPanelController& minSize(float width, float height) {
        config_.minWidth = std::max(1.0f, width);
        config_.minHeight = std::max(1.0f, height);
        return *this;
    }

    KeyboardPanelController& margin(float value) {
        config_.margin = std::max(0.0f, value);
        return *this;
    }

    KeyboardPanelController& bindInput(std::string id, std::string& value, bool multiline = false) {
        for (KeyboardInputBinding& binding : bindings_) {
            if (binding.id == id) {
                binding.value = &value;
                binding.multiline = multiline;
                return *this;
            }
        }
        bindings_.emplace_back(std::move(id), value, multiline);
        return *this;
    }

    KeyboardPanelController& config(const KeyboardPanelConfig& value) {
        config_ = value;
        if (config_.hasPosition) {
            frame_.x = config_.x;
            frame_.y = config_.y;
        }
        if (config_.hasWidth) {
            frame_.width = config_.width;
        }
        if (config_.hasHeight) {
            frame_.height = config_.height;
        }
        return *this;
    }

    void show(KeyboardOptions options = {}) {
        appearanceTheme_ = options.theme;
        accent_ = options.accent;
        appearanceInitialized_ = true;
        keyboard_.show(std::move(options));
    }

    void hide() {
        keyboard_.hide();
    }

    bool visible() const {
        return keyboard_.visible();
    }

    KeyboardMode mode() const {
        return keyboard_.mode();
    }

    bool uppercase() const {
        return keyboard_.uppercase();
    }

    void setMode(KeyboardMode value) {
        keyboard_.setMode(value);
    }

    void setUppercase(bool value) {
        keyboard_.setUppercase(value);
    }

    void setAppearance(KeyboardTheme theme, core::Color accent = components::theme::defaultPrimary()) {
        appearanceTheme_ = theme;
        accent_ = accent;
        appearanceInitialized_ = true;
        if (keyboard_.visible()) {
            keyboard_.setAppearance(theme, accent);
        }
    }

    void compose(core::dsl::Ui& ui, const std::string& id, float screenWidth, float screenHeight) {
        syncFocusedInput(ui);
        if (!keyboard_.visible()) {
            return;
        }

        ensureFrame(screenWidth, screenHeight);
        const KeyboardPanelStyle panelStyle = currentStyle();
        const core::Transition transition = panelTransition();
        const float pad = std::max(0.0f, panelStyle.padding);
        const float handleZone = std::max(24.0f, panelStyle.handleZone);
        const float keyboardWidth = std::max(260.0f, frame_.width - pad * 2.0f);
        const float keyboardHeight = std::max(230.0f, frame_.height - pad - handleZone);
        const float handleScaleX = handlePressed_ ? 0.84f : 1.0f;
        const float handleScaleY = handlePressed_ ? 1.28f : 1.0f;

        ui.stack(id)
            .position(frame_.x, frame_.y)
            .size(frame_.width, frame_.height)
            .zIndex(90)
            .content([&] {
                ui.rect(id + ".panel")
                    .size(frame_.width, frame_.height)
                    .color(panelStyle.panel)
                    .radius(panelStyle.radius)
                    .border(1.0f, panelStyle.border)
                    .shadow(panelStyle.shadow)
                    .transition(transition)
                    .animate(core::AnimProperty::Color | core::AnimProperty::Border | core::AnimProperty::Shadow)
                    .interactive()
                    .preserveFocusOnPress()
                    .cursor(core::CursorShape::Arrow)
                    .build();

                ui.stack(id + ".surface")
                    .position(pad, pad)
                    .size(keyboardWidth, keyboardHeight)
                    .content([&] {
                        keyboard_.compose(ui, id + ".keyboard", keyboardWidth, keyboardHeight);
                    })
                    .build();

                ui.rect(id + ".handle")
                    .position((frame_.width - 96.0f) * 0.5f, frame_.height - 22.0f)
                    .size(96.0f, 6.0f)
                    .color(panelStyle.handle)
                    .radius(4.0f)
                    .scale(handleScaleX, handleScaleY)
                    .transformOrigin(0.5f, 0.5f)
                    .transition(transition)
                    .animate(core::AnimProperty::Color | core::AnimProperty::Transform)
                    .build();

                composeDragHandle(ui, id, screenWidth, screenHeight);
                composeResizeAreas(ui, id, screenWidth, screenHeight, panelStyle);
            })
            .build();
    }

private:
    enum class ResizeEdge {
        Left,
        Right,
        Top,
        TopLeft,
        TopRight
    };

    void syncFocusedInput(core::dsl::Ui& ui) {
        bool anyFocused = false;
        for (std::size_t index = 0; index < bindings_.size(); ++index) {
            const KeyboardInputBinding& binding = bindings_[index];
            if (!ui.isFocused(binding.id + ".hit")) {
                continue;
            }

            anyFocused = true;
            if (suppressedInput_ == binding.id) {
                return;
            }
            activeInput_ = index;
            show(boundOptions());
            return;
        }

        if (!anyFocused) {
            suppressedInput_.clear();
        }
    }

    KeyboardOptions boundOptions() {
        KeyboardOptions options;
        options.mode = keyboard_.visible() ? keyboard_.mode() : KeyboardMode::Letters;
        options.uppercase = keyboard_.visible() ? keyboard_.uppercase() : true;
        options.theme = appearanceInitialized_ ? appearanceTheme_ : KeyboardTheme::Night;
        options.accent = accent_;
        options.onText = [this](const std::string& text) {
            if (KeyboardInputBinding* binding = activeBinding()) {
                binding->value->append(text);
            }
        };
        options.onAction = [this](KeyboardAction action) {
            handleBoundAction(action);
        };
        return options;
    }

    KeyboardInputBinding* activeBinding() {
        if (activeInput_ >= bindings_.size()) {
            return nullptr;
        }
        KeyboardInputBinding& binding = bindings_[activeInput_];
        return binding.value != nullptr ? &binding : nullptr;
    }

    void handleBoundAction(KeyboardAction action) {
        KeyboardInputBinding* binding = activeBinding();
        if (binding == nullptr) {
            return;
        }

        if (action == KeyboardAction::Backspace) {
            eraseLastTextUnit(*binding->value);
        } else if (action == KeyboardAction::Space) {
            binding->value->push_back(' ');
        } else if (action == KeyboardAction::Enter && binding->multiline) {
            binding->value->push_back('\n');
        } else if (action == KeyboardAction::Dismiss) {
            suppressedInput_ = binding->id;
        }
    }

    void ensureFrame(float screenWidth, float screenHeight) {
        const float margin = std::max(0.0f, config_.margin);
        const float maxWidth = std::max(config_.minWidth, screenWidth - margin * 2.0f);
        const float maxHeight = std::max(config_.minHeight, screenHeight - margin * 2.0f);
        if (!initialized_) {
            frame_.width = config_.hasWidth
                ? config_.width
                : std::clamp(screenWidth * 0.56f, std::min(520.0f, maxWidth), std::min(760.0f, maxWidth));
            frame_.height = config_.hasHeight
                ? config_.height
                : std::clamp(screenHeight * 0.36f, std::min(280.0f, maxHeight), std::min(360.0f, maxHeight));
            if (config_.hasPosition) {
                frame_.x = config_.x;
                frame_.y = config_.y;
            } else {
                frame_.x = std::max(margin, (screenWidth - frame_.width) * 0.5f);
                frame_.y = std::max(margin, screenHeight - frame_.height - 28.0f);
            }
            initialized_ = true;
        }
        clampFrame(screenWidth, screenHeight);
    }

    void clampFrame(float screenWidth, float screenHeight) {
        const float margin = std::max(0.0f, config_.margin);
        const float maxWidth = std::max(config_.minWidth, screenWidth - margin * 2.0f);
        const float maxHeight = std::max(config_.minHeight, screenHeight - margin * 2.0f);
        frame_.width = std::clamp(frame_.width, std::min(config_.minWidth, maxWidth), maxWidth);
        frame_.height = std::clamp(frame_.height, std::min(config_.minHeight, maxHeight), maxHeight);
        frame_.x = std::clamp(frame_.x, margin, std::max(margin, screenWidth - frame_.width - margin));
        frame_.y = std::clamp(frame_.y, margin, std::max(margin, screenHeight - frame_.height - margin));
    }

    void moveFrame(float deltaX, float deltaY, float screenWidth, float screenHeight) {
        frame_.x += deltaX;
        frame_.y += deltaY;
        clampFrame(screenWidth, screenHeight);
    }

    void resizeFrame(ResizeEdge edge, float deltaX, float deltaY, float screenWidth, float screenHeight) {
        const float oldRight = frame_.x + frame_.width;
        const float oldBottom = frame_.y + frame_.height;
        if (edge == ResizeEdge::Left || edge == ResizeEdge::TopLeft) {
            frame_.x += deltaX;
            frame_.width -= deltaX;
        }
        if (edge == ResizeEdge::Right || edge == ResizeEdge::TopRight) {
            frame_.width += deltaX;
        }
        if (edge == ResizeEdge::Top || edge == ResizeEdge::TopLeft || edge == ResizeEdge::TopRight) {
            frame_.y += deltaY;
            frame_.height -= deltaY;
        }

        if (frame_.width < config_.minWidth &&
            (edge == ResizeEdge::Left || edge == ResizeEdge::TopLeft)) {
            frame_.x = oldRight - config_.minWidth;
        }
        if (frame_.height < config_.minHeight &&
            (edge == ResizeEdge::Top || edge == ResizeEdge::TopLeft || edge == ResizeEdge::TopRight)) {
            frame_.y = oldBottom - config_.minHeight;
        }
        clampFrame(screenWidth, screenHeight);
    }

    void composeDragHandle(core::dsl::Ui& ui, const std::string& id, float screenWidth, float screenHeight) {
        components::mouseArea(ui, id + ".handle.drag")
            .position((frame_.width - 174.0f) * 0.5f, frame_.height - 36.0f)
            .size(174.0f, 32.0f)
            .radius(16.0f)
            .dragThreshold(0.0f)
            .preserveFocusOnPress()
            .onPress([this](const components::MouseEvent&) {
                handlePressed_ = true;
            })
            .onRelease([this](const components::MouseEvent&) {
                handlePressed_ = false;
            })
            .onDragEnd([this](const components::MouseDragEvent&) {
                handlePressed_ = false;
            })
            .onDrag([this, screenWidth, screenHeight](const components::MouseDragEvent& event) {
                moveFrame(event.deltaX, event.deltaY, screenWidth, screenHeight);
            })
            .build();
    }

    void composeResizeAreas(core::dsl::Ui& ui,
                            const std::string& id,
                            float screenWidth,
                            float screenHeight,
                            const KeyboardPanelStyle& style) {
        constexpr float edge = 10.0f;
        constexpr float corner = 26.0f;
        auto resizeArea = [&](const std::string& suffix,
                              ResizeEdge resizeEdge,
                              float x,
                              float y,
                              float w,
                              float h) {
            components::mouseArea(ui, id + ".resize." + suffix)
                .position(x, y)
                .size(std::max(1.0f, w), std::max(1.0f, h))
                .color({style.resizeHint.r, style.resizeHint.g, style.resizeHint.b, 0.001f})
                .dragThreshold(0.0f)
                .preserveFocusOnPress()
                .onDrag([this, resizeEdge, screenWidth, screenHeight](const components::MouseDragEvent& event) {
                    resizeFrame(resizeEdge, event.deltaX, event.deltaY, screenWidth, screenHeight);
                })
                .build();
        };

        resizeArea("left", ResizeEdge::Left, 0.0f, corner, edge, frame_.height - corner - edge);
        resizeArea("right", ResizeEdge::Right, frame_.width - edge, corner, edge, frame_.height - corner - edge);
        resizeArea("top", ResizeEdge::Top, corner, 0.0f, frame_.width - corner * 2.0f, edge);
        resizeArea("topLeft", ResizeEdge::TopLeft, 0.0f, 0.0f, corner, corner);
        resizeArea("topRight", ResizeEdge::TopRight, frame_.width - corner, 0.0f, corner, corner);
    }

    KeyboardPanelStyle currentStyle() const {
        const KeyboardTheme theme = appearanceInitialized_ ? appearanceTheme_ : KeyboardTheme::Night;
        return theme == KeyboardTheme::Night ? KeyboardPanelStyle::night(accent_) : KeyboardPanelStyle::light(accent_);
    }

    static core::Transition panelTransition() {
        auto transition = core::Transition::make(0.18f, core::Ease::OutCubic);
        transition.animate(core::AnimProperty::Color | core::AnimProperty::TextColor |
                           core::AnimProperty::Border | core::AnimProperty::Shadow |
                           core::AnimProperty::Transform);
        return transition;
    }

    static void eraseLastTextUnit(std::string& value) {
        if (value.empty()) {
            return;
        }
        std::size_t start = value.size() - 1;
        while (start > 0 &&
               (static_cast<unsigned char>(value[start]) & 0xC0) == 0x80) {
            --start;
        }
        value.erase(start);
    }

    KeyboardController keyboard_;
    KeyboardPanelConfig config_;
    KeyboardPanelFrame frame_;
    std::vector<KeyboardInputBinding> bindings_;
    std::size_t activeInput_ = std::numeric_limits<std::size_t>::max();
    std::string suppressedInput_;
    KeyboardTheme appearanceTheme_ = KeyboardTheme::Night;
    core::Color accent_ = components::theme::defaultPrimary();
    bool appearanceInitialized_ = false;
    bool initialized_ = false;
    bool handlePressed_ = false;
};

inline KeyboardBuilder keyboard(core::dsl::Ui& ui, const std::string& id) {
    return KeyboardBuilder(ui, id);
}

} // namespace modules::keyboard
