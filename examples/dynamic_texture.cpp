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

void submitDemoFrame(std::uint64_t sequence) {
    constexpr std::uint32_t width = 640;
    constexpr std::uint32_t height = 360;
    auto y = std::make_shared<std::vector<std::uint8_t>>(width * height);
    auto uv = std::make_shared<std::vector<std::uint8_t>>(width * height / 2u);
    for (std::uint32_t row = 0; row < height; ++row) {
        for (std::uint32_t column = 0; column < width; ++column) {
            // Move the gradient several pixels per frame so motion is obvious
            // even in a low-rate screen capture.
            (*y)[row * width + column] = static_cast<std::uint8_t>((column + sequence * 4u) % 256u);
        }
    }
    for (std::size_t index = 0; index < uv->size(); index += 2) {
        (*uv)[index] = 96;
        (*uv)[index + 1] = 196;
    }
    stream()->submit({y, width, height, width, eui::ImagePixelFormat::NV12, sequence,
                      uv, nullptr, width, 0,
                      eui::ImageColorSpace::BT709, eui::ImageColorRange::Limited});
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
