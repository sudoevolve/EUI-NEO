#pragma once

#include "eui/app.h"
#include "core/dsl_runtime.h"
#include "core/render/render_backend.h"

#include <utility>

namespace app {

class DslWindowRuntime {
public:
    bool initialize(core::window::Handle window, DslWindowRequest request) {
        request_ = std::move(request);
        paintRequested_ = true;
        return runtime_.initialize(window);
    }

    void shutdown(bool releaseCachedImageTextures = false) {
        runtime_.shutdown(releaseCachedImageTextures);
        composed_ = false;
        paintRequested_ = false;
    }

    const DslWindowRequest& request() const {
        return request_;
    }

    bool isAnimating() const {
        return runtime_.isAnimating();
    }

    bool paintRequested() const {
        return paintRequested_;
    }

    void requestPaint() {
        paintRequested_ = true;
    }

    void requestFullPaint() {
        runtime_.requestFullPaint();
        paintRequested_ = true;
    }

    bool update(core::window::Handle window,
                float deltaSeconds,
                float logicalWidth,
                float logicalHeight,
                float pointerScale,
                float dpiScale,
                bool updateRequested,
                bool inputEnabled = true) {
        bool changed = false;
        const auto composeFrame = [&] {
            runtime_.compose(request_.pageId, logicalWidth, logicalHeight,
                [&](core::dsl::Ui& ui, const core::dsl::Screen& screen) {
                    request_.compose(ui, screen);
                });
            composed_ = true;
            logicalWidth_ = logicalWidth;
            logicalHeight_ = logicalHeight;
        };

        const bool needsInitialOrResizeCompose = !composed_ || logicalWidth_ != logicalWidth || logicalHeight_ != logicalHeight;
        if (needsInitialOrResizeCompose || updateRequested) {
            composeFrame();
            paintRequested_ = true;
            changed = true;
        }

        if (runtime_.update(window, deltaSeconds, pointerScale, dpiScale, inputEnabled)) {
            paintRequested_ = true;
            changed = true;
        }

        if (runtime_.composeRequested()) {
            // A compose can change retained content without changing the element structure.
            // Rebuild the complete cache so state-driven text is visible immediately.
            runtime_.requestFullPaint();
            composeFrame();
            if (runtime_.update(window, 0.0f, pointerScale, dpiScale, inputEnabled)) {
                changed = true;
            }
            paintRequested_ = true;
            changed = true;
        }

        return changed;
    }

    void render(core::render::RenderBackend& renderBackend, int framebufferWidth, int framebufferHeight, float dpiScale) {
        core::render::ScopedRenderBackend scopedRenderBackend(renderBackend);
        runtime_.render(framebufferWidth, framebufferHeight, dpiScale, request_.clearColor);
        paintRequested_ = runtime_.paintRequested();
    }

private:
    core::dsl::Runtime runtime_;
    DslWindowRequest request_;
    bool composed_ = false;
    bool paintRequested_ = true;
    float logicalWidth_ = 0.0f;
    float logicalHeight_ = 0.0f;
};

} // namespace app
