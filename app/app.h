#pragma once

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#include "core/primitive.h"

#include <functional>
#include <string>
#include <vector>

namespace core::dsl {
class Ui;
struct Screen;
}

namespace app {

using DslWindowCompose = std::function<void(core::dsl::Ui&, const core::dsl::Screen&)>;

struct DslWindowRequest {
    std::string title = "Window";
    std::string pageId = "window";
    core::Color clearColor = {0.16f, 0.18f, 0.20f, 1.0f};
    int width = 640;
    int height = 420;
    bool modal = false;
    DslWindowCompose compose;
};

const char* windowTitle();
bool showFrameCountInTitle();
double frameRateLimit();
int initialWindowWidth();
int initialWindowHeight();
bool trayEnabled();
const char* trayTitle();
const char* trayIconPath();
bool initialize(GLFWwindow* window);
bool update(GLFWwindow* window, float deltaSeconds, int windowWidth, int windowHeight, float dpiScale, float pointerScale);
bool update(GLFWwindow* window, float deltaSeconds, int windowWidth, int windowHeight, float dpiScale, float pointerScale, bool networkTextReady);
bool update(GLFWwindow* window, float deltaSeconds, int windowWidth, int windowHeight, float dpiScale, float pointerScale, bool networkTextReady, bool inputEnabled);
bool isAnimating();
void render(int windowWidth, int windowHeight, float dpiScale);
void releaseGraphicsResources();
void shutdown();
std::vector<DslWindowRequest> consumeWindowRequests();

} // namespace app
