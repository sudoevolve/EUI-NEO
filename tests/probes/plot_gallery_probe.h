#pragma once
#include "core/dsl_runtime.h"
#include "core/render/render_backend.h"
#include "core/window/window_backend.h"
#include "modules/plot/gpu_scene3d.h"
#include <glad/glad.h>
#if defined(EUI_WINDOW_BACKEND_SDL2)
#define SDL_MAIN_HANDLED
#include <SDL.h>
#else
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#endif
#include <chrono>
#include <iostream>
#include <algorithm>

namespace {
void requireGallery(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
std::vector<std::uint8_t> readGallery(const modules::plot::GpuSceneRenderer3D& gpu) {
    const auto& image = gpu.image()->descriptor();
    GLuint fbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, image.texture, 0);
    std::vector<std::uint8_t> pixels(std::size_t(image.width) * image.height * 4);
    glReadPixels(0, 0, image.width, image.height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &fbo);
    return pixels;
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
    request.width = 1680;
    request.height = 980;
    request.title = "Plot gallery GPU probe";
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
            std::cout << "GPU " << glGetString(GL_RENDERER) << '\n';
            runtime.compose("gallery", 1680, 980, app::compose);
            for (std::size_t i = 0; i < app::panels.size(); ++i) {
                auto& plot = *app::panels[i].plot;
                auto scene = plot.scene();
                for (auto& object : scene.objects)
                    if (object.material.scalarColors)
                        object.material.colorScale = plot.view().colorScale;
                SceneRenderer3D cpu(scene);
                GpuSceneRenderer3D gpu;
                for (auto projection : {Projection3D::Orthographic, Projection3D::Perspective}) {
                    auto camera = plot.view().camera;
                    camera.projection = projection;
                    camera.zoom(0.65);
                    auto expected = cpu.render(camera, 96, 72);
                    requireGallery(gpu.render(cpu, 1, camera, 96, 72),
                                   "gallery unexpectedly fell back to CPU");
                    auto actual = readGallery(gpu);
                    std::size_t errors = 0;
                    double absolute = 0;
                    for (std::size_t p = 0; p < actual.size(); p += 4) {
                        int worst = 0;
                        for (int c = 0; c < 4; ++c) {
                            int delta = std::abs(int(actual[p + c]) - int(expected.rgba[p + c]));
                            worst = std::max(worst, delta);
                            absolute += delta;
                        }
                        if (worst > 4)
                            ++errors;
                    }
                    std::cout << "panel " << i << " projection " << int(projection) << " changed_pixels "
                              << errors << " mean_channel_error " << absolute / actual.size() << '\n';
                    requireGallery(errors <= 96 * 72 / 100 && absolute / actual.size() < 0.8,
                                   "GPU/reference mismatch beyond silhouette tolerance");
                }
                requireGallery(gpu.uploads() == 1, "camera change uploaded BVH again");
                gpu.release();
            }
            auto compose = [&] { runtime.compose("gallery", 1680, 980, app::compose); };
            for (double dpi : {1., 2.}) {
                app::dpi = dpi;
                for (auto& panel : app::panels) {
                    panel.plot->resetView();
                    auto view = panel.plot->view();
                    view.camera.zoom(0.4);
                    panel.plot->setView(view);
                }
                compose();
                glFinish();
                std::vector<double> times, gpuTimes;
                for (int iteration = 0; iteration < 12; ++iteration) {
                    GLuint query = 0;
                    glGenQueries(1, &query);
                    auto start = std::chrono::steady_clock::now();
                    glBeginQuery(GL_TIME_ELAPSED, query);
                    for (auto& panel : app::panels) {
                        auto view = panel.plot->view();
                        view.camera.orbit(0.01, 0.002);
                        panel.plot->setView(view);
                    }
                    compose();
                    glEndQuery(GL_TIME_ELAPSED);
                    glFinish();
                    times.push_back(
                        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
                            .count());
                    GLuint64 nanoseconds = 0;
                    glGetQueryObjectui64v(query, GL_QUERY_RESULT, &nanoseconds);
                    gpuTimes.push_back(nanoseconds / 1e6);
                    glDeleteQueries(1, &query);
                }
                std::sort(times.begin(), times.end());
                std::sort(gpuTimes.begin(), gpuTimes.end());
                std::cout << "gallery " << app::panels.size() << " dpi " << dpi
                          << " zoom 2.5x wall_median_ms " << times[6] << " wall_max_ms " << times.back()
                          << " gpu_median_ms " << gpuTimes[6] << '\n';
                // Normal interaction invalidates one panel. Measure each to include the most expensive.
                double slowestPanelMedian = 0;
                for (auto& panel : app::panels) {
                    std::vector<double> single;
                    for (int iteration = 0; iteration < 5; ++iteration) {
                        const auto start = std::chrono::steady_clock::now();
                        auto view = panel.plot->view();
                        view.camera.zoom(0.99);
                        panel.plot->setView(view);
                        compose();
                        glFinish();
                        single.push_back(std::chrono::duration<double, std::milli>(
                                             std::chrono::steady_clock::now() - start)
                                             .count());
                    }
                    std::sort(single.begin(), single.end());
                    slowestPanelMedian = std::max(slowestPanelMedian, single[2]);
                }
                std::cout << "slowest_single_panel_median_ms " << slowestPanelMedian << '\n';
                // Reference baseline at the same physical panel dimensions, with all panels rendered.
                const int columns = app::panels.size() == 6 ? 3 : 4;
                const double panelWidth = (1680. - 12. * (columns + 1)) / columns - 8;
                const double panelHeight = (980. - (columns == 3 ? 110. : 138.) - 36.) / 2. - 114.;
                auto start = std::chrono::steady_clock::now();
                for (auto& panel : app::panels)
                    panel.plot->render(std::uint32_t(std::ceil(panelWidth * dpi)),
                                       std::uint32_t(std::ceil(panelHeight * dpi)));
                std::cout << "cpu_reference_ms "
                          << std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() -
                                                                       start)
                                 .count()
                          << '\n';
            }
            requireGallery(glGetError() == GL_NO_ERROR, "gallery OpenGL error");
        } catch (const std::exception& e) {
            std::cerr << e.what() << '\n';
            result = 1;
        }
        scientific_gallery::release(app::panels);
        runtime.shutdown();
        glFinish();
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
