#pragma once

#include "eui/dsl_app.h"
#include "eui/network.h"

#include "3rd/stb_image.h"
#include "core/dsl_runtime.h"
#include "core/platform/platform.h"
#include "core/render/text.h"

#include <algorithm>
#include <filesystem>
#include <vector>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

namespace app {

namespace detail {

inline core::dsl::Runtime& dslRuntime() {
    static core::dsl::Runtime runtime;
    return runtime;
}

inline std::vector<DslWindowRequest>& dslWindowRequests() {
    static std::vector<DslWindowRequest> requests;
    return requests;
}

struct DslAppState {
    bool composed = false;
    bool iconApplied = false;
    float logicalWidth = 0.0f;
    float logicalHeight = 0.0f;
};

inline DslAppState& dslAppState() {
    static DslAppState state;
    return state;
}

inline std::string resolveIconPath(const std::string& iconPath) {
    if (iconPath.empty()) {
        return {};
    }

    namespace fs = std::filesystem;
    std::error_code error;
    const fs::path requested(iconPath);
    const fs::path current = fs::current_path(error);
    std::vector<fs::path> candidates;
    candidates.push_back(requested);
    if (!error) {
        candidates.push_back(current / requested);
        candidates.push_back(current / "assets" / requested.filename());
    }

    fs::path executableDir;
#if defined(__APPLE__)
    char executablePath[4096];
    uint32_t executablePathSize = sizeof(executablePath);
    if (_NSGetExecutablePath(executablePath, &executablePathSize) == 0) {
        executableDir = fs::absolute(fs::path(executablePath), error).parent_path();
    }
#elif defined(_WIN32)
    char executablePath[MAX_PATH];
    const DWORD executablePathSize = GetModuleFileNameA(nullptr, executablePath, MAX_PATH);
    if (executablePathSize > 0 && executablePathSize < MAX_PATH) {
        executableDir = fs::absolute(fs::path(executablePath), error).parent_path();
    }
#elif defined(__linux__)
    char executablePath[4096];
    const ssize_t executablePathSize = readlink("/proc/self/exe", executablePath, sizeof(executablePath) - 1);
    if (executablePathSize > 0) {
        executablePath[executablePathSize] = '\0';
        executableDir = fs::absolute(fs::path(executablePath), error).parent_path();
    }
#endif
    if (!executableDir.empty()) {
        candidates.push_back(executableDir / requested);
        candidates.push_back(executableDir / "assets" / requested.filename());
    }

    for (const fs::path& candidate : candidates) {
        error.clear();
        if (fs::exists(candidate, error) && !error) {
            return fs::absolute(candidate, error).string();
        }
    }
    return {};
}

inline void applyWindowIcon(core::window::Handle window) {
    if (window == nullptr) {
        return;
    }

    const std::string iconPath = resolveIconPath(dslAppConfig().iconPathValue);
    if (iconPath.empty()) {
        return;
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_set_flip_vertically_on_load(0);
    unsigned char* pixels = stbi_load(iconPath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
    if (pixels == nullptr || width <= 0 || height <= 0) {
        if (pixels != nullptr) {
            stbi_image_free(pixels);
        }
        return;
    }

    core::window::setWindowIcon(window, width, height, pixels);
    stbi_image_free(pixels);
}

} // namespace detail

void openWindow(const DslWindowConfig& config, DslWindowCompose composeFn) {
    if (!composeFn) {
        return;
    }

    DslWindowRequest request;
    request.title = config.titleValue.empty() ? "Window" : config.titleValue;
    request.pageId = config.pageIdValue.empty() ? request.title : config.pageIdValue;
    request.clearColor = config.clearColorValue;
    request.width = std::max(160, config.windowWidthValue);
    request.height = std::max(120, config.windowHeightValue);
    request.modal = config.modalValue;
    request.compose = std::move(composeFn);
    detail::dslWindowRequests().push_back(std::move(request));
    requestUpdate();
}

void openWindow(const char* title, int width, int height, DslWindowCompose composeFn) {
    openWindow(DslWindowConfig{}
                   .title(title != nullptr ? title : "Window")
                   .pageId(title != nullptr ? title : "window")
                   .windowSize(width, height),
               std::move(composeFn));
}

std::vector<DslWindowRequest> consumeWindowRequests() {
    std::vector<DslWindowRequest> requests = std::move(detail::dslWindowRequests());
    detail::dslWindowRequests().clear();
    return requests;
}

const char* windowTitle() {
    return dslAppConfig().titleValue.c_str();
}

bool showDebugStatsInTitle() {
    return dslAppConfig().showDebugStatsInTitleValue;
}

double frameRateLimit() {
    return dslAppConfig().fpsValue;
}

int initialWindowWidth() {
    return dslAppConfig().windowWidthValue;
}

int initialWindowHeight() {
    return dslAppConfig().windowHeightValue;
}

bool trayEnabled() {
    return dslAppConfig().trayEnabledValue;
}

const char* trayTitle() {
    const DslAppConfig& config = dslAppConfig();
    return (config.trayTitleValue.empty() ? config.titleValue : config.trayTitleValue).c_str();
}

const char* trayIconPath() {
    const DslAppConfig& config = dslAppConfig();
    return (config.trayIconPathValue.empty() ? config.iconPathValue : config.trayIconPathValue).c_str();
}

void requestUpdate() {
    core::platform::requestUiUpdate();
}

namespace detail {

void requestFullPaint() {
    dslRuntime().requestFullPaint();
    core::platform::requestUiUpdate();
}

} // namespace detail

bool initialize(core::window::Handle window) {
    const DslAppConfig& config = dslAppConfig();
    core::TextPrimitive::setDefaultFontFiles(config.textFontFileValue, config.iconFontFileValue);

    detail::DslAppState& state = detail::dslAppState();
    if (!state.iconApplied) {
        detail::applyWindowIcon(window);
        state.iconApplied = true;
    }
    return detail::dslRuntime().initialize(window);
}

bool update(core::window::Handle window, float deltaSeconds, int windowWidth, int windowHeight, float dpiScale, float pointerScale) {
    const bool asyncReady = core::async::dispatchReady();
    const bool updateRequested = core::platform::consumeUiUpdate();
    return update(window, deltaSeconds, windowWidth, windowHeight, dpiScale, pointerScale, updateRequested || asyncReady);
}

bool update(core::window::Handle window, float deltaSeconds, int windowWidth, int windowHeight, float dpiScale, float pointerScale, bool updateRequested) {
    return update(window, deltaSeconds, windowWidth, windowHeight, dpiScale, pointerScale, updateRequested, true);
}

bool update(core::window::Handle window, float deltaSeconds, int windowWidth, int windowHeight, float dpiScale, float pointerScale, bool updateRequested, bool inputEnabled) {
    if (windowWidth <= 0 || windowHeight <= 0 || dpiScale <= 0.0f) {
        return false;
    }

    const DslAppConfig& config = dslAppConfig();
    const float logicalWidth = static_cast<float>(windowWidth) / dpiScale;
    const float logicalHeight = static_cast<float>(windowHeight) / dpiScale;
    detail::DslAppState& state = detail::dslAppState();

    const auto composeFrame = [&] {
        detail::dslRuntime().compose(config.pageIdValue, logicalWidth, logicalHeight, [](core::dsl::Ui& ui, const core::dsl::Screen& screen) {
            compose(ui, screen);
        });
        state.composed = true;
        state.logicalWidth = logicalWidth;
        state.logicalHeight = logicalHeight;
    };

    if (!state.composed || state.logicalWidth != logicalWidth || state.logicalHeight != logicalHeight) {
        composeFrame();
    }

    bool changed = false;
    if (updateRequested) {
        composeFrame();
        changed = true;
    }

    changed = detail::dslRuntime().update(window, deltaSeconds, pointerScale, dpiScale, inputEnabled) || changed;
    if (detail::dslRuntime().composeRequested()) {
        composeFrame();
        changed = detail::dslRuntime().update(window, 0.0f, pointerScale, dpiScale, inputEnabled) || changed;
        changed = true;
    }

    return changed;
}

bool isAnimating() {
    return detail::dslRuntime().isAnimating();
}

void render(int windowWidth, int windowHeight, float dpiScale) {
    if (windowWidth <= 0 || windowHeight <= 0 || dpiScale <= 0.0f) {
        return;
    }

    const core::Color clearColor = dslAppConfig().clearColorValue;
    detail::dslRuntime().render(windowWidth, windowHeight, dpiScale, clearColor);
}

void releaseGraphicsResources() {
    detail::dslRuntime().releaseGraphicsResources();
}

void shutdown() {
    core::async::shutdown();
    detail::dslRuntime().shutdown();
    eui::network::shutdown();
}

} // namespace app
