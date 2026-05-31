#pragma once

#include "core/render/image_types.h"

#include <string>

namespace eui {

using ImageFit = core::ImageFit;

namespace image {

bool isSourceReady(const std::string& source);
bool consumeRemoteImageReady();

} // namespace image

} // namespace eui
