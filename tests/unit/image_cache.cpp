#include "core/platform/async.h"
#include "core/platform/network.h"
#include "core/render/image.h"
#include "core/render/image_source.h"
#include "core/render/render_backend.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <thread>
#include <unordered_set>
#include <vector>

namespace {

class CountingBackend final : public core::render::RenderBackend {
public:
    ~CountingBackend() override {
        for (TextureHandle handle : liveTextures_) {
            delete static_cast<int*>(handle);
        }
    }

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
    void blitRenderCache(int,
                         int,
                         core::render::RenderCacheBlitMode,
                         const std::vector<core::Rect>&) override {}
    void clear(const core::Color&) override {}
    void setScissor(bool, const core::Rect&, int) override {}
    void prepareBackdropBlur(const core::Rect&, float, int, int) override {}
    void drawRoundedRect(const core::render::RoundedRectDrawCommand&, int, int) override {}
    void drawPolygon(const core::render::PolygonDrawCommand&, int, int) override {}
    void drawText(const core::render::TextDrawCommand&, int, int) override {}

    TextureHandle createTexture(const unsigned char*, int, int) override {
        TextureHandle handle = new int(++nextTexture_);
        liveTextures_.insert(handle);
        ++createdTextures;
        return handle;
    }

    void destroyTexture(TextureHandle handle) override {
        const auto found = liveTextures_.find(handle);
        if (found == liveTextures_.end()) {
            return;
        }
        delete static_cast<int*>(handle);
        liveTextures_.erase(found);
        ++destroyedTextures;
    }

    int createdTextures = 0;
    int destroyedTextures = 0;

private:
    int nextTexture_ = 0;
    std::unordered_set<TextureHandle> liveTextures_;
};

struct AsyncShutdown {
    ~AsyncShutdown() { core::async::shutdown(); }
};

struct TempDirectory {
    std::filesystem::path path;
    ~TempDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
};

struct TempFiles {
    std::vector<std::filesystem::path> paths;
    ~TempFiles() {
        for (const std::filesystem::path& path : paths) {
            std::error_code error;
            std::filesystem::remove(path, error);
        }
    }
};

bool writeSvg(const std::filesystem::path& path) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"8\" height=\"8\">"
              "<rect width=\"8\" height=\"8\" fill=\"#4c7a9f\"/>"
              "</svg>";
    return output.good();
}

std::shared_ptr<const core::render::image::StaticImageData> awaitDecodedImage(
    const std::filesystem::path& path) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline) {
        core::async::dispatchReady();
        bool pending = false;
        auto image = core::render::image::requestStaticImageFromPath(path.string(), false, &pending);
        if (image) {
            return image;
        }
        if (!pending) {
            return {};
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return {};
}

bool textureSurvivesAnIdleRelease(const std::filesystem::path& path) {
    CountingBackend backend;
    core::render::ScopedRenderBackend scopedBackend(backend);

    core::ImagePrimitive first;
    first.initialize();
    first.setSource(path.string());
    first.setBounds(0.0f, 0.0f, 64.0f, 64.0f);
    first.updateTexture();
    first.render(64, 64);
    first.destroy();
    const bool retainedWhileIdle = backend.createdTextures == 1 && backend.destroyedTextures == 0;

    core::ImagePrimitive second;
    second.initialize();
    second.setSource(path.string());
    second.setBounds(0.0f, 0.0f, 64.0f, 64.0f);
    second.updateTexture();
    second.render(64, 64);
    second.destroy();
    const bool reused = backend.createdTextures == 1 && backend.destroyedTextures == 0;

    core::ImagePrimitive::releaseCachedTextures();
    const bool released = backend.destroyedTextures == 1;
    return retainedWhileIdle && reused && released;
}

bool remoteReadySurvivesDecodedPixelEviction() {
    constexpr int kImageCount = 70;
    TempFiles cacheFiles;
    std::vector<std::string> sources;
    sources.reserve(kImageCount);

    for (int index = 0; index < kImageCount; ++index) {
        const std::string source = "https://eui.invalid/image-ready-" +
                                   std::to_string(index);
        const std::filesystem::path cachePath =
            core::network::cacheFilePath(source, ".cache", "eui_test_image_cache");
        cacheFiles.paths.push_back(cachePath);
        if (cachePath.empty() || !writeSvg(cachePath)) {
            return false;
        }

        bool pending = false;
        if (core::render::image::resolveImagePath(source, &pending) != cachePath.string() || pending) {
            return false;
        }
        auto decoded = core::render::image::loadStaticImageFromPath(cachePath.string(), false);
        if (!decoded || decoded->byteCount < 1024u * 1024u) {
            return false;
        }
        sources.push_back(source);
    }

    return core::render::image::isSourceReady(sources.front());
}

bool remoteTextureUploadsArePaced() {
    TempFiles cacheFiles;
    std::vector<std::string> sources;
    std::vector<std::shared_ptr<const core::render::image::StaticImageData>> decodedImages;
    for (int index = 0; index < 3; ++index) {
        const std::string source = "https://eui.invalid/image-cache-budget-" +
                                   std::to_string(index) + ".svg";
        const std::filesystem::path cachePath =
            core::network::cacheFilePath(source, ".svg", "eui_test_image_cache");
        cacheFiles.paths.push_back(cachePath);
        if (cachePath.empty() || !writeSvg(cachePath)) {
            return false;
        }
        auto decoded = core::render::image::loadStaticImageFromPath(cachePath.string(), false);
        if (!decoded) {
            return false;
        }
        sources.push_back(source);
        decodedImages.push_back(std::move(decoded));
    }

    CountingBackend backend;
    core::render::ScopedRenderBackend scopedBackend(backend);
    std::vector<core::ImagePrimitive> images;
    images.reserve(sources.size());
    bool updated = true;
    for (const std::string& source : sources) {
        images.emplace_back();
        core::ImagePrimitive& image = images.back();
        image.initialize();
        image.setSource(source);
        image.setBounds(0.0f, 0.0f, 64.0f, 64.0f);
        updated = image.updateTexture() && updated;
    }

    core::ImagePrimitive::beginRenderFrame();
    for (core::ImagePrimitive& image : images) {
        image.render(64, 64);
    }
    const bool firstFrameWasLimited = backend.createdTextures == 2;

    int deferredRepaints = 0;
    for (core::ImagePrimitive& image : images) {
        if (image.updateTexture()) {
            ++deferredRepaints;
        }
    }

    core::ImagePrimitive::beginRenderFrame();
    for (core::ImagePrimitive& image : images) {
        image.render(64, 64);
    }
    const bool nextFrameCompleted = backend.createdTextures == 3;

    for (core::ImagePrimitive& image : images) {
        image.destroy();
    }
    core::ImagePrimitive::releaseCachedTextures();
    return updated && firstFrameWasLimited && deferredRepaints == 1 &&
           nextFrameCompleted && backend.destroyedTextures == 3;
}

} // namespace

int main() {
    AsyncShutdown asyncShutdown;
    std::error_code error;
    TempDirectory directory{
        std::filesystem::temp_directory_path(error) / "eui_neo_image_cache_test"
    };
    if (error) {
        std::cerr << "failed to locate temp directory: " << error.message() << "\n";
        return 1;
    }
    std::filesystem::remove_all(directory.path, error);
    error.clear();
    std::filesystem::create_directories(directory.path, error);
    if (error || !writeSvg(directory.path / "cached.svg")) {
        std::cerr << "failed to create cache test image\n";
        return 1;
    }

    const std::filesystem::path imagePath = directory.path / "cached.svg";
    auto decoded = awaitDecodedImage(imagePath);
    if (!decoded) {
        std::cerr << "asynchronous image decode did not complete\n";
        return 1;
    }
    const unsigned char* firstPixels = decoded->pixels.get();
    decoded.reset();
    const auto retained = core::render::image::loadStaticImageFromPath(imagePath.string(), false);
    if (!retained || retained->pixels.get() != firstPixels) {
        std::cerr << "decoded image was not retained in the memory cache\n";
        return 1;
    }
    if (!textureSurvivesAnIdleRelease(imagePath)) {
        std::cerr << "idle image texture was recreated instead of reused\n";
        return 1;
    }
    if (!remoteReadySurvivesDecodedPixelEviction()) {
        std::cerr << "remote image readiness regressed after decoded pixels were evicted\n";
        return 1;
    }
    if (!remoteTextureUploadsArePaced()) {
        std::cerr << "remote image texture uploads were not paced across frames\n";
        return 1;
    }
    return 0;
}
