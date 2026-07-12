#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace core::video {

enum class PixelFormat {
    Rgba8
};

struct Frame {
    std::shared_ptr<const std::vector<unsigned char>> pixels;
    int width = 0;
    int height = 0;
    int stride = 0;
    PixelFormat format = PixelFormat::Rgba8;
    std::uint64_t generation = 0;
};

bool publishRgbaFrame(const std::string& source,
                      int width,
                      int height,
                      const unsigned char* pixels,
                      int stride = 0);
bool latestFrame(const std::string& source, Frame& frame);
void clearFrame(const std::string& source);
void clearAllFrames();

} // namespace core::video
