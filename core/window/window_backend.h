#pragma once

#include "core/window/window_types.h"

#include <string>

namespace core::window {

Handle createWindow(const WindowCreateRequest& request);
void destroyWindow(Handle window);
NativeWindowInfo nativeWindowInfo(Handle window);
#if defined(EUI_WINDOW_BACKEND_SDL2)
// SDL2 desktop Linux only: returns Xft.dpi / 96 for an X11 window,
// or 0.0f when SDL selected another video backend.
float x11ContentScale(Handle window);
#endif

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

// Window controls used by a custom (frameless) title bar. All are no-ops on a
// null handle. minimize/maximize/restore/close are cross-platform in both
// backends; dragWindow/startWindowResize need a per-platform bridge (see the
// .cpp) and return false when the platform can't synthesize the move/resize
// (e.g. Wayland).
void minimizeWindow(Handle window);
void maximizeWindow(Handle window);
void restoreWindow(Handle window);
bool isWindowMaximized(Handle window);
void requestWindowClose(Handle window);
bool dragWindow(Handle window);
bool startWindowResize(Handle window, ResizeEdge edge);

} // namespace core::window
