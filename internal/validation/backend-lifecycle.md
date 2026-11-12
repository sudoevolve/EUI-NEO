# Lifecycle probe across window and render backends

The lifecycle probe uses `core::render::windowRenderApi()` to create a window
matching the configured backend. It does not include GLAD or call OpenGL APIs.
A Vulkan build must create a Vulkan surface rather than an OpenGL context; merely
removing the unused GLAD header would fix compilation but leave runtime invalid.

This preserves the existing assertions: onStart runs once before the first
compose, update performs that compose, and shutdown pairs with onStart. The
upstream dev no-exceptions cleanup remains intact. No backend implementation or
application scheduler is added.

## Validation

Use the existing CI matrix: `EUI_WINDOW_BACKEND=glfw|sdl2` and
`EUI_RENDER_BACKEND=opengl|vulkan`. Configure with the fixtures on, build, run
unit/probe tests on a private Xvfb display, install the SDK, then build/run its
`tests/consumers/find-package` consumer. Vulkan requires its development SDK,
glslang tools and a working ICD (software lavapipe is sufficient for this test).
SDL2 needs a system package or `EUI_DEPS_MODE=auto`; it is not bundled.

```sh
cmake -S . -B build-ci -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DEUI_BUILD_APPS=OFF -DEUI_BUILD_USER_APPS=OFF \
  -DEUI_BUILD_TEST_FIXTURES=ON -DEUI_ENABLE_INSTALL=ON \
  -DEUI_WINDOW_BACKEND=glfw -DEUI_RENDER_BACKEND=vulkan \
  -DGLFW_BUILD_WAYLAND=OFF -DGLFW_BUILD_X11=ON
cmake --build build-ci --parallel
ctest --test-dir build-ci -L unit --output-on-failure
xvfb-run -a ctest --test-dir build-ci -L probe --output-on-failure
cmake --install build-ci --prefix "$PWD/package-sdk"
cmake -S tests/consumers/find-package -B package-consumer \
  -DCMAKE_PREFIX_PATH="$PWD/package-sdk"
cmake --build package-consumer --parallel
ctest --test-dir package-consumer --output-on-failure
```

Local Linux/GCC verification: GLFW/Vulkan Release builds, 25 unit + 8 probe
tests pass under private Xvfb/lavapipe, and SDK install/consumer build + test pass.
GLFW/OpenGL Debug and SDL2/OpenGL Release lifecycle regressions pass.
SDL2/Vulkan and the full remote matrix are tracked separately until completed.
Windows/native Wayland/physical GPU validation is outside this Linux evidence.
