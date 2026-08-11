#pragma once

#include "eui_neo.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace app::gallery {

inline constexpr const char* kExampleUnsplashAccessKey =
    "2936LnaPNtkGmdCd7lWL93mJ26JUO5Bm-hyD65NWGOQ";

enum class Page { News = 0, About = 1, Exhibitions = 2, Contacts = 3 };

struct Artwork {
    const char* image;
    const char* title;
    const char* artist;
    const char* year;
    const char* medium;
};

inline constexpr std::array<Artwork, 7> kArtworks{{
    {"assets/art-01.jpg", "Chromatic Field No. 7", "Mira Sol", "2025", "Pigment and acrylic on canvas"},
    {"assets/art-02.jpg", "Blue Current", "Avery Lin", "2024", "Ink suspended in water"},
    {"assets/art-03.jpg", "Violet Passage", "Noor Vale", "2026", "Fluid acrylic on aluminum"},
    {"assets/art-04.jpg", "After the Last Bloom", "Elena March", "2023", "Oil and botanical study"},
    {"assets/art-05.jpg", "The Arrival", "Studio Archive", "1891", "Oil on canvas"},
    {"assets/art-06.jpg", "Autumn Range", "Calgary Editions", "2025", "Watercolor on cotton paper"},
    {"assets/art-07.jpg", "Look Back", "Theo Mar", "2024", "Mixed media mural study"},
}};

struct UnsplashPhoto {
    std::string id;
    std::string rawUrl;
    std::string readyListUrl;
    std::string title;
    std::string photographer;
    std::string photographerUrl;
    std::string photoUrl;
    int width = 1;
    int height = 1;
};

struct UnsplashFeed {
    std::vector<UnsplashPhoto> photos;
    std::unordered_set<std::string> ids;
    int nextPage = 1;
    int requestColumns = 0;
    int requestedCount = 0;
    bool loading = false;
    bool failed = false;
    bool exhausted = false;
    bool missingKey = false;
    std::string error;
};

struct GalleryState {
    Page page = Page::About;
    float pageReveal = 0.0f;
    int selectedArtwork = -1;
    int selectedPhoto = -1;
    float detailReveal = 0.0f;
    bool detailClosing = false;
    bool messageSent = false;
    eui::Signal<float> newsScroll{0.0f};
    eui::Signal<float> aboutScroll{0.0f};
    eui::Signal<float> exhibitionsScroll{0.0f};
    eui::Signal<float> contactsScroll{0.0f};
    eui::Signal<std::string> name{""};
    eui::Signal<std::string> email{""};
    eui::Signal<std::string> message{""};
    UnsplashFeed feed;
};

inline GalleryState state;

inline constexpr eui::Color kPaper{0.985f, 0.982f, 0.972f, 1.0f};
inline constexpr eui::Color kInk{0.045f, 0.043f, 0.040f, 1.0f};
inline constexpr eui::Color kMuted{0.34f, 0.33f, 0.31f, 1.0f};
inline constexpr eui::Color kRule{0.10f, 0.095f, 0.085f, 0.48f};
inline constexpr eui::Color kAccent{0.96f, 0.27f, 0.035f, 1.0f};

inline float clamp01(float value) {
    return std::clamp(value, 0.0f, 1.0f);
}

inline float easeOutCubic(float value) {
    const float inverse = 1.0f - clamp01(value);
    return 1.0f - inverse * inverse * inverse;
}

inline float pagePadding(float width) {
    if (width >= 1280.0f) return 72.0f;
    if (width >= 760.0f) return 42.0f;
    return 22.0f;
}

inline eui::Transition quickMotion() {
    auto transition = eui::Transition::make(0.18f, eui::Ease::OutCubic);
    transition.animate(eui::AnimProperty::Color | eui::AnimProperty::TextColor |
                       eui::AnimProperty::Opacity | eui::AnimProperty::Transform |
                       eui::AnimProperty::Shadow);
    return transition;
}

inline eui::Transition pageMotion() {
    auto transition = eui::Transition::make(0.34f, eui::Ease::OutCubic);
    transition.animate(eui::AnimProperty::Frame | eui::AnimProperty::Color |
                       eui::AnimProperty::TextColor | eui::AnimProperty::Opacity |
                       eui::AnimProperty::Transform);
    return transition;
}

inline components::theme::ThemeColorTokens galleryTheme() {
    auto tokens = components::theme::light();
    tokens.background = kPaper;
    tokens.surface = {1.0f, 1.0f, 1.0f, 0.78f};
    tokens.primary = kAccent;
    tokens.text = kInk;
    tokens.border = kRule;
    return tokens;
}

inline const char* pageLabel(Page page) {
    switch (page) {
        case Page::News: return "NEWS";
        case Page::About: return "ABOUT";
        case Page::Exhibitions: return "EXHIBITIONS";
        case Page::Contacts: return "CONTACTS";
    }
    return "ABOUT";
}

inline eui::Signal<float>& activeScrollSignal() {
    switch (state.page) {
        case Page::News: return state.newsScroll;
        case Page::About: return state.aboutScroll;
        case Page::Exhibitions: return state.exhibitionsScroll;
        case Page::Contacts: return state.contactsScroll;
    }
    return state.aboutScroll;
}

inline void switchPage(Page page) {
    if (state.page == page) {
        activeScrollSignal().set(0.0f);
        return;
    }
    state.page = page;
    state.pageReveal = 0.0f;
    state.messageSent = false;
    state.selectedArtwork = -1;
    state.selectedPhoto = -1;
    state.detailReveal = 0.0f;
    state.detailClosing = false;
    activeScrollSignal().set(0.0f);
}

inline void openArtwork(int index) {
    state.selectedArtwork = std::clamp(index, 0, static_cast<int>(kArtworks.size()) - 1);
    state.selectedPhoto = -1;
    state.detailReveal = 0.0f;
    state.detailClosing = false;
}

inline void openPhoto(int index) {
    if (index < 0 || index >= static_cast<int>(state.feed.photos.size())) {
        return;
    }
    state.selectedPhoto = index;
    state.selectedArtwork = -1;
    state.detailReveal = 0.0f;
    state.detailClosing = false;
}

inline void closeArtwork() {
    if (state.selectedArtwork >= 0 || state.selectedPhoto >= 0) {
        state.detailClosing = true;
    }
}

inline std::string withQuery(std::string url, const std::string& query) {
    if (url.empty()) {
        return url;
    }
    url += url.find('?') == std::string::npos ? "?" : "&";
    url += query;
    return url;
}

inline int imageWidthBucket(float displayWidth, bool detail = false) {
    if (detail) {
        const float target = displayWidth * 2.0f;
        if (target <= 1280.0f) return 1280;
        if (target <= 1600.0f) return 1600;
        return 1920;
    }
    const float target = displayWidth * 2.0f;
    if (target <= 720.0f) return 640;
    if (target <= 1080.0f) return 960;
    return 1280;
}

inline std::string resizedUnsplashUrl(const UnsplashPhoto& photo, float displayWidth, bool detail = false) {
    return withQuery(photo.rawUrl,
                     "auto=format&fit=max&q=82&w=" + std::to_string(imageWidthBucket(displayWidth, detail)));
}

inline void requestUnsplashBatch(int columns, bool retry = false) {
    UnsplashFeed& feed = state.feed;
    if (feed.loading || feed.exhausted || (feed.failed && !retry)) {
        return;
    }
    const char* accessKey = std::getenv("EUI_UNSPLASH_ACCESS_KEY");
    if (accessKey == nullptr || *accessKey == '\0') {
        accessKey = kExampleUnsplashAccessKey;
    }
    if (*accessKey == '\0') {
        feed.missingKey = true;
        feed.failed = true;
        feed.error = "Set EUI_UNSPLASH_ACCESS_KEY to load the live feed.";
        return;
    }

    feed.missingKey = false;
    feed.failed = false;
    feed.error.clear();
    feed.requestColumns = std::clamp(columns, 1, 5);
    feed.requestedCount = std::min(15, feed.requestColumns * 3);
    feed.loading = true;
    const std::string url = "https://api.unsplash.com/photos?page=" + std::to_string(feed.nextPage) +
                            "&per_page=" + std::to_string(feed.requestedCount) +
                            "&order_by=latest" +
                            "&client_id=" + std::string(accessKey);
    eui::network::requestText("calgary.unsplash.feed", url);
}

inline bool readString(const eui::json::Value& object, const std::string& key, std::string& output) {
    return object.get(key).string(output);
}

inline void failUnsplashFeed(std::string message) {
    state.feed.loading = false;
    state.feed.failed = true;
    state.feed.error = std::move(message);
}

inline void consumeUnsplashBatch() {
    if (!state.feed.loading) {
        return;
    }
    eui::network::TextResult result = eui::network::consumeTextResult("calgary.unsplash.feed");
    if (!result.ready) {
        return;
    }
    if (!result.ok) {
        failUnsplashFeed("Unsplash request failed. Check the network or API rate limit.");
        return;
    }

    eui::json::Document document;
    if (!document.parse(result.body)) {
        failUnsplashFeed("Unsplash returned an invalid response.");
        return;
    }

    const eui::json::Value root = document.root();
    int added = 0;
    for (std::size_t index = 0; index < root.size(); ++index) {
        const eui::json::Value value = root.at(index);
        UnsplashPhoto photo;
        if (!readString(value, "id", photo.id) || !readString(value.get("urls"), "raw", photo.rawUrl) ||
            photo.id.empty() || photo.rawUrl.empty() || state.feed.ids.count(photo.id) != 0) {
            continue;
        }
        double width = 1.0;
        double height = 1.0;
        value.get("width").number(width);
        value.get("height").number(height);
        photo.width = std::max(1, static_cast<int>(width));
        photo.height = std::max(1, static_cast<int>(height));
        readString(value, "alt_description", photo.title);
        if (photo.title.empty()) {
            photo.title = "Untitled photograph";
        }
        readString(value.get("user"), "name", photo.photographer);
        readString(value.get("user").get("links"), "html", photo.photographerUrl);
        readString(value.get("links"), "html", photo.photoUrl);
        photo.photographerUrl = withQuery(photo.photographerUrl,
            "utm_source=eui_neo_gallery&utm_medium=referral");
        photo.photoUrl = withQuery(photo.photoUrl,
            "utm_source=eui_neo_gallery&utm_medium=referral");
        state.feed.ids.insert(photo.id);
        state.feed.photos.push_back(std::move(photo));
        ++added;
    }

    state.feed.loading = false;
    state.feed.failed = false;
    state.feed.error.clear();
    if (added == 0 || root.size() < static_cast<std::size_t>(state.feed.requestedCount)) {
        state.feed.exhausted = true;
    } else {
        ++state.feed.nextPage;
    }
}

inline int masonryColumns(float width) {
    if (width >= 1180.0f) return 5;
    if (width >= 930.0f) return 4;
    if (width >= 680.0f) return 3;
    if (width >= 440.0f) return 2;
    return 1;
}

} // namespace app::gallery
