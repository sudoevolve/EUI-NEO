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
    if (!stream->submit({pixels, 4, 2, 16, core::render::ImagePixelFormat::BGRA8, 6})) {
        return 1;
    }
    auto bgraLatest = stream->consumeLatest();
    if (!bgraLatest || bgraLatest->sequence != 6 || bgraLatest->format != core::render::ImagePixelFormat::BGRA8) {
        return 1;
    }

    std::vector<std::uint8_t> converted;
    const auto bgra = std::make_shared<const std::vector<std::uint8_t>>(
        std::vector<std::uint8_t>{1, 2, 3, 255});
    const core::render::ImageFrame bgraFrame{bgra, 1, 1, 4, core::render::ImagePixelFormat::BGRA8, 0};
    if (!bgraFrame.convertToRgba8(converted) || converted != std::vector<std::uint8_t>{3, 2, 1, 255}) {
        return 1;
    }
    if (!stream->submit({bgra, 1, 1, 4, core::render::ImagePixelFormat::BGRA8, 7})) {
        return 1;
    }
    auto bgraPixelFrame = stream->consumeLatest();
    if (!bgraPixelFrame || !bgraPixelFrame->convertToRgba8(converted) ||
        converted != std::vector<std::uint8_t>{3, 2, 1, 255}) {
        return 1;
    }

    // 回归测试：多像素 BGRA8/RGBA8 转换必须逐像素前进，不能只写行首 4 字节。
    // 历史 bug：内层循环的 destination 指针从不前进，导致每行只剩最后一个像素。
    {
        // 4x1 BGRA8：源 4 个像素 B/G/R/A 各不相同
        const auto bgraWide = std::make_shared<const std::vector<std::uint8_t>>(
            std::vector<std::uint8_t>{
                1, 2, 3, 255,     // BGRA -> RGBA (3,2,1,255)
                4, 5, 6, 255,     // BGRA -> RGBA (6,5,4,255)
                7, 8, 9, 255,     // BGRA -> RGBA (9,8,7,255)
                10, 11, 12, 255,  // BGRA -> RGBA (12,11,10,255)
            });
        const core::render::ImageFrame bgraWideFrame{
            bgraWide, 4, 1, 16, core::render::ImagePixelFormat::BGRA8, 0};
        if (!bgraWideFrame.convertToRgba8(converted) ||
            converted != std::vector<std::uint8_t>{
                            3, 2, 1, 255, 6, 5, 4, 255, 9, 8, 7, 255, 12, 11, 10, 255}) {
            return 1;
        }
    }
    {
        // 2x2 RGBA8：源 4 个像素 R/G/B/A 各不相同
        const auto rgbaWide = std::make_shared<const std::vector<std::uint8_t>>(
            std::vector<std::uint8_t>{
                10, 20, 30, 255, 40, 50, 60, 255,
                70, 80, 90, 255, 100, 110, 120, 255,
            });
        const core::render::ImageFrame rgbaWideFrame{
            rgbaWide, 2, 2, 8, core::render::ImagePixelFormat::RGBA8, 0};
        if (!rgbaWideFrame.convertToRgba8(converted) ||
            converted != std::vector<std::uint8_t>{
                            10, 20, 30, 255, 40, 50, 60, 255,
                            70, 80, 90, 255, 100, 110, 120, 255}) {
            return 1;
        }
    }

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
