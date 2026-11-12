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
        // 对照高反差页面背景检查四角；短表格、滚动条、分数行偏移和浅色主题均不能越过圆角。
        auto cornerRows = std::make_shared<components::DataTableRows>();
        for (int row = 0; row < 100; ++row) cornerRows->push_back({"Row " + std::to_string(row), "Value"});
        for (int frame = 0; frame < 3; ++frame) {
            runtime.compose("corners", 1120, 760, [&](auto& ui, const auto&) {
                ui.rect("page").size(1120, 760).color({0.16f, 0.22f, 0.30f, 1}).build();
                for (int panel = 0; panel < 4; ++panel) {
                    const float x = 30.0f + (panel % 2) * 540.0f, y = 30.0f + (panel / 2) * 300.0f;
                    const std::string id = "corners." + std::to_string(panel);
                    ui.stack(id + ".position").position(x, y).size(480, 230).content([&] {
                        ui.template state<components::table_detail::State>(id + ".tableState").offset = frame * 17.5f;
                        auto table = components::dataTable(ui, id).theme(panel % 2 ? components::theme::light() : components::theme::dark())
                            .size(480, 230).columns({"Name", "Status"});
                        if (panel < 2) table.rows({{"EUI Core", "Active"}, {"Gallery", "Review"}, {"Docs", "Draft"}, {"Runtime", "Stable"}}).rowHeight(48);
                        else table.data(cornerRows);
                        table.build();
                    }).build();
                }
            });
            runtime.update(window, .016f, 1, 1, false);
            backend->makeCurrent();
            backend->beginFrame({window, core::window::nativeWindowInfo(window), 1120, 760, 1});
            runtime.render(1120, 760, 1, {0, 0, 0, 1});
#if defined(EUI_RENDER_BACKEND_OPENGL)
            std::vector<unsigned char> pixels(1120 * 760 * 3);
            glPixelStorei(GL_PACK_ALIGNMENT, 1);
            glReadPixels(0, 0, 1120, 760, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
            const auto at = [&](int x, int y, int channel) { return pixels[((759 - y) * 1120 + x) * 3 + channel]; };
            for (int panel = 0; panel < 4; ++panel) {
                const int x = 30 + (panel % 2) * 540, y = 30 + (panel / 2) * 300;
                for (const auto point : {std::pair<int, int>{x + 1, y + 1}, {x + 478, y + 1}, {x + 1, y + 228}, {x + 478, y + 228}})
                    for (int channel = 0; channel < 3; ++channel)
                        if (std::abs(int(at(point.first, point.second, channel)) - int(at(5, 5, channel))) > 5) failure = 5;
                if (std::abs(int(at(x + 240, y + 5, 0)) - int(at(5, 5, 0))) < 5) failure = 6;
            }
            if (frame == 2 && argc == 2) {
                std::ofstream output(std::string(argv[1]) + ".corners.ppm", std::ios::binary);
                output << "P6\n1120 760\n255\n";
                for (int y = 759; y >= 0; --y) output.write(reinterpret_cast<const char*>(pixels.data() + y * 1120 * 3), 1120 * 3);
            }
#endif
            backend->present();
        }
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
    if (failure) std::cerr << "data tools keyboard filtering or rendering failed: " << failure << '\n';
    return failure;
}
