struct GalleryControlsPage {
    bool checked = true;
    eui::Signal<bool> switchOn{true};
    bool radioA = true;
    std::string input = "Hello EUI-NEO 😉";
    std::string editor = "Hello EUI-NEO 😉\nType multiple lines here.";
    eui::Signal<float> slider{0.44f};
    int segment = 1;
    int tab = 0;
    int stepperDec = 42;
    int stepperHex = 0x2A;
    int stepperBin = 0x15;
    int dropdown = 1;
    eui::Signal<bool> dropdownOpen{false};
    int year = 2026;
    int month = 4;
    int day = 28;
    int hour = 9;
    int minute = 30;
    bool dateOpen = false;
    bool timeOpen = false;
    bool colorOpen = false;
    eui::Signal<bool> dialogOpen{false};
    bool toastVisible = false;
    bool contextMenuOpen = false;
    float contextMenuX = 0.0f;
    float contextMenuY = 0.0f;
    std::string feedback = "Ready";

    std::string dateText() const {
        char buffer[16] = {};
        std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", year, month, day);
        return std::string(buffer);
    }

    std::string timeText() const {
        char buffer[8] = {};
        std::snprintf(buffer, sizeof(buffer), "%02d:%02d", hour, minute);
        return std::string(buffer);
    }


    void propertyCard(eui::Ui& ui, const std::string& id, const std::string& title, const std::string& note,
                      const eui::Color& color, const std::string& kind, float width) {
        ui.stack(id)
            .size(width, 144.0f)
            .visualStateFrom(id + ".bg", 0.95f)
            .content([&] {
                if (kind == "blur") {
                    ui.rect(id + ".circle.primary")
                        .x(width * 0.14f)
                        .y(20.0f)
                        .size(64.0f, 64.0f)
                        .color(withAlpha(accent(), 0.95f))
                        .radius(32.0f)
                        .build();
    
                    ui.rect(id + ".circle.warm")
                        .x(width * 0.48f)
                        .y(62.0f)
                        .size(58.0f, 58.0f)
                        .color({1.0f, 0.54f, 0.18f, 0.92f})
                        .radius(29.0f)
                        .build();
    
                    ui.rect(id + ".circle.cool")
                        .x(width * 0.66f)
                        .y(16.0f)
                        .size(46.0f, 46.0f)
                        .color({0.16f, 0.82f, 0.72f, 0.90f})
                        .radius(23.0f)
                        .build();
                }
    
                auto rect = ui.rect(id + ".bg")
                    .size(width, 144.0f)
                    .states(color, buttonHover(color), buttonPressed(color))
                    .radius(18.0f)
                    .transition(pageTransition());
    
                if (kind == "border") {
                    rect.border(3.0f, accent());
                } else if (kind == "shadow") {
                    rect.shadow(28.0f, 0.0f, 12.0f, shadowColor(0.34f, 0.18f));
                } else if (kind == "blur") {
                    rect.opacity(optionGlass ? 1.0f : 0.82f)
                        .blur(optionGlass ? 18.0f : 0.0f)
                        .border(1.0f, optionGlass ? withAlpha(textPrimary(), 0.35f) : borderColor(0.70f));
                } else if (kind == "rotate") {
                    rect.rotate(0.08f).transformOrigin(0.5f, 0.5f);
                }
                rect.build();
    
                ui.text(id + ".title")
                    .size(width, 32.0f)
                    .margin(0.0f, 36.0f, 0.0f, 0.0f)
                    .text(title)
                    .fontSize(22.0f)
                    .lineHeight(28.0f)
                    .color(textPrimary())
                    .horizontalAlign(eui::HorizontalAlign::Center)
                    .build();
    
                caption(ui, id + ".note", note, width, 90.0f);
            })
            .build();
    }
    void compose(eui::Ui& ui, float width, float height) {
    const float cardGap = 18.0f;
    const float cardWidth = std::max(72.0f, std::min(204.0f, (width - cardGap * 2.0f) / 3.0f));
    const float rowHeight = 144.0f;
    const float buttonWidth = std::max(72.0f, std::min(178.0f, (width - 36.0f) / 3.0f));
    const float fieldWidth = std::max(0.0f, std::min(width, 680.0f));
    const float componentCardWidth = std::max(120.0f, std::min(340.0f, (width - 20.0f) * 0.5f));
    const float feedbackWidth = std::max(112.0f, std::min(176.0f, (fieldWidth - 54.0f) / 4.0f));
    const float dataRowGap = 20.0f;
    const float dropdownWidth = std::max(180.0f, std::min(260.0f, fieldWidth * 0.36f));
    const float tableWidth = std::max(260.0f, fieldWidth - dropdownWidth - dataRowGap);
    const float dataRowHeight = 200.0f;
    const float pickerGap = 18.0f;
    const float chartGap = 18.0f;
    const float chartWidth = std::max(150.0f, std::min(206.0f, (fieldWidth - chartGap * 2.0f) / 3.0f));
    const float chartHeight = 236.0f;
    const float stepperGap = 18.0f;
    const float stepperWidth = std::max(132.0f, std::min(214.0f, (fieldWidth - stepperGap * 2.0f) / 3.0f));
    const float stepperRowWidth = stepperWidth * 3.0f + stepperGap * 2.0f;
    const float pickerWidth = std::max(154.0f, std::min(210.0f, (fieldWidth - pickerGap * 2.0f) / 3.0f));
    const float choiceWidth = std::max(180.0f, (fieldWidth - 18.0f) * 0.5f);
    const float choiceRowWidth = choiceWidth * 2.0f + 18.0f;

    ui.text("controls.components.title")
        .size(width, 30.0f)
        .text("Basic Components")
        .fontSize(26.0f)
        .lineHeight(30.0f)
        .color(textPrimary())
        .build();

    ui.row("controls.buttons")
        .size(fieldWidth, 54.0f)
        .gap(18.0f)
        .content([&] {
            components::button(ui, "control.primary")
                .size(buttonWidth, 54.0f)
                .icon(0xF00C)
                .text("Filled")
                .colors(themeColors().primary, buttonHover(themeColors().primary), buttonPressed(themeColors().primary))
                .border(1.0f, withAlpha(themeColors().primary, 0.58f))
                .shadow(14.0f, 0.0f, 5.0f, shadowColor(0.22f, 0.10f))
                .transition(pageTransition())
                .build();

            components::button(ui, "control.soft")
                .size(buttonWidth, 54.0f)
                .icon(0xF0C8)
                .text("Outline")
                .colors(kTransparent, withAlpha(themeColors().primary, 0.10f), withAlpha(themeColors().primary, 0.18f))
                .textColor(themeColors().primary)
                .iconColor(themeColors().primary)
                .border(1.0f, withAlpha(themeColors().primary, 0.78f))
                .shadow(0.0f, 0.0f, 0.0f, shadowColor(0.0f, 0.0f))
                .transition(pageTransition())
                .build();

            components::button(ui, "control.warn")
                .size(buttonWidth, 54.0f)
                .icon(0xF1FC)
                .text("Ghost")
                .colors(kTransparent, withAlpha(themeColors().primary, 0.08f), withAlpha(themeColors().primary, 0.14f))
                .textColor(themeColors().primary)
                .iconColor(themeColors().primary)
                .border(0.0f, kTransparent)
                .shadow(0.0f, 0.0f, 0.0f, shadowColor(0.0f, 0.0f))
                .transition(pageTransition())
                .build();
        })
        .build();

    components::input(ui, "control.input")
        .theme(themeColors())
        .size(fieldWidth, 44.0f)
        .value(input)
        .placeholder("Type here")
        .onChange([this](const std::string& value) {
            input = value;
        })
        .build();

    components::input(ui, "control.editor")
        .theme(themeColors())
        .size(fieldWidth, 120.0f)
        .value(editor)
        .placeholder("Write notes...")
        .multiline(true)
        .onChange([this](const std::string& value) {
            editor = value;
        })
        .build();

    ui.row("controls.toggles")
        .size(fieldWidth, 92.0f)
        .gap(20.0f)
        .content([&] {
            ui.column("controls.checks")
                .size(componentCardWidth, 92.0f)
                .gap(12.0f)
                .content([&] {
                    components::checkbox(ui, "control.checkbox")
                        .theme(themeColors())
                        .size(componentCardWidth, 30.0f)
                        .checked(checked)
                        .text("Checkbox")
                        .onChange([this](bool value) { checked = value; })
                        .build();

                    components::toggleSwitch(ui, "control.switch")
                        .theme(themeColors())
                        .size(componentCardWidth, 32.0f)
                        .bind(switchOn)
                        .text("Switch")
                        .build();
                })
                .build();

            ui.column("controls.radios")
                .size(componentCardWidth, 92.0f)
                .gap(12.0f)
                .content([&] {
                    components::radio(ui, "control.radio.a")
                        .theme(themeColors())
                        .size(componentCardWidth, 30.0f)
                        .selected(radioA)
                        .text("Radio A")
                        .onChange([this](bool selected) { if (selected) { radioA = true; } })
                        .build();

                    components::radio(ui, "control.radio.b")
                        .theme(themeColors())
                        .size(componentCardWidth, 30.0f)
                        .selected(!radioA)
                        .text("Radio B")
                        .onChange([this](bool selected) { if (selected) { radioA = false; } })
                        .build();
                })
                .build();
        })
        .build();

    components::progress(ui, "control.progress")
        .theme(themeColors())
        .size(fieldWidth, 14.0f)
        .value(slider.get())
        .transition(eui::Transition::none())
        .build();

    components::slider(ui, "control.slider")
        .theme(themeColors())
        .size(fieldWidth, 32.0f)
        .bind(slider)
        .transition(pageTransition())
        .build();

    ui.column("controls.choice")
        .width(fieldWidth)
        .height(eui::SizeValue::wrapContent())
        .gap(18.0f)
        .alignItems(eui::Align::CENTER)
        .content([&] {
            ui.row("controls.choice.row")
                .size(choiceRowWidth, 42.0f)
                .gap(18.0f)
                .content([&] {
                    components::segmented(ui, "control.segmented")
                        .theme(themeColors())
                        .size(choiceWidth, 38.0f)
                        .items({"Small", "Medium", "Large"})
                        .selected(segment)
                        .transition(pageTransition())
                        .onChange([this](int index) {
                            segment = index;
                        })
                        .build();

                    components::tabs(ui, "control.tabs")
                        .theme(themeColors())
                        .size(choiceWidth, 42.0f)
                        .items({"Overview", "Details", "Logs"})
                        .selected(tab)
                        .transition(pageTransition())
                        .onChange([this](int index) {
                            tab = index;
                        })
                        .build();
                })
                .build();

            ui.row("controls.stepper.row")
                .size(stepperRowWidth, 84.0f)
                .gap(stepperGap)
                .content([&] {
                    ui.column("controls.stepper.dec")
                        .size(stepperWidth, 84.0f)
                        .gap(8.0f)
                        .justifyContent(eui::Align::CENTER)
                        .alignItems(eui::Align::CENTER)
                        .content([&] {
                            ui.text("controls.stepper.dec.label")
                                .width(stepperWidth)
                                .height(18.0f)
                                .text("DEC 4-digit")
                                .fontSize(13.0f)
                                .lineHeight(16.0f)
                                .color(textMuted())
                                .horizontalAlign(eui::HorizontalAlign::Center)
                                .verticalAlign(eui::VerticalAlign::Center)
                                .build();

                            components::stepper(ui, "control.stepper.dec")
                                .theme(themeColors())
                                .size(stepperWidth, 40.0f)
                                .value(stepperDec)
                                .step(5)
                                .min(0)
                                .max(9999)
                                .base(10)
                                .digits(4)
                                .showBasePrefix(false)
                                .transition(pageTransition())
                                .onChange([this](long long value) {
                                    stepperDec = static_cast<int>(value);
                                    feedback = "Decimal stepper changed";
                                })
                                .build();
                        })
                        .build();

                    ui.column("controls.stepper.hex")
                        .size(stepperWidth, 84.0f)
                        .gap(8.0f)
                        .justifyContent(eui::Align::CENTER)
                        .alignItems(eui::Align::CENTER)
                        .content([&] {
                            ui.text("controls.stepper.hex.label")
                                .width(stepperWidth)
                                .height(18.0f)
                                .text("HEX 16-bit")
                                .fontSize(13.0f)
                                .lineHeight(16.0f)
                                .color(textMuted())
                                .horizontalAlign(eui::HorizontalAlign::Center)
                                .verticalAlign(eui::VerticalAlign::Center)
                                .build();

                            components::stepper(ui, "control.stepper.hex")
                                .theme(themeColors())
                                .size(stepperWidth, 40.0f)
                                .value(stepperHex)
                                .step(1)
                                .base(16)
                                .bitWidth(16)
                                .transition(pageTransition())
                                .onChange([this](long long value) {
                                    stepperHex = static_cast<int>(value);
                                    feedback = "Hex stepper changed";
                                })
                                .build();
                        })
                        .build();

                    ui.column("controls.stepper.bin")
                        .size(stepperWidth, 84.0f)
                        .gap(8.0f)
                        .justifyContent(eui::Align::CENTER)
                        .alignItems(eui::Align::CENTER)
                        .content([&] {
                            ui.text("controls.stepper.bin.label")
                                .width(stepperWidth)
                                .height(18.0f)
                                .text("BIN 8-bit")
                                .fontSize(13.0f)
                                .lineHeight(16.0f)
                                .color(textMuted())
                                .horizontalAlign(eui::HorizontalAlign::Center)
                                .verticalAlign(eui::VerticalAlign::Center)
                                .build();

                            components::stepper(ui, "control.stepper.bin")
                                .theme(themeColors())
                                .size(stepperWidth, 40.0f)
                                .value(stepperBin)
                                .step(1)
                                .base(2)
                                .bitWidth(8)
                                .transition(pageTransition())
                                .onChange([this](long long value) {
                                    stepperBin = static_cast<int>(value);
                                    feedback = "Binary stepper changed";
                                })
                                .build();
                        })
                        .build();
                })
                .build();
        })
        .build();

    ui.text("controls.feedback.title")
        .size(width, 30.0f)
        .text("Feedback Components")
        .fontSize(25.0f)
        .lineHeight(30.0f)
        .color(textPrimary())
        .build();

    ui.row("controls.feedback")
        .size(fieldWidth, 54.0f)
        .gap(18.0f)
        .content([&] {
            components::button(ui, "control.dialog")
                .theme(themeColors(), false)
                .size(feedbackWidth, 54.0f)
                .icon(0xF2D0)
                .text("Dialog")
                .textColor(textPrimary())
                .iconColor(accent())
                .radius(12.0f)
                .border(1.0f, borderColor(0.70f))
                .shadow(10.0f, 0.0f, 3.0f, shadowColor(0.16f, 0.08f))
                .transition(pageTransition())
                .onClick([this] {
                    dialogOpen = true;
                    feedback = "Dialog opened";
                })
                .build();

            components::button(ui, "control.toast")
                .theme(themeColors(), false)
                .size(feedbackWidth, 54.0f)
                .icon(0xF0F3)
                .text("Toast")
                .textColor(textPrimary())
                .iconColor(accent())
                .radius(12.0f)
                .border(1.0f, borderColor(0.70f))
                .shadow(10.0f, 0.0f, 3.0f, shadowColor(0.16f, 0.08f))
                .transition(pageTransition())
                .onClick([this] {
                    toastVisible = true;
                    feedback = "Toast queued";
                })
                .build();

            components::button(ui, "control.context")
                .theme(themeColors(), false)
                .size(feedbackWidth, 54.0f)
                .icon(0xF0C9)
                .text("Right Click")
                .textColor(textPrimary())
                .iconColor(accent())
                .radius(12.0f)
                .border(1.0f, borderColor(0.70f))
                .shadow(10.0f, 0.0f, 3.0f, shadowColor(0.16f, 0.08f))
                .transition(pageTransition())
                .onContextMenu([this](const eui::PointerEvent& event, const eui::Rect&) {
                    contextMenuOpen = true;
                    contextMenuX = static_cast<float>(event.x);
                    contextMenuY = static_cast<float>(event.y);
                    feedback = "Context menu opened";
                })
                .build();

            components::button(ui, "control.window")
                .theme(themeColors(), false)
                .size(feedbackWidth, 54.0f)
                .icon(0xF24D)
                .text("Window")
                .textColor(textPrimary())
                .iconColor(accent())
                .radius(12.0f)
                .border(1.0f, borderColor(0.70f))
                .shadow(10.0f, 0.0f, 3.0f, shadowColor(0.16f, 0.08f))
                .transition(pageTransition())
                .onClick([this] {
                    app::openWindow(
                        app::DslWindowConfig{}
                            .title("Inspector")
                            .pageId("inspector")
                            .windowSize(640, 420)
                            .modal(true)
                            .clearColor(appBg()),
                        [](eui::Ui& ui, const eui::Screen& screen) {
                            ui.stack("inspector.root")
                                .size(screen.width, screen.height)
                                .content([&] {
                                    ui.rect("inspector.bg")
                                        .size(screen.width, screen.height)
                                        .color(appBg())
                                        .build();

                                    ui.text("inspector.title")
                                        .x(28.0f)
                                        .y(24.0f)
                                        .size(std::max(0.0f, screen.width - 56.0f), 34.0f)
                                        .text("Inspector Window")
                                        .fontSize(28.0f)
                                        .lineHeight(34.0f)
                                        .color(textPrimary())
                                        .build();

                                    ui.text("inspector.note")
                                        .x(28.0f)
                                        .y(70.0f)
                                        .size(std::max(0.0f, screen.width - 56.0f), 24.0f)
                                        .text("This window was opened from a button callback.")
                                        .fontSize(16.0f)
                                        .lineHeight(22.0f)
                                        .color(textMuted())
                                        .build();
                                })
                                .build();
                        });
                    feedback = "Window opened";
                })
                .build();
        })
        .build();

    ui.text("controls.feedback.state")
        .size(width, 22.0f)
        .text(feedback)
        .fontSize(15.0f)
        .lineHeight(20.0f)
        .color(textMuted())
        .build();

    ui.text("controls.data.title")
        .size(width, 30.0f)
        .text("Selection & Data")
        .fontSize(25.0f)
        .lineHeight(30.0f)
        .color(textPrimary())
        .build();

    ui.row("controls.pickers.row")
        .size(fieldWidth, 44.0f)
        .gap(pickerGap)
        .content([&] {
            components::button(ui, "control.datepicker.open")
                .theme(themeColors(), false)
                .size(pickerWidth, 44.0f)
                .icon(0xF073)
                .text(dateText())
                .textColor(textPrimary())
                .iconColor(accent())
                .radius(12.0f)
                .border(1.0f, borderColor(0.70f))
                .shadow(10.0f, 0.0f, 3.0f, shadowColor(0.16f, 0.08f))
                .transition(pageTransition())
                .onClick([this] {
                    dateOpen = true;
                    timeOpen = false;
                    colorOpen = false;
                    dropdownOpen = false;
                    feedback = "Date picker opened";
                })
                .build();

            components::button(ui, "control.timepicker.open")
                .theme(themeColors(), false)
                .size(pickerWidth, 44.0f)
                .icon(0xF017)
                .text(timeText())
                .textColor(textPrimary())
                .iconColor(accent())
                .radius(12.0f)
                .border(1.0f, borderColor(0.70f))
                .shadow(10.0f, 0.0f, 3.0f, shadowColor(0.16f, 0.08f))
                .transition(pageTransition())
                .onClick([this] {
                    timeOpen = true;
                    dateOpen = false;
                    colorOpen = false;
                    dropdownOpen = false;
                    feedback = "Time picker opened";
                })
                .build();

            components::button(ui, "control.colorpicker.open")
                .theme(themeColors(), false)
                .size(pickerWidth, 44.0f)
                .icon(0xF53F)
                .text(colorHex(sampleColor))
                .textColor(textPrimary())
                .iconColor(sampleColor)
                .radius(12.0f)
                .border(1.0f, borderColor(0.70f))
                .shadow(10.0f, 0.0f, 3.0f, shadowColor(0.16f, 0.08f))
                .transition(pageTransition())
                .onClick([this] {
                    colorOpen = true;
                    dateOpen = false;
                    timeOpen = false;
                    dropdownOpen = false;
                    feedback = "Color picker opened";
                })
                .build();
        })
        .build();

    ui.row("controls.data.row")
        .size(fieldWidth, dataRowHeight)
        .gap(dataRowGap)
        .content([&] {
            components::dropdown(ui, "control.dropdown")
                .theme(themeColors())
                .size(dropdownWidth, 44.0f)
                .items({"Draft", "Review", "Published", "Archived"})
                .selected(dropdown)
                .open(dropdownOpen.get())
                .transition(pageTransition())
                .onOpenChange([this](bool open) {
                    dropdownOpen.set(open);
                    if (open) {
                        dateOpen = false;
                        timeOpen = false;
                        colorOpen = false;
                    }
                })
                .onChange([this](int index) {
                    dropdown = index;
                    feedback = "Dropdown changed";
                })
                .build();

            components::dataTable(ui, "control.table")
                .theme(themeColors())
                .size(tableWidth, 174.0f)
                .columns({"Name", "Status", "Owner"})
                .rows({
                    {"EUI Core", "Active", "Sudo"},
                    {"Gallery", "Review", "Design"},
                    {"Docs", "Draft", "DevRel"},
                    {"Runtime", "Stable", "Engine"}
                })
                .transition(pageTransition())
                .build();
        })
        .build();

    ui.text("controls.charts.title")
        .size(width, 30.0f)
        .text("Charts")
        .fontSize(25.0f)
        .lineHeight(30.0f)
        .color(textPrimary())
        .build();

    ui.row("controls.charts.row")
        .size(fieldWidth, chartHeight)
        .gap(chartGap)
        .content([&] {
            components::lineChart(ui, "control.chart.line")
                .theme(themeColors())
                .size(chartWidth, chartHeight)
                .title("LineChart")
                .values({0.22f, 0.30f, 0.20f, 0.55f, 0.42f, 0.86f})
                .labels({"Jan", "Feb", "Mar", "Apr", "May", "Jun"})
                .transition(pageTransition())
                .build();

            components::barChart(ui, "control.chart.bar")
                .theme(themeColors())
                .size(chartWidth, chartHeight)
                .title("BarChart")
                .values({0.92f, 0.36f, 0.68f, 0.52f})
                .labels({"D1", "D2", "D3", "D4"})
                .transition(pageTransition())
                .build();

            components::pieChart(ui, "control.chart.pie")
                .theme(themeColors())
                .size(chartWidth, chartHeight)
                .title("PieChart")
                .values({0.42f, 0.24f, 0.18f, 0.16f})
                .labels({"Blue", "Green", "Orange", "Pink"})
                .transition(pageTransition())
                .build();
        })
        .build();

    ui.text("controls.primitives.title")
        .size(width, 30.0f)
        .text("Primitive Properties")
        .fontSize(25.0f)
        .lineHeight(30.0f)
        .color(textPrimary())
        .build();

    ui.row("properties.a")
        .size(fieldWidth, rowHeight)
        .gap(cardGap)
        .content([&] {
            propertyCard(ui, "prop.color", "Color", "hover + press", {0.22f, 0.48f, 0.82f, 1.0f}, "color", cardWidth);
            propertyCard(ui, "prop.border", "Border", "animated edge", surface(), "border", cardWidth);
            propertyCard(ui, "prop.shadow", "Shadow", "elevation", surfaceSoft(), "shadow", cardWidth);
        })
        .build();

    ui.row("properties.b")
        .size(fieldWidth, rowHeight)
        .gap(cardGap)
        .content([&] {
            propertyCard(ui, "prop.alpha", "Opacity", "transparent fill", {0.86f, 0.38f, 0.52f, 0.58f}, "color", cardWidth);
            propertyCard(ui, "prop.blur", "Blur", "glass card", {0.78f, 0.92f, 1.0f, 0.22f}, "blur", cardWidth);
            propertyCard(ui, "prop.rotate", "Rotate", "transform", {0.48f, 0.64f, 0.36f, 1.0f}, "rotate", cardWidth);
        })
        .build();

    }
    void composeOverlays(eui::Ui& ui, const eui::Screen& screen) {
    components::dialog(ui, "feedback.dialog")
        .theme(themeColors())
        .screen(screen.width, screen.height)
        .size(430.0f, 228.0f)
        .open(dialogOpen.get())
        .title("Dialog Component")
        .message("A modal surface for focused confirmation. It uses the same theme tokens, buttons and dirty-region rendering path as the rest of the gallery.")
        .primaryText("Confirm")
        .secondaryText("Cancel")
        .onPrimary([this] {
            dialogOpen = false;
            toastVisible = true;
            feedback = "Dialog confirmed";
        })
        .onSecondary([this] {
            dialogOpen = false;
            feedback = "Dialog cancelled";
        })
        .onOpenChange([this](bool open) {
            dialogOpen = open;
            if (!open) {
                feedback = "Dialog closed";
            }
        })
        .build();

    components::contextMenu(ui, "feedback.context")
        .theme(themeColors())
        .screen(screen.width, screen.height)
        .position(contextMenuX, contextMenuY)
        .items({"Inspect", "Duplicate", "Copy Token", "Dismiss"})
        .open(contextMenuOpen)
        .onSelect([this](int index) {
            contextMenuOpen = false;
            toastVisible = true;
            if (index == 0) {
                feedback = "Inspect selected";
            } else if (index == 1) {
                feedback = "Duplicate selected";
            } else if (index == 2) {
                feedback = "Copy Token selected";
            } else {
                feedback = "Context menu dismissed";
                toastVisible = false;
            }
        })
        .onOpenChange([this](bool open) {
            contextMenuOpen = open;
            if (!open) {
                feedback = "Context menu dismissed";
            }
        })
        .build();

    components::datePicker(ui, "feedback.datepicker")
        .theme(themeColors())
        .screen(screen.width, screen.height)
        .size(420.0f, 270.0f)
        .date(year, month, day)
        .open(dateOpen)
        .transition(pageTransition())
        .zIndex(1200)
        .onOpenChange([this](bool open) {
            dateOpen = open;
        })
        .onChange([this](int nextYear, int nextMonth, int nextDay) {
            year = nextYear;
            month = nextMonth;
            day = nextDay;
            feedback = "Date changed";
        })
        .build();

    components::timePicker(ui, "feedback.timepicker")
        .theme(themeColors())
        .screen(screen.width, screen.height)
        .size(330.0f, 264.0f)
        .time(hour, minute)
        .minuteStep(5)
        .open(timeOpen)
        .transition(pageTransition())
        .zIndex(1200)
        .onOpenChange([this](bool open) {
            timeOpen = open;
        })
        .onChange([this](int nextHour, int nextMinute) {
            hour = nextHour;
            minute = nextMinute;
            feedback = "Time changed";
        })
        .build();

    components::colorPicker(ui, "feedback.colorpicker")
        .theme(themeColors())
        .screen(screen.width, screen.height)
        .size(420.0f, 320.0f)
        .value(sampleColor)
        .open(colorOpen)
        .transition(pageTransition())
        .zIndex(1200)
        .onOpenChange([this](bool open) {
            colorOpen = open;
        })
        .onChange([this](eui::Color color) {
            sampleColor = color;
            feedback = "Color changed";
        })
        .build();

    components::toast(ui, "feedback.toast")
        .theme(themeColors())
        .screen(screen.width, screen.height)
        .visible(toastVisible)
        .duration(3.0f)
        .title("Gallery Feedback")
        .message(feedback)
        .onAutoDismiss([this] {
            toastVisible = false;
            feedback = "Ready";
        })
        .onDismiss([this] {
            toastVisible = false;
            feedback = "Toast dismissed";
        })
        .build();
    }

};

