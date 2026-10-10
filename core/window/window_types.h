#pragma once

#include <string>

namespace core::window {

using Handle = void*;
using ContextKey = void*;
using CursorHandle = void*;

enum class CursorType {
    Arrow,
    Hand
};

// Edge or corner that starts a compositor-mediated resize. The values match
// the EWMH _NET_WM_MOVERESIZE SIZE_* directions (0-7); move is 8.
enum class WindowResizeEdge {
    TopLeft,
    Top,
    TopRight,
    Right,
    BottomRight,
    Bottom,
    BottomLeft,
    Left
};

enum class RenderApi {
    OpenGL,
    Vulkan
};

struct WindowCreateRequest {
    int width = 0;
    int height = 0;
    int x = 0;
    int y = 0;
    bool positionSet = false;
    int minWidth = 0;
    int minHeight = 0;
    int maxWidth = 0;
    int maxHeight = 0;
    const char* title = "";
    std::string appId;
    bool resizable = true;
    bool highDpi = true;
    bool decorated = true;
    bool transparent = false;
    bool alwaysOnTop = false;
    bool maximized = false;
    bool modal = false;
    Handle parent = nullptr;
    RenderApi renderApi = RenderApi::OpenGL;
};

struct NativeWindowInfo {
    Handle handle = nullptr;
    void* platformWindow = nullptr;
    void* platformDisplay = nullptr;
    void* platformView = nullptr;
};

} // namespace core::window
