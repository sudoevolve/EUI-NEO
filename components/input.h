#pragma once

#include "components/theme.h"
#include "components/input_model.h"
#include "components/scroll.h"
#include "core/dsl.h"
#include "eui/signal.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <utility>

namespace components {

struct InputStyle {
    InputStyle() : InputStyle(theme::dark()) {}

    explicit InputStyle(const theme::ThemeColorTokens& tokens) {
        background = tokens.surface;
        focused = theme::resolveFieldFill(tokens, tokens.surface, 0.20f, 0.70f);
        border = theme::withOpacity(tokens.border, 0.78f);
        focusBorder = theme::withAlpha(tokens.primary, 0.86f);
        text = tokens.text;
        placeholder = theme::withOpacity(tokens.text, 0.45f);
        cursor = tokens.primary;
        shadow = theme::popupShadow(tokens);
        radius = tokens.metrics.radius.popup;
    }

    core::Color background;
    core::Color focused;
    core::Color border;
    core::Color focusBorder;
    core::Color text;
    core::Color placeholder;
    core::Color cursor;
    core::Shadow shadow;
    float radius = 10.0f;
};

class InputBuilder {
public:
    InputBuilder(core::dsl::Ui& ui, std::string id)
        : ui_(ui), id_(std::move(id)) {}

    InputBuilder& x(float value) { x_ = value; hasX_ = true; return *this; }
    InputBuilder& y(float value) { y_ = value; hasY_ = true; return *this; }
    InputBuilder& position(float xValue, float yValue) { return x(xValue).y(yValue); }
    InputBuilder& size(float width, float height) { width_ = width; height_ = height; return *this; }
    InputBuilder& value(std::string value) { text_ = std::move(value); return *this; }
    InputBuilder& bind(eui::Signal<std::string>& signal) {
        value(signal.get());
        onChange([&signal](const std::string& value) { signal.set(value); });
        return *this;
    }
    InputBuilder& placeholder(std::string value) { placeholder_ = std::move(value); return *this; }
    InputBuilder& accessibleName(std::string value) { accessibleName_ = std::move(value); return *this; }
    InputBuilder& multiline(bool value = true) { multiline_ = value; return *this; }
    /** @brief 多行输入溢出时显示垂直滚动条，默认关闭；不影响滚轮和光标跟随。 */
    InputBuilder& scrollbar(bool value = true) { scrollbar_ = value; return *this; }
    InputBuilder& fontSize(float value) { fontSize_ = std::max(1.0f, value); return *this; }
    InputBuilder& fontFamily(std::string value) { fontFamily_ = std::move(value); return *this; }
    InputBuilder& inset(float value) { inset_ = std::max(0.0f, value); return *this; }
    InputBuilder& style(const InputStyle& value) { style_ = value; return *this; }
    InputBuilder& theme(const theme::ThemeColorTokens& tokens) {
        style_ = InputStyle(tokens);
        scrollStyle_ = ScrollStyle(tokens);
        metrics_ = tokens.metrics;
        return *this;
    }
    InputBuilder& transition(const core::Transition& value) { transition_ = value; return *this; }
    InputBuilder& transition(float duration, core::Ease ease = core::Ease::OutCubic) {
        transition_ = core::Transition::make(duration, ease);
        return *this;
    }
    InputBuilder& onChange(std::function<void(const std::string&)> callback) {
        onChange_ = std::move(callback);
        return *this;
    }
    InputBuilder& onEnter(std::function<void()> callback) {
        onEnter_ = std::move(callback);
        return *this;
    }
    InputBuilder& onFocus(std::function<void(bool)> callback) {
        onFocus_ = std::move(callback);
        return *this;
    }

    void build() {
        const std::string hitId = id_ + ".hit";
        const bool focused = ui_.isFocused(hitId);
        const float inset = inset_ >= 0.0f ? inset_ : metrics_.spacing.content;
        const float fontSize = fontSize_ > 0.0f ? fontSize_ : metrics_.typography.input;
        // 开启时固定预留槽位，避免溢出临界点因滚动条显隐反复改变换行宽度。
        const float scrollbarWidth = scrollbar_ && multiline_
            ? std::min(metrics_.control.scrollbar, std::max(0.0f, width_ - inset * 2.0f)) : 0.0f;
        const float scrollbarGutter = scrollbarWidth > 0.0f ? scrollbarWidth + 4.0f : 0.0f;
        const float textWidth = std::max(0.0f, width_ - inset * 2.0f - scrollbarGutter);
        const bool allowMultiline = multiline_;
        const std::function<void(const std::string&)> onChange = onChange_;
        const std::function<void()> onEnter = onEnter_;
        const std::function<void(bool)> onFocus = onFocus_;
        const float textLineHeight = fontSize * 1.2f;
        const float textY = multiline_ ? inset : std::max(0.0f, (height_ - textLineHeight) * 0.5f);
        const float textHeight = multiline_ ? std::max(0.0f, height_ - inset * 2.0f) : textLineHeight;
        const float width = width_ - scrollbarGutter;
        const float controlWidth = width_;
        const std::string fontFamily = fontFamily_;
        InputState& state = ui_.state<InputState>(id_);
        if (state.text != text_) {
            const bool wasFocused = focused;
            state.text = text_;
            ++state.textRevision;
            state.cursor = InputModel::clampUtf8Boundary(state.text, static_cast<int>(state.text.size()));
            state.selectionStart = state.cursor;
            state.selectionEnd = state.cursor;
            if (!wasFocused) {
                state.horizontalScroll = 0.0f;
                state.verticalScroll = 0.0f;
                state.undoStack.clear();
                state.redoStack.clear();
            }
        }
        state.cursor = InputModel::clampUtf8Boundary(state.text, state.cursor);
        state.selectionStart = InputModel::clampUtf8Boundary(state.text, state.selectionStart);
        state.selectionEnd = InputModel::clampUtf8Boundary(state.text, state.selectionEnd);
        const bool hasComposition = focused && !state.compositionText.empty();
        InputState& display = InputModel::displayState(state, hasComposition);
        const InputLayout layout = InputLayout::build(display, textWidth, textHeight, width, inset, textY, textLineHeight, fontFamily_, fontSize, multiline_);
        state.horizontalScroll = display.horizontalScroll;
        state.verticalScroll = display.verticalScroll;
        const bool empty = display.text.empty();
        const bool hasSelection = !layout.selectionRects.empty();
        const std::string textDirtyKey = id_ + ".text|" + std::to_string(state.textRevision) + "|" + std::to_string(display.textRevision) +
            "|" + std::to_string(static_cast<int>(std::lround(state.horizontalScroll * 64.0f))) +
            "|" + std::to_string(static_cast<int>(std::lround(state.verticalScroll * 64.0f))) +
            (empty ? "|p" : "|v") + (hasComposition ? "|ime" : "");
        const float renderedTextHeight = multiline_ ? layout.contentHeight : textHeight;
        const float caretX = layout.clampedCursorX();
        const auto compositionRange = InputModel::selectionRange(state);
        const int compositionSize = hasComposition ? static_cast<int>(state.compositionText.size()) : 0;
        const auto documentIndex = [hasComposition, compositionRange, compositionSize](int index) {
            if (!hasComposition || index <= compositionRange.first) return index;
            if (index < compositionRange.first + compositionSize) return compositionRange.first;
            return index - compositionSize + compositionRange.second - compositionRange.first;
        };

        auto root = ui_.stack(id_)
            .size(width_, height_)
            .clip()
            .dirtyKey(InputModel::makeDirtyKey(state, focused, layout) + (scrollbar_ ? "|bar" : "|no-bar"));
        if (hasX_) {
            root.x(x_);
        }
        if (hasY_) {
            root.y(y_);
        }
        root.content([&] {
                auto hit = ui_.rect(hitId)
                    .accessible(core::AccessibleRole::TextInput, accessibleName_.empty() ? placeholder_ : accessibleName_, state.text)
                    .size(width_, height_)
                    .color(style_.background)
                    .radius(style_.radius)
                    .border(1.0f, focused ? style_.focusBorder : style_.border)
                    .shadow(focused ? style_.shadow : core::Shadow{})
                    .transition(transition_)
                    .focusable()
                    .imeRect(caretX, layout.cursorY, 1.5f, textLineHeight)
                    .onPress([&state, controlWidth, inset, layout, documentIndex](const core::PointerEvent& event, const core::Rect& bounds) {
                        state.lastBounds = bounds;
                        state.cursor = InputModel::clampUtf8Boundary(state.text, documentIndex(layout.cursorFromPointer(event.x, event.y, bounds, controlWidth, inset)));
                        state.hasPreferredCursorX = false;
                        InputModel::clearSelection(state);
                        state.dragAnchor = state.cursor;
                        state.selecting = true;
                    })
                    .onFocusChanged(onFocus)
                    .onDrag([&state, width, controlWidth, inset, fontSize, fontFamily, allowMultiline, textHeight, layout, documentIndex](const core::dsl::DragEvent& event) {
                        state.cursor = InputModel::clampUtf8Boundary(state.text, documentIndex(layout.cursorFromPointer(event.x, event.y, state.lastBounds, controlWidth, inset)));
                        state.hasPreferredCursorX = false;
                        state.selectionStart = state.dragAnchor;
                        state.selectionEnd = state.cursor;
                        if (allowMultiline) {
                            InputModel::syncVerticalScroll(state, layout, textHeight);
                        } else {
                            InputModel::syncScroll(state, std::max(0.0f, width - inset * 2.0f), fontFamily, fontSize);
                        }
                    });
                if (allowMultiline && layout.maxVerticalScroll > 0.0f) {
                    hit.onScroll([&state, layout, fontSize](const core::ScrollEvent& event) {
                        const float step = std::max(12.0f, fontSize * 2.2f);
                        state.followCaret = false;
                        state.verticalScroll = std::clamp(
                            state.verticalScroll - static_cast<float>(event.y) * step,
                            0.0f,
                            layout.maxVerticalScroll);
                    });
                }
                hit.onKeyEvent([&state, allowMultiline, onChange, onEnter, width, inset, fontSize, fontFamily, textHeight](const core::KeyEvent& event) {
                        if (!event.isDown()) {
                            return false;
                        }

                        state.followCaret = true;
                        bool changed = false;
                        bool handled = true;
                        const bool shortcut = event.modifiers.shortcut();
                        if (!state.compositionText.empty() &&
                            (event.key == core::InputKey::Backspace ||
                             event.key == core::InputKey::Delete)) {
                            return true;
                        }
                        const bool undo = shortcut && !event.modifiers.shift && event.key == core::InputKey::Z;
                        const bool redo = shortcut &&
                            (event.key == core::InputKey::Y ||
                             (event.modifiers.shift && event.key == core::InputKey::Z));
                        if (undo || redo) {
                            if (!state.compositionText.empty()) {
                                state.compositionText.clear();
                                ++state.compositionRevision;
                            }
                            changed = undo ? InputModel::undoEdit(state) : InputModel::redoEdit(state);
                            if (allowMultiline) {
                                state.horizontalScroll = 0.0f;
                                const InputLayout nextLayout = InputLayout::build(
                                    state,
                                    std::max(0.0f, width - inset * 2.0f),
                                    textHeight,
                                    width,
                                    inset,
                                    0.0f,
                                    fontSize * 1.2f,
                                    fontFamily,
                                    fontSize,
                                    allowMultiline);
                                InputModel::syncVerticalScroll(state, nextLayout, textHeight);
                            } else {
                                InputModel::syncScroll(state, std::max(0.0f, width - inset * 2.0f), fontFamily, fontSize);
                            }
                            if (changed && onChange) {
                                onChange(state.text);
                            }
                            return true;
                        }

                        if (shortcut && event.key == core::InputKey::A) {
                            state.selectionStart = 0;
                            state.selectionEnd = static_cast<int>(state.text.size());
                            state.cursor = state.selectionEnd;
                        } else if (shortcut && event.key == core::InputKey::C) {
                            InputModel::copySelection(state);
                        } else if (shortcut && event.key == core::InputKey::X) {
                            if (InputModel::hasTextSelection(state)) {
                                InputModel::copySelection(state);
                                InputModel::pushUndoState(state);
                                InputModel::eraseSelection(state);
                                changed = true;
                            }
                        } else if (shortcut && event.key == core::InputKey::V) {
                            // Clipboard text is delivered separately through onTextInput.
                        } else if (event.key == core::InputKey::Left) {
                            InputModel::moveCursor(state, -1, event.modifiers.shift, fontFamily, fontSize, allowMultiline, std::max(0.0f, width - inset * 2.0f));
                        } else if (event.key == core::InputKey::Right) {
                            InputModel::moveCursor(state, 1, event.modifiers.shift, fontFamily, fontSize, allowMultiline, std::max(0.0f, width - inset * 2.0f));
                        } else if (event.key == core::InputKey::Up && allowMultiline) {
                            InputModel::moveCursorVertical(state, -1, event.modifiers.shift, fontFamily, fontSize, std::max(0.0f, width - inset * 2.0f), textHeight);
                        } else if (event.key == core::InputKey::Down && allowMultiline) {
                            InputModel::moveCursorVertical(state, 1, event.modifiers.shift, fontFamily, fontSize, std::max(0.0f, width - inset * 2.0f), textHeight);
                        } else if (event.key == core::InputKey::Home) {
                            if (allowMultiline) {
                                InputModel::moveCursorToLineEdge(state, false, event.modifiers.shift, fontFamily, fontSize, std::max(0.0f, width - inset * 2.0f));
                            } else {
                                InputModel::moveCursorTo(state, 0, event.modifiers.shift);
                            }
                        } else if (event.key == core::InputKey::End) {
                            if (allowMultiline) {
                                InputModel::moveCursorToLineEdge(state, true, event.modifiers.shift, fontFamily, fontSize, std::max(0.0f, width - inset * 2.0f));
                            } else {
                                InputModel::moveCursorTo(state, static_cast<int>(state.text.size()), event.modifiers.shift);
                            }
                        } else if (event.key == core::InputKey::Delete) {
                            if (InputModel::hasTextSelection(state)) {
                                InputModel::pushUndoState(state);
                                InputModel::eraseSelection(state);
                                changed = true;
                            } else if (state.cursor < static_cast<int>(state.text.size())) {
                                const int next = InputModel::nextCursorIndex(state, fontFamily, fontSize, allowMultiline, std::max(0.0f, width - inset * 2.0f));
                                InputModel::pushUndoState(state);
                                state.text.erase(static_cast<std::size_t>(state.cursor), static_cast<std::size_t>(next - state.cursor));
                                ++state.textRevision;
                                changed = true;
                            }
                        } else if (event.key == core::InputKey::Backspace) {
                            if (InputModel::hasTextSelection(state)) {
                                InputModel::pushUndoState(state);
                                InputModel::eraseSelection(state);
                                changed = true;
                            } else if (state.cursor > 0) {
                                const int previous = InputModel::prevCursorIndex(state, fontFamily, fontSize, allowMultiline, std::max(0.0f, width - inset * 2.0f));
                                InputModel::pushUndoState(state);
                                state.text.erase(static_cast<std::size_t>(previous), static_cast<std::size_t>(state.cursor - previous));
                                ++state.textRevision;
                                state.cursor = previous;
                                InputModel::clearSelection(state);
                                changed = true;
                            }
                        } else if (event.key == core::InputKey::Enter) {
                            if (allowMultiline) {
                                InputModel::pushUndoState(state);
                                InputModel::insertAtCursor(state, "\n");
                                changed = true;
                            } else if (onEnter) {
                                onEnter();
                            }
                        } else if (event.key == core::InputKey::Escape) {
                            if (onEnter) {
                                onEnter();
                            }
                        } else {
                            handled = false;
                        }
                        if (allowMultiline) {
                            state.horizontalScroll = 0.0f;
                        } else {
                            InputModel::syncScroll(state, std::max(0.0f, width - inset * 2.0f), fontFamily, fontSize);
                        }
                        if (changed && onChange) {
                            onChange(state.text);
                        }
                        return handled;
                    })
                    .onTextInput([&state, allowMultiline, onChange, width, inset, fontSize, fontFamily](const core::TextInputEvent& event) {
                        state.followCaret = true;
                        bool changed = false;
                        const std::string nextComposition = event.composing
                            ? InputModel::filteredText(event.compositionText, allowMultiline)
                            : std::string{};
                        if (state.compositionText != nextComposition) {
                            state.compositionText = nextComposition;
                            ++state.compositionRevision;
                        }

                        const auto insertText = [&](const std::string& text) {
                            if (text.empty()) {
                                return;
                            }
                            if (!state.compositionText.empty()) {
                                state.compositionText.clear();
                                ++state.compositionRevision;
                            }
                            InputModel::pushUndoState(state);
                            InputModel::insertAtCursor(state, InputModel::filteredText(text, allowMultiline));
                            changed = true;
                        };
                        insertText(event.text);
                        insertText(event.pasteText);

                        if (allowMultiline) {
                            state.horizontalScroll = 0.0f;
                        } else {
                            InputModel::syncScroll(state, std::max(0.0f, width - inset * 2.0f), fontFamily, fontSize);
                        }
                        if (changed && onChange) {
                            onChange(state.text);
                        }
                    })
                    .build();

                ui_.stack(id_ + ".textViewport")
                    .position(inset, textY)
                    .size(textWidth, textHeight)
                    .clip()
                    .content([&] {
                        if (hasSelection) {
                            for (size_t index = 0; index < layout.selectionRects.size(); ++index) {
                                const auto& selectionRect = layout.selectionRects[index];
                                ui_.rect(id_ + ".selection." + std::to_string(index))
                                    .position(selectionRect.x - inset, selectionRect.y - textY)
                                    .size(selectionRect.width, selectionRect.height)
                                    .color(theme::withAlpha(style_.cursor, hasComposition ? 0.12f : 0.24f))
                                    .radius(multiline_ ? 0.0f : 3.0f)
                                    .build();
                                if (hasComposition) {
                                    ui_.rect(id_ + ".composition.underline." + std::to_string(index))
                                        .position(selectionRect.x - inset, selectionRect.y - textY + textLineHeight - 2.f)
                                        .size(selectionRect.width, 1.f).color(style_.cursor).build();
                                }
                            }
                        }

                        if (multiline_ && !empty) {
                            const auto& lines = layout.lineList();
                            const auto firstLine = static_cast<std::size_t>(std::max(0.0f, std::floor(state.verticalScroll / textLineHeight)));
                            const auto endLine = std::min(lines.size(), static_cast<std::size_t>(std::ceil((state.verticalScroll + textHeight) / textLineHeight)) + 1);
                            for (std::size_t index = firstLine; index < endLine; ++index) {
                                const auto& line = lines[index];
                                const float y = static_cast<float>(index) * textLineHeight - state.verticalScroll;
                                if (y + textLineHeight < 0.0f || y > textHeight) {
                                    continue;
                                }
                                ui_.text(id_ + ".text." + std::to_string(index))
                                    .position(0.0f, y)
                                    .size(layout.visibleTextWidth, textLineHeight)
                                    .dirtyKey(textDirtyKey + "|" + std::to_string(index))
                                    .text(display.text.substr(static_cast<std::size_t>(line.start),
                                                            static_cast<std::size_t>(std::max(0, line.end - line.start))))
                                    .fontSize(fontSize)
                                    .fontFamily(fontFamily_)
                                    .lineHeight(textLineHeight)
                                    .color(style_.text)
                                    .wrap(false)
                                    .verticalAlign(core::VerticalAlign::Top)
                                    .build();
                            }
                        } else {
                            ui_.text(id_ + ".text")
                                .position(-state.horizontalScroll, -state.verticalScroll)
                                .size(layout.visibleTextWidth, renderedTextHeight)
                                .dirtyKey(textDirtyKey)
                                .text(empty ? placeholder_ : display.text)
                                .fontSize(fontSize)
                                .fontFamily(fontFamily_)
                                .lineHeight(textLineHeight)
                                .color(empty ? style_.placeholder : style_.text)
                                .wrap(false)
                                .verticalAlign(core::VerticalAlign::Top)
                                .build();
                        }

                        if (focused) {
                            ui_.rect(id_ + ".cursor")
                                .position(caretX - inset, layout.cursorY - textY)
                                .size(1.5f, fontSize * 1.18f)
                                .color(style_.cursor)
                                .radius(1.0f)
                                .build();
                        }
                    })
                    .build();
                if (scrollbarWidth > 0.0f && textHeight > 0.0f && layout.maxVerticalScroll > 0.0f) {
                    const float thumbHeight = std::clamp(textHeight * textHeight / layout.contentHeight,
                                                        std::min(24.0f, textHeight), textHeight);
                    const float travel = textHeight - thumbHeight;
                    const float maximum = layout.maxVerticalScroll;
                    const float thumbY = travel * state.verticalScroll / maximum;
                    const float barX = width_ - inset - scrollbarWidth;
                    const auto wheel = [&state, maximum, fontSize](const core::ScrollEvent& event) {
                        state.followCaret = false;
                        state.verticalScroll = std::clamp(state.verticalScroll - static_cast<float>(event.y) *
                            std::max(12.0f, fontSize * 2.2f), 0.0f, maximum);
                    };
                    ui_.rect(id_ + ".scrollbar.track")
                        .position(barX, textY).size(scrollbarWidth, textHeight)
                        .color(scrollStyle_.track).radius(scrollStyle_.radius)
                        .preserveFocusOnPress().onScroll(wheel)
                        .onPress([&state, maximum, travel, thumbHeight, textHeight](const core::PointerEvent& event, const core::Rect& bounds) {
                            const float scale = bounds.height / textHeight;
                            const float y = static_cast<float>(event.y - bounds.y) / std::max(0.001f, scale);
                            state.followCaret = false;
                            state.verticalScroll = travel > 0.0f ? std::clamp((y - thumbHeight * 0.5f) / travel, 0.0f, 1.0f) * maximum : 0.0f;
                        }).build();
                    ui_.rect(id_ + ".scrollbar.thumb")
                        .position(barX, textY + thumbY).size(scrollbarWidth, thumbHeight)
                        .states(scrollStyle_.thumb, scrollStyle_.thumbHover, scrollStyle_.thumbPressed)
                        .radius(scrollStyle_.radius).cursor(core::CursorShape::Hand)
                        .preserveFocusOnPress().onScroll(wheel)
                        .onPress([&state, thumbHeight](const core::PointerEvent&, const core::Rect& bounds) {
                            state.followCaret = false;
                            state.scrollbarDragOffset = state.verticalScroll;
                            state.scrollbarDragScale = std::max(0.001f, bounds.height / thumbHeight);
                        })
                        .onDrag([&state, travel, maximum](const core::dsl::DragEvent& event) {
                            state.followCaret = false;
                            if (travel > 0.0f) {
                                state.verticalScroll = std::clamp(state.scrollbarDragOffset +
                                    static_cast<float>(event.totalY) / state.scrollbarDragScale * maximum / travel, 0.0f, maximum);
                            }
                        }).build();
                }
            })
            .build();
    }

private:
    using InputModel = input_detail::InputModel;
    using InputState = InputModel::InputState;
    using InputLayout = InputModel::InputLayout;

    core::dsl::Ui& ui_;
    std::string id_;
    std::string accessibleName_;
    InputStyle style_;
    ScrollStyle scrollStyle_;
    theme::ThemeMetricTokens metrics_;
    core::Transition transition_ = core::Transition::make(0.16f, core::Ease::OutCubic);
    std::function<void(const std::string&)> onChange_;
    std::function<void()> onEnter_;
    std::function<void(bool)> onFocus_;
    std::string text_;
    std::string placeholder_ = "Hello EUI-NEO 😉";
    bool multiline_ = false;
    bool scrollbar_ = false;
    float width_ = 260.0f;
    float height_ = 44.0f;
    float x_ = 0.0f;
    float y_ = 0.0f;
    float inset_ = -1.0f;
    float fontSize_ = 0.0f;
    std::string fontFamily_ = "Microsoft YaHei";
    bool hasX_ = false;
    bool hasY_ = false;
};

inline InputBuilder input(core::dsl::Ui& ui, const std::string& id) {
    return InputBuilder(ui, id);
}

} // namespace components
