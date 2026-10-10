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
std::string clipboardText(Handle window);
void setClipboardText(const std::string& text);

CursorHandle createStandardCursor(CursorType type);
void setCursor(Handle window, CursorHandle cursor);
void destroyCursor(CursorHandle cursor);

void setWindowIcon(Handle window, int width, int height, unsigned char* pixels);
void setImeCursorRect(Handle window, float x, float y, float width, float height);
// Reports whether the created framebuffer actually carries alpha (GLFW
// honors the transparent request; SDL2 and compositor-less X11 do not).
bool framebufferTransparent(Handle window);
// Hand an interactive move/resize to the window manager (EWMH
// _NET_WM_MOVERESIZE on X11). Per-frame programmatic moves arrive as
// ConfigureRequests and can be clamped back into the workarea, so undecorated
// windows should delegate title-bar drags this way. Returns false when the
// current platform/backend cannot delegate; callers keep their own fallback.
bool beginWindowMove(Handle window);
bool beginWindowResize(Handle window, WindowResizeEdge edge);
void installInputCallbacks(Handle window);
void uninstallInputCallbacks(Handle window);
bool queryImeComposition(Handle window, std::string& text, bool& composing);

} // namespace core::window
