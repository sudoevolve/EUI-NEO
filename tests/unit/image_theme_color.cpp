#include "core/render/image_source.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

bool closeColor(const core::Color& left, const core::Color& right) {
    constexpr float epsilon = 0.0001f;
    return std::fabs(left.r - right.r) < epsilon &&
           std::fabs(left.g - right.g) < epsilon &&
           std::fabs(left.b - right.b) < epsilon &&
           std::fabs(left.a - right.a) < epsilon;
}

bool writeSvg(const std::filesystem::path& path, const std::string& fill, const std::string& extra = {}) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output.good()) {
        return false;
    }
    output << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"8\" height=\"8\">"
           << "<rect width=\"8\" height=\"8\" fill=\"" << fill << "\"/>"
           << extra
           << "</svg>";
    return output.good();
}

bool transparentFallbackIsNotCached(const std::filesystem::path& path) {
    if (!writeSvg(path, "none")) {
        return false;
    }

    const core::Color firstFallback{0.9f, 0.1f, 0.2f, 1.0f};
    const core::Color secondFallback{0.2f, 0.3f, 0.9f, 1.0f};
    const core::Color first = core::render::image::sampleThemeColor(path.string(), firstFallback);
    const core::Color second = core::render::image::sampleThemeColor(path.string(), secondFallback);
    return closeColor(first, firstFallback) && closeColor(second, secondFallback);
}

bool changedFileIsResampled(const std::filesystem::path& path) {
    if (!writeSvg(path, "#cc2020")) {
        return false;
    }
    const core::Color fallback{0.5f, 0.5f, 0.5f, 1.0f};
    const core::Color red = core::render::image::sampleThemeColor(path.string(), fallback);

    if (!writeSvg(path, "#2050cc", "<!-- version-two -->")) {
        return false;
    }
    const core::Color blue = core::render::image::sampleThemeColor(path.string(), fallback);
    return red.r > red.b && blue.b > blue.r;
}

} // namespace

int main() {
    std::error_code error;
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path(error) / "eui_neo_image_theme_color_test";
    if (error) {
        std::cerr << "failed to locate temp directory: " << error.message() << "\n";
        return 1;
    }

    std::filesystem::create_directories(directory, error);
    if (error) {
        std::cerr << "failed to create temp directory: " << error.message() << "\n";
        return 1;
    }

    const bool fallbackOk = transparentFallbackIsNotCached(directory / "transparent.svg");
    const bool refreshOk = changedFileIsResampled(directory / "versioned.svg");
    std::filesystem::remove_all(directory, error);

    if (!fallbackOk) {
        std::cerr << "transparent image cached a caller fallback\n";
        return 1;
    }
    if (!refreshOk) {
        std::cerr << "changed image file did not refresh its sampled color\n";
        return 1;
    }
    return 0;
}