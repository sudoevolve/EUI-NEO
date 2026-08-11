#pragma once

#include "components/detail_media.h"

namespace app::gallery {

inline void composeArtworkDetail(eui::Ui& ui, const eui::Screen& screen) {
    const bool hasLocal = state.selectedArtwork >= 0;
    const bool hasRemote = state.selectedPhoto >= 0 && state.selectedPhoto < static_cast<int>(state.feed.photos.size());
    if (!hasLocal && !hasRemote) {
        return;
    }

    const Artwork* artwork = hasLocal ? &kArtworks[static_cast<std::size_t>(state.selectedArtwork)] : nullptr;
    const UnsplashPhoto* photo = hasRemote ? &state.feed.photos[static_cast<std::size_t>(state.selectedPhoto)] : nullptr;
    const std::string title = photo != nullptr ? photo->title : artwork->title;
    const std::string artist = photo != nullptr ? photo->photographer : artwork->artist;
    const std::string medium = photo != nullptr ? "Photography / Unsplash" :
        std::string(artwork->year) + "\n" + artwork->medium;

    const float direction = state.detailClosing ? -1.0f : 1.0f;
    if ((direction > 0.0f && state.detailReveal < 1.0f) ||
        (direction < 0.0f && state.detailReveal > 0.0f)) {
        ui.stack("detail.ticker").size(1.0f, 1.0f).ignoreLayout().zIndex(1200)
            .onFrame([direction](float deltaSeconds) {
                state.detailReveal = clamp01(state.detailReveal + direction * deltaSeconds / 0.30f);
                if (state.detailClosing && state.detailReveal <= 0.0f) {
                    state.selectedArtwork = -1;
                    state.selectedPhoto = -1;
                    state.detailClosing = false;
                }
            }).build();
    }

    const float reveal = easeOutCubic(state.detailReveal);
    const bool compact = screen.width < 820.0f || screen.height < 650.0f;
    const float panelWidth = std::min(compact ? screen.width - 36.0f : 1040.0f, screen.width - 36.0f);
    const float panelHeight = std::min(compact ? screen.height - 38.0f : 650.0f, screen.height - 38.0f);
    const float panelX = (screen.width - panelWidth) * 0.5f;
    const float panelY = (screen.height - panelHeight) * 0.5f;
    const float mediaWidth = compact ? panelWidth : panelWidth * 0.60f;
    const float mediaHeight = compact ? panelHeight * 0.55f : panelHeight;
    const std::string image = photo != nullptr ? resizedUnsplashUrl(*photo, mediaWidth, true) : artwork->image;
    const std::string previewImage = photo != nullptr ? photo->readyListUrl : std::string{};
    const bool imageReady = photo == nullptr || eui::image::isSourceReady(image);
    const bool imageFailed = photo != nullptr && eui::image::hasSourceFailed(image);

    ui.stack("detail.overlay").size(screen.width, screen.height).zIndex(1100).content([&] {
        ui.rect("detail.backdrop").size(screen.width, screen.height)
            .color({0.025f, 0.022f, 0.02f, 0.76f * reveal}).onClick([] { closeArtwork(); }).build();
        ui.stack("detail.panel").position(panelX, panelY + (1.0f - reveal) * 24.0f)
            .size(panelWidth, panelHeight).opacity(reveal).scale(0.965f + reveal * 0.035f)
            .clip().transformedHitTest().content([&] {
                ui.rect("detail.panel.background").size(panelWidth, panelHeight).color(kPaper).build();
                components::mouseArea(ui, "detail.panel.block").size(panelWidth, panelHeight)
                    .cursor(eui::CursorShape::Arrow).build();
                if (!compact) {
                    detailMedia(ui, mediaWidth, mediaHeight, image, previewImage,
                                photo != nullptr, imageReady, imageFailed);
                    sectionEyebrow(ui, "detail.eyebrow", photo != nullptr ? "UNSPLASH PHOTOGRAPH" : "SELECTED WORK",
                                   panelWidth * 0.65f, 70.0f, panelWidth * 0.29f);
                    serifHeading(ui, "detail.title", title, panelWidth * 0.65f, 112.0f,
                                 panelWidth * 0.29f, 145.0f, 38.0f);
                    bodyText(ui, "detail.artist", artist, panelWidth * 0.65f, 286.0f,
                             panelWidth * 0.29f, 38.0f, 16.0f, kInk);
                    bodyText(ui, "detail.medium", medium, panelWidth * 0.65f, 342.0f,
                             panelWidth * 0.29f, 82.0f, 14.0f, kMuted);
                    if (photo != nullptr) {
                        textLink(ui, "detail.photo.link", "VIEW ON UNSPLASH", panelWidth * 0.65f, 460.0f,
                                 panelWidth * 0.29f, [url = photo->photoUrl] {
                                     if (!url.empty()) eui::platform::openUrl(url);
                                 });
                    } else {
                        bodyText(ui, "detail.description",
                                 "A study in rhythm, depth and material presence. The work rewards a slower look as colour shifts across the surface.",
                                 panelWidth * 0.65f, 464.0f, panelWidth * 0.29f, 116.0f, 14.0f, kInk);
                    }
                } else {
                    const float imageHeight = mediaHeight;
                    detailMedia(ui, mediaWidth, mediaHeight, image, previewImage,
                                photo != nullptr, imageReady, imageFailed);
                    sectionEyebrow(ui, "detail.eyebrow", photo != nullptr ? "UNSPLASH PHOTOGRAPH" : "SELECTED WORK",
                                   24.0f, imageHeight + 26.0f, panelWidth - 48.0f);
                    serifHeading(ui, "detail.title", title, 24.0f, imageHeight + 54.0f,
                                 panelWidth - 48.0f, 74.0f, 29.0f);
                    bodyText(ui, "detail.artist", artist, 24.0f, imageHeight + 132.0f,
                             panelWidth - 48.0f, 44.0f, 14.0f, kInk);
                }
                ui.stack("detail.close").position(panelWidth - 62.0f, 18.0f).size(44.0f, 44.0f)
                    .visualStateFrom("detail.close.hit", 0.94f).content([&] {
                        ui.rect("detail.close.circle").size(44.0f, 44.0f)
                            .color({0.985f, 0.982f, 0.972f, 0.96f})
                            .states({0.985f, 0.982f, 0.972f, 0.96f},
                                    {1.0f, 1.0f, 1.0f, 1.0f},
                                    {0.93f, 0.91f, 0.87f, 1.0f})
                            .border(1.5f, kInk).radius(22.0f)
                            .shadow(12.0f, 0.0f, 4.0f, {0.0f, 0.0f, 0.0f, 0.12f})
                            .transition(quickMotion()).build();
                        ui.rect("detail.close.x.forward").position(13.0f, 21.0f).size(18.0f, 2.0f)
                            .color(kInk).radius(1.0f).rotate(0.785398f).build();
                        ui.rect("detail.close.x.backward").position(13.0f, 21.0f).size(18.0f, 2.0f)
                            .color(kInk).radius(1.0f).rotate(-0.785398f).build();
                        components::mouseArea(ui, "detail.close.hit").size(44.0f, 44.0f)
                            .radius(22.0f).onTap([] { closeArtwork(); }).build();
                    }).build();
            }).build();
    }).build();
}

} // namespace app::gallery
