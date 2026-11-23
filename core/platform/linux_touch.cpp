#include "core/platform/linux_touch.h"
#include "core/input/touch_tracker.h"

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#if defined(EUI_LINUX_TOUCH_X11)
#define GLFW_EXPOSE_NATIVE_X11
#endif
#if defined(EUI_LINUX_TOUCH_WAYLAND)
#define GLFW_EXPOSE_NATIVE_WAYLAND
#endif
#include <GLFW/glfw3native.h>

#if defined(EUI_LINUX_TOUCH_X11)
#include <X11/extensions/XInput2.h>
#include <poll.h>
#endif
#if defined(EUI_LINUX_TOUCH_WAYLAND)
#include <wayland-client.h>
#include "eui-pointer-gestures.h"
#endif

#include <atomic>
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace core::platform {
namespace {

enum class Kind { Down, Move, Up, Cancel, GestureBegin, GestureUpdate, GestureEnd, GestureCancel };

struct Event {
    Kind kind;
    std::int64_t device = 0;
    std::int64_t id = 0;
    double x = 0;
    double y = 0;
    double dx = 0;
    double dy = 0;
    double scale = 1;
    double rotation = 0;
    unsigned int fingers = 0;
};

struct WindowState {
    explicit WindowState(GLFWwindow* value) : window(value), tracker(value) {}
    GLFWwindow* window;
    input::TouchTracker tracker;
    bool nativeGestureActive = false;
    std::mutex mutex;
    std::deque<Event> events;
#if defined(EUI_LINUX_TOUCH_X11)
    Display* display = nullptr;
    int opcode = 0;
    std::atomic<bool> stop{false};
    std::thread thread;
#endif
};

std::unordered_map<GLFWwindow*, std::unique_ptr<WindowState>>& windows() {
    static std::unordered_map<GLFWwindow*, std::unique_ptr<WindowState>> value;
    return value;
}

void enqueue(WindowState& state, Event event) {
    std::lock_guard<std::mutex> lock(state.mutex);
    state.events.push_back(event);
}

#if defined(EUI_LINUX_TOUCH_X11)
struct X11GestureState {
    double x = 0;
    double y = 0;
    double scale = 1;
};

#if defined(XI_GesturePinchBegin)
bool translateX11Pinch(const XIGesturePinchEvent& native,
                       std::unordered_map<int, X11GestureState>& active,
                       Event& event) {
    event = Event{Kind::GestureUpdate};
    if (native.evtype == XI_GesturePinchBegin) {
        active[native.sourceid] = {native.event_x, native.event_y, 1.0};
    }
    auto it = active.find(native.sourceid);
    if (it == active.end()) return false;
    X11GestureState& gesture = it->second;
    event.kind = native.evtype == XI_GesturePinchBegin ? Kind::GestureBegin :
        native.evtype == XI_GesturePinchUpdate ? Kind::GestureUpdate :
        (native.flags & XIGesturePinchEventCancelled) ? Kind::GestureCancel : Kind::GestureEnd;
    event.dx = native.delta_x;
    event.dy = native.delta_y;
    gesture.x += event.dx;
    gesture.y += event.dy;
    event.x = gesture.x;
    event.y = gesture.y;
    if (native.evtype == XI_GesturePinchUpdate) {
        event.scale = native.scale > 0.0 && gesture.scale > 0.0
            ? native.scale / gesture.scale : 1.0;
        if (native.scale > 0.0) gesture.scale = native.scale;
        event.rotation = native.delta_angle;
    }
    event.fingers = static_cast<unsigned int>(std::max(native.detail, 0));
    if (native.evtype == XI_GesturePinchEnd) active.erase(it);
    return true;
}

bool translateX11Swipe(const XIGestureSwipeEvent& native,
                       std::unordered_map<int, X11GestureState>& active,
                       Event& event) {
    event = Event{Kind::GestureUpdate};
    if (native.evtype == XI_GestureSwipeBegin) {
        active[native.sourceid] = {native.event_x, native.event_y, 1.0};
    }
    auto it = active.find(native.sourceid);
    if (it == active.end()) return false;
    X11GestureState& gesture = it->second;
    event.kind = native.evtype == XI_GestureSwipeBegin ? Kind::GestureBegin :
        native.evtype == XI_GestureSwipeUpdate ? Kind::GestureUpdate :
        (native.flags & XIGestureSwipeEventCancelled) ? Kind::GestureCancel : Kind::GestureEnd;
    event.dx = native.delta_x;
    event.dy = native.delta_y;
    gesture.x += event.dx;
    gesture.y += event.dy;
    event.x = gesture.x;
    event.y = gesture.y;
    event.fingers = static_cast<unsigned int>(std::max(native.detail, 0));
    if (native.evtype == XI_GestureSwipeEnd) active.erase(it);
    return true;
}
#endif

void x11EventLoop(WindowState& state) {
    pollfd descriptor{ConnectionNumber(state.display), POLLIN, 0};
    std::unordered_map<int, X11GestureState> activeGestures;
    while (!state.stop.load()) {
        while (!state.stop.load() && XPending(state.display) > 0) {
            XEvent event{};
            XNextEvent(state.display, &event);
            if (event.type != GenericEvent || event.xcookie.extension != state.opcode ||
                !XGetEventData(state.display, &event.xcookie)) {
                continue;
            }
            const int type = event.xcookie.evtype;
            if (type == XI_TouchBegin || type == XI_TouchUpdate || type == XI_TouchEnd) {
                const auto* touch = static_cast<XIDeviceEvent*>(event.xcookie.data);
                Event input{type == XI_TouchBegin ? Kind::Down :
                            type == XI_TouchUpdate ? Kind::Move : Kind::Up};
                input.device = touch->sourceid;
                input.id = touch->detail;
                input.x = touch->event_x;
                input.y = touch->event_y;
                enqueue(state, input);
                glfwPostEmptyEvent();
            }
#if defined(XI_GesturePinchBegin)
            else if (type >= XI_GesturePinchBegin && type <= XI_GestureSwipeEnd) {
                Event input{Kind::GestureUpdate};
                const bool valid = type <= XI_GesturePinchEnd
                    ? translateX11Pinch(*static_cast<XIGesturePinchEvent*>(event.xcookie.data),
                                        activeGestures, input)
                    : translateX11Swipe(*static_cast<XIGestureSwipeEvent*>(event.xcookie.data),
                                        activeGestures, input);
                if (valid) {
                    enqueue(state, input);
                    glfwPostEmptyEvent();
                }
            }
#endif
            XFreeEventData(state.display, &event.xcookie);
        }
        if (state.stop.load()) break;
        const int result = poll(&descriptor, 1, 100);
        if ((result < 0 && errno != EINTR) ||
            (result > 0 && (descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)))) {
            std::fprintf(stderr, "EUI: X11 touch event polling failed\n");
            break;
        }
    }
}

void installX11(WindowState& state) {
    Display* glfwDisplay = glfwGetX11Display();
    if (!glfwDisplay) return;
    state.display = XOpenDisplay(DisplayString(glfwDisplay));
    if (!state.display) {
        std::fprintf(stderr, "EUI: unable to open X11 touch connection\n");
        return;
    }
    int eventBase = 0, errorBase = 0;
    int major = 2;
#if defined(XI_GesturePinchBegin)
    int minor = 4;
#else
    int minor = 2;
#endif
    if (!XQueryExtension(state.display, "XInputExtension", &state.opcode, &eventBase, &errorBase) ||
        XIQueryVersion(state.display, &major, &minor) != Success ||
        major < 2 || (major == 2 && minor < 2)) {
        std::fprintf(stderr, "EUI: XInput 2.2 touch events are unavailable\n");
        XCloseDisplay(state.display);
        state.display = nullptr;
        return;
    }
    unsigned char mask[XIMaskLen(XI_LASTEVENT)]{};
    XISetMask(mask, XI_TouchBegin);
    XISetMask(mask, XI_TouchUpdate);
    XISetMask(mask, XI_TouchEnd);
#if defined(XI_GesturePinchBegin)
    if (major > 2 || (major == 2 && minor >= 4)) {
        XISetMask(mask, XI_GesturePinchBegin);
        XISetMask(mask, XI_GesturePinchUpdate);
        XISetMask(mask, XI_GesturePinchEnd);
        XISetMask(mask, XI_GestureSwipeBegin);
        XISetMask(mask, XI_GestureSwipeUpdate);
        XISetMask(mask, XI_GestureSwipeEnd);
    }
#endif
    XIEventMask eventMask{XIAllMasterDevices, static_cast<int>(sizeof(mask)), mask};
    if (XISelectEvents(state.display, glfwGetX11Window(state.window), &eventMask, 1) != Success) {
        std::fprintf(stderr, "EUI: unable to select X11 touch events\n");
        XCloseDisplay(state.display);
        state.display = nullptr;
        return;
    }
    XFlush(state.display);
    state.thread = std::thread(x11EventLoop, std::ref(state));
}
#endif

#if defined(EUI_LINUX_TOUCH_WAYLAND)
struct WaylandContact {
    WindowState* window;
    double x;
    double y;
};

struct WaylandSeat {
    wl_seat* seat = nullptr;
    wl_pointer* pointer = nullptr;
    wl_touch* touch = nullptr;
    zwp_pointer_gesture_pinch_v1* pinch = nullptr;
    zwp_pointer_gesture_swipe_v1* swipe = nullptr;
    std::unordered_map<std::int32_t, WaylandContact> contacts;
    WindowState* pointerWindow = nullptr;
    WindowState* gestureWindow = nullptr;
    double pointerX = 0, pointerY = 0, gestureX = 0, gestureY = 0, gestureScale = 1;
    unsigned int fingers = 0;
    std::uint32_t name = 0;
};

struct WaylandState {
    wl_display* display = nullptr;
    wl_registry* registry = nullptr;
    zwp_pointer_gestures_v1* gestures = nullptr;
    std::uint32_t gesturesName = 0;
    std::vector<std::unique_ptr<WaylandSeat>> seats;
    std::unordered_map<wl_surface*, WindowState*> surfaces;
};

WaylandState& wayland() {
    static WaylandState value;
    return value;
}

WindowState* findSurface(wl_surface* surface) {
    auto it = wayland().surfaces.find(surface);
    return it == wayland().surfaces.end() ? nullptr : it->second;
}

void touchDown(void* data, wl_touch* touch, std::uint32_t, std::uint32_t,
               wl_surface* surface, std::int32_t id, wl_fixed_t x, wl_fixed_t y) {
    auto& seat = *static_cast<WaylandSeat*>(data);
    WindowState* state = findSurface(surface);
    if (!state) return;
    const double px = wl_fixed_to_double(x), py = wl_fixed_to_double(y);
    seat.contacts.insert_or_assign(id, WaylandContact{state, px, py});
    enqueue(*state, {Kind::Down, reinterpret_cast<std::intptr_t>(touch), id, px, py});
}

void touchUp(void* data, wl_touch* touch, std::uint32_t, std::uint32_t, std::int32_t id) {
    auto& seat = *static_cast<WaylandSeat*>(data);
    auto it = seat.contacts.find(id);
    if (it == seat.contacts.end()) return;
    enqueue(*it->second.window,
            {Kind::Up, reinterpret_cast<std::intptr_t>(touch), id, it->second.x, it->second.y});
    seat.contacts.erase(it);
}

void touchMotion(void* data, wl_touch* touch, std::uint32_t, std::int32_t id,
                 wl_fixed_t x, wl_fixed_t y) {
    auto& seat = *static_cast<WaylandSeat*>(data);
    auto it = seat.contacts.find(id);
    if (it == seat.contacts.end()) return;
    it->second.x = wl_fixed_to_double(x);
    it->second.y = wl_fixed_to_double(y);
    enqueue(*it->second.window,
            {Kind::Move, reinterpret_cast<std::intptr_t>(touch), id, it->second.x, it->second.y});
}

void touchFrame(void*, wl_touch*) {}
void touchCancel(void* data, wl_touch*) {
    auto& seat = *static_cast<WaylandSeat*>(data);
    std::vector<WindowState*> canceled;
    for (const auto& entry : seat.contacts) {
        WindowState* window = entry.second.window;
        if (std::find(canceled.begin(), canceled.end(), window) == canceled.end()) {
            canceled.push_back(window);
            enqueue(*window, {Kind::Cancel});
        }
    }
    seat.contacts.clear();
}
void touchShape(void*, wl_touch*, std::int32_t, wl_fixed_t, wl_fixed_t) {}
void touchOrientation(void*, wl_touch*, std::int32_t, wl_fixed_t) {}
const wl_touch_listener touchListener{touchDown, touchUp, touchMotion, touchFrame,
                                      touchCancel, touchShape, touchOrientation};

void pointerEnter(void* data, wl_pointer*, std::uint32_t, wl_surface* surface,
                  wl_fixed_t x, wl_fixed_t y) {
    auto& seat = *static_cast<WaylandSeat*>(data);
    seat.pointerWindow = findSurface(surface);
    seat.pointerX = wl_fixed_to_double(x);
    seat.pointerY = wl_fixed_to_double(y);
}
void pointerLeave(void* data, wl_pointer*, std::uint32_t, wl_surface*) {
    static_cast<WaylandSeat*>(data)->pointerWindow = nullptr;
}
void pointerMotion(void* data, wl_pointer*, std::uint32_t, wl_fixed_t x, wl_fixed_t y) {
    auto& seat = *static_cast<WaylandSeat*>(data);
    seat.pointerX = wl_fixed_to_double(x);
    seat.pointerY = wl_fixed_to_double(y);
}
void pointerButton(void*, wl_pointer*, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t) {}
void pointerAxis(void*, wl_pointer*, std::uint32_t, std::uint32_t, wl_fixed_t) {}
const wl_pointer_listener pointerListener{pointerEnter, pointerLeave, pointerMotion,
                                          pointerButton, pointerAxis};

void gestureBegin(WaylandSeat& seat, wl_surface* surface, std::uint32_t fingers) {
    seat.gestureWindow = findSurface(surface);
    if (seat.gestureWindow && seat.pointerWindow != seat.gestureWindow) {
        glfwGetCursorPos(seat.gestureWindow->window, &seat.pointerX, &seat.pointerY);
    }
    seat.fingers = fingers;
    seat.gestureScale = 1.0;
    seat.gestureX = seat.pointerX;
    seat.gestureY = seat.pointerY;
    if (seat.gestureWindow) {
        Event event{Kind::GestureBegin};
        event.x = seat.gestureX;
        event.y = seat.gestureY;
        event.fingers = fingers;
        enqueue(*seat.gestureWindow, event);
    }
}

void gestureUpdate(WaylandSeat& seat, wl_fixed_t dx, wl_fixed_t dy,
                   double scale, double rotation) {
    if (!seat.gestureWindow) return;
    Event event{Kind::GestureUpdate};
    event.dx = wl_fixed_to_double(dx);
    event.dy = wl_fixed_to_double(dy);
    seat.gestureX += event.dx;
    seat.gestureY += event.dy;
    event.x = seat.gestureX;
    event.y = seat.gestureY;
    event.scale = scale;
    event.rotation = rotation;
    event.fingers = seat.fingers;
    enqueue(*seat.gestureWindow, event);
}

void gestureEnd(WaylandSeat& seat, bool canceled) {
    if (seat.gestureWindow) {
        Event event{canceled ? Kind::GestureCancel : Kind::GestureEnd};
        event.x = seat.gestureX;
        event.y = seat.gestureY;
        event.fingers = seat.fingers;
        enqueue(*seat.gestureWindow, event);
    }
    seat.gestureWindow = nullptr;
}

void pinchBegin(void* data, zwp_pointer_gesture_pinch_v1*, std::uint32_t,
                std::uint32_t, wl_surface* surface, std::uint32_t fingers) {
    gestureBegin(*static_cast<WaylandSeat*>(data), surface, fingers);
}
void pinchUpdate(void* data, zwp_pointer_gesture_pinch_v1*, std::uint32_t,
                 wl_fixed_t dx, wl_fixed_t dy, wl_fixed_t scale, wl_fixed_t rotation) {
    auto& seat = *static_cast<WaylandSeat*>(data);
    const double absoluteScale = wl_fixed_to_double(scale);
    const double delta = absoluteScale > 0.0 && seat.gestureScale > 0.0
        ? absoluteScale / seat.gestureScale : 1.0;
    if (absoluteScale > 0.0) seat.gestureScale = absoluteScale;
    gestureUpdate(seat, dx, dy, delta, wl_fixed_to_double(rotation));
}
void pinchEnd(void* data, zwp_pointer_gesture_pinch_v1*, std::uint32_t,
              std::uint32_t, std::int32_t canceled) {
    gestureEnd(*static_cast<WaylandSeat*>(data), canceled != 0);
}
const zwp_pointer_gesture_pinch_v1_listener pinchListener{pinchBegin, pinchUpdate, pinchEnd};

void swipeBegin(void* data, zwp_pointer_gesture_swipe_v1*, std::uint32_t,
                std::uint32_t, wl_surface* surface, std::uint32_t fingers) {
    gestureBegin(*static_cast<WaylandSeat*>(data), surface, fingers);
}
void swipeUpdate(void* data, zwp_pointer_gesture_swipe_v1*, std::uint32_t,
                 wl_fixed_t dx, wl_fixed_t dy) {
    gestureUpdate(*static_cast<WaylandSeat*>(data), dx, dy, 1.0, 0.0);
}
void swipeEnd(void* data, zwp_pointer_gesture_swipe_v1*, std::uint32_t,
              std::uint32_t, std::int32_t canceled) {
    gestureEnd(*static_cast<WaylandSeat*>(data), canceled != 0);
}
const zwp_pointer_gesture_swipe_v1_listener swipeListener{swipeBegin, swipeUpdate, swipeEnd};

void setupGestures(WaylandSeat& seat) {
    if (!wayland().gestures || !seat.pointer || seat.pinch) return;
    seat.pinch = zwp_pointer_gestures_v1_get_pinch_gesture(wayland().gestures, seat.pointer);
    seat.swipe = zwp_pointer_gestures_v1_get_swipe_gesture(wayland().gestures, seat.pointer);
    if (!seat.pinch || !seat.swipe) {
        std::fprintf(stderr, "EUI: unable to create Wayland touchpad gestures\n");
        if (seat.pinch) zwp_pointer_gesture_pinch_v1_destroy(seat.pinch);
        if (seat.swipe) zwp_pointer_gesture_swipe_v1_destroy(seat.swipe);
        seat.pinch = nullptr;
        seat.swipe = nullptr;
        return;
    }
    zwp_pointer_gesture_pinch_v1_add_listener(seat.pinch, &pinchListener, &seat);
    zwp_pointer_gesture_swipe_v1_add_listener(seat.swipe, &swipeListener, &seat);
}

void releaseTouch(wl_touch* touch) {
    if (wl_touch_get_version(touch) >= WL_TOUCH_RELEASE_SINCE_VERSION) wl_touch_release(touch);
    else wl_touch_destroy(touch);
}

void releasePointer(wl_pointer* pointer) {
    if (wl_pointer_get_version(pointer) >= WL_POINTER_RELEASE_SINCE_VERSION) wl_pointer_release(pointer);
    else wl_pointer_destroy(pointer);
}

void seatCapabilities(void* data, wl_seat* nativeSeat, std::uint32_t capabilities) {
    auto& seat = *static_cast<WaylandSeat*>(data);
    if ((capabilities & WL_SEAT_CAPABILITY_TOUCH) && !seat.touch) {
        seat.touch = wl_seat_get_touch(nativeSeat);
        if (seat.touch) wl_touch_add_listener(seat.touch, &touchListener, &seat);
        else std::fprintf(stderr, "EUI: unable to create Wayland touch input\n");
    } else if (!(capabilities & WL_SEAT_CAPABILITY_TOUCH) && seat.touch) {
        touchCancel(&seat, seat.touch);
        releaseTouch(seat.touch);
        seat.touch = nullptr;
    }
    if ((capabilities & WL_SEAT_CAPABILITY_POINTER) && !seat.pointer) {
        seat.pointer = wl_seat_get_pointer(nativeSeat);
        if (seat.pointer) {
            wl_pointer_add_listener(seat.pointer, &pointerListener, &seat);
            setupGestures(seat);
        } else {
            std::fprintf(stderr, "EUI: unable to create Wayland pointer input\n");
        }
    } else if (!(capabilities & WL_SEAT_CAPABILITY_POINTER) && seat.pointer) {
        gestureEnd(seat, true);
        if (seat.pinch) zwp_pointer_gesture_pinch_v1_destroy(seat.pinch);
        if (seat.swipe) zwp_pointer_gesture_swipe_v1_destroy(seat.swipe);
        seat.pinch = nullptr;
        seat.swipe = nullptr;
        releasePointer(seat.pointer);
        seat.pointer = nullptr;
        seat.pointerWindow = nullptr;
    }
}
void seatName(void*, wl_seat*, const char*) {}
const wl_seat_listener seatListener{seatCapabilities, seatName};

void destroySeat(WaylandSeat& seat) {
    if (seat.touch) {
        touchCancel(&seat, seat.touch);
        releaseTouch(seat.touch);
    }
    gestureEnd(seat, true);
    if (seat.pinch) zwp_pointer_gesture_pinch_v1_destroy(seat.pinch);
    if (seat.swipe) zwp_pointer_gesture_swipe_v1_destroy(seat.swipe);
    if (seat.pointer) releasePointer(seat.pointer);
    if (seat.seat) wl_seat_destroy(seat.seat);
}

void registryGlobal(void*, wl_registry* registry, std::uint32_t name,
                    const char* interface, std::uint32_t version) {
    auto& state = wayland();
    if (std::strcmp(interface, wl_seat_interface.name) == 0) {
        auto seat = std::make_unique<WaylandSeat>();
        seat->name = name;
        seat->seat = static_cast<wl_seat*>(wl_registry_bind(
            registry, name, &wl_seat_interface, std::min(version, 4u)));
        if (!seat->seat) {
            std::fprintf(stderr, "EUI: unable to bind Wayland input seat\n");
            return;
        }
        wl_seat_add_listener(seat->seat, &seatListener, seat.get());
        state.seats.push_back(std::move(seat));
    } else if (std::strcmp(interface, zwp_pointer_gestures_v1_interface.name) == 0 && !state.gestures) {
        state.gesturesName = name;
        state.gestures = static_cast<zwp_pointer_gestures_v1*>(
            wl_registry_bind(registry, name, &zwp_pointer_gestures_v1_interface, 1));
        if (!state.gestures) {
            std::fprintf(stderr, "EUI: unable to bind Wayland touchpad gestures\n");
            return;
        }
        for (auto& seat : state.seats) setupGestures(*seat);
    }
}

void registryRemove(void*, wl_registry*, std::uint32_t name) {
    auto& state = wayland();
    for (auto it = state.seats.begin(); it != state.seats.end(); ++it) {
        if ((*it)->name == name) {
            destroySeat(**it);
            state.seats.erase(it);
            return;
        }
    }
    if (state.gestures && state.gesturesName == name) {
        for (auto& seat : state.seats) {
            gestureEnd(*seat, true);
            if (seat->pinch) zwp_pointer_gesture_pinch_v1_destroy(seat->pinch);
            if (seat->swipe) zwp_pointer_gesture_swipe_v1_destroy(seat->swipe);
            seat->pinch = nullptr;
            seat->swipe = nullptr;
        }
        zwp_pointer_gestures_v1_destroy(state.gestures);
        state.gestures = nullptr;
    }
}
const wl_registry_listener registryListener{registryGlobal, registryRemove};

void installWayland(WindowState& window) {
    auto& state = wayland();
    wl_surface* surface = glfwGetWaylandWindow(window.window);
    if (!surface) return;
    state.surfaces.emplace(surface, &window);
    if (state.registry) return;
    state.display = glfwGetWaylandDisplay();
    if (!state.display) {
        std::fprintf(stderr, "EUI: unable to access Wayland display for touch input\n");
        return;
    }
    state.registry = wl_display_get_registry(state.display);
    if (!state.registry) {
        std::fprintf(stderr, "EUI: unable to create Wayland input registry\n");
        return;
    }
    wl_registry_add_listener(state.registry, &registryListener, nullptr);
    if (wl_display_roundtrip(state.display) < 0 || wl_display_roundtrip(state.display) < 0) {
        std::fprintf(stderr, "EUI: unable to initialize Wayland touch input\n");
    } else if (!state.gestures) {
        std::fprintf(stderr, "EUI: Wayland compositor does not provide touchpad gesture events\n");
    }
}

void uninstallWayland(WindowState& window) {
    auto& state = wayland();
    for (auto& seat : state.seats) {
        for (auto it = seat->contacts.begin(); it != seat->contacts.end();) {
            if (it->second.window == &window) it = seat->contacts.erase(it);
            else ++it;
        }
        if (seat->pointerWindow == &window) seat->pointerWindow = nullptr;
        if (seat->gestureWindow == &window) seat->gestureWindow = nullptr;
    }
    for (auto it = state.surfaces.begin(); it != state.surfaces.end(); ++it) {
        if (it->second == &window) {
            state.surfaces.erase(it);
            break;
        }
    }
    if (state.surfaces.empty() && state.registry) {
        for (auto& seat : state.seats) destroySeat(*seat);
        state.seats.clear();
        if (state.gestures) zwp_pointer_gestures_v1_destroy(state.gestures);
        wl_registry_destroy(state.registry);
        state.gestures = nullptr;
        state.registry = nullptr;
        state.display = nullptr;
    }
}
#endif

} // namespace

void installLinuxTouch(GLFWwindow* window) {
    if (!window || windows().count(window)) return;
    auto state = std::make_unique<WindowState>(window);
#if defined(GLFW_PLATFORM_X11) && defined(EUI_LINUX_TOUCH_X11)
    if (glfwGetPlatform() == GLFW_PLATFORM_X11) installX11(*state);
#elif !defined(GLFW_PLATFORM_X11) && defined(EUI_LINUX_TOUCH_X11)
    installX11(*state);
#endif
#if defined(GLFW_PLATFORM_WAYLAND) && defined(EUI_LINUX_TOUCH_WAYLAND)
    if (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND) installWayland(*state);
#endif
    windows().emplace(window, std::move(state));
}

void pollLinuxTouch(GLFWwindow* window, float pointerScale) {
    auto it = windows().find(window);
    if (it == windows().end()) return;
    WindowState& state = *it->second;
    std::deque<Event> events;
    {
        std::lock_guard<std::mutex> lock(state.mutex);
        events.swap(state.events);
    }
    for (const Event& event : events) {
        switch (event.kind) {
        case Kind::Down:
            if (state.nativeGestureActive) {
                core::cancelGestureInput(window);
                state.nativeGestureActive = false;
            }
            state.tracker.down(event.device, event.id, event.x, event.y);
            break;
        case Kind::Move: state.tracker.move(event.device, event.id, event.x, event.y, pointerScale); break;
        case Kind::Up: state.tracker.up(event.device, event.id, event.x, event.y); break;
        case Kind::Cancel:
            state.tracker.reset();
            state.nativeGestureActive = false;
            core::cancelTouchPointerInput(window);
            core::clearTouchScrollInput(window);
            core::cancelGestureInput(window);
            break;
        default: {
            if (event.kind == Kind::GestureBegin) {
                state.tracker.reset();
                core::cancelTouchPointerInput(window);
                core::clearTouchScrollInput(window);
                core::cancelGestureInput(window);
                state.nativeGestureActive = true;
            } else if (!state.nativeGestureActive) {
                // 失焦后到达的旧 Update/End 不能重新激活已经取消的手势。
                break;
            }
            GestureEvent gesture;
            switch (event.kind) {
            case Kind::GestureBegin: gesture.phase = GesturePhase::Begin; break;
            case Kind::GestureUpdate: gesture.phase = GesturePhase::Update; break;
            case Kind::GestureEnd: gesture.phase = GesturePhase::End; break;
            case Kind::GestureCancel: gesture.phase = GesturePhase::Cancel; break;
            default: break;
            }
            gesture.x = event.x;
            gesture.y = event.y;
            gesture.deltaX = event.dx;
            gesture.deltaY = event.dy;
            gesture.scale = event.scale;
            gesture.rotation = event.rotation;
            gesture.touchCount = event.fingers;
            queueGestureInput(window, gesture);
            if (event.kind == Kind::GestureEnd || event.kind == Kind::GestureCancel) {
                state.nativeGestureActive = false;
            }
            break;
        }
        }
    }
}

void resetLinuxTouch(GLFWwindow* window) {
    auto it = windows().find(window);
    if (it == windows().end()) return;
    it->second->tracker.reset();
    it->second->nativeGestureActive = false;
#if defined(GLFW_PLATFORM_WAYLAND) && defined(EUI_LINUX_TOUCH_WAYLAND)
    if (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND) {
        for (auto& seat : wayland().seats) {
            for (auto contact = seat->contacts.begin(); contact != seat->contacts.end();) {
                if (contact->second.window == it->second.get()) contact = seat->contacts.erase(contact);
                else ++contact;
            }
            if (seat->gestureWindow == it->second.get()) seat->gestureWindow = nullptr;
        }
    }
#endif
    std::lock_guard<std::mutex> lock(it->second->mutex);
    it->second->events.clear();
}

void uninstallLinuxTouch(GLFWwindow* window) {
    auto it = windows().find(window);
    if (it == windows().end()) return;
    WindowState& state = *it->second;
#if defined(GLFW_PLATFORM_WAYLAND) && defined(EUI_LINUX_TOUCH_WAYLAND)
    if (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND) uninstallWayland(state);
#endif
#if defined(EUI_LINUX_TOUCH_X11)
    state.stop.store(true);
    if (state.thread.joinable()) state.thread.join();
    if (state.display) XCloseDisplay(state.display);
#endif
    windows().erase(it);
}

#if defined(EUI_LINUX_TOUCH_TESTING)
bool linuxTouchReady(GLFWwindow* window) {
    auto it = windows().find(window);
    if (it == windows().end()) return false;
#if defined(GLFW_PLATFORM_X11) && defined(EUI_LINUX_TOUCH_X11)
    if (glfwGetPlatform() == GLFW_PLATFORM_X11) {
        return it->second->display && it->second->thread.joinable();
    }
#endif
#if defined(GLFW_PLATFORM_WAYLAND) && defined(EUI_LINUX_TOUCH_WAYLAND)
    if (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND) {
        return wayland().registry && findSurface(glfwGetWaylandWindow(window)) == it->second.get();
    }
#endif
    return false;
}

bool injectLinuxTouchTestEvents(GLFWwindow* window) {
    auto it = windows().find(window);
    if (it == windows().end()) return false;
#if defined(GLFW_PLATFORM_WAYLAND) && defined(EUI_LINUX_TOUCH_WAYLAND)
    if (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND) {
        WaylandSeat seat;
        auto* touch = reinterpret_cast<wl_touch*>(static_cast<std::uintptr_t>(1));
        wl_surface* surface = glfwGetWaylandWindow(window);
        touchDown(&seat, touch, 0, 0, surface, 1, wl_fixed_from_int(10), wl_fixed_from_int(20));
        touchDown(&seat, touch, 0, 0, surface, 2, wl_fixed_from_int(110), wl_fixed_from_int(20));
        touchMotion(&seat, touch, 0, 2, wl_fixed_from_int(60), wl_fixed_from_int(70));
        touchUp(&seat, touch, 0, 0, 2);
        touchUp(&seat, touch, 0, 0, 1);
        seat.pointerX = 25.0;
        seat.pointerY = 30.0;
        pinchBegin(&seat, nullptr, 0, 0, surface, 2);
        pinchUpdate(&seat, nullptr, 0, wl_fixed_from_int(4), wl_fixed_from_int(5),
                    wl_fixed_from_double(1.5), wl_fixed_from_int(10));
        pinchUpdate(&seat, nullptr, 0, wl_fixed_from_int(0), wl_fixed_from_int(0),
                    wl_fixed_from_double(3.0), wl_fixed_from_int(-5));
        pinchEnd(&seat, nullptr, 0, 0, 1);
        swipeBegin(&seat, nullptr, 0, 0, surface, 3);
        swipeUpdate(&seat, nullptr, 0, wl_fixed_from_int(-3), wl_fixed_from_int(7));
        swipeEnd(&seat, nullptr, 0, 0, 0);
        enqueue(*it->second, {Kind::GestureUpdate});
        enqueue(*it->second, {Kind::GestureEnd});
        return true;
    }
#endif
#if defined(GLFW_PLATFORM_X11) && defined(EUI_LINUX_TOUCH_X11)
    if (glfwGetPlatform() == GLFW_PLATFORM_X11) {
        enqueue(*it->second, {Kind::Down, 1, 1, 10.0, 20.0});
        enqueue(*it->second, {Kind::Down, 1, 2, 110.0, 20.0});
        enqueue(*it->second, {Kind::Move, 1, 2, 60.0, 70.0});
        enqueue(*it->second, {Kind::Up, 1, 2, 60.0, 70.0});
        enqueue(*it->second, {Kind::Up, 1, 1, 10.0, 20.0});
#if defined(XI_GesturePinchBegin)
        std::unordered_map<int, X11GestureState> active;
        XIGesturePinchEvent native{};
        native.sourceid = 1;
        native.detail = 2;
        native.event_x = 25.0;
        native.event_y = 30.0;
        Event converted{Kind::GestureBegin};
        native.evtype = XI_GesturePinchBegin;
        if (!translateX11Pinch(native, active, converted)) return false;
        enqueue(*it->second, converted);
        native.evtype = XI_GesturePinchUpdate;
        native.delta_x = 4.0;
        native.delta_y = 5.0;
        native.scale = 1.5;
        native.delta_angle = 10.0;
        if (!translateX11Pinch(native, active, converted)) return false;
        enqueue(*it->second, converted);
        native.delta_x = 0.0;
        native.delta_y = 0.0;
        native.scale = 3.0;
        native.delta_angle = -5.0;
        if (!translateX11Pinch(native, active, converted)) return false;
        enqueue(*it->second, converted);
        native.evtype = XI_GesturePinchEnd;
        native.flags = XIGesturePinchEventCancelled;
        if (!translateX11Pinch(native, active, converted)) return false;
        enqueue(*it->second, converted);
        XIGestureSwipeEvent swipe{};
        swipe.sourceid = 1;
        swipe.detail = 3;
        swipe.event_x = 25.0;
        swipe.event_y = 30.0;
        swipe.evtype = XI_GestureSwipeBegin;
        if (!translateX11Swipe(swipe, active, converted)) return false;
        enqueue(*it->second, converted);
        swipe.evtype = XI_GestureSwipeUpdate;
        swipe.delta_x = -3.0;
        swipe.delta_y = 7.0;
        if (!translateX11Swipe(swipe, active, converted)) return false;
        enqueue(*it->second, converted);
        swipe.evtype = XI_GestureSwipeEnd;
        swipe.delta_x = 0.0;
        swipe.delta_y = 0.0;
        if (!translateX11Swipe(swipe, active, converted)) return false;
        enqueue(*it->second, converted);
#endif
        enqueue(*it->second, {Kind::GestureUpdate});
        enqueue(*it->second, {Kind::GestureEnd});
        return true;
    }
#endif
    return false;
}
#endif

} // namespace core::platform
