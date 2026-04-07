#pragma once

#include "../EUINEO.h"
#include "../ui/UIBuilder.h"
#include "../ui/ThemeTokens.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace EUINEO {

class InputBoxNode : public UINode {
public:
    class Builder : public UIBuilderBase<InputBoxNode, Builder> {
    public:
        Builder(UIContext& context, InputBoxNode& node) : UIBuilderBase<InputBoxNode, Builder>(context, node) {}

        Builder& placeholder(std::string value) {
            this->node_.trackComposeValue("placeholder", value);
            this->node_.placeholder_ = std::move(value);
            return *this;
        }

        Builder& text(std::string value) {
            this->node_.trackComposeValue("text", value);
            this->node_.text_ = std::move(value);
            return *this;
        }

        Builder& fontSize(float value) {
            this->node_.trackComposeValue("fontSize", value);
            this->node_.fontSize_ = value;
            return *this;
        }

        Builder& multiline(bool value) {
            this->node_.trackComposeValue("multiline", value);
            this->node_.multiline_ = value;
            return *this;
        }

        Builder& onChange(std::function<void(const std::string&)> handler) {
            this->node_.onChange_ = std::move(handler);
            return *this;
        }

        Builder& onEnter(std::function<void()> handler) {
            this->node_.onEnter_ = std::move(handler);
            return *this;
        }
    };

    explicit InputBoxNode(const std::string& key) : UINode(key) {
        resetDefaults();
    }

    static constexpr const char* StaticTypeName() {
        return "InputBoxNode";
    }

    const char* typeName() const override {
        return StaticTypeName();
    }

    bool wantsContinuousUpdate() const override {
        return isFocused_ ||
               (hoverAnim_ > 0.001f && hoverAnim_ < 0.999f) ||
               (focusAnim_ > 0.001f && focusAnim_ < 0.999f);
    }

    void update() override {
        cursorPosition_ = ClampUtf8Boundary(text_, std::clamp(cursorPosition_, 0, static_cast<int>(text_.size())));
        ensureUndoInitialized();
        const bool isHovered = hovered();

        if (!primitive_.enabled && isFocused_) {
            isFocused_ = false;
            clearSelection();
            requestRepaint(4.0f);
        }

        if (animateTowards(hoverAnim_, isHovered ? 1.0f : 0.0f, State.deltaTime * 15.0f)) {
            requestRepaint(4.0f);
        }

        if (animateTowards(focusAnim_, isFocused_ ? 1.0f : 0.0f, State.deltaTime * 15.0f)) {
            requestRepaint(4.0f);
        }

        if (State.mouseClicked) {
            const bool nextFocus = primitive_.enabled && isHovered;
            if (isFocused_ != nextFocus) {
                isFocused_ = nextFocus;
                cursorBlinkTime_ = 0.0f;
                cursorVisible_ = true;
                requestRepaint(4.0f);
            }
            if (nextFocus) {
                cursorPosition_ = hitTestCursor(State.mouseX, State.mouseY);
                clearSelection();
                dragAnchor_ = cursorPosition_;
                selectingWithMouse_ = true;
                preferredColumnX_ = -1.0f;
                syncSingleLineScroll();
                requestRepaint(4.0f);
            } else {
                clearSelection();
                selectingWithMouse_ = false;
            }
        }

        if (!State.mouseDown) {
            selectingWithMouse_ = false;
        }

        if (isFocused_ && State.mouseDown) {
            if (!selectingWithMouse_ && isHovered) {
                cursorPosition_ = hitTestCursor(State.mouseX, State.mouseY);
                clearSelection();
                dragAnchor_ = cursorPosition_;
                selectingWithMouse_ = true;
                preferredColumnX_ = -1.0f;
                syncSingleLineScroll();
                requestRepaint(4.0f);
            }
            if (selectingWithMouse_) {
                const int nextCursor = hitTestCursor(State.mouseX, State.mouseY);
                if (nextCursor != cursorPosition_ || selectionStart_ != dragAnchor_ || selectionEnd_ != nextCursor) {
                    cursorPosition_ = nextCursor;
                    selectionStart_ = dragAnchor_;
                    selectionEnd_ = cursorPosition_;
                    preferredColumnX_ = -1.0f;
                    syncSingleLineScroll();
                    requestRepaint(4.0f);
                }
            }
        }

        if (isFocused_) {
            bool textChanged = false;
            bool undoStateRestored = false;
            cursorBlinkTime_ += State.deltaTime;
            if (cursorBlinkTime_ >= 1.0f) {
                cursorBlinkTime_ = std::fmod(cursorBlinkTime_, 1.0f);
            }

            const bool nextCursorVisible = cursorBlinkTime_ < 0.5f;
            if (cursorVisible_ != nextCursorVisible) {
                cursorVisible_ = nextCursorVisible;
                requestRepaint(4.0f);
            }

            const bool ctrl = ctrlDown();
            const bool shift = shiftDown();
            const bool comboA = ctrl && (State.keys[GLFW_KEY_A] || State.keysPressed[GLFW_KEY_A]);
            const bool comboC = ctrl && (State.keys[GLFW_KEY_C] || State.keysPressed[GLFW_KEY_C]);
            const bool comboX = ctrl && (State.keys[GLFW_KEY_X] || State.keysPressed[GLFW_KEY_X]);
            const bool comboV = ctrl && (State.keys[GLFW_KEY_V] || State.keysPressed[GLFW_KEY_V]);
            const bool comboZ = ctrl && (State.keys[GLFW_KEY_Z] || State.keysPressed[GLFW_KEY_Z]);
            const bool comboY = ctrl && (State.keys[GLFW_KEY_Y] || State.keysPressed[GLFW_KEY_Y]);
            const bool ctrlA = comboA && !comboDownA_;
            const bool ctrlC = comboC && !comboDownC_;
            const bool ctrlX = comboX && !comboDownX_;
            const bool ctrlV = comboV && !comboDownV_;
            const bool ctrlZ = comboZ && !comboDownZ_;
            const bool ctrlY = comboY && !comboDownY_;
            comboDownA_ = comboA;
            comboDownC_ = comboC;
            comboDownX_ = comboX;
            comboDownV_ = comboV;
            comboDownZ_ = comboZ;
            comboDownY_ = comboY;
            if (ctrlA || ctrlC || ctrlX || ctrlV || ctrlZ || ctrlY) {
                suppressTextInputFrames_ = 2;
            } else if (suppressTextInputFrames_ > 0) {
                --suppressTextInputFrames_;
            }

            if (ctrlA) {
                selectionStart_ = 0;
                selectionEnd_ = static_cast<int>(text_.size());
                cursorPosition_ = selectionEnd_;
                dragAnchor_ = selectionStart_;
                preferredColumnX_ = -1.0f;
                requestRepaint(4.0f);
            }
            if (ctrlC && hasSelection()) {
                const std::pair<int, int> range = selectionRange();
                if (GLFWwindow* window = glfwGetCurrentContext()) {
                    glfwSetClipboardString(window, text_.substr(static_cast<std::size_t>(range.first), static_cast<std::size_t>(range.second - range.first)).c_str());
                }
                requestRepaint(4.0f);
            }
            if (ctrlX && hasSelection()) {
                const std::pair<int, int> range = selectionRange();
                if (GLFWwindow* window = glfwGetCurrentContext()) {
                    glfwSetClipboardString(window, text_.substr(static_cast<std::size_t>(range.first), static_cast<std::size_t>(range.second - range.first)).c_str());
                }
                eraseSelection();
                textChanged = true;
            }
            if (ctrlV) {
                if (GLFWwindow* window = glfwGetCurrentContext()) {
                    const char* clip = glfwGetClipboardString(window);
                    if (clip != nullptr) {
                        std::string pasted(clip);
                        normalizeLineBreaks(pasted);
                        if (!multilineEnabled()) {
                            pasted.erase(std::remove(pasted.begin(), pasted.end(), '\n'), pasted.end());
                        }
                        if (!pasted.empty()) {
                            insertAtCursor(pasted);
                            textChanged = true;
                        }
                    }
                }
            }
            if (ctrlZ) {
                const bool redo = shift || State.keys[GLFW_KEY_RIGHT_SHIFT] || State.keys[GLFW_KEY_LEFT_SHIFT];
                if (restoreUndoState(redo ? 1 : -1)) {
                    undoStateRestored = true;
                }
            }
            if (ctrlY) {
                if (restoreUndoState(1)) {
                    undoStateRestored = true;
                }
            }

            if (State.keysPressed[GLFW_KEY_LEFT]) {
                moveCursorHorizontal(-1, shift);
            }
            if (State.keysPressed[GLFW_KEY_RIGHT]) {
                moveCursorHorizontal(1, shift);
            }
            if (State.keysPressed[GLFW_KEY_HOME]) {
                moveCursorHome(shift);
            }
            if (State.keysPressed[GLFW_KEY_END]) {
                moveCursorEnd(shift);
            }
            if (State.keysPressed[GLFW_KEY_UP]) {
                moveCursorVertical(-1, shift);
            }
            if (State.keysPressed[GLFW_KEY_DOWN]) {
                moveCursorVertical(1, shift);
            }

            if (State.keysPressed[GLFW_KEY_BACKSPACE]) {
                if (hasSelection()) {
                    eraseSelection();
                    textChanged = true;
                } else if (cursorPosition_ > 0) {
                    const int previous = PrevUtf8Index(text_, cursorPosition_);
                    text_.erase(static_cast<std::size_t>(previous), static_cast<std::size_t>(cursorPosition_ - previous));
                    cursorPosition_ = previous;
                    clearSelection();
                    textChanged = true;
                }
            }
            if (State.keysPressed[GLFW_KEY_DELETE]) {
                if (hasSelection()) {
                    eraseSelection();
                    textChanged = true;
                } else if (cursorPosition_ < static_cast<int>(text_.size())) {
                    const int next = NextUtf8Index(text_, cursorPosition_);
                    text_.erase(static_cast<std::size_t>(cursorPosition_), static_cast<std::size_t>(next - cursorPosition_));
                    clearSelection();
                    textChanged = true;
                }
            }

            if (State.keysPressed[GLFW_KEY_ENTER] || State.keysPressed[GLFW_KEY_KP_ENTER]) {
                if (multilineEnabled() && !ctrl) {
                    insertAtCursor("\n");
                    textChanged = true;
                } else if (onEnter_) {
                    onEnter_();
                }
                requestRepaint(4.0f);
            }

            if (!State.textInput.empty() && !ctrl && suppressTextInputFrames_ <= 0) {
                std::string incoming = State.textInput;
                normalizeLineBreaks(incoming);
                if (!multilineEnabled()) {
                    incoming.erase(std::remove(incoming.begin(), incoming.end(), '\n'), incoming.end());
                }
                if (!incoming.empty()) {
                    insertAtCursor(incoming);
                    textChanged = true;
                    State.textInput.clear();
                }
            }

            if (textChanged) {
                pushUndoState();
            }
            if (textChanged || undoStateRestored || State.keysPressed[GLFW_KEY_LEFT] || State.keysPressed[GLFW_KEY_RIGHT] ||
                State.keysPressed[GLFW_KEY_HOME] || State.keysPressed[GLFW_KEY_END]) {
                syncSingleLineScroll();
            }
            if (textChanged || undoStateRestored) {
                if (onChange_) {
                    onChange_(text_);
                }
                cursorBlinkTime_ = 0.0f;
                cursorVisible_ = true;
                requestRepaint(4.0f);
            }
        } else {
            cursorBlinkTime_ = 0.0f;
            cursorVisible_ = true;
        }
    }

    void draw() override {
        PrimitiveClipScope clip(primitive_);
        const RectFrame frame = PrimitiveFrame(primitive_);
        const UIFieldVisualTokens visuals = CurrentFieldVisuals();
        DrawFieldChrome(primitive_, hoverAnim_, focusAnim_, primitive_.rounding);

        const float textScale = fontSize_ / 24.0f;
        const float baseTextX = frame.x + visuals.horizontalInset;
        const float textX = multilineEnabled() ? baseTextX : baseTextX - horizontalScrollX_;
        const float lineHeight = std::max(fontSize_ + 8.0f, fontSize_ * 1.35f);
        const bool multiline = multilineEnabled();
        const float firstLineBaseline = multiline
            ? frame.y + visuals.horizontalInset + fontSize_
            : frame.y + frame.height * 0.5f + (fontSize_ / 4.0f);

        if (hasSelection()) {
            drawSelection(textX, firstLineBaseline, lineHeight, textScale, multiline);
        }

        if (text_.empty()) {
            if (!placeholder_.empty()) {
                Color placeholderColor = CurrentTheme->text;
                placeholderColor.a = 0.5f;
                Renderer::DrawTextStr(placeholder_, baseTextX, firstLineBaseline, ApplyOpacity(placeholderColor, primitive_.opacity), textScale);
            }
        } else {
            if (multiline) {
                std::vector<int> starts = lineStarts();
                for (std::size_t line = 0; line < starts.size(); ++line) {
                    const int start = starts[line];
                    const int end = lineEnd(start);
                    const std::string segment = text_.substr(static_cast<std::size_t>(start), static_cast<std::size_t>(end - start));
                    const float y = firstLineBaseline + static_cast<float>(line) * lineHeight;
                    Renderer::DrawTextStr(segment, textX, y, ApplyOpacity(CurrentTheme->text, primitive_.opacity), textScale);
                }
            } else {
                Renderer::DrawTextStr(text_, textX, firstLineBaseline, ApplyOpacity(CurrentTheme->text, primitive_.opacity), textScale);
            }
        }

        if (isFocused_) {
            const CursorVisual cursor = cursorVisual(textX, firstLineBaseline, lineHeight, textScale, multiline);
            State.imeCursorActive = true;
            State.imeCursorX = cursor.x;
            State.imeCursorY = cursor.y;
            State.imeCursorH = cursor.h;
            if (!cursorVisible_) {
                return;
            }
            Renderer::DrawRect(
                cursor.x,
                cursor.y,
                2.0f,
                cursor.h,
                ApplyOpacity(CurrentTheme->primary, primitive_.opacity),
                1.0f
            );
        }
    }

protected:
    void resetDefaults() override {
        primitive_ = UIPrimitive{};
        primitive_.width = 220.0f;
        primitive_.height = 36.0f;
        placeholder_.clear();
        text_.clear();
        fontSize_ = 20.0f;
        multiline_ = false;
        onChange_ = {};
        onEnter_ = {};
    }

private:
    struct CursorVisual {
        float x = 0.0f;
        float y = 0.0f;
        float h = 0.0f;
    };

    struct UndoState {
        std::string text;
        int cursor = 0;
        int selectionStart = 0;
        int selectionEnd = 0;
    };

    static int PrevUtf8Index(const std::string& value, int index) {
        int out = std::clamp(index, 0, static_cast<int>(value.size()));
        if (out <= 0) {
            return 0;
        }
        --out;
        while (out > 0 && (static_cast<unsigned char>(value[static_cast<std::size_t>(out)]) & 0xC0U) == 0x80U) {
            --out;
        }
        return out;
    }

    static int NextUtf8Index(const std::string& value, int index) {
        int out = std::clamp(index, 0, static_cast<int>(value.size()));
        if (out >= static_cast<int>(value.size())) {
            return static_cast<int>(value.size());
        }
        ++out;
        while (out < static_cast<int>(value.size()) && (static_cast<unsigned char>(value[static_cast<std::size_t>(out)]) & 0xC0U) == 0x80U) {
            ++out;
        }
        return out;
    }

    static int ClampUtf8Boundary(const std::string& value, int index) {
        int out = std::clamp(index, 0, static_cast<int>(value.size()));
        while (out > 0 && out < static_cast<int>(value.size()) &&
               (static_cast<unsigned char>(value[static_cast<std::size_t>(out)]) & 0xC0U) == 0x80U) {
            --out;
        }
        return out;
    }

    static void normalizeLineBreaks(std::string& value) {
        std::string normalized;
        normalized.reserve(value.size());
        for (std::size_t index = 0; index < value.size(); ++index) {
            const char c = value[index];
            if (c == '\r') {
                if (index + 1 < value.size() && value[index + 1] == '\n') {
                    ++index;
                }
                normalized.push_back('\n');
            } else {
                normalized.push_back(c);
            }
        }
        value.swap(normalized);
    }

    bool multilineEnabled() const {
        return multiline_ || primitive_.height >= 56.0f;
    }

    bool ctrlDown() const {
        return State.keys[GLFW_KEY_LEFT_CONTROL] || State.keys[GLFW_KEY_RIGHT_CONTROL] ||
               State.keysPressed[GLFW_KEY_LEFT_CONTROL] || State.keysPressed[GLFW_KEY_RIGHT_CONTROL];
    }

    bool shiftDown() const {
        return State.keys[GLFW_KEY_LEFT_SHIFT] || State.keys[GLFW_KEY_RIGHT_SHIFT];
    }

    std::pair<int, int> selectionRange() const {
        return {std::min(selectionStart_, selectionEnd_), std::max(selectionStart_, selectionEnd_)};
    }

    bool hasSelection() const {
        return selectionStart_ != selectionEnd_;
    }

    void clearSelection() {
        selectionStart_ = cursorPosition_;
        selectionEnd_ = cursorPosition_;
        dragAnchor_ = cursorPosition_;
    }

    void eraseSelection() {
        const std::pair<int, int> range = selectionRange();
        if (range.first == range.second) {
            return;
        }
        text_.erase(static_cast<std::size_t>(range.first), static_cast<std::size_t>(range.second - range.first));
        cursorPosition_ = range.first;
        clearSelection();
        preferredColumnX_ = -1.0f;
    }

    void insertAtCursor(const std::string& value) {
        if (value.empty()) {
            return;
        }
        if (hasSelection()) {
            eraseSelection();
        }
        text_.insert(static_cast<std::size_t>(cursorPosition_), value);
        cursorPosition_ += static_cast<int>(value.size());
        enforceSoftWrapAtCursor();
        clearSelection();
        preferredColumnX_ = -1.0f;
    }

    void enforceSoftWrapAtCursor() {
        if (!multilineEnabled()) {
            return;
        }
        const RectFrame frame = PrimitiveFrame(primitive_);
        const UIFieldVisualTokens visuals = CurrentFieldVisuals();
        const float textScale = fontSize_ / 24.0f;
        const float maxWidth = std::max(24.0f, frame.width - visuals.horizontalInset * 2.0f);
        if (maxWidth <= 24.0f) {
            return;
        }

        int guard = 0;
        while (guard < 24) {
            ++guard;
            int lineStart = cursorPosition_;
            while (lineStart > 0 && text_[static_cast<std::size_t>(lineStart - 1)] != '\n') {
                --lineStart;
            }
            int lineEndIndex = lineEnd(lineStart);
            if (measureWidth(lineStart, lineEndIndex, textScale) <= maxWidth) {
                break;
            }

            int breakPos = -1;
            int scan = cursorPosition_;
            while (scan > lineStart) {
                scan = PrevUtf8Index(text_, scan);
                if (scan > lineStart && (text_[static_cast<std::size_t>(scan)] == ' ' || text_[static_cast<std::size_t>(scan)] == '\t')) {
                    breakPos = scan;
                    break;
                }
            }
            if (breakPos <= lineStart) {
                int probe = lineStart;
                float width = 0.0f;
                while (probe < lineEndIndex) {
                    const int next = NextUtf8Index(text_, probe);
                    const float nextWidth = width + glyphAdvance(probe, next, textScale);
                    if (nextWidth > maxWidth) {
                        breakPos = probe;
                        break;
                    }
                    width = nextWidth;
                    probe = next;
                }
                if (breakPos <= lineStart) {
                    break;
                }
            }

            if (breakPos < static_cast<int>(text_.size()) && (text_[static_cast<std::size_t>(breakPos)] == ' ' || text_[static_cast<std::size_t>(breakPos)] == '\t')) {
                text_[static_cast<std::size_t>(breakPos)] = '\n';
            } else {
                text_.insert(static_cast<std::size_t>(breakPos), "\n");
                if (cursorPosition_ >= breakPos) {
                    ++cursorPosition_;
                }
                ++lineEndIndex;
            }

            int trimPos = breakPos + 1;
            while (trimPos < static_cast<int>(text_.size()) && (text_[static_cast<std::size_t>(trimPos)] == ' ' || text_[static_cast<std::size_t>(trimPos)] == '\t')) {
                text_.erase(static_cast<std::size_t>(trimPos), 1);
                if (cursorPosition_ > trimPos) {
                    --cursorPosition_;
                }
            }
        }
    }

    std::vector<int> lineStarts() const {
        std::vector<int> starts;
        starts.push_back(0);
        for (int index = 0; index < static_cast<int>(text_.size()); ++index) {
            if (text_[static_cast<std::size_t>(index)] == '\n' && index + 1 <= static_cast<int>(text_.size())) {
                starts.push_back(index + 1);
            }
        }
        return starts;
    }

    int lineEnd(int start) const {
        for (int index = start; index < static_cast<int>(text_.size()); ++index) {
            if (text_[static_cast<std::size_t>(index)] == '\n') {
                return index;
            }
        }
        return static_cast<int>(text_.size());
    }

    float measureWidth(int start, int end, float textScale) const {
        if (end <= start) {
            return 0.0f;
        }
        float width = 0.0f;
        int index = start;
        while (index < end) {
            const int next = NextUtf8Index(text_, index);
            width += glyphAdvance(index, next, textScale);
            index = next;
        }
        return width;
    }

    int hitTestInLine(int start, int end, float localX, float textScale) const {
        float cursorX = 0.0f;
        int index = start;
        while (index < end) {
            const int next = NextUtf8Index(text_, index);
            const float w = glyphAdvance(index, next, textScale);
            if (localX < cursorX + w * 0.5f) {
                return index;
            }
            cursorX += w;
            index = next;
        }
        return end;
    }

    CursorVisual cursorVisual(float textX, float firstLineBaseline, float lineHeight, float textScale, bool multiline) const {
        CursorVisual visual;
        const int cursor = ClampUtf8Boundary(text_, cursorPosition_);
        if (!multiline) {
            visual.x = textX + measureWidth(0, cursor, textScale);
            visual.y = firstLineBaseline - fontSize_;
            visual.h = std::max(16.0f, fontSize_ + 4.0f);
            return visual;
        }

        const std::vector<int> starts = lineStarts();
        std::size_t lineIndex = 0;
        for (std::size_t i = 0; i < starts.size(); ++i) {
            const int start = starts[i];
            const int end = lineEnd(start);
            if (cursor >= start && cursor <= end) {
                lineIndex = i;
                break;
            }
        }
        const int start = starts[lineIndex];
        visual.x = textX + measureWidth(start, cursor, textScale);
        visual.y = firstLineBaseline + static_cast<float>(lineIndex) * lineHeight - fontSize_;
        visual.h = std::max(16.0f, fontSize_ + 4.0f);
        return visual;
    }

    int hitTestCursor(float mouseX, float mouseY) const {
        const RectFrame frame = PrimitiveFrame(primitive_);
        const UIFieldVisualTokens visuals = CurrentFieldVisuals();
        const float textScale = fontSize_ / 24.0f;
        const float textX = frame.x + visuals.horizontalInset;
        const bool multiline = multilineEnabled();
        const float lineHeight = std::max(fontSize_ + 8.0f, fontSize_ * 1.35f);
        const float firstLineBaseline = multiline
            ? frame.y + visuals.horizontalInset + fontSize_
            : frame.y + frame.height * 0.5f + (fontSize_ / 4.0f);

        if (!multiline) {
            const float localX = mouseX - textX + horizontalScrollX_;
            return hitTestInLine(0, static_cast<int>(text_.size()), localX, textScale);
        }

        const std::vector<int> starts = lineStarts();
        const float topY = firstLineBaseline - fontSize_;
        const int line = std::clamp(static_cast<int>((mouseY - topY) / lineHeight), 0, static_cast<int>(starts.size()) - 1);
        const int start = starts[static_cast<std::size_t>(line)];
        const int end = lineEnd(start);
        return hitTestInLine(start, end, mouseX - textX, textScale);
    }

    void drawSelection(float textX, float firstLineBaseline, float lineHeight, float textScale, bool multiline) const {
        if (!hasSelection()) {
            return;
        }
        const std::pair<int, int> range = selectionRange();
        Color selectionColor = CurrentTheme->primary;
        selectionColor.a = 0.22f;

        if (!multiline) {
            const float x1 = textX + measureWidth(0, range.first, textScale);
            const float x2 = textX + measureWidth(0, range.second, textScale);
            Renderer::DrawRect(x1, firstLineBaseline - fontSize_, std::max(1.0f, x2 - x1), std::max(16.0f, fontSize_ + 4.0f),
                               ApplyOpacity(selectionColor, primitive_.opacity), 3.0f);
            return;
        }

        const std::vector<int> starts = lineStarts();
        for (std::size_t i = 0; i < starts.size(); ++i) {
            const int lineStart = starts[i];
            const int lineEndIndex = lineEnd(lineStart);
            const int drawStart = std::max(lineStart, range.first);
            const int drawEnd = std::min(lineEndIndex, range.second);
            if (drawEnd <= drawStart) {
                continue;
            }
            const float x1 = textX + measureWidth(lineStart, drawStart, textScale);
            const float x2 = textX + measureWidth(lineStart, drawEnd, textScale);
            const float y = firstLineBaseline + static_cast<float>(i) * lineHeight - fontSize_;
            Renderer::DrawRect(x1, y, std::max(1.0f, x2 - x1), std::max(16.0f, fontSize_ + 4.0f),
                               ApplyOpacity(selectionColor, primitive_.opacity), 3.0f);
        }
    }

    void moveCursorHorizontal(int direction, bool keepSelection) {
        const int next = direction < 0 ? PrevUtf8Index(text_, cursorPosition_) : NextUtf8Index(text_, cursorPosition_);
        if (keepSelection) {
            if (!hasSelection()) {
                selectionStart_ = cursorPosition_;
            }
            cursorPosition_ = next;
            selectionEnd_ = cursorPosition_;
        } else {
            cursorPosition_ = next;
            clearSelection();
        }
        preferredColumnX_ = -1.0f;
        requestRepaint(4.0f);
    }

    void moveCursorHome(bool keepSelection) {
        const int previousCursor = cursorPosition_;
        if (!multilineEnabled()) {
            cursorPosition_ = 0;
        } else {
            const std::vector<int> starts = lineStarts();
            for (std::size_t i = 0; i < starts.size(); ++i) {
                const int start = starts[i];
                const int end = lineEnd(start);
                if (cursorPosition_ >= start && cursorPosition_ <= end) {
                    cursorPosition_ = start;
                    break;
                }
            }
        }
        if (keepSelection) {
            if (!hasSelection()) {
                selectionStart_ = previousCursor;
            }
            selectionEnd_ = cursorPosition_;
        } else {
            clearSelection();
        }
        preferredColumnX_ = -1.0f;
        requestRepaint(4.0f);
    }

    void moveCursorEnd(bool keepSelection) {
        const int previousCursor = cursorPosition_;
        if (!multilineEnabled()) {
            cursorPosition_ = static_cast<int>(text_.size());
        } else {
            const std::vector<int> starts = lineStarts();
            for (std::size_t i = 0; i < starts.size(); ++i) {
                const int start = starts[i];
                const int end = lineEnd(start);
                if (cursorPosition_ >= start && cursorPosition_ <= end) {
                    cursorPosition_ = end;
                    break;
                }
            }
        }
        if (keepSelection) {
            if (!hasSelection()) {
                selectionStart_ = previousCursor;
            }
            selectionEnd_ = cursorPosition_;
        } else {
            clearSelection();
        }
        preferredColumnX_ = -1.0f;
        requestRepaint(4.0f);
    }

    void moveCursorVertical(int direction, bool keepSelection) {
        if (!multilineEnabled()) {
            return;
        }
        const int previousCursor = cursorPosition_;
        const std::vector<int> starts = lineStarts();
        if (starts.empty()) {
            return;
        }

        std::size_t lineIndex = 0;
        for (std::size_t i = 0; i < starts.size(); ++i) {
            const int start = starts[i];
            const int end = lineEnd(start);
            if (cursorPosition_ >= start && cursorPosition_ <= end) {
                lineIndex = i;
                break;
            }
        }
        const int targetLine = std::clamp(static_cast<int>(lineIndex) + direction, 0, static_cast<int>(starts.size()) - 1);
        const float textScale = fontSize_ / 24.0f;
        if (preferredColumnX_ < 0.0f) {
            preferredColumnX_ = measureWidth(starts[lineIndex], cursorPosition_, textScale);
        }
        const int targetStart = starts[static_cast<std::size_t>(targetLine)];
        const int targetEnd = lineEnd(targetStart);
        cursorPosition_ = hitTestInLine(targetStart, targetEnd, preferredColumnX_, textScale);
        if (keepSelection) {
            if (!hasSelection()) {
                selectionStart_ = previousCursor;
            }
            selectionEnd_ = cursorPosition_;
        } else {
            clearSelection();
        }
        requestRepaint(4.0f);
    }

    void ensureUndoInitialized() {
        if (!undoStates_.empty()) {
            return;
        }
        undoStates_.push_back({text_, cursorPosition_, selectionStart_, selectionEnd_});
        undoIndex_ = 0;
    }

    void pushUndoState() {
        ensureUndoInitialized();
        UndoState current{text_, cursorPosition_, selectionStart_, selectionEnd_};
        if (undoIndex_ >= 0 && undoIndex_ < static_cast<int>(undoStates_.size())) {
            const UndoState& active = undoStates_[static_cast<std::size_t>(undoIndex_)];
            if (active.text == current.text && active.cursor == current.cursor &&
                active.selectionStart == current.selectionStart && active.selectionEnd == current.selectionEnd) {
                return;
            }
        }
        if (undoIndex_ + 1 < static_cast<int>(undoStates_.size())) {
            undoStates_.erase(undoStates_.begin() + undoIndex_ + 1, undoStates_.end());
        }
        undoStates_.push_back(current);
        if (undoStates_.size() > 160) {
            undoStates_.erase(undoStates_.begin());
        }
        undoIndex_ = static_cast<int>(undoStates_.size()) - 1;
    }

    bool restoreUndoState(int step) {
        ensureUndoInitialized();
        if (undoStates_.empty()) {
            return false;
        }
        const int next = std::clamp(undoIndex_ + step, 0, static_cast<int>(undoStates_.size()) - 1);
        if (next == undoIndex_) {
            return false;
        }
        const std::string previous = text_;
        undoIndex_ = next;
        const UndoState& state = undoStates_[static_cast<std::size_t>(undoIndex_)];
        text_ = state.text;
        cursorPosition_ = ClampUtf8Boundary(text_, state.cursor);
        selectionStart_ = ClampUtf8Boundary(text_, state.selectionStart);
        selectionEnd_ = ClampUtf8Boundary(text_, state.selectionEnd);
        dragAnchor_ = selectionStart_;
        preferredColumnX_ = -1.0f;
        cursorBlinkTime_ = 0.0f;
        cursorVisible_ = true;
        syncSingleLineScroll();
        requestRepaint(4.0f);
        return previous != text_;
    }

    float glyphAdvance(int start, int end, float textScale) const {
        if (end <= start) {
            return 0.0f;
        }
        if (end == start + 1 && text_[static_cast<std::size_t>(start)] == ' ') {
            return fontSize_ * 0.3f;
        }
        return Renderer::MeasureTextWidth(text_.substr(static_cast<std::size_t>(start), static_cast<std::size_t>(end - start)), textScale);
    }

    void syncSingleLineScroll() {
        if (multilineEnabled()) {
            horizontalScrollX_ = 0.0f;
            return;
        }
        const RectFrame frame = PrimitiveFrame(primitive_);
        const UIFieldVisualTokens visuals = CurrentFieldVisuals();
        const float textScale = fontSize_ / 24.0f;
        const float viewportWidth = std::max(1.0f, frame.width - visuals.horizontalInset * 2.0f);
        const float textWidth = measureWidth(0, static_cast<int>(text_.size()), textScale);
        const float cursorPixel = measureWidth(0, cursorPosition_, textScale);
        if (textWidth <= viewportWidth) {
            horizontalScrollX_ = 0.0f;
            return;
        }
        const float rightSafe = std::max(6.0f, viewportWidth - 10.0f);
        if (cursorPixel - horizontalScrollX_ < 0.0f) {
            horizontalScrollX_ = cursorPixel;
        } else if (cursorPixel - horizontalScrollX_ > rightSafe) {
            horizontalScrollX_ = cursorPixel - rightSafe;
        }
        horizontalScrollX_ = std::clamp(horizontalScrollX_, 0.0f, std::max(0.0f, textWidth - viewportWidth + 10.0f));
    }

    void requestRepaint(float expand = 4.0f, float duration = 0.0f) {
        (void)expand;
        requestVisualRepaint(duration > 0.0f ? duration : 0.18f);
    }

    std::string placeholder_;
    std::string text_;
    float fontSize_ = 20.0f;
    bool multiline_ = false;
    std::function<void(const std::string&)> onChange_;
    std::function<void()> onEnter_;
    bool isFocused_ = false;
    float cursorBlinkTime_ = 0.0f;
    int cursorPosition_ = 0;
    bool cursorVisible_ = true;
    float hoverAnim_ = 0.0f;
    float focusAnim_ = 0.0f;
    int selectionStart_ = 0;
    int selectionEnd_ = 0;
    int dragAnchor_ = 0;
    bool selectingWithMouse_ = false;
    float preferredColumnX_ = -1.0f;
    float horizontalScrollX_ = 0.0f;
    bool comboDownA_ = false;
    bool comboDownC_ = false;
    bool comboDownX_ = false;
    bool comboDownV_ = false;
    bool comboDownZ_ = false;
    bool comboDownY_ = false;
    int suppressTextInputFrames_ = 0;
    std::vector<UndoState> undoStates_;
    int undoIndex_ = -1;
};

} // namespace EUINEO
