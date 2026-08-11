#pragma once

#include "components/gallery_components.h"

namespace app::gallery {

inline void composeNewsPage(eui::Ui& ui, float width, float viewportHeight) {
    const float pad = pagePadding(width);
    const float contentWidth = std::min(1240.0f, std::max(280.0f, width - pad * 2.0f));
    const float originX = std::max(pad, (width - contentWidth) * 0.5f);
    const bool wide = contentWidth >= 900.0f;
    const float pageHeight = wide ? 1260.0f : 2060.0f;
    const float scroll = state.newsScroll.get();

    ui.stack("news.document").position(originX, 0.0f).size(contentWidth, pageHeight).content([&] {
        sectionEyebrow(ui, "news.eyebrow", "JOURNAL / 2026", 0.0f, 48.0f, 220.0f);
        serifHeading(ui, "news.heading", "New perspectives,\ninside and outside.", 0.0f, 78.0f,
                     wide ? contentWidth * 0.48f : contentWidth, 158.0f, wide ? 53.0f : 43.0f);
        if (wide) {
            artCard(ui, "news.hero.art", 2, contentWidth * 0.55f, 48.0f,
                    contentWidth * 0.45f, 480.0f, scroll, viewportHeight, false);
            bodyText(ui, "news.hero.copy",
                     "Conversations with artists, studio notes and observations from the evolving programme.",
                     0.0f, 260.0f, contentWidth * 0.38f, 90.0f, 16.0f, kInk);
        } else {
            bodyText(ui, "news.hero.copy",
                     "Conversations with artists, studio notes and observations from the evolving programme.",
                     0.0f, 244.0f, contentWidth, 82.0f, 15.0f, kInk);
            artCard(ui, "news.hero.art", 2, 0.0f, 354.0f, contentWidth, 440.0f,
                    scroll, viewportHeight, false);
        }

        const float listTop = wide ? 612.0f : 850.0f;
        const float thumbWidth = wide ? 210.0f : 112.0f;
        const std::array<int, 3> newsArt{{4, 0, 6}};
        const std::array<const char*, 3> titles{{
            "A room built around colour",
            "Five questions for a new abstraction",
            "Public art and the memory of a city",
        }};
        const std::array<const char*, 3> dates{{"JUL 18, 2026", "JUN 02, 2026", "MAY 11, 2026"}};
        for (int index = 0; index < 3; ++index) {
            const float rowY = listTop + static_cast<float>(index) * (wide ? 188.0f : 310.0f);
            ui.rect("news.row.rule." + std::to_string(index)).position(0.0f, rowY)
                .size(contentWidth, 1.0f).color(kRule).build();
            sectionEyebrow(ui, "news.row.date." + std::to_string(index), dates[static_cast<std::size_t>(index)],
                           0.0f, rowY + 28.0f, wide ? 150.0f : contentWidth - thumbWidth - 18.0f);
            serifHeading(ui, "news.row.title." + std::to_string(index), titles[static_cast<std::size_t>(index)],
                         wide ? 180.0f : 0.0f, rowY + (wide ? 24.0f : 63.0f),
                         wide ? contentWidth - 180.0f - thumbWidth - 38.0f : contentWidth - thumbWidth - 18.0f,
                         wide ? 88.0f : 100.0f, wide ? 30.0f : 25.0f);
            artCard(ui, "news.row.art." + std::to_string(index), newsArt[static_cast<std::size_t>(index)],
                    contentWidth - thumbWidth, rowY + 24.0f, thumbWidth, wide ? 136.0f : 220.0f,
                    scroll, viewportHeight, false);
        }
        footer(ui, "news.footer", contentWidth, wide ? 1175.0f : 1900.0f);
    }).build();
}

} // namespace app::gallery
