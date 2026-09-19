#include "modules/plot/plot.h"
#include "modules/plot/session.h"
#include "core/dsl_runtime.h"
#include "core/render/render_backend.h"
#include "core/window/window_backend.h"
#include <glad/glad.h>
#if defined(EUI_WINDOW_BACKEND_SDL2)
#define SDL_MAIN_HANDLED
#include <SDL.h>
#else
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#endif
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
} // namespace
int main() {
    using namespace modules::plot;
    core::render::initializeRenderBackendLoader();
#if defined(EUI_WINDOW_BACKEND_SDL2)
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        return 1;
#else
    if (!glfwInit())
        return 1;
#endif
    core::window::WindowCreateRequest request;
    request.width = 800;
    request.height = 600;
    request.title = "Plot application interaction validation";
    auto window = core::window::createWindow(request);
    auto backend = core::render::createRenderBackend(window);
    if (!window || !backend || !backend->initialize())
        return 2;
    int result = 0;
    {
        backend->makeCurrent();
        core::render::ScopedRenderBackend scope(*backend);
        core::dsl::Runtime runtime;
        runtime.initialize(window);
        PlotSession session;
        auto heat = session.make<HeatmapPlot>();
        auto mailbox = session.mailbox<ScalarField>();
        auto shutdown = session.shutdownHandler();
        try {
            mailbox->publish(ScalarField(2, 2, {1, 2, 3, 4}, {10, 30}, {100, 140}));
            heat->setField(std::move(*mailbox->take()));
            core::dsl::Ui* ui = nullptr;
            for (double dpi : {1., 2.}) {
                heat->resetView();
                heat->setEnabled(true);
                const auto frame = [&] {
                    runtime.compose("test", 800 / dpi, 600 / dpi,
                                    [&](core::dsl::Ui& value, const core::dsl::Screen&) {
                                        ui = &value;
                                        heat->compose(value, "heat", float(800 / dpi), float(600 / dpi), dpi);
                                    });
                    runtime.update(window, .016f, 1, float(dpi));
                    backend->beginFrame(
                        {window, core::window::nativeWindowInfo(window), 800, 600, float(dpi)});
                    runtime.render(800, 600, float(dpi), {0, 0, 0, 1});
                    backend->present();
                    glFinish();
                };
                frame();
                auto bounds = ui->find("heat.input")->frame;
                // Element layout is logical; input queues take physical pixel coordinates.
                const double x = (bounds.x + bounds.width * .75) * dpi,
                             y = (bounds.y + bounds.height * .25) * dpi;
                core::queuePointerMotion(window, x, y, {}, {});
                runtime.update(window, .016f, 1, float(dpi));
                require(heat->probe() && heat->probe()->value == 4, "heatmap original cell hover at DPI");
                const auto original = heat->axes().x.range();
                core::queuePointerButton(window, x, y, core::PointerButton::Left, core::PointerAction::Press,
                                         {});
                runtime.update(window, .016f, 1, float(dpi));
                core::queuePointerMotion(window, x + 40, y, core::PointerButton::Left, {});
                runtime.update(window, .016f, 1, float(dpi));
                frame(); // Recomposition must preserve capture.
                core::queuePointerButton(window, x + 40, y, core::PointerButton::Left,
                                         core::PointerAction::Release, {});
                runtime.update(window, .016f, 1, float(dpi));
                require(heat->axes().x.range().min < original.min, "heatmap drag failed");
                const auto before = heat->axes().x.range();
                core::queueScrollInput(window, 0, 1);
                runtime.update(window, .016f, 1, float(dpi));
                frame();
                require(heat->axes().x.range().max - heat->axes().x.range().min < before.max - before.min,
                        "heatmap zoom failed");
                heat->setEnabled(false);
                frame();
                const auto disabled = heat->axes().x.range();
                core::queueScrollInput(window, 0, 2);
                runtime.update(window, .016f, 1, float(dpi));
                require(heat->axes().x.range().min == disabled.min, "disabled heatmap zoomed");
                heat->setEnabled(true);
                frame();
                core::queuePointerButton(window, x, y, core::PointerButton::Right, core::PointerAction::Press,
                                         {});
                runtime.update(window, .016f, 1, float(dpi));
                core::queuePointerButton(window, x, y, core::PointerButton::Right,
                                         core::PointerAction::Release, {});
                runtime.update(window, .016f, 1, float(dpi));
                frame();
                require(heat->axes().x.range().min == 10 && heat->axes().x.range().max == 30,
                        "heatmap right-click reset");
                bounds = ui->find("heat.input")->frame;
                const double startX = (bounds.x + bounds.width * .25) * dpi;
                const double startY = (bounds.y + bounds.height * .25) * dpi;
                const double endX = (bounds.x + bounds.width * .75) * dpi;
                const double endY = (bounds.y + bounds.height * .75) * dpi;
                core::KeyModifiers shift;
                shift.shift = true;
                core::queuePointerButton(window, startX, startY, core::PointerButton::Left,
                                         core::PointerAction::Press, shift);
                runtime.update(window, .016f, 1, float(dpi));
                core::queuePointerMotion(window, endX, endY, core::PointerButton::Left, shift);
                runtime.update(window, .016f, 1, float(dpi));
                frame();
                core::queuePointerButton(window, endX, endY, core::PointerButton::Left,
                                         core::PointerAction::Release, shift);
                runtime.update(window, .016f, 1, float(dpi));
                require(std::abs(heat->axes().x.range().min - 15) < 1e-6 &&
                        std::abs(heat->axes().x.range().max - 25) < 1e-6 &&
                        std::abs(heat->axes().y.range().min - 110) < 1e-6 &&
                        std::abs(heat->axes().y.range().max - 130) < 1e-6,
                        "heatmap box zoom failed at DPI");
                frame();
                // The UI image is a real GPU texture, not a data-only interaction mock.
                require(ui->find("heat.data") != nullptr && glGetError() == GL_NO_ERROR,
                        "heatmap render failed");
            }
            shutdown();
            shutdown();
            require(!mailbox->publish(ScalarField(1, 1, {1})), "closed session accepted worker update");
            std::cout << "Heatmap drag, zoom, box zoom, hover, reset, disable, recompose and session shutdown passed "
                         "at 1x/2x\n";
        } catch (const std::exception& e) {
            std::cerr << e.what() << '\n';
            result = 1;
            shutdown();
        }
        runtime.shutdown();
    }
    backend.reset();
    core::window::destroyWindow(window);
#if defined(EUI_WINDOW_BACKEND_SDL2)
    SDL_Quit();
#else
    glfwTerminate();
#endif
    return result;
}
