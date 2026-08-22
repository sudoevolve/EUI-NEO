struct GalleryAnimationPage {
    bool moved = false;
    bool rotated = false;
    bool faded = false;
    bool scaled = false;
    bool rounded = false;
    bool glowing = false;
    bool flipX = false;
    bool flipY = false;
    bool perspective = false;

    void compose(eui::Ui& ui, float width, float height) {
    (void)height;
    const float stageWidth = std::max(280.0f, std::min(width, 860.0f));
    const float stageHeight = 252.0f;
    const float actorWidth = std::max(220.0f, std::min(320.0f, stageWidth * 0.40f));
    const float actorHeight = 152.0f;
    const float actorScale = scaled ? 1.18f : 1.0f;
    const float actorBaseX = 46.0f;
    const float actorBaseY = 50.0f;
    const float actorTravel = std::max(0.0f, stageWidth - actorWidth * actorScale - 180.0f - actorBaseX);
    const int buttonColumns = stageWidth < 360.0f ? 2 : 3;
    const float buttonGap = 18.0f;
    const float buttonWidth = std::max(92.0f, std::min(166.0f, (stageWidth - buttonGap * static_cast<float>(buttonColumns - 1)) / static_cast<float>(buttonColumns)));
    const eui::Color rotateColor{0.84f, 0.46f, 0.60f, 1.0f};
    const eui::Color fadeColor{0.50f, 0.72f, 0.34f, 1.0f};
    const eui::Color scaleColor{0.92f, 0.62f, 0.26f, 1.0f};
    const eui::Color radiusColor{0.50f, 0.58f, 0.94f, 1.0f};
    const eui::Color glowColor{0.28f, 0.76f, 0.72f, 1.0f};
    const eui::Color flipXColor{0.74f, 0.46f, 0.92f, 1.0f};
    const eui::Color flipYColor{0.34f, 0.68f, 0.94f, 1.0f};
    const eui::Color perspectiveColor{0.94f, 0.66f, 0.30f, 1.0f};
    const bool perspectiveActive = flipX || flipY || perspective;

    auto matrixButton = [&](const std::string& id, const char* label, bool active, const eui::Color& color, const std::function<void()>& onClick) {
        components::button(ui, id)
            .size(buttonWidth, 50.0f)
            .text(label)
            .colors(active ? color : surfaceSoft(),
                    buttonHover(active ? color : surfaceSoft()),
                    buttonPressed(active ? color : surfaceSoft()))
            .textColor(active || optionNight ? eui::Color{0.94f, 0.97f, 1.0f, 1.0f} : textPrimary())
            .iconColor(active || optionNight ? eui::Color{0.94f, 0.97f, 1.0f, 1.0f} : textPrimary())
            .border(1.0f, active ? withAlpha(color, 0.58f) : borderColor(0.70f))
            .shadow(12.0f, 0.0f, 4.0f, shadowColor(0.18f, 0.08f))
            .onClick(onClick)
            .transition(pageTransition())
            .build();
    };

    ui.flow("animation.controls")
        .width(stageWidth)
        .height(eui::SizeValue::wrapContent())
        .gap(buttonGap)
        .lineGap(10.0f)
        .content([&] {
            components::button(ui, "anim.move")
                .size(buttonWidth, 50.0f)
                .text("Move")
                .colors(moved ? accent() : surfaceSoft(),
                        buttonHover(moved ? accent() : surfaceSoft()),
                        buttonPressed(moved ? accent() : surfaceSoft()))
                .textColor(moved || optionNight ? eui::Color{0.94f, 0.97f, 1.0f, 1.0f} : textPrimary())
                .iconColor(moved || optionNight ? eui::Color{0.94f, 0.97f, 1.0f, 1.0f} : textPrimary())
                .border(1.0f, moved ? withAlpha(accent(), 0.58f) : borderColor(0.70f))
                .shadow(12.0f, 0.0f, 4.0f, shadowColor(0.18f, 0.08f))
                .onClick([this] { moved = !moved; })
                .transition(pageTransition())
                .build();

            components::button(ui, "anim.rotate")
                .size(buttonWidth, 50.0f)
                .text("Rotate")
                .colors(rotated ? rotateColor : surfaceSoft(),
                        buttonHover(rotated ? rotateColor : surfaceSoft()),
                        buttonPressed(rotated ? rotateColor : surfaceSoft()))
                .textColor(rotated || optionNight ? eui::Color{0.94f, 0.97f, 1.0f, 1.0f} : textPrimary())
                .iconColor(rotated || optionNight ? eui::Color{0.94f, 0.97f, 1.0f, 1.0f} : textPrimary())
                .border(1.0f, rotated ? withAlpha(rotateColor, 0.58f) : borderColor(0.70f))
                .shadow(12.0f, 0.0f, 4.0f, shadowColor(0.18f, 0.08f))
                .onClick([this] { rotated = !rotated; })
                .transition(pageTransition())
                .build();

            components::button(ui, "anim.fade")
                .size(buttonWidth, 50.0f)
                .text("Fade")
                .colors(faded ? fadeColor : surfaceSoft(),
                        buttonHover(faded ? fadeColor : surfaceSoft()),
                        buttonPressed(faded ? fadeColor : surfaceSoft()))
                .textColor(faded || optionNight ? eui::Color{0.94f, 0.97f, 1.0f, 1.0f} : textPrimary())
                .iconColor(faded || optionNight ? eui::Color{0.94f, 0.97f, 1.0f, 1.0f} : textPrimary())
                .border(1.0f, faded ? withAlpha(fadeColor, 0.58f) : borderColor(0.70f))
                .shadow(12.0f, 0.0f, 4.0f, shadowColor(0.18f, 0.08f))
                .onClick([this] { faded = !faded; })
                .transition(pageTransition())
                .build();
        });

    ui.flow("animation.controls.extra")
        .width(stageWidth)
        .height(eui::SizeValue::wrapContent())
        .gap(buttonGap)
        .lineGap(10.0f)
        .content([&] {
            components::button(ui, "anim.scale")
                .size(buttonWidth, 50.0f)
                .text("Scale")
                .colors(scaled ? scaleColor : surfaceSoft(),
                        buttonHover(scaled ? scaleColor : surfaceSoft()),
                        buttonPressed(scaled ? scaleColor : surfaceSoft()))
                .textColor(scaled || optionNight ? eui::Color{0.94f, 0.97f, 1.0f, 1.0f} : textPrimary())
                .iconColor(scaled || optionNight ? eui::Color{0.94f, 0.97f, 1.0f, 1.0f} : textPrimary())
                .border(1.0f, scaled ? withAlpha(scaleColor, 0.58f) : borderColor(0.70f))
                .shadow(12.0f, 0.0f, 4.0f, shadowColor(0.18f, 0.08f))
                .onClick([this] { scaled = !scaled; })
                .transition(pageTransition())
                .build();

            components::button(ui, "anim.radius")
                .size(buttonWidth, 50.0f)
                .text("Radius")
                .colors(rounded ? radiusColor : surfaceSoft(),
                        buttonHover(rounded ? radiusColor : surfaceSoft()),
                        buttonPressed(rounded ? radiusColor : surfaceSoft()))
                .textColor(rounded || optionNight ? eui::Color{0.94f, 0.97f, 1.0f, 1.0f} : textPrimary())
                .iconColor(rounded || optionNight ? eui::Color{0.94f, 0.97f, 1.0f, 1.0f} : textPrimary())
                .border(1.0f, rounded ? withAlpha(radiusColor, 0.58f) : borderColor(0.70f))
                .shadow(12.0f, 0.0f, 4.0f, shadowColor(0.18f, 0.08f))
                .onClick([this] { rounded = !rounded; })
                .transition(pageTransition())
                .build();

            components::button(ui, "anim.glow")
                .size(buttonWidth, 50.0f)
                .text("Glow")
                .colors(glowing ? glowColor : surfaceSoft(),
                        buttonHover(glowing ? glowColor : surfaceSoft()),
                        buttonPressed(glowing ? glowColor : surfaceSoft()))
                .textColor(glowing || optionNight ? eui::Color{0.94f, 0.97f, 1.0f, 1.0f} : textPrimary())
                .iconColor(glowing || optionNight ? eui::Color{0.94f, 0.97f, 1.0f, 1.0f} : textPrimary())
                .border(1.0f, glowing ? withAlpha(glowColor, 0.58f) : borderColor(0.70f))
                .shadow(12.0f, 0.0f, 4.0f, shadowColor(0.18f, 0.08f))
                .onClick([this] { glowing = !glowing; })
                .transition(pageTransition())
                .build();
        });

    ui.flow("animation.controls.matrix")
        .width(stageWidth)
        .height(eui::SizeValue::wrapContent())
        .gap(buttonGap)
        .lineGap(10.0f)
        .content([&] {
            matrixButton("anim.matrix.flipX", "Flip X", flipX, flipXColor, [this] {
                flipX = !flipX;
            });
            matrixButton("anim.matrix.flipY", "Flip Y", flipY, flipYColor, [this] {
                flipY = !flipY;
            });
            matrixButton("anim.matrix.depth", "Depth", perspective, perspectiveColor, [this] {
                perspective = !perspective;
            });
        });

    ui.stack("animation.stage")
        .size(stageWidth, stageHeight)
        .content([&] {
            ui.rect("animation.stage.bg")
                .size(stageWidth, stageHeight)
                .color(surface())
                .radius(24.0f)
                .border(1.0f, borderColor())
                .build();

            ui.rect("animation.stage.track")
                .x(38.0f)
                .y(stageHeight - 38.0f)
                .size(std::max(0.0f, stageWidth - 76.0f), 2.0f)
                .color(borderColor(0.48f))
                .radius(1.0f)
                .build();

            ui.stack("animation.actor.card")
                .x(actorBaseX)
                .y(actorBaseY)
                .size(actorWidth, actorHeight)
                .translate(moved ? actorTravel : 0.0f, moved ? -12.0f : 0.0f)
                .rotate(rotated ? 0.34f : 0.0f)
                .rotateX(flipX ? 0.82f : 0.0f)
                .rotateY(flipY ? -0.72f : (perspective ? 0.42f : 0.0f))
                .translateZ(perspective ? 72.0f : 0.0f)
                .perspective(480.0f)
                .scale(actorScale)
                .transformOrigin(0.5f, 0.5f)
                .transformedHitTest()
                .onClick([this] { perspective = !perspective; })
                .opacity(faded ? 0.36f : 1.0f)
                .transition(motionTransition())
                .animate(eui::AnimProperty::Opacity | eui::AnimProperty::Transform)
                .content([&] {
                    ui.rect("animation.actor.bg")
                        .size(actorWidth, actorHeight)
                        .color(moved ? accent() : radiusColor)
                        .radius(rounded ? 36.0f : 20.0f)
                        .border(1.0f, perspectiveActive ? withAlpha(flipXColor, 0.48f) : borderColor(0.70f))
                        .shadow(glowing ? 44.0f : 26.0f, 0.0f, glowing ? 0.0f : 12.0f,
                                glowing ? withAlpha(glowColor, optionNight ? 0.42f : 0.26f) : shadowColor(0.32f, 0.16f))
                        .transition(motionTransition())
                        .animate(eui::AnimProperty::Color | eui::AnimProperty::Radius | eui::AnimProperty::Shadow)
                        .build();

                    ui.rect("animation.actor.chip.primary")
                        .x(24.0f)
                        .y(24.0f)
                        .size(actorWidth * 0.36f, 34.0f)
                        .color(perspectiveActive ? flipXColor : withAlpha(textPrimary(), 0.88f))
                        .radius(12.0f)
                        .transition(motionTransition())
                        .animate(eui::AnimProperty::Color)
                        .build();

                    ui.rect("animation.actor.chip.secondary")
                        .x(actorWidth * 0.56f)
                        .y(24.0f)
                        .size(actorWidth * 0.26f, 34.0f)
                        .color(perspectiveActive ? flipYColor : withAlpha(textPrimary(), 0.72f))
                        .radius(12.0f)
                        .transition(motionTransition())
                        .animate(eui::AnimProperty::Color)
                        .build();

                    ui.image("animation.actor.icon")
                        .x(actorWidth - 84.0f)
                        .y(actorHeight - 82.0f)
                        .size(54.0f, 54.0f)
                        .source("assets/icon.png")
                        .cover()
                        .radius(13.0f)
                        .build();

                    ui.text("animation.actor.title")
                        .x(24.0f)
                        .y(76.0f)
                        .size(std::max(0.0f, actorWidth - 118.0f), 30.0f)
                        .text(flipX ? "Flip X" : (flipY ? "Flip Y" : (perspective ? "Depth" : "Motion")))
                        .fontSize(24.0f)
                        .lineHeight(29.0f)
                        .color(eui::Color{0.96f, 0.97f, 1.0f, 1.0f})
                        .transition(motionTransition())
                        .animate(eui::AnimProperty::TextColor)
                        .build();

                    ui.text("animation.actor.note")
                        .x(24.0f)
                        .y(110.0f)
                        .size(std::max(0.0f, actorWidth - 118.0f), 24.0f)
                        .text(flipX ? "rotateX + perspective" :
                              (flipY ? "rotateY + translateZ" :
                              (perspective ? "translateZ + perspective" : "one card, many transforms")))
                        .fontSize(15.0f)
                        .lineHeight(20.0f)
                        .color(withAlpha(eui::Color{0.96f, 0.97f, 1.0f, 1.0f}, 0.74f))
                        .horizontalAlign(eui::HorizontalAlign::Center)
                        .build();
                })
                .build();
        });
    }
};

