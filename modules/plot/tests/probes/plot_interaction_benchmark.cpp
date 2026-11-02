#include "core/dsl_runtime.h"
#include "core/render/render_backend.h"
#include "core/window/window_backend.h"
#include "modules/plot/plot.h"
#include "modules/plot/plot3d.h"
#include "modules/plot/volume.h"

#if defined(EUI_RENDER_BACKEND_OPENGL)
#include <glad/glad.h>
#else
#include <vulkan/vulkan.h>
#endif

#if defined(EUI_WINDOW_BACKEND_SDL2)
#define SDL_MAIN_HANDLED
#include <SDL.h>
#if defined(EUI_RENDER_BACKEND_VULKAN)
#include <SDL_vulkan.h>
#endif
#else
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Clock = std::chrono::steady_clock;

double elapsedMs(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

double percentile(std::vector<double> values, double p) {
    std::sort(values.begin(), values.end());
    const auto index = std::min(values.size() - 1,
                                static_cast<std::size_t>(std::ceil(p * values.size()) - 1));
    return values[index];
}

void pumpEvents() {
#if defined(EUI_WINDOW_BACKEND_SDL2)
    SDL_Event event;
    while (SDL_PollEvent(&event)) {}
#else
    glfwPollEvents();
#endif
}

struct Context {
    core::window::Handle window = nullptr;
    std::unique_ptr<core::render::RenderBackend> backend;
    std::unique_ptr<core::render::ScopedRenderBackend> scope;
    core::dsl::Runtime runtime;
    core::dsl::Ui* ui = nullptr;
    modules::plot::Plot plot;
    std::vector<double> x;
    std::vector<double> y0;
    std::vector<double> y1;

    explicit Context(std::size_t count) {
        core::render::initializeRenderBackendLoader();
#if defined(EUI_WINDOW_BACKEND_SDL2)
        SDL_SetMainReady();
        if (SDL_Init(SDL_INIT_VIDEO) != 0)
            throw std::runtime_error("SDL initialization failed");
#else
        if (!glfwInit())
            throw std::runtime_error("GLFW initialization failed");
#endif
        core::window::WindowCreateRequest request;
        request.width = 1920;
        request.height = 1080;
        request.title = "Plot interaction benchmark";
        request.renderApi = core::render::windowRenderApi();
        window = core::window::createWindow(request);
        backend = core::render::createRenderBackend(window);
        if (!window || !backend || !backend->initialize())
            throw std::runtime_error("render backend initialization failed");
        backend->makeCurrent();
        scope = std::make_unique<core::render::ScopedRenderBackend>(*backend);
#if defined(EUI_RENDER_BACKEND_OPENGL)
#if defined(EUI_WINDOW_BACKEND_SDL2)
        SDL_GL_SetSwapInterval(0);
#else
        glfwSwapInterval(0);
#endif
#endif
        if (!runtime.initialize(window))
            throw std::runtime_error("runtime initialization failed");

        x.resize(count);
        y0.resize(count);
        y1.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            x[i] = 100.0 * static_cast<double>(i) / static_cast<double>(count - 1);
            y0[i] = std::sin(x[i]);
            y1[i] = std::cos(x[i] * 1.03);
        }
        modules::plot::Series first, second;
        first.data = modules::plot::Data(x, y0);
        first.style.color = {0.15f, 0.8f, 1.0f, 1.0f};
        second.data = modules::plot::Data(x, y1);
        second.style.color = {1.0f, 0.55f, 0.2f, 1.0f};
        plot.setSeries({std::move(first), std::move(second)});
    }

    void compose() {
        runtime.compose("interaction", 1920, 1080, [&](core::dsl::Ui& ui, const core::dsl::Screen&) {
            this->ui = &ui;
            plot.compose(ui, "chart", 1920, 1080, 1.0);
        });
    }

    void present() {
        runtime.update(window, 1.0f / 60.0f, 1.0f, 1.0f, true);
        backend->beginFrame({window, core::window::nativeWindowInfo(window), 1920, 1080, 1.0f});
        runtime.render(1920, 1080, 1.0f, {0.025f, 0.035f, 0.05f, 1.0f});
        backend->present();
#if defined(EUI_RENDER_BACKEND_OPENGL)
        glFinish();
#else
        if (vkQueueWaitIdle(reinterpret_cast<VkQueue>(backend->gpuDeviceInfo().graphicsQueue)) != VK_SUCCESS)
            throw std::runtime_error("Vulkan queue wait failed");
#endif
        pumpEvents();
    }

    ~Context() {
        plot.releaseGpu();
        runtime.shutdown();
#if defined(EUI_RENDER_BACKEND_OPENGL)
        glFinish();
#else
        if (backend)
            vkDeviceWaitIdle(reinterpret_cast<VkDevice>(backend->gpuDeviceInfo().device));
#endif
        scope.reset();
        backend.reset();
        core::window::destroyWindow(window);
#if defined(EUI_WINDOW_BACKEND_SDL2)
        SDL_Quit();
#else
        glfwTerminate();
#endif
    }
};

void printStats(const char* label, const std::vector<double>& values) {
    std::cout << "\"" << label << "\":{\"n\":" << values.size()
              << ",\"median_ms\":" << percentile(values, 0.5)
              << ",\"p95_ms\":" << percentile(values, 0.95)
              << ",\"p99_ms\":" << percentile(values, 0.99)
              << ",\"max_ms\":" << percentile(values, 1.0) << "}";
}
} // namespace

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--volume") {
        try {
            Context context(2);
            modules::plot::VolumeLayout layout{{21, 21, 21}, {0.1, 0.1, 0.1}, {-1, -1, -1}};
            auto samples = std::make_shared<std::vector<double>>();
            samples->reserve(layout.sampleCount());
            for (std::size_t z = 0; z < 21; ++z) {
                for (std::size_t y = 0; y < 21; ++y) {
                    for (std::size_t x = 0; x < 21; ++x) {
                        const modules::plot::Vec3 point{-1 + x * 0.1, -1 + y * 0.1, -1 + z * 0.1};
                        samples->push_back(std::exp(-modules::plot::dot(point, point) * 5));
                    }
                }
            }
            auto volume = std::make_shared<modules::plot::VolumeData>(layout, samples);
            auto layer = std::make_shared<modules::plot::VolumeLayer>();
            layer->data = volume;
            layer->referenceStep = 0.1;
            layer->transfer = modules::plot::TransferFunction({
                {0, {0, 0, 0, 0}}, {0.05, {0, 0, 1, 0}},
                {0.3, {0.1f, 0.6f, 1, 0.1f}}, {1, {1, 0.3f, 0, 0.5f}}});
            modules::plot::Scene3D scene;
            scene.bounds = layout.bounds();
            scene.showAxes = false;
            scene.volume = std::move(layer);
            modules::plot::Plot3D spatial;
            spatial.setScene(std::move(scene));
            spatial.setSettings({16 * 1024 * 1024, 4096, 0.16, 4096, {0.02f, 0.03f, 0.05f, 1.0f}});

            auto compose = [&] {
                context.runtime.compose("volume-interaction", 1920, 1080,
                    [&](core::dsl::Ui& ui, const core::dsl::Screen&) {
                        context.ui = &ui;
                        ui.stack("volume-panel")
                            .position(20, 20)
                            .size(512, 384)
                            .content([&] { spatial.compose(ui, "volume-plot", 512, 384, 1.0); })
                            .build();
                    });
            };
            for (int warmup = 0; warmup < 2; ++warmup) {
                compose();
                context.present();
            }
            context.runtime.update(context.window, 1.0f / 60.0f, 1.0f, 1.0f, true);
            const auto bounds = context.ui->find("volume-plot.input")->frame;
            const double pointerX = (bounds.x + bounds.width * 0.5) * 1.0;
            const double pointerY = (bounds.y + bounds.height * 0.5) * 1.0;
            core::queuePointerButton(context.window, pointerX, pointerY, core::PointerButton::Left,
                                     core::PointerAction::Press, {});
            context.runtime.update(context.window, 1.0f / 60.0f, 1.0f, 1.0f, true);
            compose();
            std::vector<double> dragFrames;
            std::size_t cameraChanges = 0;
            double previousYaw = spatial.view().camera.yaw;
            for (int step = 1; step <= 12; ++step) {
                const auto start = Clock::now();
                core::queuePointerMotion(context.window, pointerX + step * 4.0, pointerY,
                                         core::PointerButton::Left, {});
                context.runtime.update(context.window, 1.0f / 60.0f, 1.0f, 1.0f, true);
                if (spatial.view().camera.yaw != previousYaw)
                    ++cameraChanges;
                previousYaw = spatial.view().camera.yaw;
                compose();
                context.present();
                dragFrames.push_back(elapsedMs(start));
            }
            core::queuePointerButton(context.window, pointerX + 48.0, pointerY,
                                     core::PointerButton::Left, core::PointerAction::Release, {});
            context.runtime.update(context.window, 1.0f / 60.0f, 1.0f, 1.0f, true);
            const auto releaseStart = Clock::now();
            compose();
            context.present();
            const double releaseFullResolutionMs = elapsedMs(releaseStart);
            std::cout << "{\"mode\":\"volume_orbit_drag\",\"viewport\":[512,384],\"samples\":[21,21,21],\"step\":0.16,\"backend\":\""
#if defined(EUI_RENDER_BACKEND_OPENGL)
                      << "OpenGL"
#else
                      << "Vulkan"
#endif
                      << "\",\"camera_changes\":" << cameraChanges
                      << ",\"release_full_resolution_ms\":" << releaseFullResolutionMs << ',';
            printStats("drag_frame_end_to_end", dragFrames);
            std::cout << "}\n";
            if (cameraChanges != 12)
                throw std::runtime_error("synthetic volume drag did not reach Plot3D camera");
        } catch (const std::exception& error) {
            std::cerr << error.what() << '\n';
            return 1;
        }
        return 0;
    }
    const std::size_t count = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 125000;
    if (count < 2 || count > 1250000)
        return 2;
    try {
        Context context(count);
        for (int warmup = 0; warmup < 3; ++warmup) {
            context.compose();
            context.present();
        }
        std::vector<double> panCompose, panPresent, zoomCompose, zoomPresent;
        std::vector<double> dragFrames;
        constexpr int rounds = 20;
        for (int round = 0; round < rounds; ++round) {
            context.plot.pan(round % 2 ? 0.002 : -0.002, 0);
            auto start = Clock::now();
            context.compose();
            panCompose.push_back(elapsedMs(start));
            start = Clock::now();
            context.present();
            panPresent.push_back(elapsedMs(start));

            context.plot.zoom({0.5, 0.5}, round % 2 ? 1.02 : 1.0 / 1.02);
            start = Clock::now();
            context.compose();
            zoomCompose.push_back(elapsedMs(start));
            start = Clock::now();
            context.present();
            zoomPresent.push_back(elapsedMs(start));
        }

        context.compose();
        context.present();
        const auto inputBounds = context.ui->find("chart.input")->frame;
        const double pointerX = inputBounds.x + inputBounds.width * 0.5;
        const double pointerY = inputBounds.y + inputBounds.height * 0.5;
        core::queuePointerButton(context.window, pointerX, pointerY, core::PointerButton::Left,
                                 core::PointerAction::Press, {});
        context.runtime.update(context.window, 1.0f / 60.0f, 1.0f, 1.0f, true);
        for (int step = 1; step <= rounds; ++step) {
            const auto start = Clock::now();
            core::queuePointerMotion(context.window, pointerX + step * 3.0, pointerY,
                                     core::PointerButton::Left, {});
            context.runtime.update(context.window, 1.0f / 60.0f, 1.0f, 1.0f, true);
            context.compose();
            context.present();
            dragFrames.push_back(elapsedMs(start));
        }
        core::queuePointerButton(context.window, pointerX + rounds * 3.0, pointerY,
                                 core::PointerButton::Left, core::PointerAction::Release, {});
        context.runtime.update(context.window, 1.0f / 60.0f, 1.0f, 1.0f, true);
        context.compose();
        context.present();

        std::cout << "{\"backend\":\""
#if defined(EUI_RENDER_BACKEND_OPENGL)
                  << "OpenGL"
#else
                  << "Vulkan"
#endif
                  << "\",\"samples_per_series\":" << count << ",\"viewport\":[1920,1080],\"rounds\":"
                  << rounds << ',';
        printStats("pan_compose", panCompose);
        std::cout << ',';
        printStats("pan_present_and_gpu_wait", panPresent);
        std::cout << ',';
        printStats("zoom_compose", zoomCompose);
        std::cout << ',';
        printStats("zoom_present_and_gpu_wait", zoomPresent);
        std::cout << ',';
        printStats("drag_frame_end_to_end", dragFrames);
        std::cout << "}\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
