#include "core/dsl_runtime.h"
#include "core/render/render_backend.h"
#include "core/window/window_backend.h"

#if defined(EUI_WINDOW_BACKEND_SDL2)
#ifndef SDL_MAIN_HANDLED
#define SDL_MAIN_HANDLED
#endif
#include <SDL.h>
#else
#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <vector>

namespace {

bool initializeWindowSystem() {
#if defined(EUI_WINDOW_BACKEND_SDL2)
    SDL_SetMainReady();
    return SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0;
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

bool near(float value, float expected, float tolerance = 0.01f) {
    return std::fabs(value - expected) <= tolerance;
}

class RecordingBackend final : public core::render::RenderBackend {
public:
    bool initialize() override { return true; }
    bool valid() const override { return true; }
    void makeCurrent() override {}
    void beginFrame(const core::render::RenderSurface&) override {}
    void present() override {}
    bool ensureRenderCache(int, int) override { return true; }
    bool renderCacheWasRecreated() const override { return false; }
    void releaseRenderCache() override {}
    void beginRenderCacheFrame(int, int, const std::vector<core::Rect>& dirty) override {
        lastDirty = dirty;
    }
    void endRenderCacheFrame() override {}
    void blitRenderCache(int, int, core::render::RenderCacheBlitMode,
                         const std::vector<core::Rect>&) override {}
    void clear(const core::Color&) override {}
    void setScissor(bool enabled, const core::Rect& rect, int) override {
        if (enabled) scissors.push_back(rect);
    }
    void prepareBackdropBlur(const core::Rect&, float, int, int) override {}
    void drawRoundedRect(const core::render::RoundedRectDrawCommand&, int, int) override {}
    void drawPolygon(const core::render::PolygonDrawCommand&, int, int) override {}
    void drawText(const core::render::TextDrawCommand&, int, int) override {}

    ShaderToyHandle createShaderToy(const core::render::ShaderToyGraph&,
                                    core::render::ShaderToyError* error) override {
        ++creates;
        if (error != nullptr) *error = {};
        return &toyHandle;
    }
    TextureHandle renderShaderToy(ShaderToyHandle,
                                  const core::render::ShaderToyGraph&,
                                  int width,
                                  int height,
                                  const core::render::ShaderToyFrameData& frame,
                                  bool paused,
                                  bool reset,
                                  core::render::ShaderToyError* error) override {
        ++renders;
        widths.push_back(width);
        heights.push_back(height);
        frames.push_back(frame);
        pausedValues.push_back(paused);
        resetValues.push_back(reset);
        if (error != nullptr) *error = {};
        return &textureHandle;
    }
    void destroyShaderToy(ShaderToyHandle) override { ++destroys; }
    void drawTexture(TextureHandle,
                     const float* vertices,
                     std::size_t count,
                     const core::Color& tint,
                     const core::Rect& rect,
                     float radius,
                     float,
                     int,
                     int) override {
        if (count == 42) {
            lastVertices.assign(vertices, vertices + count);
            lastTint = tint;
            lastRect = rect;
            lastRadius = radius;
        }
    }

    LayerHandle createLayer(int, int) override {
        ++layersCreated;
        return reinterpret_cast<LayerHandle>(
            static_cast<std::uintptr_t>(layersCreated + 1));
    }
    bool resizeLayer(LayerHandle, int, int) override { return true; }
    void destroyLayer(LayerHandle) override { ++layersDestroyed; }
    bool beginLayerFrame(LayerHandle, int, int) override { return true; }
    void endLayerFrame() override {}
    TextureHandle layerTexture(LayerHandle) override { return &layerTextureValue; }

    int creates = 0;
    int renders = 0;
    int destroys = 0;
    int layersCreated = 0;
    int layersDestroyed = 0;
    int toyHandle = 1;
    int textureHandle = 2;
    int layerTextureValue = 3;
    float lastRadius = 0.0f;
    core::Color lastTint{};
    core::Rect lastRect{};
    std::vector<float> lastVertices;
    std::vector<int> widths;
    std::vector<int> heights;
    std::vector<bool> pausedValues;
    std::vector<bool> resetValues;
    std::vector<core::render::ShaderToyFrameData> frames;
    std::vector<core::Rect> scissors;
    std::vector<core::Rect> lastDirty;
};

core::render::ShaderToyGraph graph() {
    core::render::ShaderToyGraph result;
    result.addPass("image", "unused.frag");
    return result;
}

bool layoutParticipation() {
    core::dsl::Ui ui;
    ui.begin("layout");
    ui.row("row").position(10.0f, 10.0f).size(180.0f, 50.0f).gap(6.0f).content([&] {
        ui.rect("row.before").size(30.0f, 20.0f).build();
        ui.shadertoy("row.toy").graph(graph()).size(40.0f, 20.0f).build();
    }).build();
    ui.column("column").position(10.0f, 70.0f).size(80.0f, 100.0f).gap(4.0f).content([&] {
        ui.rect("column.before").size(20.0f, 20.0f).build();
        ui.shadertoy("column.toy").graph(graph()).size(30.0f, 25.0f).build();
    }).build();
    ui.stack("stack").position(100.0f, 70.0f).size(80.0f, 60.0f).content([&] {
        ui.shadertoy("stack.toy").graph(graph()).position(7.0f, 9.0f).size(30.0f, 20.0f).build();
    }).build();
    ui.flow("flow").position(10.0f, 180.0f).size(90.0f, 70.0f).gap(4.0f).content([&] {
        ui.rect("flow.before").size(50.0f, 20.0f).build();
        ui.shadertoy("flow.toy").graph(graph()).size(50.0f, 20.0f).build();
    }).build();
    ui.end();
    ui.layout(240.0f, 270.0f);

    const core::dsl::Element* rowBefore = ui.find("row.before");
    const core::dsl::Element* rowToy = ui.find("row.toy");
    const core::dsl::Element* columnBefore = ui.find("column.before");
    const core::dsl::Element* columnToy = ui.find("column.toy");
    const core::dsl::Element* stack = ui.find("stack");
    const core::dsl::Element* stackToy = ui.find("stack.toy");
    const core::dsl::Element* flowBefore = ui.find("flow.before");
    const core::dsl::Element* flowToy = ui.find("flow.toy");
    return rowBefore && rowToy && columnBefore && columnToy && stack && stackToy &&
           flowBefore && flowToy &&
           rowToy->kind == core::dsl::ElementKind::Shadertoy &&
           rowToy->frame.x > rowBefore->frame.x &&
           columnToy->frame.y > columnBefore->frame.y &&
           stackToy->frame.x >= stack->frame.x && stackToy->frame.y >= stack->frame.y &&
           flowToy->frame.y > flowBefore->frame.y &&
           near(rowToy->frame.width, 40.0f) && near(columnToy->frame.height, 25.0f);
}

void composeToy(core::dsl::Runtime& runtime, bool paused) {
    runtime.compose("runtime", 100.0f, 100.0f, [&](core::dsl::Ui& ui, const core::dsl::Screen&) {
        ui.stack("clip")
            .position(5.0f, 6.0f)
            .size(60.0f, 50.0f)
            .clip()
            .content([&] {
                ui.shadertoy("toy")
                    .graph(graph())
                    .position(10.0f, 8.0f)
                    .size(40.0f, 20.0f)
                    .radius(4.0f)
                    .opacity(0.5f)
                    .translate(3.0f, 4.0f)
                    .transformedHitTest()
                    .paused(paused)
                    .build();
            })
            .build();
    });
}

bool runtimeFrameAndComposition(core::window::Handle window) {
    RecordingBackend backend;
    core::render::ScopedRenderBackend scoped(backend);
    core::dsl::Runtime runtime;
    composeToy(runtime, false);
    if (!runtime.update(window, 0.1f, 1.0f, 2.0f, false) || !runtime.isAnimating()) {
        return false;
    }
    runtime.render(200, 200, 2.0f, {0.0f, 0.0f, 0.0f, 1.0f});
    runtime.requestFullPaint();
    runtime.render(200, 200, 2.0f, {0.0f, 0.0f, 0.0f, 1.0f});
    if (backend.renders != 2 || backend.frames.size() != 2 ||
        backend.frames[0].frame != 0 || backend.frames[1].frame != 0 ||
        backend.frames[0].frameToken != backend.frames[1].frameToken ||
        backend.widths[0] != 80 || backend.heights[0] != 40 ||
        !near(backend.lastTint.a, 0.5f) || !near(backend.lastRadius, 8.0f) ||
        backend.lastVertices.size() != 42 || backend.scissors.empty()) {
        return false;
    }
    float minX = backend.lastVertices[0];
    float minY = backend.lastVertices[1];
    for (std::size_t offset = 0; offset < backend.lastVertices.size(); offset += 7) {
        minX = std::min(minX, backend.lastVertices[offset]);
        minY = std::min(minY, backend.lastVertices[offset + 1]);
    }
    if (!near(minX, backend.lastRect.x + 6.0f) ||
        !near(minY, backend.lastRect.y + 8.0f)) {
        return false;
    }

    runtime.update(window, 0.1f, 1.0f, 2.0f, false);
    runtime.render(200, 200, 2.0f, {0.0f, 0.0f, 0.0f, 1.0f});
    if (backend.frames.back().frame != 1 ||
        backend.frames.back().frameToken == backend.frames.front().frameToken) {
        return false;
    }

    composeToy(runtime, true);
    const bool pausedPaint = runtime.update(window, 5.0f, 1.0f, 2.0f, false);
    const bool pausedAnimating = runtime.isAnimating();
    const bool idlePaint = runtime.update(window, 5.0f, 1.0f, 2.0f, false);
    runtime.shutdown(false);
    return !pausedPaint && !pausedAnimating && !idlePaint &&
           backend.destroys == 1;
}

void composeLifecycleToy(core::dsl::Runtime& runtime,
                         bool includeToy,
                         float width) {
    runtime.compose("lifecycle", 160.0f, 120.0f,
                    [&](core::dsl::Ui& ui, const core::dsl::Screen&) {
        ui.stack("page").size(160.0f, 120.0f).content([&] {
            if (includeToy) {
                ui.shadertoy("page.toy")
                    .graph(graph())
                    .size(width, 30.0f)
                    .build();
            }
        }).build();
    });
}

bool repeatedLifecycle(core::window::Handle window) {
    RecordingBackend backend;
    core::render::ScopedRenderBackend scoped(backend);
    core::dsl::Runtime runtime;
    for (int cycle = 0; cycle < 3; ++cycle) {
        const float width = 40.0f + static_cast<float>(cycle * 10);
        composeLifecycleToy(runtime, true, width);
        runtime.update(window, 0.1f, 1.0f, 1.0f, false);
        runtime.render(160, 120, 1.0f, {});
        if (backend.creates != cycle + 1 || backend.destroys != cycle ||
            backend.widths.back() != static_cast<int>(width) ||
            !backend.resetValues.back()) {
            return false;
        }

        composeLifecycleToy(runtime, false, width);
        runtime.update(window, 0.0f, 1.0f, 1.0f, false);
        if (backend.destroys != cycle + 1) return false;
    }

    composeLifecycleToy(runtime, true, 70.0f);
    runtime.update(window, 0.1f, 1.0f, 1.0f, false);
    runtime.render(160, 120, 1.0f, {});
    if (backend.creates != 4 || backend.destroys != 3) return false;
    runtime.shutdown(false);
    return backend.destroys == 4;
}

void composeLayerTree(core::dsl::Runtime& runtime, bool includeToy) {
    runtime.compose("layers", 160.0f, 120.0f, [&](core::dsl::Ui& ui, const core::dsl::Screen&) {
        ui.stack("root").size(160.0f, 120.0f).content([&] {
            ui.stack("candidate").position(10.0f, 10.0f).size(120.0f, 90.0f).content([&] {
                for (int index = 0; index < 9; ++index) {
                    ui.rect("rect." + std::to_string(index))
                        .position(static_cast<float>((index % 3) * 35),
                                  static_cast<float>((index / 3) * 25))
                        .size(30.0f, 20.0f)
                        .build();
                }
                if (includeToy) {
                    ui.shadertoy("layer.toy")
                        .graph(graph())
                        .position(5.0f, 5.0f)
                        .size(40.0f, 30.0f)
                        .paused()
                        .build();
                }
            }).build();
        }).build();
    });
}

bool retainedLayerBlocker(core::window::Handle window) {
    RecordingBackend staticBackend;
    {
        core::render::ScopedRenderBackend scoped(staticBackend);
        core::dsl::Runtime runtime;
        composeLayerTree(runtime, false);
        runtime.update(window, 0.0f, 1.0f, 1.0f, false);
        runtime.render(160, 120, 1.0f, {});
        runtime.requestFullPaint();
        runtime.render(160, 120, 1.0f, {});
        runtime.shutdown(false);
    }
    RecordingBackend dynamicBackend;
    {
        core::render::ScopedRenderBackend scoped(dynamicBackend);
        core::dsl::Runtime runtime;
        composeLayerTree(runtime, true);
        runtime.update(window, 0.0f, 1.0f, 1.0f, false);
        runtime.render(160, 120, 1.0f, {});
        runtime.requestFullPaint();
        runtime.render(160, 120, 1.0f, {});
        runtime.shutdown(false);
    }
    // Shadertoy blocks its containing layer, while static sibling runs remain cacheable.
    return staticBackend.layersCreated == dynamicBackend.layersCreated + 1;
}

} // namespace

int main() {
    if (!layoutParticipation()) return 1;
    if (!initializeWindowSystem()) return 2;
    core::window::WindowCreateRequest request;
    request.width = 200;
    request.height = 200;
    request.title = "Shadertoy Runtime Probe";
    request.renderApi = core::render::windowRenderApi();
    core::window::Handle window = core::window::createWindow(request);
    if (window == nullptr) {
        shutdownWindowSystem();
        return 3;
    }
    const bool runtimeOk = runtimeFrameAndComposition(window);
    const bool retainedOk = retainedLayerBlocker(window);
    const bool lifecycleOk = repeatedLifecycle(window);
    core::window::destroyWindow(window);
    shutdownWindowSystem();
    return runtimeOk && retainedOk && lifecycleOk ? 0 : 4;
}
