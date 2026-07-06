#include "core/platform/network.h"
#include "core/platform/async.h"
#include "core/platform/platform.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <urlmon.h>
#endif

#if defined(__ANDROID__)
#include <SDL_system.h>
#include <jni.h>
#endif

#if defined(EUI_HAS_CURL)
#include <curl/curl.h>
#endif

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <unordered_map>

namespace core::network {
namespace {

struct TextState {
    bool started = false;
    bool ready = false;
    bool ok = false;
    std::string body;
};

std::mutex gTextMutex;
std::unordered_map<std::string, TextState> gTextRequests;

#if defined(EUI_HAS_CURL)
struct CurlCancelContext {
    const async::CancelToken* token = nullptr;
};

size_t curlWriteToFile(void* data, size_t size, size_t nmemb, void* userdata) {
    const size_t bytes = size * nmemb;
    auto* output = reinterpret_cast<std::ofstream*>(userdata);
    output->write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(bytes));
    return bytes;
}

size_t curlWriteToString(void* data, size_t size, size_t nmemb, void* userdata) {
    const size_t bytes = size * nmemb;
    auto* output = reinterpret_cast<std::string*>(userdata);
    output->append(reinterpret_cast<const char*>(data), bytes);
    return bytes;
}

int curlProgressCallback(void* userdata, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
    const auto* context = reinterpret_cast<const CurlCancelContext*>(userdata);
    return context != nullptr && context->token != nullptr && context->token->canceled() ? 1 : 0;
}

void installCurlCancel(CURL* curl, CurlCancelContext& context, const async::CancelToken* cancelToken) {
    context.token = cancelToken;
    if (cancelToken == nullptr) {
        return;
    }
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, curlProgressCallback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &context);
}

bool ensureCurlReady() {
    static bool initialized = false;
    static bool ready = false;
    if (!initialized) {
        ready = curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
        initialized = true;
    }
    return ready;
}
#endif

} // namespace

#if defined(__ANDROID__)
bool androidClearException(JNIEnv* env) {
    if (env != nullptr && env->ExceptionCheck()) {
        env->ExceptionClear();
        return true;
    }
    return false;
}

jclass androidMainActivityClass(JNIEnv* env) {
    if (env == nullptr) {
        return nullptr;
    }
    jobject activity = static_cast<jobject>(SDL_AndroidGetActivity());
    if (activity == nullptr) {
        return nullptr;
    }
    jclass local = env->GetObjectClass(activity);
    env->DeleteLocalRef(activity);
    if (androidClearException(env) || local == nullptr) {
        return nullptr;
    }
    return local;
}

bool androidDownloadUrlToString(const std::string& url, std::string& output) {
    output.clear();
    JNIEnv* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    jclass cls = androidMainActivityClass(env);
    if (env == nullptr || cls == nullptr) {
        return false;
    }
    jmethodID method = env->GetStaticMethodID(cls, "downloadUrlToString", "(Ljava/lang/String;)Ljava/lang/String;");
    if (androidClearException(env) || method == nullptr) {
        env->DeleteLocalRef(cls);
        return false;
    }
    jstring jUrl = env->NewStringUTF(url.c_str());
    if (jUrl == nullptr) {
        env->DeleteLocalRef(cls);
        return false;
    }
    auto result = static_cast<jstring>(env->CallStaticObjectMethod(cls, method, jUrl));
    env->DeleteLocalRef(jUrl);
    if (androidClearException(env) || result == nullptr) {
        env->DeleteLocalRef(cls);
        return false;
    }
    const char* chars = env->GetStringUTFChars(result, nullptr);
    if (chars != nullptr) {
        output = chars;
        env->ReleaseStringUTFChars(result, chars);
    }
    env->DeleteLocalRef(result);
    env->DeleteLocalRef(cls);
    return !output.empty();
}

bool androidDownloadUrlToFile(const std::string& url, const std::string& localPath) {
    JNIEnv* env = static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());
    jclass cls = androidMainActivityClass(env);
    if (env == nullptr || cls == nullptr) {
        return false;
    }
    jmethodID method = env->GetStaticMethodID(cls, "downloadUrlToFile", "(Ljava/lang/String;Ljava/lang/String;)Z");
    if (androidClearException(env) || method == nullptr) {
        env->DeleteLocalRef(cls);
        return false;
    }
    jstring jUrl = env->NewStringUTF(url.c_str());
    jstring jPath = env->NewStringUTF(localPath.c_str());
    if (jUrl == nullptr || jPath == nullptr) {
        if (jUrl != nullptr) {
            env->DeleteLocalRef(jUrl);
        }
        if (jPath != nullptr) {
            env->DeleteLocalRef(jPath);
        }
        env->DeleteLocalRef(cls);
        return false;
    }
    const bool ok = env->CallStaticBooleanMethod(cls, method, jUrl, jPath) == JNI_TRUE;
    const bool failed = androidClearException(env);
    env->DeleteLocalRef(jUrl);
    env->DeleteLocalRef(jPath);
    env->DeleteLocalRef(cls);
    return ok && !failed;
}
#endif

bool isHttpUrl(const std::string& url) {
    return url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0;
}

std::string cacheFilePath(const std::string& key, const std::string& extension, const std::string& bucket) {
    std::error_code error;
    const std::filesystem::path dir = std::filesystem::temp_directory_path(error) / bucket;
    if (error) {
        return {};
    }
    std::filesystem::create_directories(dir, error);
    if (error) {
        return {};
    }

    std::string safeExtension = extension.empty() ? ".cache" : extension;
    if (safeExtension.front() != '.') {
        safeExtension.insert(safeExtension.begin(), '.');
    }

    std::ostringstream name;
    name << "item_" << std::hash<std::string>{}(key) << safeExtension;
    return (dir / name.str()).string();
}

bool downloadUrlToFile(const std::string& url, const std::string& localPath, const async::CancelToken* cancelToken) {
    if (cancelToken != nullptr && cancelToken->canceled()) {
        return false;
    }
#if defined(__ANDROID__)
    (void)cancelToken;
    return androidDownloadUrlToFile(url, localPath);
#elif defined(EUI_HAS_CURL)
    if (!ensureCurlReady()) {
        return false;
    }
    std::ofstream output(localPath, std::ios::binary | std::ios::trunc);
    if (!output.good()) {
        return false;
    }
    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        return false;
    }
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteToFile);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &output);
    CurlCancelContext cancelContext;
    installCurlCancel(curl, cancelContext, cancelToken);
    const CURLcode code = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);
    output.close();
    if (cancelToken != nullptr && cancelToken->canceled()) {
        std::remove(localPath.c_str());
        return false;
    }
    return code == CURLE_OK && status >= 200 && status < 300;
#elif defined(_WIN32)
    const HRESULT result = URLDownloadToFileA(nullptr, url.c_str(), localPath.c_str(), 0, nullptr);
    return SUCCEEDED(result);
#else
    (void)url;
    (void)localPath;
    return false;
#endif
}

bool downloadUrlToString(const std::string& url, std::string& output, const async::CancelToken* cancelToken) {
    if (cancelToken != nullptr && cancelToken->canceled()) {
        output.clear();
        return false;
    }
#if defined(__ANDROID__)
    (void)cancelToken;
    return androidDownloadUrlToString(url, output);
#elif defined(EUI_HAS_CURL)
    if (!ensureCurlReady()) {
        return false;
    }
    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        return false;
    }
    output.clear();
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteToString);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &output);
    CurlCancelContext cancelContext;
    installCurlCancel(curl, cancelContext, cancelToken);
    const CURLcode code = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);
    if (cancelToken != nullptr && cancelToken->canceled()) {
        output.clear();
        return false;
    }
    return code == CURLE_OK && status >= 200 && status < 300;
#elif defined(_WIN32)
    const std::string tempFile = cacheFilePath(url + "__text", ".txt", "eui_test_network_cache");
    if (tempFile.empty() || !downloadUrlToFile(url, tempFile, cancelToken)) {
        return false;
    }
    std::ifstream input(tempFile, std::ios::binary);
    if (!input.good()) {
        std::remove(tempFile.c_str());
        return false;
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    output = buffer.str();
    input.close();
    std::remove(tempFile.c_str());
    if (cancelToken != nullptr && cancelToken->canceled()) {
        output.clear();
        return false;
    }
    return !output.empty();
#else
    (void)url;
    output.clear();
    return false;
#endif
}

void requestText(const std::string& key, const std::string& url) {
    if (key.empty() || !isHttpUrl(url)) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(gTextMutex);
        TextState& state = gTextRequests[key];
        if (state.started) {
            return;
        }
        state.started = true;
        state.ready = false;
        state.ok = false;
        state.body.clear();
    }

    async::runOnce(
        "network.text." + key,
        [url](const async::CancelToken& token) {
            std::string body;
            const bool ok = downloadUrlToString(url, body, &token);
            if (!ok) {
                return async::failure<std::string>("Request failed.");
            }
            return async::success(std::move(body));
        },
        [key](const async::Result<std::string>& result) {
            {
                std::lock_guard<std::mutex> lock(gTextMutex);
                TextState& state = gTextRequests[key];
                state.ready = true;
                state.ok = result.ok;
                state.body = result.ok ? result.value : std::string{};
            }
            platform::requestUiUpdate();
        });
}

TextResult textResult(const std::string& key) {
    std::lock_guard<std::mutex> lock(gTextMutex);
    const auto it = gTextRequests.find(key);
    if (it == gTextRequests.end()) {
        return {};
    }
    return {it->second.ready, it->second.ok, it->second.body};
}

void shutdown() {
#if defined(EUI_HAS_CURL)
    curl_global_cleanup();
#endif
}

} // namespace core::network
