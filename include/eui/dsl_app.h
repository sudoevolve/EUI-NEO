#pragma once

#include "eui/app.h"
#include "eui/async.h"

#include <string>
#include <utility>

namespace app {

struct DslAppConfig {
    std::string titleValue = "App";
    std::string pageIdValue = "app";
    eui::Color clearColorValue = {0.16f, 0.18f, 0.20f, 1.0f};
    int windowWidthValue = 800;
    int windowHeightValue = 600;
#ifndef NDEBUG
    bool showDebugStatsInTitleValue = true;
#else
    bool showDebugStatsInTitleValue = false;
#endif
    double fpsValue = 90.0;
    std::string iconPathValue = "assets/icon.png";
    std::string textFontFileValue;
    std::string iconFontFileValue;
    bool trayEnabledValue = false;
    std::string trayTitleValue;
    std::string trayIconPathValue;

    DslAppConfig& title(std::string value) { titleValue = std::move(value); return *this; }
    DslAppConfig& pageId(std::string value) { pageIdValue = std::move(value); return *this; }
    DslAppConfig& clearColor(const eui::Color& value) { clearColorValue = value; return *this; }
    DslAppConfig& background(const eui::Color& value) { return clearColor(value); }
    DslAppConfig& windowSize(int width, int height) {
        windowWidthValue = width;
        windowHeightValue = height;
        return *this;
    }
    DslAppConfig& windowWidth(int value) { windowWidthValue = value; return *this; }
    DslAppConfig& windowHeight(int value) { windowHeightValue = value; return *this; }
    DslAppConfig& showDebugStatsInTitle(bool value = true) {
        showDebugStatsInTitleValue = value;
        return *this;
    }
    DslAppConfig& fps(double value) { fpsValue = value; return *this; }
    DslAppConfig& iconPath(std::string value) { iconPathValue = std::move(value); return *this; }
    DslAppConfig& textFont(std::string value) { textFontFileValue = std::move(value); return *this; }
    DslAppConfig& iconFont(std::string value) { iconFontFileValue = std::move(value); return *this; }
    DslAppConfig& fonts(std::string textFont, std::string iconFont = {}) {
        textFontFileValue = std::move(textFont);
        iconFontFileValue = std::move(iconFont);
        return *this;
    }
    DslAppConfig& tray(bool value = true) {
        trayEnabledValue = value;
        return *this;
    }
    DslAppConfig& trayTitle(std::string value) {
        trayTitleValue = std::move(value);
        return *this;
    }
    DslAppConfig& trayIcon(std::string value) {
        trayIconPathValue = std::move(value);
        return *this;
    }
};

struct DslWindowConfig {
    std::string titleValue = "Window";
    std::string pageIdValue = "window";
    eui::Color clearColorValue = {0.16f, 0.18f, 0.20f, 1.0f};
    int windowWidthValue = 640;
    int windowHeightValue = 420;
    bool modalValue = false;

    DslWindowConfig& title(std::string value) { titleValue = std::move(value); return *this; }
    DslWindowConfig& pageId(std::string value) { pageIdValue = std::move(value); return *this; }
    DslWindowConfig& clearColor(const eui::Color& value) { clearColorValue = value; return *this; }
    DslWindowConfig& background(const eui::Color& value) { return clearColor(value); }
    DslWindowConfig& windowSize(int width, int height) {
        windowWidthValue = width;
        windowHeightValue = height;
        return *this;
    }
    DslWindowConfig& windowWidth(int value) { windowWidthValue = value; return *this; }
    DslWindowConfig& windowHeight(int value) { windowHeightValue = value; return *this; }
    DslWindowConfig& modal(bool value = true) { modalValue = value; return *this; }
};

const DslAppConfig& dslAppConfig();
void compose(eui::Ui& ui, const eui::Screen& screen);

void openWindow(const DslWindowConfig& config, DslWindowCompose composeFn);
void openWindow(const char* title, int width, int height, DslWindowCompose composeFn);

} // namespace app
