#include "components/markdown.h"
#include "core/dsl_runtime.h"
#include "core/render/render_backend.h"
#include "core/window/window_backend.h"
#if defined(EUI_RENDER_BACKEND_OPENGL)
#include <glad/glad.h>
#if defined(EUI_WINDOW_BACKEND_SDL2)
#define SDL_MAIN_HANDLED
#include <SDL.h>
#else
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#endif
#include <iostream>
#include <vector>
int main() {
#if defined(EUI_WINDOW_BACKEND_SDL2)
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
#else
    if (!glfwInit()) return 1;
#endif
    core::window::WindowCreateRequest request;
    request.width = 480; request.height = 300; request.title = "Markdown viewport pixel oracle";
    auto window = core::window::createWindow(request);
    core::render::initializeRenderBackendLoader();
    auto backend = core::render::createRenderBackend(window);
    if (!window || !backend || !backend->initialize()) return 1;
    backend->makeCurrent();
    core::render::ScopedRenderBackend scope(*backend);
    core::TextPrimitive::setDefaultFontFiles(std::string(EUI_TEST_SOURCE_DIR) + "/assets/JingNanJunJunTi-JinNanJunJunTi-Bold-2.ttf", {});
    core::dsl::Runtime runtime;
    if (!runtime.initialize(window)) return 1;
    std::string source;
    for (int i = 0; i < 12; ++i)
        source += "# Heading 中文\n\nLong **bold** paragraph, 中文 `inline` [link](https://example.com) ~~delete~~.\n\n"
                  "> Quoted text wraps across the viewport.\n\n- item with 中文 and English\n\n"
                  "| Left | Right |\n| :- | -: |\n| a long value wraps into rows | 中文字符 |\n\n"
                  "```cpp\nint x = 123;\n```\n\n---\n\n";
    int failures = 0;
    for (bool dark : {false, true}) for (float dpi : {1.0f, 1.5f}) for (float top : {0.0f, 113.5f, 430.0f}) {
        std::vector<unsigned char> original;
        for (bool sliced : {false, true}) {
            runtime.compose("pixels", 480 / dpi, 300 / dpi, [&](auto& ui, const auto&) {
                ui.stack("clip").position(8, 8).size(480 / dpi - 16, 300 / dpi - 16).clip().content([&] {
                    auto md = components::markdown(ui, "md").position(0, -top).width(480 / dpi - 16)
                        .markdown(source).theme(dark ? components::theme::dark() : components::theme::light());
                    if (sliced) md.viewport(top, 300 / dpi - 16);
                    md.build();
                }).build();
            });
            runtime.update(window, 0, dpi, 1);
            runtime.requestFullPaint();
            runtime.render(480, 300, dpi, dark ? core::Color{0, 0, 0, 1} : core::Color{1, 1, 1, 1});
            glFinish();
            std::vector<unsigned char> pixels(480 * 300 * 4);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadPixels(0, 0, 480, 300, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            if (!sliced) original = std::move(pixels);
            else if (pixels != original) { std::cerr << "viewport pixels changed at " << top << " dpi " << dpi << '\n'; ++failures; }
        }
    }
    runtime.shutdown(); backend.reset(); core::window::destroyWindow(window);
#if defined(EUI_WINDOW_BACKEND_SDL2)
    SDL_Quit();
#else
    glfwTerminate();
#endif
    return failures ? 1 : 0;
}
#else
// GPU readback oracle is OpenGL-specific; geometry/ID coverage runs on all backends.
int main() { return 77; }
#endif
