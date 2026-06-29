#pragma once

#include "components/theme.h"
#include "components/input_model.h"
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
        hover = tokens.surfaceHover;
        focused = theme::resolveFieldFill(tokens, tokens.surface, 0.20f, 0.70f);
        pressed = tokens.surfaceActive;
        border = theme::withOpacity(tokens.border, 0.78f);
        focusBorder = theme::withAlpha(tokens.primary, 0.86f);
        text = tokens.text;
        placeholder = theme::withOpacity(tokens.text, 0.45f);
        cursor = tokens.primary;
        shadow = theme::popupShadow(tokens);
    }

    core::Color background;
    core::Color hover;
    core::Color focused;
    core::Color pressed;
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

    InputBuilder& size(float width, float height) { width_ = width; height_ = height; return *this; }
    InputBuilder& value(std::string value) { text_ = std::move(value); return *this; }
    InputBuilder& bind(eui::Signal<std::string>& signal) {
        value(signal.get());
        onChange([&signal](const std::string& value) { signal.set(value); });
        return *this;
    }
    InputBuilder& placeholder(std::string value) { placeholder_ = std::move(value); return *this; }
    InputBuilder& multiline(bool value = true) { multiline_ = value; return *this; }
    InputBuilder& fontSize(float value) { fontSize_ = std::max(1.0f, value); return *this; }
    InputBuilder& fontFamily(std::string value) { fontFamily_ = std::move(value); return *this; }
    InputBuilder& inset(float value) { inset_ = std::max(0.0f, value); return *this; }
    InputBuilder& style(const InputStyle& value) { style_ = value; return *this; }
    InputBuilder& theme(const theme::ThemeColorTokens& tokens) { style_ = InputStyle(tokens); return *this; }
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
        const float textWidth = std::max(0.0f, width_ - inset_ * 2.0f);
        const bool allowMultiline = multiline_;
        const std::function<void(const std::string&)> onChange = onChange_;
        const std::function<void()> onEnter = onEnter_;
        const std::function<void(bool)> onFocus = onFocus_;
        const float textLineHeight = fontSize_;
        const float textY = multiline_ ? inset_ : std::max(0.0f, (height_ - textLineHeight) * 0.5f);
        const float textHeight = multiline_ ? std::max(0.0f, height_ - inset_ * 2.0f) : textLineHeight;
        const float width = width_;
        const float inset = inset_;
        const float fontSize = fontSize_;
        const std::string fontFamily = fontFamily_;
        InputState& state = ui_.state<InputState>(id_);
        if (state.text != text_) {
            state.text = text_;
            ++state.textRevision;
            state.cursor = InputModel::clampUtf8Boundary(state.text, static_cast<int>(state.text.size()));
            state.selectionStart = state.cursor;
            state.selectionEnd = state.cursor;
            state.horizontalScroll = 0.0f;
            state.verticalScroll = 0.0f;
            state.undoStack.clear();
            state.redoStack.clear();
        }
        state.cursor = InputModel::clampUtf8Boundary(state.text, state.cursor);
        state.selectionStart = InputModel::clampUtf8Boundary(state.text, state.selectionStart);
        state.selectionEnd = InputModel::clampUtf8Boundary(state.text, state.selectionEnd);
        const InputLayout layout = InputLayout::build(state, textWidth, textHeight, width_, inset_, textY, textLineHeight, fontFamily_, fontSize_, multiline_);
        const bool empty = state.text.empty();
        const bool hasComposition = focused && !state.compositionText.empty();
        const bool hasSelection = !layout.selectionRects.empty();
        const std::string textDirtyKey = id_ + ".text|" + std::to_string(state.textRevision) + (empty ? "|p" : "|v");
        const std::string compositionDirtyKey = id_ + ".composition|" + std::to_string(state.compositionRevision);
        const float renderedTextHeight = multiline_ ? layout.contentHeight : textHeight;
        const float compositionPadding = 1.0f;
        const float compositionTextLeft = inset_;
        const float compositionTextRight = std::max(compositionTextLeft, width_ - inset_);
        const float compositionAvailableWidth = std::max(4.0f, compositionTextRight - compositionTextLeft);
        const float compositionTextWidth = hasComposition
            ? InputModel::measureMetrics(state.compositionText, fontFamily_, fontSize_).width
            : 0.0f;
        const float compositionWidth = hasComposition
            ? std::clamp(std::ceil(compositionTextWidth) + compositionPadding * 2.0f, 2.0f, compositionAvailableWidth)
            : 0.0f;
        const float compositionX = hasComposition
            ? std::clamp(layout.clampedCursorX(), compositionTextLeft, std::max(compositionTextLeft, compositionTextRight - compositionWidth))
            : layout.clampedCursorX();

        ui_.stack(id_)
            .size(width_, height_)
            .clip()
            .dirtyKey(InputModel::makeDirtyKey(state, focused, layout))
            .content([&] {
                ui_.rect(hitId)
                    .size(width_, height_)
                    .states(style_.background,
                            style_.background,
                            style_.background)
                    .radius(style_.radius)
                    .border(1.0f, focused ? style_.focusBorder : style_.border)
                    .shadow(focused ? style_.shadow : core::Shadow{})
                    .transition(transition_)
                    .focusable()
                    .imeRect(layout.clampedCursorX(), layout.cursorY, 1.5f, textLineHeight)
                    .onPress([&state, width, inset, layout](const core::PointerEvent& event, const core::Rect& bounds) {
                        state.lastBounds = bounds;
                        state.cursor = InputModel::clampUtf8Boundary(state.text, layout.cursorFromPointer(event.x, event.y, bounds, width, inset));
                        state.hasPreferredCursorX = false;
                        InputModel::clearSelection(state);
                        state.dragAnchor = state.cursor;
                        state.selecting = true;
                    })
                    .onFocusChanged(onFocus)
                    .onDrag([&state, width, inset, fontSize, fontFamily, allowMultiline, textHeight, layout](const core::dsl::DragEvent& event) {
                        state.cursor = InputModel::clampUtf8Boundary(state.text, layout.cursorFromPointer(event.x, event.y, state.lastBounds, width, inset));
                        state.hasPreferredCursorX = false;
                        state.selectionStart = state.dragAnchor;
                        state.selectionEnd = state.cursor;
                        if (allowMultiline) {
                            InputModel::syncVerticalScroll(state, layout, textHeight);
                        } else {
                            InputModel::syncScroll(state, std::max(0.0f, width - inset * 2.0f), fontFamily, fontSize);
                        }
                    })
                    .onScroll([&state, allowMultiline, layout, fontSize](const core::ScrollEvent& event) {
                        if (!allowMultiline || layout.maxVerticalScroll <= 0.0f) {
                            return;
                        }
                        const float step = std::max(12.0f, fontSize * 2.2f);
                        state.verticalScroll = std::clamp(
                            state.verticalScroll - static_cast<float>(event.y) * step,
                            0.0f,
                            layout.maxVerticalScroll);
                    })
                    .onTextInput([&state, allowMultiline, onChange, onEnter, width, inset, fontSize, fontFamily, textHeight](const core::KeyboardEvent& event) {
                        bool changed = false;
                        const std::string nextComposition = event.composing ? InputModel::filteredText(event.compositionText, allowMultiline) : std::string{};
                        if (state.compositionText != nextComposition) {
                            state.compositionText = nextComposition;
                            ++state.compositionRevision;
                        }

                        if (event.undo || event.redo) {
                            if (!state.compositionText.empty()) {
                                state.compositionText.clear();
                                ++state.compositionRevision;
                            }
                            changed = event.undo ? InputModel::undoEdit(state) : InputModel::redoEdit(state);
                            if (allowMultiline) {
                                state.horizontalScroll = 0.0f;
                                const InputLayout nextLayout = InputLayout::build(
                                    state,
                                    std::max(0.0f, width - inset * 2.0f),
                                    textHeight,
                                    width,
                                    inset,
                                    0.0f,
                                    fontSize,
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
                            return;
                        }

                        if (event.selectAll) {
                            state.selectionStart = 0;
                            state.selectionEnd = static_cast<int>(state.text.size());
                            state.cursor = state.selectionEnd;
                        }
                        if (event.copy) {
                            InputModel::copySelection(state);
                        }
                        if (event.cut && InputModel::hasTextSelection(state)) {
                            InputModel::copySelection(state);
                            InputModel::pushUndoState(state);
                            InputModel::eraseSelection(state);
                            changed = true;
                        }
                        if (event.left) {
                            InputModel::moveCursor(state, -1, event.shift, fontFamily, fontSize, allowMultiline, std::max(0.0f, width - inset * 2.0f));
                        }
                        if (event.right) {
                            InputModel::moveCursor(state, 1, event.shift, fontFamily, fontSize, allowMultiline, std::max(0.0f, width - inset * 2.0f));
                        }
                        if (event.up && allowMultiline) {
                            InputModel::moveCursorVertical(state, -1, event.shift, fontFamily, fontSize, std::max(0.0f, width - inset * 2.0f), textHeight);
                        }
                        if (event.down && allowMultiline) {
                            InputModel::moveCursorVertical(state, 1, event.shift, fontFamily, fontSize, std::max(0.0f, width - inset * 2.0f), textHeight);
                        }
                        if (event.home) {
                            if (allowMultiline) {
                                InputModel::moveCursorToLineEdge(state, false, event.shift, fontFamily, fontSize, std::max(0.0f, width - inset * 2.0f));
                            } else {
                                InputModel::moveCursorTo(state, 0, event.shift);
                            }
                        }
                        if (event.end) {
                            if (allowMultiline) {
                                InputModel::moveCursorToLineEdge(state, true, event.shift, fontFamily, fontSize, std::max(0.0f, width - inset * 2.0f));
                            } else {
                                InputModel::moveCursorTo(state, static_cast<int>(state.text.size()), event.shift);
                            }
                        }
                        if (event.del) {
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
                        }
                        if (event.backspace) {
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
                        }
                        if (!event.text.empty()) {
                            if (!state.compositionText.empty()) {
                                state.compositionText.clear();
                                ++state.compositionRevision;
                            }
                            InputModel::pushUndoState(state);
                            InputModel::insertAtCursor(state, InputModel::filteredText(event.text, allowMultiline));
                            changed = true;
                        }
                        if (!event.pasteText.empty()) {
                            if (!state.compositionText.empty()) {
                                state.compositionText.clear();
                                ++state.compositionRevision;
                            }
                            InputModel::pushUndoState(state);
                            InputModel::insertAtCursor(state, InputModel::filteredText(event.pasteText, allowMultiline));
                            changed = true;
                        }
                        if (event.enter) {
                            if (allowMultiline) {
                                InputModel::pushUndoState(state);
                                InputModel::insertAtCursor(state, "\n");
                                changed = true;
                            } else if (onEnter) {
                                onEnter();
                            }
                        }
                        if (event.escape && onEnter) {
                            onEnter();
                        }
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

                if (hasSelection) {
                    for (size_t index = 0; index < layout.selectionRects.size(); ++index) {
                        const auto& selectionRect = layout.selectionRects[index];
                        ui_.rect(id_ + ".selection." + std::to_string(index))
                            .position(selectionRect.x, selectionRect.y)
                            .size(selectionRect.width, selectionRect.height)
                            .color(theme::withAlpha(style_.cursor, 0.24f))
                            .radius(multiline_ ? 0.0f : 3.0f)
                            .build();
                    }
                }

                ui_.text(id_ + ".text")
                    .position(inset_ - state.horizontalScroll, textY - state.verticalScroll)
                    .size(layout.visibleTextWidth, renderedTextHeight)
                    .dirtyKey(textDirtyKey)
                    .text(empty ? placeholder_ : state.text)
                    .fontSize(fontSize_)
                    .fontFamily(fontFamily_)
                    .lineHeight(textLineHeight)
                    .color(empty ? style_.placeholder : style_.text)
                    .wrap(multiline_)
                    .verticalAlign(core::VerticalAlign::Top)
                    .build();

                if (hasComposition) {
                    ui_.rect(id_ + ".composition.bg")
                        .position(compositionX, layout.cursorY)
                        .size(compositionWidth, textLineHeight)
                        .color(theme::withAlpha(style_.focused, 0.82f))
                        .radius(2.0f)
                        .build();

                    ui_.text(id_ + ".composition")
                        .position(compositionX + compositionPadding, layout.cursorY)
                        .size(std::max(1.0f, compositionWidth - compositionPadding * 2.0f), textLineHeight)
                        .dirtyKey(compositionDirtyKey)
                        .text(state.compositionText)
                        .fontSize(fontSize_)
                        .fontFamily(fontFamily_)
                        .lineHeight(textLineHeight)
                        .color(style_.text)
                        .wrap(false)
                        .verticalAlign(core::VerticalAlign::Top)
                        .build();
                }

                if (focused) {
                    ui_.rect(id_ + ".cursor")
                        .position(layout.clampedCursorX(), layout.cursorY)
                        .size(1.5f, fontSize_ * 1.18f)
                        .color(style_.cursor)
                        .radius(1.0f)
                        .build();
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
    InputStyle style_;
    core::Transition transition_ = core::Transition::make(0.16f, core::Ease::OutCubic);
    std::function<void(const std::string&)> onChange_;
    std::function<void()> onEnter_;
    std::function<void(bool)> onFocus_;
    std::string text_;
    std::string placeholder_ = "Hello EUI-NEO 😉";
    bool multiline_ = false;
    float width_ = 260.0f;
    float height_ = 40.0f;
    float inset_ = 12.0f;
    float fontSize_ = 17.0f;
    std::string fontFamily_ = "monospace";
};

inline InputBuilder input(core::dsl::Ui& ui, const std::string& id) {
    return InputBuilder(ui, id);
}

} // namespace components
