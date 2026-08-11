#include "core/render/render_backend.h"
#include "core/window/window_backend.h"

#if defined(EUI_RENDER_BACKEND_OPENGL)
#include <glad/glad.h>
#endif

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

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

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

bool near(float value, float expected) {
    return std::fabs(value - expected) < 0.015f;
}

bool writeShader(const std::filesystem::path& path,
                 const std::string& source,
                 int timestampOffset) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output || !(output << source)) return false;
    output.close();
    std::error_code error;
    std::filesystem::last_write_time(
        path,
        std::filesystem::file_time_type::clock::now() +
            std::chrono::seconds(timestampOffset),
        error);
    return !error;
}

#if defined(EUI_RENDER_BACKEND_VULKAN)
bool copySpirv(const std::filesystem::path& source,
               const std::filesystem::path& target,
               int timestampOffset) {
    std::error_code error;
    std::filesystem::create_directories(target.parent_path(), error);
    if (error) return false;
    std::filesystem::copy_file(
        source, target, std::filesystem::copy_options::overwrite_existing,
        error);
    if (error) return false;
    std::filesystem::last_write_time(
        target,
        std::filesystem::file_time_type::clock::now() +
            std::chrono::seconds(timestampOffset),
        error);
    return !error;
}

bool writeInvalidSpirv(const std::filesystem::path& path,
                       int timestampOffset) {
    const std::array<std::uint32_t, 4> invalid{};
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) return false;
    output.write(reinterpret_cast<const char*>(invalid.data()),
                 static_cast<std::streamsize>(sizeof(invalid)));
    output.close();
    if (!output) return false;
    std::error_code error;
    std::filesystem::last_write_time(
        path,
        std::filesystem::file_time_type::clock::now() +
            std::chrono::seconds(timestampOffset),
        error);
    return !error;
}
#endif

void beginBackendFrame(core::window::Handle window,
                       core::render::RenderBackend& backend) {
    backend.makeCurrent();
    backend.beginFrame({
        window,
        core::window::nativeWindowInfo(window),
        64,
        64,
        1.0f
    });
    backend.clear({0.0f, 0.0f, 0.0f, 1.0f});
}

} // namespace

int main() {
    core::render::initializeRenderBackendLoader();
    if (!initializeWindowSystem()) return 1;

    core::window::WindowCreateRequest request;
    request.width = 64;
    request.height = 64;
    request.title = "Shadertoy Probe";
    request.renderApi = core::render::windowRenderApi();
    core::window::Handle window = core::window::createWindow(request);
    if (window == nullptr) {
        shutdownWindowSystem();
        return 2;
    }

    std::unique_ptr<core::render::RenderBackend> backend =
        core::render::createRenderBackend(window);
    if (!backend || !backend->initialize()) {
        core::window::destroyWindow(window);
        shutdownWindowSystem();
        return 3;
    }
    backend->makeCurrent();

    const std::filesystem::path source =
        std::filesystem::path(EUI_TEST_SOURCE_DIR) / "tests/assets/shadertoy/feedback.frag";
    core::render::ShaderToyGraph graph;
#if defined(EUI_RENDER_BACKEND_VULKAN)
    const std::filesystem::path spirv =
        std::filesystem::path(EUI_TEST_SHADERS_DIR) / "feedback.frag.spv";
    graph.addPass("image", source.string(), spirv.string());
#else
    graph.addPass("image", source.string());
#endif
    graph.setChannel("image", 0, core::render::ShaderToyChannel::self());

    core::render::ShaderToyError error;
    const auto toy = backend->createShaderToy(graph, &error);
    if (toy == nullptr || error) return 4;

    core::render::ShaderToyFrameData frame;
    frame.frameToken = 1;
    beginBackendFrame(window, *backend);
    auto texture = backend->renderShaderToy(toy, graph, 8, 8, frame, false, true, &error);
    backend->present();
    std::array<float, 4> pixel{};
    if (texture == nullptr || error || !backend->readShaderToyPixel(toy, pixel.data()) ||
        !near(pixel[0], 0.1f) || !near(pixel[1], 0.0f)) return 5;

    beginBackendFrame(window, *backend);
    texture = backend->renderShaderToy(toy, graph, 8, 8, frame, false, false, &error);
    texture = backend->renderShaderToy(toy, graph, 8, 8, frame, false, false, &error);
    backend->present();
    if (!backend->readShaderToyPixel(toy, pixel.data()) || !near(pixel[0], 0.1f)) return 6;

    frame.frame = 1;
    frame.frameToken = 2;
    beginBackendFrame(window, *backend);
    texture = backend->renderShaderToy(toy, graph, 8, 8, frame, false, false, &error);
    backend->present();
    if (!backend->readShaderToyPixel(toy, pixel.data()) || !near(pixel[0], 0.2f) || !near(pixel[1], 0.1f)) return 7;

    frame.frame = 2;
    frame.frameToken = 3;
    beginBackendFrame(window, *backend);
    texture = backend->renderShaderToy(toy, graph, 8, 8, frame, true, false, &error);
    backend->present();
    if (!backend->readShaderToyPixel(toy, pixel.data()) || !near(pixel[0], 0.2f)) return 8;

    frame.frame = 0;
    frame.frameToken = 4;
    beginBackendFrame(window, *backend);
    texture = backend->renderShaderToy(toy, graph, 8, 8, frame, false, true, &error);
    backend->present();
    if (!backend->readShaderToyPixel(toy, pixel.data()) || !near(pixel[0], 0.1f) || !near(pixel[1], 0.0f)) return 9;

    frame.frame = 1;
    frame.frameToken = 5;
    beginBackendFrame(window, *backend);
    texture = backend->renderShaderToy(
        toy, graph, 1 << 30, 1 << 30, frame, false, false, &error);
    backend->present();
    if (texture == nullptr ||
        error.code != core::render::ShaderToyErrorCode::ResourceCreationFailed ||
        !backend->readShaderToyPixel(toy, pixel.data()) ||
        !near(pixel[0], 0.1f) || !near(pixel[1], 0.0f)) return 25;

    error = {};
    const auto secondToy = backend->createShaderToy(graph, &error);
    if (secondToy == nullptr || error) return 26;
    core::render::ShaderToyFrameData secondFrame;
    secondFrame.frameToken = 100;
    beginBackendFrame(window, *backend);
    texture = backend->renderShaderToy(
        secondToy, graph, 8, 8, secondFrame, false, true, &error);
    backend->present();
    std::array<float, 4> secondPixel{};
    if (texture == nullptr || error ||
        !backend->readShaderToyPixel(secondToy, secondPixel.data()) ||
        !near(secondPixel[0], 0.1f) || !near(secondPixel[1], 0.0f) ||
        !backend->readShaderToyPixel(toy, pixel.data()) || !near(pixel[0], 0.1f)) return 27;

    frame.frame = 1;
    frame.frameToken = 6;
    beginBackendFrame(window, *backend);
    texture = backend->renderShaderToy(toy, graph, 8, 8, frame, false, false, &error);
    backend->present();
    if (texture == nullptr || error ||
        !backend->readShaderToyPixel(toy, pixel.data()) ||
        !near(pixel[0], 0.2f) || !near(pixel[1], 0.1f) ||
        !backend->readShaderToyPixel(secondToy, secondPixel.data()) ||
        !near(secondPixel[0], 0.1f) || !near(secondPixel[1], 0.0f)) return 28;

    backend->destroyShaderToy(toy);
    backend->destroyShaderToy(secondToy);

    const std::filesystem::path bufferSource =
        std::filesystem::path(EUI_TEST_SOURCE_DIR) / "tests/assets/shadertoy/buffer.frag";
    const std::filesystem::path channelsSource =
        std::filesystem::path(EUI_TEST_SOURCE_DIR) / "tests/assets/shadertoy/channels.frag";
    const std::filesystem::path imageSource =
        std::filesystem::path(EUI_TEST_SOURCE_DIR) / "assets/icon.png";
    core::render::ShaderToyGraph channelsGraph;
#if defined(EUI_RENDER_BACKEND_VULKAN)
    const std::filesystem::path bufferSpirv =
        std::filesystem::path(EUI_TEST_SHADERS_DIR) / "buffer.frag.spv";
    const std::filesystem::path channelsSpirv =
        std::filesystem::path(EUI_TEST_SHADERS_DIR) / "channels.frag.spv";
    channelsGraph.addPass("buffer", bufferSource.string(), bufferSpirv.string());
    channelsGraph.addPass("image", channelsSource.string(), channelsSpirv.string());
#else
    channelsGraph.addPass("buffer", bufferSource.string());
    channelsGraph.addPass("image", channelsSource.string());
#endif
    channelsGraph.setChannel("image", 0,
                             core::render::ShaderToyChannel::image(imageSource.string()));
    channelsGraph.setChannel("image", 1,
                             core::render::ShaderToyChannel::buffer("buffer"));
    channelsGraph.setChannel("image", 2, core::render::ShaderToyChannel::self());
    channelsGraph.setChannel("image", 3, core::render::ShaderToyChannel::none());
    channelsGraph.setUniform("uValue", 0.25f);

    const auto channelsToy = backend->createShaderToy(channelsGraph, &error);
    if (channelsToy == nullptr || error) return 10;
    frame = {};
    frame.frameToken = 10;
    beginBackendFrame(window, *backend);
    texture = backend->renderShaderToy(
        channelsToy, channelsGraph, 8, 8, frame, false, true, &error);
    backend->present();
    if (texture == nullptr || error ||
        !backend->readShaderToyPixel(channelsToy, pixel.data()) ||
        !near(pixel[0], 0.25f) || !near(pixel[1], 0.1f) ||
        !near(pixel[2], 0.0f) || !near(pixel[3], 1.0f)) return 11;

    channelsGraph.setUniform("uValue", 0.75f);
    frame.frame = 1;
    frame.frameToken = 11;
    beginBackendFrame(window, *backend);
    texture = backend->renderShaderToy(
        channelsToy, channelsGraph, 8, 8, frame, false, false, &error);
    backend->present();
    if (!backend->readShaderToyPixel(channelsToy, pixel.data()) ||
        !near(pixel[0], 0.75f) || !near(pixel[1], 0.2f)) return 12;

    frame.frame = 0;
    frame.frameToken = 12;
    beginBackendFrame(window, *backend);
    texture = backend->renderShaderToy(
        channelsToy, channelsGraph, 12, 6, frame, false, true, &error);
    backend->present();
    if (!backend->readShaderToyPixel(channelsToy, pixel.data()) ||
        !near(pixel[0], 0.75f) || !near(pixel[1], 0.1f)) return 13;

    backend->destroyShaderToy(channelsToy);

    const std::string inlineSource =
        "void mainImage(out vec4 color, in vec2 coord) { color = vec4(0.3, 0.4, 0.5, 1.0); }";
    core::render::ShaderToyGraph inlineGraph;
#if defined(EUI_RENDER_BACKEND_VULKAN)
    inlineGraph.addInlinePass(
        "image", inlineSource,
        (std::filesystem::path(EUI_TEST_SHADERS_DIR) /
         "inline.frag.spv").string(),
        "inline-test.frag");
#else
    inlineGraph.addInlinePass("image", inlineSource, {},
                              "inline-test.frag");
#endif
    error = {};
    const auto inlineToy = backend->createShaderToy(inlineGraph, &error);
    if (inlineToy == nullptr || error) return 41;
    core::render::ShaderToyFrameData inlineFrame;
    inlineFrame.frameToken = 400;
    beginBackendFrame(window, *backend);
    texture = backend->renderShaderToy(
        inlineToy, inlineGraph, 4, 4, inlineFrame, false, true, &error);
    backend->present();
    if (texture == nullptr || error ||
        !backend->readShaderToyPixel(inlineToy, pixel.data()) ||
        !near(pixel[0], 0.3f) || !near(pixel[1], 0.4f) ||
        !near(pixel[2], 0.5f)) return 42;
#if defined(EUI_RENDER_BACKEND_OPENGL)
    // Match a runtime dirty repaint: Shadertoy restores the dirty scissor,
    // then drawTexture repopulates unrelated cached GL state before the blit.
    backend->makeCurrent();
    backend->beginFrame({
        window,
        core::window::nativeWindowInfo(window),
        64,
        64,
        1.0f
    });
    if (!backend->ensureRenderCache(64, 64)) return 46;
    backend->beginRenderCacheFrame(64, 64);
    backend->setScissor(false, {}, 64);
    backend->clear({0.0f, 1.0f, 0.0f, 1.0f});
    backend->endRenderCacheFrame();

    backend->setScissor(false, {}, 64);
    backend->clear({1.0f, 0.0f, 0.0f, 1.0f});

    const core::Rect dirty{0.0f, 0.0f, 8.0f, 8.0f};
    backend->beginRenderCacheFrame(64, 64, {dirty});
    backend->setScissor(true, dirty, 64);
    inlineFrame.frameToken = 401;
    texture = backend->renderShaderToy(
        inlineToy, inlineGraph, 4, 4, inlineFrame, false, false, &error);
    const float vertices[42] = {
        0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
        64.0f, 0.0f, 1.0f, 64.0f, 0.0f, 1.0f, 1.0f,
        64.0f, 64.0f, 1.0f, 64.0f, 64.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f,
        64.0f, 64.0f, 1.0f, 64.0f, 64.0f, 1.0f, 0.0f,
        0.0f, 64.0f, 1.0f, 0.0f, 64.0f, 0.0f, 0.0f
    };
    if (texture == nullptr || error) return 47;
    backend->drawTexture(texture, vertices, 42,
                         {1.0f, 1.0f, 1.0f, 1.0f},
                         {0.0f, 0.0f, 64.0f, 64.0f},
                         0.0f, 0.0f, 64, 64);
    backend->endRenderCacheFrame();
    backend->blitRenderCache(64, 64,
                             core::render::RenderCacheBlitMode::Full);

    std::array<unsigned char, 4> backbufferPixel{};
    glGetError();
    glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE,
                 backbufferPixel.data());
    if (glGetError() != GL_NO_ERROR ||
        backbufferPixel[0] > 16 || backbufferPixel[1] < 239 ||
        backbufferPixel[2] > 16 || backbufferPixel[3] < 239) {
        return 48;
    }
#endif
    backend->destroyShaderToy(inlineToy);
    const std::filesystem::path passContractSource =
        std::filesystem::path(EUI_TEST_SOURCE_DIR) /
        "tests/assets/shadertoy/pass_contract.frag";
    const auto runPassContract = [&]() {
        core::render::ShaderToyGraph passGraph;
#if defined(EUI_RENDER_BACKEND_VULKAN)
        passGraph.addPass(
            "source", passContractSource.string(),
            (std::filesystem::path(EUI_TEST_SHADERS_DIR) /
             "pass_contract.frag.spv").string());
        passGraph.addPass(
            "image", passContractSource.string(),
            (std::filesystem::path(EUI_TEST_SHADERS_DIR) /
             "pass_contract.frag.spv").string());
#else
        passGraph.addPass("source", passContractSource.string());
        passGraph.addPass("image", passContractSource.string());
#endif
        passGraph.setChannel(
            "image", 0,
            core::render::ShaderToyChannel::buffer("source"));
        core::render::ShaderToyError passError;
        const auto passToy = backend->createShaderToy(passGraph, &passError);
        if (passToy == nullptr || passError) return false;
        core::render::ShaderToyFrameData passFrame;
        passFrame.frameToken = 500;
        passFrame.mouse = {4.0f, 2.0f, 4.0f, 2.0f};
        beginBackendFrame(window, *backend);
        const auto passTexture = backend->renderShaderToy(
            passToy, passGraph, 16, 8, passFrame, false, true, &passError);
        backend->present();
        std::array<float, 4> passPixel{};
        const bool success =
            passTexture != nullptr && !passError &&
            backend->readShaderToyPixel(passToy, passPixel.data()) &&
            near(passPixel[0], 1.0f) && near(passPixel[1], 0.5f) &&
            near(passPixel[2], 0.25f) && near(passPixel[3], 1.0f);
        backend->destroyShaderToy(passToy);
        return success;
    };
    if (!runPassContract()) return 43;

    core::render::ShaderToyGraph invalidGraph;
    invalidGraph.addPass("duplicate", source.string());
    invalidGraph.addPass("duplicate", source.string());
    error = {};
    if (backend->createShaderToy(invalidGraph, &error) != nullptr ||
        error.code != core::render::ShaderToyErrorCode::DuplicatePassName) return 14;

    core::render::ShaderToyGraph missingImageGraph;
#if defined(EUI_RENDER_BACKEND_VULKAN)
    missingImageGraph.addPass(
        "image", channelsSource.string(),
        (std::filesystem::path(EUI_TEST_SHADERS_DIR) / "channels.frag.spv").string());
#else
    missingImageGraph.addPass("image", channelsSource.string());
#endif
    const std::filesystem::path missingImage =
        std::filesystem::path(EUI_TEST_SOURCE_DIR) / "tests/assets/shadertoy/missing.png";
    missingImageGraph.setChannel(
        "image", 0, core::render::ShaderToyChannel::image(missingImage.string()));
    error = {};
    if (backend->createShaderToy(missingImageGraph, &error) != nullptr ||
        error.code != core::render::ShaderToyErrorCode::SourceReadFailed ||
        error.stage != "image" || error.sourcePath != missingImage.string()) return 15;

#if defined(EUI_RENDER_BACKEND_VULKAN)
    core::render::ShaderToyGraph missingSpirvGraph;
    const std::filesystem::path missingSpirv =
        std::filesystem::path(EUI_TEST_SHADERS_DIR) / "missing.frag.spv";
    missingSpirvGraph.addPass("image", source.string(), missingSpirv.string());
    error = {};
    if (backend->createShaderToy(missingSpirvGraph, &error) != nullptr ||
        error.code != core::render::ShaderToyErrorCode::SourceReadFailed ||
        error.stage != "fragment" || error.sourcePath != missingSpirv.string()) return 16;

    const std::filesystem::path reloadDirectory =
        std::filesystem::path(EUI_TEST_SHADERS_DIR);
    const std::filesystem::path reloadInitial =
        reloadDirectory / "hot_reload_initial.frag.spv";
    const std::filesystem::path reloadRecovered =
        reloadDirectory / "hot_reload_recovered.frag.spv";
    const std::filesystem::path reloadActive =
        reloadDirectory / "hot_reload_active.frag.spv";
    if (!copySpirv(reloadInitial, reloadActive, 1)) return 17;
    core::render::ShaderToyGraph reloadGraph;
    reloadGraph.addPass("image", source.string(), reloadActive.string());
    error = {};
    const auto reloadToy = backend->createShaderToy(reloadGraph, &error);
    if (reloadToy == nullptr || error) return 18;
    frame = {};
    frame.frameToken = 20;
    beginBackendFrame(window, *backend);
    texture = backend->renderShaderToy(
        reloadToy, reloadGraph, 8, 8, frame, false, true, &error);
    backend->present();
    if (texture == nullptr || error ||
        !backend->readShaderToyPixel(reloadToy, pixel.data()) ||
        !near(pixel[0], 0.2f)) return 19;

    if (!writeInvalidSpirv(reloadActive, 2)) return 20;
    frame.frame = 1;
    frame.frameToken = 21;
    beginBackendFrame(window, *backend);
    texture = backend->renderShaderToy(
        reloadToy, reloadGraph, 8, 8, frame, false, false, &error);
    backend->present();
    if (texture == nullptr ||
        error.code != core::render::ShaderToyErrorCode::ShaderCompileFailed ||
        error.passName != "image" || error.stage != "fragment" ||
        error.sourcePath != reloadActive.string() ||
        !backend->readShaderToyPixel(reloadToy, pixel.data()) ||
        !near(pixel[0], 0.2f)) return 21;

    frame.frame = 2;
    frame.frameToken = 22;
    error = {};
    beginBackendFrame(window, *backend);
    texture = backend->renderShaderToy(
        reloadToy, reloadGraph, 8, 8, frame, false, false, &error);
    backend->present();
    if (texture == nullptr || !error ||
        !backend->readShaderToyPixel(reloadToy, pixel.data()) ||
        !near(pixel[0], 0.2f)) return 22;

    if (!copySpirv(reloadRecovered, reloadActive, 3)) return 23;
    frame.frame = 3;
    frame.frameToken = 23;
    beginBackendFrame(window, *backend);
    texture = backend->renderShaderToy(
        reloadToy, reloadGraph, 8, 8, frame, false, false, &error);
    backend->present();
    if (texture == nullptr || error ||
        !backend->readShaderToyPixel(reloadToy, pixel.data()) ||
        !near(pixel[0], 0.7f)) return 24;
    backend->destroyShaderToy(reloadToy);
    std::error_code removeError;
    std::filesystem::remove(reloadActive, removeError);
#else
    const std::filesystem::path reloadSource =
        std::filesystem::path(EUI_TEST_SOURCE_DIR) / "build-shadertoy/shadertoy/hot_reload.frag";
    std::filesystem::create_directories(reloadSource.parent_path());
    const std::string initialSource =
        "void mainImage(out vec4 fragColor, in vec2 fragCoord) {\n"
        "    vec2 uv = fragCoord / iResolution.xy;\n"
        "    fragColor = vec4(0.2, uv.y * 0.0, 0.0, 1.0);\n"
        "}\n";
    const std::string invalidSource =
        "void mainImage(out vec4 fragColor, in vec2 fragCoord) {\n"
        "    vec2 uv = fragCoord / iResolution.xy;\n"
        "    this is invalid glsl;\n"
        "    fragColor = vec4(uv, 0.0, 1.0);\n"
        "}\n";
    const std::string recoveredSource =
        "void mainImage(out vec4 fragColor, in vec2 fragCoord) {\n"
        "    vec2 uv = fragCoord / iResolution.xy;\n"
        "    fragColor = vec4(0.7, uv.y * 0.0, 0.0, 1.0);\n"
        "}\n";
    if (!writeShader(reloadSource, initialSource, 1)) return 17;
    core::render::ShaderToyGraph reloadGraph;
    reloadGraph.addPass("image", reloadSource.string());
    error = {};
    const auto reloadToy = backend->createShaderToy(reloadGraph, &error);
    if (reloadToy == nullptr || error) return 18;
    frame = {};
    frame.frameToken = 20;
    beginBackendFrame(window, *backend);
    texture = backend->renderShaderToy(
        reloadToy, reloadGraph, 8, 8, frame, false, true, &error);
    backend->present();
    if (texture == nullptr || error ||
        !backend->readShaderToyPixel(reloadToy, pixel.data()) ||
        !near(pixel[0], 0.2f)) return 19;

    if (!writeShader(reloadSource, invalidSource, 2)) return 20;
    frame.frame = 1;
    frame.frameToken = 21;
    beginBackendFrame(window, *backend);
    texture = backend->renderShaderToy(
        reloadToy, reloadGraph, 8, 8, frame, false, false, &error);
    backend->present();
    if (texture == nullptr ||
        error.code != core::render::ShaderToyErrorCode::ShaderCompileFailed ||
        error.passName != "image" || error.stage != "fragment" ||
        error.sourcePath != reloadSource.string() || error.line != 3 ||
        !backend->readShaderToyPixel(reloadToy, pixel.data()) ||
        !near(pixel[0], 0.2f)) return 21;

    frame.frame = 2;
    frame.frameToken = 22;
    error = {};
    beginBackendFrame(window, *backend);
    texture = backend->renderShaderToy(
        reloadToy, reloadGraph, 8, 8, frame, false, false, &error);
    backend->present();
    if (texture == nullptr || !error ||
        !backend->readShaderToyPixel(reloadToy, pixel.data()) ||
        !near(pixel[0], 0.2f)) return 22;

    if (!writeShader(reloadSource, recoveredSource, 3)) return 23;
    frame.frame = 3;
    frame.frameToken = 23;
    beginBackendFrame(window, *backend);
    texture = backend->renderShaderToy(
        reloadToy, reloadGraph, 8, 8, frame, false, false, &error);
    backend->present();
    if (texture == nullptr || error ||
        !backend->readShaderToyPixel(reloadToy, pixel.data()) ||
        !near(pixel[0], 0.7f)) return 24;
    backend->destroyShaderToy(reloadToy);
    std::error_code removeError;
    std::filesystem::remove(reloadSource, removeError);
#endif

    for (int cycle = 0; cycle < 3; ++cycle) {
        error = {};
        const auto lifecycleToy = backend->createShaderToy(graph, &error);
        if (lifecycleToy == nullptr || error) return 29;
        core::render::ShaderToyFrameData lifecycleFrame;
        lifecycleFrame.frameToken = static_cast<std::uint64_t>(200 + cycle);
        beginBackendFrame(window, *backend);
        texture = backend->renderShaderToy(
            lifecycleToy, graph, 8 + cycle, 8 + cycle,
            lifecycleFrame, false, true, &error);
        if (texture == nullptr || error) return 30;
        backend->destroyShaderToy(lifecycleToy);
        backend->present();
    }

    backend.reset();
    core::window::destroyWindow(window);
    shutdownWindowSystem();
    return 0;
}
