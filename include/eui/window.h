#pragma once

#include "core/window/window_backend.h"
#include "core/window/window_types.h"

namespace eui::window {

using ContextKey = core::window::ContextKey;
using CursorHandle = core::window::CursorHandle;
using CursorType = core::window::CursorType;
using Handle = core::window::Handle;
using WindowResizeEdge = core::window::WindowResizeEdge;

// Hand an interactive move/resize to the window manager (EWMH
// _NET_WM_MOVERESIZE on X11). Returns false when the current
// platform/backend cannot delegate; callers keep their own fallback.
using core::window::beginWindowMove;
using core::window::beginWindowResize;

} // namespace eui::window
