#include "core/dsl_runtime.h"
#include "core/render/render_backend.h"
#include "core/window/window_backend.h"
#include "modules/plot/contour.h"
#include "modules/plot/plot.h"
#include "modules/plot/plot3d.h"
#include "modules/plot/gpu_scene3d.h"
#include "modules/plot/vector.h"
#include "modules/plot/volume.h"

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
    request.title = "Spatial plot runtime probe";
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
            auto geometry = std::make_shared<Geometry3D>();
            geometry->positions = {{-1, -1, 0}, {1, -1, 0}, {1, 1, 0}, {-1, 1, 0}};
            geometry->triangles = {{0, 1, 2}, {0, 2, 3}};
            Material3D material;
            material.color = {1, 0, 0, 1};
            material.lighting = false;
            Scene3D scene;
            scene.showAxes = false;
            scene.objects = {{geometry, material, true}};
            Camera3D camera;
            camera.yaw = 0;
            camera.pitch = 0;
            camera.distance = 4;
            camera.verticalSpan = 2;
            const auto frame = SceneRenderer3D(scene).render(camera, 64, 64);
            Renderer uploader;
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_GREATER);
            glDepthMask(GL_FALSE);
            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT);
            glFrontFace(GL_CW);
            glEnable(GL_SCISSOR_TEST);
            glScissor(1, 2, 3, 4);
            glViewport(7, 8, 120, 130);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 7);
            glPixelStorei(GL_UNPACK_SKIP_ROWS, 2);
            glPixelStorei(GL_UNPACK_SKIP_PIXELS, 1);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 8);
            uploader.uploadRgba(frame.width, frame.height, frame.rgba);
            GpuSceneRenderer3D gpu;
            SceneRenderer3D reference(scene);
            require(gpu.render(reference, 1, camera, 64, 64), "GPU render fell back");
            GLint value = 0;
            GLint viewport[4]{};
            glGetIntegerv(GL_VIEWPORT, viewport);
            require(viewport[0] == 7 && viewport[3] == 130, "viewport leaked");
            glGetIntegerv(GL_DEPTH_FUNC, &value);
            require(value == GL_GREATER && glIsEnabled(GL_DEPTH_TEST), "depth state leaked");
            glGetIntegerv(GL_CULL_FACE_MODE, &value);
            require(value == GL_FRONT && glIsEnabled(GL_CULL_FACE), "cull state leaked");
            glGetIntegerv(GL_FRONT_FACE, &value);
            require(value == GL_CW, "winding state leaked");
            glGetIntegerv(GL_UNPACK_ROW_LENGTH, &value);
            require(value == 7, "row length leaked");
            glGetIntegerv(GL_UNPACK_SKIP_ROWS, &value);
            require(value == 2, "unpack rows leaked");
            glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &value);
            require(value == 1, "unpack pixels leaked");
            glGetIntegerv(GL_UNPACK_ALIGNMENT, &value);
            require(value == 8, "unpack alignment leaked");
            require(glIsEnabled(GL_SCISSOR_TEST), "scissor leaked");
            glDisable(GL_SCISSOR_TEST);
            glDisable(GL_CULL_FACE);
            glDisable(GL_DEPTH_TEST);
            glDepthMask(GL_TRUE);
            glDepthFunc(GL_LESS);
            glCullFace(GL_BACK);
            glFrontFace(GL_CCW);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
            glPixelStorei(GL_UNPACK_SKIP_ROWS, 0);
            glPixelStorei(GL_UNPACK_SKIP_PIXELS, 0);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
            near(gpu.depthAt(32, 32), 4);
            const GLuint gpuTexture = gpu.image()->descriptor().texture;
            require(gpu.render(reference, 1, camera, 64, 64), "GPU rerender fell back");
            require(gpu.uploads() == 1 && gpu.image()->descriptor().texture == gpuTexture,
                    "camera render reallocated GPU geometry/texture");
            gpu.release();
            glFinish();
            require(!glIsTexture(gpuTexture), "GPU ray texture leaked");
            GLuint readback = 0;
            glGenFramebuffers(1, &readback);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, readback);
            glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                   uploader.image()->descriptor().texture, 0);
            unsigned char pixel[4]{};
            glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
            require(pixel[0] == 255 && pixel[1] == 0, "3D texture pixel mismatch");
            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
            glDeleteFramebuffers(1, &readback);
            const GLuint reused = uploader.image()->descriptor().texture;
            uploader.uploadRgba(frame.width, frame.height, frame.rgba);
            require(uploader.image()->descriptor().texture == reused, "same-size upload reallocated");
            uploader.release();
            glFinish();
            require(!glIsTexture(reused), "upload texture leaked");
            VolumeLayout sampleLayout{{9, 9, 9}, {0.25, 0.25, 0.25}, {-1, -1, -1}};
            auto samples = std::make_shared<std::vector<double>>();
            for (int z = 0; z < 9; ++z)
                for (int y = 0; y < 9; ++y)
                    for (int x = 0; x < 9; ++x) {
                        const Vec3 p{-1 + x * 0.25, -1 + y * 0.25, -1 + z * 0.25};
                        samples->push_back(dot(p, p));
                    }
            auto sampleVolume = std::make_shared<VolumeData>(sampleLayout, samples);
            for (int kind = 0; kind < 3; ++kind) {
                Scene3D volumeScene;
                volumeScene.showAxes = false;
                if (kind == 2) {
                    auto volumeLayer = std::make_shared<VolumeLayer>();
                    volumeLayer->data = sampleVolume;
                    volumeLayer->transfer = TransferFunction({{0, {1, 0, 0, 0.5f}}, {0.7, {0, 0, 1, 0}}});
                    volumeScene.volume = volumeLayer;
                } else {
                    auto mesh = kind == 0 ? volumeSlice(*sampleVolume, SliceLink(sampleLayout).plane(2))
                                          : isoSurface(*sampleVolume, 0.38);
                    volumeScene.objects = {{std::make_shared<Geometry3D>(std::move(mesh)), material, true}};
                }
                SceneRenderer3D volumeReference(volumeScene);
                const auto expected = volumeReference.render(camera, 64, 64);
                uploader.uploadRgba(64, 64, expected.rgba);
                glGenFramebuffers(1, &readback);
                glBindFramebuffer(GL_READ_FRAMEBUFFER, readback);
                glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                       uploader.image()->descriptor().texture, 0);
                std::vector<std::uint8_t> actual(expected.rgba.size());
                glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, actual.data());
                require(actual == expected.rgba, "slice/isosurface/volume GPU pixels differ from reference");
                require(gpu.render(volumeReference, kind + 2, camera, 64, 64), "volume GPU fallback");
                glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                       gpu.image()->descriptor().texture, 0);
                glReadPixels(0, 0, 64, 64, GL_RGBA, GL_UNSIGNED_BYTE, actual.data());
                int maxError = 0;
                std::size_t errors = 0;
                for (std::size_t i = 0; i < actual.size(); ++i) {
                    const int delta = std::abs(int(actual[i]) - int(expected.rgba[i]));
                    maxError = std::max(maxError, delta);
                    errors += delta > 2;
                }
                std::cout << "GPU scene " << kind << " max channel error " << maxError
                          << " channels over tolerance " << errors << '\n';
                require(maxError <= 2, "GPU volume/slice/isosurface reference mismatch");
                require(actual[(32 * 64 + 32) * 4] > 30, "empty volume image");
                glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
                glDeleteFramebuffers(1, &readback);
                uploader.release();
                gpu.release();
            }
            for (int iteration = 0; iteration < 32; ++iteration) {
                Renderer temporary;
                temporary.uploadRgba(64, 64, frame.rgba);
                const GLuint texture = temporary.image()->descriptor().texture;
                temporary.release();
                glFinish();
                require(!glIsTexture(texture), "repeated destruction leaked texture");
            }
            Plot3D plot;
            plot.setScene(scene);
            ViewState3D view;
            view.camera = camera;
            plot.setView(view);
            core::dsl::Ui* ui = nullptr;
            auto compose = [&](float dpi) {
                runtime.compose("spatial", 400, 300, [&](core::dsl::Ui& value, const core::dsl::Screen&) {
                    ui = &value;
                    plot.compose(value, "chart", 400, 300, dpi);
                });
            };
            for (float dpi : {1.f, 2.f}) {
                plot.setView(view);
                compose(dpi);
                runtime.update(window, 0.016f, 1, dpi);
                const auto bounds = ui->find("chart.input")->frame;
                const double x = (bounds.x + bounds.width / 2) * dpi,
                             y = (bounds.y + bounds.height / 2) * dpi;
                auto click = [&](double px, double py, core::KeyModifiers mods) {
                    core::queuePointerButton(window, px, py, core::PointerButton::Left,
                                             core::PointerAction::Press, mods);
                    runtime.update(window, 0.016f, 1, dpi);
                    compose(dpi);
                    core::queuePointerButton(window, px, py, core::PointerButton::Left,
                                             core::PointerAction::Release, mods);
                    runtime.update(window, 0.016f, 1, dpi);
                };
                click(x, y, {});
                require(plot.selection().has_value(), "Runtime 3D selection failed");
                near(plot.selection()->position.z, 0);
                core::KeyModifiers control;
                control.control = true;
                click(x - 30 * dpi, y, control);
                click(x + 30 * dpi, y, control);
                require(plot.measurement().has_value(), "Runtime measure failed");
                near(plot.measurement()->distance, 0.4);
                core::queuePointerButton(window, x, y, core::PointerButton::Left, core::PointerAction::Press,
                                         {});
                runtime.update(window, 0.016f, 1, dpi);
                compose(dpi);
                core::queuePointerMotion(window, x + 30 * dpi, y, core::PointerButton::Left, {});
                runtime.update(window, 0.016f, 1, dpi);
                near(plot.view().camera.yaw, -0.3);
                compose(dpi);
                core::queuePointerButton(window, 900 * dpi, 700 * dpi, core::PointerButton::Left,
                                         core::PointerAction::Release, {});
                runtime.update(window, 0.016f, 1, dpi);
                core::queuePointerMotion(window, x, y, {}, {});
                runtime.update(window, 0.016f, 1, dpi);
                near(plot.view().camera.yaw, -0.3);
                core::queueScrollInput(window, 0, 1);
                runtime.update(window, 0.016f, 1, dpi);
                near(plot.view().camera.verticalSpan, 2 * std::exp(-0.12));
                core::queueScrollInput(window, 0, -1);
                runtime.update(window, 0.016f, 1, dpi);
                near(plot.view().camera.verticalSpan, 2);
                plot.setView(view);
                compose(dpi);
                runtime.update(window, 0.016f, 1, dpi);
                core::KeyModifiers shift;
                shift.shift = true;
                core::queuePointerButton(window, x, y, core::PointerButton::Left, core::PointerAction::Press,
                                         shift);
                runtime.update(window, 0.016f, 1, dpi);
                core::queuePointerMotion(window, x + 30 * dpi, y, core::PointerButton::Left, shift);
                runtime.update(window, 0.016f, 1, dpi);
                near(plot.view().camera.target.x, -0.2);
                plot.setEnabled(false);
                compose(dpi);
                core::queuePointerMotion(window, x + 60 * dpi, y, core::PointerButton::Left, shift);
                runtime.update(window, 0.016f, 1, dpi);
                near(plot.view().camera.target.x, -0.2);
                core::queuePointerButton(window, x, y, core::PointerButton::Left,
                                         core::PointerAction::Release, shift);
                runtime.update(window, 0.016f, 1, dpi);
                plot.setEnabled(true);
                compose(dpi);
                runtime.update(window, 0.016f, 1, dpi);
                const auto revision = ui->find("chart.data")->gpuImageRevision;
                compose(dpi);
                require(ui->find("chart.data")->gpuImageRevision == revision, "static 3D redrew");
                require(ui->find("chart.data")->gpuImage->descriptor().width == int(400 * dpi),
                        "DPI texture width");
            }
            plot.setView(view);
            compose(1);
            runtime.update(window, 0.016f, 1, 1);
            backend->beginFrame({window, core::window::nativeWindowInfo(window), 800, 600, 1});
            runtime.render(800, 600, 1);
            backend->present();
            require(glGetError() == GL_NO_ERROR, "OpenGL error");
            auto layer = std::make_shared<VolumeLayer>();
            layer->data = std::make_shared<VolumeData>(VolumeLayout{{2, 2, 2}, {2, 2, 2}, {-1, -1, -1}},
                                                       std::make_shared<const std::vector<double>>(8, 1));
            scene.objects.clear();
            scene.volume = layer;
            plot.setScene(scene);
            compose(1);
            const auto revision = ui->find("chart.data")->gpuImageRevision;
            layer->data->invalidate({{0, 0, 0}, {1, 1, 1}});
            compose(1);
            require(ui->find("chart.data")->gpuImageRevision > revision, "volume change not rendered");
            plot.releaseGpu();
            runtime.shutdown();
            glFinish();
            std::cout << "plot_spatial_runtime_probe: pixels, GL state, resources, Runtime input, DPI, idle, "
                         "volume revision passed\n";
        } catch (const std::exception& error) {
            std::cerr << "plot_spatial_runtime_probe: " << error.what() << '\n';
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
