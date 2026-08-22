struct GalleryLayoutPage {
    bool loaderKeepAlive = true;
    bool loaderOpen = true;
    float localScroll = 0.0f;
    float virtualScroll = 0.0f;
    int loaderCounter = 0;

    void cardTitle(eui::Ui& ui, const std::string& id, const std::string& text, float width) {
        ui.text(id)
            .size(width, 24.0f)
            .text(text)
            .fontSize(17.0f)
            .lineHeight(22.0f)
            .fontWeight(760)
            .color(textPrimary())
            .verticalAlign(eui::VerticalAlign::Center)
            .build();
    }

    void note(eui::Ui& ui, const std::string& id, const std::string& text, float width, float y) {
        ui.text(id)
            .position(0.0f, y)
            .size(width, 22.0f)
            .text(text)
            .fontSize(14.0f)
            .lineHeight(19.0f)
            .color(textMuted())
            .verticalAlign(eui::VerticalAlign::Center)
            .build();
    }

    void pill(eui::Ui& ui, const std::string& id, const std::string& text, float width, float height, eui::Color color) {
        const float luminance = color.r * 0.2126f + color.g * 0.7152f + color.b * 0.0722f;
        const eui::Color labelColor = luminance > 0.58f ? textPrimary() : eui::Color{0.96f, 0.98f, 1.0f, 1.0f};
        ui.stack(id)
            .size(width, height)
            .content([&] {
                ui.rect(id + ".bg")
                    .fill()
                    .color(color)
                    .radius(10.0f)
                    .transition(pageTransition())
                    .animate(eui::AnimProperty::Color)
                    .build();

                ui.text(id + ".text")
                    .fill()
                    .text(text)
                    .fontSize(13.0f)
                    .lineHeight(18.0f)
                    .fontWeight(720)
                    .color(labelColor)
                    .horizontalAlign(eui::HorizontalAlign::Center)
                    .verticalAlign(eui::VerticalAlign::Center)
                    .build();
            })
            .build();
    }

    void rowColumnCard(eui::Ui& ui, float width) {
        const float inset = 18.0f;
        const float contentWidth = std::max(0.0f, width - inset * 2.0f);
        const float itemWidth = std::max(54.0f, (contentWidth - 20.0f) / 3.0f);

        components::card(ui, "layout.rowcol")
            .width(width)
            .height(178.0f)
            .padding(inset)
            .theme(themeColors())
            .content([&] {
                cardTitle(ui, "layout.rowcol.title", "row + column", contentWidth);
                note(ui, "layout.rowcol.note", "gap + alignItems + fixed child sizes", contentWidth, 28.0f);

                ui.row("layout.rowcol.row")
                    .position(0.0f, 66.0f)
                    .size(contentWidth, 58.0f)
                    .gap(10.0f)
                    .alignItems(eui::Align::CENTER)
                    .content([&] {
                        pill(ui, "layout.rowcol.a", "A", itemWidth, 42.0f, accent());
                        pill(ui, "layout.rowcol.b", "B", itemWidth, 58.0f, surfaceActive());
                        pill(ui, "layout.rowcol.c", "C", itemWidth, 34.0f, surfaceSoft());
                    })
                    .build();
            })
            .build();
    }

    void stackPaddingCard(eui::Ui& ui, float width) {
        const float inset = 20.0f;
        const float contentWidth = std::max(0.0f, width - inset * 2.0f);
        const float stageHeight = 88.0f;

        components::card(ui, "layout.stack")
            .width(width)
            .height(194.0f)
            .padding(inset)
            .theme(themeColors())
            .content([&] {
                cardTitle(ui, "layout.stack.title", "stack + padding", contentWidth);
                note(ui, "layout.stack.note", "absolute children share one local coordinate space", contentWidth, 28.0f);

                ui.stack("layout.stack.stage")
                    .position(0.0f, 72.0f)
                    .size(contentWidth, stageHeight)
                    .content([&] {
                        ui.rect("layout.stack.stage.bg")
                            .fill()
                            .color(surfaceSoft())
                            .radius(14.0f)
                            .build();
                        ui.rect("layout.stack.back")
                            .position(18.0f, 18.0f)
                            .size(std::max(0.0f, contentWidth - 80.0f), 44.0f)
                            .color(withAlpha(accent(), 0.34f))
                            .radius(12.0f)
                            .build();
                        ui.rect("layout.stack.front")
                            .position(std::max(0.0f, contentWidth - 118.0f), 30.0f)
                            .size(88.0f, 38.0f)
                            .color(accent())
                            .radius(12.0f)
                            .shadow(18.0f, 0.0f, 8.0f, shadowColor(0.22f, 0.12f))
                            .build();
                        components::layoutDebugOverlay(ui, "layout.stack.debug", contentWidth, stageHeight, 18.0f, "debug overlay");
                    })
                    .build();
            })
            .build();
    }

    void fillWrapCard(eui::Ui& ui, float width) {
        const float inset = 18.0f;
        const float contentWidth = std::max(0.0f, width - inset * 2.0f);

        components::card(ui, "layout.fillwrap")
            .width(width)
            .wrapContentHeight()
            .padding(inset)
            .theme(themeColors())
            .content([&] {
                ui.column("layout.fillwrap.column")
                    .width(contentWidth)
                    .height(eui::SizeValue::wrapContent())
                    .gap(10.0f)
                    .content([&] {
                        cardTitle(ui, "layout.fillwrap.title", "wrapContentHeight + fill()", contentWidth);
                        pill(ui, "layout.fillwrap.one", "fixed row", contentWidth, 38.0f, surfaceSoft());
                        ui.stack("layout.fillwrap.target")
                            .size(contentWidth, 66.0f)
                            .content([&] {
                                ui.rect("layout.fillwrap.target.bg")
                                    .fill()
                                    .color(withAlpha(accent(), 0.28f))
                                    .radius(12.0f)
                                    .build();
                                ui.text("layout.fillwrap.target.text")
                                    .fill()
                                    .text("child fill() stays inside this target")
                                    .fontSize(14.0f)
                                    .lineHeight(19.0f)
                                    .color(textPrimary())
                                    .horizontalAlign(eui::HorizontalAlign::Center)
                                    .verticalAlign(eui::VerticalAlign::Center)
                                    .build();
                            })
                            .build();
                        pill(ui, "layout.fillwrap.two", "wrap card grows with content", contentWidth, 38.0f, surfaceActive());
                    })
                    .build();
            })
            .build();
    }

    void scrollCard(eui::Ui& ui, float width) {
        const float inset = 18.0f;
        const float contentWidth = std::max(0.0f, width - inset * 2.0f);

        components::card(ui, "layout.scroll")
            .width(width)
            .height(266.0f)
            .padding(inset)
            .theme(themeColors())
            .content([&] {
                cardTitle(ui, "layout.scroll.title", "scrollView", contentWidth);
                note(ui, "layout.scroll.note", "measured wrap content + clipped viewport", contentWidth, 28.0f);

                components::scrollView(ui, "layout.scroll.viewport")
                    .theme(themeColors())
                    .position(0.0f, 70.0f)
                    .size(contentWidth, 144.0f)
                    .offset(localScroll)
                    .gap(8.0f)
                    .step(32.0f)
                    .scrollbarWidth(7.0f)
                    .scrollbarGap(10.0f)
                    .contentKey("layout.scroll.sample")
                    .onChange([this](float value) {
                        localScroll = value;
                    })
                    .content([&](eui::Ui& contentUi, float rowWidth, float) {
                        for (int i = 0; i < 8; ++i) {
                            pill(contentUi,
                                 "layout.scroll.row." + std::to_string(i),
                                 "scroll row " + std::to_string(i + 1),
                                 rowWidth,
                                 30.0f,
                                 i % 2 == 0 ? surfaceSoft() : surfaceActive());
                        }
                    })
                    .build();
            })
            .build();
    }

    void virtualListCard(eui::Ui& ui, float width) {
        const float inset = 18.0f;
        const float contentWidth = std::max(0.0f, width - inset * 2.0f);
        constexpr long long itemCount = 100000;
        constexpr float rowHeight = 32.0f;

        auto rowColor = [](auto index) {
            const float r = 0.16f + static_cast<float>((index * 37) % 120) / 255.0f;
            const float g = 0.30f + static_cast<float>((index * 19) % 100) / 255.0f;
            const float b = 0.42f + static_cast<float>((index * 53) % 110) / 255.0f;
            return eui::Color{r, g, b, 1.0f};
        };

        components::card(ui, "layout.virtual")
            .width(width)
            .height(286.0f)
            .padding(inset)
            .theme(themeColors())
            .content([&] {
                cardTitle(ui, "layout.virtual.title", "virtualList", contentWidth);
                note(ui, "layout.virtual.note", "fixed row height + overscanned visible slots", contentWidth, 28.0f);

                components::virtualList(ui, "layout.virtual.viewport")
                    .theme(themeColors())
                    .position(0.0f, 70.0f)
                    .size(contentWidth, 164.0f)
                    .itemCount(itemCount)
                    .rowHeight(rowHeight)
                    .offset(virtualScroll)
                    .step(rowHeight * 2.0f)
                    .overscanViewports(1.0f)
                    .scrollbarWidth(7.0f)
                    .scrollbarGap(10.0f)
                    .transition(eui::Transition{})
                    .onChange([this](float value) {
                        virtualScroll = value;
                    })
                    .row([&](eui::Ui& rowUi, const std::string& rowId, auto index, float rowWidth, float height) {
                        const eui::Color color = rowColor(index);
                        const eui::Color bg = index % 2 == 0 ? surfaceSoft() : surfaceActive();

                        rowUi.rect(rowId + ".bg")
                            .x(0.0f)
                            .y(3.0f)
                            .size(std::max(0.0f, rowWidth - 2.0f), height - 6.0f)
                            .color(bg)
                            .radius(9.0f)
                            .border(1.0f, withAlpha(color, 0.22f))
                            .build();
                        rowUi.rect(rowId + ".mark")
                            .x(10.0f)
                            .y(10.0f)
                            .size(6.0f, height - 20.0f)
                            .color(color)
                            .radius(3.0f)
                            .build();
                        rowUi.text(rowId + ".text")
                            .x(26.0f)
                            .y(0.0f)
                            .size(std::max(0.0f, rowWidth - 42.0f), height)
                            .text("virtual row " + std::to_string(index + 1) + " / " + std::to_string(itemCount))
                            .fontSize(13.0f)
                            .lineHeight(18.0f)
                            .fontWeight(700)
                            .color(textPrimary())
                            .verticalAlign(eui::VerticalAlign::Center)
                            .build();
                    })
                    .build();
            })
            .build();
    }

    void loaderCard(eui::Ui& ui, float width) {
        const float inset = 18.0f;
        const float contentWidth = std::max(0.0f, width - inset * 2.0f);
        const float buttonWidth = std::max(0.0f, (contentWidth - 12.0f) * 0.5f);

        components::card(ui, "layout.loader")
            .width(width)
            .height(230.0f)
            .padding(inset)
            .theme(themeColors())
            .content([&] {
                cardTitle(ui, "layout.loader.title", "loader", contentWidth);
                note(ui, "layout.loader.note", loaderKeepAlive ? "keepAlive keeps composed state visible across hides" : "destroyOnHide removes the subtree when hidden", contentWidth, 28.0f);

                ui.row("layout.loader.controls")
                    .position(0.0f, 62.0f)
                    .size(contentWidth, 48.0f)
                    .gap(12.0f)
                    .content([&] {
                        components::button(ui, "layout.loader.toggle")
                            .size(buttonWidth, 44.0f)
                            .text(loaderOpen ? "Hide" : "Show")
                            .colors(accent(), buttonHover(accent()), buttonPressed(accent()))
                            .textColor(eui::Color{0.96f, 0.98f, 1.0f, 1.0f})
                            .transition(pageTransition())
                            .onClick([this] {
                                loaderOpen = !loaderOpen;
                            })
                            .build();

                        components::button(ui, "layout.loader.mode")
                            .size(buttonWidth, 44.0f)
                            .text(loaderKeepAlive ? "Keep" : "Destroy")
                            .colors(surfaceSoft(), buttonHover(surfaceSoft()), buttonPressed(surfaceSoft()))
                            .textColor(textPrimary())
                            .transition(pageTransition())
                            .onClick([this] {
                                loaderKeepAlive = !loaderKeepAlive;
                            })
                            .build();
                    })
                    .build();

                ui.stack("layout.loader.slot")
                    .position(0.0f, 124.0f)
                    .size(contentWidth, 64.0f)
                    .content([&] {
                        ui.rect("layout.loader.slot.bg")
                            .fill()
                            .color(surfaceSoft())
                            .radius(14.0f)
                            .build();

                        auto loader = ui.loader("layout.loader.sample")
                            .active(loaderOpen)
                            .content([&] {
                                ui.stack("layout.loader.sample.card")
                                    .size(contentWidth, 64.0f)
                                    .content([&] {
                                        ui.rect("layout.loader.sample.bg")
                                            .fill()
                                            .color(withAlpha(accent(), 0.34f))
                                            .radius(14.0f)
                                            .build();
                                        ui.text("layout.loader.sample.text")
                                            .fill()
                                            .text("loaded subtree")
                                            .fontSize(15.0f)
                                            .lineHeight(20.0f)
                                            .color(textPrimary())
                                            .horizontalAlign(eui::HorizontalAlign::Center)
                                            .verticalAlign(eui::VerticalAlign::Center)
                                            .build();
                                    })
                                    .onClick([this] {
                                        ++loaderCounter;
                                    })
                                    .build();
                            });
                        if (loaderKeepAlive) {
                            loader.keepAlive();
                        } else {
                            loader.destroyOnHide();
                        }
                        loader.build();

                        if (!loaderOpen) {
                            ui.text("layout.loader.empty")
                                .fill()
                                .text("loader inactive")
                                .fontSize(15.0f)
                                .lineHeight(20.0f)
                                .color(textMuted())
                                .horizontalAlign(eui::HorizontalAlign::Center)
                                .verticalAlign(eui::VerticalAlign::Center)
                                .build();
                        }
                    })
                    .build();
            })
            .build();
    }

    void flexCard(eui::Ui& ui, float width) {
        const float inset = 18.0f;
        const float contentWidth = std::max(0.0f, width - inset * 2.0f);

        components::card(ui, "layout.flex")
            .width(width)
            .height(186.0f)
            .padding(inset)
            .theme(themeColors())
            .content([&] {
                cardTitle(ui, "layout.flex.title", "flexGrow / flexShrink", contentWidth);
                note(ui, "layout.flex.note", "fixed side columns with a flexible center region", contentWidth, 28.0f);

                ui.row("layout.flex.row")
                    .position(0.0f, 72.0f)
                    .size(contentWidth, 62.0f)
                    .gap(10.0f)
                    .content([&] {
                        pill(ui, "layout.flex.left", "fixed", 72.0f, 54.0f, surfaceSoft());
                        ui.stack("layout.flex.center")
                            .height(54.0f)
                            .flexGrow(1.0f)
                            .minWidth(92.0f)
                            .content([&] {
                                ui.rect("layout.flex.center.bg")
                                    .fill()
                                    .color(withAlpha(accent(), 0.36f))
                                    .radius(10.0f)
                                    .build();
                                ui.text("layout.flex.center.text")
                                    .fill()
                                    .text("flexGrow")
                                    .fontSize(13.0f)
                                    .lineHeight(18.0f)
                                    .fontWeight(720)
                                    .color(textPrimary())
                                    .horizontalAlign(eui::HorizontalAlign::Center)
                                    .verticalAlign(eui::VerticalAlign::Center)
                                    .build();
                            })
                            .build();
                        pill(ui, "layout.flex.right", "fixed", 72.0f, 54.0f, surfaceActive());
                    })
                    .build();
            })
            .build();
    }

    void alignCard(eui::Ui& ui, float width) {
        const float inset = 18.0f;
        const float contentWidth = std::max(0.0f, width - inset * 2.0f);
        const float laneWidth = std::max(0.0f, (contentWidth - 16.0f) / 3.0f);

        components::card(ui, "layout.align")
            .width(width)
            .height(212.0f)
            .padding(inset)
            .theme(themeColors())
            .content([&] {
                cardTitle(ui, "layout.align.title", "alignment", contentWidth);
                note(ui, "layout.align.note", "justifyContent + alignItems in fixed lanes", contentWidth, 28.0f);

                ui.row("layout.align.lanes")
                    .position(0.0f, 70.0f)
                    .size(contentWidth, 100.0f)
                    .gap(8.0f)
                    .content([&] {
                        alignLane(ui, "layout.align.start", "start", laneWidth, eui::Align::START, eui::Align::START);
                        alignLane(ui, "layout.align.center", "center", laneWidth, eui::Align::CENTER, eui::Align::CENTER);
                        alignLane(ui, "layout.align.end", "end", laneWidth, eui::Align::END, eui::Align::END);
                    })
                    .build();
            })
            .build();
    }

    void alignLane(eui::Ui& ui, const std::string& id, const std::string& label, float width, eui::Align main, eui::Align cross) {
        ui.column(id)
            .size(width, 100.0f)
            .padding(8.0f)
            .justifyContent(main)
            .alignItems(cross)
            .content([&] {
                ui.rect(id + ".bg")
                    .fill()
                    .ignoreLayout()
                    .color(surfaceSoft())
                    .radius(10.0f)
                    .zIndex(-1)
                    .build();
                pill(ui, id + ".chip", label, std::max(46.0f, width - 26.0f), 28.0f, main == eui::Align::CENTER ? accent() : surfaceActive());
            })
            .build();
    }

    void clipCard(eui::Ui& ui, float width) {
        const float inset = 18.0f;
        const float contentWidth = std::max(0.0f, width - inset * 2.0f);

        components::card(ui, "layout.clip")
            .width(width)
            .height(196.0f)
            .padding(inset)
            .theme(themeColors())
            .content([&] {
                cardTitle(ui, "layout.clip.title", "clip + overflow", contentWidth);
                note(ui, "layout.clip.note", "children outside the frame are clipped by the parent", contentWidth, 28.0f);

                ui.stack("layout.clip.stage")
                    .position(0.0f, 70.0f)
                    .size(contentWidth, 84.0f)
                    .clip()
                    .content([&] {
                        ui.rect("layout.clip.stage.bg")
                            .fill()
                            .color(surfaceSoft())
                            .radius(14.0f)
                            .build();
                        ui.rect("layout.clip.large")
                            .position(-36.0f, 18.0f)
                            .size(contentWidth + 72.0f, 48.0f)
                            .gradient(withAlpha(accent(), 0.18f), withAlpha(accent(), 0.62f), eui::GradientDirection::Horizontal)
                            .radius(24.0f)
                            .build();
                        ui.text("layout.clip.text")
                            .fill()
                            .text("clipped overflow")
                            .fontSize(14.0f)
                            .lineHeight(20.0f)
                            .fontWeight(720)
                            .color(textPrimary())
                            .horizontalAlign(eui::HorizontalAlign::Center)
                            .verticalAlign(eui::VerticalAlign::Center)
                            .build();
                    })
                    .build();
            })
            .build();
    }

    void responsiveCard(eui::Ui& ui, float width) {
        const float inset = 18.0f;
        const float contentWidth = std::max(0.0f, width - inset * 2.0f);
        const float compact = contentWidth < 360.0f ? 1.0f : 0.0f;
        const float swatchWidth = compact > 0.5f ? contentWidth : std::max(0.0f, (contentWidth - 16.0f) / 3.0f);

        components::card(ui, "layout.responsive")
            .width(width)
            .height(compact > 0.5f ? 286.0f : 196.0f)
            .padding(inset)
            .theme(themeColors())
            .content([&] {
                cardTitle(ui, "layout.responsive.title", "responsive sizing", contentWidth);
                note(ui, "layout.responsive.note", "same sample switches between row and column", contentWidth, 28.0f);

                if (compact > 0.5f) {
                    ui.column("layout.responsive.column")
                        .position(0.0f, 70.0f)
                        .size(contentWidth, 174.0f)
                        .gap(8.0f)
                        .content([&] {
                            pill(ui, "layout.responsive.a", "compact", swatchWidth, 50.0f, accent());
                            pill(ui, "layout.responsive.b", "single column", swatchWidth, 50.0f, surfaceSoft());
                            pill(ui, "layout.responsive.c", "stable width", swatchWidth, 50.0f, surfaceActive());
                        })
                        .build();
                } else {
                    ui.row("layout.responsive.row")
                        .position(0.0f, 78.0f)
                        .size(contentWidth, 58.0f)
                        .gap(8.0f)
                        .content([&] {
                            pill(ui, "layout.responsive.a", "wide", swatchWidth, 50.0f, accent());
                            pill(ui, "layout.responsive.b", "three", swatchWidth, 50.0f, surfaceSoft());
                            pill(ui, "layout.responsive.c", "columns", swatchWidth, 50.0f, surfaceActive());
                        })
                        .build();
                }
            })
            .build();
    }

    void compose(eui::Ui& ui, float width, float) {
        const float gap = 18.0f;
        const bool twoColumns = width >= 820.0f;
        const float cardWidth = twoColumns ? std::max(320.0f, (width - gap) * 0.5f) : std::max(300.0f, width);
        const float responsiveHeight = cardWidth - 36.0f < 360.0f ? 286.0f : 196.0f;
        constexpr float rowColumnHeight = 178.0f;
        constexpr float stackHeight = 194.0f;
        constexpr float flexHeight = 186.0f;
        constexpr float alignHeight = 212.0f;
        constexpr float clipHeight = 196.0f;
        constexpr float fillWrapHeight = 232.0f;
        constexpr float scrollHeight = 266.0f;
        constexpr float virtualListHeight = 286.0f;
        constexpr float loaderHeight = 230.0f;
        const float leftColumnHeight = rowColumnHeight + flexHeight + fillWrapHeight + responsiveHeight + loaderHeight + gap * 4.0f;
        const float rightColumnHeight = stackHeight + alignHeight + clipHeight + scrollHeight + virtualListHeight + gap * 4.0f;
        const float twoColumnHeight = std::max(leftColumnHeight, rightColumnHeight);
        const float singleColumnHeight = rowColumnHeight + stackHeight + flexHeight + alignHeight + clipHeight +
            fillWrapHeight + responsiveHeight + scrollHeight + virtualListHeight + loaderHeight + gap * 9.0f;

        if (twoColumns) {
            ui.row("layout.grid")
                .size(width, twoColumnHeight)
                .gap(gap)
                .content([&] {
                    ui.column("layout.grid.left")
                        .size(cardWidth, leftColumnHeight)
                        .gap(gap)
                        .content([&] {
                            rowColumnCard(ui, cardWidth);
                            flexCard(ui, cardWidth);
                            fillWrapCard(ui, cardWidth);
                            responsiveCard(ui, cardWidth);
                            loaderCard(ui, cardWidth);
                        })
                        .build();

                    ui.column("layout.grid.right")
                        .size(cardWidth, rightColumnHeight)
                        .gap(gap)
                        .content([&] {
                            stackPaddingCard(ui, cardWidth);
                            alignCard(ui, cardWidth);
                            clipCard(ui, cardWidth);
                            scrollCard(ui, cardWidth);
                            virtualListCard(ui, cardWidth);
                        })
                        .build();
                })
                .build();
            return;
        }

        ui.column("layout.grid.single")
            .size(cardWidth, singleColumnHeight)
            .gap(gap)
            .content([&] {
                rowColumnCard(ui, cardWidth);
                stackPaddingCard(ui, cardWidth);
                flexCard(ui, cardWidth);
                alignCard(ui, cardWidth);
                clipCard(ui, cardWidth);
                fillWrapCard(ui, cardWidth);
                responsiveCard(ui, cardWidth);
                scrollCard(ui, cardWidth);
                virtualListCard(ui, cardWidth);
                loaderCard(ui, cardWidth);
            })
            .build();
    }
};
