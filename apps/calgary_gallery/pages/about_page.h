#pragma once

#include "components/gallery_components.h"

namespace app::gallery {

inline void composeAboutPage(eui::Ui& ui, float width, float viewportHeight) {
    const float pad = pagePadding(width);
    const float contentWidth = std::min(1240.0f, std::max(280.0f, width - pad * 2.0f));
    const float originX = std::max(pad, (width - contentWidth) * 0.5f);
    const bool wide = contentWidth >= 930.0f;
    const float pageHeight = wide ? 1370.0f : 3200.0f;
    const float scroll = state.aboutScroll.get();

    ui.stack("about.document").position(originX, 0.0f).size(contentWidth, pageHeight).content([&] {
        if (wide) {
            const float gap = 34.0f;
            const float left = contentWidth * 0.38f;
            const float artWidth = (contentWidth - left - gap * 2.0f) * 0.5f;
            const float middleX = left + gap;
            const float rightX = middleX + artWidth + gap;
            serifHeading(ui, "about.brand.1", "Calgary", 0.0f, 46.0f, left - 20.0f, 76.0f, 62.0f);
            serifHeading(ui, "about.brand.2", "Art", 0.0f, 118.0f, 116.0f, 76.0f, 62.0f);
            serifHeading(ui, "about.brand.3", "Gallery.", 108.0f, 118.0f, left - 108.0f, 76.0f, 62.0f, kAccent);
            bodyText(ui, "about.intro",
                     "Since 1987, Calgary Art Gallery has presented contemporary art exhibitions showcasing emerging voices and enduring ideas. We create space for artists whose work changes how the world is seen.",
                     0.0f, 230.0f, left - 28.0f, 150.0f, 16.0f, kInk);
            textLink(ui, "about.more", "VIEW EXHIBITIONS", left - 206.0f, 390.0f, 206.0f,
                     [] { switchPage(Page::Exhibitions); });
            artCard(ui, "about.art.0", 0, middleX, 40.0f, artWidth, 540.0f, scroll, viewportHeight);
            artCard(ui, "about.art.1", 5, rightX, 40.0f, artWidth, 268.0f, scroll, viewportHeight, false);
            artCard(ui, "about.art.2", 6, rightX, 342.0f, artWidth, 610.0f, scroll, viewportHeight, false);
            artCard(ui, "about.art.3", 1, 96.0f, 530.0f, left - 96.0f, 570.0f, scroll, viewportHeight, false);
            artCard(ui, "about.art.4", 3, middleX, 616.0f, artWidth, 400.0f, scroll, viewportHeight, false);
            sectionEyebrow(ui, "about.note.eyebrow", "THE GALLERY", middleX, 1060.0f, artWidth);
            bodyText(ui, "about.note",
                     "A living archive of colour, memory and material. Every exhibition is a conversation between the artwork, the room and the visitor.",
                     middleX, 1096.0f, artWidth, 118.0f, 15.0f, kInk);
            footer(ui, "about.footer", contentWidth, 1280.0f);
        } else {
            const float titleSize = contentWidth < 470.0f ? 48.0f : 58.0f;
            serifHeading(ui, "about.mobile.brand", "Calgary\nArt Gallery.", 0.0f, 36.0f,
                         contentWidth, 160.0f, titleSize);
            bodyText(ui, "about.mobile.intro",
                     "Since 1987, Calgary Art Gallery has presented contemporary exhibitions showcasing emerging voices and enduring ideas.",
                     0.0f, 218.0f, contentWidth, 138.0f, 15.0f, kInk);
            textLink(ui, "about.mobile.more", "VIEW EXHIBITIONS", 0.0f, 368.0f,
                     std::min(220.0f, contentWidth), [] { switchPage(Page::Exhibitions); });
            float y = 452.0f;
            const std::array<int, 5> order{{0, 5, 6, 1, 3}};
            const std::array<float, 5> heights{{520.0f, 260.0f, 520.0f, 420.0f, 360.0f}};
            for (std::size_t index = 0; index < order.size(); ++index) {
                artCard(ui, "about.mobile.art." + std::to_string(index), order[index], 0.0f, y,
                        contentWidth, heights[index], scroll, viewportHeight, index == 0);
                y += heights[index] + 24.0f;
            }
            footer(ui, "about.mobile.footer", contentWidth, y + 18.0f);
        }
    }).build();
}

} // namespace app::gallery
