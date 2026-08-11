struct GallerySettingsPage {
    void settingRow(eui::Ui& ui, const std::string& id, const std::string& title, const std::string& note, bool enabled, float width, const std::function<void()>& onClick) {
        const float toggleX = std::max(0.0f, width - 80.0f);
        const float textWidth = std::max(0.0f, width - 132.0f);
        components::SwitchStyle switchStyle(themeColors());
        switchStyle.on = accent();
        switchStyle.knob = optionNight
            ? eui::Color{0.96f, 0.98f, 1.0f, 1.0f}
            : eui::Color{1.0f, 1.0f, 1.0f, 1.0f};

        ui.stack(id)
            .size(width, 72.0f)
            .content([&] {
                ui.rect(id + ".hit")
                    .size(width, 72.0f)
                    .states(surfaceSoft(), buttonHover(surfaceSoft()), buttonPressed(surfaceSoft()))
                    .radius(16.0f)
                    .transition(pageTransition())
                    .onClick(onClick)
                    .build();

                ui.text(id + ".title")
                    .x(24.0f)
                    .y(12.0f)
                    .size(textWidth, 28.0f)
                    .text(title)
                    .fontSize(20.0f)
                    .lineHeight(26.0f)
                    .color(textPrimary())
                    .build();

                ui.text(id + ".note")
                    .x(24.0f)
                    .y(42.0f)
                    .size(textWidth, 22.0f)
                    .text(note)
                    .fontSize(15.0f)
                    .lineHeight(20.0f)
                    .color(textMuted())
                    .build();

                ui.stack(id + ".switch.wrap")
                    .x(toggleX)
                    .y(22.0f)
                    .size(46.0f, 26.0f)
                    .content([&] {
                        components::toggleSwitch(ui, id + ".switch")
                            .size(46.0f, 26.0f)
                            .trackSize(46.0f, 26.0f)
                            .checked(enabled)
                            .style(switchStyle)
                            .transition(pageTransition())
                            .onChange([onClick](bool) {
                                if (onClick) {
                                    onClick();
                                }
                            })
                            .build();
                    })
                    .build();
            })
            .build();
    }

    void compose(eui::Ui& ui, float width, float height) {
        (void)height;
        const float rowWidth = std::max(0.0f, std::min(width, 720.0f));
        const float sliderWidth = std::max(0.0f, rowWidth - 48.0f);

        ui.column("settings.list")
            .width(rowWidth)
            .height(eui::SizeValue::wrapContent())
            .gap(14.0f)
            .content([&] {
                settingRow(ui, "setting.glass", "Glass surfaces", "Show transparent panel examples in controls.", optionGlass, rowWidth, [] { optionGlass = !optionGlass; });
                settingRow(ui, "setting.motion", "Animated transitions", "Keep page and property transitions enabled.", optionMotion, rowWidth, [] { optionMotion = !optionMotion; });

                ui.stack("setting.animationSpeed")
                    .size(rowWidth, 86.0f)
                    .content([&] {
                        ui.rect("setting.animationSpeed.bg")
                            .size(rowWidth, 86.0f)
                            .color(surfaceSoft())
                            .radius(16.0f)
                            .transition(pageTransition())
                            .animate(eui::AnimProperty::Color)
                            .build();

                        ui.text("setting.animationSpeed.title")
                            .x(24.0f)
                            .y(12.0f)
                            .size(std::max(0.0f, rowWidth - 132.0f), 28.0f)
                            .text("Animation speed")
                            .fontSize(20.0f)
                            .lineHeight(26.0f)
                            .color(textPrimary())
                            .build();

                        ui.text("setting.animationSpeed.value")
                            .x(std::max(0.0f, rowWidth - 96.0f))
                            .y(12.0f)
                            .size(72.0f, 28.0f)
                            .text(animationSpeedText())
                            .fontSize(18.0f)
                            .lineHeight(24.0f)
                            .fontWeight(760)
                            .color(accent())
                            .horizontalAlign(eui::HorizontalAlign::Right)
                            .verticalAlign(eui::VerticalAlign::Center)
                            .transition(textTransition())
                            .build();

                        ui.stack("setting.animationSpeed.slider.wrap")
                            .x(24.0f)
                            .y(48.0f)
                            .size(sliderWidth, 28.0f)
                            .content([&] {
                                components::slider(ui, "setting.animationSpeed.slider")
                                    .theme(themeColors())
                                    .size(sliderWidth, 28.0f)
                                    .value(animationSpeedSliderValue())
                                    .transition(pageTransition())
                                    .onChange([](float value) {
                                        optionAnimationSpeed = animationSpeedFromSlider(value);
                                    })
                                    .build();
                            })
                            .build();
                    })
                    .build();

                settingRow(ui, "setting.unlockFps", "Unlock 90 FPS limit", "Let animation rendering use the display refresh rate.", optionUnlockFps, rowWidth, [] { optionUnlockFps = !optionUnlockFps; });
                settingRow(ui, "setting.night", "Night mode", "Switch gallery between light and dark theme tokens.", optionNight, rowWidth, [] { optionNight = !optionNight; });
                settingRow(ui, "setting.shadertoy", "Shadertoy demo", "Show the demo.frag shader below the settings.", optionShaderToy, rowWidth, [] { optionShaderToy = !optionShaderToy; });

                if (optionShaderToy) {
                    const float demoHeight = std::max(180.0f, rowWidth * 9.0f / 16.0f);
                    ui.stack("setting.shadertoy.demo")
                        .size(rowWidth, demoHeight)
                        .content([&] {
                            ui.shadertoy("setting.shadertoy.demo.canvas")
                                .size(rowWidth, demoHeight)
                                .graph(galleryShaderToyGraph())
                                .radius(16.0f)
                                .opacity(1.0f)
                                .resolutionScale(1.0f)
                                .timeScale(1.0f)
                                .paused(false)
                                .build();
                        })
                        .build();
                }
            })
            .build();
    }
};
