#include "core/render/shadertoy_primitive.h"

#include <cassert>
#include <cmath>
#include <vector>

namespace {

bool near(float value, float expected) {
    return std::fabs(value - expected) < 0.0001f;
}

class FakeBackend final : public core::render::RenderBackend {
public:
    bool initialize() override { return true; }
    bool valid() const override { return true; }
    void makeCurrent() override {}
    void beginFrame(const core::render::RenderSurface&) override {}
    void present() override {}
    bool ensureRenderCache(int, int) override { return true; }
    bool renderCacheWasRecreated() const override { return false; }
    void releaseRenderCache() override {}
    void beginRenderCacheFrame(int, int, const std::vector<core::Rect>&) override {}
    void endRenderCacheFrame() override {}
    void blitRenderCache(int, int, core::render::RenderCacheBlitMode,
                         const std::vector<core::Rect>&) override {}
    void clear(const core::Color&) override {}
    void setScissor(bool, const core::Rect&, int) override {}
    void prepareBackdropBlur(const core::Rect&, float, int, int) override {}
    void drawRoundedRect(const core::render::RoundedRectDrawCommand&, int, int) override {}
    void drawPolygon(const core::render::PolygonDrawCommand&, int, int) override {}
    void drawText(const core::render::TextDrawCommand&, int, int) override {}

    ShaderToyHandle createShaderToy(const core::render::ShaderToyGraph&,
                                    core::render::ShaderToyError* error) override {
        ++creates;
        if (failNextCreate) {
            failNextCreate = false;
            if (error != nullptr) {
                *error = {core::render::ShaderToyErrorCode::ResourceCreationFailed,
                          {}, {}, "resource", {}, 0, "replacement failed"};
            }
            return nullptr;
        }
        if (error != nullptr) *error = {};
        return reinterpret_cast<ShaderToyHandle>(
            static_cast<std::uintptr_t>(creates));
    }

    TextureHandle renderShaderToy(ShaderToyHandle,
                                  const core::render::ShaderToyGraph& graph,
                                  int width,
                                  int height,
                                  const core::render::ShaderToyFrameData& frame,
                                  bool paused,
                                  bool reset,
                                  core::render::ShaderToyError* error) override {
        ++renders;
        lastGraph = graph;
        lastWidth = width;
        lastHeight = height;
        lastFrame = frame;
        lastPaused = paused;
        lastReset = reset;
        frames.push_back(frame);
        resets.push_back(reset);
        if (error != nullptr) *error = {};
        return &textureValue;
    }

    void destroyShaderToy(ShaderToyHandle) override { ++destroys; }

    void drawTexture(TextureHandle,
                     const float* vertices,
                     std::size_t count,
                     const core::Color&,
                     const core::Rect&,
                     float,
                     float,
                     int,
                     int) override {
        ++draws;
        assert(vertices != nullptr && count == 42);
    }

    int creates = 0;
    int renders = 0;
    int destroys = 0;
    int draws = 0;
    int handleValue = 1;
    int textureValue = 2;
    bool failNextCreate = false;
    int lastWidth = 0;
    int lastHeight = 0;
    bool lastPaused = false;
    bool lastReset = false;
    core::render::ShaderToyGraph lastGraph;
    core::render::ShaderToyFrameData lastFrame;
    std::vector<core::render::ShaderToyFrameData> frames;
    std::vector<bool> resets;
};

core::render::ShaderToyGraph graph(float value = 0.25f) {
    core::render::ShaderToyGraph result;
    result.addPass("image", "unused.frag");
    result.setUniform("uValue", value);
    return result;
}

void frameAndMouseSemantics() {
    FakeBackend backend;
    core::render::ScopedRenderBackend scoped(backend);
    core::ShaderToyPrimitive primitive;
    primitive.setGraph(graph());
    primitive.setBounds(10.0f, 20.0f, 100.0f, 50.0f);
    primitive.update(0.1f, {20.0f, 10.0f}, true, true, false, 1);
    primitive.render(200, 100);
    assert(backend.creates == 1 && backend.renders == 1 && backend.lastReset);
    assert(backend.lastFrame.frame == 0);
    assert(near(backend.lastFrame.time, 0.1f));
    assert(near(backend.lastFrame.mouse[0], 20.0f));
    assert(near(backend.lastFrame.mouse[1], 40.0f));
    assert(near(backend.lastFrame.mouse[2], 20.0f));
    assert(near(backend.lastFrame.mouse[3], 40.0f));

    primitive.update(9.0f, {90.0f, 45.0f}, true, false, false, 1);
    assert(near(primitive.frameData().time, 0.1f));

    primitive.update(0.2f, {30.0f, 15.0f}, false, false, true, 2);
    primitive.render(200, 100);
    assert(backend.lastFrame.frame == 1);
    assert(near(backend.lastFrame.time, 0.3f));
    assert(near(backend.lastFrame.mouse[0], 30.0f));
    assert(near(backend.lastFrame.mouse[1], 35.0f));
    assert(backend.lastFrame.mouse[2] < 0.0f && backend.lastFrame.mouse[3] < 0.0f);
}

void pointerRegionAndCaptureSemantics() {
    core::ShaderToyPrimitive primitive;
    primitive.setBounds(0.0f, 0.0f, 100.0f, 50.0f);

    primitive.update(0.1f, {120.0f, 60.0f}, true, true, false, 1, false);
    assert(near(primitive.frameData().mouse[0], 0.0f));
    assert(near(primitive.frameData().mouse[1], 0.0f));
    assert(near(primitive.frameData().mouse[2], 0.0f));
    assert(near(primitive.frameData().mouse[3], 0.0f));

    primitive.update(0.1f, {20.0f, 10.0f}, true, true, false, 2, true);
    assert(near(primitive.frameData().mouse[0], 20.0f));
    assert(near(primitive.frameData().mouse[1], 40.0f));
    assert(near(primitive.frameData().mouse[2], 20.0f));
    assert(near(primitive.frameData().mouse[3], 40.0f));

    primitive.update(0.1f, {130.0f, 70.0f}, true, false, false, 3, false);
    assert(near(primitive.frameData().mouse[0], 100.0f));
    assert(near(primitive.frameData().mouse[1], 0.0f));
    assert(near(primitive.frameData().mouse[2], 100.0f));
    assert(near(primitive.frameData().mouse[3], 0.0f));

    primitive.update(0.1f, {140.0f, 80.0f}, false, false, true, 4, false);
    assert(near(primitive.frameData().mouse[0], 100.0f));
    assert(near(primitive.frameData().mouse[1], 0.0f));
    assert(primitive.frameData().mouse[2] < 0.0f);
    assert(primitive.frameData().mouse[3] < 0.0f);

    primitive.update(0.1f, {30.0f, 15.0f}, false, false, false, 5, true);
    assert(near(primitive.frameData().mouse[0], 30.0f));
    assert(near(primitive.frameData().mouse[1], 35.0f));
}
void pauseResetAndResourceSemantics() {
    FakeBackend backend;
    core::render::ScopedRenderBackend scoped(backend);
    core::ShaderToyPrimitive primitive;
    primitive.setGraph(graph());
    primitive.setBounds(0.0f, 0.0f, 40.0f, 20.0f);
    primitive.setResolutionScale(0.5f);
    primitive.setTimeScale(2.0f);
    primitive.update(0.25f, {}, false, false, false, 1);
    primitive.render(100, 100);
    assert(backend.lastWidth == 20 && backend.lastHeight == 10);
    assert(near(backend.lastFrame.time, 0.5f));

    primitive.setPaused(true);
    primitive.update(5.0f, {}, false, false, false, 2);
    primitive.render(100, 100);
    assert(near(backend.lastFrame.time, 0.5f));
    assert(near(backend.lastFrame.deltaTime, 0.0f));
    assert(backend.lastPaused);

    primitive.setPaused(false);
    primitive.update(0.1f, {}, false, false, false, 3);
    primitive.render(100, 100);
    assert(near(backend.lastFrame.time, 0.7f));
    assert(near(backend.lastFrame.deltaTime, 0.2f));

    const int creates = backend.creates;
    primitive.setGraph(graph(0.75f));
    primitive.update(0.1f, {}, false, false, false, 4);
    primitive.render(100, 100);
    assert(backend.creates == creates);
    assert(near(backend.lastGraph.uniforms.front().values[0], 0.75f));
    assert(!backend.lastReset);

    primitive.setBounds(0.0f, 0.0f, 60.0f, 30.0f);
    primitive.update(0.1f, {}, false, false, false, 5);
    primitive.render(100, 100);
    assert(backend.lastReset);
    assert(backend.lastFrame.frame == 0);
    assert(near(backend.lastFrame.time, 1.1f));

    primitive.setOpacity(0.4f);
    primitive.setCornerRadius(6.0f);
    primitive.setTransformMatrix(core::TransformMatrix{});
    primitive.update(0.1f, {}, false, false, false, 6);
    primitive.render(100, 100);
    assert(!backend.lastReset);

    primitive.setResolutionScale(1.0f);
    primitive.update(0.1f, {}, false, false, false, 7);
    primitive.render(100, 100);
    assert(backend.lastReset);
    assert(backend.lastFrame.frame == 0);
    assert(near(backend.lastFrame.time, 1.5f));
    primitive.destroy();
    assert(backend.destroys == 1);
}

void independentPrimitiveState() {
    FakeBackend backend;
    core::render::ScopedRenderBackend scoped(backend);
    core::ShaderToyPrimitive first;
    core::ShaderToyPrimitive second;
    first.setGraph(graph());
    second.setGraph(graph());
    first.setBounds(0.0f, 0.0f, 40.0f, 20.0f);
    second.setBounds(50.0f, 0.0f, 60.0f, 30.0f);
    first.setTimeScale(2.0f);
    second.setTimeScale(0.5f);

    first.update(0.2f, {5.0f, 4.0f}, true, true, false, 1);
    first.render(120, 60);
    second.update(0.2f, {12.0f, 8.0f}, false, false, false, 1);
    second.render(120, 60);

    assert(backend.creates == 2 && backend.frames.size() == 2);
    assert(near(backend.frames[0].time, 0.4f));
    assert(near(backend.frames[1].time, 0.1f));
    assert(near(backend.frames[0].mouse[0], 5.0f));
    assert(near(backend.frames[0].mouse[2], 5.0f));
    assert(near(backend.frames[1].mouse[0], 12.0f));
    assert(near(backend.frames[1].mouse[2], 0.0f));

    first.update(0.1f, {9.0f, 6.0f}, true, false, false, 2);
    first.render(120, 60);
    second.update(0.1f, {16.0f, 10.0f}, false, false, false, 2);
    second.render(120, 60);
    assert(near(backend.frames[2].time, 0.6f));
    assert(near(backend.frames[3].time, 0.15f));
    assert(near(backend.frames[2].mouse[2], 9.0f));
    assert(near(backend.frames[3].mouse[2], 0.0f));
}

void moveAssignmentReleasesOwnedResource() {
    FakeBackend backend;
    core::render::ScopedRenderBackend scoped(backend);
    core::ShaderToyPrimitive source;
    core::ShaderToyPrimitive target;
    source.setGraph(graph());
    source.setBounds(0.0f, 0.0f, 20.0f, 20.0f);
    source.update(0.1f, {}, false, false, false, 1);
    source.render(40, 40);
    target.setGraph(graph());
    target.setBounds(20.0f, 0.0f, 20.0f, 20.0f);
    target.update(0.1f, {}, false, false, false, 1);
    target.render(40, 40);
    assert(backend.creates == 2 && backend.destroys == 0);

    target = std::move(source);
    assert(backend.destroys == 1);
    target.destroy();
    assert(backend.destroys == 2);
}

void graphReplacementKeepsLastValidOutput() {
    FakeBackend backend;
    core::render::ScopedRenderBackend scoped(backend);
    core::ShaderToyPrimitive primitive;
    primitive.setGraph(graph());
    primitive.setBounds(0.0f, 0.0f, 40.0f, 20.0f);
    primitive.update(0.1f, {}, false, false, false, 1);
    primitive.render(80, 40);
    assert(backend.creates == 1 && backend.renders == 1 &&
           backend.draws == 1 && backend.destroys == 0);

    core::render::ShaderToyGraph replacement = graph();
    replacement.passes[0].fragmentPath = "replacement.frag";
    backend.failNextCreate = true;
    primitive.setGraph(replacement);
    primitive.update(0.1f, {}, false, false, false, 2);
    primitive.render(80, 40);
    assert(backend.creates == 2 && backend.renders == 2 &&
           backend.draws == 2 && backend.destroys == 0);
    assert(backend.lastGraph.passes[0].fragmentPath == "unused.frag");

    primitive.update(0.1f, {}, false, false, false, 3);
    primitive.render(80, 40);
    assert(backend.creates == 3 && backend.renders == 3 &&
           backend.draws == 3 && backend.destroys == 1);
    assert(backend.lastGraph.passes[0].fragmentPath ==
           "replacement.frag");
}

} // namespace

int main() {
    frameAndMouseSemantics();
    pointerRegionAndCaptureSemantics();
    pauseResetAndResourceSemantics();
    independentPrimitiveState();
    moveAssignmentReleasesOwnedResource();
    graphReplacementKeepsLastValidOutput();
    return 0;
}
