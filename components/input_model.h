#pragma once

#include "core/dsl.h"
#include "core/render/text.h"
#include "core/window/window_backend.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace components::input_detail {

struct InputModel {
    struct TextLine {
        int start = 0;
        int end = 0;
        bool hardBreakAfter = false;
        core::TextPrimitive::TextMetrics metrics;
    };

    struct TextSelectionRect {
        float x = 0.0f;
        float y = 0.0f;
        float width = 0.0f;
        float height = 0.0f;
    };

    struct EditSnapshot {
        std::string text;
        int cursor = 0;
        int selectionStart = 0;
        int selectionEnd = 0;
    };

    struct InputState {
        std::string text;
        std::string compositionText;
        int cursor = 0;
        int selectionStart = 0;
        int selectionEnd = 0;
        int dragAnchor = 0;
        bool selecting = false;
        bool hasPreferredCursorX = false;
        float preferredCursorX = 0.0f;
        float horizontalScroll = 0.0f;
        float verticalScroll = 0.0f;
        unsigned long long textRevision = 0;
        unsigned long long compositionRevision = 0;
        core::Rect lastBounds;
        unsigned long long cachedTextRevision = static_cast<unsigned long long>(-1);
        std::string cachedFontFamily;
        float cachedFontSize = 0.0f;
        float cachedViewportWidth = -1.0f;
        bool cachedMultiline = false;
        core::TextPrimitive::TextMetrics cachedMetrics;
        std::vector<TextLine> cachedLines;
        float cachedTextWidth = 0.0f;
        bool layoutCacheValid = false;
        std::vector<EditSnapshot> undoStack;
        std::vector<EditSnapshot> redoStack;
    };

    struct InputLayout {
        using Line = TextLine;
        using SelectionRect = TextSelectionRect;

        core::TextPrimitive::TextMetrics metrics;
        const std::vector<Line>* lines = nullptr;
        std::vector<SelectionRect> selectionRects;
        float viewportWidth = 0.0f;
        float viewportHeight = 0.0f;
        float controlWidth = 0.0f;
        float inset = 0.0f;
        float textTop = 0.0f;
        float lineHeight = 0.0f;
        float scroll = 0.0f;
        float textWidth = 0.0f;
        float contentHeight = 0.0f;
        float cursorPixel = 0.0f;
        float cursorX = 0.0f;
        float cursorY = 0.0f;
        float visibleTextWidth = 0.0f;
        float maxVerticalScroll = 0.0f;
        int selectionStart = 0;
        int selectionEnd = 0;
        int cursorLine = 0;
        bool multiline = false;
        float clippedSelectionX = 0.0f;
        float clippedSelectionWidth = 0.0f;

        static InputLayout build(InputState& state,
                                 float viewportWidth,
                                 float viewportHeight,
                                 float controlWidth,
                                 float inset,
                                 float textTop,
                                 float lineHeight,
                                 const std::string& fontFamily,
                                 float fontSize,
                                 bool multiline) {
            InputLayout layout;
            layout.viewportWidth = viewportWidth;
            layout.viewportHeight = viewportHeight;
            layout.controlWidth = controlWidth;
            layout.inset = inset;
            layout.textTop = textTop;
            layout.lineHeight = lineHeight;
            layout.multiline = multiline;
            ensureLayoutCache(state, fontFamily, fontSize, viewportWidth, multiline);
            layout.lines = &state.cachedLines;

            if (multiline) {
                layout.cursorLine = layout.lineIndexFor(state.cursor);
                const Line& cursorLine = layout.lineList()[static_cast<size_t>(layout.cursorLine)];
                layout.metrics = cursorLine.metrics;
                state.horizontalScroll = 0.0f;
                layout.scroll = 0.0f;
                layout.textWidth = state.cachedTextWidth;
                layout.contentHeight = static_cast<float>(layout.lineList().size()) * lineHeight;
                layout.cursorPixel = caretX(cursorLine.metrics, state.cursor - cursorLine.start);
                layout.cursorX = inset + layout.cursorPixel;
                layout.maxVerticalScroll = std::max(0.0f, layout.contentHeight - viewportHeight);
                state.verticalScroll = std::clamp(state.verticalScroll, 0.0f, layout.maxVerticalScroll);
                syncVerticalScroll(state, layout.cursorLine, lineHeight, viewportHeight);
                state.verticalScroll = std::clamp(state.verticalScroll, 0.0f, layout.maxVerticalScroll);
                layout.currentVerticalScroll = state.verticalScroll;
                layout.cursorY = textTop + static_cast<float>(layout.cursorLine) * lineHeight - state.verticalScroll;
                layout.visibleTextWidth = viewportWidth;
            } else {
                layout.metrics = state.cachedMetrics;
                syncScroll(state, viewportWidth, layout.metrics, fontSize);
                state.verticalScroll = 0.0f;
                layout.cursorLine = 0;
                layout.scroll = state.horizontalScroll;
                layout.textWidth = layout.metrics.width;
                layout.contentHeight = lineHeight;
                layout.cursorPixel = caretX(layout.metrics, state.cursor);
                layout.cursorX = inset + layout.cursorPixel - layout.scroll;
                layout.cursorY = textTop;
                layout.currentVerticalScroll = 0.0f;
                layout.visibleTextWidth = std::max(viewportWidth, layout.textWidth + 24.0f);
            }

            const auto selection = selectionRange(state);
            layout.selectionStart = selection.first;
            layout.selectionEnd = selection.second;
            layout.buildSelectionRects(selection.first, selection.second);
            return layout;
        }

        const std::vector<Line>& lineList() const {
            static const std::vector<Line> emptyLines;
            return lines ? *lines : emptyLines;
        }

        float xFor(int byteIndex) const {
            const int lineIndex = lineIndexFor(byteIndex);
            const Line& line = lineList()[static_cast<size_t>(lineIndex)];
            return caretX(line.metrics, byteIndex - line.start);
        }

        float clampedCursorX() const {
            return std::clamp(cursorX, inset, std::max(inset, controlWidth - inset));
        }

        int cursorFromPointer(double pointerX, double pointerY, const core::Rect& bounds, float width, float inputInset) const {
            const float scale = width > 0.0f ? bounds.width / width : 1.0f;
            const float localX = static_cast<float>((pointerX - bounds.x) / std::max(0.001f, scale));
            const float localY = static_cast<float>((pointerY - bounds.y) / std::max(0.001f, scale));
            const int lineIndex = multiline ? lineIndexFromY(localY + currentVerticalScroll) : 0;
            return closestCaret(lineIndex, localX - inputInset + scroll);
        }

        int closestCaret(int lineIndex, float targetX) const {
            const std::vector<Line>& lineListRef = lineList();
            if (lineListRef.empty()) {
                return 0;
            }
            const Line& line = lineListRef[static_cast<size_t>(std::clamp(lineIndex, 0, static_cast<int>(lineListRef.size()) - 1))];
            if (line.metrics.byteIndices.empty() || line.metrics.caretX.empty()) {
                return line.start;
            }
            const size_t count = std::min(line.metrics.byteIndices.size(), line.metrics.caretX.size());
            int bestIndex = line.metrics.byteIndices.front();
            float bestDistance = std::fabs(targetX - line.metrics.caretX.front());
            for (size_t i = 1; i < count; ++i) {
                const float distance = std::fabs(targetX - line.metrics.caretX[i]);
                if (distance < bestDistance) {
                    bestDistance = distance;
                    bestIndex = line.metrics.byteIndices[i];
                }
            }
            return line.start + bestIndex;
        }

        int lineIndexFor(int byteIndex) const {
            const std::vector<Line>& lineListRef = lineList();
            if (lineListRef.empty()) {
                return 0;
            }
            const auto it = std::upper_bound(
                lineListRef.begin(),
                lineListRef.end(),
                byteIndex,
                [](int value, const Line& line) {
                    return value < line.start;
                });
            int index = it == lineListRef.begin()
                ? 0
                : static_cast<int>(std::distance(lineListRef.begin(), it)) - 1;
            index = std::clamp(index, 0, static_cast<int>(lineListRef.size()) - 1);
            if (index + 1 < static_cast<int>(lineListRef.size()) &&
                !lineListRef[static_cast<size_t>(index)].hardBreakAfter &&
                byteIndex >= lineListRef[static_cast<size_t>(index)].end) {
                ++index;
            }
            return std::clamp(index, 0, static_cast<int>(lineListRef.size()) - 1);
        }

        int lineIndexFromY(float localY) const {
            const std::vector<Line>& lineListRef = lineList();
            if (lineListRef.empty() || lineHeight <= 0.0f) {
                return 0;
            }
            const int line = static_cast<int>(std::floor((localY - textTop) / lineHeight));
            return std::clamp(line, 0, static_cast<int>(lineListRef.size()) - 1);
        }

        float currentVerticalScroll = 0.0f;

        void buildSelectionRects(int startIndex, int endIndex) {
            const std::vector<Line>& lineListRef = lineList();
            if (startIndex == endIndex || lineListRef.empty()) {
                return;
            }
            if (startIndex > endIndex) {
                std::swap(startIndex, endIndex);
            }

            const float clipLeft = inset;
            const float clipRight = std::max(inset, controlWidth - inset);
            const int firstSelectedLine = lineIndexFor(startIndex);
            const int lastSelectedLine = lineIndexFor(std::max(startIndex, endIndex - 1));
            const int firstVisibleLine = lineIndexFromY(textTop + currentVerticalScroll - lineHeight);
            const int lastVisibleLine = lineIndexFromY(textTop + currentVerticalScroll + viewportHeight + lineHeight);
            const int firstLine = std::max(firstSelectedLine, firstVisibleLine);
            const int lastLine = std::min(lastSelectedLine, lastVisibleLine);
            for (int lineIndex = firstLine; lineIndex <= lastLine; ++lineIndex) {
                const size_t i = static_cast<size_t>(lineIndex);
                const Line& line = lineListRef[i];
                const int selectableEnd = line.end + (line.hardBreakAfter ? 1 : 0);

                const int lineStart = std::clamp(startIndex, line.start, line.end);
                const int lineEnd = std::clamp(endIndex, line.start, line.end);
                const bool selectionContinuesPastLine = endIndex > line.end && startIndex <= line.end;
                if (lineStart == lineEnd && !selectionContinuesPastLine) {
                    continue;
                }

                const bool coversWholeLine = startIndex <= line.start && endIndex >= selectableEnd;
                const float startX = coversWholeLine ? 0.0f : caretX(line.metrics, lineStart - line.start);
                const float endX = selectionContinuesPastLine
                    ? (line.hardBreakAfter || coversWholeLine ? viewportWidth : std::max(line.metrics.width, startX + 1.0f))
                    : caretX(line.metrics, lineEnd - line.start);
                const float rawX = inset + startX - scroll;
                const float rawRight = inset + endX - scroll;
                const float clippedX = std::clamp(rawX, clipLeft, clipRight);
                const float clippedRight = std::clamp(rawRight, clipLeft, clipRight);
                const float width = std::max(1.0f, clippedRight - clippedX);
                const float y = textTop + static_cast<float>(i) * lineHeight - currentVerticalScroll;
                const float height = i + 1 < lineListRef.size() ? lineHeight + 1.0f : lineHeight;
                if (y + height < textTop || y > textTop + viewportHeight) {
                    continue;
                }
                selectionRects.push_back({clippedX, y, width, height});
            }
        }
    };

    static std::string filteredText(const std::string& input, bool multiline) {
        std::string output;
        for (char ch : input) {
            if (ch == '\r' || (!multiline && ch == '\n')) {
                continue;
            }
            output.push_back(ch);
        }
        return output;
    }

    static int clampUtf8Boundary(const std::string& value, int index) {
        int out = std::clamp(index, 0, static_cast<int>(value.size()));
        while (out > 0 && out < static_cast<int>(value.size()) &&
               (static_cast<unsigned char>(value[static_cast<std::size_t>(out)]) & 0xC0) == 0x80) {
            --out;
        }
        return out;
    }

    static std::pair<int, int> selectionRange(const InputState& state) {
        return {std::min(state.selectionStart, state.selectionEnd), std::max(state.selectionStart, state.selectionEnd)};
    }

    static bool hasTextSelection(const InputState& state) {
        return state.selectionStart != state.selectionEnd;
    }

    static void clearSelection(InputState& state) {
        state.selectionStart = state.cursor;
        state.selectionEnd = state.cursor;
        state.dragAnchor = state.cursor;
    }

    static EditSnapshot snapshotFor(const InputState& state) {
        return {state.text, state.cursor, state.selectionStart, state.selectionEnd};
    }

    static bool sameSnapshot(const EditSnapshot& snapshot, const InputState& state) {
        return snapshot.text == state.text &&
               snapshot.cursor == state.cursor &&
               snapshot.selectionStart == state.selectionStart &&
               snapshot.selectionEnd == state.selectionEnd;
    }

    static void restoreSnapshot(InputState& state, const EditSnapshot& snapshot) {
        state.text = snapshot.text;
        state.cursor = clampUtf8Boundary(state.text, snapshot.cursor);
        state.selectionStart = clampUtf8Boundary(state.text, snapshot.selectionStart);
        state.selectionEnd = clampUtf8Boundary(state.text, snapshot.selectionEnd);
        state.dragAnchor = state.cursor;
        state.hasPreferredCursorX = false;
        ++state.textRevision;
    }

    static void pushUndoState(InputState& state) {
        if (!state.undoStack.empty() && sameSnapshot(state.undoStack.back(), state)) {
            state.redoStack.clear();
            return;
        }
        constexpr size_t kMaxUndoDepth = 128;
        state.undoStack.push_back(snapshotFor(state));
        if (state.undoStack.size() > kMaxUndoDepth) {
            state.undoStack.erase(state.undoStack.begin());
        }
        state.redoStack.clear();
    }

    static bool undoEdit(InputState& state) {
        if (state.undoStack.empty()) {
            return false;
        }
        state.redoStack.push_back(snapshotFor(state));
        const EditSnapshot snapshot = state.undoStack.back();
        state.undoStack.pop_back();
        restoreSnapshot(state, snapshot);
        return true;
    }

    static bool redoEdit(InputState& state) {
        if (state.redoStack.empty()) {
            return false;
        }
        state.undoStack.push_back(snapshotFor(state));
        const EditSnapshot snapshot = state.redoStack.back();
        state.redoStack.pop_back();
        restoreSnapshot(state, snapshot);
        return true;
    }

    static void eraseSelection(InputState& state) {
        const auto range = selectionRange(state);
        if (range.first == range.second) {
            return;
        }
        state.text.erase(static_cast<std::size_t>(range.first), static_cast<std::size_t>(range.second - range.first));
        ++state.textRevision;
        state.cursor = range.first;
        clearSelection(state);
    }

    static void insertAtCursor(InputState& state, const std::string& value) {
        if (value.empty()) {
            return;
        }
        if (hasTextSelection(state)) {
            eraseSelection(state);
        }
        state.text.insert(static_cast<std::size_t>(state.cursor), value);
        ++state.textRevision;
        state.cursor += static_cast<int>(value.size());
        state.hasPreferredCursorX = false;
        clearSelection(state);
    }

    static void moveCursor(InputState& state,
                           int direction,
                           bool keepSelection,
                           const std::string& fontFamily,
                           float fontSize,
                           bool multiline,
                           float viewportWidth) {
        const int previous = state.cursor;
        if (!keepSelection && hasTextSelection(state)) {
            const auto range = selectionRange(state);
            state.cursor = direction < 0 ? range.first : range.second;
            state.hasPreferredCursorX = false;
            clearSelection(state);
            return;
        }
        state.cursor = direction < 0
            ? prevCursorIndex(state, fontFamily, fontSize, multiline, viewportWidth)
            : nextCursorIndex(state, fontFamily, fontSize, multiline, viewportWidth);
        state.hasPreferredCursorX = false;
        if (keepSelection) {
            if (!hasTextSelection(state)) {
                state.selectionStart = previous;
            }
            state.selectionEnd = state.cursor;
        } else {
            clearSelection(state);
        }
    }

    static void moveCursorToLineEdge(InputState& state,
                                     bool toEnd,
                                     bool keepSelection,
                                     const std::string& fontFamily,
                                     float fontSize,
                                     float viewportWidth) {
        const std::vector<InputLayout::Line>& lines = cachedLines(state, fontFamily, fontSize, viewportWidth, true);
        if (lines.empty()) {
            moveCursorTo(state, toEnd ? static_cast<int>(state.text.size()) : 0, keepSelection);
            return;
        }

        const int lineIndex = lineIndexFor(lines, state.cursor);
        const InputLayout::Line& line = lines[static_cast<size_t>(lineIndex)];
        moveCursorTo(state, toEnd ? line.end : line.start, keepSelection);
    }

    static void moveCursorVertical(InputState& state,
                                   int direction,
                                   bool keepSelection,
                                   const std::string& fontFamily,
                                   float fontSize,
                                   float viewportWidth,
                                   float viewportHeight) {
        const std::vector<InputLayout::Line>& lines = cachedLines(state, fontFamily, fontSize, viewportWidth, true);
        if (lines.empty()) {
            return;
        }

        auto xFor = [&](int byteIndex) {
            const int lineIndex = lineIndexFor(lines, byteIndex);
            const InputLayout::Line& line = lines[static_cast<size_t>(lineIndex)];
            return caretX(line.metrics, byteIndex - line.start);
        };

        auto closestOnLine = [&](int lineIndex, float targetX) {
            const InputLayout::Line& line = lines[static_cast<size_t>(std::clamp(lineIndex, 0, static_cast<int>(lines.size()) - 1))];
            if (line.metrics.byteIndices.empty() || line.metrics.caretX.empty()) {
                return line.start;
            }
            const size_t count = std::min(line.metrics.byteIndices.size(), line.metrics.caretX.size());
            int bestIndex = line.metrics.byteIndices.front();
            float bestDistance = std::fabs(targetX - line.metrics.caretX.front());
            for (size_t i = 1; i < count; ++i) {
                const float distance = std::fabs(targetX - line.metrics.caretX[i]);
                if (distance < bestDistance) {
                    bestDistance = distance;
                    bestIndex = line.metrics.byteIndices[i];
                }
            }
            return line.start + bestIndex;
        };

        const int previous = state.cursor;
        const int currentLine = lineIndexFor(lines, state.cursor);
        const int nextLine = std::clamp(currentLine + direction, 0, static_cast<int>(lines.size()) - 1);
        if (nextLine == currentLine) {
            return;
        }

        if (!state.hasPreferredCursorX) {
            state.preferredCursorX = xFor(state.cursor);
            state.hasPreferredCursorX = true;
        }
        state.cursor = clampUtf8Boundary(state.text, closestOnLine(nextLine, state.preferredCursorX));
        syncVerticalScroll(state, nextLine, fontSize, viewportHeight);

        if (keepSelection) {
            if (!hasTextSelection(state)) {
                state.selectionStart = previous;
            }
            state.selectionEnd = state.cursor;
        } else {
            clearSelection(state);
        }
    }

    static void moveCursorTo(InputState& state, int position, bool keepSelection) {
        const int previous = state.cursor;
        state.cursor = clampUtf8Boundary(state.text, position);
        state.hasPreferredCursorX = false;
        if (keepSelection) {
            if (!hasTextSelection(state)) {
                state.selectionStart = previous;
            }
            state.selectionEnd = state.cursor;
        } else {
            clearSelection(state);
        }
    }

    static void copySelection(const InputState& state) {
        if (!hasTextSelection(state)) {
            return;
        }
        const auto range = selectionRange(state);
        const std::string selected = state.text.substr(static_cast<std::size_t>(range.first), static_cast<std::size_t>(range.second - range.first));
        core::window::setClipboardText(selected);
    }

    static core::TextPrimitive::TextMetrics measureMetrics(const std::string& value, const std::string& fontFamily, float fontSize) {
        return core::TextPrimitive::measureTextMetrics(value, fontFamily, fontSize, 400);
    }

    static bool sameLayoutCacheKey(const InputState& state,
                                   const std::string& fontFamily,
                                   float fontSize,
                                   float viewportWidth,
                                   bool multiline) {
        return state.layoutCacheValid &&
               state.cachedTextRevision == state.textRevision &&
               state.cachedFontFamily == fontFamily &&
               std::fabs(state.cachedFontSize - fontSize) < 0.001f &&
               std::fabs(state.cachedViewportWidth - viewportWidth) < 0.001f &&
               state.cachedMultiline == multiline;
    }

    static void ensureLayoutCache(InputState& state,
                                  const std::string& fontFamily,
                                  float fontSize,
                                  float viewportWidth,
                                  bool multiline) {
        if (sameLayoutCacheKey(state, fontFamily, fontSize, viewportWidth, multiline)) {
            return;
        }

        state.cachedTextRevision = state.textRevision;
        state.cachedFontFamily = fontFamily;
        state.cachedFontSize = fontSize;
        state.cachedViewportWidth = viewportWidth;
        state.cachedMultiline = multiline;
        state.cachedMetrics = measureMetrics(state.text, fontFamily, fontSize);
        if (multiline) {
            state.cachedLines = measureLines(state.text, fontFamily, fontSize, viewportWidth);
            state.cachedTextWidth = 0.0f;
            for (const TextLine& line : state.cachedLines) {
                state.cachedTextWidth = std::max(state.cachedTextWidth, line.metrics.width);
            }
        } else {
            state.cachedLines = {{0, static_cast<int>(state.text.size()), false, state.cachedMetrics}};
            state.cachedTextWidth = state.cachedMetrics.width;
        }
        state.layoutCacheValid = true;
    }

    static const std::vector<InputLayout::Line>& cachedLines(InputState& state,
                                                            const std::string& fontFamily,
                                                            float fontSize,
                                                            float viewportWidth,
                                                            bool multiline) {
        ensureLayoutCache(state, fontFamily, fontSize, viewportWidth, multiline);
        return state.cachedLines;
    }

    static const core::TextPrimitive::TextMetrics& cachedMetrics(InputState& state,
                                                                 const std::string& fontFamily,
                                                                 float fontSize,
                                                                 float viewportWidth) {
        ensureLayoutCache(state, fontFamily, fontSize, viewportWidth, false);
        return state.cachedMetrics;
    }

    static std::vector<InputLayout::Line> measureLines(const std::string& value,
                                                       const std::string& fontFamily,
                                                       float fontSize,
                                                       float viewportWidth) {
        std::vector<InputLayout::Line> lines;
        int start = 0;
        while (start <= static_cast<int>(value.size())) {
            const size_t newline = value.find('\n', static_cast<size_t>(start));
            const int end = newline == std::string::npos
                ? static_cast<int>(value.size())
                : static_cast<int>(newline);
            const std::string lineText = value.substr(static_cast<size_t>(start), static_cast<size_t>(end - start));
            const core::TextPrimitive::TextMetrics metrics = measureMetrics(lineText, fontFamily, fontSize);
            if (viewportWidth <= 1.0f || metrics.byteIndices.size() <= 2 || metrics.width <= viewportWidth) {
                lines.push_back({start, end, newline != std::string::npos, metrics});
            } else {
                int segmentStart = 0;
                float segmentStartX = 0.0f;
                int previousStop = 0;
                const size_t count = std::min(metrics.byteIndices.size(), metrics.caretX.size());
                for (size_t i = 1; i < count; ++i) {
                    const int stop = metrics.byteIndices[i];
                    const float x = metrics.caretX[i];
                    if (previousStop > segmentStart && x - segmentStartX > viewportWidth) {
                        const int segmentEnd = previousStop;
                        const std::string segmentText = lineText.substr(static_cast<size_t>(segmentStart),
                                                                        static_cast<size_t>(segmentEnd - segmentStart));
                        lines.push_back({start + segmentStart, start + segmentEnd, false, measureMetrics(segmentText, fontFamily, fontSize)});
                        segmentStart = segmentEnd;
                        segmentStartX = caretX(metrics, segmentStart);
                    }
                    previousStop = stop;
                }
                if (segmentStart < static_cast<int>(lineText.size()) || lineText.empty()) {
                    const std::string segmentText = lineText.substr(static_cast<size_t>(segmentStart));
                    lines.push_back({start + segmentStart, end, newline != std::string::npos, measureMetrics(segmentText, fontFamily, fontSize)});
                }
            }
            if (newline == std::string::npos) {
                break;
            }
            start = static_cast<int>(newline) + 1;
        }
        if (lines.empty()) {
            lines.push_back({0, 0, false, measureMetrics({}, fontFamily, fontSize)});
        }
        return lines;
    }

    static int lineIndexFor(const std::vector<InputLayout::Line>& lines, int byteIndex) {
        if (lines.empty()) {
            return 0;
        }
        const auto it = std::upper_bound(
            lines.begin(),
            lines.end(),
            byteIndex,
            [](int value, const InputLayout::Line& line) {
                return value < line.start;
            });
        int index = it == lines.begin()
            ? 0
            : static_cast<int>(std::distance(lines.begin(), it)) - 1;
        index = std::clamp(index, 0, static_cast<int>(lines.size()) - 1);
        if (index + 1 < static_cast<int>(lines.size()) &&
            !lines[static_cast<size_t>(index)].hardBreakAfter &&
            byteIndex >= lines[static_cast<size_t>(index)].end) {
            ++index;
        }
        return std::clamp(index, 0, static_cast<int>(lines.size()) - 1);
    }

    static void syncVerticalScroll(InputState& state, int cursorLine, float lineHeight, float viewportHeight) {
        if (lineHeight <= 0.0f || viewportHeight <= 0.0f) {
            state.verticalScroll = 0.0f;
            return;
        }
        const float cursorTop = static_cast<float>(std::max(0, cursorLine)) * lineHeight;
        const float cursorBottom = cursorTop + lineHeight;
        if (cursorTop - state.verticalScroll < 0.0f) {
            state.verticalScroll = cursorTop;
        } else if (cursorBottom - state.verticalScroll > viewportHeight) {
            state.verticalScroll = cursorBottom - viewportHeight;
        }
        state.verticalScroll = std::max(0.0f, state.verticalScroll);
    }

    static void syncVerticalScroll(InputState& state, const InputLayout& layout, float viewportHeight) {
        syncVerticalScroll(state, layout.lineIndexFor(state.cursor), layout.lineHeight, viewportHeight);
    }

    static float caretX(const core::TextPrimitive::TextMetrics& metrics, int byteIndex) {
        if (metrics.byteIndices.empty() || metrics.caretX.empty()) {
            return 0.0f;
        }
        const auto it = std::lower_bound(metrics.byteIndices.begin(), metrics.byteIndices.end(), byteIndex);
        const size_t slot = it == metrics.byteIndices.end()
            ? metrics.caretX.size() - 1
            : static_cast<size_t>(std::distance(metrics.byteIndices.begin(), it));
        return metrics.caretX[std::min(slot, metrics.caretX.size() - 1)];
    }

    static int previousCaretIndex(const core::TextPrimitive::TextMetrics& metrics, int byteIndex) {
        if (metrics.byteIndices.empty()) {
            return 0;
        }
        const auto it = std::lower_bound(metrics.byteIndices.begin(), metrics.byteIndices.end(), byteIndex);
        if (it == metrics.byteIndices.begin()) {
            return metrics.byteIndices.front();
        }
        return *(it - 1);
    }

    static int nextCaretIndex(const core::TextPrimitive::TextMetrics& metrics, int byteIndex) {
        if (metrics.byteIndices.empty()) {
            return 0;
        }
        const auto it = std::upper_bound(metrics.byteIndices.begin(), metrics.byteIndices.end(), byteIndex);
        if (it == metrics.byteIndices.end()) {
            return metrics.byteIndices.back();
        }
        return *it;
    }

    static int previousCaretIndex(const std::vector<InputLayout::Line>& lines, int byteIndex) {
        if (lines.empty()) {
            return 0;
        }
        const int lineIndex = lineIndexFor(lines, byteIndex);
        const InputLayout::Line& line = lines[static_cast<size_t>(lineIndex)];
        if (line.hardBreakAfter && byteIndex == line.end + 1) {
            return line.end;
        }
        if (byteIndex > line.start) {
            return line.start + previousCaretIndex(line.metrics, byteIndex - line.start);
        }
        if (lineIndex <= 0) {
            return line.start;
        }
        const InputLayout::Line& previousLine = lines[static_cast<size_t>(lineIndex - 1)];
        return previousLine.end;
    }

    static int nextCaretIndex(const std::vector<InputLayout::Line>& lines, const std::string& text, int byteIndex) {
        if (lines.empty()) {
            return 0;
        }
        const int lineIndex = lineIndexFor(lines, byteIndex);
        const InputLayout::Line& line = lines[static_cast<size_t>(lineIndex)];
        if (line.hardBreakAfter && byteIndex == line.end) {
            return std::min(static_cast<int>(text.size()), line.end + 1);
        }
        if (byteIndex < line.end) {
            return line.start + nextCaretIndex(line.metrics, byteIndex - line.start);
        }
        if (lineIndex + 1 >= static_cast<int>(lines.size())) {
            return line.end;
        }
        const InputLayout::Line& nextLine = lines[static_cast<size_t>(lineIndex + 1)];
        return nextLine.start;
    }

    static int prevCursorIndex(InputState& state,
                               const std::string& fontFamily,
                               float fontSize,
                               bool multiline = false,
                               float viewportWidth = 0.0f) {
        if (multiline) {
            return clampUtf8Boundary(state.text, previousCaretIndex(cachedLines(state, fontFamily, fontSize, viewportWidth, true), state.cursor));
        }
        return clampUtf8Boundary(state.text, previousCaretIndex(cachedMetrics(state, fontFamily, fontSize, viewportWidth), state.cursor));
    }

    static int nextCursorIndex(InputState& state,
                               const std::string& fontFamily,
                               float fontSize,
                               bool multiline = false,
                               float viewportWidth = 0.0f) {
        if (multiline) {
            return clampUtf8Boundary(state.text, nextCaretIndex(cachedLines(state, fontFamily, fontSize, viewportWidth, true), state.text, state.cursor));
        }
        return clampUtf8Boundary(state.text, nextCaretIndex(cachedMetrics(state, fontFamily, fontSize, viewportWidth), state.cursor));
    }

    static void syncScroll(InputState& state, float viewportWidth, const std::string& fontFamily, float fontSize) {
        syncScroll(state, viewportWidth, cachedMetrics(state, fontFamily, fontSize, viewportWidth), fontSize);
    }

    static void syncScroll(InputState& state,
                           float viewportWidth,
                           const core::TextPrimitive::TextMetrics& metrics,
                           float fontSize) {
        const float textWidth = metrics.width;
        const float cursorPixel = caretX(metrics, state.cursor);
        if (textWidth <= viewportWidth) {
            state.horizontalScroll = 0.0f;
            return;
        }
        const float trailingPadding = std::max(6.0f, fontSize * 0.35f);
        const float rightSafe = std::max(1.0f, viewportWidth - trailingPadding);
        if (cursorPixel - state.horizontalScroll < 0.0f) {
            state.horizontalScroll = cursorPixel;
        } else if (cursorPixel - state.horizontalScroll > rightSafe) {
            state.horizontalScroll = cursorPixel - rightSafe;
        }
        state.horizontalScroll = std::clamp(state.horizontalScroll, 0.0f, std::max(0.0f, textWidth - viewportWidth + trailingPadding));
    }

    static std::string makeDirtyKey(const InputState& state, bool focused, const InputLayout& layout) {
        std::string key = focused ? "f|" : "b|";
        key += std::to_string(state.cursor);
        key += '|';
        key += std::to_string(state.selectionStart);
        key += '|';
        key += std::to_string(state.selectionEnd);
        key += '|';
        key += std::to_string(static_cast<int>(std::lround(layout.scroll * 64.0f)));
        key += '|';
        key += std::to_string(static_cast<int>(std::lround(state.verticalScroll * 64.0f)));
        key += '|';
        key += std::to_string(state.textRevision);
        key += '|';
        key += std::to_string(state.compositionRevision);
        return key;
    }

};

} // namespace components::input_detail
