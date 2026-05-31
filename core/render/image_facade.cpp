#include "eui/image.h"

#include "core/render/image.h"

namespace eui::image {

bool isSourceReady(const std::string& source) {
    return core::ImagePrimitive::isSourceReady(source);
}

bool consumeRemoteImageReady() {
    return core::ImagePrimitive::consumeRemoteImageReady();
}

} // namespace eui::image
