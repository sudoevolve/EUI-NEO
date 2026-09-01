#include "eui_neo.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace app {
namespace {

std::shared_ptr<eui::ImageStream> stream() {
    static auto value = std::make_shared<eui::ImageStream>(2);
    return value;
}

std::uint64_t nextSequence() {
    static std::uint64_t value = 0;
    return value++;
}

void writeP010Sample(std::vector<std::uint8_t>& plane, std::size_t offset, std::uint16_t sample) {
    const std::uint16_t packed = static_cast<std::uint16_t>(sample << 6u);
    plane[offset] = static_cast<std::uint8_t>(packed & 0xffu);
    plane[offset + 1] = static_cast<std::uint8_t>(packed >> 8u);
}

void submitDemoFrame(std::uint64_t sequence) {
    constexpr std::uint32_t width = 640;
    constexpr std::uint32_t height = 360;
    const std::uint64_t formatPhase = (sequence / 120u) % 3u;
    auto y = std::make_shared<std::vector<std::uint8_t>>(width * height);
    for (std::uint32_t row = 0; row < height; ++row) {
        for (std::uint32_t column = 0; column < width; ++column) {
            // Move the gradient several pixels per frame so motion is obvious
            // even in a low-rate screen capture.
            (*y)[row * width + column] = static_cast<std::uint8_t>((column + sequence * 4u) % 256u);
        }
    }
    if (formatPhase == 0) {
        auto uv = std::make_shared<std::vector<std::uint8_t>>(width * height / 2u);
        for (std::size_t index = 0; index < uv->size(); index += 2) {
            (*uv)[index] = 96;
            (*uv)[index + 1] = 196;
        }
        stream()->submit({y, width, height, width, eui::ImagePixelFormat::NV12, sequence,
                          uv, nullptr, width, 0,
                          eui::ImageColorSpace::BT709, eui::ImageColorRange::Limited});
        return;
    }
    if (formatPhase == 1) {
        auto u = std::make_shared<std::vector<std::uint8_t>>(width * height / 4u, 96);
        auto v = std::make_shared<std::vector<std::uint8_t>>(width * height / 4u, 196);
        stream()->submit({y, width, height, width, eui::ImagePixelFormat::I420, sequence,
                          u, v, width / 2u, width / 2u,
                          eui::ImageColorSpace::BT601, eui::ImageColorRange::Full});
        return;
    }

    auto y10 = std::make_shared<std::vector<std::uint8_t>>(width * height * 2u);
    auto uv10 = std::make_shared<std::vector<std::uint8_t>>(width * height);
    for (std::uint32_t row = 0; row < height; ++row) {
        for (std::uint32_t column = 0; column < width; ++column) {
            const std::uint16_t value = static_cast<std::uint16_t>(((*y)[row * width + column]) << 2u);
            writeP010Sample(*y10, (static_cast<std::size_t>(row) * width + column) * 2u, value);
        }
    }
    for (std::size_t index = 0; index < uv10->size(); index += 4) {
        writeP010Sample(*uv10, index, 384);
        writeP010Sample(*uv10, index + 2, 784);
    }
    stream()->submit({y10, width, height, width * 2u, eui::ImagePixelFormat::P010, sequence,
                      uv10, nullptr, width * 2u, 0,
                      eui::ImageColorSpace::BT2020, eui::ImageColorRange::Limited});
}

} // namespace

const DslAppConfig& dslAppConfig() {
    static const DslAppConfig config = DslAppConfig{}
        .title("Dynamic Texture")
        .pageId("dynamic_texture")
        .clearColor({0.035f, 0.045f, 0.060f, 1.0f})
        .windowSize(900, 560)
        .fps(60.0);
    return config;
}

void compose(eui::Ui& ui, const eui::Screen& screen) {
    // Seed the stream before the first render. Subsequent frames are produced
    // by the retained onFrame callback below; compose() is not a render loop.
    static bool seeded = false;
    if (!seeded) {
        submitDemoFrame(nextSequence());
        seeded = true;
    }

    const float width = screen.width - 64.0f;
    const float height = width * 9.0f / 16.0f;
    ui.stack("page").size(screen.width, screen.height).align(eui::Align::CENTER, eui::Align::CENTER)
        .content([&] {
            ui.image("stream")
                .size(width, height)
                .stream(stream())
                .fit(eui::ImageFit::Contain)
                .onFrame([](float) {
                    submitDemoFrame(nextSequence());
                })
                .build();
        }).build();
}

} // namespace app
