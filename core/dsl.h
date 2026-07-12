#pragma once

#include "core/layout.h"
#include "core/animation.h"
#include "core/input/input_types.h"
#include "core/render/image_types.h"
#include "core/render/render_types.h"
#include "core/render/text_types.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace core::dsl {

using AnimProperty = core::AnimProperty;
using Ease = core::Ease;
using Transition = core::Transition;

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
    Flow,
    Rect,
    Polygon,
    Text,
    Image,
    Svg
};

enum class HitTestMode {
    Layout,
    Transformed,
    None
};

enum class LoaderMode {
    DestroyOnHide,
    KeepAlive
};

struct Screen {
    float width = 0.0f;
    float height = 0.0f;
};

struct DragEvent {
    double x = 0.0;
    double y = 0.0;
    double deltaX = 0.0;
    double deltaY = 0.0;
    double totalX = 0.0;
    double totalY = 0.0;
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
    EdgeInsets padding;
    float spacing = 0.0f;
    float lineSpacing = 0.0f;
    Align mainAlign = Align::START;
    Align crossAlign = Align::START;
    LayoutRect frame;
    int zIndex = 0;
    bool clip = false;
    float minLayoutWidth = 0.0f;
    float minLayoutHeight = 0.0f;
    float maxLayoutWidth = 0.0f;
    float maxLayoutHeight = 0.0f;
    float flexGrow = 0.0f;
    float flexShrink = 0.0f;
    bool ignoreLayout = false;

    Color color = {1.0f, 1.0f, 1.0f, 1.0f};
    Gradient gradient;
    Border border;
    Shadow shadow;
    Transform transform;
    float radius = 0.0f;
    float blur = 0.0f;
    float opacity = 1.0f;
    std::vector<Vec2> polygonPoints;

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

    std::string imageSource;
    std::string svgSource;
    bool imageFlipVertically = false;
    ImageFit imageFit = ImageFit::Cover;
    bool imageHasCoverViewport = false;
    Vec2 imageCoverViewportSize;
    Vec2 imageCoverViewportOffset;

    bool interactive = false;
    bool focusable = false;
    bool preserveFocusOnPress = false;
    bool disabled = false;
    HitTestMode hitTestMode = HitTestMode::Layout;
    bool hasImeRect = false;
    Rect imeRect;
    CursorShape cursor = CursorShape::Arrow;
    Color hoverColor = {1.0f, 1.0f, 1.0f, 1.0f};
    Color pressedColor = {1.0f, 1.0f, 1.0f, 1.0f};
    bool hasStateColors = false;
    bool smoothStateColors = true;
    std::function<void()> onClick;
    std::function<void(const PointerEvent&, const Rect&)> onPress;
    std::function<void(const PointerEvent&, const Rect&)> onRelease;
    std::function<bool(const PointerEvent&, const Rect&)> onMove;
    std::function<void(const PointerEvent&, const Rect&)> onContextMenu;
    std::function<void(bool)> onHoverChanged;
    std::function<void(bool)> onFocusChanged;
    std::function<void(const KeyboardEvent&)> onTextInput;
    std::function<void(const ScrollEvent&)> onScroll;
    std::function<void(float)> onScrollOffsetChanged;
    std::function<void(const DragEvent&)> onDrag;
    std::function<void()> onTimer;
    std::function<void(float)> onFrame;
    float timerSeconds = 0.0f;
    std::string visualStateSourceId;
    std::string hoverOpacitySourceId;
    std::string pointerRuntimeSourceId;
    std::string scrollStateId;
    std::string scrollContentSourceId;
    std::string scrollDragSourceId;
    std::string scrollThumbSourceId;
    bool composeOnScrollOffsetChange = false;
    std::string sliderStateId;
    std::string sliderInputSourceId;
    std::string sliderFillSourceId;
    std::string sliderKnobSourceId;
    float pressedScale = 1.0f;
    float hoverHiddenOpacity = 0.0f;
    float hoverVisibleOpacity = 1.0f;
    float pointerRuntimeAmount = 0.0f;
    float pointerRuntimeMaxRotateX = 0.0f;
    float pointerRuntimeMaxRotateY = 0.0f;
    float pointerRuntimeHoverScale = 1.0f;
    Vec2 pointerRuntimeTranslate = {0.0f, 0.0f};
    float scrollOffset = 0.0f;
    float scrollMaxOffset = 0.0f;
    float scrollStep = 48.0f;
    float scrollDragTravel = 0.0f;
    float scrollThumbTravel = 0.0f;
    float sliderValue = 0.0f;
    float sliderWidth = 0.0f;
    float sliderKnobSize = 0.0f;
    std::function<void(float)> onSliderValueChanged;
    Transition transition;
    bool explicitFrameAnimation = false;
    std::string dirtyKey;

    std::vector<std::unique_ptr<Element>> children;
    std::vector<const Element*> orderedChildren;
    bool subtreeNeedsUpdate = true;
    bool subtreeHasDependentVisuals = false;
    bool subtreeHasBackdropBlur = false;
    bool subtreeBlocksRetainedLayer = true;

    LayoutType layoutType() const {
        if (kind == ElementKind::Row) {
            return LayoutType::Row;
        }
        if (kind == ElementKind::Column) {
            return LayoutType::Column;
        }
        if (kind == ElementKind::Flow) {
            return LayoutType::Flow;
        }
        return LayoutType::Stack;
    }
};

class Ui;
class LoaderBuilder;

class StateStore {
public:
    template <typename T>
    T& get(const std::string& key) {
        const std::string typedKey = key + "#" + stateTypeKey<T>();
        auto it = entries_.find(typedKey);
        if (it == entries_.end()) {
            auto entry = std::make_unique<StateEntry<T>>();
            T* value = &entry->value;
            entries_.emplace(typedKey, std::move(entry));
            return *value;
        }
        return static_cast<StateEntry<T>*>(it->second.get())->value;
    }

    void releasePrefix(const std::string& prefix) {
        if (prefix.empty()) {
            return;
        }
        const std::string childPrefix = prefix + ".";
        const std::string statePrefix = prefix + "#";
        for (auto it = entries_.begin(); it != entries_.end(); ) {
            const std::string& key = it->first;
            if (key == prefix ||
                key.rfind(childPrefix, 0) == 0 ||
                key.rfind(statePrefix, 0) == 0) {
                it = entries_.erase(it);
            } else {
                ++it;
            }
        }
    }

    void clear() {
        entries_.clear();
    }

private:
    struct StateEntryBase {
        virtual ~StateEntryBase() = default;
    };

    template <typename T>
    struct StateEntry : StateEntryBase {
        T value;
    };

    template <typename T>
    static std::string stateTypeKey() {
        static int token = 0;
        return std::to_string(reinterpret_cast<std::uintptr_t>(&token));
    }

    std::unordered_map<std::string, std::unique_ptr<StateEntryBase>> entries_;
};

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

    Derived& lineGap(float value) {
        element_->lineSpacing = std::max(0.0f, value);
        return self();
    }

    Derived& padding(float value) {
        element_->padding = EdgeInsets::all(std::max(0.0f, value));
        return self();
    }

    Derived& padding(float horizontal, float vertical) {
        element_->padding = {
            std::max(0.0f, horizontal),
            std::max(0.0f, vertical),
            std::max(0.0f, horizontal),
            std::max(0.0f, vertical)
        };
        return self();
    }

    Derived& padding(float left, float top, float right, float bottom) {
        element_->padding = {
            std::max(0.0f, left),
            std::max(0.0f, top),
            std::max(0.0f, right),
            std::max(0.0f, bottom)
        };
        return self();
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

    Derived& minWidth(float value) {
        element_->minLayoutWidth = std::max(0.0f, value);
        return self();
    }

    Derived& minHeight(float value) {
        element_->minLayoutHeight = std::max(0.0f, value);
        return self();
    }

    Derived& minSize(float widthValue, float heightValue) {
        element_->minLayoutWidth = std::max(0.0f, widthValue);
        element_->minLayoutHeight = std::max(0.0f, heightValue);
        return self();
    }

    Derived& maxWidth(float value) {
        element_->maxLayoutWidth = std::max(0.0f, value);
        return self();
    }

    Derived& maxHeight(float value) {
        element_->maxLayoutHeight = std::max(0.0f, value);
        return self();
    }

    Derived& maxSize(float widthValue, float heightValue) {
        element_->maxLayoutWidth = std::max(0.0f, widthValue);
        element_->maxLayoutHeight = std::max(0.0f, heightValue);
        return self();
    }

    Derived& flexGrow(float value = 1.0f) {
        element_->flexGrow = std::max(0.0f, value);
        return self();
    }

    Derived& flexShrink(float value = 1.0f) {
        element_->flexShrink = std::max(0.0f, value);
        return self();
    }

    Derived& flex(float grow, float shrink = 1.0f) {
        element_->flexGrow = std::max(0.0f, grow);
        element_->flexShrink = std::max(0.0f, shrink);
        return self();
    }

    Derived& ignoreLayout(bool value = true) {
        element_->ignoreLayout = value;
        return self();
    }

    Derived& zIndex(int value) {
        element_->zIndex = value;
        return self();
    }

    Derived& clip(bool value = true) {
        element_->clip = value;
        return self();
    }

    Derived& pressedScale(float value) {
        element_->pressedScale = std::clamp(value, 0.80f, 1.0f);
        return self();
    }

    Derived& interactive(bool value = true) {
        element_->interactive = value;
        if (value) {
            element_->cursor = CursorShape::Hand;
        }
        return self();
    }

    Derived& focusable(bool value = true) {
        element_->focusable = value;
        element_->interactive = value || element_->interactive;
        return self();
    }

    Derived& preserveFocusOnPress(bool value = true) {
        element_->preserveFocusOnPress = value;
        return self();
    }

    Derived& imeRect(float xValue, float yValue, float widthValue, float heightValue) {
        element_->hasImeRect = true;
        element_->imeRect = {
            xValue,
            yValue,
            std::max(0.0f, widthValue),
            std::max(0.0f, heightValue)
        };
        return self();
    }

    Derived& disabled(bool value = true) {
        element_->disabled = value;
        return self();
    }

    Derived& cursor(CursorShape value) {
        element_->cursor = value;
        return self();
    }

    Derived& hitTestMode(HitTestMode value) {
        element_->hitTestMode = value;
        return self();
    }

    Derived& transformedHitTest(bool value = true) {
        element_->hitTestMode = value ? HitTestMode::Transformed : HitTestMode::Layout;
        return self();
    }

    Derived& opacity(float value) {
        element_->opacity = std::clamp(value, 0.0f, 1.0f);
        return self();
    }

    Derived& translate(float xValue, float yValue) {
        element_->transform.translate = {xValue, yValue};
        return self();
    }

    Derived& translate3d(float xValue, float yValue, float zValue) {
        element_->transform.translate = {xValue, yValue};
        element_->transform.translateZ = zValue;
        return self();
    }

    Derived& translateX(float value) {
        element_->transform.translate.x = value;
        return self();
    }

    Derived& translateY(float value) {
        element_->transform.translate.y = value;
        return self();
    }

    Derived& translateZ(float value) {
        element_->transform.translateZ = value;
        return self();
    }

    Derived& scale(float value) {
        element_->transform.scale = {value, value};
        return self();
    }

    Derived& scale(float xValue, float yValue) {
        element_->transform.scale = {xValue, yValue};
        return self();
    }

    Derived& rotate(float radians) {
        element_->transform.rotate = radians;
        return self();
    }

    Derived& rotateX(float radians) {
        element_->transform.rotateX = radians;
        return self();
    }

    Derived& rotateY(float radians) {
        element_->transform.rotateY = radians;
        return self();
    }

    Derived& rotateZ(float radians) {
        element_->transform.rotate = radians;
        return self();
    }

    Derived& perspective(float value) {
        element_->transform.perspective = std::max(0.0f, value);
        return self();
    }

    Derived& transformOrigin(float xValue, float yValue) {
        element_->transform.origin = {xValue, yValue};
        return self();
    }

    Derived& smoothStates(bool value = true) {
        element_->smoothStateColors = value;
        return self();
    }

    Derived& instantStates() {
        element_->smoothStateColors = false;
        return self();
    }

    Derived& onClick(std::function<void()> callback) {
        element_->interactive = true;
        element_->cursor = CursorShape::Hand;
        element_->onClick = std::move(callback);
        return self();
    }

    Derived& onPress(std::function<void(const PointerEvent&, const Rect&)> callback) {
        element_->interactive = true;
        element_->cursor = CursorShape::Hand;
        element_->onPress = std::move(callback);
        return self();
    }

    Derived& onRelease(std::function<void(const PointerEvent&, const Rect&)> callback) {
        element_->interactive = true;
        element_->cursor = CursorShape::Hand;
        element_->onRelease = std::move(callback);
        return self();
    }

    Derived& onMove(std::function<bool(const PointerEvent&, const Rect&)> callback) {
        element_->interactive = true;
        element_->cursor = CursorShape::Hand;
        element_->onMove = std::move(callback);
        return self();
    }

    template <typename Callback,
              typename = std::enable_if_t<!std::is_convertible_v<Callback, std::function<bool(const PointerEvent&, const Rect&)>>>>
    Derived& onMove(Callback callback) {
        element_->interactive = true;
        element_->cursor = CursorShape::Hand;
        element_->onMove = [callback = std::move(callback)](const PointerEvent& event, const Rect& bounds) mutable {
            callback(event, bounds);
            return true;
        };
        return self();
    }

    Derived& onContextMenu(std::function<void(const PointerEvent&, const Rect&)> callback) {
        element_->interactive = true;
        element_->cursor = CursorShape::Hand;
        element_->onContextMenu = std::move(callback);
        return self();
    }

    Derived& onHover(std::function<void(bool)> callback) {
        element_->interactive = true;
        element_->onHoverChanged = std::move(callback);
        return self();
    }

    Derived& onFocusChanged(std::function<void(bool)> callback) {
        element_->focusable = true;
        element_->interactive = true;
        element_->onFocusChanged = std::move(callback);
        return self();
    }

    Derived& onTextInput(std::function<void(const KeyboardEvent&)> callback) {
        element_->focusable = true;
        element_->interactive = true;
        element_->onTextInput = std::move(callback);
        return self();
    }

    Derived& onScroll(std::function<void(const ScrollEvent&)> callback) {
        element_->interactive = true;
        element_->onScroll = std::move(callback);
        return self();
    }

    Derived& onScrollOffsetChanged(std::function<void(float)> callback) {
        element_->interactive = true;
        element_->onScrollOffsetChanged = std::move(callback);
        return self();
    }

    Derived& composeOnScrollOffsetChange(bool value = true) {
        element_->composeOnScrollOffsetChange = value;
        return self();
    }

    Derived& onDrag(std::function<void(const DragEvent&)> callback) {
        element_->interactive = true;
        element_->onDrag = std::move(callback);
        return self();
    }

    Derived& onTimer(float seconds, std::function<void()> callback) {
        element_->timerSeconds = std::max(0.0f, seconds);
        element_->onTimer = std::move(callback);
        return self();
    }

    Derived& onFrame(std::function<void(float)> callback) {
        element_->onFrame = std::move(callback);
        return self();
    }

    Derived& visualStateFrom(const std::string& id, float pressedScaleValue = 0.965f);
    Derived& hoverOpacityFrom(const std::string& id, float hiddenOpacity = 0.0f, float visibleOpacity = 1.0f);
    Derived& runtimePointerTransformFrom(const std::string& id,
                                         float maxRotateXRadians = 0.0f,
                                         float maxRotateYRadians = 0.0f,
                                         float hoverScale = 1.0f,
                                         float maxTranslateX = 0.0f,
                                         float maxTranslateY = 0.0f);
    Derived& runtimePointerTiltFrom(const std::string& id, float maxTiltRadians, float hoverScale = 1.0f);
    Derived& scrollState(const std::string& id, float offset, float maxOffset, float step = 48.0f);
    Derived& scrollContentFrom(const std::string& id);
    Derived& scrollDragFrom(const std::string& id, float travel);
    Derived& scrollThumbFrom(const std::string& id, float travel);
    Derived& sliderState(const std::string& id,
                         float value,
                         float width,
                         float knobSize,
                         std::function<void(float)> callback = {});
    Derived& sliderInputFrom(const std::string& id);
    Derived& sliderFillFrom(const std::string& id);
    Derived& sliderKnobFrom(const std::string& id);

    Derived& transition(const Transition& value) {
        element_->transition = value;
        return self();
    }

    Derived& transition(float durationSeconds, Ease ease = Ease::OutCubic) {
        element_->transition = Transition::make(durationSeconds, ease);
        return self();
    }

    Derived& dirtyKey(std::string value) {
        element_->dirtyKey = std::move(value);
        return self();
    }

    Derived& animate(AnimProperty property) {
        element_->transition.enabled = true;
        element_->transition.properties = property;
        if (hasAnimProperty(property, AnimProperty::Frame)) {
            element_->explicitFrameAnimation = true;
        }
        return self();
    }

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

    Derived& gradient(const Gradient& value) {
        this->element_->gradient = value;
        return this->self();
    }

    Derived& gradient(const Color& start, const Color& end, GradientDirection direction = GradientDirection::Vertical) {
        this->element_->gradient = {true, start, end, direction};
        return this->self();
    }

    Derived& radius(float value) {
        this->element_->radius = std::max(0.0f, value);
        return this->self();
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
        this->element_->shadow = {true, {0.0f, offsetY}, std::max(0.0f, blur), 0.0f, colorValue, false};
        return this->self();
    }

    Derived& shadow(float blur, float offsetX, float offsetY, const Color& colorValue) {
        this->element_->shadow = {true, {offsetX, offsetY}, std::max(0.0f, blur), 0.0f, colorValue, false};
        return this->self();
    }

    Derived& insetShadow(float blur, float offsetY, const Color& colorValue) {
        this->element_->shadow = {true, {0.0f, offsetY}, std::max(0.0f, blur), 0.0f, colorValue, true};
        return this->self();
    }

    Derived& insetShadow(float blur, float offsetX, float offsetY, const Color& colorValue) {
        this->element_->shadow = {true, {offsetX, offsetY}, std::max(0.0f, blur), 0.0f, colorValue, true};
        return this->self();
    }

    Derived& shadow(const Shadow& value) {
        this->element_->shadow = value;
        return this->self();
    }

    Derived& blur(float value) {
        this->element_->blur = std::max(0.0f, value);
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

    Derived& translate3d(float xValue, float yValue, float zValue) {
        this->element_->transform.translate = {xValue, yValue};
        this->element_->transform.translateZ = zValue;
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

    Derived& translateZ(float value) {
        this->element_->transform.translateZ = value;
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

    Derived& rotateX(float radians) {
        this->element_->transform.rotateX = radians;
        return this->self();
    }

    Derived& rotateY(float radians) {
        this->element_->transform.rotateY = radians;
        return this->self();
    }

    Derived& rotateZ(float radians) {
        this->element_->transform.rotate = radians;
        return this->self();
    }

    Derived& perspective(float value) {
        this->element_->transform.perspective = std::max(0.0f, value);
        return this->self();
    }

    Derived& transformOrigin(float xValue, float yValue) {
        this->element_->transform.origin = {xValue, yValue};
        return this->self();
    }

    Derived& interactive(bool value = true) {
        this->element_->interactive = value;
        if (value) {
            this->element_->cursor = CursorShape::Hand;
        }
        return this->self();
    }

    Derived& hoverColor(const Color& value) {
        this->element_->hoverColor = value;
        this->element_->hasStateColors = true;
        return this->self();
    }

    Derived& pressedColor(const Color& value) {
        this->element_->pressedColor = value;
        this->element_->hasStateColors = true;
        return this->self();
    }

    Derived& states(const Color& normal, const Color& hover, const Color& pressed) {
        this->element_->color = normal;
        this->element_->hoverColor = hover;
        this->element_->pressedColor = pressed;
        this->element_->hasStateColors = true;
        this->element_->interactive = true;
        this->element_->cursor = CursorShape::Hand;
        return this->self();
    }

    Derived& onClick(std::function<void()> callback) {
        this->element_->interactive = true;
        this->element_->cursor = CursorShape::Hand;
        this->element_->onClick = std::move(callback);
        return this->self();
    }

    Derived& onMove(std::function<bool(const PointerEvent&, const Rect&)> callback) {
        this->element_->interactive = true;
        this->element_->cursor = CursorShape::Hand;
        this->element_->onMove = std::move(callback);
        return this->self();
    }

    template <typename Callback,
              typename = std::enable_if_t<!std::is_convertible_v<Callback, std::function<bool(const PointerEvent&, const Rect&)>>>>
    Derived& onMove(Callback callback) {
        this->element_->interactive = true;
        this->element_->cursor = CursorShape::Hand;
        this->element_->onMove = [callback = std::move(callback)](const PointerEvent& event, const Rect& bounds) mutable {
            callback(event, bounds);
            return true;
        };
        return this->self();
    }

    Derived& onContextMenu(std::function<void(const PointerEvent&, const Rect&)> callback) {
        this->element_->interactive = true;
        this->element_->cursor = CursorShape::Hand;
        this->element_->onContextMenu = std::move(callback);
        return this->self();
    }

};

class LayoutBuilder : public BuilderBase<LayoutBuilder> {
public:
    LayoutBuilder(Ui& ui, Element* element) : BuilderBase<LayoutBuilder>(ui, element) {}
};

class LoaderBuilder {
public:
    LoaderBuilder(Ui& ui, std::string id);

    LoaderBuilder& active(bool value = true) {
        active_ = value;
        return *this;
    }

    LoaderBuilder& mode(LoaderMode value) {
        mode_ = value;
        return *this;
    }

    LoaderBuilder& destroyOnHide() {
        mode_ = LoaderMode::DestroyOnHide;
        return *this;
    }

    LoaderBuilder& keepAlive() {
        mode_ = LoaderMode::KeepAlive;
        return *this;
    }

    template <typename Fn>
    LoaderBuilder& content(Fn&& compose);

    void build() {}

private:
    Ui* ui_ = nullptr;
    std::string id_;
    bool active_ = true;
    LoaderMode mode_ = LoaderMode::DestroyOnHide;
};

class RectBuilder : public ShapeBuilderBase<RectBuilder> {
public:
    RectBuilder(Ui& ui, Element* element) : ShapeBuilderBase<RectBuilder>(ui, element) {}
};

class PolygonBuilder : public ShapeBuilderBase<PolygonBuilder> {
public:
    PolygonBuilder(Ui& ui, Element* element) : ShapeBuilderBase<PolygonBuilder>(ui, element) {}

    PolygonBuilder& points(std::vector<Vec2> value) {
        element_->polygonPoints = std::move(value);
        return *this;
    }

    PolygonBuilder& point(float x, float y) {
        element_->polygonPoints.push_back({x, y});
        return *this;
    }

    PolygonBuilder& clearPoints() {
        element_->polygonPoints.clear();
        return *this;
    }
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

    TextBuilder& opacity(float value) {
        element_->opacity = std::clamp(value, 0.0f, 1.0f);
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
        element_->fontFamily = "Icon";
        return *this;
    }

    TextBuilder& icon(const std::string& value) {
        element_->text = value;
        element_->fontFamily = "Icon";
        return *this;
    }

};

class ImageBuilder : public BuilderBase<ImageBuilder> {
public:
    ImageBuilder(Ui& ui, Element* element) : BuilderBase<ImageBuilder>(ui, element) {}

    ImageBuilder& source(const std::string& value) {
        element_->imageSource = value;
        return *this;
    }

    ImageBuilder& bingDaily(int idx = 0, const std::string& mkt = "zh-CN") {
        element_->imageSource = "bing://daily?idx=" + std::to_string(std::max(0, idx)) + "&mkt=" + mkt;
        return *this;
    }

    ImageBuilder& tint(const Color& value) {
        element_->color = value;
        return *this;
    }

    ImageBuilder& radius(float value) {
        element_->radius = std::max(0.0f, value);
        return *this;
    }

    ImageBuilder& opacity(float value) {
        element_->opacity = std::clamp(value, 0.0f, 1.0f);
        return *this;
    }

    ImageBuilder& flipVertically(bool value = true) {
        element_->imageFlipVertically = value;
        return *this;
    }

    ImageBuilder& fit(ImageFit value) {
        element_->imageFit = value;
        return *this;
    }

    ImageBuilder& cover() {
        return fit(ImageFit::Cover);
    }

    ImageBuilder& contain() {
        return fit(ImageFit::Contain);
    }

    ImageBuilder& stretch() {
        return fit(ImageFit::Stretch);
    }

    ImageBuilder& coverViewport(float canvasWidth, float canvasHeight, float offsetX = 0.0f, float offsetY = 0.0f) {
        element_->imageHasCoverViewport = true;
        element_->imageCoverViewportSize = {std::max(0.0f, canvasWidth), std::max(0.0f, canvasHeight)};
        element_->imageCoverViewportOffset = {std::max(0.0f, offsetX), std::max(0.0f, offsetY)};
        return *this;
    }

    ImageBuilder& translate(float xValue, float yValue) {
        element_->transform.translate = {xValue, yValue};
        return *this;
    }

    ImageBuilder& translate3d(float xValue, float yValue, float zValue) {
        element_->transform.translate = {xValue, yValue};
        element_->transform.translateZ = zValue;
        return *this;
    }

    ImageBuilder& translateX(float value) {
        element_->transform.translate.x = value;
        return *this;
    }

    ImageBuilder& translateY(float value) {
        element_->transform.translate.y = value;
        return *this;
    }

    ImageBuilder& translateZ(float value) {
        element_->transform.translateZ = value;
        return *this;
    }

    ImageBuilder& scale(float value) {
        element_->transform.scale = {value, value};
        return *this;
    }

    ImageBuilder& scale(float xValue, float yValue) {
        element_->transform.scale = {xValue, yValue};
        return *this;
    }

    ImageBuilder& rotate(float radians) {
        element_->transform.rotate = radians;
        return *this;
    }

    ImageBuilder& rotateX(float radians) {
        element_->transform.rotateX = radians;
        return *this;
    }

    ImageBuilder& rotateY(float radians) {
        element_->transform.rotateY = radians;
        return *this;
    }

    ImageBuilder& rotateZ(float radians) {
        element_->transform.rotate = radians;
        return *this;
    }

    ImageBuilder& perspective(float value) {
        element_->transform.perspective = std::max(0.0f, value);
        return *this;
    }

    ImageBuilder& transformOrigin(float xValue, float yValue) {
        element_->transform.origin = {xValue, yValue};
        return *this;
    }
};

class SvgBuilder : public ImageBuilder {
public:
    SvgBuilder(Ui& ui, Element* element) : ImageBuilder(ui, element) {}

    SvgBuilder& source(std::string value) {
        element_->svgSource = std::move(value);
        element_->imageSource.clear();
        return *this;
    }

};

class Ui {
public:
    void begin(const std::string& pageId = "") {
        pageId_ = pageId;
        roots_.clear();
        orderedRoots_.clear();
        hasDependentVisuals_ = false;
        hasBackdropBlur_ = false;
        stack_.clear();
        scopeStack_.clear();
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

    LayoutBuilder flow(const std::string& id = "") {
        return LayoutBuilder(*this, addElement(ElementKind::Flow, id));
    }

    LayoutBuilder stack(const std::string& id = "") {
        return LayoutBuilder(*this, addElement(ElementKind::Stack, id));
    }

    LoaderBuilder loader(const std::string& id) {
        return LoaderBuilder(*this, id);
    }

    RectBuilder rect(const std::string& id) {
        return RectBuilder(*this, addElement(ElementKind::Rect, id));
    }

    PolygonBuilder polygon(const std::string& id) {
        return PolygonBuilder(*this, addElement(ElementKind::Polygon, id));
    }

    TextBuilder text(const std::string& id) {
        return TextBuilder(*this, addElement(ElementKind::Text, id));
    }

    ImageBuilder image(const std::string& id) {
        return ImageBuilder(*this, addElement(ElementKind::Image, id));
    }

    SvgBuilder svg(const std::string& id) {
        return SvgBuilder(*this, addElement(ElementKind::Svg, id));
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
        rebuildOrderedElements();
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

    const std::vector<const Element*>& orderedRoots() const {
        return orderedRoots_;
    }

    bool hasDependentVisuals() const {
        return hasDependentVisuals_;
    }

    bool hasBackdropBlur() const {
        return hasBackdropBlur_;
    }

    bool isFocused(const std::string& id) const {
        return !focusedId_.empty() && focusedId_ == resolveId(id);
    }

    template <typename T>
    T& state(const std::string& id) {
        return stateStore_.get<T>(resolveId(id));
    }

    void releaseStateScope(const std::string& id) {
        const std::string scope = resolveId(id);
        stateStore_.releasePrefix(scope);
        releasedStateScopes_.push_back(scope);
    }

    void clearState() {
        stateStore_.clear();
        releasedStateScopes_.clear();
    }

private:
    friend class Runtime;
    friend class LoaderBuilder;
    friend class BuilderBase<LayoutBuilder>;
    friend class BuilderBase<RectBuilder>;
    friend class BuilderBase<PolygonBuilder>;
    friend class BuilderBase<TextBuilder>;
    friend class BuilderBase<ImageBuilder>;

    void setFocusedId(const std::string& id) {
        focusedId_ = id;
    }

    std::vector<std::string> consumeReleasedStateScopes() {
        std::vector<std::string> scopes = std::move(releasedStateScopes_);
        releasedStateScopes_.clear();
        return scopes;
    }

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

    void pushScope(const std::string& id) {
        const std::string scope = normalizeScopeId(id);
        if (!scope.empty()) {
            scopeStack_.push_back(scope);
        }
    }

    void popScope() {
        if (!scopeStack_.empty()) {
            scopeStack_.pop_back();
        }
    }

    std::string resolveId(const std::string& id) const {
        if (id.empty()) {
            return id;
        }

        std::string scopePrefix = pageId_;
        for (const std::string& scope : scopeStack_) {
            if (!scopePrefix.empty()) {
                scopePrefix += ".";
            }
            scopePrefix += scope;
        }
        if (scopePrefix.empty()) {
            return id;
        }

        const std::string prefix = scopePrefix + ".";
        if (id.rfind(prefix, 0) == 0) {
            return id;
        }
        return scopePrefix + "." + id;
    }

    std::string normalizeScopeId(const std::string& id) const {
        if (id.empty()) {
            return {};
        }
        const std::string pagePrefix = pageId_.empty() ? std::string{} : pageId_ + ".";
        std::string scope = id;
        if (!pagePrefix.empty() && scope.rfind(pagePrefix, 0) == 0) {
            scope = scope.substr(pagePrefix.size());
        }
        for (const std::string& parent : scopeStack_) {
            const std::string parentPrefix = parent + ".";
            if (scope.rfind(parentPrefix, 0) == 0) {
                scope = scope.substr(parentPrefix.size());
            }
        }
        return scope;
    }

    std::string makeGeneratedId(ElementKind kind) {
        const char* prefix = "__stack";
        if (kind == ElementKind::Row) {
            prefix = "__row";
        } else if (kind == ElementKind::Column) {
            prefix = "__column";
        } else if (kind == ElementKind::Rect) {
            prefix = "__rect";
        } else if (kind == ElementKind::Polygon) {
            prefix = "__polygon";
        } else if (kind == ElementKind::Text) {
            prefix = "__text";
        } else if (kind == ElementKind::Image) {
            prefix = "__image";
        } else if (kind == ElementKind::Svg) {
            prefix = "__svg";
        }
        return resolveId(std::string(prefix) + "." + std::to_string(generatedId_++));
    }

    static std::unique_ptr<Node> buildLayoutNode(Element& element, std::vector<std::pair<Element*, Node*>>& links) {
        auto node = std::make_unique<Node>(element.layoutType());
        node->setWidth(element.width);
        node->setHeight(element.height);
        node->setMargin(element.margin);
        node->setPadding(element.padding);
        node->setPosition(element.x, element.y, element.hasX, element.hasY);
        node->setSpacing(element.spacing);
        node->setLineSpacing(element.lineSpacing);
        node->setMainAlign(element.mainAlign);
        node->setCrossAlign(element.crossAlign);
        node->setMinWidth(element.minLayoutWidth);
        node->setMinHeight(element.minLayoutHeight);
        node->setMaxWidth(element.maxLayoutWidth);
        node->setMaxHeight(element.maxLayoutHeight);
        node->setFlexGrow(element.flexGrow);
        node->setFlexShrink(element.flexShrink);
        node->setIgnoreLayout(element.ignoreLayout);

        Node* raw = node.get();
        links.push_back({&element, raw});
        for (auto& child : element.children) {
            raw->addChild(buildLayoutNode(*child, links));
        }
        return node;
    }

    void rebuildOrderedElements() {
        orderedRoots_.clear();
        orderedRoots_.reserve(roots_.size());
        hasDependentVisuals_ = false;
        hasBackdropBlur_ = false;
        for (const auto& root : roots_) {
            orderedRoots_.push_back(root.get());
            rebuildOrderedChildren(*root);
            hasDependentVisuals_ = hasDependentVisuals_ || root->subtreeHasDependentVisuals;
            hasBackdropBlur_ = hasBackdropBlur_ || root->subtreeHasBackdropBlur;
        }
        std::stable_sort(orderedRoots_.begin(), orderedRoots_.end(), [](const Element* a, const Element* b) {
            return a->zIndex < b->zIndex;
        });
    }

    static void rebuildOrderedChildren(Element& element) {
        element.orderedChildren.clear();
        element.orderedChildren.reserve(element.children.size());
        element.subtreeNeedsUpdate = elementNeedsUpdate(element);
        element.subtreeHasDependentVisuals = elementHasDependentVisuals(element);
        element.subtreeHasBackdropBlur = elementHasBackdropBlur(element);
        element.subtreeBlocksRetainedLayer = elementBlocksRetainedLayer(element);
        for (const auto& child : element.children) {
            element.orderedChildren.push_back(child.get());
            rebuildOrderedChildren(*child);
            element.subtreeNeedsUpdate = element.subtreeNeedsUpdate || child->subtreeNeedsUpdate;
            element.subtreeHasDependentVisuals = element.subtreeHasDependentVisuals || child->subtreeHasDependentVisuals;
            element.subtreeHasBackdropBlur = element.subtreeHasBackdropBlur || child->subtreeHasBackdropBlur;
            element.subtreeBlocksRetainedLayer = element.subtreeBlocksRetainedLayer || child->subtreeBlocksRetainedLayer;
        }
        std::stable_sort(element.orderedChildren.begin(), element.orderedChildren.end(), [](const Element* a, const Element* b) {
            return a->zIndex < b->zIndex;
        });
    }

    static bool elementNeedsUpdate(const Element& element) {
        return element.interactive ||
               element.focusable ||
               element.disabled ||
               element.hasImeRect ||
               element.onClick ||
               element.onPress ||
               element.onRelease ||
               element.onMove ||
               element.onContextMenu ||
               element.onHoverChanged ||
               element.onFocusChanged ||
               element.onTextInput ||
               element.onScroll ||
               element.onScrollOffsetChanged ||
               element.onDrag ||
               element.onTimer ||
               element.onFrame ||
               element.timerSeconds > 0.0f ||
               element.transition.enabled ||
               !element.visualStateSourceId.empty() ||
               !element.hoverOpacitySourceId.empty() ||
               !element.pointerRuntimeSourceId.empty() ||
               !element.scrollStateId.empty() ||
               !element.scrollContentSourceId.empty() ||
               !element.scrollDragSourceId.empty() ||
               !element.scrollThumbSourceId.empty() ||
               !element.sliderStateId.empty() ||
               !element.sliderInputSourceId.empty() ||
               !element.sliderFillSourceId.empty() ||
               !element.sliderKnobSourceId.empty() ||
               !element.dirtyKey.empty() ||
               (element.kind == ElementKind::Image && !element.imageSource.empty()) ||
               element.kind == ElementKind::Svg;
    }

    static bool elementHasDependentVisuals(const Element& element) {
        return !element.visualStateSourceId.empty() || !element.hoverOpacitySourceId.empty();
    }

    static bool elementHasBackdropBlur(const Element& element) {
        return element.kind == ElementKind::Rect &&
               (element.blur > 0.0f ||
                (element.transition.enabled && hasAnimProperty(element.transition.properties, AnimProperty::Blur)));
    }

    static bool elementBlocksRetainedLayer(const Element& element) {
        return element.clip ||
               element.interactive ||
               element.focusable ||
               element.hasImeRect ||
               element.onClick ||
               element.onPress ||
               element.onRelease ||
               element.onMove ||
               element.onContextMenu ||
               element.onHoverChanged ||
               element.onFocusChanged ||
               element.onTextInput ||
               element.onScroll ||
               element.onScrollOffsetChanged ||
               element.onDrag ||
               element.onTimer ||
               element.onFrame ||
               element.timerSeconds > 0.0f ||
               !element.visualStateSourceId.empty() ||
               !element.hoverOpacitySourceId.empty() ||
               !element.pointerRuntimeSourceId.empty() ||
               !element.scrollStateId.empty() ||
               !element.scrollContentSourceId.empty() ||
               !element.scrollDragSourceId.empty() ||
               !element.scrollThumbSourceId.empty() ||
               !element.sliderStateId.empty() ||
               !element.sliderInputSourceId.empty() ||
               !element.sliderFillSourceId.empty() ||
               !element.sliderKnobSourceId.empty() ||
               !element.dirtyKey.empty() ||
               (element.kind == ElementKind::Image && !element.imageSource.empty()) ||
               element.kind == ElementKind::Svg;
    }

    std::string pageId_;
    std::vector<std::string> scopeStack_;
    std::vector<std::unique_ptr<Element>> roots_;
    std::vector<const Element*> orderedRoots_;
    bool hasDependentVisuals_ = false;
    bool hasBackdropBlur_ = false;
    std::vector<Element*> stack_;
    std::unordered_map<std::string, Element*> index_;
    std::string focusedId_;
    StateStore stateStore_;
    std::vector<std::string> releasedStateScopes_;
    std::size_t generatedId_ = 0;
};

inline LoaderBuilder::LoaderBuilder(Ui& ui, std::string id)
    : ui_(&ui), id_(std::move(id)) {}

template <typename Fn>
LoaderBuilder& LoaderBuilder::content(Fn&& compose) {
    if (ui_ == nullptr) {
        return *this;
    }
    if (!active_) {
        if (mode_ == LoaderMode::DestroyOnHide) {
            ui_->releaseStateScope(id_);
        }
        return *this;
    }

    ui_->pushScope(id_);
    std::forward<Fn>(compose)();
    ui_->popScope();
    return *this;
}

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

template <typename Derived>
Derived& BuilderBase<Derived>::hoverOpacityFrom(const std::string& id, float hiddenOpacity, float visibleOpacity) {
    element_->hoverOpacitySourceId = ui_->resolveId(id);
    element_->hoverHiddenOpacity = std::clamp(hiddenOpacity, 0.0f, 1.0f);
    element_->hoverVisibleOpacity = std::clamp(visibleOpacity, 0.0f, 1.0f);
    return self();
}

template <typename Derived>
Derived& BuilderBase<Derived>::runtimePointerTransformFrom(const std::string& id,
                                                           float maxRotateXRadians,
                                                           float maxRotateYRadians,
                                                           float hoverScale,
                                                           float maxTranslateX,
                                                           float maxTranslateY) {
    element_->pointerRuntimeSourceId = ui_->resolveId(id);
    element_->pointerRuntimeMaxRotateX = std::clamp(maxRotateXRadians, -0.80f, 0.80f);
    element_->pointerRuntimeMaxRotateY = std::clamp(maxRotateYRadians, -0.80f, 0.80f);
    element_->pointerRuntimeAmount = std::max({
        std::abs(element_->pointerRuntimeMaxRotateX),
        std::abs(element_->pointerRuntimeMaxRotateY),
        std::abs(maxTranslateX),
        std::abs(maxTranslateY)
    });
    element_->pointerRuntimeHoverScale = std::clamp(hoverScale, 0.50f, 2.0f);
    element_->pointerRuntimeTranslate = {maxTranslateX, maxTranslateY};
    return self();
}

template <typename Derived>
Derived& BuilderBase<Derived>::runtimePointerTiltFrom(const std::string& id, float maxTiltRadians, float hoverScale) {
    const float tilt = std::clamp(maxTiltRadians, 0.0f, 0.80f);
    runtimePointerTransformFrom(id, tilt, tilt, hoverScale);
    return self();
}

template <typename Derived>
Derived& BuilderBase<Derived>::scrollState(const std::string& id, float offset, float maxOffset, float step) {
    element_->scrollStateId = ui_->resolveId(id);
    element_->scrollOffset = std::max(0.0f, offset);
    element_->scrollMaxOffset = std::max(0.0f, maxOffset);
    element_->scrollStep = std::max(1.0f, step);
    element_->interactive = true;
    return self();
}

template <typename Derived>
Derived& BuilderBase<Derived>::scrollContentFrom(const std::string& id) {
    element_->scrollContentSourceId = ui_->resolveId(id);
    return self();
}

template <typename Derived>
Derived& BuilderBase<Derived>::scrollDragFrom(const std::string& id, float travel) {
    element_->scrollDragSourceId = ui_->resolveId(id);
    element_->scrollDragTravel = std::max(0.0f, travel);
    element_->interactive = true;
    return self();
}

template <typename Derived>
Derived& BuilderBase<Derived>::scrollThumbFrom(const std::string& id, float travel) {
    element_->scrollThumbSourceId = ui_->resolveId(id);
    element_->scrollThumbTravel = std::max(0.0f, travel);
    return self();
}

template <typename Derived>
Derived& BuilderBase<Derived>::sliderState(const std::string& id,
                                           float value,
                                           float width,
                                           float knobSize,
                                           std::function<void(float)> callback) {
    element_->sliderStateId = ui_->resolveId(id);
    element_->sliderValue = std::clamp(value, 0.0f, 1.0f);
    element_->sliderWidth = std::max(0.0f, width);
    element_->sliderKnobSize = std::max(0.0f, knobSize);
    element_->onSliderValueChanged = std::move(callback);
    return self();
}

template <typename Derived>
Derived& BuilderBase<Derived>::sliderInputFrom(const std::string& id) {
    element_->sliderInputSourceId = ui_->resolveId(id);
    element_->interactive = true;
    return self();
}

template <typename Derived>
Derived& BuilderBase<Derived>::sliderFillFrom(const std::string& id) {
    element_->sliderFillSourceId = ui_->resolveId(id);
    return self();
}

template <typename Derived>
Derived& BuilderBase<Derived>::sliderKnobFrom(const std::string& id) {
    element_->sliderKnobSourceId = ui_->resolveId(id);
    return self();
}

} // namespace core::dsl
