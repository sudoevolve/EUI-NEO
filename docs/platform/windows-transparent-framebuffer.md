# Windows transparent framebuffer presentation

The bundled GLFW Win32 backend includes the small WM_PAINT repair proposed in
[glfw/glfw#2815](https://github.com/glfw/glfw/pull/2815) by jeweg. A transparent
window has two distinct alpha surfaces: the renderer's framebuffer and DWM's
redirection surface. Correct GL alpha alone does not initialize the latter.
Undefined redirection alpha can produce intermittent black transparent margins,
including on NVIDIA or virtual display paths.

For transparent windows, WM_PAINT clears the invalidated redirection area with
PatBlt(BLACKNESS), which defines its RGB and alpha as zero. The normal GLFW damage
notification still requests renderer repaint. Opaque windows keep their previous
paint handling. No window region, color key or geometry crop is introduced.

External GLFW targets do not receive this bundled-source fix automatically.
Consumers supplying GLFW must carry the corresponding repair themselves.
SDL2 continues to report framebufferTransparent=false; its transparency contract
is unchanged. Vulkan swapchain composite-alpha support remains device-dependent.

`transparent_cache_probe` checks the Win32 GLFW/OpenGL path against an owned,
known-color background, including initial paint and subsequent cached frames.
It returns CTest skip code 77 on other backend combinations. `--manual` retains
the test windows for visual inspection; normal CTest `--frames` arguments do not.
The separate `transparent_window_viewer` fixture shows actual desktop margins.
