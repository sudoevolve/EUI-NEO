#include "core/image.h"
#include "core/network.h"

#ifndef GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_NONE
#endif
#include <GLFW/glfw3.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <thread>
#include <unordered_map>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include "3rd/stb_image.h"

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

struct TextureRecord {
    GLuint texture = 0;
    int width = 0;
    int height = 0;
};

std::unordered_map<std::string, TextureRecord> gTextureCache;
std::unordered_map<std::string, std::string> gDownloadedPathCache;
std::unordered_map<std::string, bool> gDownloadInFlight;
std::unordered_map<std::string, bool> gDownloadFailed;
std::mutex gRemoteMutex;
std::mutex gThreadMutex;
std::vector<std::thread> gDownloadThreads;
std::atomic<bool> gRemoteImageReady{false};

std::string lowerCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool hasSvgExtension(const std::string& path) {
    const std::string ext = lowerCopy(std::filesystem::path(path).extension().string());
    return ext == ".svg";
}

bool looksLikeSvgFile(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.good()) {
        return false;
    }

    char buffer[512] = {};
    input.read(buffer, sizeof(buffer) - 1);
    std::string head(buffer, static_cast<size_t>(input.gcount()));
    head = lowerCopy(head);
    const size_t first = head.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return false;
    }
    head = head.substr(first);
    return head.rfind("<svg", 0) == 0 || head.find("<svg") != std::string::npos;
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
    if (ext == ".svg" || ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".webp" || ext == ".bmp") {
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

    std::thread worker([url, localPath] {
        const bool ok = network::downloadUrlToFile(url, localPath);
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
        network::postNetworkReadyEvent();
    });
    {
        std::lock_guard<std::mutex> lock(gThreadMutex);
        gDownloadThreads.push_back(std::move(worker));
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

    std::thread worker([uri] {
        std::string payload;
        std::string imageUrl;
        std::string localPath;
        bool ok = network::downloadUrlToString(buildBingDailyApiUrl(uri), payload);
        if (ok) {
            imageUrl = extractBingImageUrlFromJson(payload);
            localPath = buildDownloadedImagePath(imageUrl);
            if (imageUrl.empty() || localPath.empty()) {
                ok = false;
            } else if (!std::filesystem::exists(localPath)) {
                ok = network::downloadUrlToFile(imageUrl, localPath);
            }
        }
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
        network::postNetworkReadyEvent();
    });
    {
        std::lock_guard<std::mutex> lock(gThreadMutex);
        gDownloadThreads.push_back(std::move(worker));
    }

    return {};
}

void joinDownloadThreads() {
    std::vector<std::thread> threads;
    {
        std::lock_guard<std::mutex> lock(gThreadMutex);
        threads.swap(gDownloadThreads);
    }

    for (std::thread& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

std::string resolveLocalImagePath(const std::string& source) {
    if (source.empty()) {
        return {};
    }

    std::vector<std::filesystem::path> candidates;
    candidates.emplace_back(source);
    candidates.emplace_back(std::filesystem::current_path() / source);
    candidates.emplace_back(std::filesystem::current_path() / "assets" / source);
    candidates.emplace_back(std::filesystem::current_path() / "assets" / std::filesystem::path(source).filename());

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
    const size_t rowBytes = static_cast<size_t>(width) * 4u;
    std::vector<unsigned char> temp(rowBytes);
    for (int y = 0; y < height / 2; ++y) {
        unsigned char* top = pixels.data() + static_cast<size_t>(y) * rowBytes;
        unsigned char* bottom = pixels.data() + static_cast<size_t>(height - 1 - y) * rowBytes;
        std::copy_n(top, rowBytes, temp.data());
        std::copy_n(bottom, rowBytes, top);
        std::copy_n(temp.data(), rowBytes, bottom);
    }
}

bool rasterizeSvgFile(const std::string& path, int targetWidth, int targetHeight, bool flipVertically,
                      std::vector<unsigned char>& pixels, int& width, int& height) {
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
    pixels.assign(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u, 0);

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

GLuint createTexture(const unsigned char* pixels, int width, int height) {
    if (pixels == nullptr || width <= 0 || height <= 0) {
        return 0;
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);
    return texture;
}

} // namespace

struct ImagePrimitive::SharedResources {
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint shaderProgram = 0;
    GLint windowSizeLocation = -1;
    GLint textureLocation = -1;
    GLint tintLocation = -1;
    GLint rectLocation = -1;
    GLint radiusLocation = -1;
    int references = 0;
};

bool ImagePrimitive::initialize() {
    if (!retainSharedResources()) {
        return false;
    }

    SharedResources& resources = sharedResources();
    vao_ = resources.vao;
    vbo_ = resources.vbo;
    shaderProgram_ = resources.shaderProgram;
    windowSizeLocation_ = resources.windowSizeLocation;
    textureLocation_ = resources.textureLocation;
    tintLocation_ = resources.tintLocation;
    rectLocation_ = resources.rectLocation;
    radiusLocation_ = resources.radiusLocation;
    return true;
}

void ImagePrimitive::destroy() {
    if (shaderProgram_ != 0) {
        releaseSharedResources();
    }
    vao_ = 0;
    vbo_ = 0;
    shaderProgram_ = 0;
    windowSizeLocation_ = -1;
    textureLocation_ = -1;
    tintLocation_ = -1;
    rectLocation_ = -1;
    radiusLocation_ = -1;
}

void ImagePrimitive::setSource(const std::string& source) {
    source_ = source;
}

void ImagePrimitive::setFlipVertically(bool value) {
    flipVertically_ = value;
}

void ImagePrimitive::setBounds(float x, float y, float width, float height) {
    bounds_ = {x, y, width, height};
}

void ImagePrimitive::setTint(const Color& tint) {
    tint_ = tint;
}

void ImagePrimitive::setCornerRadius(float radius) {
    radius_ = std::max(0.0f, radius);
}

void ImagePrimitive::setOpacity(float opacity) {
    opacity_ = std::clamp(opacity, 0.0f, 1.0f);
}

void ImagePrimitive::setTransform(const Transform& transform) {
    transform_ = transform;
}

void ImagePrimitive::setFit(ImageFit fit) {
    fit_ = fit;
}

bool ImagePrimitive::updateTexture() {
    bool pending = false;
    int nextWidth = 0;
    int nextHeight = 0;
    const GLuint nextTexture = loadTexture(source_, flipVertically_, &pending, &nextWidth, &nextHeight);
    pendingLoad_ = pending;

    if (nextTexture == 0) {
        if (source_.empty()) {
            texture_ = 0;
            textureWidth_ = 0;
            textureHeight_ = 0;
            loadedSource_.clear();
        }
        return false;
    }

    const bool changed = texture_ != nextTexture || loadedSource_ != source_ || loadedFlipVertically_ != flipVertically_;
    texture_ = nextTexture;
    textureWidth_ = nextWidth;
    textureHeight_ = nextHeight;
    loadedSource_ = source_;
    loadedFlipVertically_ = flipVertically_;
    pendingLoad_ = false;
    return changed;
}

bool ImagePrimitive::hasPendingLoad() const {
    return pendingLoad_;
}

void ImagePrimitive::render(int windowWidth, int windowHeight) {
    if (texture_ == 0 || shaderProgram_ == 0 || vao_ == 0 || vbo_ == 0 || bounds_.width <= 0.0f || bounds_.height <= 0.0f) {
        return;
    }

    float vertices[36] = {};
    rebuildVertices(vertices);

    const GLboolean blendEnabled = glIsEnabled(GL_BLEND);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(shaderProgram_);
    glUniform2f(windowSizeLocation_, static_cast<float>(std::max(1, windowWidth)), static_cast<float>(std::max(1, windowHeight)));
    glUniform4f(tintLocation_, tint_.r, tint_.g, tint_.b, tint_.a * opacity_);
    glUniform4f(rectLocation_, bounds_.x, bounds_.y, bounds_.width, bounds_.height);
    glUniform1f(radiusLocation_, radius_);
    glUniform1i(textureLocation_, 0);

    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    if (!blendEnabled) {
        glDisable(GL_BLEND);
    }
}

bool ImagePrimitive::consumeRemoteImageReady() {
    return gRemoteImageReady.exchange(false);
}

void ImagePrimitive::releaseCachedTextures() {
    joinDownloadThreads();
    for (auto& item : gTextureCache) {
        if (item.second.texture != 0) {
            glDeleteTextures(1, &item.second.texture);
        }
    }
    gTextureCache.clear();
}

ImagePrimitive::SharedResources& ImagePrimitive::sharedResources() {
    static std::unordered_map<GLFWwindow*, SharedResources> resourcesByContext;
    return resourcesByContext[glfwGetCurrentContext()];
}

bool ImagePrimitive::retainSharedResources() {
    SharedResources& resources = sharedResources();
    ++resources.references;
    if (resources.shaderProgram != 0) {
        return true;
    }

    const char* vertexSource =
        "#version 330 core\n"
        "layout(location = 0) in vec2 aScreenPos;\n"
        "layout(location = 1) in vec2 aLocalPos;\n"
        "layout(location = 2) in vec2 aUV;\n"
        "uniform vec2 uWindowSize;\n"
        "out vec2 vLocalPos;\n"
        "out vec2 vUV;\n"
        "void main() {\n"
        "    vLocalPos = aLocalPos;\n"
        "    vUV = aUV;\n"
        "    vec2 ndc = vec2((aScreenPos.x / uWindowSize.x) * 2.0 - 1.0,\n"
        "                    1.0 - (aScreenPos.y / uWindowSize.y) * 2.0);\n"
        "    gl_Position = vec4(ndc, 0.0, 1.0);\n"
        "}\n";

    const char* fragmentSource =
        "#version 330 core\n"
        "in vec2 vLocalPos;\n"
        "in vec2 vUV;\n"
        "out vec4 FragColor;\n"
        "uniform sampler2D uTexture;\n"
        "uniform vec4 uTint;\n"
        "uniform vec4 uRect;\n"
        "uniform float uRadius;\n"
        "float roundedBoxDistance(vec2 point, vec2 halfSize, float radius) {\n"
        "    vec2 cornerVector = abs(point) - halfSize + vec2(radius);\n"
        "    return length(max(cornerVector, 0.0)) + min(max(cornerVector.x, cornerVector.y), 0.0) - radius;\n"
        "}\n"
        "void main() {\n"
        "    vec2 center = uRect.xy + uRect.zw * 0.5;\n"
        "    float distanceToEdge = roundedBoxDistance(vLocalPos - center, uRect.zw * 0.5, uRadius);\n"
        "    float edgeWidth = max(fwidth(distanceToEdge), 0.75);\n"
        "    float shapeAlpha = 1.0 - smoothstep(-edgeWidth, edgeWidth, distanceToEdge);\n"
        "    if (shapeAlpha <= 0.0) discard;\n"
        "    vec4 sampled = texture(uTexture, vUV);\n"
        "    FragColor = vec4(sampled.rgb * uTint.rgb, sampled.a * uTint.a * shapeAlpha);\n"
        "}\n";

    const GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    const GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (vertexShader == 0 || fragmentShader == 0) {
        if (vertexShader != 0) {
            glDeleteShader(vertexShader);
        }
        if (fragmentShader != 0) {
            glDeleteShader(fragmentShader);
        }
        resources.references = std::max(0, resources.references - 1);
        return false;
    }

    resources.shaderProgram = glCreateProgram();
    glAttachShader(resources.shaderProgram, vertexShader);
    glAttachShader(resources.shaderProgram, fragmentShader);
    glLinkProgram(resources.shaderProgram);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    GLint linked = 0;
    glGetProgramiv(resources.shaderProgram, GL_LINK_STATUS, &linked);
    if (!linked) {
        glDeleteProgram(resources.shaderProgram);
        resources.shaderProgram = 0;
        resources.references = std::max(0, resources.references - 1);
        return false;
    }

    resources.windowSizeLocation = glGetUniformLocation(resources.shaderProgram, "uWindowSize");
    resources.textureLocation = glGetUniformLocation(resources.shaderProgram, "uTexture");
    resources.tintLocation = glGetUniformLocation(resources.shaderProgram, "uTint");
    resources.rectLocation = glGetUniformLocation(resources.shaderProgram, "uRect");
    resources.radiusLocation = glGetUniformLocation(resources.shaderProgram, "uRadius");

    glGenVertexArrays(1, &resources.vao);
    glGenBuffers(1, &resources.vbo);
    glBindVertexArray(resources.vao);
    glBindBuffer(GL_ARRAY_BUFFER, resources.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 36, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 6, nullptr);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 6, reinterpret_cast<void*>(sizeof(float) * 2));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(float) * 6, reinterpret_cast<void*>(sizeof(float) * 4));
    glEnableVertexAttribArray(2);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return resources.shaderProgram != 0 && resources.vao != 0 && resources.vbo != 0;
}

void ImagePrimitive::releaseSharedResources() {
    SharedResources& resources = sharedResources();
    resources.references = std::max(0, resources.references - 1);
    if (resources.references > 0) {
        return;
    }

    if (resources.vbo != 0) {
        glDeleteBuffers(1, &resources.vbo);
        resources.vbo = 0;
    }
    if (resources.vao != 0) {
        glDeleteVertexArrays(1, &resources.vao);
        resources.vao = 0;
    }
    if (resources.shaderProgram != 0) {
        glDeleteProgram(resources.shaderProgram);
        resources.shaderProgram = 0;
    }
    resources.windowSizeLocation = -1;
    resources.textureLocation = -1;
    resources.tintLocation = -1;
    resources.rectLocation = -1;
    resources.radiusLocation = -1;
}

GLuint ImagePrimitive::compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint ImagePrimitive::loadTexture(const std::string& source, bool flipVertically, bool* pending, int* outWidth, int* outHeight) {
    const std::string resolvedPath = resolveImagePath(source, pending);
    if (resolvedPath.empty()) {
        return 0;
    }

    const std::string cacheKey = resolvedPath + (flipVertically ? "#flip" : "#noflip");
    const auto cached = gTextureCache.find(cacheKey);
    if (cached != gTextureCache.end()) {
        if (outWidth != nullptr) {
            *outWidth = cached->second.width;
        }
        if (outHeight != nullptr) {
            *outHeight = cached->second.height;
        }
        return cached->second.texture;
    }

    std::vector<unsigned char> svgPixels;
    unsigned char* pixels = nullptr;
    int width = 0;
    int height = 0;
    int channels = 0;

    if (hasSvgExtension(resolvedPath) || looksLikeSvgFile(resolvedPath)) {
        const int targetSize = 512;
        if (!rasterizeSvgFile(resolvedPath, targetSize, targetSize, flipVertically, svgPixels, width, height)) {
            return 0;
        }
        pixels = svgPixels.data();
    } else {
        stbi_set_flip_vertically_on_load(flipVertically ? 1 : 0);
        pixels = stbi_load(resolvedPath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
        if (pixels == nullptr || width <= 0 || height <= 0) {
            if (pixels != nullptr) {
                stbi_image_free(pixels);
            }
            return 0;
        }
    }

    const GLuint texture = createTexture(pixels, width, height);
    if (svgPixels.empty()) {
        stbi_image_free(pixels);
    }

    if (texture != 0) {
        gTextureCache[cacheKey] = {texture, width, height};
        if (outWidth != nullptr) {
            *outWidth = width;
        }
        if (outHeight != nullptr) {
            *outHeight = height;
        }
    }
    return texture;
}

Vec2 ImagePrimitive::transformPoint(float x, float y) const {
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
        origin.y + scaledX * sine + scaledY * cosine + transform_.translate.y
    };
}

void ImagePrimitive::rebuildVertices(float* vertices) const {
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

    const Vec2 screen[4] = {
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
        const float rectAspect = bounds_.width / bounds_.height;
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
        }
    }
    const Vec2 uv[4] = {
        {u0, v0},
        {u1, v0},
        {u1, v1},
        {u0, v1}
    };
    const int order[6] = {0, 1, 2, 0, 2, 3};

    for (int i = 0; i < 6; ++i) {
        const int index = order[i];
        const int offset = i * 6;
        vertices[offset + 0] = screen[index].x;
        vertices[offset + 1] = screen[index].y;
        vertices[offset + 2] = local[index].x;
        vertices[offset + 3] = local[index].y;
        vertices[offset + 4] = uv[index].x;
        vertices[offset + 5] = uv[index].y;
    }
}

} // namespace core
