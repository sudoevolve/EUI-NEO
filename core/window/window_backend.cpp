#include "core/window/window_backend.h"
#include "core/platform/native_bridge.h"

#include <algorithm>

#if defined(EUI_WINDOW_BACKEND_SDL2) || defined(EUI_WINDOW_BACKEND_SDL3)

#if defined(EUI_WINDOW_BACKEND_SDL3)
#include <SDL3/SDL.h>
#else
#include <SDL.h>
#endif
#if defined(EUI_WINDOW_BACKEND_SDL2) && (defined(_WIN32) || defined(__APPLE__))
#include <SDL_syswm.h>
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

} // namespace

#if defined(_WIN32)
namespace {

LONG roundLong(float value) {
    return static_cast<LONG>(value >= 0.0f ? value + 0.5f : value - 0.5f);
}

HWND hwndForWindow(Handle window) {
    return static_cast<HWND>(nativeWindowInfo(window).platformWindow);
}

} // namespace
#endif

Handle createWindow(const WindowCreateRequest& request) {
    if (request.renderApi == RenderApi::OpenGL) {
        configureOpenGLWindowAttributes();
    }

#if defined(EUI_WINDOW_BACKEND_SDL3)
    SDL_WindowFlags flags = 0;
    if (request.highDpi) {
        flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
    }
    if (request.resizable) {
        flags |= SDL_WINDOW_RESIZABLE;
    }
    flags |= request.renderApi == RenderApi::Vulkan ? SDL_WINDOW_VULKAN : SDL_WINDOW_OPENGL;

    SDL_Window* window = SDL_CreateWindow(
        request.title != nullptr ? request.title : "",
        request.width,
        request.height,
        flags);
    if (window != nullptr) {
        SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    }
    return window;
#else
    Uint32 flags = 0;
    if (request.highDpi) {
        flags |= SDL_WINDOW_ALLOW_HIGHDPI;
    }
    if (request.resizable) {
        flags |= SDL_WINDOW_RESIZABLE;
    }
    flags |= request.renderApi == RenderApi::Vulkan ? SDL_WINDOW_VULKAN : SDL_WINDOW_OPENGL;

    return SDL_CreateWindow(
        request.title != nullptr ? request.title : "",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        request.width,
        request.height,
        flags);
#endif
}

void destroyWindow(Handle window) {
    if (window != nullptr) {
        SDL_DestroyWindow(static_cast<SDL_Window*>(window));
    }
}

NativeWindowInfo nativeWindowInfo(Handle window) {
    NativeWindowInfo result;
    result.handle = window;
#if defined(EUI_WINDOW_BACKEND_SDL3)
    if (window == nullptr) {
        return result;
    }
    SDL_PropertiesID properties = SDL_GetWindowProperties(static_cast<SDL_Window*>(window));
    if (properties == 0) {
        return result;
    }
#if defined(_WIN32)
    result.platformWindow = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
#elif defined(__APPLE__)
    result.platformWindow = SDL_GetPointerProperty(properties, SDL_PROP_WINDOW_COCOA_WINDOW_POINTER, nullptr);
#endif
#else
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
#if defined(EUI_WINDOW_BACKEND_SDL3)
    event.type = SDL_EVENT_USER;
#else
    event.type = SDL_USEREVENT;
#endif
    SDL_PushEvent(&event);
}

void getCursorPosition(Handle, double& x, double& y) {
#if defined(EUI_WINDOW_BACKEND_SDL3)
    float cursorX = 0.0f;
    float cursorY = 0.0f;
#else
    int cursorX = 0;
    int cursorY = 0;
#endif
    SDL_GetMouseState(&cursorX, &cursorY);
    x = static_cast<double>(cursorX);
    y = static_cast<double>(cursorY);
}

bool isMouseButtonDown(Handle, int button) {
#if defined(EUI_WINDOW_BACKEND_SDL3)
    const SDL_MouseButtonFlags state = SDL_GetMouseState(nullptr, nullptr);
    const SDL_MouseButtonFlags mask = button == 1 ? SDL_BUTTON_RMASK : SDL_BUTTON_LMASK;
#else
    const Uint32 state = SDL_GetMouseState(nullptr, nullptr);
    const Uint32 mask = button == 1 ? SDL_BUTTON_RMASK : SDL_BUTTON_LMASK;
#endif
    return (state & mask) != 0;
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
#if defined(EUI_WINDOW_BACKEND_SDL3)
    return SDL_CreateSystemCursor(type == CursorType::Hand ? SDL_SYSTEM_CURSOR_POINTER : SDL_SYSTEM_CURSOR_DEFAULT);
#else
    return SDL_CreateSystemCursor(type == CursorType::Hand ? SDL_SYSTEM_CURSOR_HAND : SDL_SYSTEM_CURSOR_ARROW);
#endif
}

void setCursor(Handle, CursorHandle cursor) {
    SDL_SetCursor(static_cast<SDL_Cursor*>(cursor));
}

void destroyCursor(CursorHandle cursor) {
#if defined(EUI_WINDOW_BACKEND_SDL3)
    SDL_DestroyCursor(static_cast<SDL_Cursor*>(cursor));
#else
    SDL_FreeCursor(static_cast<SDL_Cursor*>(cursor));
#endif
}

void setWindowIcon(Handle window, int width, int height, unsigned char* pixels) {
    if (window == nullptr || pixels == nullptr || width <= 0 || height <= 0) {
        return;
    }
#if defined(EUI_WINDOW_BACKEND_SDL3)
    SDL_Surface* surface = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_RGBA32, pixels, width * 4);
#else
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
        pixels, width, height, 32, width * 4, SDL_PIXELFORMAT_RGBA32);
#endif
    if (surface != nullptr) {
        SDL_SetWindowIcon(static_cast<SDL_Window*>(window), surface);
#if defined(EUI_WINDOW_BACKEND_SDL3)
        SDL_DestroySurface(surface);
#else
        SDL_FreeSurface(surface);
#endif
    }
    eui_set_application_icon_rgba(width, height, pixels);
}

void setImeCursorRect(Handle window, float x, float y, float width, float height) {
#if defined(_WIN32)
    HWND hwnd = hwndForWindow(window);
    if (hwnd != nullptr) {
        HIMC context = ImmGetContext(hwnd);
        if (context != nullptr) {
            const double fontHeight = std::max(12.0f, height);
            const LONG caretX = roundLong(x);
            const LONG caretY = roundLong(y + height);
            const LONG candidateY = roundLong(y + height * 0.45f);

            COMPOSITIONFORM composition{};
            composition.dwStyle = CFS_FORCE_POSITION;
            composition.ptCurrentPos.x = caretX;
            composition.ptCurrentPos.y = caretY;
            composition.rcArea.left = roundLong(x);
            composition.rcArea.top = roundLong(y);
            composition.rcArea.right = roundLong(x + width);
            composition.rcArea.bottom = roundLong(y + height);
            ImmSetCompositionWindow(context, &composition);

            CANDIDATEFORM candidate{};
            candidate.dwIndex = 0;
            candidate.dwStyle = CFS_CANDIDATEPOS;
            candidate.ptCurrentPos.x = caretX;
            candidate.ptCurrentPos.y = candidateY;
            candidate.rcArea = composition.rcArea;
            ImmSetCandidateWindow(context, &candidate);
            LOGFONTW font{};
            font.lfHeight = -roundLong(static_cast<float>(fontHeight));
            font.lfCharSet = DEFAULT_CHARSET;
            font.lfQuality = CLEARTYPE_QUALITY;
            wcscpy_s(font.lfFaceName, LF_FACESIZE, L"Microsoft YaHei UI");
            ImmSetCompositionFontW(context, &font);

            ImmReleaseContext(hwnd, context);
        }
    }

    SDL_Rect rect{
        roundLong(x),
        roundLong(y),
        roundLong(width),
        roundLong(height)
    };
#if defined(EUI_WINDOW_BACKEND_SDL3)
    SDL_SetTextInputArea(static_cast<SDL_Window*>(window), &rect, rect.w);
#else
    SDL_SetTextInputRect(&rect);
#endif
#else
    int windowWidth = 0;
    int windowHeight = 0;
    int drawableWidth = 0;
    int drawableHeight = 0;
    SDL_GetWindowSize(static_cast<SDL_Window*>(window), &windowWidth, &windowHeight);
#if defined(EUI_WINDOW_BACKEND_SDL3)
    SDL_GetWindowSizeInPixels(static_cast<SDL_Window*>(window), &drawableWidth, &drawableHeight);
#else
    SDL_GL_GetDrawableSize(static_cast<SDL_Window*>(window), &drawableWidth, &drawableHeight);
#endif
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
#if defined(EUI_WINDOW_BACKEND_SDL3)
    SDL_SetTextInputArea(static_cast<SDL_Window*>(window), &rect, rect.w);
#else
    SDL_SetTextInputRect(&rect);
#endif
#endif
}

} // namespace core::window

#else

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#include "core/platform/ime_bridge.h"

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

void getCursorPosition(Handle window, double& x, double& y) {
    glfwGetCursorPos(static_cast<GLFWwindow*>(window), &x, &y);
}

bool isMouseButtonDown(Handle window, int button) {
    const int glfwButton = button == 1 ? GLFW_MOUSE_BUTTON_RIGHT : GLFW_MOUSE_BUTTON_LEFT;
    return glfwGetMouseButton(static_cast<GLFWwindow*>(window), glfwButton) == GLFW_PRESS;
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

} // namespace core::window

#endif
