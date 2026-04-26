#pragma once

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

namespace app {

const char* windowTitle();
int initialWindowWidth();
int initialWindowHeight();
bool initialize(GLFWwindow* window);
bool update(GLFWwindow* window, float deltaSeconds, int windowWidth, int windowHeight, float dpiScale, float pointerScale);
bool isAnimating();
void render(int windowWidth, int windowHeight, float dpiScale);
void shutdown();

} // namespace app
