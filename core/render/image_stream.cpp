#include "core/render/image_stream.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace core::render {

namespace {

std::uint32_t chromaWidth(std::uint32_t width) {
    return (width + 1u) / 2u;
}

std::uint32_t chromaHeight(std::uint32_t height) {
    return (height + 1u) / 2u;
}

bool hasBytes(const std::shared_ptr<const std::vector<std::uint8_t>>& plane,
             std::uint32_t stride,
             std::uint32_t rows) {
    if (!plane || stride == 0 || rows == 0) {
        return false;
    }
    return static_cast<std::uint64_t>(stride) * rows <= plane->size();
}

float clampUnit(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

void yuvMatrix(ImageColorSpace colorSpace, float& rv, float& gu, float& gv, float& bu) {
    switch (colorSpace) {
    case ImageColorSpace::BT601:
        rv = 1.4020f;
        gu = 0.344136f;
        gv = 0.714136f;
        bu = 1.7720f;
        return;
    case ImageColorSpace::BT2020:
        rv = 1.4746f;
        gu = 0.16455f;
        gv = 0.57135f;
        bu = 1.8814f;
        return;
    case ImageColorSpace::BT709:
    default:
        rv = 1.5748f;
        gu = 0.187324f;
        gv = 0.468124f;
        bu = 1.8556f;
        return;
    }
}

void writeYuvPixel(std::uint8_t* destination,
                   std::uint16_t y,
                   std::uint16_t u,
                   std::uint16_t v,
                   int bitDepth,
                   ImageColorSpace colorSpace,
                   ImageColorRange colorRange) {
    const float maxValue = static_cast<float>((1u << bitDepth) - 1u);
    const float yOffset = colorRange == ImageColorRange::Limited
        ? static_cast<float>(1u << (bitDepth - 4))
        : 0.0f;
    const float yDenominator = colorRange == ImageColorRange::Limited
        ? static_cast<float>(219u << (bitDepth - 8))
        : maxValue;
    const float chromaCenter = static_cast<float>(1u << (bitDepth - 1));
    const float chromaDenominator = colorRange == ImageColorRange::Limited
        ? static_cast<float>(224u << (bitDepth - 8))
        : maxValue;
    const float yValue = (static_cast<float>(y) - yOffset) / yDenominator;
    const float uValue = (static_cast<float>(u) - chromaCenter) / chromaDenominator;
    const float vValue = (static_cast<float>(v) - chromaCenter) / chromaDenominator;

    float rv = 0.0f;
    float gu = 0.0f;
    float gv = 0.0f;
    float bu = 0.0f;
    yuvMatrix(colorSpace, rv, gu, gv, bu);
    destination[0] = static_cast<std::uint8_t>(std::lround(clampUnit(yValue + rv * vValue) * 255.0f));
    destination[1] = static_cast<std::uint8_t>(std::lround(clampUnit(yValue - gu * uValue - gv * vValue) * 255.0f));
    destination[2] = static_cast<std::uint8_t>(std::lround(clampUnit(yValue + bu * uValue) * 255.0f));
    destination[3] = 255;
}

std::uint16_t p010Sample(const std::uint8_t* bytes) {
    const std::uint16_t packed = static_cast<std::uint16_t>(bytes[0]) |
                                 (static_cast<std::uint16_t>(bytes[1]) << 8u);
    return static_cast<std::uint16_t>(packed >> 6u);
}

} // namespace

bool ImageFrame::valid() const {
    if (width == 0 || height == 0 || width > 16384u || height > 16384u) {
        return false;
    }
    switch (format) {
    case ImagePixelFormat::RGBA8:
    case ImagePixelFormat::BGRA8:
        return hasBytes(pixels, stride, height) && stride >= width * 4u && stride % 4u == 0;
    case ImagePixelFormat::NV12:
        return hasBytes(pixels, stride, height) && stride >= width &&
               hasBytes(plane1, stride1, chromaHeight(height)) && stride1 >= chromaWidth(width) * 2u &&
               stride1 % 2u == 0;
    case ImagePixelFormat::I420:
        return hasBytes(pixels, stride, height) && stride >= width &&
               hasBytes(plane1, stride1, chromaHeight(height)) && stride1 >= chromaWidth(width) &&
               hasBytes(plane2, stride2, chromaHeight(height)) && stride2 >= chromaWidth(width);
    case ImagePixelFormat::P010:
        return hasBytes(pixels, stride, height) && stride >= width * 2u && stride % 2u == 0 &&
               hasBytes(plane1, stride1, chromaHeight(height)) && stride1 >= chromaWidth(width) * 4u &&
               stride1 % 4u == 0;
    default:
        return false;
    }
}

bool ImageFrame::convertToRgba8(std::vector<std::uint8_t>& output) const {
    if (!valid()) {
        output.clear();
        return false;
    }
    const std::size_t outputBytes = static_cast<std::size_t>(width) * height * 4u;
    output.resize(outputBytes);

    if (format == ImagePixelFormat::RGBA8 || format == ImagePixelFormat::BGRA8) {
        const bool bgra = format == ImagePixelFormat::BGRA8;
        for (std::uint32_t y = 0; y < height; ++y) {
            const auto* source = pixels->data() + static_cast<std::size_t>(y) * stride;
            auto* destination = output.data() + static_cast<std::size_t>(y) * width * 4u;
            for (std::uint32_t x = 0; x < width; ++x) {
                const auto* pixel = source + static_cast<std::size_t>(x) * 4u;
                destination[0] = pixel[bgra ? 2 : 0];
                destination[1] = pixel[1];
                destination[2] = pixel[bgra ? 0 : 2];
                destination[3] = pixel[3];
            }
        }
        return true;
    }

    const int bitDepth = format == ImagePixelFormat::P010 ? 10 : 8;
    for (std::uint32_t y = 0; y < height; ++y) {
        const std::uint32_t chromaY = y / 2u;
        for (std::uint32_t x = 0; x < width; ++x) {
            const std::uint32_t chromaX = x / 2u;
            std::uint16_t yValue = 0;
            std::uint16_t uValue = 0;
            std::uint16_t vValue = 0;
            if (format == ImagePixelFormat::P010) {
                yValue = p010Sample(pixels->data() + static_cast<std::size_t>(y) * stride + x * 2u);
                const auto* uv = plane1->data() + static_cast<std::size_t>(chromaY) * stride1 + chromaX * 4u;
                uValue = p010Sample(uv);
                vValue = p010Sample(uv + 2u);
            } else {
                yValue = pixels->at(static_cast<std::size_t>(y) * stride + x);
                if (format == ImagePixelFormat::NV12) {
                    const auto* uv = plane1->data() + static_cast<std::size_t>(chromaY) * stride1 + chromaX * 2u;
                    uValue = uv[0];
                    vValue = uv[1];
                } else {
                    uValue = plane1->at(static_cast<std::size_t>(chromaY) * stride1 + chromaX);
                    vValue = plane2->at(static_cast<std::size_t>(chromaY) * stride2 + chromaX);
                }
            }
            writeYuvPixel(output.data() + (static_cast<std::size_t>(y) * width + x) * 4u,
                          yValue, uValue, vValue, bitDepth, colorSpace, colorRange);
        }
    }
    return true;
}

ImageStream::ImageStream(std::size_t capacity)
    : capacity_(capacity == 0 ? 1 : capacity) {}

bool ImageStream::submit(ImageFrame frame) {
    if (!frame.valid()) {
        return false;
    }
    std::lock_guard lock(mutex_);
    if (frames_.size() >= capacity_) {
        frames_.pop_front();
    }
    frames_.push_back(std::move(frame));
    return true;
}

std::optional<ImageFrame> ImageStream::consumeLatest() {
    std::lock_guard lock(mutex_);
    if (frames_.empty()) {
        return std::nullopt;
    }
    ImageFrame frame = std::move(frames_.back());
    frames_.clear();
    return frame;
}

bool ImageStream::hasPendingFrame() const {
    std::lock_guard lock(mutex_);
    return !frames_.empty();
}

} // namespace core::render
