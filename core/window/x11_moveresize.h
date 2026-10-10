#pragma once

// Compositor-mediated move/resize for X11 windows via the EWMH
// _NET_WM_MOVERESIZE ClientMessage. libX11 is loaded dynamically, matching the
// backends' other X11 lookups, so no build configuration gains a link-time X11
// dependency; when libX11 or the protocol is unavailable the request is
// reported as failed and callers keep their programmatic fallback.

#include <cstdint>

// EWMH _NET_WM_MOVERESIZE direction value for moves; 0-7 are the
// WindowResizeEdge corners/edges.
inline constexpr int kNetWmMoveResizeMove = 8;

#if defined(__linux__) && !defined(__ANDROID__)

#include <dlfcn.h>
#include <X11/Xlib.h>

namespace core::window::detail {

struct X11MoveResizeApi {
    void* library = nullptr;
    Atom (*internAtom)(Display*, const char*, Bool) = nullptr;
    Bool (*queryPointer)(Display*, Window, Window*, Window*, int*, int*, int*, int*,
                         unsigned int*) = nullptr;
    int (*ungrabPointer)(Display*, Time) = nullptr;
    Status (*sendEvent)(Display*, Window, Bool, long, XEvent*) = nullptr;
    int (*flush)(Display*) = nullptr;

    bool valid() const {
        return library && internAtom && queryPointer && ungrabPointer && sendEvent && flush;
    }
};

inline const X11MoveResizeApi& x11MoveResizeApi() {
    static const X11MoveResizeApi api = [] {
        X11MoveResizeApi result;
        result.library = dlopen("libX11.so.6", RTLD_LAZY | RTLD_LOCAL);
        if (!result.library) {
            result.library = dlopen("libX11.so", RTLD_LAZY | RTLD_LOCAL);
        }
        if (!result.library) {
            return result;
        }
        result.internAtom = reinterpret_cast<Atom (*)(Display*, const char*, Bool)>(
            dlsym(result.library, "XInternAtom"));
        result.queryPointer =
            reinterpret_cast<Bool (*)(Display*, Window, Window*, Window*, int*, int*, int*, int*,
                                      unsigned int*)>(dlsym(result.library, "XQueryPointer"));
        result.ungrabPointer =
            reinterpret_cast<int (*)(Display*, Time)>(dlsym(result.library, "XUngrabPointer"));
        result.sendEvent = reinterpret_cast<Status (*)(Display*, Window, Bool, long, XEvent*)>(
            dlsym(result.library, "XSendEvent"));
        result.flush = reinterpret_cast<int (*)(Display*)>(dlsym(result.library, "XFlush"));
        return result;
    }();
    return api;
}

inline bool sendX11MoveResize(void* displayHandle, std::uint64_t windowId, int direction) {
    Display* display = static_cast<Display*>(displayHandle);
    const Window window = static_cast<Window>(windowId);
    const X11MoveResizeApi& api = x11MoveResizeApi();
    if (!display || !window || !api.valid()) {
        return false;
    }

    const Window root = DefaultRootWindow(display);
    Window rootReturn = None;
    Window childReturn = None;
    int rootX = 0, rootY = 0, winX = 0, winY = 0;
    unsigned int mask = 0;
    if (!api.queryPointer(display, root, &rootReturn, &childReturn, &rootX, &rootY, &winX, &winY,
                          &mask)) {
        return false;
    }

    XEvent event{};
    event.xclient.type = ClientMessage;
    event.xclient.send_event = True;
    event.xclient.display = display;
    event.xclient.window = window;
    event.xclient.message_type = api.internAtom(display, "_NET_WM_MOVERESIZE", False);
    event.xclient.format = 32;
    event.xclient.data.l[0] = static_cast<long>(rootX);
    event.xclient.data.l[1] = static_cast<long>(rootY);
    event.xclient.data.l[2] = static_cast<long>(direction);
    event.xclient.data.l[3] = Button1;
    event.xclient.data.l[4] = 1; // source indication: application
    // The implicit grab from the initiating press must be released before the
    // window manager can take over the pointer.
    api.ungrabPointer(display, CurrentTime);
    const Bool sent = api.sendEvent(display, root, False,
                                    SubstructureRedirectMask | SubstructureNotifyMask, &event);
    api.flush(display);
    return event.xclient.message_type != None && sent != False;
}

} // namespace core::window::detail

#else

namespace core::window::detail {

inline bool sendX11MoveResize(void*, std::uint64_t, int) {
    return false;
}

} // namespace core::window::detail

#endif
