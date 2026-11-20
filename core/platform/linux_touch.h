#pragma once

struct GLFWwindow;

namespace core::platform {

void installLinuxTouch(GLFWwindow* window);
void pollLinuxTouch(GLFWwindow* window, float pointerScale);
void resetLinuxTouch(GLFWwindow* window);
void uninstallLinuxTouch(GLFWwindow* window);

#if defined(EUI_LINUX_TOUCH_TESTING)
bool injectLinuxTouchTestEvents(GLFWwindow* window);
bool linuxTouchReady(GLFWwindow* window);
#endif

} // namespace core::platform
