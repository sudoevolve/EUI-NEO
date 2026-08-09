#pragma once

#include "core/window/window_types.h"

#include <string>

namespace core::window {

Handle createWindow(const WindowCreateRequest& request);
void destroyWindow(Handle window);
NativeWindowInfo nativeWindowInfo(Handle window);

// SDL2 only: estimates the desktop scaling factor for a display via
// SDL_GetDisplayDPI, quantized to 0.25 steps. Returns 1.0f when the scale
// cannot be determined or is below 1.25x, and on platforms where SDL already
// reports high-density drawable sizes (Windows, macOS).
float displayScaleEstimate(int displayIndex);

ContextKey currentContextKey();
double timeSeconds();
void postEmptyEvent();

void getCursorPosition(Handle window, double& x, double& y);
bool isMouseButtonDown(Handle window, int button);
std::string clipboardText(Handle window);
void setClipboardText(const std::string& text);

CursorHandle createStandardCursor(CursorType type);
void setCursor(Handle window, CursorHandle cursor);
void destroyCursor(CursorHandle cursor);

void setWindowIcon(Handle window, int width, int height, unsigned char* pixels);
void setImeCursorRect(Handle window, float x, float y, float width, float height);
void installInputCallbacks(Handle window);
void uninstallInputCallbacks(Handle window);
bool queryImeComposition(Handle window, std::string& text, bool& composing);

} // namespace core::window
