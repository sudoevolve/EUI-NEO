#pragma once

#include "../EUINEO.h"
#include "../ui/UIBuilder.h"
#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace EUINEO {

class CheckboxNode : public UINode {
public:
    class Builder : public UIBuilderBase<CheckboxNode, Builder> {
    public:
        Builder(UIContext& context, CheckboxNode& node) : UIBuilderBase<CheckboxNode, Builder>(context, node) {}

        Builder& checked(bool value) {
            this->node_.trackComposeValue("checked", value);
            this->node_.checked_ = value;
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

        Builder& onChange(std::function<void(bool)> handler) {
            this->node_.onChange_ = std::move(handler);
            return *this;
        }
    };

    explicit CheckboxNode(const std::string& key) : UINode(key) {
        resetDefaults();
    }

    static constexpr const char* StaticTypeName() {
        return "CheckboxNode";
    }

    const char* typeName() const override {
        return StaticTypeName();
    }

    bool wantsContinuousUpdate() const override {
        const float targetChecked = checked_ ? 1.0f : 0.0f;
        const float targetHover = hovered() ? 1.0f : 0.0f;
        return std::abs(checkAnim_ - targetChecked) > 0.001f ||
               std::abs(hoverAnim_ - targetHover) > 0.001f;
    }

    RectFrame paintBounds() const override {
        const RectFrame frame = PrimitiveFrame(primitive_);
        RectFrame bounds = frame;
        if (!text_.empty()) {
            const RectFrame textBounds = Renderer::MeasureTextBounds(text_, fontSize_ / 24.0f);
            bounds.width += 10.0f + std::max(0.0f, textBounds.width);
            bounds.height = std::max(bounds.height, std::max(0.0f, textBounds.height));
        }
        return expandPaintBounds(bounds, 3.0f, 3.0f, 3.0f, 3.0f);
    }

    void update() override {
        const bool isHovered = hovered();
        if (primitive_.enabled && State.mouseClicked && isHovered) {
            checked_ = !checked_;
            if (onChange_) {
                onChange_(checked_);
            }
            State.mouseClicked = false;
            requestVisualRepaint(0.16f);
        }

        if (animateTowards(checkAnim_, checked_ ? 1.0f : 0.0f, State.deltaTime * 18.0f)) {
            requestVisualRepaint(0.16f);
        }
        if (animateTowards(hoverAnim_, isHovered ? 1.0f : 0.0f, State.deltaTime * 12.0f)) {
            requestVisualRepaint(0.16f);
        }
    }

    void draw() override {
        PrimitiveClipScope clip(primitive_);
        const RectFrame frame = PrimitiveFrame(primitive_);
        const float boxSize = std::min(frame.width, frame.height);
        const float boxY = frame.y + (frame.height - boxSize) * 0.5f;
        const float rounding = std::max(4.0f, boxSize * 0.22f);

        const Color idle = CurrentTheme->surface;
        const Color hoveredFill = CurrentTheme->surfaceHover;
        const Color active = CurrentTheme->primary;
        Color fill = Lerp(idle, hoveredFill, hoverAnim_);
        fill = Lerp(fill, active, checkAnim_ * 0.88f);
        Renderer::DrawRect(frame.x, boxY, boxSize, boxSize, ApplyOpacity(fill, primitive_.opacity), rounding);

        const Color borderColor = ApplyOpacity(Lerp(CurrentTheme->border, CurrentTheme->primary, checkAnim_), primitive_.opacity);
        Renderer::DrawRect(frame.x, boxY, boxSize, 1.5f, borderColor, rounding);

        if (checkAnim_ > 0.01f) {
            const auto lerpPoint = [](const Point2& a, const Point2& b, float t) -> Point2 {
                return Point2{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
            };
            const auto drawStrokeSegment = [](const Point2& a, const Point2& b, float thickness, const Color& color) {
                const float dx = b.x - a.x;
                const float dy = b.y - a.y;
                const float length = std::sqrt(dx * dx + dy * dy);
                if (length < 0.001f) {
                    return;
                }
                const float nx = -dy / length;
                const float ny = dx / length;
                const float half = thickness * 0.5f;
                std::vector<Point2> quad;
                quad.reserve(4);
                quad.push_back(Point2{a.x + nx * half, a.y + ny * half});
                quad.push_back(Point2{a.x - nx * half, a.y - ny * half});
                quad.push_back(Point2{b.x - nx * half, b.y - ny * half});
                quad.push_back(Point2{b.x + nx * half, b.y + ny * half});
                Renderer::DrawPolygon(quad, color);
            };
            const auto drawRoundCap = [](const Point2& centerPoint, float thickness, const Color& color) {
                const float radius = std::max(0.5f, thickness * 0.5f);
                std::vector<Point2> points;
                points.reserve(20);
                constexpr float kTau = 6.2831853f;
                for (int i = 0; i < 20; ++i) {
                    const float t = (static_cast<float>(i) / 20.0f) * kTau;
                    points.push_back(Point2{
                        centerPoint.x + std::cos(t) * radius,
                        centerPoint.y + std::sin(t) * radius
                    });
                }
                Renderer::DrawPolygon(points, color);
            };
            const auto moveTowards = [](const Point2& from, const Point2& to, float distance) -> Point2 {
                const float dx = to.x - from.x;
                const float dy = to.y - from.y;
                const float length = std::sqrt(dx * dx + dy * dy);
                if (length < 0.001f) {
                    return from;
                }
                const float k = distance / length;
                return Point2{from.x + dx * k, from.y + dy * k};
            };

            const Point2 p0{frame.x + boxSize * 0.24f, boxY + boxSize * 0.54f};
            const Point2 p1{frame.x + boxSize * 0.44f, boxY + boxSize * 0.74f};
            const Point2 p2{frame.x + boxSize * 0.78f, boxY + boxSize * 0.30f};
            const Point2 center{frame.x + boxSize * 0.5f, boxY + boxSize * 0.5f};
            const float stroke = std::max(2.0f, boxSize * 0.15f);

            if (checked_) {
                const float drawProgress = std::clamp((checkAnim_ - 0.03f) / 0.97f, 0.0f, 1.0f);
                const Color markColor = ApplyOpacity(Color(1.0f, 1.0f, 1.0f, 0.65f + drawProgress * 0.35f), primitive_.opacity);
                const float split = 0.42f;
                if (drawProgress < split) {
                    const float t = drawProgress / split;
                    const Point2 end = lerpPoint(p0, p1, t);
                    drawStrokeSegment(p0, end, stroke, markColor);
                    drawRoundCap(p0, stroke, markColor);
                    drawRoundCap(end, stroke, markColor);
                } else {
                    drawStrokeSegment(p0, p1, stroke, markColor);
                    drawRoundCap(p0, stroke, markColor);
                    const float t = (drawProgress - split) / (1.0f - split);
                    const Point2 end = lerpPoint(p1, p2, t);
                    const Point2 overlapStart = moveTowards(p1, p0, stroke * 0.28f);
                    drawStrokeSegment(overlapStart, end, stroke, markColor);
                    drawRoundCap(p1, stroke * 0.72f, markColor);
                    drawRoundCap(end, stroke, markColor);
                }
            } else {
                const float hideProgress = std::clamp(checkAnim_, 0.0f, 1.0f);
                const float scale = 0.25f + hideProgress * 0.75f;
                const Color markColor = ApplyOpacity(Color(1.0f, 1.0f, 1.0f, hideProgress * hideProgress), primitive_.opacity);
                const auto scalePoint = [&](const Point2& p) -> Point2 {
                    return Point2{
                        center.x + (p.x - center.x) * scale,
                        center.y + (p.y - center.y) * scale
                    };
                };
                const Point2 s0 = scalePoint(p0);
                const Point2 s1 = scalePoint(p1);
                const Point2 s2 = scalePoint(p2);
                const float shrinkStroke = stroke * (0.6f + hideProgress * 0.4f);
                drawStrokeSegment(s0, s1, shrinkStroke, markColor);
                const Point2 overlapStart = moveTowards(s1, s0, shrinkStroke * 0.28f);
                drawStrokeSegment(overlapStart, s2, shrinkStroke, markColor);
                drawRoundCap(s1, shrinkStroke * 0.72f, markColor);
                drawRoundCap(s0, shrinkStroke, markColor);
                drawRoundCap(s2, shrinkStroke, markColor);
            }
        }

        if (!text_.empty()) {
            const float textScale = fontSize_ / 24.0f;
            const float textX = frame.x + boxSize + 10.0f;
            const float textY = frame.y + frame.height * 0.5f + fontSize_ * 0.25f;
            Renderer::DrawTextStr(text_, textX, textY, ApplyOpacity(CurrentTheme->text, primitive_.opacity), textScale);
        }
    }

protected:
    void resetDefaults() override {
        primitive_ = UIPrimitive{};
        primitive_.width = 22.0f;
        primitive_.height = 22.0f;
        checked_ = false;
        text_.clear();
        fontSize_ = 18.0f;
        onChange_ = {};
    }

private:
    bool checked_ = false;
    std::string text_;
    float fontSize_ = 18.0f;
    std::function<void(bool)> onChange_;
    float checkAnim_ = 0.0f;
    float hoverAnim_ = 0.0f;
};

} // namespace EUINEO
