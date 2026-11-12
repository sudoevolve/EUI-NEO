// 复用交互 fixture 的 compose，确保文档示例经过真实后端绘制。
#include "tests/fixtures/data_tools_viewer.cpp"
#include "core/dsl_runtime.h"
#if defined(EUI_WINDOW_BACKEND_SDL2)
#define SDL_MAIN_HANDLED
#include <SDL.h>
#else
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#endif
#if defined(EUI_RENDER_BACKEND_OPENGL)
#include <glad/glad.h>
#endif
#include <iostream>

int main(int argc, char** argv) {
    core::render::initializeRenderBackendLoader();
#if defined(EUI_WINDOW_BACKEND_SDL2)
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
#else
    if (!glfwInit()) return 1;
#endif
    core::window::WindowCreateRequest request;
    request.width = 1120; request.height = 760; request.visible = false;
    request.renderApi = core::render::windowRenderApi();
    auto window = core::window::createWindow(request);
    if (!window) return 2;
    auto backend = core::render::createRenderBackend(window);
    if (!backend || !backend->initialize()) return 3;
    int failure = 0;
    {
        core::render::ScopedRenderBackend active(*backend);
        core::dsl::Runtime runtime;
        runtime.initialize(window);
        core::dsl::Ui* tree = nullptr;
        for (int i = 0; i < 4; ++i) {
            if (i == 1) core::queueKeyInput(window, {core::InputKey::Tab, core::KeyAction::Press, {}});
            if (i == 2) core::queueTextInput(window, "Sensor 3");
            if (i > 0) runtime.update(window, .016f, 1, 1);
            runtime.compose("tools", 1120, 760, [&](auto& ui, const auto& screen) {
                tree = &ui;
                app::compose(ui, screen);
            });
            runtime.update(window, .016f, 1, 1, false);
            backend->makeCurrent();
            backend->beginFrame({window, core::window::nativeWindowInfo(window), 1120, 760, 1});
            runtime.render(1120, 760, 1, {0, 0, 0, 1});
#if defined(EUI_RENDER_BACKEND_OPENGL)
            if (i == 3 && argc == 2) {
                std::vector<unsigned char> pixels(1120 * 760 * 3);
                glPixelStorei(GL_PACK_ALIGNMENT, 1);
                glReadPixels(0, 0, 1120, 760, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
                std::ofstream output(argv[1], std::ios::binary);
                output << "P6\n1120 760\n255\n";
                for (int y = 759; y >= 0; --y) output.write(reinterpret_cast<const char*>(pixels.data() + y * 1120 * 3), 1120 * 3);
            }
#endif
            backend->present();
        }
        const auto* filter = tree->find("filter.hit");
        const auto& table = tree->state<components::table_detail::State>("table.tableState");
        if (!filter || filter->accessibleValue != "Sensor 3" || table.order.size() != 6250 ||
            !tree->find("signal.line") || !tree->find("filter.hit.focusRing")) failure = 4;
        runtime.shutdown(false);
    }
    backend.reset();
    core::releaseInputQueue(window);
    core::window::destroyWindow(window);
#if defined(EUI_WINDOW_BACKEND_SDL2)
    SDL_Quit();
#else
    glfwTerminate();
#endif
    if (failure) std::cerr << "data tools keyboard filtering or rendering failed\n";
    return failure;
}
