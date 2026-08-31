#pragma once

#include "eui/types.h"
#include "core/render/image_stream.h"

#include <string>

namespace eui {

using ImageFit = core::ImageFit;
using ImagePixelFormat = core::render::ImagePixelFormat;
using ImageColorSpace = core::render::ImageColorSpace;
using ImageColorRange = core::render::ImageColorRange;
using ImageFrame = core::render::ImageFrame;
using ImageStream = core::render::ImageStream;

namespace image {

bool isSourceReady(const std::string& source);
bool hasSourceFailed(const std::string& source);
bool retrySource(const std::string& source);
bool consumeRemoteImageReady();
Color themeColor(const std::string& source,
                 Color fallback,
                 bool flipVertically = false,
                 bool* pending = nullptr);

} // namespace image

} // namespace eui
