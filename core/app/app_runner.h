#pragma once

#include "eui/app.h"
#include "core/platform/async.h"
#include "core/platform/performance_stats.h"
#include "core/platform/platform.h"
#include "core/render/render_backend.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace app {

struct AppRunner {
    bool paintRequested = true;
    bool trayAvailable = false;
    bool hiddenToTray = false;
    int renderedFrames = 0;
    double lastTitleUpdate = 0.0;
    double nextFrameTime = 0.0;
    double frameInterval = 1.0 / 60.0;
    double lastFrameTime = 0.0;
    double lastFrameRateLimit = 0.0;
    double lastRefreshRateUpdate = 0.0;
    double accumulatedRenderMs = 0.0;
    double accumulatedDirtyRectCount = 0.0;
    double accumulatedDirtyAreaPercent = 0.0;
    double accumulatedBlitAreaPercent = 0.0;
    double accumulatedRectDraws = 0.0;
    double accumulatedPolygonDraws = 0.0;
    double accumulatedTextPrepares = 0.0;
    double accumulatedTextDraws = 0.0;
    double accumulatedTextBatchFlushes = 0.0;
    double accumulatedTextBatchVertices = 0.0;
    double accumulatedBackdropCaptures = 0.0;
    double accumulatedBackdropCaptureReuses = 0.0;
    double accumulatedImageDraws = 0.0;
    double accumulatedRetainedLayerHits = 0.0;
    double accumulatedRetainedLayerMisses = 0.0;
    double accumulatedRetainedLayerDraws = 0.0;
    double accumulatedRetainedLayerRebuilds = 0.0;
    double accumulatedClearCalls = 0.0;
    double accumulatedRenderDirectPasses = 0.0;
    double accumulatedCacheBlits = 0.0;
    double accumulatedBackendRenderPasses = 0.0;
    double accumulatedBackendRenderPassAreaPercent = 0.0;
    double accumulatedBackendCopyRegions = 0.0;
    double accumulatedBackendBarriers = 0.0;
    double accumulatedBackendSubmits = 0.0;
    double accumulatedBackendPresents = 0.0;
    double accumulatedBackendPresentAreaPercent = 0.0;
    double accumulatedBackendIncrementalPresents = 0.0;
    double accumulatedBackendIncrementalPresentSupported = 0.0;
    double accumulatedBackendResolveDraws = 0.0;
    int measuredRenderFrames = 0;
    int measuredRenderStatsFrames = 0;
    int fullPaintFrames = 0;
    int renderCacheFrames = 0;
    int renderCacheRecreatedFrames = 0;
    bool renderedSinceLastClock = false;
    core::platform::ProcessUsageSampler usageSampler;

    void resetTiming(double now) {
        lastTitleUpdate = now;
        nextFrameTime = now;
        lastFrameTime = now;
        usageSampler.reset();
    }

    float consumeFrameDelta(double now) {
        const float deltaSeconds = static_cast<float>(now - lastFrameTime);
        lastFrameTime = now;
        return deltaSeconds;
    }

    bool initializeTray() {
        if (!trayEnabled()) {
            trayAvailable = false;
            return false;
        }
        trayAvailable = core::platform::initializeTray({
            trayTitle(),
            trayIconPath()
        });
        return trayAvailable;
    }

    void pollTray(bool wait = false) {
        core::platform::pollTray(wait);
    }

    bool consumeTrayExitRequested() {
        return core::platform::consumeTrayExitRequested();
    }

    bool consumeTrayShowRequested() {
        return core::platform::consumeTrayShowRequested();
    }

    bool consumeUpdateRequest() {
        const bool asyncReady = core::async::dispatchReady();
        const bool updateRequested = core::platform::consumeUiUpdate();
        return updateRequested || asyncReady;
    }

    bool consumeFrameRequest() {
        return core::platform::consumeFrameRequest();
    }

    bool anyAnimating(bool childAnimating) const {
        return isAnimating() || childAnimating;
    }

    void updateFrameInterval(double refreshRate, double now, bool force = false) {
        const double limit = frameRateLimit();
        if (!force && limit == lastFrameRateLimit && now - lastRefreshRateUpdate < 0.5) {
            return;
        }

        refreshRate = std::clamp(refreshRate, 30.0, 500.0);
        if (limit > 0.0) {
            refreshRate = std::min(refreshRate, limit);
        }
        frameInterval = 1.0 / std::max(1.0, refreshRate);
        lastFrameRateLimit = limit;
        lastRefreshRateUpdate = now;
    }

    void markRendered() {
        paintRequested = false;
        ++renderedFrames;
        renderedSinceLastClock = true;
    }

    void recordRenderDuration(double milliseconds) {
        if (milliseconds < 0.0 || milliseconds > 10000.0) {
            return;
        }
        accumulatedRenderMs += milliseconds;
        ++measuredRenderFrames;
    }

    void recordRenderStats(const core::render::RenderFrameStats& stats) {
        if (!stats.rendered) {
            return;
        }

        const double framePixels = static_cast<double>(std::max(1, stats.framebufferWidth)) *
                                   static_cast<double>(std::max(1, stats.framebufferHeight));
        accumulatedDirtyRectCount += static_cast<double>(stats.dirtyRectCount);
        accumulatedDirtyAreaPercent += stats.fullPaint
            ? 100.0
            : (static_cast<double>(stats.dirtyPixels) * 100.0 / framePixels);
        accumulatedBlitAreaPercent += static_cast<double>(stats.blitPixels) * 100.0 / framePixels;
        accumulatedRectDraws += static_cast<double>(stats.rectDraws);
        accumulatedPolygonDraws += static_cast<double>(stats.polygonDraws);
        accumulatedTextPrepares += static_cast<double>(stats.textPrepares);
        accumulatedTextDraws += static_cast<double>(stats.textDraws);
        accumulatedTextBatchFlushes += static_cast<double>(stats.textBatchFlushes);
        accumulatedTextBatchVertices += static_cast<double>(stats.textBatchVertices);
        accumulatedBackdropCaptures += static_cast<double>(stats.backdropCaptures);
        accumulatedBackdropCaptureReuses += static_cast<double>(stats.backdropCaptureReuses);
        accumulatedImageDraws += static_cast<double>(stats.imageDraws);
        accumulatedRetainedLayerHits += static_cast<double>(stats.retainedLayerHits);
        accumulatedRetainedLayerMisses += static_cast<double>(stats.retainedLayerMisses);
        accumulatedRetainedLayerDraws += static_cast<double>(stats.retainedLayerDraws);
        accumulatedRetainedLayerRebuilds += static_cast<double>(stats.retainedLayerRebuilds);
        accumulatedClearCalls += static_cast<double>(stats.clearCalls);
        accumulatedRenderDirectPasses += static_cast<double>(stats.renderDirectPasses);
        accumulatedCacheBlits += static_cast<double>(stats.cacheBlits);
        accumulatedBackendRenderPasses += static_cast<double>(stats.backendRenderPasses);
        accumulatedBackendRenderPassAreaPercent += static_cast<double>(stats.backendRenderPassPixels) * 100.0 / framePixels;
        accumulatedBackendCopyRegions += static_cast<double>(stats.backendCopyRegions);
        accumulatedBackendBarriers += static_cast<double>(stats.backendBarriers);
        accumulatedBackendSubmits += static_cast<double>(stats.backendSubmits);
        accumulatedBackendPresents += static_cast<double>(stats.backendPresents);
        accumulatedBackendPresentAreaPercent += static_cast<double>(stats.backendPresentPixels) * 100.0 / framePixels;
        accumulatedBackendIncrementalPresents += static_cast<double>(stats.backendIncrementalPresents);
        accumulatedBackendIncrementalPresentSupported += static_cast<double>(stats.backendIncrementalPresentSupported);
        accumulatedBackendResolveDraws += static_cast<double>(stats.backendResolveDraws);
        ++measuredRenderStatsFrames;
        if (stats.fullPaint) {
            ++fullPaintFrames;
        }
        if (stats.usedRenderCache) {
            ++renderCacheFrames;
        }
        if (stats.renderCacheRecreated) {
            ++renderCacheRecreatedFrames;
        }
    }

    template <typename SetTitleFn>
    void updateFrameTitle(double now, SetTitleFn&& setTitle) {
        if (!showDebugStatsInTitle()) {
            return;
        }
        const double elapsed = now - lastTitleUpdate;
        if (elapsed < 1.0) {
            return;
        }

        const core::platform::ProcessUsageSample usage = usageSampler.sample(elapsed);
        const double averageRenderMs = measuredRenderFrames > 0
            ? accumulatedRenderMs / static_cast<double>(measuredRenderFrames)
            : std::numeric_limits<double>::quiet_NaN();

        char cpuText[32];
        if (usage.hasCpuPercent) {
            std::snprintf(cpuText, sizeof(cpuText), "%.0f%%", usage.cpuPercent);
        } else {
            std::snprintf(cpuText, sizeof(cpuText), "n/a");
        }

        const double statsFrames = static_cast<double>(std::max(1, measuredRenderStatsFrames));
        const double averageDirtyRects = accumulatedDirtyRectCount / statsFrames;
        const double averageDirtyAreaPercent = accumulatedDirtyAreaPercent / statsFrames;
        const double averageBlitAreaPercent = accumulatedBlitAreaPercent / statsFrames;
        const double averageRectDraws = accumulatedRectDraws / statsFrames;
        const double averagePolygonDraws = accumulatedPolygonDraws / statsFrames;
        const double averageTextPrepares = accumulatedTextPrepares / statsFrames;
        const double averageTextDraws = accumulatedTextDraws / statsFrames;
        const double averageTextBatchFlushes = accumulatedTextBatchFlushes / statsFrames;
        const double averageTextBatchVertices = accumulatedTextBatchVertices / statsFrames;
        const double averageBackdropCaptures = accumulatedBackdropCaptures / statsFrames;
        const double averageBackdropCaptureReuses = accumulatedBackdropCaptureReuses / statsFrames;
        const double averageImageDraws = accumulatedImageDraws / statsFrames;
        const double averageRetainedLayerHits = accumulatedRetainedLayerHits / statsFrames;
        const double averageRetainedLayerMisses = accumulatedRetainedLayerMisses / statsFrames;
        const double averageRetainedLayerDraws = accumulatedRetainedLayerDraws / statsFrames;
        const double averageRetainedLayerRebuilds = accumulatedRetainedLayerRebuilds / statsFrames;
        const double averageClearCalls = accumulatedClearCalls / statsFrames;
        const double averageRenderDirectPasses = accumulatedRenderDirectPasses / statsFrames;
        const double averageCacheBlits = accumulatedCacheBlits / statsFrames;
        const double averageBackendRenderPasses = accumulatedBackendRenderPasses / statsFrames;
        const double averageBackendRenderPassAreaPercent = accumulatedBackendRenderPassAreaPercent / statsFrames;
        const double averageBackendCopyRegions = accumulatedBackendCopyRegions / statsFrames;
        const double averageBackendBarriers = accumulatedBackendBarriers / statsFrames;
        const double averageBackendSubmits = accumulatedBackendSubmits / statsFrames;
        const double averageBackendPresents = accumulatedBackendPresents / statsFrames;
        const double averageBackendPresentAreaPercent = accumulatedBackendPresentAreaPercent / statsFrames;
        const double averageBackendIncrementalPresents = accumulatedBackendIncrementalPresents / statsFrames;
        const double averageBackendIncrementalPresentSupported = accumulatedBackendIncrementalPresentSupported / statsFrames;
        const double averageBackendResolveDraws = accumulatedBackendResolveDraws / statsFrames;
        const double fullPaintPercent = static_cast<double>(fullPaintFrames) * 100.0 / statsFrames;
        const double cachePercent = static_cast<double>(renderCacheFrames) * 100.0 / statsFrames;
        const double cacheRecreatedPercent = static_cast<double>(renderCacheRecreatedFrames) * 100.0 / statsFrames;

        char renderStatsText[512];
        if (measuredRenderStatsFrames > 0) {
            std::snprintf(renderStatsText,
                          sizeof(renderStatsText),
                          " | Dirty %.1f/%.0f%% | Draw R%.0f P%.0f TP%.0f T%.0f I%.0f | Batch F%.1f V%.0f | Blur C%.1f R%.1f | Layer H%.0f M%.0f D%.0f Re%.0f | Pass %.1f C%.1f B%.1f/%.0f%% | Pipe RP%.1f/%.0f%% Cp%.1f Ba%.1f Sub%.1f Pr%.1f/%.0f%% Inc%.1f/%.1f Rs%.1f | Full %.0f%% Cache %.0f%% Re %.0f%%",
                          averageDirtyRects,
                          averageDirtyAreaPercent,
                          averageRectDraws,
                          averagePolygonDraws,
                          averageTextPrepares,
                          averageTextDraws,
                          averageImageDraws,
                          averageTextBatchFlushes,
                          averageTextBatchVertices,
                          averageBackdropCaptures,
                          averageBackdropCaptureReuses,
                          averageRetainedLayerHits,
                          averageRetainedLayerMisses,
                          averageRetainedLayerDraws,
                          averageRetainedLayerRebuilds,
                          averageRenderDirectPasses,
                          averageClearCalls,
                          averageCacheBlits,
                          averageBlitAreaPercent,
                          averageBackendRenderPasses,
                          averageBackendRenderPassAreaPercent,
                          averageBackendCopyRegions,
                          averageBackendBarriers,
                          averageBackendSubmits,
                          averageBackendPresents,
                          averageBackendPresentAreaPercent,
                          averageBackendIncrementalPresents,
                          averageBackendIncrementalPresentSupported,
                          averageBackendResolveDraws,
                          fullPaintPercent,
                          cachePercent,
                          cacheRecreatedPercent);
        } else {
            renderStatsText[0] = '\0';
        }

        char title[768];
        if (!usage.hasGpuPercent && std::isnan(averageRenderMs)) {
            std::snprintf(title,
                          sizeof(title),
                          "%s - %.0f FPS | CPU %s | GPU n/a%s",
                          windowTitle(),
                          renderedFrames / elapsed,
                          cpuText,
                          renderStatsText);
        } else if (!usage.hasGpuPercent) {
            std::snprintf(title,
                          sizeof(title),
                          "%s - %.0f FPS | CPU %s | GPU %.2f ms%s",
                          windowTitle(),
                          renderedFrames / elapsed,
                          cpuText,
                          averageRenderMs,
                          renderStatsText);
        } else if (std::isnan(averageRenderMs)) {
            std::snprintf(title,
                          sizeof(title),
                          "%s - %.0f FPS | CPU %s | GPU %.0f%%%s",
                          windowTitle(),
                          renderedFrames / elapsed,
                          cpuText,
                          usage.gpuPercent,
                          renderStatsText);
        } else {
            std::snprintf(title,
                          sizeof(title),
                          "%s - %.0f FPS | CPU %s | GPU %.0f%% | Render %.2f ms%s",
                          windowTitle(),
                          renderedFrames / elapsed,
                          cpuText,
                          usage.gpuPercent,
                          averageRenderMs,
                          renderStatsText);
        }
        setTitle(title);
        renderedFrames = 0;
        accumulatedRenderMs = 0.0;
        accumulatedDirtyRectCount = 0.0;
        accumulatedDirtyAreaPercent = 0.0;
        accumulatedBlitAreaPercent = 0.0;
        accumulatedRectDraws = 0.0;
        accumulatedPolygonDraws = 0.0;
        accumulatedTextPrepares = 0.0;
        accumulatedTextDraws = 0.0;
        accumulatedTextBatchFlushes = 0.0;
        accumulatedTextBatchVertices = 0.0;
        accumulatedBackdropCaptures = 0.0;
        accumulatedBackdropCaptureReuses = 0.0;
        accumulatedImageDraws = 0.0;
        accumulatedRetainedLayerHits = 0.0;
        accumulatedRetainedLayerMisses = 0.0;
        accumulatedRetainedLayerDraws = 0.0;
        accumulatedRetainedLayerRebuilds = 0.0;
        accumulatedClearCalls = 0.0;
        accumulatedRenderDirectPasses = 0.0;
        accumulatedCacheBlits = 0.0;
        accumulatedBackendRenderPasses = 0.0;
        accumulatedBackendRenderPassAreaPercent = 0.0;
        accumulatedBackendCopyRegions = 0.0;
        accumulatedBackendBarriers = 0.0;
        accumulatedBackendSubmits = 0.0;
        accumulatedBackendPresents = 0.0;
        accumulatedBackendPresentAreaPercent = 0.0;
        accumulatedBackendIncrementalPresents = 0.0;
        accumulatedBackendIncrementalPresentSupported = 0.0;
        accumulatedBackendResolveDraws = 0.0;
        measuredRenderFrames = 0;
        measuredRenderStatsFrames = 0;
        fullPaintFrames = 0;
        renderCacheFrames = 0;
        renderCacheRecreatedFrames = 0;
        lastTitleUpdate = now;
    }

    void advanceFrameClock(double now, bool animating) {
        if (animating) {
            nextFrameTime += frameInterval;
            if (nextFrameTime > now + frameInterval * 2.0) {
                nextFrameTime = now + frameInterval;
            } else if (nextFrameTime < now - frameInterval) {
                nextFrameTime = now;
            }
        } else if (renderedSinceLastClock) {
            nextFrameTime = now + frameInterval;
        } else {
            nextFrameTime = now;
        }
        renderedSinceLastClock = false;
    }
};

} // namespace app
