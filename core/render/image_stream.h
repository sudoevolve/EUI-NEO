#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace core::render {

enum class ImagePixelFormat {
    RGBA8,
    BGRA8,
    NV12,
    I420,
    P010,
};

enum class ImageColorSpace {
    BT601,
    BT709,
    BT2020,
};

enum class ImageColorRange {
    Limited,
    Full,
};

struct ImageFrame {
    std::shared_ptr<const std::vector<std::uint8_t>> pixels;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t stride = 0;
    ImagePixelFormat format = ImagePixelFormat::RGBA8;
    std::uint64_t sequence = 0;
    std::shared_ptr<const std::vector<std::uint8_t>> plane1;
    std::shared_ptr<const std::vector<std::uint8_t>> plane2;
    std::uint32_t stride1 = 0;
    std::uint32_t stride2 = 0;
    ImageColorSpace colorSpace = ImageColorSpace::BT709;
    ImageColorRange colorRange = ImageColorRange::Limited;

    bool valid() const;
    bool convertToRgba8(std::vector<std::uint8_t>& output) const;
};

// Thread-safe producer/consumer mailbox for decoded image frames. The queue
// is intentionally bounded so a slow renderer cannot accumulate latency.
class ImageStream {
public:
    explicit ImageStream(std::size_t capacity = 2);

    bool submit(ImageFrame frame);
    std::optional<ImageFrame> consumeLatest();
    bool hasPendingFrame() const;
    std::size_t capacity() const { return capacity_; }

private:
    std::size_t capacity_;
    mutable std::mutex mutex_;
    std::deque<ImageFrame> frames_;
};

} // namespace core::render
