#include "core/render/image.h"

#include "core/platform/async.h"
#include "core/platform/network.h"
#include "core/render/render_backend.h"
#include "core/window/window_backend.h"

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

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

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

namespace core {

namespace {

struct StaticImageData {
    std::vector<unsigned char> pixels;
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

std::unordered_map<std::string, std::weak_ptr<const StaticImageData>> gStaticImageCache;
std::unordered_map<std::string, std::weak_ptr<const GifFrameData>> gGifCache;
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
    buffer[static_cast<size_t>(length)] = '\0';
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
    const size_t query = url.find_first_of("?#");
    const std::string pathPart = url.substr(0, query == std::string::npos ? std::string::npos : query);
    const size_t slash = pathPart.find_last_of('/');
    const size_t dot = pathPart.find_last_of('.');
    std::string ext;
    if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) {
        ext = lowerCopy(pathPart.substr(dot));
    }
    if (ext == ".svg" || ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".webp" || ext == ".bmp" || ext == ".gif") {
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
    const size_t queryPos = uri.find('?');
    if (queryPos == std::string::npos || queryPos + 1 >= uri.size()) {
        return {};
    }

    const std::string query = uri.substr(queryPos + 1);
    size_t pos = 0;
    while (pos < query.size()) {
        const size_t next = query.find('&', pos);
        const std::string token = query.substr(pos, next == std::string::npos ? std::string::npos : next - pos);
        const size_t eq = token.find('=');
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
    for (size_t i = 0; i < value.size(); ++i) {
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
    const size_t begin = payload.find(token);
    if (begin == std::string::npos) {
        return {};
    }
    const size_t urlBegin = begin + token.size();
    const size_t end = payload.find('"', urlBegin);
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
    std::vector<std::filesystem::path> candidates;
    candidates.emplace_back(raw);
    candidates.emplace_back(std::filesystem::current_path() / raw);
    candidates.emplace_back(std::filesystem::current_path() / "assets" / raw);
    candidates.emplace_back(std::filesystem::current_path() / "assets" / raw.filename());
    candidates.emplace_back(exeDir / raw);
    candidates.emplace_back(exeDir / "assets" / raw);
    candidates.emplace_back(exeDir / "assets" / raw.filename());
    candidates.emplace_back(sourceRoot / raw);
    candidates.emplace_back(sourceRoot / "assets" / raw);
    candidates.emplace_back(sourceRoot / "assets" / raw.filename());

    std::error_code error;
    for (const auto& candidate : candidates) {
        if (std::filesystem::exists(candidate, error) && !error) {
            return std::filesystem::absolute(candidate, error).string();
        }
        error.clear();
    }
    return {};
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

bool rasterizeSvgFile(const std::string& path,
                      int targetWidth,
                      int targetHeight,
                      bool flipVertically,
                      std::vector<unsigned char>& pixels,
                      int& width,
                      int& height) {
    pixels.clear();
    width = 0;
    height = 0;

    std::ifstream file(path, std::ios::binary);
    if (!file.good()) {
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string svg = buffer.str();
    if (svg.empty()) {
        return false;
    }

    std::vector<char> svgBuffer(svg.begin(), svg.end());
    svgBuffer.push_back('\0');
    NSVGimage* image = nsvgParse(svgBuffer.data(), "px", 96.0f);
    if (image == nullptr || image->width <= 0.0f || image->height <= 0.0f) {
        if (image != nullptr) {
            nsvgDelete(image);
        }
        return false;
    }

    const float scaleX = static_cast<float>(targetWidth) / image->width;
    const float scaleY = static_cast<float>(targetHeight) / image->height;
    const float scale = std::max(0.0001f, std::min(scaleX, scaleY));
    const float rasterWidth = image->width * scale;
    const float rasterHeight = image->height * scale;
    const float offsetX = (static_cast<float>(targetWidth) - rasterWidth) * 0.5f;
    const float offsetY = (static_cast<float>(targetHeight) - rasterHeight) * 0.5f;

    width = targetWidth;
    height = targetHeight;
    pixels.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4u, 0);

    NSVGrasterizer* rasterizer = nsvgCreateRasterizer();
    if (rasterizer == nullptr) {
        nsvgDelete(image);
        pixels.clear();
        width = 0;
        height = 0;
        return false;
    }

    nsvgRasterize(rasterizer, image, offsetX, offsetY, scale, pixels.data(), width, height, width * 4);
    nsvgDeleteRasterizer(rasterizer);
    nsvgDelete(image);

    if (flipVertically) {
        flipRgbaRows(pixels, width, height);
    }
    return !pixels.empty();
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
    bytes.resize(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(bytes.data()), size);
    if (!file.good() && !file.eof()) {
        bytes.clear();
        return false;
    }
    return true;
}

std::shared_ptr<const StaticImageData> loadStaticImage(const std::string& source, bool flipVertically, bool* pending) {
    const std::string resolvedPath = resolveImagePath(source, pending);
    if (resolvedPath.empty()) {
        return {};
    }
    const std::string cacheKey = resolvedPath + (flipVertically ? "#flip" : "#noflip");
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

std::shared_ptr<const GifFrameData> loadGifFrames(const std::string& resolvedPath, bool flipVertically) {
    if (resolvedPath.empty()) {
        return {};
    }
    const std::string cacheKey = resolvedPath + (flipVertically ? "#flip" : "#noflip");
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
    unsigned char* pixels = stbi_load_gif_from_memory(bytes.data(), static_cast<int>(bytes.size()),
                                                      &delays, &width, &height, &frames, &channels, STBI_rgb_alpha);
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
    const size_t frameBytes = static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
    data->pixels.assign(pixels, pixels + frameBytes * static_cast<size_t>(frames));
    data->delays.assign(static_cast<size_t>(frames), 100);
    for (int i = 0; i < frames; ++i) {
        data->delays[static_cast<size_t>(i)] = std::max(10, delays != nullptr ? delays[i] : 100);
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

} // namespace

struct ImagePrimitive::Impl {
    bool initialize() { return true; }
    void destroy();
    void setSource(const std::string& source) { source_ = source; }
    void setFlipVertically(bool value) { flipVertically_ = value; }
    void setBounds(float x, float y, float width, float height) { bounds_ = {x, y, width, height}; }
    void setTint(const Color& tint) { tint_ = tint; }
    void setCornerRadius(float radius) { radius_ = std::max(0.0f, radius); }
    void setOpacity(float opacity) { opacity_ = std::clamp(opacity, 0.0f, 1.0f); }
    void setTransform(const Transform& transform) { transform_ = transform; hasTransformMatrix_ = false; }
    void setTransformMatrix(const TransformMatrix& matrix) { transformMatrix_ = matrix; hasTransformMatrix_ = true; }
    void setFit(ImageFit fit) { fit_ = fit; }
    void setCoverViewport(bool enabled, const Vec2& canvasSize, const Vec2& viewportOffset) {
        hasCoverViewport_ = enabled;
        coverViewportSize_ = canvasSize;
        coverViewportOffset_ = viewportOffset;
    }

    bool updateTexture();
    bool hasPendingLoad() const { return pendingLoad_; }
    bool isAnimating() const { return gifFrameCount_ > 1; }
    void render(int windowWidth, int windowHeight);

    static bool isSourceReady(const std::string& source) {
        bool pending = false;
        return !resolveImagePath(source, &pending).empty() && !pending;
    }
    static bool consumeRemoteImageReady() { return gRemoteImageReady.exchange(false); }
    static void releaseCachedTextures() {}

    void releaseTexture();
    Vec3 transformPoint(float x, float y) const;
    void rebuildVertices(float* vertices) const;

    std::string source_;
    std::string loadedSource_;
    bool flipVertically_ = false;
    bool loadedFlipVertically_ = false;
    bool pendingLoad_ = false;
    Rect bounds_;
    Color tint_ = {1.0f, 1.0f, 1.0f, 1.0f};
    float radius_ = 0.0f;
    float opacity_ = 1.0f;
    Transform transform_;
    TransformMatrix transformMatrix_;
    bool hasTransformMatrix_ = false;
    ImageFit fit_ = ImageFit::Cover;
    bool hasCoverViewport_ = false;
    Vec2 coverViewportSize_;
    Vec2 coverViewportOffset_;
    render::RenderBackend::TextureHandle texture_ = nullptr;
    int textureWidth_ = 0;
    int textureHeight_ = 0;
    bool textureDirty_ = false;
    std::shared_ptr<const StaticImageData> staticImage_;
    std::shared_ptr<const GifFrameData> gifFrames_;
    std::string loadedGifPath_;
    bool loadedGifFlipVertically_ = false;
    int gifFrameCount_ = 0;
    int gifFrameIndex_ = 0;
    double gifNextFrameTime_ = 0.0;
};

void ImagePrimitive::Impl::destroy() {
    releaseTexture();
    staticImage_.reset();
    gifFrames_.reset();
    loadedGifPath_.clear();
    loadedSource_.clear();
}

bool ImagePrimitive::Impl::updateTexture() {
    bool changed = false;
    bool pending = false;
    const std::string resolvedPath = resolveImagePath(source_, &pending);
    pendingLoad_ = pending;
    const bool isGif = !resolvedPath.empty() && (hasGifExtension(resolvedPath) || looksLikeGifFile(resolvedPath));

    if (isGif) {
        if (loadedGifPath_ != resolvedPath || loadedGifFlipVertically_ != flipVertically_) {
            gifFrames_ = loadGifFrames(resolvedPath, flipVertically_);
            if (!gifFrames_) {
                return false;
            }
            staticImage_.reset();
            loadedGifPath_ = resolvedPath;
            loadedGifFlipVertically_ = flipVertically_;
            gifFrameCount_ = gifFrames_->frameCount;
            gifFrameIndex_ = 0;
            gifNextFrameTime_ = window::timeSeconds() + static_cast<double>(gifFrames_->delays.front()) / 1000.0;
            textureWidth_ = gifFrames_->width;
            textureHeight_ = gifFrames_->height;
            loadedSource_ = source_;
            loadedFlipVertically_ = flipVertically_;
            textureDirty_ = true;
            pendingLoad_ = false;
            changed = true;
        } else if (gifFrameCount_ > 1 && gifFrames_) {
            const double now = window::timeSeconds();
            if (now >= gifNextFrameTime_) {
                int guard = 0;
                do {
                    gifFrameIndex_ = (gifFrameIndex_ + 1) % gifFrameCount_;
                    gifNextFrameTime_ += static_cast<double>(gifFrames_->delays[static_cast<size_t>(gifFrameIndex_)]) / 1000.0;
                    ++guard;
                } while (now >= gifNextFrameTime_ && guard < gifFrameCount_);
                textureDirty_ = true;
                changed = true;
            }
        }
        return changed;
    }

    if (loadedSource_ == source_ && loadedFlipVertically_ == flipVertically_ && staticImage_) {
        return false;
    }

    std::shared_ptr<const StaticImageData> image = loadStaticImage(source_, flipVertically_, &pending);
    pendingLoad_ = pending;
    if (!image) {
        if (source_.empty()) {
            releaseTexture();
            staticImage_.reset();
            loadedSource_.clear();
            textureWidth_ = 0;
            textureHeight_ = 0;
        }
        return false;
    }

    gifFrames_.reset();
    loadedGifPath_.clear();
    gifFrameCount_ = 0;
    staticImage_ = std::move(image);
    textureWidth_ = staticImage_->width;
    textureHeight_ = staticImage_->height;
    loadedSource_ = source_;
    loadedFlipVertically_ = flipVertically_;
    textureDirty_ = true;
    pendingLoad_ = false;
    return true;
}

void ImagePrimitive::Impl::render(int windowWidth, int windowHeight) {
    if (bounds_.width <= 0.0f || bounds_.height <= 0.0f || opacity_ <= 0.001f) {
        return;
    }
    auto* backend = render::activeRenderBackend();
    if (backend == nullptr) {
        return;
    }

    const unsigned char* pixels = nullptr;
    if (gifFrames_ && gifFrameCount_ > 0) {
        const size_t frameBytes = static_cast<size_t>(gifFrames_->width) * static_cast<size_t>(gifFrames_->height) * 4u;
        pixels = gifFrames_->pixels.data() + frameBytes * static_cast<size_t>(gifFrameIndex_);
    } else if (staticImage_) {
        pixels = staticImage_->pixels.data();
    }
    if (pixels == nullptr || textureWidth_ <= 0 || textureHeight_ <= 0) {
        return;
    }

    if (texture_ == nullptr) {
        texture_ = backend->createTexture(pixels, textureWidth_, textureHeight_);
        textureDirty_ = false;
    } else if (textureDirty_) {
        if (backend->updateTexture(texture_, pixels, textureWidth_, textureHeight_)) {
            textureDirty_ = false;
        }
    }
    if (texture_ == nullptr) {
        return;
    }

    float vertices[42] = {};
    rebuildVertices(vertices);
    Color tint = tint_;
    tint.a *= opacity_;
    backend->drawTexture(texture_, vertices, 42, tint, bounds_, radius_, windowWidth, windowHeight);
}

void ImagePrimitive::Impl::releaseTexture() {
    if (texture_ != nullptr) {
        if (auto* backend = render::activeRenderBackend()) {
            backend->destroyTexture(texture_);
        }
        texture_ = nullptr;
    }
}

Vec3 ImagePrimitive::Impl::transformPoint(float x, float y) const {
    if (hasTransformMatrix_) {
        return core::transformPointWithW(transformMatrix_, x, y);
    }
    const Vec2 origin = {
        bounds_.x + bounds_.width * transform_.origin.x,
        bounds_.y + bounds_.height * transform_.origin.y
    };
    const float scaledX = (x - origin.x) * transform_.scale.x;
    const float scaledY = (y - origin.y) * transform_.scale.y;
    const float cosine = std::cos(transform_.rotate);
    const float sine = std::sin(transform_.rotate);
    return {
        origin.x + scaledX * cosine - scaledY * sine + transform_.translate.x,
        origin.y + scaledX * sine + scaledY * cosine + transform_.translate.y,
        1.0f
    };
}

void ImagePrimitive::Impl::rebuildVertices(float* vertices) const {
    Rect drawRect = bounds_;
    if (fit_ == ImageFit::Contain && textureWidth_ > 0 && textureHeight_ > 0 && bounds_.width > 0.0f && bounds_.height > 0.0f) {
        const float rectAspect = bounds_.width / bounds_.height;
        const float imageAspect = static_cast<float>(textureWidth_) / static_cast<float>(textureHeight_);
        if (imageAspect > rectAspect) {
            drawRect.height = bounds_.width / imageAspect;
            drawRect.y = bounds_.y + (bounds_.height - drawRect.height) * 0.5f;
        } else if (imageAspect < rectAspect) {
            drawRect.width = bounds_.height * imageAspect;
            drawRect.x = bounds_.x + (bounds_.width - drawRect.width) * 0.5f;
        }
    }

    const Vec3 screen[4] = {
        transformPoint(drawRect.x, drawRect.y),
        transformPoint(drawRect.x + drawRect.width, drawRect.y),
        transformPoint(drawRect.x + drawRect.width, drawRect.y + drawRect.height),
        transformPoint(drawRect.x, drawRect.y + drawRect.height)
    };
    const Vec2 local[4] = {
        {drawRect.x, drawRect.y},
        {drawRect.x + drawRect.width, drawRect.y},
        {drawRect.x + drawRect.width, drawRect.y + drawRect.height},
        {drawRect.x, drawRect.y + drawRect.height}
    };
    float u0 = 0.0f;
    float v0 = 0.0f;
    float u1 = 1.0f;
    float v1 = 1.0f;
    if (fit_ != ImageFit::Stretch && textureWidth_ > 0 && textureHeight_ > 0 && bounds_.width > 0.0f && bounds_.height > 0.0f) {
        const bool useCoverViewport = fit_ == ImageFit::Cover &&
                                      hasCoverViewport_ &&
                                      coverViewportSize_.x > 0.0f &&
                                      coverViewportSize_.y > 0.0f;
        const float sampleWidth = useCoverViewport ? coverViewportSize_.x : bounds_.width;
        const float sampleHeight = useCoverViewport ? coverViewportSize_.y : bounds_.height;
        const float rectAspect = sampleWidth / sampleHeight;
        const float imageAspect = static_cast<float>(textureWidth_) / static_cast<float>(textureHeight_);
        if (fit_ == ImageFit::Cover) {
            if (imageAspect > rectAspect) {
                const float visible = std::clamp(rectAspect / imageAspect, 0.0f, 1.0f);
                u0 = (1.0f - visible) * 0.5f;
                u1 = 1.0f - u0;
            } else if (imageAspect < rectAspect) {
                const float visible = std::clamp(imageAspect / rectAspect, 0.0f, 1.0f);
                v0 = (1.0f - visible) * 0.5f;
                v1 = 1.0f - v0;
            }
            if (useCoverViewport) {
                const float left = std::clamp(coverViewportOffset_.x / sampleWidth, 0.0f, 1.0f);
                const float top = std::clamp(coverViewportOffset_.y / sampleHeight, 0.0f, 1.0f);
                const float right = std::clamp((coverViewportOffset_.x + bounds_.width) / sampleWidth, left, 1.0f);
                const float bottom = std::clamp((coverViewportOffset_.y + bounds_.height) / sampleHeight, top, 1.0f);
                const float fullU0 = u0;
                const float fullV0 = v0;
                const float fullU1 = u1;
                const float fullV1 = v1;
                u0 = fullU0 + (fullU1 - fullU0) * left;
                u1 = fullU0 + (fullU1 - fullU0) * right;
                v0 = fullV0 + (fullV1 - fullV0) * top;
                v1 = fullV0 + (fullV1 - fullV0) * bottom;
            }
        }
    }
    const Vec2 uv[4] = {{u0, v0}, {u1, v0}, {u1, v1}, {u0, v1}};
    const int order[6] = {0, 1, 2, 0, 2, 3};
    for (int i = 0; i < 6; ++i) {
        const int index = order[i];
        const int offset = i * 7;
        vertices[offset + 0] = screen[index].x;
        vertices[offset + 1] = screen[index].y;
        vertices[offset + 2] = screen[index].z;
        vertices[offset + 3] = local[index].x;
        vertices[offset + 4] = local[index].y;
        vertices[offset + 5] = uv[index].x;
        vertices[offset + 6] = uv[index].y;
    }
}

ImagePrimitive::ImagePrimitive()
    : impl_(std::make_unique<Impl>()) {}

ImagePrimitive::~ImagePrimitive() = default;
ImagePrimitive::ImagePrimitive(ImagePrimitive&&) noexcept = default;
ImagePrimitive& ImagePrimitive::operator=(ImagePrimitive&&) noexcept = default;

bool ImagePrimitive::initialize() { return impl_->initialize(); }
void ImagePrimitive::destroy() { impl_->destroy(); }
void ImagePrimitive::setSource(const std::string& source) { impl_->setSource(source); }
void ImagePrimitive::setFlipVertically(bool value) { impl_->setFlipVertically(value); }
void ImagePrimitive::setBounds(float x, float y, float width, float height) { impl_->setBounds(x, y, width, height); }
void ImagePrimitive::setTint(const Color& tint) { impl_->setTint(tint); }
void ImagePrimitive::setCornerRadius(float radius) { impl_->setCornerRadius(radius); }
void ImagePrimitive::setOpacity(float opacity) { impl_->setOpacity(opacity); }
void ImagePrimitive::setTransform(const Transform& transform) { impl_->setTransform(transform); }
void ImagePrimitive::setTransformMatrix(const TransformMatrix& matrix) { impl_->setTransformMatrix(matrix); }
void ImagePrimitive::setFit(ImageFit fit) { impl_->setFit(fit); }
void ImagePrimitive::setCoverViewport(bool enabled, const Vec2& canvasSize, const Vec2& viewportOffset) {
    impl_->setCoverViewport(enabled, canvasSize, viewportOffset);
}
bool ImagePrimitive::updateTexture() { return impl_->updateTexture(); }
bool ImagePrimitive::hasPendingLoad() const { return impl_->hasPendingLoad(); }
bool ImagePrimitive::isAnimating() const { return impl_->isAnimating(); }
void ImagePrimitive::render(int windowWidth, int windowHeight) { impl_->render(windowWidth, windowHeight); }
bool ImagePrimitive::isSourceReady(const std::string& source) { return Impl::isSourceReady(source); }
bool ImagePrimitive::consumeRemoteImageReady() { return Impl::consumeRemoteImageReady(); }
void ImagePrimitive::releaseCachedTextures() { Impl::releaseCachedTextures(); }

} // namespace core
