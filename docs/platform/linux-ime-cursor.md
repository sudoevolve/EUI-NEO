# Linux X11 IME caret positioning

EUI's bundled GLFW prefers `XIMPreeditPosition | XIMStatusNothing`, with a
window-owned font set and the original `PreeditNothing` fallback. It updates
`XNSpotLocation` through `glfwSetX11InputMethodCursorPos(window, x, y)` on the UI
thread. Coordinates are window-local pixels, clamped to XPoint's positive range;
the EUI bridge converts framebuffer rectangles and uses the caret line bottom.
The function returns false when positioning is unavailable. No private GLFW
layout is exposed to applications, and the window releases its XIC/font set.

The bundled target exports `EUI_GLFW_X11_IME_POSITION`. Stock external GLFW and
SDL retain the existing path; native Wayland positioning is not implemented by
this change. GLFW is vendored under its zlib license; this is an EUI-specific
extension, not a claim about upstream GLFW 3.4 API availability.

## Reproduce

Configure with `-DEUI_DEPS_MODE=bundled -DEUI_BUILD_LINUX_IME_PROBE=ON -DEUI_WINDOW_BACKEND=glfw` and build
`eui_linux_ime_cursor_probe`. On an isolated Xvfb display, run a private
`dbus-run-session` with `ibus-daemon --xim`; set `LC_ALL=C.UTF-8` and
`XMODIFIERS=@im=ibus`, then run the probe. It requires a real positioning-capable
XIM server and returns nonzero when absent; it does not silently count a skip as
success. The optional `--interactive` mode stays open for twelve seconds: type
Chinese with an installed engine, press F2 to move the caret, then select a
candidate; committed Unicode codepoints are printed without capturing user data.

Linux/GCC Debug integration: IBus 1.5.29 accepted the negotiated position style
and three caret updates. XGetICValues(inputStyle) reported zero despite successful
creation on this server; the adapter records the chosen style from XCreateIC
instead of relying on that getter. The reproducible private-display test is:

```sh
python3 tests/platform/run_linux_ime_cursor_probe.py /path/to/eui_linux_ime_cursor_probe
```

It requires Xvfb, dbus-run-session, IBus, its GTK3 panel, libpinyin,
python-xlib, and system Python GI/IBus. Test-only process paths reflect Debian;
other distributions can adapt those paths. Failure/timeouts return nonzero.
2026-10-05: the real candidate popup moved from (220,440) to (400,280),
matching window origin (100,80) plus caret (120,360) and (300,200).
Selecting the candidate committed U+4F60 U+597D (你好). Both the standalone
probe and EUI bundled Debug library built successfully. Native Wayland,
Windows/macOS, other IM engines, and physical high-DPI displays remain untested.
