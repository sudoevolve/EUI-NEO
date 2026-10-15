#include "core/render/render_backend.h"
#include "core/window/window_backend.h"
#include "core/app/main_window_runtime.h"
#include "eui/detail/dsl_app_impl.h"
#include "eui_neo.h"

namespace app {
const DslAppConfig& dslAppConfig() {
    static const auto config = DslAppConfig{}.iconPath("").tray(false).transparent(true)
        .clearColor({0, 0, 0, 0});
    return config;
}
void compose(eui::Ui& ui, const eui::Screen&) {
    ui.rect("panel").position(40, 40).size(240, 140).radius(28).color({0.8f, 0.2f, 0.1f, 1}).build();
}
}

#if defined(_WIN32) && defined(EUI_RENDER_BACKEND_OPENGL) && !defined(EUI_WINDOW_BACKEND_SDL2)
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <dwmapi.h>
#include <cstdio>
#include <cstring>

#if defined(_MSC_VER)
namespace {
int redirectionClears = 0;
BOOL WINAPI observeRedirectionClear(HDC dc, int x, int y, int width, int height, DWORD operation) {
    using Blit = BOOL(WINAPI *)(HDC, int, int, int, int, DWORD);
    const auto actual = reinterpret_cast<Blit>(GetProcAddress(GetModuleHandleW(L"gdi32.dll"), "PatBlt"));
    const BOOL result = actual(dc, x, y, width, height, operation);
    if (result && operation == BLACKNESS && width > 0 && height > 0) ++redirectionClears;
    return result;
}
}
// Observe and forward the real GDI clear. Without the Win32 repair, correctly
// zeroed allocations could make a pixels-only regression accidentally pass.
extern "C" { BOOL(WINAPI *__imp_PatBlt)(HDC, int, int, int, int, DWORD) = observeRedirectionClear; }
#endif

int main(int argc, char** argv) {
    if (!glfwInit()) return 1;
    const COLORREF backingColor = RGB(80, 160, 100);
    HBRUSH brush = CreateSolidBrush(backingColor);
    WNDCLASSW wc{};
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpfnWndProc = DefWindowProcW;
    wc.lpszClassName = L"EuiAlphaProbeBacking";
    wc.hbrBackground = brush;
    RegisterClassW(&wc);
    HWND backing = CreateWindowExW(WS_EX_TOPMOST, wc.lpszClassName, L"EUI alpha probe backing", WS_POPUP,
                                  120, 120, 420, 320, nullptr, nullptr, wc.hInstance, nullptr);
    if (!backing) return 2;
    ShowWindow(backing, SW_SHOWNOACTIVATE);
    UpdateWindow(backing);

    core::window::WindowCreateRequest request;
    request.width = 320;
    request.height = 220;
    request.title = "EUI transparent cache probe";
    request.decorated = false;
    request.transparent = true;
    request.visible = false;
    request.alwaysOnTop = true;
    request.positionSet = true;
    request.x = 160;
    request.y = 160;
    auto window = core::window::createWindow(request);
    if (!window || !core::window::framebufferTransparent(window)) return 3;
    auto backend = core::render::createRenderBackend(window, nullptr, true);
    if (!backend || !backend->initialize()) return 4;
    auto *glfw = static_cast<GLFWwindow *>(window);
    const HWND native = glfwGetWin32Window(glfw);
    // Model a newly allocated redirection surface containing opaque black.
    // This is separate from the GL framebuffer's correctly zeroed alpha.
    BITMAPINFO info{};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = 1;
    info.bmiHeader.biHeight = 1;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    const unsigned int opaqueBlack = 0xff000000u;
    HDC redirection = GetDC(native);
    StretchDIBits(redirection, 0, 0, 320, 220, 0, 0, 1, 1, &opaqueBlack, &info,
                  DIB_RGB_COLORS, SRCCOPY);
    ReleaseDC(native, redirection);
    InvalidateRect(native, nullptr, FALSE);
    SetWindowPos(backing, HWND_TOP, 120, 120, 420, 320, SWP_NOACTIVATE);
    app::initialize(window);
    app::AppRunner runner;
    app::MainWindowRuntime runtime(runner, [&] {
        glfwShowWindow(glfw);
        SetWindowPos(native, HWND_TOP, 160, 160, 320, 220, SWP_NOACTIVATE);
        InvalidateRect(native, nullptr, FALSE);
        UpdateWindow(native);
    });
    int failures = 0;
    for (int frame = 0; frame < 6; ++frame) {
        runner.paintRequested = true;
        runtime.updateAndRender(window, *backend, {320, 220, 1, 1}, 0, false, true,
                                [] {}, [] { return app::MainWindowMetrics{320, 220, 1, 1}; });
        glfwPollEvents();
        DwmFlush();
        POINT origin{};
        ClientToScreen(native, &origin);
        HDC screen = GetDC(nullptr);
        const COLORREF margin = GetPixel(screen, origin.x + 8, origin.y + 8);
        const COLORREF center = GetPixel(screen, origin.x + 160, origin.y + 110);
        ReleaseDC(nullptr, screen);
        std::printf("frame %d: margin=%06lx expected=%06lx center=%06lx\n",
                    frame, margin, backingColor, center);
        // WM_PAINT initializes the redirection surface and requests a GL
        // repaint. From the next frame onward the panel must also be visible.
        if (margin != backingColor || (frame > 0 && (GetRValue(center) < 180 || GetGValue(center) > 80))) ++failures;
    }
#if defined(_MSC_VER)
    std::printf("redirection initialization clears: %d\n", redirectionClears);
    if (redirectionClears == 0) ++failures;
#endif
    if (argc > 1 && std::strcmp(argv[1], "--manual") == 0) {
        while (!glfwWindowShouldClose(glfw)) glfwWaitEvents();
    }
    app::shutdown();
    backend.reset();
    core::window::destroyWindow(window);
    DestroyWindow(backing);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    DeleteObject(brush);
    glfwTerminate();
    return failures ? 1 : 0;
}
#else
int main() { return 77; } // Not applicable without the Win32 GLFW/OpenGL compositor.
#endif
