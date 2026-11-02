#include "modules/plot/contour.h"
#include "modules/plot/renderer.h"

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

#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
bool initializeWindowSystem() {
#if defined(EUI_WINDOW_BACKEND_SDL2)
    SDL_SetMainReady();
    return SDL_Init(SDL_INIT_VIDEO) == 0;
#else
    return glfwInit() == GLFW_TRUE;
#endif
}
void shutdownWindowSystem() {
#if defined(EUI_WINDOW_BACKEND_SDL2)
    SDL_Quit();
#else
    glfwTerminate();
#endif
}
} // namespace

int main() {
    using namespace modules::plot;
    core::render::initializeRenderBackendLoader();
    if (!initializeWindowSystem())
        return 1;
    core::window::WindowCreateRequest request;
    request.width = 240;
    request.height = 180;
    request.title = "Contour runtime probe";
    request.renderApi = core::window::RenderApi::OpenGL;
    const auto window = core::window::createWindow(request);
    auto backend = core::render::createRenderBackend(window);
    if (!window || !backend || !backend->initialize()) {
        shutdownWindowSystem();
        return 2;
    }
    int result = 0;
    {
        backend->makeCurrent();
        core::render::ScopedRenderBackend scope(*backend);
        core::dsl::Runtime runtime;
        runtime.initialize(window);
        try {
            const ScalarField field(2, 2, {0, 0, 1, 1}, {0, 1}, {0, 1}, FieldOrigin::LowerLeft,
                                    FieldSampling::GridPoints);
            Axes axes;
            axes.x.setRange({0, 1});
            axes.y.setRange({0, 1});
            Style fill;
            fill.color = {1, 0, 0, 1};
            std::vector<Batch> fillBatches;
            const auto bands = filledContours(field, {0.5});
            for (const auto& triangle : bands[0].triangles) {
                const auto vertices =
                    pathGeometry({triangle[0], triangle[1], triangle[2]}, fill, true, true, axes, {0, 0, 100, 100});
                require(!vertices.empty(), "filled contour produced no vertices");
                fillBatches.push_back({vertices, fill.color});
            }
            Renderer renderer;
            renderer.render(fillBatches, 100, 100);
            GLuint readback = 0;
            glGenFramebuffers(1, &readback);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, readback);
            glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                   renderer.image()->descriptor().texture, 0);
            std::vector<unsigned char> pixels(100 * 100 * 4);
            glReadPixels(0, 0, 100, 100, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            std::size_t redPixels = 0;
            for (std::size_t index = 0; index < pixels.size(); index += 4)
                redPixels += pixels[index] > 240 && pixels[index + 1] < 10;
            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
            glDeleteFramebuffers(1, &readback);
            require(redPixels > 0, "filled contour pixels missing");

            Style line;
            line.color = {1, 1, 1, 1};
            std::vector<Batch> lineBatches;
            const auto lines = marchingSquares(field, {0.5});
            for (const auto& segment : lines[0].segments)
                lineBatches.push_back(
                    {pathGeometry({segment.from, segment.to}, line, false, false, axes, {0, 0, 100, 100}), line.color});
            renderer.render(lineBatches, 100, 100);
            glGenFramebuffers(1, &readback);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, readback);
            glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                   renderer.image()->descriptor().texture, 0);
            glReadPixels(0, 0, 100, 100, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            std::size_t whitePixels = 0;
            for (std::size_t index = 0; index < pixels.size(); index += 4)
                whitePixels += pixels[index] > 240 && pixels[index + 1] > 240;
            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
            glDeleteFramebuffers(1, &readback);
            renderer.release();
            require(whitePixels > 0, "contour line pixels missing");
        } catch (const std::exception& error) {
            std::cerr << "plot_contour_runtime_probe: " << error.what() << '\n';
            result = 1;
        }
        runtime.shutdown();
    }
    backend.reset();
    core::window::destroyWindow(window);
    shutdownWindowSystem();
    return result;
}