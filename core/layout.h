#pragma once

#include <algorithm>
#include <limits>
#include <memory>
#include <vector>

namespace core {

enum class Align {
    START,
    CENTER,
    END
};

enum class LayoutType {
    Row,
    Column,
    Stack,
    Flow
};

enum class SizeMode {
    Fixed,
    WrapContent,
    Fill
};

struct SizeValue {
    SizeMode mode = SizeMode::Fixed;
    float value = 0.0f;

    static SizeValue fixed(float value) {
        return {SizeMode::Fixed, value};
    }

    static SizeValue wrapContent() {
        return {SizeMode::WrapContent, 0.0f};
    }

    static SizeValue fill() {
        return {SizeMode::Fill, 0.0f};
    }
};

struct EdgeInsets {
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;

    static EdgeInsets all(float value) {
        return {value, value, value, value};
    }
};

struct LayoutRect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

class Node {
public:
    explicit Node(LayoutType type = LayoutType::Stack)
        : type_(type) {}

    Node* addChild(std::unique_ptr<Node> child) {
        children_.push_back(std::move(child));
        return children_.back().get();
    }

    Node* addChild(LayoutType type = LayoutType::Stack) {
        children_.push_back(std::make_unique<Node>(type));
        return children_.back().get();
    }

    void setType(LayoutType type) { type_ = type; }
    void setWidth(SizeValue width) { width_ = width; }
    void setHeight(SizeValue height) { height_ = height; }
    void setFixedSize(float width, float height) {
        width_ = SizeValue::fixed(width);
        height_ = SizeValue::fixed(height);
    }
    void setMargin(const EdgeInsets& margin) { margin_ = margin; }
    void setPadding(const EdgeInsets& padding) { padding_ = padding; }
    void setSpacing(float spacing) { spacing_ = spacing; }
    void setLineSpacing(float spacing) { lineSpacing_ = spacing; }
    void setMainAlign(Align align) { mainAlign_ = align; }
    void setCrossAlign(Align align) { crossAlign_ = align; }
    void setMinWidth(float value) { minWidth_ = std::max(0.0f, value); }
    void setMinHeight(float value) { minHeight_ = std::max(0.0f, value); }
    void setMaxWidth(float value) { maxWidth_ = value > 0.0f ? value : std::numeric_limits<float>::max(); }
    void setMaxHeight(float value) { maxHeight_ = value > 0.0f ? value : std::numeric_limits<float>::max(); }
    void setFlexGrow(float value) { flexGrow_ = std::max(0.0f, value); }
    void setFlexShrink(float value) { flexShrink_ = std::max(0.0f, value); }
    void setPosition(float x, float y, bool hasX, bool hasY) {
        x_ = x;
        y_ = y;
        hasX_ = hasX;
        hasY_ = hasY;
    }

    LayoutType type() const { return type_; }
    const SizeValue& widthValue() const { return width_; }
    const SizeValue& heightValue() const { return height_; }
    const EdgeInsets& margin() const { return margin_; }
    const EdgeInsets& padding() const { return padding_; }
    float spacing() const { return spacing_; }
    float lineSpacing() const { return lineSpacing_; }
    Align mainAlign() const { return mainAlign_; }
    Align crossAlign() const { return crossAlign_; }
    const LayoutRect& frame() const { return frame_; }
    float measuredWidth() const { return measuredWidth_; }
    float measuredHeight() const { return measuredHeight_; }
    float flexGrow() const { return flexGrow_; }
    float flexShrink() const { return flexShrink_; }
    const std::vector<std::unique_ptr<Node>>& children() const { return children_; }

    void measure(float availableWidth = 0.0f, float availableHeight = 0.0f) {
        measure(availableWidth, availableHeight, false, false);
    }

private:
    void measure(float availableWidth, float availableHeight, bool forceWidth, bool forceHeight) {
        const float innerAvailableWidth = availableWidth > 0.0f
            ? std::max(0.0f, availableWidth - padding_.left - padding_.right)
            : 0.0f;
        const float innerAvailableHeight = availableHeight > 0.0f
            ? std::max(0.0f, availableHeight - padding_.top - padding_.bottom)
            : 0.0f;

        const LayoutRect content = measureContent(innerAvailableWidth, innerAvailableHeight);
        measuredWidth_ = resolveSize(width_, content.width + padding_.left + padding_.right, availableWidth, minWidth_, maxWidth_, forceWidth);
        measuredHeight_ = resolveSize(height_, content.height + padding_.top + padding_.bottom, availableHeight, minHeight_, maxHeight_, forceHeight);
    }

public:
    void layout(float x, float y) {
        frame_ = {x, y, measuredWidth_, measuredHeight_};

        if (children_.empty()) {
            return;
        }

        if (type_ == LayoutType::Row) {
            layoutRow();
        } else if (type_ == LayoutType::Column) {
            layoutColumn();
        } else if (type_ == LayoutType::Flow) {
            layoutFlow();
        } else {
            layoutStack();
        }
    }

private:
    static float clampSize(float value, float minValue, float maxValue) {
        const float upper = std::max(minValue, maxValue);
        return std::clamp(value, minValue, upper);
    }

    static float resolveSize(const SizeValue& size,
                             float contentSize,
                             float availableSize,
                             float minValue,
                             float maxValue,
                             bool forceAvailable = false) {
        float resolved = contentSize;
        if (forceAvailable && availableSize > 0.0f) {
            resolved = availableSize;
        } else if (size.mode == SizeMode::Fixed) {
            resolved = size.value;
        } else if (size.mode == SizeMode::Fill) {
            resolved = availableSize > 0.0f ? availableSize : contentSize;
        }
        return clampSize(resolved, minValue, maxValue);
    }

    static float outerWidth(const Node& node) {
        return node.measuredWidth_ + node.margin_.left + node.margin_.right;
    }

    static float outerHeight(const Node& node) {
        return node.measuredHeight_ + node.margin_.top + node.margin_.bottom;
    }

    static float innerSpan(float total, float leading, float trailing) {
        return std::max(0.0f, total - leading - trailing);
    }

    float innerWidth() const {
        return innerSpan(frame_.width, padding_.left, padding_.right);
    }

    float innerHeight() const {
        return innerSpan(frame_.height, padding_.top, padding_.bottom);
    }

    bool isFlexibleAlongRow(const Node& child) const {
        return child.width_.mode == SizeMode::Fill || child.flexGrow_ > 0.0f;
    }

    bool isFlexibleAlongColumn(const Node& child) const {
        return child.height_.mode == SizeMode::Fill || child.flexGrow_ > 0.0f;
    }

    float childBaseMainSize(const Node& child, bool rowAxis) const {
        if (rowAxis && child.width_.mode == SizeMode::Fill) {
            return 0.0f;
        }
        if (!rowAxis && child.height_.mode == SizeMode::Fill) {
            return 0.0f;
        }
        return rowAxis ? child.measuredWidth_ : child.measuredHeight_;
    }

    float childFlexGrowWeight(const Node& child) const {
        return child.flexGrow_ > 0.0f ? child.flexGrow_ : 1.0f;
    }

    LayoutRect measureContent(float availableWidth, float availableHeight) {
        if (children_.empty()) {
            return {
                0.0f,
                0.0f,
                width_.mode == SizeMode::Fixed ? width_.value : availableWidth,
                height_.mode == SizeMode::Fixed ? height_.value : availableHeight
            };
        }

        if (type_ == LayoutType::Row) {
            return measureRowContent(availableWidth, availableHeight);
        }
        if (type_ == LayoutType::Column) {
            return measureColumnContent(availableWidth, availableHeight);
        }
        if (type_ == LayoutType::Flow) {
            return measureFlowContent(availableWidth, availableHeight);
        }

        float maxHeight = 0.0f;
        float maxWidth = 0.0f;
        for (const auto& child : children_) {
            child->measure(availableWidth, availableHeight);
            const float childX = child->hasX_ ? std::max(0.0f, child->x_) : 0.0f;
            const float childY = child->hasY_ ? std::max(0.0f, child->y_) : 0.0f;
            maxWidth = std::max(maxWidth, childX + outerWidth(*child));
            maxHeight = std::max(maxHeight, childY + outerHeight(*child));
        }
        return {0.0f, 0.0f, maxWidth, maxHeight};
    }

    LayoutRect measureRowContent(float availableWidth, float availableHeight) {
        float baseTotal = 0.0f;
        float contentHeight = 0.0f;
        float growWeight = 0.0f;
        float shrinkWeight = 0.0f;

        for (size_t i = 0; i < children_.size(); ++i) {
            Node& child = *children_[i];
            const bool flexible = isFlexibleAlongRow(child);
            child.measure(flexible ? 0.0f : availableWidth, availableHeight);
            baseTotal += childBaseMainSize(child, true) + child.margin_.left + child.margin_.right;
            contentHeight = std::max(contentHeight, outerHeight(child));
            if (flexible) {
                growWeight += childFlexGrowWeight(child);
            }
            if (child.flexShrink_ > 0.0f) {
                shrinkWeight += child.flexShrink_;
            }
            if (i + 1 < children_.size()) {
                baseTotal += spacing_;
            }
        }

        const float remaining = availableWidth > 0.0f ? availableWidth - baseTotal : 0.0f;
        if ((remaining > 0.0f && growWeight > 0.0f) || (remaining < 0.0f && shrinkWeight > 0.0f)) {
            for (Node& childRef : derefChildren()) {
                Node& child = childRef;
                const bool flexible = isFlexibleAlongRow(child);
                if (!flexible && !(remaining < 0.0f && child.flexShrink_ > 0.0f)) {
                    continue;
                }
                float assigned = childBaseMainSize(child, true);
                if (remaining > 0.0f && flexible) {
                    assigned += remaining * (childFlexGrowWeight(child) / growWeight);
                } else if (remaining < 0.0f && child.flexShrink_ > 0.0f) {
                    assigned += remaining * (child.flexShrink_ / shrinkWeight);
                }
                assigned = std::max(0.0f, assigned);
                child.measure(assigned, availableHeight, true, false);
                contentHeight = std::max(contentHeight, outerHeight(child));
            }
        }

        float contentWidth = 0.0f;
        for (size_t i = 0; i < children_.size(); ++i) {
            contentWidth += outerWidth(*children_[i]);
            if (i + 1 < children_.size()) {
                contentWidth += spacing_;
            }
        }
        return {0.0f, 0.0f, contentWidth, contentHeight};
    }

    LayoutRect measureColumnContent(float availableWidth, float availableHeight) {
        float baseTotal = 0.0f;
        float contentWidth = 0.0f;
        float growWeight = 0.0f;
        float shrinkWeight = 0.0f;

        for (size_t i = 0; i < children_.size(); ++i) {
            Node& child = *children_[i];
            const bool flexible = isFlexibleAlongColumn(child);
            child.measure(availableWidth, flexible ? 0.0f : availableHeight);
            baseTotal += childBaseMainSize(child, false) + child.margin_.top + child.margin_.bottom;
            contentWidth = std::max(contentWidth, outerWidth(child));
            if (flexible) {
                growWeight += childFlexGrowWeight(child);
            }
            if (child.flexShrink_ > 0.0f) {
                shrinkWeight += child.flexShrink_;
            }
            if (i + 1 < children_.size()) {
                baseTotal += spacing_;
            }
        }

        const float remaining = availableHeight > 0.0f ? availableHeight - baseTotal : 0.0f;
        if ((remaining > 0.0f && growWeight > 0.0f) || (remaining < 0.0f && shrinkWeight > 0.0f)) {
            for (Node& childRef : derefChildren()) {
                Node& child = childRef;
                const bool flexible = isFlexibleAlongColumn(child);
                if (!flexible && !(remaining < 0.0f && child.flexShrink_ > 0.0f)) {
                    continue;
                }
                float assigned = childBaseMainSize(child, false);
                if (remaining > 0.0f && flexible) {
                    assigned += remaining * (childFlexGrowWeight(child) / growWeight);
                } else if (remaining < 0.0f && child.flexShrink_ > 0.0f) {
                    assigned += remaining * (child.flexShrink_ / shrinkWeight);
                }
                assigned = std::max(0.0f, assigned);
                child.measure(availableWidth, assigned, false, true);
                contentWidth = std::max(contentWidth, outerWidth(child));
            }
        }

        float contentHeight = 0.0f;
        for (size_t i = 0; i < children_.size(); ++i) {
            contentHeight += outerHeight(*children_[i]);
            if (i + 1 < children_.size()) {
                contentHeight += spacing_;
            }
        }
        return {0.0f, 0.0f, contentWidth, contentHeight};
    }

    LayoutRect measureFlowContent(float availableWidth, float availableHeight) {
        float maxLineWidth = 0.0f;
        float lineWidth = 0.0f;
        float lineHeight = 0.0f;
        float totalHeight = 0.0f;
        bool hasLine = false;

        for (const auto& child : children_) {
            child->measure(availableWidth, availableHeight);
            const float childWidth = outerWidth(*child);
            const float childHeight = outerHeight(*child);
            const bool wrap = hasLine && availableWidth > 0.0f && lineWidth + spacing_ + childWidth > availableWidth;
            if (wrap) {
                maxLineWidth = std::max(maxLineWidth, lineWidth);
                totalHeight += lineHeight + lineSpacing_;
                lineWidth = childWidth;
                lineHeight = childHeight;
            } else {
                if (hasLine) {
                    lineWidth += spacing_;
                }
                lineWidth += childWidth;
                lineHeight = std::max(lineHeight, childHeight);
            }
            hasLine = true;
        }

        if (hasLine) {
            maxLineWidth = std::max(maxLineWidth, lineWidth);
            totalHeight += lineHeight;
        }
        return {0.0f, 0.0f, maxLineWidth, totalHeight};
    }

    float mainOffset(float containerSize, float contentSize) const {
        if (mainAlign_ == Align::CENTER) {
            return (containerSize - contentSize) * 0.5f;
        }
        if (mainAlign_ == Align::END) {
            return containerSize - contentSize;
        }
        return 0.0f;
    }

    float crossOffset(float containerSize, float childOuterSize) const {
        if (crossAlign_ == Align::CENTER) {
            return (containerSize - childOuterSize) * 0.5f;
        }
        if (crossAlign_ == Align::END) {
            return containerSize - childOuterSize;
        }
        return 0.0f;
    }

    float rowTotalWidth() const {
        float total = 0.0f;
        for (size_t i = 0; i < children_.size(); ++i) {
            total += outerWidth(*children_[i]);
            if (i + 1 < children_.size()) {
                total += spacing_;
            }
        }
        return total;
    }

    float columnTotalHeight() const {
        float total = 0.0f;
        for (size_t i = 0; i < children_.size(); ++i) {
            total += outerHeight(*children_[i]);
            if (i + 1 < children_.size()) {
                total += spacing_;
            }
        }
        return total;
    }

    void layoutRow() {
        const float contentWidth = rowTotalWidth();
        float cursorX = frame_.x + padding_.left + mainOffset(innerWidth(), contentWidth);

        for (const auto& child : children_) {
            const float childOuterHeight = outerHeight(*child);
            const float childX = cursorX + child->margin_.left;
            const float childY = frame_.y + padding_.top + crossOffset(innerHeight(), childOuterHeight) + child->margin_.top;

            child->layout(childX, childY);
            cursorX += outerWidth(*child) + spacing_;
        }
    }

    void layoutColumn() {
        const float contentHeight = columnTotalHeight();
        float cursorY = frame_.y + padding_.top + mainOffset(innerHeight(), contentHeight);

        for (const auto& child : children_) {
            const float childOuterWidth = outerWidth(*child);
            const float childX = frame_.x + padding_.left + crossOffset(innerWidth(), childOuterWidth) + child->margin_.left;
            const float childY = cursorY + child->margin_.top;

            child->layout(childX, childY);
            cursorY += outerHeight(*child) + spacing_;
        }
    }

    void layoutStack() {
        for (const auto& child : children_) {
            const float childOuterWidth = outerWidth(*child);
            const float childOuterHeight = outerHeight(*child);
            const float childX = frame_.x + padding_.left + (child->hasX_ ? child->x_ : crossOffset(innerWidth(), childOuterWidth)) + child->margin_.left;
            const float childY = frame_.y + padding_.top + (child->hasY_ ? child->y_ : mainOffset(innerHeight(), childOuterHeight)) + child->margin_.top;
            child->layout(childX, childY);
        }
    }

    void layoutFlow() {
        const float startX = frame_.x + padding_.left;
        const float startY = frame_.y + padding_.top;
        const float availableWidth = innerWidth();
        float lineWidth = 0.0f;
        float lineHeight = 0.0f;
        float cursorX = startX;
        float cursorY = startY;

        for (const auto& child : children_) {
            const float childOuterWidth = outerWidth(*child);
            const float childOuterHeight = outerHeight(*child);
            const bool wrap = cursorX > startX && availableWidth > 0.0f && lineWidth + spacing_ + childOuterWidth > availableWidth;
            if (wrap) {
                cursorX = startX;
                cursorY += lineHeight + lineSpacing_;
                lineWidth = 0.0f;
                lineHeight = 0.0f;
            }

            if (cursorX > startX) {
                cursorX += spacing_;
                lineWidth += spacing_;
            }

            child->layout(cursorX + child->margin_.left, cursorY + child->margin_.top);
            cursorX += childOuterWidth;
            lineWidth += childOuterWidth;
            lineHeight = std::max(lineHeight, childOuterHeight);
        }
    }

    std::vector<std::reference_wrapper<Node>> derefChildren() {
        std::vector<std::reference_wrapper<Node>> refs;
        refs.reserve(children_.size());
        for (const auto& child : children_) {
            refs.push_back(*child);
        }
        return refs;
    }

    LayoutType type_ = LayoutType::Stack;
    SizeValue width_ = SizeValue::wrapContent();
    SizeValue height_ = SizeValue::wrapContent();
    EdgeInsets margin_;
    EdgeInsets padding_;
    bool hasX_ = false;
    bool hasY_ = false;
    float x_ = 0.0f;
    float y_ = 0.0f;
    float spacing_ = 0.0f;
    float lineSpacing_ = 0.0f;
    Align mainAlign_ = Align::START;
    Align crossAlign_ = Align::START;
    float minWidth_ = 0.0f;
    float minHeight_ = 0.0f;
    float maxWidth_ = std::numeric_limits<float>::max();
    float maxHeight_ = std::numeric_limits<float>::max();
    float flexGrow_ = 0.0f;
    float flexShrink_ = 1.0f;
    float measuredWidth_ = 0.0f;
    float measuredHeight_ = 0.0f;
    LayoutRect frame_;
    std::vector<std::unique_ptr<Node>> children_;
};

} // namespace core
