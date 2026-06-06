#pragma once

#include "eui/app.h"
#include "core/platform/async.h"
#include "core/platform/network.h"
#include "core/platform/performance_stats.h"
#include "core/platform/platform.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

namespace app {

struct AppRunner {
    bool needsRender = true;
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
    int measuredRenderFrames = 0;
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

    bool consumeExternalReady() {
        const bool asyncReady = core::async::dispatchReady();
        return core::network::consumeAnyTextReady() || asyncReady;
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
        needsRender = false;
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

    template <typename SetTitleFn>
    void updateFrameTitle(double now, SetTitleFn&& setTitle) {
        if (!showFrameCountInTitle()) {
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

        char title[224];
        if (!usage.hasGpuPercent && std::isnan(averageRenderMs)) {
            std::snprintf(title,
                          sizeof(title),
                          "%s - %.0f FPS | CPU %s | GPU n/a",
                          windowTitle(),
                          renderedFrames / elapsed,
                          cpuText);
        } else if (!usage.hasGpuPercent) {
            std::snprintf(title,
                          sizeof(title),
                          "%s - %.0f FPS | CPU %s | GPU %.2f ms",
                          windowTitle(),
                          renderedFrames / elapsed,
                          cpuText,
                          averageRenderMs);
        } else if (std::isnan(averageRenderMs)) {
            std::snprintf(title,
                          sizeof(title),
                          "%s - %.0f FPS | CPU %s | GPU %.0f%%",
                          windowTitle(),
                          renderedFrames / elapsed,
                          cpuText,
                          usage.gpuPercent);
        } else {
            std::snprintf(title,
                          sizeof(title),
                          "%s - %.0f FPS | CPU %s | GPU %.0f%% | Render %.2f ms",
                          windowTitle(),
                          renderedFrames / elapsed,
                          cpuText,
                          usage.gpuPercent,
                          averageRenderMs);
        }
        setTitle(title);
        renderedFrames = 0;
        accumulatedRenderMs = 0.0;
        measuredRenderFrames = 0;
        lastTitleUpdate = now;
    }

    void advanceFrameClock(double now, bool animating) {
        if (animating || renderedSinceLastClock) {
            nextFrameTime += frameInterval;
            if (nextFrameTime <= now || nextFrameTime > now + frameInterval * 2.0) {
                nextFrameTime = now + frameInterval;
            }
        } else {
            nextFrameTime = now;
        }
        renderedSinceLastClock = false;
    }
};

} // namespace app
