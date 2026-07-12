#pragma once

#include "core/render/video_source.h"

#include <string>

namespace eui::video {

inline bool publishRgbaFrame(const std::string& source,
                             int width,
                             int height,
                             const unsigned char* pixels,
                             int stride = 0) {
    return core::video::publishRgbaFrame(source, width, height, pixels, stride);
}

inline void clearFrame(const std::string& source) {
    core::video::clearFrame(source);
}

inline void clearAllFrames() {
    core::video::clearAllFrames();
}

} // namespace eui::video
