#include "core/render/image.h"
#include "core/render/image_stream.h"
#include "core/render/render_backend.h"

#include <cassert>
#include <cstdint>
#include <memory>
#include <unordered_set>
#include <vector>

namespace {

using core::render::ImageColorRange;
using core::render::ImageColorSpace;
using core::render::ImageFrame;
using core::render::ImagePixelFormat;
using core::render::RenderBackend;

class DynamicTextureBackend final : public RenderBackend {
public:
    ~DynamicTextureBackend() override {
        for (TextureHandle handle : textures_) {
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

    TextureHandle createTexture(const unsigned char* pixels, int width, int height) override {
        ++rgbaCreates;
        copyRgba(pixels, width, height);
        return createHandle();
    }

    bool updateTexture(TextureHandle handle, const unsigned char* pixels, int width, int height) override {
        ++rgbaUpdates;
        copyRgba(pixels, width, height);
        return textures_.find(handle) != textures_.end();
    }

    TextureHandle createDynamicTexture(const ImageFrame& frame) override {
        ++nativeCreates;
        nativeFormats.push_back(frame.format);
        return nativeEnabled ? createHandle() : nullptr;
    }

    bool updateDynamicTexture(TextureHandle handle, const ImageFrame&) override {
        ++nativeUpdates;
        return !failNativeUpdates && textures_.find(handle) != textures_.end();
    }

    void destroyTexture(TextureHandle handle) override {
        const auto found = textures_.find(handle);
        if (found == textures_.end()) {
            return;
        }
        delete static_cast<int*>(handle);
        textures_.erase(found);
        ++destroys;
    }

    bool nativeEnabled = true;
    bool failNativeUpdates = false;
    int nativeCreates = 0;
    int nativeUpdates = 0;
    int rgbaCreates = 0;
    int rgbaUpdates = 0;
    int destroys = 0;
    std::vector<ImagePixelFormat> nativeFormats;
    std::vector<std::uint8_t> lastRgba;

private:
    TextureHandle createHandle() {
        TextureHandle handle = new int(++nextTexture_);
        textures_.insert(handle);
        return handle;
    }

    void copyRgba(const unsigned char* pixels, int width, int height) {
        if (pixels == nullptr || width <= 0 || height <= 0) {
            lastRgba.clear();
            return;
        }
        const std::size_t bytes = static_cast<std::size_t>(width) * height * 4u;
        lastRgba.assign(pixels, pixels + bytes);
    }

    int nextTexture_ = 0;
    std::unordered_set<TextureHandle> textures_;
};

std::shared_ptr<const std::vector<std::uint8_t>> bytes(std::initializer_list<std::uint8_t> values) {
    return std::make_shared<const std::vector<std::uint8_t>>(values);
}

ImageFrame nv12Frame(std::uint64_t sequence, std::uint32_t width = 1) {
    return {std::make_shared<const std::vector<std::uint8_t>>(width, 128), width, 1, width,
            ImagePixelFormat::NV12, sequence,
            bytes({128, 128}), nullptr, 2, 0,
            ImageColorSpace::BT709, ImageColorRange::Full};
}

ImageFrame i420Frame(std::uint64_t sequence) {
    return {bytes({128}), 1, 1, 1, ImagePixelFormat::I420, sequence,
            bytes({128}), bytes({128}), 1, 1,
            ImageColorSpace::BT601, ImageColorRange::Full};
}

ImageFrame p010Frame(std::uint64_t sequence) {
    return {bytes({0, 128}), 1, 1, 2, ImagePixelFormat::P010, sequence,
            bytes({0, 128, 0, 128}), nullptr, 4, 0,
            ImageColorSpace::BT2020, ImageColorRange::Full};
}

void submitAndRender(core::ImagePrimitive& image,
                     const std::shared_ptr<core::render::ImageStream>& stream,
                     ImageFrame frame) {
    assert(stream->submit(std::move(frame)));
    assert(image.updateTexture());
    image.render(1, 1);
}

void nativeDynamicTexturesRecreateForFormatChanges() {
    DynamicTextureBackend backend;
    core::render::ScopedRenderBackend scopedBackend(backend);
    auto stream = std::make_shared<core::render::ImageStream>();
    core::ImagePrimitive image;
    image.initialize();
    image.setStream(stream);
    image.setBounds(0.0f, 0.0f, 1.0f, 1.0f);

    submitAndRender(image, stream, nv12Frame(1));
    submitAndRender(image, stream, nv12Frame(2, 2));
    submitAndRender(image, stream, i420Frame(3));
    submitAndRender(image, stream, p010Frame(4));
    submitAndRender(image, stream, p010Frame(5));

    assert(backend.nativeCreates == 4);
    assert(backend.nativeUpdates == 1);
    assert(backend.rgbaCreates == 0);
    assert((backend.nativeFormats == std::vector<ImagePixelFormat>{
        ImagePixelFormat::NV12, ImagePixelFormat::NV12, ImagePixelFormat::I420,
        ImagePixelFormat::P010}));
    assert(backend.destroys == 3);
    image.destroy();
    assert(backend.destroys == 4);
}

void unsupportedNativeTexturesUseRgbaFallback() {
    DynamicTextureBackend backend;
    backend.nativeEnabled = false;
    core::render::ScopedRenderBackend scopedBackend(backend);
    auto stream = std::make_shared<core::render::ImageStream>();
    core::ImagePrimitive image;
    image.initialize();
    image.setStream(stream);
    image.setBounds(0.0f, 0.0f, 1.0f, 1.0f);

    submitAndRender(image, stream, p010Frame(1));
    submitAndRender(image, stream, p010Frame(2));

    assert(backend.nativeCreates == 1);
    assert(backend.rgbaCreates == 1);
    assert(backend.rgbaUpdates == 1);
    assert((backend.lastRgba == std::vector<std::uint8_t>{128, 128, 128, 255}));
    image.destroy();
    assert(backend.destroys == 1);
}

void failedNativeUpdateFallsBackToRgba() {
    DynamicTextureBackend backend;
    core::render::ScopedRenderBackend scopedBackend(backend);
    auto stream = std::make_shared<core::render::ImageStream>();
    core::ImagePrimitive image;
    image.initialize();
    image.setStream(stream);
    image.setBounds(0.0f, 0.0f, 1.0f, 1.0f);

    submitAndRender(image, stream, nv12Frame(1));
    backend.failNativeUpdates = true;
    submitAndRender(image, stream, nv12Frame(2));

    assert(backend.nativeCreates == 1);
    assert(backend.nativeUpdates == 1);
    assert(backend.rgbaCreates == 1);
    assert((backend.lastRgba == std::vector<std::uint8_t>{128, 128, 128, 255}));
    image.destroy();
    assert(backend.destroys == 2);
}

} // namespace

int main() {
    nativeDynamicTexturesRecreateForFormatChanges();
    unsupportedNativeTexturesUseRgbaFallback();
    failedNativeUpdateFallsBackToRgba();
    return 0;
}
