#include "eui/image.h"

#include "core/render/image.h"
#include "core/render/image_source.h"

namespace eui::image {

bool isSourceReady(const std::string& source) {
    return core::ImagePrimitive::isSourceReady(source);
}

bool consumeRemoteImageReady() {
    return core::ImagePrimitive::consumeRemoteImageReady();
}

Color themeColor(const std::string& source, Color fallback, bool flipVertically, bool* pending) {
    return core::render::image::sampleThemeColor(source, fallback, flipVertically, pending);
}

} // namespace eui::image
