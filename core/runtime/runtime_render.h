#pragma once

namespace core::dsl {

inline void Runtime::renderDirect(core::render::RenderBackend& renderBackend, int windowWidth, int windowHeight, float dpiScale, const Rect* dirtyRect) {
    const RenderTransform identity;
    const bool hasScissor = dirtyRect != nullptr;
    const Rect scissor = dirtyRect ? *dirtyRect : Rect{};
    const std::vector<const Element*>& roots = orderedElements(ui_);
    for (const Element* root : roots) {
        prepareTextElement(*root, windowWidth, windowHeight, dpiScale, identity, dirtyRect, hasScissor, scissor);
    }
    for (const Element* root : roots) {
        renderElement(renderBackend, *root, windowWidth, windowHeight, dpiScale, identity, dirtyRect, hasScissor, scissor);
    }
}

inline void Runtime::prepareTextElement(
    const Element& element,
    int windowWidth,
    int windowHeight,
    float dpiScale,
    const RenderTransform& inheritedTransform,
    const Rect* dirtyRect,
    bool hasScissor,
    const Rect& scissorRect) {
    if (dirtyRect != nullptr || hasScissor) {
        const auto cached = paintBounds_.find(element.id);
        if (cached != paintBounds_.end()) {
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

    const RenderTransform renderTransform = resolveRenderTransform(element, dpiScale, inheritedTransform);
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
        runtime::TextInstance& instance = textInstance(element.id);
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

    const std::vector<const Element*>& children = orderedElements(element);
    for (const Element* child : children) {
        prepareTextElement(*child, windowWidth, windowHeight, dpiScale, renderTransform, dirtyRect, effectiveHasScissor, effectiveScissor);
    }
}

inline void Runtime::renderElement(
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
        const auto cached = paintBounds_.find(element.id);
        if (cached != paintBounds_.end()) {
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

    const RenderTransform renderTransform = resolveRenderTransform(element, dpiScale, inheritedTransform);
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
        Rect visual = toPixelRect(visualRect(rectInstance(element.id).frame.value(),
                                            rectInstance(element.id).shadow.value(),
                                            rectInstance(element.id).blur.value(),
                                            rectInstance(element.id).transform.value()), dpiScale);
        visual = applyRenderTransform(visual, renderTransform);
        if ((!dirtyRect || intersects(visual, *dirtyRect)) &&
            (!effectiveHasScissor || intersects(visual, effectiveScissor))) {
            applyOptionalScissor(renderBackend, effectiveHasScissor, effectiveScissor, windowHeight);
            renderRect(element, windowWidth, windowHeight, dpiScale, renderTransform);
        }
    } else if (element.kind == ElementKind::Polygon) {
        Rect visual = toPixelRect(visualRect(polygonInstance(element.id).frame.value(),
                                            Shadow{},
                                            0.0f,
                                            polygonInstance(element.id).transform.value()), dpiScale);
        visual = applyRenderTransform(visual, renderTransform);
        if ((!dirtyRect || intersects(visual, *dirtyRect)) &&
            (!effectiveHasScissor || intersects(visual, effectiveScissor))) {
            applyOptionalScissor(renderBackend, effectiveHasScissor, effectiveScissor, windowHeight);
            renderPolygon(element, windowWidth, windowHeight, dpiScale, renderTransform);
        }
    } else if (element.kind == ElementKind::Text) {
        runtime::TextInstance& instance = textInstance(element.id);
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
        Rect visual = toPixelRect(imageVisualRect(imageInstance(element.id).frame.value(),
                                                 imageInstance(element.id).transform.value()), dpiScale);
        visual = applyRenderTransform(visual, renderTransform);
        if ((!dirtyRect || intersects(visual, *dirtyRect)) &&
            (!effectiveHasScissor || intersects(visual, effectiveScissor))) {
            applyOptionalScissor(renderBackend, effectiveHasScissor, effectiveScissor, windowHeight);
            renderImage(element, windowWidth, windowHeight, dpiScale, renderTransform);
        }
    } else if (element.kind == ElementKind::Video) {
        Rect visual = toPixelRect(imageVisualRect(videoInstance(element.id).frame.value(),
                                                 videoInstance(element.id).transform.value()), dpiScale);
        visual = applyRenderTransform(visual, renderTransform);
        if ((!dirtyRect || intersects(visual, *dirtyRect)) &&
            (!effectiveHasScissor || intersects(visual, effectiveScissor))) {
            applyOptionalScissor(renderBackend, effectiveHasScissor, effectiveScissor, windowHeight);
            renderVideo(element, windowWidth, windowHeight, dpiScale, renderTransform);
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

inline void Runtime::renderElementChildren(
    core::render::RenderBackend& renderBackend,
    const Element& element,
    int windowWidth,
    int windowHeight,
    float dpiScale,
    const RenderTransform& renderTransform,
    const Rect* dirtyRect,
    bool hasScissor,
    const Rect& scissorRect) {
    const std::vector<const Element*>& children = orderedElements(element);
    for (const Element* child : children) {
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

inline bool Runtime::isRetainedLayerCandidate(const Element& element,
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
    if (element.subtreeHasDependentVisuals ||
        element.subtreeHasBackdropBlur ||
        element.subtreeBlocksRetainedLayer ||
        bounds.subtreeAnimating) {
        return false;
    }
    return true;
}

inline std::uint64_t Runtime::retainedLayerSignature(const Element& element,
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
    const std::vector<const Element*>& children = orderedElements(element);
    seed = mix(seed, static_cast<std::uint64_t>(children.size()));
    for (const Element* child : children) {
        const auto childBounds = paintBounds_.find(child->id);
        if (childBounds != paintBounds_.end()) {
            seed = mix(seed, retainedLayerSignature(*child, childBounds->second, dpiScale));
        } else {
            seed = retainedElementPaintSignature(*child, seed);
        }
    }
    return seed;
}

inline std::uint64_t Runtime::retainedElementPaintSignature(const Element& element, std::uint64_t seed) const {
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
    mixString(element.videoSource);
    mixString(element.dirtyKey);
    return seed;
}

inline runtime::RetainedLayerInstance& Runtime::retainedLayerInstance(const std::string& id) {
    runtime::RetainedLayerInstance& instance = retainedLayers_.try_emplace(id).first->second;
    instance.seen = true;
    return instance;
}

inline bool Runtime::renderRetainedLayer(core::render::RenderBackend& renderBackend,
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
    const auto boundsIt = paintBounds_.find(element.id);
    if (boundsIt == paintBounds_.end()) {
        return false;
    }
    const Rect subtreePixels = toPixelRect(boundsIt->second.subtree, dpiScale);
    if (!isRetainedLayerCandidate(element, boundsIt->second, subtreePixels, dirtyRect, hasScissor, scissorRect)) {
        return false;
    }

    const runtime::PaintBoundsInstance& bounds = boundsIt->second;
    runtime::RetainedLayerInstance& layer = retainedLayerInstance(element.id);
    const std::uint64_t signature = retainedLayerSignature(element, bounds, dpiScale);
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
        layer.stableFrames = std::min(layer.stableFrames + 1, 2);
        ++core::render::currentRenderFrameStats().retainedLayerMisses;
        if (layer.stableFrames < 2) {
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
        renderElement(renderBackend, element, layerWidth, layerHeight, dpiScale, layerTransform);
        renderBackend.endLayerFrame();
        layer.valid = true;
        ++core::render::currentRenderFrameStats().retainedLayerRebuilds;
        // Use the freshly rebuilt layer on the next repaint; keep this frame on the direct path.
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

inline void Runtime::renderRect(
    const Element& element,
    int windowWidth,
    int windowHeight,
    float dpiScale,
    const RenderTransform& renderTransform) {
    runtime::RectInstance& instance = rectInstance(element.id);
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

inline void Runtime::renderPolygon(
    const Element& element,
    int windowWidth,
    int windowHeight,
    float dpiScale,
    const RenderTransform& renderTransform) {
    runtime::PolygonInstance& instance = polygonInstance(element.id);
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

inline void Runtime::prepareText(
    const Element& element,
    int,
    int,
    float dpiScale,
    const RenderTransform& renderTransform) {
    runtime::TextInstance& instance = textInstance(element.id);
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

inline void Runtime::renderText(
    const Element& element,
    int windowWidth,
    int windowHeight,
    float dpiScale,
    const RenderTransform& renderTransform) {
    runtime::TextInstance& instance = textInstance(element.id);
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

inline void Runtime::renderImage(
    const Element& element,
    int windowWidth,
    int windowHeight,
    float dpiScale,
    const RenderTransform& renderTransform) {
    runtime::ImageInstance& instance = imageInstance(element.id);
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

inline void Runtime::renderVideo(
    const Element& element,
    int windowWidth,
    int windowHeight,
    float dpiScale,
    const RenderTransform& renderTransform) {
    runtime::VideoInstance& instance = videoInstance(element.id);
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

} // namespace core::dsl
