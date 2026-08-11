#pragma once

#include "components/gallery_components.h"

namespace app::gallery {

inline void detailMedia(eui::Ui& ui, float width, float height,
                        const std::string& source, const std::string& previewSource,
                        bool remote, bool ready, bool failed) {
    const float statusWidth = std::min(214.0f, width - 24.0f);
    const float statusX = std::max(12.0f, (width - statusWidth) * 0.5f);
    const float statusY = std::max(12.0f, height - 66.0f);

    ui.stack("detail.media").size(width, height).clip().content([&] {
        ui.rect("detail.media.skeleton").size(width, height)
            .color({0.86f, 0.84f, 0.79f, 1.0f}).build();
        if (!previewSource.empty()) {
            ui.image("detail.media.preview").size(width, height).source(previewSource)
                .coverViewport(width, height).build();
        }
        ui.image("detail.media.full").size(width, height).source(source)
            .coverViewport(width, height).opacity(ready ? 1.0f : 0.0f)
            .transition(pageMotion()).build();
        if (remote && !ready) {
            ui.rect("detail.media.status.bg").position(statusX, statusY).size(statusWidth, 42.0f)
                .color({0.025f, 0.022f, 0.02f, 0.78f}).radius(2.0f).build();
            if (failed) {
                components::button(ui, "detail.media.retry")
                    .position(statusX, statusY).size(statusWidth, 42.0f)
                    .text("RETRY HIGH-RES IMAGE").fontSize(10.0f)
                    .colors(kInk, kAccent, kInk).textColor(kPaper)
                    .radius(2.0f).transition(quickMotion()).onClick([source] {
                        eui::image::retrySource(source);
                    }).build();
            } else {
                ui.text("detail.media.status.text").position(statusX, statusY).size(statusWidth, 42.0f)
                    .text("LOADING HIGH-RES IMAGE").fontSize(10.0f).lineHeight(14.0f)
                    .fontWeight(720).color(kPaper)
                    .horizontalAlign(eui::HorizontalAlign::Center)
                    .verticalAlign(eui::VerticalAlign::Center).build();
            }
        }
    }).build();
}

} // namespace app::gallery
