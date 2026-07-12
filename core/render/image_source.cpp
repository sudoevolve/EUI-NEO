#include "core/render/image_source.h"

#include "core/platform/async.h"
#include "core/platform/network.h"

#include "3rd/stb_image.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4244)
#endif
#define NANOSVG_IMPLEMENTATION
#define NANOSVGRAST_IMPLEMENTATION
#include "3rd/nanosvg.h"
#include "3rd/nanosvgrast.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <mutex>
#include <sstream>
#include <unordered_map>

namespace core::render::image {
namespace {

std::unordered_map<std::string, std::weak_ptr<const StaticImageData>> gStaticImageCache;
std::unordered_map<std::string, std::weak_ptr<const GifFrameData>> gGifCache;
struct ThemeColorCacheEntry {
    std::string imageVersionKey;
    core::Color color;
};

std::unordered_map<std::string, ThemeColorCacheEntry> gThemeColorCache;
std::unordered_map<std::string, std::string> gDownloadedPathCache;
std::unordered_map<std::string, bool> gDownloadInFlight;
std::unordered_map<std::string, bool> gDownloadFailed;
std::mutex gRemoteMutex;
std::atomic<bool> gRemoteImageReady{false};

std::string lowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool hasSvgExtension(const std::string& path) {
    return lowerCopy(std::filesystem::path(path).extension().string()) == ".svg";
}

bool hasGifExtension(const std::string& path) {
    return lowerCopy(std::filesystem::path(path).extension().string()) == ".gif";
}

bool looksLikeSvgFile(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.good()) {
        return false;
    }

    char buffer[512] = {};
    input.read(buffer, sizeof(buffer) - 1);
    std::string head(buffer, static_cast<std::size_t>(input.gcount()));
    head = lowerCopy(head);
    const std::size_t first = head.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return false;
    }
    head = head.substr(first);
    return head.rfind("<svg", 0) == 0 || head.find("<svg") != std::string::npos;
}

bool looksLikeGifFile(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.good()) {
        return false;
    }
    char buffer[6] = {};
    input.read(buffer, sizeof(buffer));
    if (input.gcount() != static_cast<std::streamsize>(sizeof(buffer))) {
        return false;
    }
    return std::string(buffer, sizeof(buffer)) == "GIF87a" || std::string(buffer, sizeof(buffer)) == "GIF89a";
}

std::filesystem::path executableDirectory() {
#ifdef _WIN32
    std::vector<char> buffer(MAX_PATH);
    DWORD length = 0;
    while (true) {
        length = GetModuleFileNameA(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            return {};
        }
        if (length < buffer.size() - 1) {
            break;
        }
        buffer.resize(buffer.size() * 2);
    }
    return std::filesystem::path(buffer.data()).parent_path();
#elif defined(__APPLE__)
    std::vector<char> buffer(4096);
    uint32_t size = static_cast<uint32_t>(buffer.size());
    if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
        buffer.resize(size);
        if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
            return {};
        }
    }
    std::error_code error;
    return std::filesystem::absolute(std::filesystem::path(buffer.data()), error).parent_path();
#elif defined(__linux__)
    std::vector<char> buffer(4096);
    const ssize_t length = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
    if (length <= 0) {
        return {};
    }
    buffer[static_cast<std::size_t>(length)] = '\0';
    std::error_code error;
    return std::filesystem::absolute(std::filesystem::path(buffer.data()), error).parent_path();
#else
    return {};
#endif
}

std::filesystem::path sourceRootDirectory() {
    return std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
}

std::string urlExtension(const std::string& url) {
    const std::size_t query = url.find_first_of("?#");
    const std::string pathPart = url.substr(0, query == std::string::npos ? std::string::npos : query);
    const std::size_t slash = pathPart.find_last_of('/');
    const std::size_t dot = pathPart.find_last_of('.');
    std::string ext;
    if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) {
        ext = lowerCopy(pathPart.substr(dot));
    }
    if (ext == ".svg" || ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
        ext == ".webp" || ext == ".bmp" || ext == ".gif") {
        return ext;
    }
    return ".cache";
}

std::string buildDownloadedImagePath(const std::string& url) {
    return network::cacheFilePath(url, urlExtension(url), "eui_test_image_cache");
}

bool isBingDailyScheme(const std::string& source) {
    return source.rfind("bing://daily", 0) == 0;
}

std::string queryParamValue(const std::string& uri, const std::string& key) {
    const std::size_t queryPos = uri.find('?');
    if (queryPos == std::string::npos || queryPos + 1 >= uri.size()) {
        return {};
    }

    const std::string query = uri.substr(queryPos + 1);
    std::size_t pos = 0;
    while (pos < query.size()) {
        const std::size_t next = query.find('&', pos);
        const std::string token = query.substr(pos, next == std::string::npos ? std::string::npos : next - pos);
        const std::size_t eq = token.find('=');
        if (token.substr(0, eq) == key) {
            if (eq == std::string::npos || eq + 1 >= token.size()) {
                return {};
            }
            return token.substr(eq + 1);
        }
        if (next == std::string::npos) {
            break;
        }
        pos = next + 1;
    }
    return {};
}

std::string buildBingDailyApiUrl(const std::string& uri) {
    std::string idx = queryParamValue(uri, "idx");
    std::string mkt = queryParamValue(uri, "mkt");
    if (idx.empty()) {
        idx = "0";
    }
    if (mkt.empty()) {
        mkt = "zh-CN";
    }
    return "https://www.bing.com/HPImageArchive.aspx?format=js&n=1&idx=" + idx + "&mkt=" + mkt;
}

std::string jsonUnescapeSimple(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '\\' && i + 1 < value.size()) {
            const char next = value[i + 1];
            if (next == '/' || next == '\\' || next == '"') {
                out.push_back(next);
                ++i;
                continue;
            }
        }
        out.push_back(value[i]);
    }
    return out;
}

std::string extractBingImageUrlFromJson(const std::string& payload) {
    const std::string token = "\"url\":\"";
    const std::size_t begin = payload.find(token);
    if (begin == std::string::npos) {
        return {};
    }
    const std::size_t urlBegin = begin + token.size();
    const std::size_t end = payload.find('"', urlBegin);
    if (end == std::string::npos || end <= urlBegin) {
        return {};
    }
    std::string imageUrl = jsonUnescapeSimple(payload.substr(urlBegin, end - urlBegin));
    if (!network::isHttpUrl(imageUrl)) {
        imageUrl = "https://www.bing.com" + imageUrl;
    }
    return imageUrl;
}

std::string resolveRemoteImagePath(const std::string& url, bool* pending) {
    if (pending != nullptr) {
        *pending = false;
    }

    const std::string localPath = buildDownloadedImagePath(url);
    if (localPath.empty()) {
        return {};
    }

    {
        std::lock_guard<std::mutex> lock(gRemoteMutex);
        const auto cached = gDownloadedPathCache.find(url);
        if (cached != gDownloadedPathCache.end() && std::filesystem::exists(cached->second)) {
            return cached->second;
        }
        if (std::filesystem::exists(localPath)) {
            gDownloadedPathCache[url] = localPath;
            return localPath;
        }
        if (gDownloadFailed[url]) {
            return {};
        }
        if (gDownloadInFlight[url]) {
            if (pending != nullptr) {
                *pending = true;
            }
            return {};
        }
        gDownloadInFlight[url] = true;
        if (pending != nullptr) {
            *pending = true;
        }
    }

    const bool started = async::restart(
        "image.remote." + url,
        [url, localPath](const async::CancelToken& token) {
            return network::downloadUrlToFile(url, localPath, &token);
        },
        [url, localPath](const async::Result<bool>& result) {
            const bool ok = result.ok && result.value;
            {
                std::lock_guard<std::mutex> lock(gRemoteMutex);
                gDownloadInFlight.erase(url);
                if (ok && std::filesystem::exists(localPath)) {
                    gDownloadedPathCache[url] = localPath;
                    gDownloadFailed.erase(url);
                } else {
                    gDownloadFailed[url] = true;
                    std::remove(localPath.c_str());
                }
            }
            gRemoteImageReady.store(true);
        });
    if (!started) {
        std::lock_guard<std::mutex> lock(gRemoteMutex);
        gDownloadInFlight.erase(url);
        gDownloadFailed[url] = true;
    }
    return {};
}

std::string resolveBingImagePath(const std::string& uri, bool* pending) {
    if (pending != nullptr) {
        *pending = false;
    }

    {
        std::lock_guard<std::mutex> lock(gRemoteMutex);
        const auto cached = gDownloadedPathCache.find(uri);
        if (cached != gDownloadedPathCache.end() && std::filesystem::exists(cached->second)) {
            return cached->second;
        }
        if (gDownloadFailed[uri]) {
            return {};
        }
        if (gDownloadInFlight[uri]) {
            if (pending != nullptr) {
                *pending = true;
            }
            return {};
        }
        gDownloadInFlight[uri] = true;
        if (pending != nullptr) {
            *pending = true;
        }
    }

    struct BingDownloadResult {
        bool ok = false;
        std::string imageUrl;
        std::string localPath;
    };

    const bool started = async::restart(
        "image.bing." + uri,
        [uri](const async::CancelToken& token) {
            BingDownloadResult result;
            std::string payload;
            result.ok = network::downloadUrlToString(buildBingDailyApiUrl(uri), payload, &token);
            if (result.ok && !token.canceled()) {
                result.imageUrl = extractBingImageUrlFromJson(payload);
                result.localPath = buildDownloadedImagePath(result.imageUrl);
                if (result.imageUrl.empty() || result.localPath.empty()) {
                    result.ok = false;
                } else if (!std::filesystem::exists(result.localPath)) {
                    result.ok = network::downloadUrlToFile(result.imageUrl, result.localPath, &token);
                }
            }
            return result;
        },
        [uri](const async::Result<BingDownloadResult>& result) {
            const bool ok = result.ok && result.value.ok;
            const std::string& imageUrl = result.value.imageUrl;
            const std::string& localPath = result.value.localPath;
            {
                std::lock_guard<std::mutex> lock(gRemoteMutex);
                gDownloadInFlight.erase(uri);
                if (ok && std::filesystem::exists(localPath)) {
                    gDownloadedPathCache[uri] = localPath;
                    gDownloadedPathCache[imageUrl] = localPath;
                    gDownloadFailed.erase(uri);
                } else {
                    gDownloadFailed[uri] = true;
                    if (!localPath.empty()) {
                        std::remove(localPath.c_str());
                    }
                }
            }
            gRemoteImageReady.store(true);
        });
    if (!started) {
        std::lock_guard<std::mutex> lock(gRemoteMutex);
        gDownloadInFlight.erase(uri);
        gDownloadFailed[uri] = true;
    }
    return {};
}

std::string resolveLocalImagePath(const std::string& source) {
    if (source.empty()) {
        return {};
    }

    const std::filesystem::path raw(source);
    const std::filesystem::path exeDir = executableDirectory();
    const std::filesystem::path sourceRoot = sourceRootDirectory();
    std::error_code error;
    const std::filesystem::path currentDir = std::filesystem::current_path(error);
    std::vector<std::filesystem::path> candidates;
    candidates.emplace_back(raw);
    if (!error) {
        candidates.emplace_back(currentDir / raw);
        candidates.emplace_back(currentDir / "assets" / raw);
        candidates.emplace_back(currentDir / "assets" / raw.filename());
    }
    candidates.emplace_back(exeDir / raw);
    candidates.emplace_back(exeDir / "assets" / raw);
    candidates.emplace_back(exeDir / "assets" / raw.filename());
    candidates.emplace_back(sourceRoot / raw);
    candidates.emplace_back(sourceRoot / "assets" / raw);
    candidates.emplace_back(sourceRoot / "assets" / raw.filename());

    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate, error) && !error) {
            return std::filesystem::absolute(candidate, error).string();
        }
        error.clear();
    }
    return {};
}

void flipRgbaRows(std::vector<unsigned char>& pixels, int width, int height) {
    if (width <= 0 || height <= 1 || pixels.empty()) {
        return;
    }
    const std::size_t rowBytes = static_cast<std::size_t>(width) * 4u;
    std::vector<unsigned char> temp(rowBytes);
    for (int y = 0; y < height / 2; ++y) {
        unsigned char* top = pixels.data() + static_cast<std::size_t>(y) * rowBytes;
        unsigned char* bottom = pixels.data() + static_cast<std::size_t>(height - 1 - y) * rowBytes;
        std::copy_n(top, rowBytes, temp.data());
        std::copy_n(bottom, rowBytes, top);
        std::copy_n(temp.data(), rowBytes, bottom);
    }
}

bool rasterizeSvgString(const std::string& svg,
                        int targetWidth,
                        int targetHeight,
                        bool flipVertically,
                        std::vector<unsigned char>& pixels,
                        int& width,
                        int& height) {
    pixels.clear();
    width = 0;
    height = 0;

    if (svg.empty()) {
        return false;
    }

    std::vector<char> svgBuffer(svg.begin(), svg.end());
    svgBuffer.push_back('\0');
    NSVGimage* svgImage = nsvgParse(svgBuffer.data(), "px", 96.0f);
    if (svgImage == nullptr || svgImage->width <= 0.0f || svgImage->height <= 0.0f) {
        if (svgImage != nullptr) {
            nsvgDelete(svgImage);
        }
        return false;
    }

    const float scaleX = static_cast<float>(targetWidth) / svgImage->width;
    const float scaleY = static_cast<float>(targetHeight) / svgImage->height;
    const float scale = std::max(0.0001f, std::min(scaleX, scaleY));
    const float rasterWidth = svgImage->width * scale;
    const float rasterHeight = svgImage->height * scale;
    const float offsetX = (static_cast<float>(targetWidth) - rasterWidth) * 0.5f;
    const float offsetY = (static_cast<float>(targetHeight) - rasterHeight) * 0.5f;

    width = targetWidth;
    height = targetHeight;
    pixels.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u, 0);

    NSVGrasterizer* rasterizer = nsvgCreateRasterizer();
    if (rasterizer == nullptr) {
        nsvgDelete(svgImage);
        pixels.clear();
        width = 0;
        height = 0;
        return false;
    }

    nsvgRasterize(rasterizer, svgImage, offsetX, offsetY, scale, pixels.data(), width, height, width * 4);
    nsvgDeleteRasterizer(rasterizer);
    nsvgDelete(svgImage);

    if (flipVertically) {
        flipRgbaRows(pixels, width, height);
    }
    return !pixels.empty();
}

bool rasterizeSvgFile(const std::string& path,
                      int targetWidth,
                      int targetHeight,
                      bool flipVertically,
                      std::vector<unsigned char>& pixels,
                      int& width,
                      int& height) {
    std::ifstream file(path, std::ios::binary);
    if (!file.good()) {
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return rasterizeSvgString(buffer.str(), targetWidth, targetHeight, flipVertically, pixels, width, height);
}

bool readBinaryFile(const std::string& path, std::vector<unsigned char>& bytes) {
    bytes.clear();
    std::ifstream file(path, std::ios::binary);
    if (!file.good()) {
        return false;
    }
    file.seekg(0, std::ios::end);
    const std::streamoff size = file.tellg();
    if (size <= 0) {
        return false;
    }
    file.seekg(0, std::ios::beg);
    bytes.resize(static_cast<std::size_t>(size));
    file.read(reinterpret_cast<char*>(bytes.data()), size);
    if (!file.good() && !file.eof()) {
        bytes.clear();
        return false;
    }
    return true;
}

float colorLuminance(const core::Color& color) {
    return color.r * 0.299f + color.g * 0.587f + color.b * 0.114f;
}

float colorSaturation(float r, float g, float b) {
    const float hi = std::max({r, g, b});
    const float lo = std::min({r, g, b});
    return hi <= 0.001f ? 0.0f : (hi - lo) / hi;
}

core::Color readableThemeColor(core::Color color) {
    color.a = 1.0f;
    const float luma = colorLuminance(color);
    if (luma < 0.34f) {
        color = core::mixColor(color, core::Color{1.0f, 1.0f, 1.0f, 1.0f}, 0.24f);
    } else if (luma > 0.74f) {
        color = core::mixColor(color, core::Color{0.0f, 0.0f, 0.0f, 1.0f}, 0.22f);
    }
    return color;
}

std::string baseImageCacheKey(const std::string& resolvedPath, bool flipVertically) {
    return resolvedPath + (flipVertically ? "#flip" : "#noflip");
}

std::string imageFileVersionSuffix(const std::string& resolvedPath) {
    std::error_code error;
    const std::filesystem::file_time_type modified = std::filesystem::last_write_time(resolvedPath, error);
    if (error) {
        return {};
    }

    const auto size = std::filesystem::file_size(resolvedPath, error);
    if (error) {
        return {};
    }

    const auto modifiedTicks = static_cast<long long>(modified.time_since_epoch().count());
    return "#size=" + std::to_string(size) +
           "#mtime=" + std::to_string(modifiedTicks);
}
} // namespace

std::string imageCacheKey(const std::string& resolvedPath, bool flipVertically) {
    return baseImageCacheKey(resolvedPath, flipVertically) + imageFileVersionSuffix(resolvedPath);
}

std::string resolveImagePath(const std::string& source, bool* pending) {
    if (isBingDailyScheme(source)) {
        return resolveBingImagePath(source, pending);
    }
    if (network::isHttpUrl(source)) {
        return resolveRemoteImagePath(source, pending);
    }
    if (pending != nullptr) {
        *pending = false;
    }
    return resolveLocalImagePath(source);
}

bool isGifPath(const std::string& path) {
    return hasGifExtension(path) || looksLikeGifFile(path);
}

bool isSourceReady(const std::string& source) {
    bool pending = false;
    return !resolveImagePath(source, &pending).empty() && !pending;
}

bool consumeRemoteImageReady() {
    return gRemoteImageReady.exchange(false);
}

std::shared_ptr<const StaticImageData> loadStaticImage(const std::string& source,
                                                       bool flipVertically,
                                                       bool* pending) {
    const std::string resolvedPath = resolveImagePath(source, pending);
    return loadStaticImageFromPath(resolvedPath, flipVertically);
}

std::shared_ptr<const StaticImageData> loadStaticImageFromPath(const std::string& resolvedPath,
                                                               bool flipVertically) {
    if (resolvedPath.empty()) {
        return {};
    }

    const std::string cacheKey = imageCacheKey(resolvedPath, flipVertically);
    if (auto cached = gStaticImageCache[cacheKey].lock()) {
        return cached;
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    std::vector<unsigned char> svgPixels;
    unsigned char* pixels = nullptr;

    if (hasSvgExtension(resolvedPath) || looksLikeSvgFile(resolvedPath)) {
        constexpr int kSvgRasterSize = 512;
        if (!rasterizeSvgFile(resolvedPath, kSvgRasterSize, kSvgRasterSize, flipVertically, svgPixels, width, height)) {
            return {};
        }
        pixels = svgPixels.data();
    } else {
        stbi_set_flip_vertically_on_load(flipVertically ? 1 : 0);
        pixels = stbi_load(resolvedPath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
        if (pixels == nullptr || width <= 0 || height <= 0) {
            if (pixels != nullptr) {
                stbi_image_free(pixels);
            }
            return {};
        }
    }

    auto image = std::make_shared<StaticImageData>();
    image->width = width;
    image->height = height;
    image->pixels.assign(pixels, pixels + static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u);
    if (svgPixels.empty()) {
        stbi_image_free(pixels);
    }
    gStaticImageCache[cacheKey] = image;
    return image;
}

static bool trySampleThemeColor(const StaticImageData& image, core::Color& color) {
    const std::size_t expectedBytes =
        static_cast<std::size_t>(image.width) * static_cast<std::size_t>(image.height) * 4u;
    if (image.width <= 0 || image.height <= 0 || image.pixels.size() < expectedBytes) {
        return false;
    }

    const int pixelCount = image.width * image.height;
    const int step = std::max(1, pixelCount / 2500);
    float weightedR = 0.0f;
    float weightedG = 0.0f;
    float weightedB = 0.0f;
    float totalWeight = 0.0f;
    float averageR = 0.0f;
    float averageG = 0.0f;
    float averageB = 0.0f;
    float averageCount = 0.0f;

    for (int pixel = 0; pixel < pixelCount; pixel += step) {
        const std::size_t offset = static_cast<std::size_t>(pixel) * 4u;
        const float alpha = image.pixels[offset + 3] / 255.0f;
        if (alpha < 0.25f) {
            continue;
        }

        const float r = image.pixels[offset] / 255.0f;
        const float g = image.pixels[offset + 1] / 255.0f;
        const float b = image.pixels[offset + 2] / 255.0f;
        const float luma = r * 0.299f + g * 0.587f + b * 0.114f;
        averageR += r;
        averageG += g;
        averageB += b;
        averageCount += 1.0f;

        if (luma < 0.10f || luma > 0.92f) {
            continue;
        }
        const float sat = colorSaturation(r, g, b);
        const float weight = alpha * (0.20f + sat * 1.80f) * (1.0f - std::fabs(luma - 0.52f));
        weightedR += r * weight;
        weightedG += g * weight;
        weightedB += b * weight;
        totalWeight += weight;
    }

    if (totalWeight > 0.001f) {
        color = readableThemeColor({weightedR / totalWeight, weightedG / totalWeight, weightedB / totalWeight, 1.0f});
        return true;
    }
    if (averageCount > 0.0f) {
        color = readableThemeColor({averageR / averageCount, averageG / averageCount, averageB / averageCount, 1.0f});
        return true;
    }
    return false;
}

core::Color sampleThemeColor(const StaticImageData& image, core::Color fallback) {
    core::Color color;
    return trySampleThemeColor(image, color) ? color : fallback;
}

core::Color sampleThemeColor(const std::string& source,
                             core::Color fallback,
                             bool flipVertically,
                             bool* pending) {
    const std::string resolvedPath = resolveImagePath(source, pending);
    if (resolvedPath.empty()) {
        return fallback;
    }

    const std::string baseCacheKey = baseImageCacheKey(resolvedPath, flipVertically);
    const std::string imageVersionKey = imageCacheKey(resolvedPath, flipVertically);
    const auto cached = gThemeColorCache.find(baseCacheKey);
    if (cached != gThemeColorCache.end() && cached->second.imageVersionKey == imageVersionKey) {
        return cached->second.color;
    }

    const auto image = loadStaticImageFromPath(resolvedPath, flipVertically);
    if (!image) {
        return fallback;
    }

    core::Color color;
    if (!trySampleThemeColor(*image, color)) {
        return fallback;
    }

    gThemeColorCache[baseCacheKey] = {imageVersionKey, color};
    return color;
}

std::shared_ptr<const StaticImageData> loadStaticSvg(const std::string& cacheKey,
                                                     const std::string& svg,
                                                     bool flipVertically) {
    if (cacheKey.empty() || svg.empty()) {
        return {};
    }

    const std::string resolvedCacheKey = "svg-string:" + cacheKey + "#" +
                                         std::to_string(std::hash<std::string>{}(svg)) +
                                         (flipVertically ? "#flip" : "#noflip");
    if (auto cached = gStaticImageCache[resolvedCacheKey].lock()) {
        return cached;
    }

    int width = 0;
    int height = 0;
    std::vector<unsigned char> pixels;
    constexpr int kSvgRasterSize = 512;
    if (!rasterizeSvgString(svg, kSvgRasterSize, kSvgRasterSize, flipVertically, pixels, width, height)) {
        return {};
    }

    auto image = std::make_shared<StaticImageData>();
    image->width = width;
    image->height = height;
    image->pixels = std::move(pixels);
    gStaticImageCache[resolvedCacheKey] = image;
    return image;
}

std::shared_ptr<const GifFrameData> loadGifFrames(const std::string& resolvedPath,
                                                  bool flipVertically) {
    if (resolvedPath.empty()) {
        return {};
    }

    const std::string cacheKey = imageCacheKey(resolvedPath, flipVertically);
    if (auto cached = gGifCache[cacheKey].lock()) {
        return cached;
    }

    std::vector<unsigned char> bytes;
    if (!readBinaryFile(resolvedPath, bytes)) {
        return {};
    }

    stbi_set_flip_vertically_on_load(flipVertically ? 1 : 0);
    int* delays = nullptr;
    int width = 0;
    int height = 0;
    int frames = 0;
    int channels = 0;
    unsigned char* pixels = stbi_load_gif_from_memory(bytes.data(),
                                                      static_cast<int>(bytes.size()),
                                                      &delays,
                                                      &width,
                                                      &height,
                                                      &frames,
                                                      &channels,
                                                      STBI_rgb_alpha);
    if (pixels == nullptr || width <= 0 || height <= 0 || frames <= 0) {
        if (pixels != nullptr) {
            stbi_image_free(pixels);
        }
        if (delays != nullptr) {
            stbi_image_free(delays);
        }
        return {};
    }

    auto data = std::make_shared<GifFrameData>();
    const std::size_t frameBytes = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u;
    data->pixels.assign(pixels, pixels + frameBytes * static_cast<std::size_t>(frames));
    data->delays.assign(static_cast<std::size_t>(frames), 100);
    for (int i = 0; i < frames; ++i) {
        data->delays[static_cast<std::size_t>(i)] = std::max(10, delays != nullptr ? delays[i] : 100);
    }
    data->width = width;
    data->height = height;
    data->frameCount = frames;
    stbi_image_free(pixels);
    if (delays != nullptr) {
        stbi_image_free(delays);
    }
    gGifCache[cacheKey] = data;
    return data;
}

} // namespace core::render::image
