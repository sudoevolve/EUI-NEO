#pragma once

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#include "core/dsl.h"
#include "core/event.h"

#include <cmath>
#include <memory>
#include <string>
#include <unordered_map>

namespace core::dsl {

class Runtime {
    struct RenderTransform {
        bool active = false;
        Vec2 origin = {0.0f, 0.0f};
        float scale = 1.0f;
    };

public:
    bool initialize() {
        return true;
    }

    template <typename ComposeFn>
    void compose(const std::string& pageId, float logicalWidth, float logicalHeight, ComposeFn&& composeFn) {
        const Screen screen{logicalWidth, logicalHeight};
        ui_.begin(pageId);
        composeFn(ui_, screen);
        ui_.end();
        ui_.layout(screen);

        if (logicalWidth_ != logicalWidth || logicalHeight_ != logicalHeight) {
            needsRender_ = true;
        }
        logicalWidth_ = logicalWidth;
        logicalHeight_ = logicalHeight;
    }

    bool update(GLFWwindow* window, float deltaSeconds, float pointerScale, float dpiScale) {
        const PointerEvent event = readPointerEvent(window, pointerScale);
        animating_ = false;

        forEachElement([&](const Element& element) {
            if (element.kind != ElementKind::Rect || !element.interactive) {
                return;
            }

            RectInstance& instance = rectInstance(element.id);
            const Rect bounds = toPixelRect(element.frame, dpiScale);
            instance.interaction.update(bounds, event);

            const float oldHover = instance.hoverBlend;
            const float oldPress = instance.pressBlend;
            instance.hoverBlend = animateToward(instance.hoverBlend, instance.interaction.hover ? 1.0f : 0.0f, 9.0f, deltaSeconds);
            instance.pressBlend = animateToward(instance.pressBlend, instance.interaction.pressed ? 1.0f : 0.0f, 16.0f, deltaSeconds);

            if (instance.interaction.clicked && element.onClick) {
                element.onClick();
            }

            const bool visualChanged = std::fabs(oldHover - instance.hoverBlend) > 0.0001f ||
                                       std::fabs(oldPress - instance.pressBlend) > 0.0001f ||
                                       instance.interaction.changed;
            needsRender_ = needsRender_ || visualChanged;
            animating_ = animating_ || isRectAnimating(element, instance);
        });

        const bool result = needsRender_;
        needsRender_ = false;
        return result;
    }

    bool isAnimating() const {
        return animating_;
    }

    void render(int windowWidth, int windowHeight, float dpiScale) {
        const RenderTransform identity;
        for (const auto& root : ui_.roots()) {
            renderElement(*root, windowWidth, windowHeight, dpiScale, identity);
        }
    }

    void shutdown() {
        for (auto& item : rects_) {
            if (item.second.initialized) {
                item.second.primitive->destroy();
            }
        }
        for (auto& item : texts_) {
            if (item.second.initialized) {
                item.second.primitive->destroy();
            }
        }
        rects_.clear();
        texts_.clear();
    }

private:
    struct RectInstance {
        std::unique_ptr<RoundedRectPrimitive> primitive = std::make_unique<RoundedRectPrimitive>();
        InteractionState interaction;
        bool initialized = false;
        float hoverBlend = 0.0f;
        float pressBlend = 0.0f;
    };

    struct TextInstance {
        std::unique_ptr<TextPrimitive> primitive = std::make_unique<TextPrimitive>();
        bool initialized = false;
    };

    template <typename Fn>
    void forEachElement(Fn&& fn) const {
        for (const auto& root : ui_.roots()) {
            forEachElement(*root, fn);
        }
    }

    template <typename Fn>
    static void forEachElement(const Element& element, Fn&& fn) {
        fn(element);
        for (const auto& child : element.children) {
            forEachElement(*child, fn);
        }
    }

    static float toPixels(float value, float dpiScale) {
        return value * dpiScale;
    }

    static Rect toPixelRect(const LayoutRect& frame, float dpiScale) {
        return {
            toPixels(frame.x, dpiScale),
            toPixels(frame.y, dpiScale),
            toPixels(frame.width, dpiScale),
            toPixels(frame.height, dpiScale)
        };
    }

    static Vec2 applyRenderTransform(Vec2 point, const RenderTransform& transform) {
        if (!transform.active) {
            return point;
        }
        return {
            transform.origin.x + (point.x - transform.origin.x) * transform.scale,
            transform.origin.y + (point.y - transform.origin.y) * transform.scale
        };
    }

    static Rect applyRenderTransform(const Rect& rect, const RenderTransform& transform) {
        if (!transform.active) {
            return rect;
        }

        const Vec2 topLeft = applyRenderTransform(Vec2{rect.x, rect.y}, transform);
        return {
            topLeft.x,
            topLeft.y,
            rect.width * transform.scale,
            rect.height * transform.scale
        };
    }

    static Border scaleBorder(Border border, float dpiScale) {
        border.width = toPixels(border.width, dpiScale);
        return border;
    }

    static Shadow scaleShadow(Shadow shadow, float dpiScale) {
        shadow.offset.x = toPixels(shadow.offset.x, dpiScale);
        shadow.offset.y = toPixels(shadow.offset.y, dpiScale);
        shadow.blur = toPixels(shadow.blur, dpiScale);
        shadow.spread = toPixels(shadow.spread, dpiScale);
        return shadow;
    }

    static Transform scaleTransform(Transform transform, float dpiScale) {
        transform.translate.x = toPixels(transform.translate.x, dpiScale);
        transform.translate.y = toPixels(transform.translate.y, dpiScale);
        return transform;
    }

    static float animateToward(float current, float target, float speed, float deltaSeconds) {
        if (deltaSeconds <= 0.0f) {
            return current;
        }

        const float next = target + (current - target) * std::exp(-speed * deltaSeconds);
        return std::fabs(next - target) < 0.001f ? target : next;
    }

    static bool isRectAnimating(const Element& element, const RectInstance& instance) {
        return std::fabs(instance.hoverBlend - (element.interactive && instance.interaction.hover ? 1.0f : 0.0f)) > 0.0001f ||
               std::fabs(instance.pressBlend - (element.interactive && instance.interaction.pressed ? 1.0f : 0.0f)) > 0.0001f;
    }

    RectInstance& rectInstance(const std::string& id) {
        return rects_.try_emplace(id).first->second;
    }

    TextInstance& textInstance(const std::string& id) {
        return texts_.try_emplace(id).first->second;
    }

    RenderTransform resolveRenderTransform(const Element& element, float dpiScale, const RenderTransform& inherited) {
        if (element.visualStateSourceId.empty()) {
            return inherited;
        }

        const auto instance = rects_.find(element.visualStateSourceId);
        if (instance == rects_.end()) {
            return inherited;
        }

        const float scale = 1.0f - (1.0f - element.pressedScale) * instance->second.pressBlend;
        if (std::fabs(scale - 1.0f) <= 0.0001f) {
            return inherited;
        }

        const Rect frame = applyRenderTransform(toPixelRect(element.frame, dpiScale), inherited);
        return {
            true,
            {frame.x + frame.width * 0.5f, frame.y + frame.height * 0.5f},
            scale
        };
    }

    void renderElement(const Element& element,
                       int windowWidth,
                       int windowHeight,
                       float dpiScale,
                       const RenderTransform& inheritedTransform) {
        const RenderTransform renderTransform = resolveRenderTransform(element, dpiScale, inheritedTransform);

        if (element.kind == ElementKind::Rect) {
            renderRect(element, windowWidth, windowHeight, dpiScale, renderTransform);
        } else if (element.kind == ElementKind::Text) {
            renderText(element, windowWidth, windowHeight, dpiScale, renderTransform);
        }

        for (const auto& child : element.children) {
            renderElement(*child, windowWidth, windowHeight, dpiScale, renderTransform);
        }
    }

    void renderRect(const Element& element,
                    int windowWidth,
                    int windowHeight,
                    float dpiScale,
                    const RenderTransform& renderTransform) {
        RectInstance& instance = rectInstance(element.id);
        if (!instance.initialized) {
            instance.initialized = instance.primitive->initialize();
            if (!instance.initialized) {
                return;
            }
        }

        const Rect frame = toPixelRect(element.frame, dpiScale);
        const float visualScale = renderTransform.active ? renderTransform.scale : 1.0f;
        const Color hoverColor = mixColor(element.color, element.hoverColor, instance.hoverBlend);
        const Color currentColor = mixColor(hoverColor, element.pressedColor, instance.pressBlend);
        Transform transform = scaleTransform(element.transform, dpiScale);
        if (renderTransform.active && frame.width > 0.0f && frame.height > 0.0f) {
            transform.origin = {
                (renderTransform.origin.x - frame.x) / frame.width,
                (renderTransform.origin.y - frame.y) / frame.height
            };
            transform.scale.x *= visualScale;
            transform.scale.y *= visualScale;
        }

        instance.primitive->setBounds(frame.x, frame.y, frame.width, frame.height);
        instance.primitive->setColor(currentColor);
        instance.primitive->setGradient(element.gradient);
        instance.primitive->setBorder(scaleBorder(element.border, dpiScale));
        instance.primitive->setShadow(scaleShadow(element.shadow, dpiScale));
        instance.primitive->setCornerRadius(toPixels(element.radius, dpiScale));
        instance.primitive->setOpacity(element.opacity);
        instance.primitive->setTransform(transform);
        instance.primitive->render(windowWidth, windowHeight);
    }

    void renderText(const Element& element,
                    int windowWidth,
                    int windowHeight,
                    float dpiScale,
                    const RenderTransform& renderTransform) {
        TextInstance& instance = textInstance(element.id);
        if (!instance.initialized) {
            instance.initialized = instance.primitive->initialize();
            if (!instance.initialized) {
                return;
            }
        }

        const Rect frame = toPixelRect(element.frame, dpiScale);
        const float visualScale = renderTransform.active ? renderTransform.scale : 1.0f;
        const float maxWidth = element.maxWidth > 0.0f ? toPixels(element.maxWidth, dpiScale) : frame.width;
        const float lineHeight = element.lineHeight > 0.0f ? toPixels(element.lineHeight, dpiScale) : 0.0f;

        float x = frame.x;
        if (element.horizontalAlign == HorizontalAlign::Center) {
            x = frame.x + frame.width * 0.5f;
        } else if (element.horizontalAlign == HorizontalAlign::Right) {
            x = frame.x + frame.width;
        }

        float y = frame.y;
        if (element.verticalAlign == VerticalAlign::Center) {
            y = frame.y + frame.height * 0.5f;
        } else if (element.verticalAlign == VerticalAlign::Bottom) {
            y = frame.y + frame.height;
        }

        instance.primitive->setPosition(x, y);
        instance.primitive->setVisualScale(renderTransform.origin.x, renderTransform.origin.y, visualScale);
        instance.primitive->setText(element.text);
        instance.primitive->setFontFamily(element.fontFamily);
        instance.primitive->setFontSize(toPixels(element.fontSize, dpiScale));
        instance.primitive->setFontWeight(element.fontWeight);
        instance.primitive->setColor(element.textColor);
        instance.primitive->setMaxWidth(maxWidth);
        instance.primitive->setWrap(element.wrap);
        instance.primitive->setHorizontalAlign(element.horizontalAlign);
        instance.primitive->setVerticalAlign(element.verticalAlign);
        instance.primitive->setLineHeight(lineHeight);
        instance.primitive->render(windowWidth, windowHeight);
    }

    Ui ui_;
    std::unordered_map<std::string, RectInstance> rects_;
    std::unordered_map<std::string, TextInstance> texts_;
    bool needsRender_ = true;
    bool animating_ = false;
    float logicalWidth_ = 0.0f;
    float logicalHeight_ = 0.0f;
};

} // namespace core::dsl
