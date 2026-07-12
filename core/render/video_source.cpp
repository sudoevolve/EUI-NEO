#include "core/render/video_source.h"

#include "core/platform/platform.h"

#include <algorithm>
#include <mutex>
#include <unordered_map>

namespace core::video {
namespace {

struct SourceState {
    Frame frame;
    std::uint64_t generation = 0;
};

std::mutex gVideoMutex;
std::unordered_map<std::string, SourceState> gVideoSources;

} // namespace

bool publishRgbaFrame(const std::string& source,
                      int width,
                      int height,
                      const unsigned char* pixels,
                      int stride) {
    if (source.empty() || width <= 0 || height <= 0 || pixels == nullptr) {
        return false;
    }

    const int sourceStride = stride > 0 ? stride : width * 4;
    if (sourceStride < width * 4) {
        return false;
    }

    auto packed = std::make_shared<std::vector<unsigned char>>();
    const std::size_t rowBytes = static_cast<std::size_t>(width) * 4u;
    packed->resize(rowBytes * static_cast<std::size_t>(height));
    for (int y = 0; y < height; ++y) {
        const unsigned char* src = pixels + static_cast<std::size_t>(sourceStride) * static_cast<std::size_t>(y);
        unsigned char* dst = packed->data() + rowBytes * static_cast<std::size_t>(y);
        std::copy_n(src, rowBytes, dst);
    }

    {
        std::lock_guard<std::mutex> lock(gVideoMutex);
        SourceState& state = gVideoSources[source];
        state.frame.pixels = std::move(packed);
        state.frame.width = width;
        state.frame.height = height;
        state.frame.stride = width * 4;
        state.frame.format = PixelFormat::Rgba8;
        state.frame.generation = ++state.generation;
    }

    core::platform::requestUiUpdate();
    return true;
}

bool latestFrame(const std::string& source, Frame& frame) {
    std::lock_guard<std::mutex> lock(gVideoMutex);
    const auto item = gVideoSources.find(source);
    if (item == gVideoSources.end() || !item->second.frame.pixels) {
        return false;
    }
    frame = item->second.frame;
    return true;
}

void clearFrame(const std::string& source) {
    {
        std::lock_guard<std::mutex> lock(gVideoMutex);
        gVideoSources.erase(source);
    }
    core::platform::requestUiUpdate();
}

void clearAllFrames() {
    {
        std::lock_guard<std::mutex> lock(gVideoMutex);
        gVideoSources.clear();
    }
    core::platform::requestUiUpdate();
}

} // namespace core::video
