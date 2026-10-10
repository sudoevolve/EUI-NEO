#include "eui/dsl_app.h"

#include <cassert>
#include <string>

int main() {
    const app::DslAppConfig defaults;
    assert(defaults.debugTitleIntervalValue == 1.0);
    assert(!defaults.startHandler);
    assert(!defaults.shutdownHandler);
#if defined(EUI_DEBUG_BUILD)
    assert(defaults.showDebugOverlayValue);
#else
    assert(!defaults.showDebugOverlayValue);
#endif

    std::string title = "Owned title";
    std::string pageId = "owned_page";
    std::string appId = "aki";
    std::string iconPath = "icons/app.png";
    std::string textFont = "fonts/text.ttf";
    std::string iconFont = "fonts/icons.ttf";
    std::string trayTitle = "Owned tray title";
    std::string trayIcon = "icons/tray.png";
    int keyEvents = 0;
    bool debugOverlayCalled = false;
    bool startCalled = false;
    bool shutdownCalled = false;

    app::DslAppConfig config = app::DslAppConfig{}
        .title(title)
        .pageId(pageId)
        .appId(appId)
        .windowSize(1280, 720)
        .windowPosition(120, 80)
        .minWindowSize(640, 480)
        .maxWindowSize(1920, 1080)
        .resizable(false)
        .highDpi(false)
        .decorated(false)
        .alwaysOnTop(true)
        .maximized(true)
        .debugTitleInterval(0.5)
        .showDebugOverlay(true)
        .onDebugOverlay([&](eui::Ui&, const eui::Screen&) { debugOverlayCalled = true; })
        .uiScale(1.25f)
        .iconPath(iconPath)
        .fonts(textFont, iconFont)
        .trayTitle(trayTitle)
        .trayIcon(trayIcon)
        .onKeyEvent([&](const eui::KeyEvent&) { ++keyEvents; })
        .onStart([&] { startCalled = true; })
        .onShutdown([&] { shutdownCalled = true; });

    assert(config.startHandler);
    config.startHandler();
    assert(startCalled);
    config.shutdownHandler();
    assert(shutdownCalled);

    assert(config.uiScaleValue == 1.25f);
    assert(config.windowWidthValue == 1280);
    assert(config.windowHeightValue == 720);
    assert(config.windowXValue == 120);
    assert(config.windowYValue == 80);
    assert(config.windowPositionSetValue);
    assert(config.minWindowWidthValue == 640);
    assert(config.minWindowHeightValue == 480);
    assert(config.maxWindowWidthValue == 1920);
    assert(config.maxWindowHeightValue == 1080);
    assert(!config.resizableValue);
    assert(!config.highDpiValue);
    assert(!config.decoratedValue);
    assert(config.alwaysOnTopValue);
    assert(config.maximizedValue);
    assert(config.debugTitleIntervalValue == 0.5);
    assert(config.showDebugOverlayValue);
    assert(static_cast<bool>(config.debugOverlayCompose));
    eui::Ui debugUi;
    config.debugOverlayCompose(debugUi, {});
    assert(debugOverlayCalled);
#if defined(EUI_DEBUG_BUILD)
    assert(config.showDebugStatsInTitleValue);
#else
    assert(!config.showDebugStatsInTitleValue);
#endif

    title.clear();
    pageId.clear();
    appId.clear();
    iconPath.clear();
    textFont.clear();
    iconFont.clear();
    trayTitle.clear();
    trayIcon.clear();

    assert(config.titleValue == "Owned title");
    assert(config.pageIdValue == "owned_page");
    assert(config.appIdValue == "aki");
    assert(config.iconPathValue == "icons/app.png");
    assert(config.textFontFileValue == "fonts/text.ttf");
    assert(config.iconFontFileValue == "fonts/icons.ttf");
    assert(config.trayTitleValue == "Owned tray title");
    assert(config.trayIconPathValue == "icons/tray.png");
    assert(static_cast<bool>(config.keyEventHandler));
    config.keyEventHandler({});
    assert(keyEvents == 1);

    app::DslWindowConfig windowConfig;
    windowConfig.onKeyEvent([&](const eui::KeyEvent&) { ++keyEvents; });
    assert(static_cast<bool>(windowConfig.keyEventHandler));
    windowConfig.keyEventHandler({});
    assert(keyEvents == 2);

    config.title(std::string("Temporary title"));
    config.pageId(std::string("temporary_page"));
    config.appId(std::string("temporary-app"));
    config.iconPath(std::string("icons/temporary.png"));
    config.textFont(std::string("fonts/temporary.ttf"));
    config.iconFont(std::string("fonts/temporary-icons.ttf"));
    config.trayTitle(std::string("Temporary tray title"));
    config.trayIcon(std::string("icons/temporary-tray.png"));

    assert(config.titleValue == "Temporary title");
    assert(config.pageIdValue == "temporary_page");
    assert(config.appIdValue == "temporary-app");
    assert(config.iconPathValue == "icons/temporary.png");
    assert(config.textFontFileValue == "fonts/temporary.ttf");
    assert(config.iconFontFileValue == "fonts/temporary-icons.ttf");
    assert(config.trayTitleValue == "Temporary tray title");
    assert(config.trayIconPathValue == "icons/temporary-tray.png");
    config.uiScale(0.0f);
    assert(config.uiScaleValue == 1.0f);
    config.centerWindow();
    assert(!config.windowPositionSetValue);
    config.debugTitleInterval(0.0);
    assert(config.debugTitleIntervalValue == 1.0);

    config.minWindowSize(640, 0);
    config.maxWindowSize(0, 1080);
    assert(config.minWindowWidthValue == 640);
    assert(config.minWindowHeightValue == 0);
    assert(config.maxWindowWidthValue == 0);
    assert(config.maxWindowHeightValue == 1080);
    return 0;
}
