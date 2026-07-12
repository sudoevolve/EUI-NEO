#pragma once

#include "core/render/image_types.h"
#include "eui/types.h"

#include <string>

namespace eui {

using ImageFit = core::ImageFit;

namespace image {

bool isSourceReady(const std::string& source);
bool consumeRemoteImageReady();
Color themeColor(const std::string& source,
                 Color fallback,
                 bool flipVertically = false,
                 bool* pending = nullptr);

} // namespace image

} // namespace eui
