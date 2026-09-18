#include "modules/plot/plot.h"
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
void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
void near(double actual, double expected) {
    require(std::abs(actual - expected) < 1e-6, "view numeric mismatch");
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
    request.title = "Plot runtime probe";
    request.renderApi = core::window::RenderApi::OpenGL;
    const auto window = core::window::createWindow(request);
    auto backend = core::render::createRenderBackend(window);
    if (!window || !backend || !backend->initialize())
        return 2;
    int result = 0;
    {
        backend->makeCurrent();
        core::render::ScopedRenderBackend scope(*backend);
        core::dsl::Runtime runtime;
        runtime.initialize(window);
        try {
            Renderer renderer;
            Batch triangle{{{0, 0}, {100, 0}, {0, 100}}, {1, 0, 0, 1}};
            glEnable(GL_SCISSOR_TEST);
            glScissor(1, 2, 3, 4);
            glViewport(7, 8, 120, 130);
            renderer.render({triangle}, 100, 100);
            GLint viewport[4]{};
            glGetIntegerv(GL_VIEWPORT, viewport);
            require(viewport[0] == 7 && viewport[1] == 8 && viewport[2] == 120 && viewport[3] == 130,
                    "GL viewport leaked");
            require(glIsEnabled(GL_SCISSOR_TEST), "GL scissor enable leaked");
            glDisable(GL_SCISSOR_TEST);
            GLuint readback = 0;
            glGenFramebuffers(1, &readback);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, readback);
            glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                   renderer.image()->descriptor().texture, 0);
            unsigned char pixel[4]{};
            glReadPixels(10, 89, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
            require(pixel[0] > 240 && pixel[1] < 10 && pixel[2] < 10, "triangle pixel mismatch");
            glReadPixels(90, 9, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
            require(pixel[0] < 30 && pixel[1] < 30 && pixel[2] < 30, "outside triangle pixel mismatch");
            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
            glDeleteFramebuffers(1, &readback);
            renderer.release();
            Axes pixelAxes;
            pixelAxes.x.setRange({0, 10});
            pixelAxes.y.setRange({-2, 2});
            for (const auto kind : {Graph::Line, Graph::Scatter, Graph::Step, Graph::Stem, Graph::Bar,
                                    Graph::Area, Graph::ErrorBars, Graph::Band}) {
                Series shape;
                shape.data = Data({1, 3, 7, 9}, {-1, 1, -1, 1});
                shape.graph = kind;
                shape.lower = {0.2, 0.2, 0.2, 0.2};
                shape.upper = {0.8, 0.8, 0.8, 0.8};
                renderer.render({{tessellate(shape, pixelAxes, {0, 0, 100, 100}), {1, 0, 0, 1}}}, 100, 100);
                glGenFramebuffers(1, &readback);
                glBindFramebuffer(GL_READ_FRAMEBUFFER, readback);
                glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                       renderer.image()->descriptor().texture, 0);
                std::vector<unsigned char> pixels(100 * 100 * 4);
                glReadPixels(0, 0, 100, 100, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
                std::size_t red = 0;
                for (std::size_t i = 0; i < pixels.size(); i += 4)
                    red += pixels[i] > 240 && pixels[i + 1] < 10;
                require(red > 0, "graph produced no pixels");
                glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
                glDeleteFramebuffers(1, &readback);
            }
            // 缺失样本两侧可见，中间必须保持背景色。
            Series gap;
            gap.data = Data({0, 3, 5, 7, 10}, {0, 0, std::numeric_limits<double>::quiet_NaN(), 0, 0});
            gap.style.lineWidth = 3;
            renderer.render({{tessellate(gap, pixelAxes, {0, 0, 100, 100}), {1, 0, 0, 1}}}, 100, 100);
            glGenFramebuffers(1, &readback);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, readback);
            glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                   renderer.image()->descriptor().texture, 0);
            glReadPixels(50, 50, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
            require(pixel[0] < 30, "NaN gap bridged");
            glReadPixels(1, 50, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
            require(pixel[0] > 240, "left edge lost");
            glReadPixels(98, 50, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
            require(pixel[0] > 240, "right edge lost");
            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
            glDeleteFramebuffers(1, &readback);
            for (int i = 0; i < 32; ++i) {
                renderer.render({triangle}, 100 + i % 2, 100);
                const GLuint texture = renderer.image()->descriptor().texture;
                renderer.release();
                glFinish();
                require(!glIsTexture(texture), "plot texture leaked after release");
            }

            Plot plot;
            Series line;
            line.data = Data({0, 0.2, 1}, {-1, 1, -1});
            plot.setSeries({line});
            Axes axes;
            axes.x.setRange({0, 1});
            axes.y.setRange({-1, 1});
            plot.setAxes(axes);
            core::dsl::Ui* ui = nullptr;
            const auto compose = [&](float dpi) {
                runtime.compose("plot", 400, 300, [&](core::dsl::Ui& value, const core::dsl::Screen&) {
                    ui = &value;
                    plot.compose(value, "chart", 400, 300, dpi);
                });
            };
            for (const float dpi : {1.f, 2.f}) {
                plot.setAxes(axes);
                compose(dpi);
                runtime.update(window, 0.016f, 1, dpi);
                const auto frame = ui->find("chart.input")->frame;
                const double x = (frame.x + frame.width / 2) * dpi, y = (frame.y + frame.height / 2) * dpi;
                core::queuePointerButton(window, x, y, core::PointerButton::Left, core::PointerAction::Press,
                                         {});
                runtime.update(window, 0.016f, 1, dpi);
                compose(dpi);
                core::queuePointerMotion(window, x + frame.width * 0.1 * dpi, y, core::PointerButton::Left,
                                         {});
                runtime.update(window, 0.016f, 1, dpi);
                near(plot.axes().x.range().min, -0.1);
                compose(dpi);
                core::queuePointerButton(window, 900, 700, core::PointerButton::Left,
                                         core::PointerAction::Release, {});
                runtime.update(window, 0.016f, 1, dpi);
                core::queuePointerMotion(window, x, y, {}, {});
                runtime.update(window, 0.016f, 1, dpi);
                near(plot.axes().x.range().min, -0.1);
                core::queueScrollInput(window, 0, 1);
                runtime.update(window, 0.016f, 1, dpi);
                const auto zoomed = plot.axes().x.range();
                near(zoomed.max - zoomed.min, std::exp(-0.12));
                near((zoomed.min + zoomed.max) / 2, 0.4);
                core::queueScrollInput(window, 0, -1);
                runtime.update(window, 0.016f, 1, dpi);
                near(plot.axes().x.range().min, -0.1);
                plot.setEnabled(false);
                compose(dpi);
                core::queuePointerButton(window, x, y, core::PointerButton::Left, core::PointerAction::Press,
                                         {});
                core::queuePointerMotion(window, x + 50, y, core::PointerButton::Left, {});
                core::queuePointerButton(window, x + 50, y, core::PointerButton::Left,
                                         core::PointerAction::Release, {});
                runtime.update(window, 0.016f, 1, dpi);
                near(plot.axes().x.range().min, -0.1);
                plot.setEnabled(true);
            }
            plot.setAxes(axes);
            compose(1);
            runtime.update(window, 0.016f, 1, 1);
            const auto box = ui->find("chart.input")->frame;
            core::KeyModifiers shift;
            shift.shift = true;
            core::queuePointerButton(window, box.x + box.width * 0.25, box.y + box.height * 0.25,
                                     core::PointerButton::Left, core::PointerAction::Press, shift);
            runtime.update(window, 0.016f, 1, 1);
            compose(1);
            core::queuePointerMotion(window, box.x + box.width * 0.75, box.y + box.height * 0.75,
                                     core::PointerButton::Left, shift);
            runtime.update(window, 0.016f, 1, 1);
            compose(1);
            core::queuePointerButton(window, box.x + box.width * 0.75, box.y + box.height * 0.75,
                                     core::PointerButton::Left, core::PointerAction::Release, shift);
            runtime.update(window, 0.016f, 1, 1);
            near(plot.axes().x.range().min, 0.25);
            near(plot.axes().x.range().max, 0.75);
            near(plot.axes().y.range().min, -0.5);
            near(plot.axes().y.range().max, 0.5);
            compose(1);
            runtime.update(window, 0.016f, 1, 1);
            backend->beginFrame({window, core::window::nativeWindowInfo(window), 800, 600, 1});
            runtime.render(800, 600, 1);
            backend->present();
            require(glGetError() == GL_NO_ERROR, "OpenGL error");
            const auto revision = ui->find("chart.data")->gpuImageRevision;
            compose(1);
            require(ui->find("chart.data")->gpuImageRevision == revision, "idle re-rendered plot");
            plot.releaseGpu();
            runtime.shutdown();
            glFinish();
            std::cout
                << "plot_runtime_probe: pixels, state, DPI, capture, recompose, disabled, idle passed\n";
        } catch (const std::exception& error) {
            std::cerr << "plot_runtime_probe: " << error.what() << '\n';
            result = 1;
            runtime.shutdown();
        }
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
