#include "core/render/image_stream.h"

#include <cassert>
#include <cstdint>
#include <memory>
#include <vector>

namespace {

std::shared_ptr<const std::vector<std::uint8_t>> bytes(std::initializer_list<std::uint8_t> values) {
    return std::make_shared<const std::vector<std::uint8_t>>(values);
}

} // namespace

int main() {
    auto stream = std::make_shared<core::render::ImageStream>(2);
    auto pixels = std::make_shared<const std::vector<std::uint8_t>>(
        std::vector<std::uint8_t>(4u * 2u * 4u, 0x7f));

    assert(stream->submit({pixels, 4, 2, 16, core::render::ImagePixelFormat::RGBA8, 1}));
    assert(stream->hasPendingFrame());
    auto first = stream->consumeLatest();
    assert(first && first->sequence == 1);
    assert(!stream->hasPendingFrame());

    assert(stream->submit({pixels, 4, 2, 16, core::render::ImagePixelFormat::RGBA8, 2}));
    assert(stream->submit({pixels, 4, 2, 16, core::render::ImagePixelFormat::RGBA8, 3}));
    assert(stream->submit({pixels, 4, 2, 16, core::render::ImagePixelFormat::RGBA8, 4}));
    auto latest = stream->consumeLatest();
    assert(latest && latest->sequence == 4);

    assert(!stream->submit({pixels, 4, 2, 15, core::render::ImagePixelFormat::RGBA8, 5}));
    assert(!stream->submit({pixels, 4, 2, 16, core::render::ImagePixelFormat::BGRA8, 6}));

    std::vector<std::uint8_t> converted;
    const auto bgra = std::make_shared<const std::vector<std::uint8_t>>(
        std::vector<std::uint8_t>{1, 2, 3, 255});
    const core::render::ImageFrame bgraFrame{bgra, 1, 1, 4, core::render::ImagePixelFormat::BGRA8, 0};
    assert(bgraFrame.convertToRgba8(converted));
    assert((converted == std::vector<std::uint8_t>{3, 2, 1, 255}));

    const auto nv12Y = bytes({128});
    const auto nv12UV = bytes({128, 128});
    core::render::ImageFrame nv12{nv12Y, 1, 1, 1, core::render::ImagePixelFormat::NV12, 0,
                                  nv12UV, nullptr, 2, 0,
                                  core::render::ImageColorSpace::BT709,
                                  core::render::ImageColorRange::Full};
    assert(nv12.valid());
    assert(nv12.convertToRgba8(converted));
    assert((converted == std::vector<std::uint8_t>{128, 128, 128, 255}));

    const auto i420Y = bytes({128});
    const auto i420U = bytes({128});
    const auto i420V = bytes({128});
    core::render::ImageFrame i420{i420Y, 1, 1, 1, core::render::ImagePixelFormat::I420, 0,
                                  i420U, i420V, 1, 1,
                                  core::render::ImageColorSpace::BT601,
                                  core::render::ImageColorRange::Full};
    assert(i420.convertToRgba8(converted));
    assert(converted[0] == 128 && converted[1] == 128 && converted[2] == 128);

    const auto p010Y = bytes({0, 128});
    const auto p010UV = bytes({0, 128, 0, 128});
    core::render::ImageFrame p010{p010Y, 1, 1, 2, core::render::ImagePixelFormat::P010, 0,
                                  p010UV, nullptr, 4, 0,
                                  core::render::ImageColorSpace::BT2020,
                                  core::render::ImageColorRange::Full};
    assert(p010.convertToRgba8(converted));
    assert((converted == std::vector<std::uint8_t>{128, 128, 128, 255}));
    return 0;
}
