struct GalleryBingPage {
    float carouselPosition = 0.0f;

    std::string bingApiText() {
        const std::string key = "gallery.bing.text";
        eui::network::requestText(key, "https://www.bing.com/HPImageArchive.aspx?format=js&n=1&idx=0&mkt=zh-CN");
        const eui::network::TextResult result = eui::network::textResult(key);
        if (!result.ready) {
            return "Loading Bing API text...";
        }
        if (!result.ok) {
            return "Network text request failed.";
        }
        eui::json::Document document;
        const std::string copyright = document.parse(result.body)
            ? document.stringAt("/images/0/copyright")
            : std::string{};
        return copyright.empty() ? "Bing API returned text data." : copyright;
    }

    void compose(eui::Ui& ui, float width, float height) {
    const float contentWidth = std::max(260.0f, std::min(width, 860.0f));
    const float mediaHeight = 282.0f;
    const float apiHeight = 138.0f;
    const std::vector<components::CarouselItem> bingItems = {
        {"bing://daily?idx=0&mkt=zh-CN", "Bing Today", "zh-CN daily image"},
        {"bing://daily?idx=1&mkt=zh-CN", "Bing Yesterday", "previous daily image"},
        {"bing://daily?idx=2&mkt=zh-CN", "Two Days Ago", "archive image"},
        {"bing://daily?idx=3&mkt=zh-CN", "Earlier", "archive image"}
    };
    components::CarouselStyle carouselStyle(themeColors());
    carouselStyle.background = surface();
    carouselStyle.border = borderColor(0.72f);
    carouselStyle.text = eui::Color{1.0f, 1.0f, 1.0f, 0.96f};
    carouselStyle.mutedText = eui::Color{1.0f, 1.0f, 1.0f, 0.70f};
    carouselStyle.activeIndicator = accent();
    carouselStyle.shadow = components::theme::shadow(themeColors(), 26.0f, 10.0f, 0.30f, 0.16f);

    ui.column("bing.body")
        .size(width, height)
        .alignItems(eui::Align::CENTER)
        .gap(22.0f)
        .content([&] {
            components::carousel(ui, "bing.media.carousel")
                .size(contentWidth, mediaHeight)
                .items(bingItems)
                .index(carouselPosition)
                .cardWidthRatio(contentWidth < 520.0f ? 0.84f : 0.64f)
                .overlap(contentWidth < 520.0f ? 0.22f : 0.44f)
                .parallax(contentWidth < 520.0f ? 18.0f : 34.0f)
                .style(carouselStyle)
                .transition(pageTransition())
                .onChange([this](float next) {
                    carouselPosition = std::clamp(next, 0.0f, 3.0f);
                })
                .build();

            ui.stack("bing.api")
                .size(contentWidth, apiHeight)
                .content([&] {
                    ui.rect("bing.api.bg")
                        .size(contentWidth, apiHeight)
                        .color(surface())
                        .radius(18.0f)
                        .border(1.0f, borderColor())
                        .build();

                    ui.text("bing.api.title")
                        .x(22.0f)
                        .y(18.0f)
                        .size(std::max(0.0f, contentWidth - 44.0f), 30.0f)
                        .text("Bing API text")
                        .fontSize(22.0f)
                        .lineHeight(26.0f)
                        .color(textPrimary())
                        .build();

                    ui.text("bing.api.text")
                        .x(22.0f)
                        .y(54.0f)
                        .size(std::max(0.0f, contentWidth - 44.0f), std::max(0.0f, apiHeight - 68.0f))
                        .text(bingApiText())
                        .fontSize(16.0f)
                        .lineHeight(22.0f)
                        .maxWidth(std::max(0.0f, contentWidth - 44.0f))
                        .wrap(true)
                        .color(textMuted())
                        .build();
                })
                .build();
        });
    }
};

