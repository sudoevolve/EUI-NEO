#pragma once

#include "pages/page_context.h"

#include <algorithm>
#include <cstddef>
#include <functional>
#include <string>
#include <utility>

namespace app::gallery {

inline void sectionEyebrow(eui::Ui& ui, const std::string& id, const std::string& text,
                           float x, float y, float width) {
    ui.text(id).position(x, y).size(width, 22.0f).text(text)
        .fontSize(12.0f).lineHeight(17.0f).fontWeight(760).color(kAccent).build();
}

inline void serifHeading(eui::Ui& ui, const std::string& id, const std::string& text,
                         float x, float y, float width, float height, float fontSize,
                         eui::Color color = kInk) {
    ui.text(id).position(x, y).size(width, height).text(text)
        .fontFamily("Georgia").fontSize(fontSize).lineHeight(fontSize * 1.08f)
        .fontWeight(520).color(color).wrap(true).build();
}

inline void bodyText(eui::Ui& ui, const std::string& id, const std::string& text,
                     float x, float y, float width, float height, float fontSize = 16.0f,
                     eui::Color color = kMuted) {
    ui.text(id).position(x, y).size(width, height).text(text)
        .fontSize(fontSize).lineHeight(fontSize * 1.55f).fontWeight(450)
        .color(color).wrap(true).build();
}

inline void footer(eui::Ui& ui, const std::string& id, float width, float y) {
    ui.rect(id + ".rule").position(0.0f, y).size(width, 1.0f).color(kRule).build();
    ui.text(id + ".copy").position(0.0f, y + 24.0f).size(width * 0.6f, 24.0f)
        .text("CALGARY ART GALLERY  /  EST. 1987").fontSize(11.0f).lineHeight(16.0f)
        .fontWeight(700).color(kMuted).build();
    ui.text(id + ".city").position(width * 0.6f, y + 24.0f).size(width * 0.4f, 24.0f)
        .text("CALGARY  -  CANADA").fontSize(11.0f).lineHeight(16.0f).fontWeight(700)
        .color(kMuted).horizontalAlign(eui::HorizontalAlign::Right).build();
}

inline bool intersectsViewport(float y, float height, float scrollOffset, float viewportHeight) {
    const float overscan = viewportHeight;
    return y + height >= scrollOffset - overscan && y <= scrollOffset + viewportHeight + overscan;
}

inline void artCard(eui::Ui& ui, const std::string& id, int artworkIndex,
                    float x, float y, float width, float height, float scrollOffset,
                    float viewportHeight, bool showCaption = true) {
    if (!intersectsViewport(y, height, scrollOffset, viewportHeight)) {
        return;
    }
    const Artwork& artwork = kArtworks[static_cast<std::size_t>(artworkIndex)];
    const std::string hitId = id + ".hit";
    const float captionHeight = showCaption ? 66.0f : 0.0f;
    ui.stack(id).position(x, y).size(width, height).clip()
        .visualStateFrom(hitId, 0.985f).transformedHitTest().content([&] {
            ui.rect(id + ".placeholder").size(width, height).color({0.86f, 0.84f, 0.78f, 1.0f}).build();
            ui.image(id + ".image").size(width, height).source(artwork.image)
                .coverViewport(width, height).runtimePointerTransformFrom(hitId, 0.0f, 0.0f, 1.045f, 0.0f, -4.0f)
                .transition(pageMotion()).build();
            ui.rect(id + ".hover").size(width, height).color({0.035f, 0.03f, 0.025f, 0.22f})
                .opacity(0.0f).hoverOpacityFrom(hitId, 0.0f, 1.0f).transition(quickMotion()).build();
            if (showCaption) {
                ui.rect(id + ".caption.bg").position(0.0f, height - captionHeight)
                    .size(width, captionHeight).color({0.025f, 0.022f, 0.02f, 0.70f}).build();
                ui.text(id + ".caption.title").position(18.0f, height - 52.0f)
                    .size(std::max(0.0f, width - 100.0f), 27.0f).text(artwork.title)
                    .fontFamily("Georgia").fontSize(17.0f).lineHeight(22.0f)
                    .color({1.0f, 0.99f, 0.96f, 1.0f}).build();
                ui.text(id + ".caption.year").position(std::max(0.0f, width - 76.0f), height - 52.0f)
                    .size(58.0f, 27.0f).text(artwork.year).fontSize(14.0f).lineHeight(20.0f)
                    .color({1.0f, 0.99f, 0.96f, 0.90f}).horizontalAlign(eui::HorizontalAlign::Right).build();
            }
            components::mouseArea(ui, hitId).size(width, height)
                .onTap([artworkIndex] { openArtwork(artworkIndex); }).build();
        }).build();
}

inline void textLink(eui::Ui& ui, const std::string& id, const std::string& label,
                     float x, float y, float width, std::function<void()> onClick) {
    ui.stack(id).position(x, y).size(width, 42.0f).content([&] {
        ui.text(id + ".label").size(width - 72.0f, 42.0f).text(label)
            .fontSize(14.0f).lineHeight(20.0f).fontWeight(700).color(kInk)
            .verticalAlign(eui::VerticalAlign::Center).build();
        ui.rect(id + ".rule").position(width - 70.0f, 20.0f).size(56.0f, 1.0f).color(kInk)
            .runtimePointerTransformFrom(id + ".hit", 0.0f, 0.0f, 1.0f, 8.0f, 0.0f)
            .transition(quickMotion()).build();
        ui.text(id + ".arrow").position(width - 24.0f, 0.0f).size(24.0f, 42.0f)
            .icon(0xF061).fontSize(13.0f).lineHeight(42.0f).color(kInk)
            .horizontalAlign(eui::HorizontalAlign::Right).verticalAlign(eui::VerticalAlign::Center)
            .runtimePointerTransformFrom(id + ".hit", 0.0f, 0.0f, 1.0f, 8.0f, 0.0f)
            .transition(quickMotion()).build();
        components::mouseArea(ui, id + ".hit").size(width, 42.0f).onTap(std::move(onClick)).build();
    }).build();
}

} // namespace app::gallery
