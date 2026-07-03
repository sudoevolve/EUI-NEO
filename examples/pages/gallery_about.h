struct GalleryAboutPage {
    const char* windowBackendName() {
    #if defined(EUI_WINDOW_BACKEND_SDL3)
        return "SDL3";
    #elif defined(EUI_WINDOW_BACKEND_SDL2)
        return "SDL2";
    #else
        return "GLFW";
    #endif
    }

    const char* renderBackendName() {
    #if defined(EUI_RENDER_BACKEND_VULKAN)
        return "Vulkan";
    #elif defined(EUI_RENDER_BACKEND_OPENGL)
        return "OpenGL";
    #else
        return "Unknown";
    #endif
    }

    void compose(eui::Ui& ui, float width, float height) {
    const float contentWidth = std::max(280.0f, std::min(width, 860.0f));
    const bool compact = contentWidth < 620.0f;
    const float logoSize = compact ? 112.0f : 126.0f;
    const float buttonGap = compact ? 16.0f : 14.0f;
    const float buttonWidth = compact
        ? std::max(124.0f, std::min(180.0f, (contentWidth - buttonGap) * 0.5f))
        : 162.0f;
    const float buttonRowWidth = buttonWidth * 2.0f + buttonGap;
    const float heroHeight = compact ? 342.0f : 238.0f;
    const float runtimeHeight = compact ? 118.0f : 86.0f;
    const float licenseHeight = 92.0f;

    ui.column("about.body")
        .width(width)
        .height(eui::SizeValue::wrapContent())
        .alignItems(eui::Align::CENTER)
        .gap(20.0f)
        .content([&] {
            ui.stack("about.hero")
                .size(contentWidth, heroHeight)
                .content([&] {
                    ui.rect("about.hero.bg")
                        .size(contentWidth, heroHeight)
                        .color(surface())
                        .radius(22.0f)
                        .border(1.0f, borderColor())
                        .build();

                    const float logoX = compact ? (contentWidth - logoSize) * 0.5f : 24.0f;
                    const float logoY = compact ? 20.0f : 40.0f;
                    ui.rect("about.logo.frame")
                        .x(logoX)
                        .y(logoY)
                        .size(logoSize, logoSize)
                        .color(surfaceSoft())
                        .radius(28.0f)
                        .shadow(20.0f, 0.0f, 10.0f, shadowColor(0.24f, 0.12f))
                        .build();

                    ui.image("about.logo.image")
                        .x(logoX)
                        .y(logoY)
                        .size(logoSize, logoSize)
                        .source("assets/icon.png")
                        .radius(28.0f)
                        .cover()
                        .build();

                    const float infoX = compact ? 24.0f : logoX + logoSize + 30.0f;
                    const float infoY = compact ? logoY + logoSize + 18.0f : 38.0f;
                    const float infoWidth = compact ? std::max(0.0f, contentWidth - 48.0f) : std::max(0.0f, contentWidth - infoX - 24.0f);
                    ui.text("about.hero.title")
                        .x(infoX)
                        .y(infoY)
                        .size(infoWidth, 36.0f)
                        .text("EUI Neo")
                        .fontSize(32.0f)
                        .lineHeight(36.0f)
                        .color(textPrimary())
                        .horizontalAlign(compact ? eui::HorizontalAlign::Center : eui::HorizontalAlign::Left)
                        .build();

                    ui.text("about.hero.copy")
                        .x(infoX)
                        .y(infoY + 42.0f)
                        .size(infoWidth, compact ? 58.0f : 58.0f)
                        .text("A lightweight C++ UI playground for themed controls, motion and image rendering.")
                        .fontSize(17.0f)
                        .lineHeight(24.0f)
                        .maxWidth(infoWidth)
                        .wrap(true)
                        .color(textMuted())
                        .horizontalAlign(compact ? eui::HorizontalAlign::Center : eui::HorizontalAlign::Left)
                        .build();

                    ui.row("about.actions")
                        .x(compact ? (contentWidth - buttonRowWidth) * 0.5f : infoX)
                        .y(compact ? heroHeight - 74.0f : 162.0f)
                        .size(buttonRowWidth, 52.0f)
                        .gap(buttonGap)
                        .content([&] {
                            components::button(ui, "about.github")
                                .size(buttonWidth, 52.0f)
                                .icon(0xF0C1)
                                .iconSize(20.0f)
                                .fontSize(19.0f)
                                .text("GitHub")
                                .colors(accent(), buttonHover(accent()), buttonPressed(accent()))
                                .radius(12.0f)
                                .border(1.0f, withAlpha(accent(), 0.58f))
                                .shadow(14.0f, 0.0f, 5.0f, shadowColor(0.22f, 0.10f))
                                .transition(pageTransition())
                                .onClick([] {
                                    eui::platform::openUrl("https://github.com/sudoevolve/EUI-NEO");
                                })
                                .build();

                            components::button(ui, "about.group")
                                .size(buttonWidth, 52.0f)
                                .icon(0xF0C0)
                                .iconSize(19.0f)
                                .fontSize(19.0f)
                                .text("Group")
                                .colors(surfaceSoft(), buttonHover(surfaceSoft()), buttonPressed(surfaceSoft()))
                                .textColor(textPrimary())
                                .iconColor(textPrimary())
                                .radius(12.0f)
                                .border(1.0f, borderColor())
                                .shadow(12.0f, 0.0f, 4.0f, shadowColor(0.18f, 0.08f))
                                .transition(pageTransition())
                                .onClick([] {
                                    eui::platform::openUrl("https://qm.qq.com/q/kaPB4paOpa");
                                })
                                .build();
                        });
                })
                .build();

            ui.stack("about.runtime")
                .size(contentWidth, runtimeHeight)
                .content([&] {
                    ui.rect("about.runtime.bg")
                        .size(contentWidth, runtimeHeight)
                        .color(surface())
                        .radius(18.0f)
                        .border(1.0f, borderColor())
                        .build();

                    ui.text("about.runtime.title")
                        .x(22.0f)
                        .y(13.0f)
                        .size(std::max(0.0f, contentWidth - 44.0f), 24.0f)
                        .text("Runtime")
                        .fontSize(21.0f)
                        .lineHeight(24.0f)
                        .color(textPrimary())
                        .build();

                    const float itemGap = compact ? 8.0f : 14.0f;
                    const float itemWidth = compact
                        ? std::max(0.0f, contentWidth - 44.0f)
                        : std::max(0.0f, (contentWidth - 44.0f - itemGap) * 0.5f);
                    const float itemY = compact ? 46.0f : 44.0f;
                    const float secondX = compact ? 22.0f : 22.0f + itemWidth + itemGap;
                    const float secondY = compact ? itemY + 32.0f : itemY;

                    ui.text("about.runtime.window")
                        .x(22.0f)
                        .y(itemY)
                        .size(itemWidth, 24.0f)
                        .text(std::string("Window: ") + windowBackendName())
                        .fontSize(17.0f)
                        .lineHeight(22.0f)
                        .color(textMuted())
                        .build();

                    ui.text("about.runtime.render")
                        .x(secondX)
                        .y(secondY)
                        .size(itemWidth, 24.0f)
                        .text(std::string("Render: ") + renderBackendName())
                        .fontSize(17.0f)
                        .lineHeight(22.0f)
                        .color(textMuted())
                        .build();
                })
                .build();

            ui.stack("about.license")
                .size(contentWidth, licenseHeight)
                .content([&] {
                    ui.rect("about.license.bg")
                        .size(contentWidth, licenseHeight)
                        .color(surface())
                        .radius(18.0f)
                        .border(1.0f, borderColor())
                        .build();

                    ui.text("about.license.title")
                        .x(22.0f)
                        .y(12.0f)
                        .size(std::max(0.0f, contentWidth - 44.0f), 26.0f)
                        .text("License")
                        .fontSize(22.0f)
                        .lineHeight(25.0f)
                        .color(textPrimary())
                        .build();

                    ui.text("about.license.copy")
                        .x(22.0f)
                        .y(40.0f)
                        .size(std::max(0.0f, contentWidth - 44.0f), 22.0f)
                        .text("Copyright @2026 SudoEvolve")
                        .fontSize(17.0f)
                        .lineHeight(21.0f)
                        .color(textMuted())
                        .build();

                    ui.text("about.license.type")
                        .x(22.0f)
                        .y(64.0f)
                        .size(std::max(0.0f, contentWidth - 44.0f), 22.0f)
                        .text("Licensed under apache2.0")
                        .fontSize(16.0f)
                        .lineHeight(20.0f)
                        .color(textMuted())
                        .build();

                })
                .build();
        });
    }
};
