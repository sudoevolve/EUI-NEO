#pragma once

#include "components/gallery_components.h"

namespace app::gallery {

inline constexpr float kCardCaptionHeight = 112.0f;

inline float localFallbackHeight(int index, float width) {
    constexpr std::array<float, 7> ratios{{1.27f, 0.78f, 1.50f, 1.31f, 0.76f, 0.80f, 0.78f}};
    return width * ratios[static_cast<std::size_t>(index % static_cast<int>(ratios.size()))] +
           kCardCaptionHeight;
}

inline void unsplashCard(eui::Ui& ui, const std::string& id, int index, float width, float height,
                         bool suspendRemoteImages) {
    UnsplashPhoto& photo = state.feed.photos[static_cast<std::size_t>(index)];
    const float imageHeight = std::max(1.0f, height - kCardCaptionHeight);
    const std::string source = resizedUnsplashUrl(photo, width);
    const bool ready = suspendRemoteImages ? !photo.readyListUrl.empty()
                                           : eui::image::isSourceReady(source);
    const bool failed = !suspendRemoteImages && eui::image::hasSourceFailed(source);
    if (!suspendRemoteImages && ready) {
        photo.readyListUrl = source;
    }
    const std::string& displaySource = suspendRemoteImages ? photo.readyListUrl : source;
    ui.rect(id + ".skeleton").size(width, imageHeight).color({0.87f, 0.85f, 0.80f, 1.0f})
        .radius(3.0f).build();
    ui.image(id + ".image").size(width, imageHeight).source(displaySource).cover()
        .radius(3.0f).opacity(ready ? 1.0f : 0.0f).translateY(ready ? 0.0f : 12.0f)
        .transition(pageMotion()).build();
    ui.rect(id + ".shade").size(width, imageHeight).color({0.02f, 0.018f, 0.015f, 0.18f})
        .opacity(0.0f).hoverOpacityFrom(id + ".image.hit", 0.0f, 1.0f).transition(quickMotion()).build();
    components::mouseArea(ui, id + ".image.hit").size(width, imageHeight)
        .onTap([index] { openPhoto(index); }).build();
    if (failed) {
        components::button(ui, id + ".image.retry")
            .position(std::max(12.0f, (width - 112.0f) * 0.5f), std::max(12.0f, (imageHeight - 42.0f) * 0.5f))
            .size(std::min(112.0f, width - 24.0f), 42.0f).text("RETRY IMAGE")
            .fontSize(10.0f).colors(kInk, kAccent, kInk).textColor(kPaper)
            .radius(2.0f).transition(quickMotion()).onClick([source] {
                eui::image::retrySource(source);
            }).build();
    }
    ui.stack(id + ".title.clip").position(0.0f, imageHeight + 10.0f)
        .size(width, 58.0f).clip().content([&] {
            ui.text(id + ".title").size(width, 58.0f).maxWidth(width).wrap(true)
                .text(photo.title).fontFamily("Georgia").fontSize(16.0f)
                .lineHeight(20.0f).fontWeight(520).color(kInk).build();
        }).build();
    const std::string photographer = photo.photographer.empty() ? "Unsplash" : photo.photographer;
    ui.stack(id + ".credit.clip").position(0.0f, imageHeight + 72.0f)
        .size(width, 32.0f).clip().content([&] {
            ui.text(id + ".credit").size(width, 32.0f).maxWidth(width).wrap(true)
                .text("Photo by " + photographer + " on Unsplash")
                .fontSize(11.0f).lineHeight(16.0f).fontWeight(650).color(kMuted).build();
        }).build();
    if (!photo.photographerUrl.empty()) {
        components::mouseArea(ui, id + ".credit.hit").position(0.0f, imageHeight + 68.0f)
            .size(width, 40.0f).onTap([url = photo.photographerUrl] { eui::platform::openUrl(url); }).build();
    }
}

inline void fallbackCard(eui::Ui& ui, const std::string& id, int index, float width, float height) {
    const Artwork& artwork = kArtworks[static_cast<std::size_t>(index)];
    const float imageHeight = std::max(1.0f, height - kCardCaptionHeight);
    ui.rect(id + ".skeleton").size(width, imageHeight).color({0.87f, 0.85f, 0.80f, 1.0f}).build();
    ui.image(id + ".image").size(width, imageHeight).source(artwork.image).cover().build();
    components::mouseArea(ui, id + ".image.hit").size(width, imageHeight)
        .onTap([index] { openArtwork(index); }).build();
    ui.stack(id + ".title.clip").position(0.0f, imageHeight + 10.0f)
        .size(width, 58.0f).clip().content([&] {
            ui.text(id + ".title").size(width, 58.0f).maxWidth(width).wrap(true)
                .text(artwork.title).fontFamily("Georgia").fontSize(16.0f).lineHeight(20.0f)
                .fontWeight(520).color(kInk).build();
        }).build();
    ui.stack(id + ".credit.clip").position(0.0f, imageHeight + 72.0f)
        .size(width, 32.0f).clip().content([&] {
            ui.text(id + ".credit").size(width, 32.0f).maxWidth(width).wrap(true)
                .text(std::string(artwork.artist) + "  /  " + artwork.year).fontSize(11.0f)
                .lineHeight(16.0f).fontWeight(650).color(kMuted).build();
        }).build();
}

inline void feedStatus(eui::Ui& ui, float width, float height, int columns) {
    if (!state.feed.loading && !state.feed.failed && !state.feed.exhausted) {
        return;
    }
    const float boxWidth = std::min(460.0f, std::max(240.0f, width - 36.0f));
    const float boxHeight = state.feed.failed ? 78.0f : 44.0f;
    const float x = (width - boxWidth) * 0.5f;
    const float y = std::max(12.0f, height - boxHeight - 18.0f);
    ui.stack("exhibitions.status").position(x, y).size(boxWidth, boxHeight).zIndex(20).content([&] {
        ui.rect("exhibitions.status.bg").size(boxWidth, boxHeight)
            .color({0.985f, 0.982f, 0.972f, 0.96f}).border(1.0f, kRule).radius(3.0f)
            .shadow(16.0f, 0.0f, 6.0f, {0.0f, 0.0f, 0.0f, 0.10f}).build();
        const std::string message = state.feed.loading ? "LOADING THE NEXT THREE ROWS..." :
                                    (state.feed.exhausted ? "END OF THE UNSPLASH COLLECTION" : state.feed.error);
        ui.text("exhibitions.status.text").position(16.0f, 0.0f)
            .size(state.feed.failed ? boxWidth - 126.0f : boxWidth - 32.0f, boxHeight)
            .text(message).fontSize(11.0f).lineHeight(16.0f).fontWeight(720)
            .color(state.feed.failed ? kAccent : kInk).wrap(true)
            .verticalAlign(eui::VerticalAlign::Center).build();
        if (state.feed.failed && !state.feed.missingKey) {
            components::button(ui, "exhibitions.status.retry").position(boxWidth - 102.0f, 17.0f)
                .size(86.0f, 44.0f).text("RETRY").fontSize(11.0f).colors(kInk, kAccent, kInk)
                .textColor(kPaper).radius(2.0f).onClick([columns] {
                    requestUnsplashBatch(columns, true);
                }).build();
        }
    }).build();
}

inline void composeExhibitionsPage(eui::Ui& ui, float width, float height) {
    consumeUnsplashBatch();
    const float pad = pagePadding(width);
    const float contentWidth = std::min(1240.0f, std::max(280.0f, width - pad * 2.0f));
    const float originX = std::max(pad, (width - contentWidth) * 0.5f);
    const bool compact = contentWidth < 720.0f;
    const float gridTop = 132.0f;
    const float gridHeight = std::max(120.0f, height - gridTop - 18.0f);
    const int columns = masonryColumns(contentWidth);
    const bool detailOpen = state.selectedArtwork >= 0 || state.selectedPhoto >= 0;
    if (state.feed.photos.empty() && !state.feed.loading && !state.feed.failed && !state.feed.exhausted) {
        requestUnsplashBatch(columns);
    }

    ui.stack("exhibitions.page").size(width, height).content([&] {
        sectionEyebrow(ui, "exhibitions.eyebrow", "PROGRAMME / UNSPLASH", originX, 24.0f, 260.0f);
        serifHeading(ui, "exhibitions.heading", "Exhibitions", originX, 48.0f,
                     compact ? contentWidth : contentWidth * 0.48f, 72.0f, compact ? 45.0f : 58.0f);
        if (!compact) {
            bodyText(ui, "exhibitions.intro",
                     "A continuously changing field of images. Only the visible works remain in graphics memory.",
                     originX + contentWidth * 0.56f, 48.0f, contentWidth * 0.44f, 64.0f, 14.0f, kInk);
        }
        const bool remote = !state.feed.photos.empty();
        const std::int64_t count = remote ? static_cast<std::int64_t>(state.feed.photos.size())
                                          : static_cast<std::int64_t>(kArtworks.size());
        components::virtualMasonry(ui, "exhibitions.masonry")
            .position(originX, gridTop).size(contentWidth, gridHeight)
            .itemCount(count).columns(columns).gap(compact ? 14.0f : 22.0f)
            .bind(state.exhibitionsScroll).step(58.0f).overscanViewports(1.0f)
            .endReachedThresholdViewports(1.0f).scrollbarWidth(3.0f).scrollbarGap(8.0f)
            .theme(galleryTheme()).transition(pageMotion())
            .itemHeight([remote](std::int64_t index, float itemWidth) {
                if (!remote) {
                    return localFallbackHeight(static_cast<int>(index), itemWidth);
                }
                const UnsplashPhoto& photo = state.feed.photos[static_cast<std::size_t>(index)];
                const float ratio = std::clamp(static_cast<float>(photo.height) / static_cast<float>(photo.width), 0.72f, 1.62f);
                return itemWidth * ratio + kCardCaptionHeight;
            })
            .onEndReached([columns, detailOpen] {
                if (!detailOpen) {
                    requestUnsplashBatch(columns);
                }
            })
            .item([remote, detailOpen](eui::Ui& itemUi, const std::string& id, std::int64_t index,
                           float itemWidth, float itemHeight) {
                if (remote) {
                    unsplashCard(itemUi, id, static_cast<int>(index), itemWidth, itemHeight, detailOpen);
                } else {
                    fallbackCard(itemUi, id, static_cast<int>(index), itemWidth, itemHeight);
                }
            }).build();
        feedStatus(ui, width, height, columns);
    }).build();
}

} // namespace app::gallery
