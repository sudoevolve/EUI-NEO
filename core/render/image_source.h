#pragma once

#include "core/render/render_types.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace core::render::image {

struct StaticImageData {
    std::shared_ptr<unsigned char> pixels;
    std::size_t byteCount = 0;
    int width = 0;
    int height = 0;
};

struct GifFrameData {
    std::vector<unsigned char> pixels;
    std::vector<int> delays;
    int width = 0;
    int height = 0;
    int frameCount = 0;
};

std::string imageCacheKey(const std::string& resolvedPath, bool flipVertically);
std::string resolveImagePath(const std::string& source, bool* pending);
bool isRemoteSource(const std::string& source);
bool isGifPath(const std::string& path);
bool isSourceReady(const std::string& source);
bool hasSourceFailed(const std::string& source);
bool retrySource(const std::string& source);
bool consumeRemoteImageReady();

std::shared_ptr<const StaticImageData> loadStaticImage(const std::string& source,
                                                       bool flipVertically,
                                                       bool* pending);
std::shared_ptr<const StaticImageData> loadStaticImageFromPath(const std::string& resolvedPath,
                                                               bool flipVertically);
std::shared_ptr<const StaticImageData> requestStaticImageFromPath(const std::string& resolvedPath,
                                                                  bool flipVertically,
                                                                  bool* pending);
core::Color sampleThemeColor(const StaticImageData& image, core::Color fallback);
core::Color sampleThemeColor(const std::string& source,
                             core::Color fallback,
                             bool flipVertically = false,
                             bool* pending = nullptr);
std::shared_ptr<const StaticImageData> loadStaticSvg(const std::string& cacheKey,
                                                     const std::string& svg,
                                                     bool flipVertically);
std::shared_ptr<const GifFrameData> loadGifFrames(const std::string& resolvedPath,
                                                  bool flipVertically);

} // namespace core::render::image
