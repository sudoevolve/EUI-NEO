#include "core/window/window_backend.h"
#include "core/platform/native_bridge.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#if defined(EUI_WINDOW_BACKEND_SDL2)

#include <SDL.h>
#if defined(__linux__) && !defined(__ANDROID__) && defined(SDL_VIDEO_DRIVER_X11)
#include <SDL_syswm.h>
#include <X11/Xresource.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#endif
#ifdef None
#undef None
#endif
#ifdef Bool
#undef Bool
#endif
#ifdef Status
#undef Status
#endif
#ifdef CursorShape
#undef CursorShape
#endif
#ifdef Success
#undef Success
#endif
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <imm.h>

#include <new>
#include <unordered_map>
#endif
#if defined(_WIN32) || defined(__APPLE__)
#include <SDL_syswm.h>
#endif
namespace core::window {

namespace {

void configureOpenGLWindowAttributes() {
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
}

#if defined(_WIN32)

struct SdlImeFilterState {
    WNDPROC previousProc = nullptr;
    SDL_Rect rect{};
    bool hasRect = false;
    bool applying = false;
    UINT_PTR reapplyTimer = 0;
};

std::unordered_map<HWND, SdlImeFilterState*> gSdlImeFilters;

SdlImeFilterState* sdlImeState(HWND hwnd) {
    const auto iterator = gSdlImeFilters.find(hwnd);
    return iterator != gSdlImeFilters.end() ? iterator->second : nullptr;
}

HWND hwndForSdlWindow(SDL_Window* window) {
    if (window == nullptr) {
        return nullptr;
    }

    SDL_SysWMinfo info{};
    SDL_VERSION(&info.version);
    if (SDL_GetWindowWMInfo(window, &info) != SDL_TRUE ||
        info.subsystem != SDL_SYSWM_WINDOWS) {
        return nullptr;
    }
    return info.info.win.window;
}

void applySdlImeRect(HWND hwnd, const SDL_Rect& rect) {
    HIMC context = ImmGetContext(hwnd);
    if (context == nullptr) {
        return;
    }

    LOGFONTW font{};
    const HFONT defaultFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    if (defaultFont != nullptr &&
        GetObjectW(defaultFont, sizeof(font), &font) == sizeof(font)) {
        font.lfHeight = -std::max(12, rect.h);
        font.lfQuality = CLEARTYPE_QUALITY;
        ImmSetCompositionFontW(context, &font);
    }

    COMPOSITIONFORM composition{};
    composition.dwStyle = CFS_FORCE_POSITION;
    composition.ptCurrentPos.x = rect.x;
    composition.ptCurrentPos.y = rect.y;
    composition.rcArea.left = rect.x;
    composition.rcArea.top = rect.y;
    composition.rcArea.right = rect.x + rect.w;
    composition.rcArea.bottom = rect.y + rect.h;
    ImmSetCompositionWindow(context, &composition);

    CANDIDATEFORM candidate{};
    candidate.dwIndex = 0;
    candidate.dwStyle = CFS_EXCLUDE;
    candidate.ptCurrentPos = composition.ptCurrentPos;
    candidate.rcArea = composition.rcArea;
    ImmSetCandidateWindow(context, &candidate);

    ImmReleaseContext(hwnd, context);
}

void reapplySdlImeRect(HWND hwnd, SdlImeFilterState* state) {
    if (state == nullptr || !state->hasRect || state->applying) {
        return;
    }

    state->applying = true;
    applySdlImeRect(hwnd, state->rect);
    if (sdlImeState(hwnd) == state) {
        state->applying = false;
    }
}

LRESULT CALLBACK sdlImeWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    auto* state = sdlImeState(hwnd);
    if (message == WM_TIMER && state != nullptr &&
        state->reapplyTimer != 0 && wParam == state->reapplyTimer) {
        KillTimer(hwnd, state->reapplyTimer);
        state->reapplyTimer = 0;
        reapplySdlImeRect(hwnd, state);
        return 0;
    }

    const bool placementChanged = message == WM_IME_STARTCOMPOSITION ||
        message == WM_IME_COMPOSITION ||
        (message == WM_IME_NOTIFY &&
         (wParam == IMN_OPENCANDIDATE || wParam == IMN_CHANGECANDIDATE));

    if (placementChanged) {
        reapplySdlImeRect(hwnd, state);
    }

    const LRESULT result = state != nullptr && state->previousProc != nullptr
        ? CallWindowProcW(state->previousProc, hwnd, message, wParam, lParam)
        : DefWindowProcW(hwnd, message, wParam, lParam);

    if (placementChanged) {
        state = sdlImeState(hwnd);
        reapplySdlImeRect(hwnd, state);
        if (state != nullptr) {
            if (state->reapplyTimer != 0) {
                KillTimer(hwnd, state->reapplyTimer);
            }
            state->reapplyTimer = SetTimer(hwnd, 0, USER_TIMER_MINIMUM, nullptr);
        }
    }
    return result;
}

void installSdlImeFilter(SDL_Window* window) {
    HWND hwnd = hwndForSdlWindow(window);
    if (hwnd == nullptr || sdlImeState(hwnd) != nullptr) {
        return;
    }

    auto* state = new (std::nothrow) SdlImeFilterState{};
    if (state == nullptr) {
        return;
    }
    state->previousProc = reinterpret_cast<WNDPROC>(GetWindowLongPtrW(hwnd, GWLP_WNDPROC));
    if (state->previousProc == nullptr) {
        delete state;
        return;
    }

    gSdlImeFilters.emplace(hwnd, state);
    SetLastError(ERROR_SUCCESS);
    const LONG_PTR previous = SetWindowLongPtrW(
        hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(sdlImeWindowProc));
    if (previous == 0 && GetLastError() != ERROR_SUCCESS) {
        gSdlImeFilters.erase(hwnd);
        delete state;
    }
}

void uninstallSdlImeFilter(SDL_Window* window) {
    HWND hwnd = hwndForSdlWindow(window);
    if (hwnd == nullptr) {
        return;
    }

    auto* state = sdlImeState(hwnd);
    if (state == nullptr) {
        return;
    }

    if (state->reapplyTimer != 0) {
        KillTimer(hwnd, state->reapplyTimer);
    }
    SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(state->previousProc));
    gSdlImeFilters.erase(hwnd);
    delete state;
}

#endif
#if defined(__linux__) && !defined(__ANDROID__) && defined(SDL_VIDEO_DRIVER_X11)
struct X11ResourceApi {
    using Initialize = void (*)();
    using ResourceManagerString = char* (*)(Display*);
    using GetStringDatabase = XrmDatabase (*)(const char*);
    using GetResource = int (*)(XrmDatabase, const char*, const char*, char**, XrmValue*);
    using DestroyDatabase = void (*)(XrmDatabase);

    void* library = nullptr;
    Initialize initialize = nullptr;
    ResourceManagerString resourceManagerString = nullptr;
    GetStringDatabase getStringDatabase = nullptr;
    GetResource getResource = nullptr;
    DestroyDatabase destroyDatabase = nullptr;

    bool available() const {
        return library != nullptr && initialize != nullptr &&
               resourceManagerString != nullptr && getStringDatabase != nullptr &&
               getResource != nullptr && destroyDatabase != nullptr;
    }
};

X11ResourceApi loadX11ResourceApi() {
    X11ResourceApi api;
    void* library = SDL_LoadObject("libX11.so.6");
    if (library == nullptr) {
        library = SDL_LoadObject("libX11.so");
    }
    if (library == nullptr) {
        return api;
    }

    api.library = library;
    api.initialize = reinterpret_cast<X11ResourceApi::Initialize>(
        SDL_LoadFunction(library, "XrmInitialize"));
    api.resourceManagerString = reinterpret_cast<X11ResourceApi::ResourceManagerString>(
        SDL_LoadFunction(library, "XResourceManagerString"));
    api.getStringDatabase = reinterpret_cast<X11ResourceApi::GetStringDatabase>(
        SDL_LoadFunction(library, "XrmGetStringDatabase"));
    api.getResource = reinterpret_cast<X11ResourceApi::GetResource>(
        SDL_LoadFunction(library, "XrmGetResource"));
    api.destroyDatabase = reinterpret_cast<X11ResourceApi::DestroyDatabase>(
        SDL_LoadFunction(library, "XrmDestroyDatabase"));
    if (!api.available()) {
        SDL_UnloadObject(library);
        return {};
    }
    return api;
}

const X11ResourceApi& x11ResourceApi() {
    static const X11ResourceApi api = loadX11ResourceApi();
    return api;
}
#endif


} // namespace


float x11ContentScale(Handle window) {
    SDL_Window* sdlWindow = static_cast<SDL_Window*>(window);
    if (sdlWindow == nullptr) {
        return 0.0f;
    }
#if defined(__linux__) && !defined(__ANDROID__) && defined(SDL_VIDEO_DRIVER_X11)
    SDL_SysWMinfo info{};
    SDL_VERSION(&info.version);
    if (SDL_GetWindowWMInfo(sdlWindow, &info) != SDL_TRUE ||
        info.subsystem != SDL_SYSWM_X11 ||
        info.info.x11.display == nullptr) {
        return 0.0f;
    }

    const X11ResourceApi& api = x11ResourceApi();
    if (!api.available()) {
        return 0.0f;
    }

    static Display* cachedDisplay = nullptr;
    static float cachedScale = 1.0f;
    if (cachedDisplay == info.info.x11.display) {
        return cachedScale;
    }
    cachedDisplay = info.info.x11.display;
    cachedScale = 1.0f;

    api.initialize();
    char* resources = api.resourceManagerString(info.info.x11.display);
    if (resources == nullptr) {
        return cachedScale;
    }
    XrmDatabase database = api.getStringDatabase(resources);
    if (database == nullptr) {
        return cachedScale;
    }

    char* type = nullptr;
    XrmValue value{};
    if (api.getResource(database, "Xft.dpi", "Xft.Dpi", &type, &value) &&
        value.addr != nullptr) {
        char* end = nullptr;
        const float dpi = std::strtof(value.addr, &end);
        if (end != value.addr && std::isfinite(dpi) && dpi > 0.0f) {
            cachedScale = dpi / 96.0f;
        }
    }
    api.destroyDatabase(database);
    return cachedScale;
#else
    (void)sdlWindow;
    return 0.0f;
#endif
}

Handle createWindow(const WindowCreateRequest& request) {
    if (request.renderApi == RenderApi::OpenGL) {
        configureOpenGLWindowAttributes();
    }

    Uint32 flags = 0;
    if (request.highDpi) {
        flags |= SDL_WINDOW_ALLOW_HIGHDPI;
    }
    if (request.resizable) {
        flags |= SDL_WINDOW_RESIZABLE;
    }
    if (!request.decorated) {
        flags |= SDL_WINDOW_BORDERLESS;
    }
    flags |= request.renderApi == RenderApi::Vulkan ? SDL_WINDOW_VULKAN : SDL_WINDOW_OPENGL;

    SDL_Window* window = SDL_CreateWindow(
        request.title != nullptr ? request.title : "",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        request.width,
        request.height,
        flags);
#if defined(__linux__) && !defined(__ANDROID__) && defined(SDL_VIDEO_DRIVER_X11)
    if (window != nullptr && request.highDpi) {
        const float scale = x11ContentScale(window);
        if (scale > 0.0f && scale != 1.0f) {
            SDL_SetWindowSize(
                window,
                static_cast<int>(std::lround(static_cast<float>(request.width) * scale)),
                static_cast<int>(std::lround(static_cast<float>(request.height) * scale)));
            SDL_SetWindowPosition(
                window,
                SDL_WINDOWPOS_CENTERED,
                SDL_WINDOWPOS_CENTERED);
        }
    }
#endif
#if defined(_WIN32)
    installSdlImeFilter(window);
#endif
    return window;
}

void destroyWindow(Handle window) {
    if (window != nullptr) {
        auto* sdlWindow = static_cast<SDL_Window*>(window);
#if defined(_WIN32)
        uninstallSdlImeFilter(sdlWindow);
#endif
        SDL_DestroyWindow(sdlWindow);
    }
}

NativeWindowInfo nativeWindowInfo(Handle window) {
    NativeWindowInfo result;
    result.handle = window;
#if defined(_WIN32) || defined(__APPLE__)
    if (window == nullptr) {
        return result;
    }
    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    if (SDL_GetWindowWMInfo(static_cast<SDL_Window*>(window), &info) != SDL_TRUE) {
        return result;
    }
#if defined(_WIN32)
    if (info.subsystem == SDL_SYSWM_WINDOWS) {
        result.platformWindow = info.info.win.window;
    }
#elif defined(__APPLE__)
    if (info.subsystem == SDL_SYSWM_COCOA) {
        result.platformWindow = info.info.cocoa.window;
    }
#endif
#endif
    return result;
}

ContextKey currentContextKey() {
    return SDL_GL_GetCurrentContext();
}

double timeSeconds() {
    const Uint64 frequency = SDL_GetPerformanceFrequency();
    return frequency > 0
        ? static_cast<double>(SDL_GetPerformanceCounter()) / static_cast<double>(frequency)
        : 0.0;
}

void postEmptyEvent() {
    SDL_Event event{};
    event.type = SDL_USEREVENT;
    SDL_PushEvent(&event);
}

std::string clipboardText(Handle) {
    char* text = SDL_GetClipboardText();
    if (text == nullptr) {
        return {};
    }
    std::string result(text);
    SDL_free(text);
    return result;
}

void setClipboardText(const std::string& text) {
    SDL_SetClipboardText(text.c_str());
}

CursorHandle createStandardCursor(CursorType type) {
    return SDL_CreateSystemCursor(type == CursorType::Hand ? SDL_SYSTEM_CURSOR_HAND : SDL_SYSTEM_CURSOR_ARROW);
}

void setCursor(Handle, CursorHandle cursor) {
    SDL_SetCursor(static_cast<SDL_Cursor*>(cursor));
}

void destroyCursor(CursorHandle cursor) {
    SDL_FreeCursor(static_cast<SDL_Cursor*>(cursor));
}

void setWindowIcon(Handle window, int width, int height, unsigned char* pixels) {
    if (window == nullptr || pixels == nullptr || width <= 0 || height <= 0) {
        return;
    }
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
        pixels, width, height, 32, width * 4, SDL_PIXELFORMAT_RGBA32);
    if (surface != nullptr) {
        SDL_SetWindowIcon(static_cast<SDL_Window*>(window), surface);
        SDL_FreeSurface(surface);
    }
    eui_set_application_icon_rgba(width, height, pixels);
}

void setImeCursorRect(Handle window, float x, float y, float width, float height) {
#if defined(_WIN32)
    SDL_Rect rect{
        static_cast<int>(x + 0.5f),
        static_cast<int>(y + 0.5f),
        static_cast<int>(width + 0.5f),
        static_cast<int>(height + 0.5f)
    };
    HWND hwnd = hwndForSdlWindow(static_cast<SDL_Window*>(window));
    if (hwnd != nullptr) {
        auto* state = sdlImeState(hwnd);
        if (state != nullptr) {
            state->rect = rect;
            state->hasRect = true;
        }
    }
    SDL_SetTextInputRect(&rect);
#else
    int windowWidth = 0;
    int windowHeight = 0;
    int drawableWidth = 0;
    int drawableHeight = 0;
    SDL_GetWindowSize(static_cast<SDL_Window*>(window), &windowWidth, &windowHeight);
    SDL_GL_GetDrawableSize(static_cast<SDL_Window*>(window), &drawableWidth, &drawableHeight);
    const float scaleX = windowWidth > 0 && drawableWidth > 0
        ? static_cast<float>(drawableWidth) / static_cast<float>(windowWidth)
        : 1.0f;
    const float scaleY = windowHeight > 0 && drawableHeight > 0
        ? static_cast<float>(drawableHeight) / static_cast<float>(windowHeight)
        : 1.0f;
    SDL_Rect rect{
        static_cast<int>(x / scaleX),
        static_cast<int>(y / scaleY),
        static_cast<int>(width / scaleX),
        static_cast<int>(height / scaleY)
    };
    SDL_SetTextInputRect(&rect);
#endif
}

void minimizeWindow(Handle window) {
    if (window != nullptr) {
        SDL_MinimizeWindow(static_cast<SDL_Window*>(window));
    }
}

void maximizeWindow(Handle window) {
    if (window != nullptr) {
        SDL_MaximizeWindow(static_cast<SDL_Window*>(window));
    }
}

void restoreWindow(Handle window) {
    if (window != nullptr) {
        SDL_RestoreWindow(static_cast<SDL_Window*>(window));
    }
}

bool isWindowMaximized(Handle window) {
    if (window == nullptr) {
        return false;
    }
    return (SDL_GetWindowFlags(static_cast<SDL_Window*>(window)) & SDL_WINDOW_MAXIMIZED) != 0;
}

void requestWindowClose(Handle window) {
    if (window == nullptr) {
        return;
    }
    SDL_Event event{};
    event.type = SDL_QUIT;
    SDL_PushEvent(&event);
}

bool dragWindow(Handle window) {
    if (window == nullptr) {
        return false;
    }
#if defined(_WIN32)
    HWND hwnd = hwndForSdlWindow(static_cast<SDL_Window*>(window));
    if (hwnd == nullptr) {
        return false;
    }
    ReleaseCapture();
    SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
    return true;
#elif defined(__linux__) && !defined(__ANDROID__) && defined(SDL_VIDEO_DRIVER_X11)
    SDL_SysWMinfo info{};
    SDL_VERSION(&info.version);
    if (SDL_GetWindowWMInfo(static_cast<SDL_Window*>(window), &info) != SDL_TRUE ||
        info.subsystem != SDL_SYSWM_X11) {
        return false;
    }
    Display* display = info.info.x11.display;
    const Window xwindow = info.info.x11.window;
    if (display == nullptr || xwindow == 0) {
        return false;
    }
    // The WM grabs the pointer from the root position we report; pass the
    // cursor's current root coordinates so the window doesn't jump to (0,0).
    int rootX = 0;
    int rootY = 0;
    {
        Window rootRet = 0;
        Window childRet = 0;
        int winX = 0;
        int winY = 0;
        unsigned int mask = 0;
        if (XQueryPointer(display, xwindow, &rootRet, &childRet,
                          &rootX, &rootY, &winX, &winY, &mask) == False) {
            rootX = 0;
            rootY = 0;
        }
    }
    XEvent event{};
    event.xclient.type = ClientMessage;
    event.xclient.message_type = XInternAtom(display, "_NET_WM_MOVERESIZE", False);
    event.xclient.display = display;
    event.xclient.window = xwindow;
    event.xclient.format = 32;
    event.xclient.data.l[0] = rootX;  /* cursor root x */
    event.xclient.data.l[1] = rootY;  /* cursor root y */
    event.xclient.data.l[2] = 8;      /* _NET_WM_MOVERESIZE_MOVE */
    event.xclient.data.l[3] = Button1;
    event.xclient.data.l[4] = 0;
    XSendEvent(display, DefaultRootWindow(display), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &event);
    XFlush(display);
    return true;
#else
    return false;
#endif
}

bool startWindowResize(Handle window, ResizeEdge edge) {
    if (window == nullptr) {
        return false;
    }
#if defined(_WIN32)
    HWND hwnd = hwndForSdlWindow(static_cast<SDL_Window*>(window));
    if (hwnd == nullptr) {
        return false;
    }
    static const int hitTests[] = {
        HTTOPLEFT, HTTOP, HTTOPRIGHT, HTRIGHT,
        HTBOTTOMRIGHT, HTBOTTOM, HTBOTTOMLEFT, HTLEFT
    };
    ReleaseCapture();
    SendMessageW(hwnd, WM_NCLBUTTONDOWN, hitTests[static_cast<int>(edge)], 0);
    return true;
#elif defined(__linux__) && !defined(__ANDROID__) && defined(SDL_VIDEO_DRIVER_X11)
    SDL_SysWMinfo info{};
    SDL_VERSION(&info.version);
    if (SDL_GetWindowWMInfo(static_cast<SDL_Window*>(window), &info) != SDL_TRUE ||
        info.subsystem != SDL_SYSWM_X11) {
        return false;
    }
    Display* display = info.info.x11.display;
    const Window xwindow = info.info.x11.window;
    if (display == nullptr || xwindow == 0) {
        return false;
    }
    // ResizeEdge enum order matches _NET_WM_MOVERESIZE directions
    // (SIZE_TOPLEFT=0 .. SIZE_LEFT=7).
    const int direction = static_cast<int>(edge);
    int rootX = 0;
    int rootY = 0;
    {
        Window rootRet = 0;
        Window childRet = 0;
        int winX = 0;
        int winY = 0;
        unsigned int mask = 0;
        if (XQueryPointer(display, xwindow, &rootRet, &childRet,
                          &rootX, &rootY, &winX, &winY, &mask) == False) {
            rootX = 0;
            rootY = 0;
        }
    }
    XEvent event{};
    event.xclient.type = ClientMessage;
    event.xclient.message_type = XInternAtom(display, "_NET_WM_MOVERESIZE", False);
    event.xclient.display = display;
    event.xclient.window = xwindow;
    event.xclient.format = 32;
    event.xclient.data.l[0] = rootX;
    event.xclient.data.l[1] = rootY;
    event.xclient.data.l[2] = direction;
    event.xclient.data.l[3] = Button1;
    event.xclient.data.l[4] = 0;
    XSendEvent(display, DefaultRootWindow(display), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &event);
    XFlush(display);
    return true;
#else
    return false;
#endif
}

} // namespace core::window

#else

#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__linux__) && !defined(__ANDROID__)
#define GLFW_EXPOSE_NATIVE_X11
#elif defined(__APPLE__)
#define GLFW_EXPOSE_NATIVE_COCOA
#endif

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#include "core/platform/ime_bridge.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <GLFW/glfw3native.h>
#elif defined(__linux__) && !defined(__ANDROID__)
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <GLFW/glfw3native.h>
// X11 headers (including the ones glfw3native.h pulls in) use the Bool/Status
// macros below; undef only after they are all included to avoid polluting the
// C++ code that follows.
#ifdef None
#undef None
#endif
#ifdef Bool
#undef Bool
#endif
#ifdef Status
#undef Status
#endif
#ifdef Success
#undef Success
#endif
#endif

namespace core::window {

namespace {

void configureOpenGLWindowHints() {
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
    glfwWindowHint(GLFW_SAMPLES, 0);
    glfwWindowHint(GLFW_RED_BITS, 8);
    glfwWindowHint(GLFW_GREEN_BITS, 8);
    glfwWindowHint(GLFW_BLUE_BITS, 8);
    glfwWindowHint(GLFW_ALPHA_BITS, 8);
    glfwWindowHint(GLFW_DEPTH_BITS, 16);
    glfwWindowHint(GLFW_STENCIL_BITS, 0);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
}

} // namespace

Handle createWindow(const WindowCreateRequest& request) {
    GLFWwindow* shareContext = nullptr;
    if (request.renderApi == RenderApi::Vulkan) {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    } else {
        configureOpenGLWindowHints();
        shareContext = static_cast<GLFWwindow*>(request.parent);
    }
    glfwWindowHint(GLFW_RESIZABLE, request.resizable ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_DECORATED, request.decorated ? GLFW_TRUE : GLFW_FALSE);

    return glfwCreateWindow(
        request.width,
        request.height,
        request.title != nullptr ? request.title : "",
        nullptr,
        shareContext);
}

void destroyWindow(Handle window) {
    if (window != nullptr) {
        glfwDestroyWindow(static_cast<GLFWwindow*>(window));
    }
}

NativeWindowInfo nativeWindowInfo(Handle window) {
    NativeWindowInfo result;
    result.handle = window;
    if (window == nullptr) {
        return result;
    }
#if defined(_WIN32)
    result.platformWindow = glfwGetWin32Window(static_cast<GLFWwindow*>(window));
#elif defined(__APPLE__)
    result.platformWindow = glfwGetCocoaWindow(static_cast<GLFWwindow*>(window));
#endif
    // X11's Window is an XID (unsigned long), not a pointer, so platformWindow
    // stays null there — matching the SDL2 backend. dragWindow() reads the X11
    // window directly via glfwGetX11Window() instead.
    return result;
}

ContextKey currentContextKey() {
    return glfwGetCurrentContext();
}

double timeSeconds() {
    return glfwGetTime();
}

void postEmptyEvent() {
    glfwPostEmptyEvent();
}

std::string clipboardText(Handle window) {
    const char* text = glfwGetClipboardString(static_cast<GLFWwindow*>(window));
    return text != nullptr ? text : "";
}

void setClipboardText(const std::string& text) {
    glfwSetClipboardString(glfwGetCurrentContext(), text.c_str());
}

CursorHandle createStandardCursor(CursorType type) {
    return glfwCreateStandardCursor(type == CursorType::Hand ? GLFW_HAND_CURSOR : GLFW_ARROW_CURSOR);
}

void setCursor(Handle window, CursorHandle cursor) {
    glfwSetCursor(static_cast<GLFWwindow*>(window), static_cast<GLFWcursor*>(cursor));
}

void destroyCursor(CursorHandle cursor) {
    glfwDestroyCursor(static_cast<GLFWcursor*>(cursor));
}

void setWindowIcon(Handle window, int width, int height, unsigned char* pixels) {
    if (window == nullptr || pixels == nullptr || width <= 0 || height <= 0) {
        return;
    }
    GLFWimage image{};
    image.width = width;
    image.height = height;
    image.pixels = pixels;
    glfwSetWindowIcon(static_cast<GLFWwindow*>(window), 1, &image);
    eui_set_application_icon_rgba(width, height, pixels);
}

void setImeCursorRect(Handle window, float x, float y, float width, float height) {
    eui_ime_set_cursor_rect_with_font(static_cast<GLFWwindow*>(window), x, y, width, height, height);
}

void minimizeWindow(Handle window) {
    if (window != nullptr) {
        glfwIconifyWindow(static_cast<GLFWwindow*>(window));
    }
}

void maximizeWindow(Handle window) {
    if (window != nullptr) {
        glfwMaximizeWindow(static_cast<GLFWwindow*>(window));
    }
}

void restoreWindow(Handle window) {
    if (window != nullptr) {
        glfwRestoreWindow(static_cast<GLFWwindow*>(window));
    }
}

bool isWindowMaximized(Handle window) {
    if (window == nullptr) {
        return false;
    }
    return glfwGetWindowAttrib(static_cast<GLFWwindow*>(window), GLFW_MAXIMIZED) == GLFW_TRUE;
}

void requestWindowClose(Handle window) {
    if (window != nullptr) {
        glfwSetWindowShouldClose(static_cast<GLFWwindow*>(window), GLFW_TRUE);
    }
}

bool dragWindow(Handle window) {
    if (window == nullptr) {
        return false;
    }
#if defined(_WIN32)
    HWND hwnd = glfwGetWin32Window(static_cast<GLFWwindow*>(window));
    if (hwnd == nullptr) {
        return false;
    }
    ReleaseCapture();
    SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
    return true;
#elif defined(__linux__) && !defined(__ANDROID__)
    Display* display = glfwGetX11Display();
    const Window xwindow = glfwGetX11Window(static_cast<GLFWwindow*>(window));
    if (display == nullptr || xwindow == 0) {
        return false;
    }
    // The WM grabs the pointer from the root position we report; pass the
    // cursor's current root coordinates so the window doesn't jump to (0,0).
    int rootX = 0;
    int rootY = 0;
    {
        Window rootRet = 0;
        Window childRet = 0;
        int winX = 0;
        int winY = 0;
        unsigned int mask = 0;
        if (XQueryPointer(display, xwindow, &rootRet, &childRet,
                          &rootX, &rootY, &winX, &winY, &mask) == False) {
            rootX = 0;
            rootY = 0;
        }
    }
    XEvent event{};
    event.xclient.type = ClientMessage;
    event.xclient.message_type = XInternAtom(display, "_NET_WM_MOVERESIZE", False);
    event.xclient.display = display;
    event.xclient.window = xwindow;
    event.xclient.format = 32;
    event.xclient.data.l[0] = rootX;  /* cursor root x */
    event.xclient.data.l[1] = rootY;  /* cursor root y */
    event.xclient.data.l[2] = 8;      /* _NET_WM_MOVERESIZE_MOVE */
    event.xclient.data.l[3] = Button1;
    event.xclient.data.l[4] = 0;
    XSendEvent(display, DefaultRootWindow(display), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &event);
    XFlush(display);
    return true;
#else
    return false;
#endif
}

bool startWindowResize(Handle window, ResizeEdge edge) {
    if (window == nullptr) {
        return false;
    }
#if defined(_WIN32)
    HWND hwnd = glfwGetWin32Window(static_cast<GLFWwindow*>(window));
    if (hwnd == nullptr) {
        return false;
    }
    static const int hitTests[] = {
        HTTOPLEFT, HTTOP, HTTOPRIGHT, HTRIGHT,
        HTBOTTOMRIGHT, HTBOTTOM, HTBOTTOMLEFT, HTLEFT
    };
    ReleaseCapture();
    SendMessageW(hwnd, WM_NCLBUTTONDOWN, hitTests[static_cast<int>(edge)], 0);
    return true;
#elif defined(__linux__) && !defined(__ANDROID__)
    Display* display = glfwGetX11Display();
    const Window xwindow = glfwGetX11Window(static_cast<GLFWwindow*>(window));
    if (display == nullptr || xwindow == 0) {
        return false;
    }
    // ResizeEdge enum order matches _NET_WM_MOVERESIZE directions
    // (SIZE_TOPLEFT=0 .. SIZE_LEFT=7).
    const int direction = static_cast<int>(edge);
    int rootX = 0;
    int rootY = 0;
    {
        Window rootRet = 0;
        Window childRet = 0;
        int winX = 0;
        int winY = 0;
        unsigned int mask = 0;
        if (XQueryPointer(display, xwindow, &rootRet, &childRet,
                          &rootX, &rootY, &winX, &winY, &mask) == False) {
            rootX = 0;
            rootY = 0;
        }
    }
    XEvent event{};
    event.xclient.type = ClientMessage;
    event.xclient.message_type = XInternAtom(display, "_NET_WM_MOVERESIZE", False);
    event.xclient.display = display;
    event.xclient.window = xwindow;
    event.xclient.format = 32;
    event.xclient.data.l[0] = rootX;
    event.xclient.data.l[1] = rootY;
    event.xclient.data.l[2] = direction;
    event.xclient.data.l[3] = Button1;
    event.xclient.data.l[4] = 0;
    XSendEvent(display, DefaultRootWindow(display), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &event);
    XFlush(display);
    return true;
#else
    return false;
#endif
}

} // namespace core::window

#endif
