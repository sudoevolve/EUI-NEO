#pragma once

#include "eui/dsl.h"
#include "eui/types.h"
#include "eui/window.h"

#include <functional>
#include <string>
#include <vector>

namespace app {

using DslWindowCompose = std::function<void(eui::Ui&, const eui::Screen&)>;

struct DslWindowRequest {
    std::string title = "Window";
    std::string pageId = "window";
    eui::Color clearColor = {0.16f, 0.18f, 0.20f, 1.0f};
    int width = 640;
    int height = 420;
    bool modal = false;
    std::function<void(const eui::KeyEvent&)> onKeyEvent;
    DslWindowCompose compose;
};

const char* windowTitle();
bool showDebugStatsInTitle();
double frameRateLimit();
int initialWindowWidth();
int initialWindowHeight();
float uiScale();
bool trayEnabled();
const char* trayTitle();
const char* trayIconPath();
bool frameless();
void requestUpdate();
bool initialize(eui::window::Handle window);
bool update(eui::window::Handle window, float deltaSeconds, int windowWidth, int windowHeight, float dpiScale, float pointerScale);
bool update(eui::window::Handle window, float deltaSeconds, int windowWidth, int windowHeight, float dpiScale, float pointerScale, bool updateRequested);
bool update(eui::window::Handle window, float deltaSeconds, int windowWidth, int windowHeight, float dpiScale, float pointerScale, bool updateRequested, bool inputEnabled);
bool isAnimating();
void render(int windowWidth, int windowHeight, float dpiScale);
void releaseGraphicsResources();
void shutdown();
std::vector<DslWindowRequest> consumeWindowRequests();

// Window controls for a custom (frameless) title bar. Each operates on the
// window backing the running DSL app, so they only have an effect after
// initialize() has captured the window handle. Safe no-ops otherwise.
void minimizeWindow();
void maximizeWindow();
void toggleMaximizeWindow();
bool isWindowMaximized();
void requestWindowClose();
bool dragWindow();
bool startWindowResize(eui::window::ResizeEdge edge);

namespace detail {
void requestFullPaint();
}

} // namespace app
