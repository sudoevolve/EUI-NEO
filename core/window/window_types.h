#pragma once

namespace core::window {

using Handle = void*;
using ContextKey = void*;
using CursorHandle = void*;

enum class CursorType {
    Arrow,
    Hand
};

enum class RenderApi {
    OpenGL,
    Vulkan
};

enum class ResizeEdge {
    TopLeft,
    Top,
    TopRight,
    Right,
    BottomRight,
    Bottom,
    BottomLeft,
    Left
};

struct WindowCreateRequest {
    int width = 0;
    int height = 0;
    const char* title = "";
    bool resizable = true;
    bool highDpi = true;
    bool modal = false;
    bool decorated = true;
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
