#pragma once

namespace core::dsl {

class RuntimeRenderer {
public:
    RuntimeRenderer(Ui& ui, runtime::InstanceStore& instances)
        : ui_(ui), instances_(instances) {}

    void renderDirect(core::render::RenderBackend& renderBackend,
                      int windowWidth,
                      int windowHeight,
                      float dpiScale,
                      const Rect* dirtyRect = nullptr);

private:
    void prepareTextElement(const Element& element,
                            int windowWidth,
                            int windowHeight,
                            float dpiScale,
                            const RenderTransform& inheritedTransform,
                            const Rect* dirtyRect = nullptr,
                            bool hasScissor = false,
                            const Rect& scissorRect = {});

    void renderElement(core::render::RenderBackend& renderBackend,
                       const Element& element,
                       int windowWidth,
                       int windowHeight,
                       float dpiScale,
                       const RenderTransform& inheritedTransform,
                       const Rect* dirtyRect = nullptr,
                       bool hasScissor = false,
                       const Rect& scissorRect = {});

    void renderElementChildren(core::render::RenderBackend& renderBackend,
                               const Element& element,
                               int windowWidth,
                               int windowHeight,
                               float dpiScale,
                               const RenderTransform& renderTransform,
                               const Rect* dirtyRect,
                               bool hasScissor,
                               const Rect& scissorRect);

    bool isRetainedLayerCandidate(const Element& element,
                                  const runtime::PaintBoundsInstance& bounds,
                                  const Rect& subtreePixels,
                                  const Rect* dirtyRect,
                                  bool hasScissor,
                                  const Rect& scissorRect) const;

    bool isRetainedLayerGeometryCandidate(const runtime::PaintBoundsInstance& bounds,
                                          const Rect& subtreePixels,
                                          const Rect* dirtyRect,
                                          bool hasScissor,
                                          const Rect& scissorRect) const;

    bool isRetainedSiblingCandidate(const Element& element) const;

    bool renderRetainedSiblingRun(core::render::RenderBackend& renderBackend,
                                  const Element& parent,
                                  const std::vector<const Element*>& children,
                                  std::size_t begin,
                                  std::size_t end,
                                  int windowWidth,
                                  int windowHeight,
                                  float dpiScale,
                                  const RenderTransform& renderTransform,
                                  const Rect* dirtyRect,
                                  bool hasScissor,
                                  const Rect& scissorRect);

    std::uint64_t retainedLayerSignature(const Element& element,
                                         const runtime::PaintBoundsInstance& bounds,
                                         float dpiScale) const;

    std::uint64_t retainedSiblingRunSignature(const Element& parent,
                                              const std::vector<const Element*>& children,
                                              std::size_t begin,
                                              std::size_t end,
                                              const runtime::PaintBoundsInstance& bounds,
                                              float dpiScale) const;

    std::uint64_t retainedElementPaintSignature(const Element& element,
                                                std::uint64_t seed) const;

    bool renderRetainedLayer(core::render::RenderBackend& renderBackend,
                             const Element& element,
                             int windowWidth,
                             int windowHeight,
                             float dpiScale,
                             const RenderTransform& renderTransform,
                             const Rect* dirtyRect,
                             bool hasScissor,
                             const Rect& scissorRect);

    bool renderRetainedElements(core::render::RenderBackend& renderBackend,
                                const std::string& layerId,
                                const Element* const* elements,
                                std::size_t elementCount,
                                const runtime::PaintBoundsInstance& bounds,
                                std::uint64_t signature,
                                int stableFrameThreshold,
                                int windowWidth,
                                int windowHeight,
                                float dpiScale,
                                const RenderTransform& renderTransform,
                                const Rect* dirtyRect,
                                bool hasScissor,
                                const Rect& scissorRect);

    void renderRect(const Element& element,
                    int windowWidth,
                    int windowHeight,
                    float dpiScale,
                    const RenderTransform& renderTransform);

    void renderPolygon(const Element& element,
                       int windowWidth,
                       int windowHeight,
                       float dpiScale,
                       const RenderTransform& renderTransform);

    void prepareText(const Element& element,
                     int windowWidth,
                     int windowHeight,
                     float dpiScale,
                     const RenderTransform& renderTransform);

    void renderText(const Element& element,
                    int windowWidth,
                    int windowHeight,
                    float dpiScale,
                    const RenderTransform& renderTransform);

    void renderImage(const Element& element,
                     int windowWidth,
                     int windowHeight,
                     float dpiScale,
                     const RenderTransform& renderTransform);

    void renderShaderToy(const Element& element,
                         int windowWidth,
                         int windowHeight,
                         float dpiScale,
                         const RenderTransform& renderTransform);

    Ui& ui_;
    runtime::InstanceStore& instances_;
    bool retainedLayerRenderDisabled_ = false;
};

inline void applyOptionalScissor(core::render::RenderBackend& renderBackend,
                                 bool enabled,
                                 const Rect& rect,
                                 int windowHeight) {
    renderBackend.setScissor(enabled, rect, windowHeight);
}

inline std::vector<Vec2> scaledPolygonPoints(const std::vector<Vec2>& points, float dpiScale) {
    std::vector<Vec2> result;
    result.reserve(points.size());
    for (const Vec2& point : points) {
        result.push_back({toPixels(point.x, dpiScale), toPixels(point.y, dpiScale)});
    }
    return result;
}

inline void RuntimeRenderer::renderDirect(core::render::RenderBackend& renderBackend, int windowWidth, int windowHeight, float dpiScale, const Rect* dirtyRect) {
    const RenderTransform identity;
    const bool hasScissor = dirtyRect != nullptr;
    const Rect scissor = dirtyRect ? *dirtyRect : Rect{};
    const std::vector<const Element*>& roots = ui_.orderedRoots();
    for (const Element* root : roots) {
        prepareTextElement(*root, windowWidth, windowHeight, dpiScale, identity, dirtyRect, hasScissor, scissor);
    }
    for (const Element* root : roots) {
        renderElement(renderBackend, *root, windowWidth, windowHeight, dpiScale, identity, dirtyRect, hasScissor, scissor);
    }
}

inline void RuntimeRenderer::prepareTextElement(
    const Element& element,
    int windowWidth,
    int windowHeight,
    float dpiScale,
    const RenderTransform& inheritedTransform,
    const Rect* dirtyRect,
    bool hasScissor,
    const Rect& scissorRect) {
    if (dirtyRect != nullptr || hasScissor) {
        const auto cached = instances_.paintBounds.find(element.id);
        if (cached != instances_.paintBounds.end()) {
            if (!cached->second.hasSubtree) {
                return;
            }
            const Rect subtree = toPixelRect(cached->second.subtree, dpiScale);
            if (dirtyRect != nullptr && !intersects(subtree, *dirtyRect)) {
                return;
            }
            if (hasScissor && !intersects(subtree, scissorRect)) {
                return;
            }
        }
    }

    const RenderTransform renderTransform = instances_.renderTransform(element, dpiScale, inheritedTransform);
    if (renderTransform.opacity <= 0.001f) {
        return;
    }

    Rect effectiveScissor = scissorRect;
    bool effectiveHasScissor = hasScissor;
    if (element.clip) {
        Rect clipFrame = applyRenderTransform(toPixelRect(element.frame, dpiScale), renderTransform);
        if (effectiveHasScissor) {
            if (!intersectRect(effectiveScissor, clipFrame, effectiveScissor)) {
                return;
            }
        } else {
            effectiveScissor = clipFrame;
            effectiveHasScissor = true;
        }
    }

    if (element.kind == ElementKind::Text) {
        runtime::TextInstance& instance = instances_.text(element.id);
        Rect frame = toPixelRect(transformRect({instance.frame.value().x,
                                                instance.frame.value().y,
                                                instance.frame.value().width,
                                                instance.frame.value().height},
                                               instance.frame.value(),
                                               instance.transform.value()), dpiScale);
        frame = applyRenderTransform(frame, renderTransform);
        if ((!dirtyRect || intersects(frame, *dirtyRect)) &&
            (!effectiveHasScissor || intersects(frame, effectiveScissor))) {
            prepareText(element, windowWidth, windowHeight, dpiScale, renderTransform);
        }
    }

    const std::vector<const Element*>& children = element.orderedChildren;
    for (const Element* child : children) {
        prepareTextElement(*child, windowWidth, windowHeight, dpiScale, renderTransform, dirtyRect, effectiveHasScissor, effectiveScissor);
    }
}

inline void RuntimeRenderer::renderElement(
    core::render::RenderBackend& renderBackend,
    const Element& element,
    int windowWidth,
    int windowHeight,
    float dpiScale,
    const RenderTransform& inheritedTransform,
    const Rect* dirtyRect,
    bool hasScissor,
    const Rect& scissorRect) {
    if (dirtyRect != nullptr || hasScissor) {
        const auto cached = instances_.paintBounds.find(element.id);
        if (cached != instances_.paintBounds.end()) {
            if (!cached->second.hasSubtree) {
                return;
            }
            const Rect subtree = toPixelRect(cached->second.subtree, dpiScale);
            if (dirtyRect != nullptr && !intersects(subtree, *dirtyRect)) {
                return;
            }
            if (hasScissor && !intersects(subtree, scissorRect)) {
                return;
            }
        }
    }

    const RenderTransform renderTransform = instances_.renderTransform(element, dpiScale, inheritedTransform);
    if (renderTransform.opacity <= 0.001f) {
        return;
    }
    Rect effectiveScissor = scissorRect;
    bool effectiveHasScissor = hasScissor;
    if (element.clip) {
        Rect clipFrame = applyRenderTransform(toPixelRect(element.frame, dpiScale), renderTransform);
        if (effectiveHasScissor) {
            if (!intersectRect(effectiveScissor, clipFrame, effectiveScissor)) {
                return;
            }
        } else {
            effectiveScissor = clipFrame;
            effectiveHasScissor = true;
        }
    }

    if (element.kind == ElementKind::Rect) {
        Rect visual = toPixelRect(visualRect(instances_.rect(element.id).frame.value(),
                                            instances_.rect(element.id).shadow.value(),
                                            instances_.rect(element.id).blur.value(),
                                            instances_.rect(element.id).transform.value()), dpiScale);
        visual = applyRenderTransform(visual, renderTransform);
        if ((!dirtyRect || intersects(visual, *dirtyRect)) &&
            (!effectiveHasScissor || intersects(visual, effectiveScissor))) {
            applyOptionalScissor(renderBackend, effectiveHasScissor, effectiveScissor, windowHeight);
            renderRect(element, windowWidth, windowHeight, dpiScale, renderTransform);
        }
    } else if (element.kind == ElementKind::Polygon) {
        Rect visual = toPixelRect(visualRect(instances_.polygon(element.id).frame.value(),
                                            Shadow{},
                                            0.0f,
                                            instances_.polygon(element.id).transform.value()), dpiScale);
        visual = applyRenderTransform(visual, renderTransform);
        if ((!dirtyRect || intersects(visual, *dirtyRect)) &&
            (!effectiveHasScissor || intersects(visual, effectiveScissor))) {
            applyOptionalScissor(renderBackend, effectiveHasScissor, effectiveScissor, windowHeight);
            renderPolygon(element, windowWidth, windowHeight, dpiScale, renderTransform);
        }
    } else if (element.kind == ElementKind::Text) {
        runtime::TextInstance& instance = instances_.text(element.id);
        Rect frame = toPixelRect(transformRect({instance.frame.value().x,
                                                instance.frame.value().y,
                                                instance.frame.value().width,
                                                instance.frame.value().height},
                                               instance.frame.value(),
                                               instance.transform.value()), dpiScale);
        frame = applyRenderTransform(frame, renderTransform);
        if ((!dirtyRect || intersects(frame, *dirtyRect)) &&
            (!effectiveHasScissor || intersects(frame, effectiveScissor))) {
            applyOptionalScissor(renderBackend, effectiveHasScissor, effectiveScissor, windowHeight);
            renderText(element, windowWidth, windowHeight, dpiScale, renderTransform);
        }
    } else if (element.kind == ElementKind::Image || element.kind == ElementKind::Svg) {
        Rect visual = toPixelRect(imageVisualRect(instances_.image(element.id).frame.value(),
                                                 instances_.image(element.id).transform.value()), dpiScale);
        visual = applyRenderTransform(visual, renderTransform);
        if ((!dirtyRect || intersects(visual, *dirtyRect)) &&
            (!effectiveHasScissor || intersects(visual, effectiveScissor))) {
            applyOptionalScissor(renderBackend, effectiveHasScissor, effectiveScissor, windowHeight);
            renderImage(element, windowWidth, windowHeight, dpiScale, renderTransform);
        }
    } else if (element.kind == ElementKind::Shadertoy) {
        runtime::ShaderToyInstance& instance = instances_.shaderToy(element.id);
        Rect visual = toPixelRect(imageVisualRect(instance.frame.value(), instance.transform.value()), dpiScale);
        visual = applyRenderTransform(visual, renderTransform);
        if ((!dirtyRect || intersects(visual, *dirtyRect)) &&
            (!effectiveHasScissor || intersects(visual, effectiveScissor))) {
            applyOptionalScissor(renderBackend, effectiveHasScissor, effectiveScissor, windowHeight);
            renderShaderToy(element, windowWidth, windowHeight, dpiScale, renderTransform);
        }
    }

    renderElementChildren(renderBackend,
                          element,
                          windowWidth,
                          windowHeight,
                          dpiScale,
                          renderTransform,
                          dirtyRect,
                          effectiveHasScissor,
                          effectiveScissor);
}

inline void RuntimeRenderer::renderElementChildren(
    core::render::RenderBackend& renderBackend,
    const Element& element,
    int windowWidth,
    int windowHeight,
    float dpiScale,
    const RenderTransform& renderTransform,
    const Rect* dirtyRect,
    bool hasScissor,
    const Rect& scissorRect) {
    const std::vector<const Element*>& children = element.orderedChildren;
    const auto renderChild = [&](const Element* child) {
        const bool mayUseRetainedLayer =
            !child->orderedChildren.empty() &&
            !child->subtreeBlocksRetainedLayer &&
            !child->subtreeHasDependentVisuals &&
            !child->subtreeHasBackdropBlur;
        const bool retainedLayerRendered =
            mayUseRetainedLayer &&
            renderRetainedLayer(renderBackend,
                                *child,
                                windowWidth,
                                windowHeight,
                                dpiScale,
                                renderTransform,
                                dirtyRect,
                                hasScissor,
                                scissorRect);
        if (!retainedLayerRendered) {
            renderElement(renderBackend, *child, windowWidth, windowHeight, dpiScale, renderTransform, dirtyRect, hasScissor, scissorRect);
        }
    };

    std::size_t index = 0;
    while (index < children.size()) {
        std::size_t runEnd = index;
        while (runEnd < children.size() && isRetainedSiblingCandidate(*children[runEnd])) {
            ++runEnd;
        }

        if (runEnd - index >= 2) {
            if (!renderRetainedSiblingRun(renderBackend,
                                          element,
                                          children,
                                          index,
                                          runEnd,
                                          windowWidth,
                                          windowHeight,
                                          dpiScale,
                                          renderTransform,
                                          dirtyRect,
                                          hasScissor,
                                          scissorRect)) {
                for (std::size_t childIndex = index; childIndex < runEnd; ++childIndex) {
                    renderChild(children[childIndex]);
                }
            }
            index = runEnd;
            continue;
        }

        renderChild(children[index]);
        ++index;
    }
}

struct RetainedLayerRenderScope {
    bool& disabled;
    explicit RetainedLayerRenderScope(bool& value) : disabled(value) {
        disabled = true;
    }
    ~RetainedLayerRenderScope() {
        disabled = false;
    }
};

inline bool RuntimeRenderer::isRetainedLayerGeometryCandidate(
    const runtime::PaintBoundsInstance& bounds,
    const Rect& subtreePixels,
    const Rect* dirtyRect,
    bool hasScissor,
    const Rect& scissorRect) const {
    if (!bounds.hasSubtree ||
        bounds.drawCost < 8) {
        return false;
    }
    if (subtreePixels.width < 24.0f || subtreePixels.height < 24.0f) {
        return false;
    }
    const float area = subtreePixels.width * subtreePixels.height;
    if (area < 4096.0f || area > 2048.0f * 2048.0f) {
        return false;
    }
    if (dirtyRect != nullptr && !intersects(subtreePixels, *dirtyRect)) {
        return false;
    }
    if (hasScissor && !intersects(subtreePixels, scissorRect)) {
        return false;
    }
    return !bounds.subtreeAnimating;
}

inline bool RuntimeRenderer::isRetainedLayerCandidate(
    const Element& element,
    const runtime::PaintBoundsInstance& bounds,
    const Rect& subtreePixels,
    const Rect* dirtyRect,
    bool hasScissor,
    const Rect& scissorRect) const {
    if (!isRetainedLayerGeometryCandidate(bounds,
                                          subtreePixels,
                                          dirtyRect,
                                          hasScissor,
                                          scissorRect)) {
        return false;
    }
    if (element.subtreeHasDependentVisuals ||
        element.subtreeHasBackdropBlur ||
        element.subtreeBlocksRetainedLayer) {
        return false;
    }
    return true;
}

inline bool RuntimeRenderer::isRetainedSiblingCandidate(const Element& element) const {
    if (element.subtreeHasDependentVisuals ||
        element.subtreeHasBackdropBlur ||
        element.subtreeBlocksRetainedLayer) {
        return false;
    }
    const auto bounds = instances_.paintBounds.find(element.id);
    return bounds != instances_.paintBounds.end() &&
           bounds->second.hasSubtree &&
           bounds->second.drawCost > 0 &&
           !bounds->second.subtreeAnimating;
}

inline bool RuntimeRenderer::renderRetainedSiblingRun(
    core::render::RenderBackend& renderBackend,
    const Element& parent,
    const std::vector<const Element*>& children,
    std::size_t begin,
    std::size_t end,
    int windowWidth,
    int windowHeight,
    float dpiScale,
    const RenderTransform& renderTransform,
    const Rect* dirtyRect,
    bool hasScissor,
    const Rect& scissorRect) {
    if (retainedLayerRenderDisabled_ ||
        renderTransform.active ||
        !closeEnough(renderTransform.opacity, 1.0f) ||
        begin >= end ||
        end > children.size()) {
        return false;
    }

    runtime::PaintBoundsInstance combined;
    combined.seen = true;
    for (std::size_t index = begin; index < end; ++index) {
        const Element* child = children[index];
        const auto bounds = instances_.paintBounds.find(child->id);
        if (bounds == instances_.paintBounds.end() ||
            !bounds->second.hasSubtree ||
            bounds->second.subtreeAnimating) {
            return false;
        }
        combined.subtree = combined.hasSubtree
            ? unionRect(combined.subtree, bounds->second.subtree)
            : bounds->second.subtree;
        combined.hasSubtree = true;
        combined.drawCost += bounds->second.drawCost;
    }

    const Rect subtreePixels = toPixelRect(combined.subtree, dpiScale);
    if (!isRetainedLayerGeometryCandidate(combined,
                                          subtreePixels,
                                          dirtyRect,
                                          hasScissor,
                                          scissorRect)) {
        return false;
    }

    std::string layerId = "\x1eretained-siblings:";
    layerId += parent.id;
    layerId.push_back(':');
    layerId += children[begin]->id;
    layerId.push_back(':');
    layerId += children[end - 1]->id;
    return renderRetainedElements(renderBackend,
                                  layerId,
                                  children.data() + begin,
                                  end - begin,
                                  combined,
                                  retainedSiblingRunSignature(parent,
                                                              children,
                                                              begin,
                                                              end,
                                                              combined,
                                                              dpiScale),
                                  2,
                                  windowWidth,
                                  windowHeight,
                                  dpiScale,
                                  renderTransform,
                                  dirtyRect,
                                  hasScissor,
                                  scissorRect);
}

inline std::uint64_t RuntimeRenderer::retainedLayerSignature(const Element& element,
                                                     const runtime::PaintBoundsInstance& bounds,
                                                     float dpiScale) const {
    auto mix = [](std::uint64_t seed, std::uint64_t value) {
        seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
        return seed;
    };
    auto quant = [](float value) {
        return static_cast<std::uint64_t>(std::llround(value * 64.0f));
    };
    std::uint64_t seed = 1469598103934665603ull;
    for (char c : element.id) {
        seed = mix(seed, static_cast<unsigned char>(c));
    }
    seed = mix(seed, static_cast<std::uint64_t>(element.kind));
    seed = mix(seed, static_cast<std::uint64_t>(element.zIndex));
    seed = mix(seed, static_cast<std::uint64_t>(bounds.drawCost));
    seed = mix(seed, quant(bounds.subtree.x));
    seed = mix(seed, quant(bounds.subtree.y));
    seed = mix(seed, quant(bounds.subtree.width));
    seed = mix(seed, quant(bounds.subtree.height));
    seed = mix(seed, quant(dpiScale));
    seed = retainedElementPaintSignature(element, seed);
    const std::vector<const Element*>& children = element.orderedChildren;
    seed = mix(seed, static_cast<std::uint64_t>(children.size()));
    for (const Element* child : children) {
        const auto childBounds = instances_.paintBounds.find(child->id);
        if (childBounds != instances_.paintBounds.end()) {
            seed = mix(seed, retainedLayerSignature(*child, childBounds->second, dpiScale));
        } else {
            seed = retainedElementPaintSignature(*child, seed);
        }
    }
    return seed;
}

inline std::uint64_t RuntimeRenderer::retainedSiblingRunSignature(
    const Element& parent,
    const std::vector<const Element*>& children,
    std::size_t begin,
    std::size_t end,
    const runtime::PaintBoundsInstance& bounds,
    float dpiScale) const {
    auto mix = [](std::uint64_t seed, std::uint64_t value) {
        seed ^= value + 0x9e3779b97f4a7c15ull + (seed << 6) + (seed >> 2);
        return seed;
    };
    auto quant = [](float value) {
        return static_cast<std::uint64_t>(std::llround(value * 64.0f));
    };
    std::uint64_t seed = 1469598103934665603ull;
    for (char c : parent.id) {
        seed = mix(seed, static_cast<unsigned char>(c));
    }
    seed = mix(seed, static_cast<std::uint64_t>(begin));
    seed = mix(seed, static_cast<std::uint64_t>(end - begin));
    seed = mix(seed, static_cast<std::uint64_t>(bounds.drawCost));
    seed = mix(seed, quant(bounds.subtree.x));
    seed = mix(seed, quant(bounds.subtree.y));
    seed = mix(seed, quant(bounds.subtree.width));
    seed = mix(seed, quant(bounds.subtree.height));
    seed = mix(seed, quant(dpiScale));
    for (std::size_t index = begin; index < end; ++index) {
        const Element& child = *children[index];
        const auto childBounds = instances_.paintBounds.find(child.id);
        seed = mix(seed, childBounds != instances_.paintBounds.end()
            ? retainedLayerSignature(child, childBounds->second, dpiScale)
            : retainedElementPaintSignature(child, seed));
    }
    return seed;
}

inline std::uint64_t RuntimeRenderer::retainedElementPaintSignature(const Element& element, std::uint64_t seed) const {
    auto mix = [](std::uint64_t current, std::uint64_t value) {
        current ^= value + 0x9e3779b97f4a7c15ull + (current << 6) + (current >> 2);
        return current;
    };
    auto quant = [](float value) {
        return static_cast<std::uint64_t>(std::llround(value * 4096.0f));
    };
    auto mixString = [&](const std::string& value) {
        seed = mix(seed, static_cast<std::uint64_t>(value.size()));
        for (char c : value) {
            seed = mix(seed, static_cast<unsigned char>(c));
        }
    };
    auto mixColorValue = [&](const Color& value) {
        seed = mix(seed, quant(value.r));
        seed = mix(seed, quant(value.g));
        seed = mix(seed, quant(value.b));
        seed = mix(seed, quant(value.a));
    };
    auto mixRectValue = [&](const LayoutRect& value) {
        seed = mix(seed, quant(value.x));
        seed = mix(seed, quant(value.y));
        seed = mix(seed, quant(value.width));
        seed = mix(seed, quant(value.height));
    };
    auto mixTransformValue = [&](const Transform& value) {
        seed = mix(seed, quant(value.translate.x));
        seed = mix(seed, quant(value.translate.y));
        seed = mix(seed, quant(value.translateZ));
        seed = mix(seed, quant(value.scale.x));
        seed = mix(seed, quant(value.scale.y));
        seed = mix(seed, quant(value.rotate));
        seed = mix(seed, quant(value.rotateX));
        seed = mix(seed, quant(value.rotateY));
        seed = mix(seed, quant(value.origin.x));
        seed = mix(seed, quant(value.origin.y));
        seed = mix(seed, quant(value.perspective));
    };

    mixRectValue(element.frame);
    mixColorValue(element.color);
    mixColorValue(element.textColor);
    mixColorValue(element.hoverColor);
    mixColorValue(element.pressedColor);
    mixColorValue(element.border.color);
    mixColorValue(element.shadow.color);
    mixColorValue(element.gradient.start);
    mixColorValue(element.gradient.end);
    seed = mix(seed, quant(element.radius));
    seed = mix(seed, quant(element.blur));
    seed = mix(seed, quant(element.opacity));
    seed = mix(seed, quant(element.border.width));
    seed = mix(seed, element.shadow.enabled ? 1u : 0u);
    seed = mix(seed, element.shadow.inset ? 1u : 0u);
    seed = mix(seed, quant(element.shadow.offset.x));
    seed = mix(seed, quant(element.shadow.offset.y));
    seed = mix(seed, quant(element.shadow.blur));
    seed = mix(seed, quant(element.shadow.spread));
    seed = mix(seed, element.gradient.enabled ? 1u : 0u);
    seed = mix(seed, static_cast<std::uint64_t>(element.gradient.direction));
    seed = mix(seed, quant(element.fontSize));
    seed = mix(seed, static_cast<std::uint64_t>(element.fontWeight));
    seed = mix(seed, quant(element.maxWidth));
    seed = mix(seed, quant(element.lineHeight));
    seed = mix(seed, element.wrap ? 1u : 0u);
    seed = mix(seed, static_cast<std::uint64_t>(element.horizontalAlign));
    seed = mix(seed, static_cast<std::uint64_t>(element.verticalAlign));
    seed = mix(seed, static_cast<std::uint64_t>(element.imageFit));
    seed = mix(seed, element.imageFlipVertically ? 1u : 0u);
    mixTransformValue(element.transform);
    mixString(element.text);
    mixString(element.fontFamily);
    mixString(element.imageSource);
    mixString(element.svgSource);
    mixString(element.dirtyKey);
    return seed;
}

inline bool RuntimeRenderer::renderRetainedLayer(core::render::RenderBackend& renderBackend,
                                         const Element& element,
                                         int windowWidth,
                                         int windowHeight,
                                         float dpiScale,
                                         const RenderTransform& renderTransform,
                                         const Rect* dirtyRect,
                                         bool hasScissor,
                                         const Rect& scissorRect) {
    if (renderTransform.active || !closeEnough(renderTransform.opacity, 1.0f)) {
        return false;
    }
    if (retainedLayerRenderDisabled_) {
        return false;
    }
    const auto boundsIt = instances_.paintBounds.find(element.id);
    if (boundsIt == instances_.paintBounds.end()) {
        return false;
    }
    const Rect subtreePixels = toPixelRect(boundsIt->second.subtree, dpiScale);
    if (!isRetainedLayerCandidate(element, boundsIt->second, subtreePixels, dirtyRect, hasScissor, scissorRect)) {
        return false;
    }

    const Element* elements[] = {&element};
    return renderRetainedElements(renderBackend,
                                  element.id,
                                  elements,
                                  1,
                                  boundsIt->second,
                                  retainedLayerSignature(element, boundsIt->second, dpiScale),
                                  2,
                                  windowWidth,
                                  windowHeight,
                                  dpiScale,
                                  renderTransform,
                                  dirtyRect,
                                  hasScissor,
                                  scissorRect);
}

inline bool RuntimeRenderer::renderRetainedElements(
    core::render::RenderBackend& renderBackend,
    const std::string& layerId,
    const Element* const* elements,
    std::size_t elementCount,
    const runtime::PaintBoundsInstance& bounds,
    std::uint64_t signature,
    int stableFrameThreshold,
    int windowWidth,
    int windowHeight,
    float dpiScale,
    const RenderTransform& renderTransform,
    const Rect* dirtyRect,
    bool hasScissor,
    const Rect& scissorRect) {
    if (retainedLayerRenderDisabled_ ||
        renderTransform.active ||
        !closeEnough(renderTransform.opacity, 1.0f) ||
        elements == nullptr ||
        elementCount == 0) {
        return false;
    }

    stableFrameThreshold = std::max(1, stableFrameThreshold);
    runtime::RetainedLayerInstance& layer = instances_.retainedLayer(layerId);
    const Rect subtreePixels = toPixelRect(bounds.subtree, dpiScale);
    const Rect layerBounds = core::render::clampRenderRect(subtreePixels, windowWidth, windowHeight);
    const int layerWidth = static_cast<int>(std::ceil(layerBounds.width));
    const int layerHeight = static_cast<int>(std::ceil(layerBounds.height));
    if (layerWidth <= 0 || layerHeight <= 0) {
        return false;
    }

    const bool sameLayer = layer.valid &&
                           layer.handle != nullptr &&
                           layer.signature == signature &&
                           closeEnough(layer.bounds, layerBounds) &&
                           layer.width == layerWidth &&
                           layer.height == layerHeight;
    if (!sameLayer) {
        layer.valid = false;
        layer.signature = signature;
        layer.bounds = layerBounds;
        layer.width = layerWidth;
        layer.height = layerHeight;
        layer.stableFrames = std::min(layer.stableFrames + 1, stableFrameThreshold);
        ++core::render::currentRenderFrameStats().retainedLayerMisses;
        if (layer.stableFrames < stableFrameThreshold) {
            return false;
        }
        if (layer.handle == nullptr) {
            layer.handle = renderBackend.createLayer(layerWidth, layerHeight);
        } else if (!renderBackend.resizeLayer(layer.handle, layerWidth, layerHeight)) {
            renderBackend.destroyLayer(layer.handle);
            layer.handle = renderBackend.createLayer(layerWidth, layerHeight);
        }
        if (layer.handle == nullptr ||
            !renderBackend.beginLayerFrame(layer.handle, layerWidth, layerHeight)) {
            return false;
        }

        const RenderTransform layerTransform{
            true,
            TransformMatrix{1.0f, 0.0f, -layerBounds.x,
                            0.0f, 1.0f, -layerBounds.y,
                            0.0f, 0.0f, 1.0f},
            1.0f
        };
        ++core::render::currentRenderFrameStats().clearCalls;
        renderBackend.setScissor(false, {}, layerHeight);
        renderBackend.clear({0.0f, 0.0f, 0.0f, 0.0f});
        RetainedLayerRenderScope scope(retainedLayerRenderDisabled_);
        for (std::size_t index = 0; index < elementCount; ++index) {
            renderElement(renderBackend,
                          *elements[index],
                          layerWidth,
                          layerHeight,
                          dpiScale,
                          layerTransform);
        }
        renderBackend.endLayerFrame();
        layer.valid = true;
        ++core::render::currentRenderFrameStats().retainedLayerRebuilds;
        return false;
    } else {
        ++core::render::currentRenderFrameStats().retainedLayerHits;
    }

    core::render::RenderBackend::TextureHandle texture = renderBackend.layerTexture(layer.handle);
    if (texture == nullptr) {
        return false;
    }
    const float left = layerBounds.x;
    const float top = layerBounds.y;
    const float right = layerBounds.x + layerBounds.width;
    const float bottom = layerBounds.y + layerBounds.height;
    const float vertices[42] = {
        left, top, 1.0f, left, top, 0.0f, 1.0f,
        right, top, 1.0f, right, top, 1.0f, 1.0f,
        right, bottom, 1.0f, right, bottom, 1.0f, 0.0f,
        left, top, 1.0f, left, top, 0.0f, 1.0f,
        right, bottom, 1.0f, right, bottom, 1.0f, 0.0f,
        left, bottom, 1.0f, left, bottom, 0.0f, 0.0f
    };
    applyOptionalScissor(renderBackend, hasScissor, scissorRect, windowHeight);
    renderBackend.drawLayerTexture(texture, vertices, 42, layerBounds, windowWidth, windowHeight);
    ++core::render::currentRenderFrameStats().retainedLayerDraws;
    return true;
}

inline void RuntimeRenderer::renderRect(
    const Element& element,
    int windowWidth,
    int windowHeight,
    float dpiScale,
    const RenderTransform& renderTransform) {
    runtime::RectInstance& instance = instances_.rect(element.id);
    if (!instance.initialized) {
        instance.initialized = instance.primitive->initialize();
        if (!instance.initialized) {
            return;
        }
    }

    const Rect frame = toPixelRect(instance.frame.value(), dpiScale);
    const Color currentColor = instance.color.value();
    Transform transform = scaleTransform(instance.transform.value(), dpiScale);

    instance.primitive->setBounds(frame.x, frame.y, frame.width, frame.height);
    instance.primitive->setColor(currentColor);
    instance.primitive->setGradient(element.gradient);
    instance.primitive->setBorder(scaleBorder(instance.border.value(), dpiScale));
    instance.primitive->setShadow(scaleShadow(instance.shadow.value(), dpiScale));
    instance.primitive->setCornerRadius(toPixels(instance.radius.value(), dpiScale));
    instance.primitive->setBlur(toPixels(instance.blur.value(), dpiScale));
    instance.primitive->setOpacity(instance.opacity.value() * renderTransform.opacity);
    instance.primitive->setTransformMatrix(combinedPrimitiveMatrix(renderTransform, frame, transform));
    ++core::render::currentRenderFrameStats().rectDraws;
    instance.primitive->render(windowWidth, windowHeight);
}

inline void RuntimeRenderer::renderPolygon(
    const Element& element,
    int windowWidth,
    int windowHeight,
    float dpiScale,
    const RenderTransform& renderTransform) {
    runtime::PolygonInstance& instance = instances_.polygon(element.id);
    if (!instance.initialized) {
        instance.initialized = instance.primitive->initialize();
        if (!instance.initialized) {
            return;
        }
    }

    const Rect frame = toPixelRect(instance.frame.value(), dpiScale);
    Transform transform = scaleTransform(instance.transform.value(), dpiScale);

    instance.primitive->setBounds(frame.x, frame.y, frame.width, frame.height);
    instance.primitive->setPoints(scaledPolygonPoints(instance.points, dpiScale));
    instance.primitive->setRadius(toPixels(instance.radius.value(), dpiScale));
    instance.primitive->setColor(instance.color.value());
    instance.primitive->setOpacity(instance.opacity.value() * renderTransform.opacity);
    instance.primitive->setTransformMatrix(combinedPrimitiveMatrix(renderTransform, frame, transform));
    ++core::render::currentRenderFrameStats().polygonDraws;
    instance.primitive->render(windowWidth, windowHeight);
}

inline void RuntimeRenderer::prepareText(
    const Element& element,
    int,
    int,
    float dpiScale,
    const RenderTransform& renderTransform) {
    runtime::TextInstance& instance = instances_.text(element.id);
    if (!instance.initialized) {
        instance.initialized = instance.primitive->initialize();
        if (!instance.initialized) {
            return;
        }
    }

    const Rect frame = toPixelRect(instance.frame.value(), dpiScale);
    const float maxWidth = element.maxWidth > 0.0f ? toPixels(element.maxWidth, dpiScale) : frame.width;
    const float lineHeight = element.lineHeight > 0.0f ? toPixels(element.lineHeight, dpiScale) : 0.0f;
    Color textColor = instance.color.value();
    Transform transform = scaleTransform(instance.transform.value(), dpiScale);
    textColor.a *= instance.opacity.value();

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
    textColor.a *= renderTransform.opacity;

    instance.primitive->setPosition(x, y);
    instance.primitive->setTransformMatrix(combinedPrimitiveMatrix(renderTransform, frame, transform));
    instance.primitive->setColor(textColor);
    instance.primitive->setText(instance.text);
    instance.primitive->setFontFamily(instance.fontFamily);
    instance.primitive->setFontSize(toPixels(instance.fontSize, dpiScale));
    instance.primitive->setFontWeight(instance.fontWeight);
    instance.primitive->setMaxWidth(maxWidth);
    instance.primitive->setWrap(instance.wrap);
    instance.primitive->setHorizontalAlign(instance.horizontalAlign);
    instance.primitive->setVerticalAlign(instance.verticalAlign);
    instance.primitive->setLineHeight(lineHeight);
    ++core::render::currentRenderFrameStats().textPrepares;
    instance.primitive->prepare();
}

inline void RuntimeRenderer::renderText(
    const Element& element,
    int windowWidth,
    int windowHeight,
    float dpiScale,
    const RenderTransform& renderTransform) {
    runtime::TextInstance& instance = instances_.text(element.id);
    if (!instance.initialized) {
        instance.initialized = instance.primitive->initialize();
        if (!instance.initialized) {
            return;
        }
    }

    const Rect frame = toPixelRect(instance.frame.value(), dpiScale);
    const float maxWidth = element.maxWidth > 0.0f ? toPixels(element.maxWidth, dpiScale) : frame.width;
    const float lineHeight = element.lineHeight > 0.0f ? toPixels(element.lineHeight, dpiScale) : 0.0f;
    Color textColor = instance.color.value();
    Transform transform = scaleTransform(instance.transform.value(), dpiScale);
    textColor.a *= instance.opacity.value();

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
    textColor.a *= renderTransform.opacity;

    instance.primitive->setPosition(x, y);
    instance.primitive->setTransformMatrix(combinedPrimitiveMatrix(renderTransform, frame, transform));
    instance.primitive->setColor(textColor);
    instance.primitive->setText(instance.text);
    instance.primitive->setFontFamily(instance.fontFamily);
    instance.primitive->setFontSize(toPixels(instance.fontSize, dpiScale));
    instance.primitive->setFontWeight(instance.fontWeight);
    instance.primitive->setMaxWidth(maxWidth);
    instance.primitive->setWrap(instance.wrap);
    instance.primitive->setHorizontalAlign(instance.horizontalAlign);
    instance.primitive->setVerticalAlign(instance.verticalAlign);
    instance.primitive->setLineHeight(lineHeight);
    ++core::render::currentRenderFrameStats().textDraws;
    instance.primitive->render(windowWidth, windowHeight);
}

inline void RuntimeRenderer::renderImage(
    const Element& element,
    int windowWidth,
    int windowHeight,
    float dpiScale,
    const RenderTransform& renderTransform) {
    runtime::ImageInstance& instance = instances_.image(element.id);
    if (!instance.initialized) {
        instance.initialized = instance.primitive->initialize();
        if (!instance.initialized) {
            return;
        }
    }

    const Rect frame = toPixelRect(instance.frame.value(), dpiScale);
    Transform transform = scaleTransform(instance.transform.value(), dpiScale);

    instance.primitive->setBounds(frame.x, frame.y, frame.width, frame.height);
    instance.primitive->setTint(instance.tint.value());
    instance.primitive->setCornerRadius(toPixels(instance.radius.value(), dpiScale));
    instance.primitive->setBlur(toPixels(instance.blur.value(), dpiScale));
    instance.primitive->setOpacity(instance.opacity.value() * renderTransform.opacity);
    instance.primitive->setTransformMatrix(combinedPrimitiveMatrix(renderTransform, frame, transform));
    instance.primitive->setFit(instance.fit);
    instance.primitive->setCoverViewport(instance.hasCoverViewport,
                                         {toPixels(instance.coverViewportSize.x, dpiScale),
                                          toPixels(instance.coverViewportSize.y, dpiScale)},
                                         {toPixels(instance.coverViewportOffset.x, dpiScale),
                                          toPixels(instance.coverViewportOffset.y, dpiScale)});
    ++core::render::currentRenderFrameStats().imageDraws;
    instance.primitive->render(windowWidth, windowHeight);
}

inline void RuntimeRenderer::renderShaderToy(
    const Element& element,
    int windowWidth,
    int windowHeight,
    float dpiScale,
    const RenderTransform& renderTransform) {
    runtime::ShaderToyInstance& instance = instances_.shaderToy(element.id);
    if (!instance.initialized) {
        instance.initialized = instance.primitive->initialize();
        if (!instance.initialized) {
            return;
        }
    }

    const Rect frame = toPixelRect(instance.frame.value(), dpiScale);
    const Transform transform = scaleTransform(instance.transform.value(), dpiScale);
    instance.primitive->setElementId(element.id);
    instance.primitive->setBounds(frame.x, frame.y, frame.width, frame.height);
    instance.primitive->setCornerRadius(toPixels(instance.radius.value(), dpiScale));
    instance.primitive->setOpacity(instance.opacity.value() * renderTransform.opacity);
    instance.primitive->setTransformMatrix(combinedPrimitiveMatrix(renderTransform, frame, transform));
    instance.primitive->setResolutionScale(element.shaderToyResolutionScale);
    instance.primitive->setTimeScale(element.shaderToyTimeScale);
    instance.primitive->setPaused(element.shaderToyPaused);
    ++core::render::currentRenderFrameStats().shadertoyDraws;
    instance.primitive->render(windowWidth, windowHeight);

    const core::render::ShaderToyError& error = instance.primitive->error();
    if (error && element.onShaderToyError) {
        const std::uint64_t errorHash = core::render::shaderToyErrorHash(error);
        if (errorHash != instance.reportedErrorHash) {
            instance.reportedErrorHash = errorHash;
            element.onShaderToyError(error);
        }
    } else if (!error) {
        instance.reportedErrorHash = 0;
    }
}

} // namespace core::dsl
