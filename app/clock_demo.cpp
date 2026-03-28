#include "app/DslAppRuntime.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>

namespace {

struct CitySpec {
    const char* key;
    const char* name;
    const char* country;
    const char* utcText;
    int offsetMinutes;
};

std::tm ToUtcTm(std::time_t value) {
    std::tm out{};
#ifdef _WIN32
    gmtime_s(&out, &value);
#else
    gmtime_r(&value, &out);
#endif
    return out;
}

std::tm TimeAtOffset(int offsetMinutes) {
    const std::time_t now = std::time(nullptr);
    return ToUtcTm(now + static_cast<std::time_t>(offsetMinutes) * 60);
}

std::string FormatTime(const std::tm& tm, bool showSeconds, bool use24Hour) {
    int hour = tm.tm_hour;
    if (!use24Hour) {
        hour %= 12;
        if (hour == 0) {
            hour = 12;
        }
    }

    char buffer[32]{};
    if (showSeconds) {
        if (use24Hour) {
            std::snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d", hour, tm.tm_min, tm.tm_sec);
        } else {
            std::snprintf(buffer, sizeof(buffer), "%d:%02d:%02d", hour, tm.tm_min, tm.tm_sec);
        }
    } else {
        if (use24Hour) {
            std::snprintf(buffer, sizeof(buffer), "%02d:%02d", hour, tm.tm_min);
        } else {
            std::snprintf(buffer, sizeof(buffer), "%d:%02d", hour, tm.tm_min);
        }
    }
    return std::string(buffer);
}

std::string FormatDate(const std::tm& tm) {
    static constexpr std::array<const char*, 7> kWeekdays{
        "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
    };
    static constexpr std::array<const char*, 12> kMonths{
        "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };

    const int weekday = std::clamp(tm.tm_wday, 0, 6);
    const int month = std::clamp(tm.tm_mon, 0, 11);

    char buffer[64]{};
    std::snprintf(
        buffer,
        sizeof(buffer),
        "%s, %s %02d %04d",
        kWeekdays[weekday],
        kMonths[month],
        tm.tm_mday,
        tm.tm_year + 1900
    );
    return std::string(buffer);
}

} // namespace

int main() {
    EUINEO::LightTheme.background = EUINEO::Color(1.0f, 1.0f, 1.0f, 1.0f);
    EUINEO::LightTheme.surface = EUINEO::Color(1.0f, 1.0f, 1.0f, 1.0f);
    EUINEO::LightTheme.surfaceHover = EUINEO::Color(1.0f, 1.0f, 1.0f, 1.0f);
    EUINEO::LightTheme.primary = EUINEO::Color(0.030f, 0.030f, 0.036f, 1.0f);
    EUINEO::LightTheme.text = EUINEO::Color(0.045f, 0.045f, 0.050f, 1.0f);
    EUINEO::LightTheme.border = EUINEO::Color(0.89f, 0.89f, 0.89f, 1.0f);
    EUINEO::CurrentTheme = &EUINEO::LightTheme;
    EUINEO::DslAppConfig config;
    config.title = "EUI Clock Demo";
    config.width = 1400;
    config.height = 780;
    config.pageId = "clock_demo";
    config.fps = 120;

    return EUINEO::RunDslApp(config, [](EUINEO::UIContext& ui, const EUINEO::RectFrame& screen) {
        static bool nightMode = false;
        static bool use24Hour = true;
        static int selectedCityIndex = 2;
        static constexpr std::array<CitySpec, 4> kCities{{
            {"clock.city.la", "Los Angeles", "United States", "UTC-8", -8 * 60},
            {"clock.city.ny", "New York", "United States", "UTC-5", -5 * 60},
            {"clock.city.bj", "Beijing", "China", "UTC+8", 8 * 60},
            {"clock.city.paris", "Paris", "France", "UTC+1", 60}
        }};
        EUINEO::CurrentTheme = nightMode ? &EUINEO::DarkTheme : &EUINEO::LightTheme;
        EUINEO::CurrentTheme->background = nightMode
            ? EUINEO::Color(0.0f, 0.0f, 0.0f, 1.0f)
            : EUINEO::Color(1.0f, 1.0f, 1.0f, 1.0f);
        EUINEO::CurrentTheme->surface = nightMode
            ? EUINEO::Color(0.0f, 0.0f, 0.0f, 1.0f)
            : EUINEO::Color(1.0f, 1.0f, 1.0f, 1.0f);
        EUINEO::CurrentTheme->surfaceHover = nightMode
            ? EUINEO::Color(0.10f, 0.10f, 0.10f, 1.0f)
            : EUINEO::Color(1.0f, 1.0f, 1.0f, 1.0f);
        EUINEO::CurrentTheme->primary = nightMode
            ? EUINEO::Color(0.97f, 0.97f, 0.97f, 1.0f)
            : EUINEO::Color(0.030f, 0.030f, 0.036f, 1.0f);
        EUINEO::CurrentTheme->text = nightMode
            ? EUINEO::Color(0.95f, 0.95f, 0.95f, 1.0f)
            : EUINEO::Color(0.045f, 0.045f, 0.050f, 1.0f);
        EUINEO::CurrentTheme->border = nightMode
            ? EUINEO::Color(0.26f, 0.26f, 0.26f, 1.0f)
            : EUINEO::Color(0.89f, 0.89f, 0.89f, 1.0f);
        if (GLFWwindow* window = EUINEO::ActiveDslWindowState().window) {
            glfwSetWindowOpacity(window, 1.0f);
        }
        selectedCityIndex = std::clamp(selectedCityIndex, 0, static_cast<int>(kCities.size()) - 1);
        const CitySpec& selectedCity = kCities[static_cast<std::size_t>(selectedCityIndex)];
        const std::string cityTitleLine1 = std::string(selectedCity.name) + ",";

        const std::tm heroNow = TimeAtOffset(selectedCity.offsetMinutes);
        const std::string heroTime = FormatTime(heroNow, true, use24Hour);
        const std::string dateText = FormatDate(heroNow);
        const EUINEO::Color ink = nightMode
            ? EUINEO::Color(0.95f, 0.95f, 0.95f, 1.0f)
            : EUINEO::Color(0.045f, 0.045f, 0.050f, 1.0f);
        const EUINEO::Color softText = nightMode
            ? EUINEO::Color(0.60f, 0.60f, 0.60f, 1.0f)
            : EUINEO::Color(0.62f, 0.62f, 0.62f, 1.0f);
        const EUINEO::Color hairline = nightMode
            ? EUINEO::Color(0.22f, 0.22f, 0.22f, 1.0f)
            : EUINEO::Color(0.89f, 0.89f, 0.89f, 1.0f);
        const EUINEO::Color strongPill = nightMode
            ? EUINEO::Color(0.97f, 0.97f, 0.97f, 1.0f)
            : EUINEO::Color(0.030f, 0.030f, 0.036f, 1.0f);
        const EUINEO::Color brightText = nightMode
            ? EUINEO::Color(0.03f, 0.03f, 0.03f, 1.0f)
            : EUINEO::Color(0.975f, 0.975f, 0.975f, 1.0f);
        const EUINEO::Color searchBg = nightMode
            ? EUINEO::Color(0.02f, 0.02f, 0.02f, 1.0f)
            : EUINEO::Color(1.0f, 1.0f, 1.0f, 1.0f);
        const EUINEO::Color searchBorder = nightMode
            ? EUINEO::Color(0.25f, 0.25f, 0.25f, 1.0f)
            : EUINEO::Color(0.92f, 0.92f, 0.92f, 1.0f);

        ui.requestVisualRefresh(1.05f);
        const float pageWidth = screen.width;
        const float pageHeight = screen.height;
        const float pageX = 0.0f;
        const float pageY = 0.0f;
        const float sidePad = 24.0f;
        const float contentW = std::min(1640.0f, std::max(360.0f, pageWidth - sidePad * 2.0f));
        const float contentX = (pageWidth - contentW) * 0.5f;
        const bool compactNav = pageWidth < 1720.0f;
        auto label = [&](const std::string& key, float x, float y, float size, const EUINEO::Color& color, const std::string& text) {
            ui.label(key).position(x, y).fontSize(size).color(color).text(text).build();
        };
        auto panel = [&](const std::string& key, float x, float y, float w, float h, float r, const EUINEO::Color& color) {
            ui.panel(key).position(x, y).size(w, h).rounding(r).background(color).build();
        };

        const float navY = pageY + 20.0f;
        panel("clock.brand.dot", contentX, navY + 2.0f, 34.0f, 34.0f, 17.0f, strongPill);
        panel("clock.brand.hand.v", contentX + 16.0f, navY + 10.0f, 2.0f, 10.0f, 1.0f, brightText);
        panel("clock.brand.hand.h", contentX + 16.0f, navY + 19.0f, 7.0f, 2.0f, 1.0f, brightText);
        panel("clock.brand.center", contentX + 15.0f, navY + 18.0f, 4.0f, 4.0f, 2.0f, brightText);
        label("clock.brand.text", contentX + 46.0f, navY + 27.0f, 28.0f, ink, "TimeSpot");

        const float searchW = 360.0f;
        const float searchX = pageX + (pageWidth - searchW) * 0.5f;
        ui.panel("clock.search")
            .position(searchX, navY + 2.0f)
            .size(searchW, 34.0f)
            .rounding(17.0f)
            .background(searchBg)
            .border(1.0f, searchBorder)
            .build();
        label("clock.search.text", searchX + 26.0f, navY + 24.0f, 20.0f, softText, "Search");

        const float rightX = contentX + contentW;
        const bool fullscreenNow = EUINEO::IsDslWindowFullscreen();
        if (!compactNav) {
            label("clock.login", rightX - 230.0f, navY + 26.0f, 24.0f, ink, "Log In");
        }
        const float appButtonWidth = compactNav ? 116.0f : 80.0f;
        const std::string appButtonText = compactNav ? (fullscreenNow ? "Windowed" : "Fullscreen") : "Get the App";
        ui.segmented("clock.mode.segmented")
            .position(rightX - appButtonWidth - 142.0f, navY + 2.0f)
            .size(132.0f, 36.0f)
            .rounding(18.0f)
            .items(std::vector<std::string>{"Day", "Night"})
            .selected(nightMode ? 1 : 0)
            .fontSize(18.0f)
            .onChange([&](int index) { nightMode = index == 1; })
            .build();
        panel("clock.getapp.bg", rightX - appButtonWidth, navY + 2.0f, appButtonWidth, 36.0f, 18.0f, strongPill);
        label("clock.getapp.text", rightX - appButtonWidth + 16.0f, navY + 26.0f, 20.0f, brightText, appButtonText);
        ui.button("clock.getapp.hit")
            .position(rightX - appButtonWidth, navY + 2.0f)
            .size(appButtonWidth, 36.0f)
            .opacity(0.0f)
            .text("")
            .onClick([&]() {
                if (compactNav) {
                    EUINEO::ToggleDslWindowFullscreen();
                }
            })
            .build();

        const float heroY = navY + 78.0f;
        label("clock.hero.time", contentX + 4.0f, heroY + 172.0f, 230.0f, ink, heroTime);
        const float metaY = heroY + 220.0f;
        label("clock.current", contentX, metaY + 26.0f, 22.0f, softText, "Current");
        label("clock.sun", contentX + contentW * 0.33f, metaY + 24.0f, 26.0f, ink, "Sun 07:12 - 17:17 (10h 06m)");
        label("clock.date", contentX + contentW * 0.33f, metaY + 54.0f, 30.0f, ink, dateText);
        ui.segmented("clock.toggle.segmented")
            .position(rightX - 122.0f, metaY + 8.0f)
            .size(118.0f, 42.0f)
            .rounding(21.0f)
            .items(std::vector<std::string>{"12h", "24h"})
            .selected(use24Hour ? 1 : 0)
            .fontSize(20.0f)
            .onChange([&](int index) { use24Hour = index == 1; })
            .build();

        const float splitY = metaY + 84.0f;
        ui.panel("clock.split.1")
            .position(contentX, splitY)
            .size(contentW, 1.0f)
            .background(hairline)
            .build();
        const float cityTitleY = splitY + 28.0f;
        label("clock.city.title.line1", contentX, cityTitleY + 52.0f, 72.0f, ink, cityTitleLine1);
        label("clock.city.title.line2", contentX, cityTitleY + 122.0f, 72.0f, ink, selectedCity.country);
        label("clock.city.note", contentX + contentW * 0.33f, cityTitleY + 88.0f, 30.0f, softText, "小手拍拍，大家的笑容闪闪发亮!");
        label("clock.city.add", rightX - 250.0f, cityTitleY + 88.0f, 30.0f, ink, "Add Another City +");

        const float cardsY = cityTitleY + 154.0f;
        const float gap = 12.0f;
        const float cardW = (contentW - gap * 3.0f) * 0.25f;
        const float cardH = 156.0f;
        for (std::size_t i = 0; i < kCities.size(); ++i) {
            const CitySpec& city = kCities[i];
            const bool isSelected = static_cast<int>(i) == selectedCityIndex;
            const float x = contentX + static_cast<float>(i) * (cardW + gap);
            const std::tm cityTm = TimeAtOffset(city.offsetMinutes);
            const std::string cityTime = FormatTime(cityTm, false, use24Hour);
            const std::string dayText = cityTm.tm_hour >= 6 && cityTm.tm_hour < 18 ? "Day" : "Night";
            const std::string stateText = use24Hour ? dayText : (cityTm.tm_hour >= 12 ? "PM" : "AM");
            const EUINEO::Color bg = isSelected
                ? (nightMode ? EUINEO::Color(0.98f, 0.98f, 0.98f, 1.0f) : EUINEO::Color(0.035f, 0.035f, 0.040f, 1.0f))
                : (nightMode ? EUINEO::Color(0.0f, 0.0f, 0.0f, 1.0f) : EUINEO::Color(1.0f, 1.0f, 1.0f, 1.0f));
            const EUINEO::Color textMain = isSelected ? brightText : ink;
            const EUINEO::Color textSub = isSelected
                ? (nightMode ? EUINEO::Color(0.10f, 0.10f, 0.10f, 1.0f) : EUINEO::Color(0.91f, 0.91f, 0.91f, 1.0f))
                : softText;

            ui.panel(std::string(city.key) + ".card")
                .position(x, cardsY)
                .size(cardW, cardH)
                .rounding(20.0f)
                .background(bg)
                .border(isSelected ? 0.0f : 1.0f, hairline)
                .build();
            label(std::string(city.key) + ".name", x + 20.0f, cardsY + 34.0f, 30.0f, textMain, city.name);
            label(std::string(city.key) + ".utc", x + cardW - 92.0f, cardsY + 34.0f, 24.0f, textSub, city.utcText);
            label(std::string(city.key) + ".time", x + 20.0f, cardsY + 98.0f, 62.0f, textMain, cityTime);
            label(std::string(city.key) + ".state", x + cardW - 84.0f, cardsY + 98.0f, 24.0f, textSub, stateText);
            ui.button(std::string(city.key) + ".hit")
                .position(x, cardsY)
                .size(cardW, cardH)
                .opacity(0.0f)
                .text("")
                .onClick([&, i]() { selectedCityIndex = static_cast<int>(i); })
                .build();
        }
    });
}
