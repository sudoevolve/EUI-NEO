#pragma once

#include <algorithm>
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
    Stack
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
    void setSpacing(float spacing) { spacing_ = spacing; }
    void setMainAlign(Align align) { mainAlign_ = align; }
    void setCrossAlign(Align align) { crossAlign_ = align; }

    LayoutType type() const { return type_; }
    const SizeValue& widthValue() const { return width_; }
    const SizeValue& heightValue() const { return height_; }
    const EdgeInsets& margin() const { return margin_; }
    float spacing() const { return spacing_; }
    Align mainAlign() const { return mainAlign_; }
    Align crossAlign() const { return crossAlign_; }
    const LayoutRect& frame() const { return frame_; }
    float measuredWidth() const { return measuredWidth_; }
    float measuredHeight() const { return measuredHeight_; }
    const std::vector<std::unique_ptr<Node>>& children() const { return children_; }

    void measure(float availableWidth = 0.0f, float availableHeight = 0.0f) {
        for (const auto& child : children_) {
            child->measure(availableWidth, availableHeight);
        }

        const float contentWidth = measureContentWidth(availableWidth);
        const float contentHeight = measureContentHeight(availableHeight);

        measuredWidth_ = resolveSize(width_, contentWidth, availableWidth);
        measuredHeight_ = resolveSize(height_, contentHeight, availableHeight);
    }

    void layout(float x, float y) {
        frame_ = {x, y, measuredWidth_, measuredHeight_};

        if (children_.empty()) {
            return;
        }

        if (type_ == LayoutType::Row) {
            layoutRow();
        } else if (type_ == LayoutType::Column) {
            layoutColumn();
        } else {
            layoutStack();
        }
    }

private:
    static float resolveSize(const SizeValue& size, float contentSize, float availableSize) {
        if (size.mode == SizeMode::Fixed) {
            return size.value;
        }
        if (size.mode == SizeMode::Fill) {
            return availableSize > 0.0f ? availableSize : contentSize;
        }
        return contentSize;
    }

    static float outerWidth(const Node& node) {
        return node.measuredWidth_ + node.margin_.left + node.margin_.right;
    }

    static float outerHeight(const Node& node) {
        return node.measuredHeight_ + node.margin_.top + node.margin_.bottom;
    }

    float measureContentWidth(float availableWidth) const {
        if (children_.empty()) {
            return width_.mode == SizeMode::Fixed ? width_.value : availableWidth;
        }

        if (type_ == LayoutType::Row) {
            float total = 0.0f;
            for (size_t i = 0; i < children_.size(); ++i) {
                total += outerWidth(*children_[i]);
                if (i + 1 < children_.size()) {
                    total += spacing_;
                }
            }
            return total;
        }

        float maxWidth = 0.0f;
        for (const auto& child : children_) {
            maxWidth = std::max(maxWidth, outerWidth(*child));
        }
        return maxWidth;
    }

    float measureContentHeight(float availableHeight) const {
        if (children_.empty()) {
            return height_.mode == SizeMode::Fixed ? height_.value : availableHeight;
        }

        if (type_ == LayoutType::Column) {
            float total = 0.0f;
            for (size_t i = 0; i < children_.size(); ++i) {
                total += outerHeight(*children_[i]);
                if (i + 1 < children_.size()) {
                    total += spacing_;
                }
            }
            return total;
        }

        float maxHeight = 0.0f;
        for (const auto& child : children_) {
            maxHeight = std::max(maxHeight, outerHeight(*child));
        }
        return maxHeight;
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
        float cursorX = frame_.x + mainOffset(frame_.width, rowTotalWidth());

        for (const auto& child : children_) {
            const float childOuterHeight = outerHeight(*child);
            const float childX = cursorX + child->margin_.left;
            const float childY = frame_.y + crossOffset(frame_.height, childOuterHeight) + child->margin_.top;

            child->layout(childX, childY);
            cursorX += outerWidth(*child) + spacing_;
        }
    }

    void layoutColumn() {
        float cursorY = frame_.y + mainOffset(frame_.height, columnTotalHeight());

        for (const auto& child : children_) {
            const float childOuterWidth = outerWidth(*child);
            const float childX = frame_.x + crossOffset(frame_.width, childOuterWidth) + child->margin_.left;
            const float childY = cursorY + child->margin_.top;

            child->layout(childX, childY);
            cursorY += outerHeight(*child) + spacing_;
        }
    }

    void layoutStack() {
        for (const auto& child : children_) {
            const float childOuterWidth = outerWidth(*child);
            const float childOuterHeight = outerHeight(*child);
            const float childX = frame_.x + crossOffset(frame_.width, childOuterWidth) + child->margin_.left;
            const float childY = frame_.y + mainOffset(frame_.height, childOuterHeight) + child->margin_.top;
            child->layout(childX, childY);
        }
    }

    LayoutType type_ = LayoutType::Stack;
    SizeValue width_ = SizeValue::wrapContent();
    SizeValue height_ = SizeValue::wrapContent();
    EdgeInsets margin_;
    float spacing_ = 0.0f;
    Align mainAlign_ = Align::START;
    Align crossAlign_ = Align::START;
    float measuredWidth_ = 0.0f;
    float measuredHeight_ = 0.0f;
    LayoutRect frame_;
    std::vector<std::unique_ptr<Node>> children_;
};

} // namespace core
