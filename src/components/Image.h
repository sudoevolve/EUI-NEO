#pragma once

#include "../EUINEO.h"
#include "../ui/UIBuilder.h"
#include <algorithm>
#include <cstddef>
#include <string>
#include <utility>

namespace EUINEO {

namespace ImageSource {

inline bool IsHttpUrl(const std::string& path) {
    return path.rfind("http://", 0) == 0 || path.rfind("https://", 0) == 0;
}

inline bool IsBingDailyScheme(const std::string& path) {
    return path.rfind("bing://daily", 0) == 0;
}

inline std::string QueryParamValue(const std::string& uri, const std::string& key) {
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
        const std::string currentKey = token.substr(0, eq);
        if (currentKey == key) {
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

inline std::string BuildBingDailyApiUrl(const std::string& uri) {
    std::string idx = QueryParamValue(uri, "idx");
    std::string mkt = QueryParamValue(uri, "mkt");
    if (idx.empty()) {
        idx = "0";
    }
    if (mkt.empty()) {
        mkt = "zh-CN";
    }
    return "https://www.bing.com/HPImageArchive.aspx?format=js&n=1&idx=" + idx + "&mkt=" + mkt;
}

inline std::string JsonUnescapeSimple(const std::string& value) {
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

inline std::string ExtractBingImageUrlFromJson(const std::string& payload) {
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
    std::string imageUrl = JsonUnescapeSimple(payload.substr(urlBegin, end - urlBegin));
    if (imageUrl.rfind("http://", 0) != 0 && imageUrl.rfind("https://", 0) != 0) {
        imageUrl = "https://www.bing.com" + imageUrl;
    }
    return imageUrl;
}

} // namespace ImageSource

class ImageNode : public UINode {
public:
    class Builder : public UIBuilderBase<ImageNode, Builder> {
    public:
        Builder(UIContext& context, ImageNode& node) : UIBuilderBase<ImageNode, Builder>(context, node) {}

        Builder& path(std::string value) {
            this->node_.trackComposeValue("path", value);
            this->node_.path_ = std::move(value);
            return *this;
        }

        Builder& url(std::string value) {
            return path(std::move(value));
        }

        Builder& api(std::string value) {
            return path(std::move(value));
        }

        Builder& flipVertically(bool value) {
            this->node_.trackComposeValue("flipVertically", value);
            this->node_.flipVertically_ = value;
            return *this;
        }

        Builder& tint(const Color& value) {
            this->node_.trackComposeValue("tintR", value.r);
            this->node_.trackComposeValue("tintG", value.g);
            this->node_.trackComposeValue("tintB", value.b);
            this->node_.trackComposeValue("tintA", value.a);
            this->node_.tint_ = value;
            return *this;
        }

        Builder& bingDaily(int idx = 0, std::string mkt = "zh-CN") {
            this->node_.trackComposeValue("bingIdx", idx);
            this->node_.trackComposeValue("bingMkt", mkt);
            this->node_.path_ = "bing://daily?idx=" + std::to_string(std::max(0, idx)) + "&mkt=" + std::move(mkt);
            return *this;
        }
    };

    explicit ImageNode(const std::string& key) : UINode(key) {
        resetDefaults();
    }

    static constexpr const char* StaticTypeName() {
        return "ImageNode";
    }

    const char* typeName() const override {
        return StaticTypeName();
    }

    void update() override {
        if (loadedPath_ == path_ && loadedFlipVertically_ == flipVertically_ && textureId_ != 0) {
            return;
        }
        const GLuint previousTextureId = textureId_;
        textureId_ = Renderer::LoadImageTexture(path_, flipVertically_);
        if (textureId_ != previousTextureId) {
            forceComposeDirty();
        }
        if (textureId_ == 0 && (ImageSource::IsHttpUrl(path_) || ImageSource::IsBingDailyScheme(path_))) {
            Renderer::RequestRepaint(0.2f);
        }
        if (textureId_ != 0 || loadedPath_ != path_ || loadedFlipVertically_ != flipVertically_) {
            loadedPath_ = path_;
            loadedFlipVertically_ = flipVertically_;
        }
    }

    void draw() override {
        PrimitiveClipScope clip(primitive_);
        const RectFrame frame = PrimitiveFrame(primitive_);
        if (textureId_ != 0) {
            Renderer::DrawImage(textureId_, frame.x, frame.y, frame.width, frame.height, primitive_.rounding,
                                ApplyOpacity(tint_, primitive_.opacity));
            return;
        }

        Renderer::DrawRect(frame.x, frame.y, frame.width, frame.height,
                           ApplyOpacity(CurrentTheme->surfaceHover, primitive_.opacity), primitive_.rounding);
    }

protected:
    void resetDefaults() override {
        primitive_ = UIPrimitive{};
        primitive_.width = 240.0f;
        primitive_.height = 160.0f;
        primitive_.rounding = 14.0f;
        path_.clear();
        flipVertically_ = true;
        tint_ = Color(1.0f, 1.0f, 1.0f, 1.0f);
    }

private:
    std::string path_;
    bool flipVertically_ = true;
    Color tint_ = Color(1.0f, 1.0f, 1.0f, 1.0f);
    GLuint textureId_ = 0;
    std::string loadedPath_;
    bool loadedFlipVertically_ = true;
};

} // namespace EUINEO
