#include "eui_neo.h"

#include <algorithm>
#include <cstdio>
#include <string>

namespace app {

const DslAppConfig& dslAppConfig() {
    static const DslAppConfig config = DslAppConfig{}
        .title("EUI State Starter")
        .pageId("sidebar_starter")
        .clearColor({0.06f, 0.07f, 0.09f, 1.0f})
        .windowSize(1320, 900)
        .fps(90.0);
    return config;
}

namespace {

enum class Page { Overview, Settings };

// One enum replaces several booleans when only one popup may be open.
enum class Popup { None, Theme, Date };

struct AppState {
    // Plain fields are page-owned business state.
    Page page = Page::Overview;
    Popup popup = Popup::None;
    int year = 2026;
    int month = 7;
    int day = 12;
    int themeChoice = 0;
    std::string lastAction = "All changes saved";

    // Signals are values that controls bind and update directly.
    eui::Signal<std::string> displayName{"Sudo"};
    eui::Signal<bool> notifications{true};
    eui::Signal<float> focusVolume{0.68f};
    eui::Signal<float> overviewScroll{0.0f};
    eui::Signal<float> settingsScroll{0.0f};
};

struct ActivityState {
    int refreshCount = 0;
    bool detailsVisible = false;
};

AppState state;

constexpr float kWideNav = 250.0f;
constexpr float kCompactNav = 92.0f;
constexpr float kPagePad = 34.0f;

components::theme::ThemeColorTokens theme();
eui::Transition motion();
eui::Color textPrimary();
eui::Color textMuted();
eui::Color borderColor(float alpha = 1.0f);
void composeNavigation(eui::Ui& ui, float width, float height, bool compact);
void composePageHost(eui::Ui& ui, float width, float height);
void composeOverview(eui::Ui& ui, float width);
void composeSettings(eui::Ui& ui, float width);
void composeOverlays(eui::Ui& ui, const eui::Screen& screen);

void title(eui::Ui& ui, const std::string& heading, const std::string& subtitle, float width) {
    ui.text("page.title")
        .position(kPagePad, 36.0f)
        .size(width - kPagePad * 2.0f, 42.0f)
        .text(heading)
        .fontSize(34.0f)
        .lineHeight(40.0f)
        .fontWeight(820)
        .color(textPrimary())
        .transition(motion())
        .build();

    ui.text("page.subtitle")
        .position(kPagePad, 88.0f)
        .size(width - kPagePad * 2.0f, 28.0f)
        .text(subtitle)
        .fontSize(16.0f)
        .lineHeight(22.0f)
        .color(textMuted())
        .transition(motion())
        .build();
}

void panel(eui::Ui& ui, const std::string& id, float x, float y, float width, float height) {
    components::panel(ui, id, theme())
        .position(x, y)
        .size(width, height)
        .radius(12.0f)
        .border(1.0f, borderColor(0.72f))
        .transition(motion())
        .animate(eui::AnimProperty::Color | eui::AnimProperty::Border)
        .build();
}

void panelHeading(eui::Ui& ui, const std::string& id, const std::string& value,
                  float x, float y, float width) {
    ui.text(id)
        .position(x, y)
        .size(width, 28.0f)
        .text(value)
        .fontSize(20.0f)
        .lineHeight(26.0f)
        .fontWeight(760)
        .color(textPrimary())
        .build();
}

std::string dateText() {
    char buffer[20] = {};
    std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", state.year, state.month, state.day);
    return buffer;
}

} // namespace

void compose(eui::Ui& ui, const eui::Screen& screen) {
    const bool compact = screen.width < 820.0f;
    const float navWidth = compact ? kCompactNav : kWideNav;
    const float contentWidth = std::max(0.0f, screen.width - navWidth);

    ui.row("root")
        .size(screen.width, screen.height)
        .content([&] {
            composeNavigation(ui, navWidth, screen.height, compact);
            composePageHost(ui, contentWidth, screen.height);
        })
        .build();

    composeOverlays(ui, screen);
}

namespace {

void composeNavigation(eui::Ui& ui, float width, float height, bool compact) {
    const bool light = state.themeChoice == 1;
    components::navbar(ui, "navigation")
        .theme(theme())
        .size(width, height)
        .compact(compact)
        .brand("State Starter", 0xF5FD)
        .subtitle("Clear ownership")
        .selected(state.page == Page::Overview ? 0 : 1)
        .items({
            {"overview", "Overview", 0xF201, 0},
            {"settings", "Settings", 0xF013, 1},
        })
        .footer(light ? "Dark theme" : "Light theme", light ? 0xF186 : 0xF185, [] {
            state.themeChoice = state.themeChoice == 1 ? 0 : 1;
            state.popup = Popup::None;
        })
        .transition(motion())
        .onChange([](int page) {
            state.page = page == 0 ? Page::Overview : Page::Settings;
            state.popup = Popup::None;
        })
        .build();
}

void composePageHost(eui::Ui& ui, float width, float height) {
    ui.stack("page.host")
        .size(width, height)
        .content([&] {
            ui.rect("page.background")
                .size(width, height)
                .color(theme().background)
                .transition(motion())
                .animate(eui::AnimProperty::Color)
                .build();

            // keepAlive preserves page-local ui.state values while the page is hidden.
            ui.loader("overview.page")
                .active(state.page == Page::Overview)
                .keepAlive()
                .content([&] {
                    components::scrollView(ui, "overview.scroll")
                        .theme(theme())
                        .size(width, height)
                        .bind(state.overviewScroll)
                        .step(52.0f)
                        .scrollbarWidth(10.0f)
                        .scrollbarGap(10.0f)
                        .content([&](eui::Ui& contentUi, float contentWidth, float) {
                            contentUi.stack("overview.canvas")
                                .size(contentWidth, 610.0f)
                                .content([&] { composeOverview(contentUi, contentWidth); })
                                .build();
                        })
                        .build();
                })
                .build();

            ui.loader("settings.page")
                .active(state.page == Page::Settings)
                .keepAlive()
                .content([&] {
                    components::scrollView(ui, "settings.scroll")
                        .theme(theme())
                        .size(width, height)
                        .bind(state.settingsScroll)
                        .step(52.0f)
                        .scrollbarWidth(10.0f)
                        .scrollbarGap(10.0f)
                        .content([&](eui::Ui& contentUi, float contentWidth, float) {
                            contentUi.stack("settings.canvas")
                                .size(contentWidth, 780.0f)
                                .content([&] { composeSettings(contentUi, contentWidth); })
                                .build();
                        })
                        .build();
                })
                .build();
        })
        .build();
}

void composeActivityPanel(eui::Ui& ui, float x, float y, float width) {
    // This state belongs only to this reusable UI block, so the page does not own it.
    ActivityState& local = ui.state<ActivityState>("activity.state");
    panel(ui, "activity.panel", x, y, width, 250.0f);
    panelHeading(ui, "activity.title", "Recent activity", x + 24.0f, y + 22.0f, width - 48.0f);

    const std::string summary = local.detailsVisible
        ? "Workspace health is stable. The latest background refresh completed without errors."
        : "Your workspace is healthy and ready for the next session.";
    ui.text("activity.summary")
        .position(x + 24.0f, y + 64.0f)
        .size(width - 48.0f, 52.0f)
        .text(summary)
        .fontSize(15.0f)
        .lineHeight(22.0f)
        .wrap(true)
        .color(textMuted())
        .build();

    ui.text("activity.count")
        .position(x + 24.0f, y + 132.0f)
        .size(160.0f, 34.0f)
        .text("Refreshes: " + std::to_string(local.refreshCount))
        .fontSize(17.0f)
        .lineHeight(24.0f)
        .color(textPrimary())
        .build();

    ui.stack("activity.refresh.wrap")
        .position(x + 24.0f, y + 184.0f)
        .size(118.0f, 42.0f)
        .content([&] {
            components::button(ui, "activity.refresh")
                .theme(theme()).size(118.0f, 42.0f).text("Refresh").icon(0xF021)
                .onClick([localPtr = &local] { ++localPtr->refreshCount; }).build();
        }).build();

    ui.stack("activity.details.wrap")
        .position(x + 152.0f, y + 184.0f)
        .size(112.0f, 42.0f)
        .content([&] {
            components::button(ui, "activity.details")
                .theme(theme()).size(112.0f, 42.0f)
                .text(local.detailsVisible ? "Less" : "Details")
                .icon(local.detailsVisible ? 0xF077 : 0xF078)
                .onClick([localPtr = &local] { localPtr->detailsVisible = !localPtr->detailsVisible; })
                .build();
        }).build();
}

void composeOverview(eui::Ui& ui, float width) {
    const float bodyWidth = std::min(760.0f, std::max(320.0f, width - kPagePad * 2.0f));
    title(ui, "Workspace", "Your profile, activity, and current workspace status.", width);

    panel(ui, "overview.account.panel", kPagePad, 144.0f, bodyWidth, 156.0f);
    panelHeading(ui, "overview.account.title", "Current profile", kPagePad + 24.0f, 166.0f, bodyWidth - 48.0f);
    ui.text("overview.account.name")
        .position(kPagePad + 24.0f, 210.0f)
        .size(bodyWidth - 48.0f, 32.0f)
        .text(state.displayName.get())
        .fontSize(23.0f).lineHeight(30.0f).color(theme().primary).build();
    ui.text("overview.account.status")
        .position(kPagePad + 24.0f, 254.0f)
        .size(bodyWidth - 48.0f, 24.0f)
        .text(state.lastAction)
        .fontSize(14.0f).lineHeight(20.0f).color(textMuted()).build();

    composeActivityPanel(ui, kPagePad, 322.0f, bodyWidth);
}

void composeSettings(eui::Ui& ui, float width) {
    const float bodyWidth = std::min(760.0f, std::max(320.0f, width - kPagePad * 2.0f));
    title(ui, "Settings", "Manage your profile and workspace preferences.", width);

    panel(ui, "settings.profile.panel", kPagePad, 144.0f, bodyWidth, 158.0f);
    panelHeading(ui, "settings.profile.title", "Profile", kPagePad + 24.0f, 166.0f, bodyWidth - 48.0f);
    components::input(ui, "settings.displayName")
        .position(kPagePad + 24.0f, 214.0f)
        .size(std::min(360.0f, bodyWidth - 48.0f), 46.0f)
        .placeholder("Display name")
        .bind(state.displayName)
        .theme(theme())
        .build();

    panel(ui, "settings.preferences.panel", kPagePad, 324.0f, bodyWidth, 210.0f);
    panelHeading(ui, "settings.preferences.title", "Preferences", kPagePad + 24.0f, 346.0f, bodyWidth - 48.0f);
    ui.stack("settings.notifications.wrap")
        .position(kPagePad + 14.0f, 390.0f).size(bodyWidth - 28.0f, 42.0f)
        .content([&] {
            components::toggleSwitch(ui, "settings.notifications")
                .theme(theme()).size(bodyWidth - 28.0f, 42.0f)
                .text("Desktop notifications").bind(state.notifications).transition(motion()).build();
        }).build();

    ui.text("settings.volume.label")
        .position(kPagePad + 24.0f, 452.0f).size(170.0f, 24.0f)
        .text("Focus sound").fontSize(15.0f).lineHeight(21.0f).color(textMuted()).build();
    ui.stack("settings.volume.wrap")
        .position(kPagePad + 190.0f, 447.0f).size(std::max(120.0f, bodyWidth - 238.0f), 34.0f)
        .content([&] {
            components::slider(ui, "settings.volume")
                .theme(theme()).size(std::max(120.0f, bodyWidth - 238.0f), 34.0f)
                .bind(state.focusVolume).transition(motion()).build();
        }).build();

    panel(ui, "settings.schedule.panel", kPagePad, 556.0f, bodyWidth, 184.0f);
    panelHeading(ui, "settings.schedule.title", "Workspace defaults", kPagePad + 24.0f, 578.0f, bodyWidth - 48.0f);

    const float controlWidth = std::min(230.0f, (bodyWidth - 66.0f) * 0.5f);
    ui.stack("settings.theme.wrap")
        .position(kPagePad + 24.0f, 626.0f).size(controlWidth, 150.0f)
        .content([&] {
            components::dropdown(ui, "settings.theme")
                .theme(theme()).size(controlWidth, 44.0f)
                .items({"Graphite", "Light", "Ocean"})
                .selected(state.themeChoice)
                .open(state.popup == Popup::Theme)
                .onChange([](int value) {
                    state.themeChoice = value;
                    state.lastAction = "Theme updated";
                })
                .onOpenChange([](bool open) { state.popup = open ? Popup::Theme : Popup::None; })
                .build();
        }).build();

    ui.stack("settings.date.wrap")
        .position(kPagePad + 42.0f + controlWidth, 626.0f).size(controlWidth, 44.0f)
        .content([&] {
            components::button(ui, "settings.date")
                .theme(theme()).size(controlWidth, 44.0f).text(dateText()).icon(0xF073)
                .onClick([] { state.popup = Popup::Date; }).build();
        }).build();
}

void composeOverlays(eui::Ui& ui, const eui::Screen& screen) {
    components::datePicker(ui, "schedule.datePicker")
        .theme(theme())
        .screen(screen.width, screen.height)
        .date(state.year, state.month, state.day)
        .open(state.popup == Popup::Date)
        .onOpenChange([](bool open) { state.popup = open ? Popup::Date : Popup::None; })
        .onChange([](int year, int month, int day) {
            state.year = year;
            state.month = month;
            state.day = day;
            state.lastAction = "Schedule updated";
        })
        .build();
}

components::theme::ThemeColorTokens theme() {
    const int choice = state.themeChoice;
    auto tokens = choice == 1 ? components::theme::light() : components::theme::dark();
    if (choice == 2) {
        tokens.primary = {0.10f, 0.72f, 0.78f, 1.0f};
    }
    return tokens;
}

eui::Transition motion() {
    return eui::Transition::make(0.18f, eui::Ease::OutCubic);
}

eui::Color textPrimary() {
    return components::theme::pageVisuals(theme()).titleColor;
}

eui::Color textMuted() {
    return components::theme::pageVisuals(theme()).subtitleColor;
}

eui::Color borderColor(float alpha) {
    return components::theme::withOpacity(theme().border, alpha);
}

} // namespace

} // namespace app
