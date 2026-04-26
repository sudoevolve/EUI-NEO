#pragma once

#include "core/layout.h"
#include "core/primitive.h"
#include "core/text.h"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace core::dsl {

inline std::string utf8(unsigned int codepoint) {
    std::string result;
    if (codepoint <= 0x7F) {
        result.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FF) {
        result.push_back(static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F)));
        result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else if (codepoint <= 0xFFFF) {
        result.push_back(static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F)));
        result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    } else {
        result.push_back(static_cast<char>(0xF0 | ((codepoint >> 18) & 0x07)));
        result.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
        result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        result.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
    }
    return result;
}

enum class ElementKind {
    Row,
    Column,
    Stack,
    Rect,
    Text
};

struct Screen {
    float width = 0.0f;
    float height = 0.0f;
};

struct Element {
    ElementKind kind = ElementKind::Stack;
    std::string id;

    bool hasX = false;
    bool hasY = false;
    float x = 0.0f;
    float y = 0.0f;
    SizeValue width = SizeValue::wrapContent();
    SizeValue height = SizeValue::wrapContent();
    EdgeInsets margin;
    float spacing = 0.0f;
    Align mainAlign = Align::START;
    Align crossAlign = Align::START;
    LayoutRect frame;

    Color color = {1.0f, 1.0f, 1.0f, 1.0f};
    Gradient gradient;
    Border border;
    Shadow shadow;
    Transform transform;
    float radius = 0.0f;
    float opacity = 1.0f;

    std::string text;
    std::string fontFamily;
    float fontSize = 16.0f;
    int fontWeight = 400;
    Color textColor = {1.0f, 1.0f, 1.0f, 1.0f};
    float maxWidth = 0.0f;
    bool wrap = false;
    HorizontalAlign horizontalAlign = HorizontalAlign::Left;
    VerticalAlign verticalAlign = VerticalAlign::Top;
    float lineHeight = 0.0f;

    bool interactive = false;
    Color hoverColor = {1.0f, 1.0f, 1.0f, 1.0f};
    Color pressedColor = {1.0f, 1.0f, 1.0f, 1.0f};
    std::function<void()> onClick;
    std::string visualStateSourceId;
    float pressedScale = 1.0f;

    std::vector<std::unique_ptr<Element>> children;

    LayoutType layoutType() const {
        if (kind == ElementKind::Row) {
            return LayoutType::Row;
        }
        if (kind == ElementKind::Column) {
            return LayoutType::Column;
        }
        return LayoutType::Stack;
    }
};

class Ui;

template <typename Derived>
class BuilderBase {
public:
    Derived& x(float value) {
        element_->hasX = true;
        element_->x = value;
        return self();
    }

    Derived& y(float value) {
        element_->hasY = true;
        element_->y = value;
        return self();
    }

    Derived& position(float xValue, float yValue) {
        element_->hasX = true;
        element_->hasY = true;
        element_->x = xValue;
        element_->y = yValue;
        return self();
    }

    Derived& width(float value) {
        element_->width = SizeValue::fixed(value);
        return self();
    }

    Derived& width(SizeValue value) {
        element_->width = value;
        return self();
    }

    Derived& height(float value) {
        element_->height = SizeValue::fixed(value);
        return self();
    }

    Derived& height(SizeValue value) {
        element_->height = value;
        return self();
    }

    Derived& size(float widthValue, float heightValue) {
        element_->width = SizeValue::fixed(widthValue);
        element_->height = SizeValue::fixed(heightValue);
        return self();
    }

    Derived& size(SizeValue widthValue, SizeValue heightValue) {
        element_->width = widthValue;
        element_->height = heightValue;
        return self();
    }

    Derived& fill() {
        element_->width = SizeValue::fill();
        element_->height = SizeValue::fill();
        return self();
    }

    Derived& wrapContent() {
        element_->width = SizeValue::wrapContent();
        element_->height = SizeValue::wrapContent();
        return self();
    }

    Derived& margin(float value) {
        element_->margin = EdgeInsets::all(std::max(0.0f, value));
        return self();
    }

    Derived& margin(float horizontal, float vertical) {
        element_->margin = {
            std::max(0.0f, horizontal),
            std::max(0.0f, vertical),
            std::max(0.0f, horizontal),
            std::max(0.0f, vertical)
        };
        return self();
    }

    Derived& margin(float left, float top, float right, float bottom) {
        element_->margin = {
            std::max(0.0f, left),
            std::max(0.0f, top),
            std::max(0.0f, right),
            std::max(0.0f, bottom)
        };
        return self();
    }

    Derived& gap(float value) {
        element_->spacing = std::max(0.0f, value);
        return self();
    }

    Derived& spacing(float value) {
        return gap(value);
    }

    Derived& justifyContent(Align value) {
        element_->mainAlign = value;
        return self();
    }

    Derived& alignItems(Align value) {
        element_->crossAlign = value;
        return self();
    }

    Derived& align(Align main, Align cross) {
        element_->mainAlign = main;
        element_->crossAlign = cross;
        return self();
    }

    Derived& pressedScale(float value) {
        element_->pressedScale = std::clamp(value, 0.80f, 1.0f);
        return self();
    }

    Derived& visualStateFrom(const std::string& id, float pressedScaleValue = 0.965f);

    template <typename Fn>
    Derived& content(Fn&& compose);

    void build() {}

protected:
    BuilderBase(Ui& ui, Element* element) : ui_(&ui), element_(element) {}

    Derived& self() {
        return static_cast<Derived&>(*this);
    }

    Ui* ui_ = nullptr;
    Element* element_ = nullptr;
};

template <typename Derived>
class ShapeBuilderBase : public BuilderBase<Derived> {
public:
    using BuilderBase<Derived>::BuilderBase;

    Derived& color(const Color& value) {
        this->element_->color = value;
        return this->self();
    }

    Derived& background(const Color& value) {
        return color(value);
    }

    Derived& background(float r, float g, float b, float a = 1.0f) {
        this->element_->color = {r, g, b, a};
        return this->self();
    }

    Derived& gradient(const Gradient& value) {
        this->element_->gradient = value;
        return this->self();
    }

    Derived& gradient(const Color& start, const Color& end, GradientDirection direction = GradientDirection::Vertical) {
        this->element_->gradient = {true, start, end, direction};
        return this->self();
    }

    Derived& rounding(float value) {
        this->element_->radius = std::max(0.0f, value);
        return this->self();
    }

    Derived& radius(float value) {
        return rounding(value);
    }

    Derived& border(float widthValue, const Color& colorValue) {
        this->element_->border = {std::max(0.0f, widthValue), colorValue};
        return this->self();
    }

    Derived& border(const Border& value) {
        this->element_->border = value;
        return this->self();
    }

    Derived& shadow(float blur, float offsetY, const Color& colorValue) {
        this->element_->shadow = {true, {0.0f, offsetY}, std::max(0.0f, blur), 0.0f, colorValue};
        return this->self();
    }

    Derived& shadow(float blur, float offsetX, float offsetY, const Color& colorValue) {
        this->element_->shadow = {true, {offsetX, offsetY}, std::max(0.0f, blur), 0.0f, colorValue};
        return this->self();
    }

    Derived& shadow(const Shadow& value) {
        this->element_->shadow = value;
        return this->self();
    }

    Derived& opacity(float value) {
        this->element_->opacity = std::clamp(value, 0.0f, 1.0f);
        return this->self();
    }

    Derived& translate(float xValue, float yValue) {
        this->element_->transform.translate = {xValue, yValue};
        return this->self();
    }

    Derived& translateX(float value) {
        this->element_->transform.translate.x = value;
        return this->self();
    }

    Derived& translateY(float value) {
        this->element_->transform.translate.y = value;
        return this->self();
    }

    Derived& scale(float value) {
        this->element_->transform.scale = {value, value};
        return this->self();
    }

    Derived& scale(float xValue, float yValue) {
        this->element_->transform.scale = {xValue, yValue};
        return this->self();
    }

    Derived& rotate(float radians) {
        this->element_->transform.rotate = radians;
        return this->self();
    }

    Derived& rotation(float radians) {
        return rotate(radians);
    }

    Derived& transformOrigin(float xValue, float yValue) {
        this->element_->transform.origin = {xValue, yValue};
        return this->self();
    }

    Derived& interactive(bool value = true) {
        this->element_->interactive = value;
        return this->self();
    }

    Derived& hoverColor(const Color& value) {
        this->element_->hoverColor = value;
        return this->self();
    }

    Derived& pressedColor(const Color& value) {
        this->element_->pressedColor = value;
        return this->self();
    }

    Derived& states(const Color& normal, const Color& hover, const Color& pressed) {
        this->element_->color = normal;
        this->element_->hoverColor = hover;
        this->element_->pressedColor = pressed;
        this->element_->interactive = true;
        return this->self();
    }

    Derived& onClick(std::function<void()> callback) {
        this->element_->interactive = true;
        this->element_->onClick = std::move(callback);
        return this->self();
    }

};

class LayoutBuilder : public BuilderBase<LayoutBuilder> {
public:
    LayoutBuilder(Ui& ui, Element* element) : BuilderBase<LayoutBuilder>(ui, element) {}
};

class RectBuilder : public ShapeBuilderBase<RectBuilder> {
public:
    RectBuilder(Ui& ui, Element* element) : ShapeBuilderBase<RectBuilder>(ui, element) {}
};

class TextBuilder : public BuilderBase<TextBuilder> {
public:
    TextBuilder(Ui& ui, Element* element) : BuilderBase<TextBuilder>(ui, element) {}

    TextBuilder& text(const std::string& value) {
        element_->text = value;
        return *this;
    }

    TextBuilder& fontFamily(const std::string& value) {
        element_->fontFamily = value;
        return *this;
    }

    TextBuilder& font(const std::string& value) {
        return fontFamily(value);
    }

    TextBuilder& customFont(const std::string& value) {
        return fontFamily(value);
    }

    TextBuilder& fontSize(float value) {
        element_->fontSize = std::max(1.0f, value);
        return *this;
    }

    TextBuilder& fontWeight(int value) {
        element_->fontWeight = value;
        return *this;
    }

    TextBuilder& color(const Color& value) {
        element_->textColor = value;
        return *this;
    }

    TextBuilder& maxWidth(float value) {
        element_->maxWidth = std::max(0.0f, value);
        return *this;
    }

    TextBuilder& wrap(bool value = true) {
        element_->wrap = value;
        return *this;
    }

    TextBuilder& horizontalAlign(HorizontalAlign value) {
        element_->horizontalAlign = value;
        return *this;
    }

    TextBuilder& verticalAlign(VerticalAlign value) {
        element_->verticalAlign = value;
        return *this;
    }

    TextBuilder& lineHeight(float value) {
        element_->lineHeight = std::max(0.0f, value);
        return *this;
    }

    TextBuilder& icon(unsigned int codepoint) {
        element_->text = utf8(codepoint);
        return *this;
    }

    TextBuilder& icon(const std::string& value) {
        element_->text = value;
        return *this;
    }

};

class Ui {
public:
    void begin(const std::string& pageId = "") {
        pageId_ = pageId;
        roots_.clear();
        stack_.clear();
        index_.clear();
        generatedId_ = 0;
    }

    void end() {
        stack_.clear();
    }

    LayoutBuilder row(const std::string& id = "") {
        return LayoutBuilder(*this, addElement(ElementKind::Row, id));
    }

    LayoutBuilder column(const std::string& id = "") {
        return LayoutBuilder(*this, addElement(ElementKind::Column, id));
    }

    LayoutBuilder stack(const std::string& id = "") {
        return LayoutBuilder(*this, addElement(ElementKind::Stack, id));
    }

    RectBuilder rect(const std::string& id) {
        return RectBuilder(*this, addElement(ElementKind::Rect, id));
    }

    TextBuilder text(const std::string& id) {
        return TextBuilder(*this, addElement(ElementKind::Text, id));
    }

    TextBuilder label(const std::string& id) {
        return text(id);
    }

    void layout(float width, float height) {
        for (const auto& root : roots_) {
            std::vector<std::pair<Element*, Node*>> links;
            std::unique_ptr<Node> layoutRoot = buildLayoutNode(*root, links);
            layoutRoot->measure(width, height);
            layoutRoot->layout(root->hasX ? root->x : 0.0f, root->hasY ? root->y : 0.0f);
            for (const auto& link : links) {
                link.first->frame = link.second->frame();
            }
        }
    }

    void layout(const Screen& screen) {
        layout(screen.width, screen.height);
    }

    Element* find(const std::string& id) {
        const auto it = index_.find(resolveId(id));
        return it == index_.end() ? nullptr : it->second;
    }

    const Element* find(const std::string& id) const {
        const auto it = index_.find(resolveId(id));
        return it == index_.end() ? nullptr : it->second;
    }

    const std::vector<std::unique_ptr<Element>>& roots() const {
        return roots_;
    }

private:
    friend class BuilderBase<LayoutBuilder>;
    friend class BuilderBase<RectBuilder>;
    friend class BuilderBase<TextBuilder>;

    Element* addElement(ElementKind kind, const std::string& id) {
        auto element = std::make_unique<Element>();
        element->kind = kind;
        element->id = id.empty() ? makeGeneratedId(kind) : resolveId(id);
        Element* raw = element.get();

        if (stack_.empty()) {
            roots_.push_back(std::move(element));
        } else {
            stack_.back()->children.push_back(std::move(element));
        }

        index_[raw->id] = raw;
        return raw;
    }

    void push(Element* element) {
        stack_.push_back(element);
    }

    void pop() {
        if (!stack_.empty()) {
            stack_.pop_back();
        }
    }

    std::string resolveId(const std::string& id) const {
        if (id.empty() || pageId_.empty()) {
            return id;
        }
        return pageId_ + "." + id;
    }

    std::string makeGeneratedId(ElementKind kind) {
        const char* prefix = "__stack";
        if (kind == ElementKind::Row) {
            prefix = "__row";
        } else if (kind == ElementKind::Column) {
            prefix = "__column";
        } else if (kind == ElementKind::Rect) {
            prefix = "__rect";
        } else if (kind == ElementKind::Text) {
            prefix = "__text";
        }
        return resolveId(std::string(prefix) + "." + std::to_string(generatedId_++));
    }

    static std::unique_ptr<Node> buildLayoutNode(Element& element, std::vector<std::pair<Element*, Node*>>& links) {
        auto node = std::make_unique<Node>(element.layoutType());
        node->setWidth(element.width);
        node->setHeight(element.height);
        node->setMargin(element.margin);
        node->setSpacing(element.spacing);
        node->setMainAlign(element.mainAlign);
        node->setCrossAlign(element.crossAlign);

        Node* raw = node.get();
        links.push_back({&element, raw});
        for (auto& child : element.children) {
            raw->addChild(buildLayoutNode(*child, links));
        }
        return node;
    }

    std::string pageId_;
    std::vector<std::unique_ptr<Element>> roots_;
    std::vector<Element*> stack_;
    std::unordered_map<std::string, Element*> index_;
    std::size_t generatedId_ = 0;
};

template <typename Derived>
template <typename Fn>
Derived& BuilderBase<Derived>::content(Fn&& compose) {
    ui_->push(element_);
    std::forward<Fn>(compose)();
    ui_->pop();
    return self();
}

template <typename Derived>
Derived& BuilderBase<Derived>::visualStateFrom(const std::string& id, float pressedScaleValue) {
    element_->visualStateSourceId = ui_->resolveId(id);
    element_->pressedScale = std::clamp(pressedScaleValue, 0.80f, 1.0f);
    return self();
}

} // namespace core::dsl
