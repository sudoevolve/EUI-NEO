#include "app/DslAppRuntime.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>
namespace {
struct CitySpec {
    const char* key;
    const char* nameEn;
    const char* nameZh;
    const char* countryEn;
    const char* countryZh;
    const char* utcText;
    int offsetMinutes;
};
struct DemoState {
    bool nightMode = false;
    bool use24Hour = true;
    int selectedCityIndex = 20;
    std::string searchText;
    std::vector<int> favoriteCityIndices{4, 7, 20, 13};
};
struct UiPalette {
    EUINEO::Color ink;
    EUINEO::Color softText;
    EUINEO::Color hairline;
    EUINEO::Color strongPill;
    EUINEO::Color brightText;
    EUINEO::Color searchBg;
    EUINEO::Color searchBorder;
};
constexpr std::array<CitySpec, 27> kCities{{
    {"clock.city.baker", "Baker Island", "贝克岛", "United States", "美国", "UTC-12", -12 * 60},
    {"clock.city.pago", "Pago Pago", "帕果帕果", "American Samoa", "美属萨摩亚", "UTC-11", -11 * 60},
    {"clock.city.honolulu", "Honolulu", "火奴鲁鲁", "United States", "美国", "UTC-10", -10 * 60},
    {"clock.city.anchorage", "Anchorage", "安克雷奇", "United States", "美国", "UTC-9", -9 * 60},
    {"clock.city.la", "Los Angeles", "洛杉矶", "United States", "美国", "UTC-8", -8 * 60},
    {"clock.city.denver", "Denver", "丹佛", "United States", "美国", "UTC-7", -7 * 60},
    {"clock.city.chicago", "Chicago", "芝加哥", "United States", "美国", "UTC-6", -6 * 60},
    {"clock.city.ny", "New York", "纽约", "United States", "美国", "UTC-5", -5 * 60},
    {"clock.city.halifax", "Halifax", "哈利法克斯", "Canada", "加拿大", "UTC-4", -4 * 60},
    {"clock.city.buenos", "Buenos Aires", "布宜诺斯艾利斯", "Argentina", "阿根廷", "UTC-3", -3 * 60},
    {"clock.city.southgeo", "South Georgia", "南乔治亚", "United Kingdom", "英国", "UTC-2", -2 * 60},
    {"clock.city.azores", "Azores", "亚速尔", "Portugal", "葡萄牙", "UTC-1", -60},
    {"clock.city.london", "London", "伦敦", "United Kingdom", "英国", "UTC+0", 0},
    {"clock.city.paris", "Paris", "巴黎", "France", "法国", "UTC+1", 60},
    {"clock.city.cairo", "Cairo", "开罗", "Egypt", "埃及", "UTC+2", 2 * 60},
    {"clock.city.moscow", "Moscow", "莫斯科", "Russia", "俄罗斯", "UTC+3", 3 * 60},
    {"clock.city.dubai", "Dubai", "迪拜", "United Arab Emirates", "阿联酋", "UTC+4", 4 * 60},
    {"clock.city.karachi", "Karachi", "卡拉奇", "Pakistan", "巴基斯坦", "UTC+5", 5 * 60},
    {"clock.city.dhaka", "Dhaka", "达卡", "Bangladesh", "孟加拉国", "UTC+6", 6 * 60},
    {"clock.city.bangkok", "Bangkok", "曼谷", "Thailand", "泰国", "UTC+7", 7 * 60},
    {"clock.city.bj", "Beijing", "北京", "China", "中国", "UTC+8", 8 * 60},
    {"clock.city.tokyo", "Tokyo", "东京", "Japan", "日本", "UTC+9", 9 * 60},
    {"clock.city.sydney", "Sydney", "悉尼", "Australia", "澳大利亚", "UTC+10", 10 * 60},
    {"clock.city.noumea", "Noumea", "努美阿", "New Caledonia", "新喀里多尼亚", "UTC+11", 11 * 60},
    {"clock.city.auckland", "Auckland", "奥克兰", "New Zealand", "新西兰", "UTC+12", 12 * 60},
    {"clock.city.tongatapu", "Nuku'alofa", "努库阿洛法", "Tonga", "汤加", "UTC+13", 13 * 60},
    {"clock.city.kiritimati", "Kiritimati", "圣诞岛", "Kiribati", "基里巴斯", "UTC+14", 14 * 60}
}};
constexpr int kDefaultCityIndex = 20;
constexpr std::size_t kMaxFavoriteCities = 8;
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
    return ToUtcTm(std::time(nullptr) + static_cast<std::time_t>(offsetMinutes) * 60);
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
        std::snprintf(buffer, sizeof(buffer), use24Hour ? "%02d:%02d:%02d" : "%d:%02d:%02d", hour, tm.tm_min, tm.tm_sec);
    } else {
        std::snprintf(buffer, sizeof(buffer), use24Hour ? "%02d:%02d" : "%d:%02d", hour, tm.tm_min);
    }
    return std::string(buffer);
}
std::string FormatDate(const std::tm& tm) {
    static constexpr std::array<const char*, 7> kWeekdays{"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    static constexpr std::array<const char*, 12> kMonths{"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    char buffer[64]{};
    std::snprintf(buffer, sizeof(buffer), "%s, %s %02d %04d", kWeekdays[std::clamp(tm.tm_wday, 0, 6)], kMonths[std::clamp(tm.tm_mon, 0, 11)], tm.tm_mday, tm.tm_year + 1900);
    return std::string(buffer);
}
std::string ToLowerAscii(std::string value) {
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}
bool ContainsIgnoreCaseAscii(const std::string& haystack, const std::string& needleLower) {
    return needleLower.empty() || ToLowerAscii(haystack).find(needleLower) != std::string::npos;
}
bool MatchesCity(const CitySpec& city, const std::string& queryRaw, const std::string& queryLower) {
    return queryRaw.empty() ||
           ContainsIgnoreCaseAscii(city.nameEn, queryLower) ||
           ContainsIgnoreCaseAscii(city.countryEn, queryLower) ||
           ContainsIgnoreCaseAscii(city.utcText, queryLower) ||
           std::string(city.nameZh).find(queryRaw) != std::string::npos ||
           std::string(city.countryZh).find(queryRaw) != std::string::npos;
}
void ApplyTheme(bool nightMode) {
    EUINEO::CurrentTheme = nightMode ? &EUINEO::DarkTheme : &EUINEO::LightTheme;
    EUINEO::CurrentTheme->background = nightMode ? EUINEO::Color(0.0f, 0.0f, 0.0f, 1.0f) : EUINEO::Color(1.0f, 1.0f, 1.0f, 1.0f);
    EUINEO::CurrentTheme->surface = nightMode ? EUINEO::Color(0.0f, 0.0f, 0.0f, 1.0f) : EUINEO::Color(1.0f, 1.0f, 1.0f, 1.0f);
    EUINEO::CurrentTheme->surfaceHover = nightMode ? EUINEO::Color(0.10f, 0.10f, 0.10f, 1.0f) : EUINEO::Color(1.0f, 1.0f, 1.0f, 1.0f);
    EUINEO::CurrentTheme->primary = nightMode ? EUINEO::Color(0.97f, 0.97f, 0.97f, 1.0f) : EUINEO::Color(0.030f, 0.030f, 0.036f, 1.0f);
    EUINEO::CurrentTheme->text = nightMode ? EUINEO::Color(0.95f, 0.95f, 0.95f, 1.0f) : EUINEO::Color(0.045f, 0.045f, 0.050f, 1.0f);
    EUINEO::CurrentTheme->border = nightMode ? EUINEO::Color(0.26f, 0.26f, 0.26f, 1.0f) : EUINEO::Color(0.89f, 0.89f, 0.89f, 1.0f);
}
UiPalette BuildPalette(bool nightMode) {
    UiPalette palette;
    palette.ink = nightMode ? EUINEO::Color(0.95f, 0.95f, 0.95f, 1.0f) : EUINEO::Color(0.045f, 0.045f, 0.050f, 1.0f);
    palette.softText = nightMode ? EUINEO::Color(0.60f, 0.60f, 0.60f, 1.0f) : EUINEO::Color(0.62f, 0.62f, 0.62f, 1.0f);
    palette.hairline = nightMode ? EUINEO::Color(0.22f, 0.22f, 0.22f, 1.0f) : EUINEO::Color(0.89f, 0.89f, 0.89f, 1.0f);
    palette.strongPill = nightMode ? EUINEO::Color(0.97f, 0.97f, 0.97f, 1.0f) : EUINEO::Color(0.030f, 0.030f, 0.036f, 1.0f);
    palette.brightText = nightMode ? EUINEO::Color(0.03f, 0.03f, 0.03f, 1.0f) : EUINEO::Color(0.975f, 0.975f, 0.975f, 1.0f);
    palette.searchBg = nightMode ? EUINEO::Color(0.02f, 0.02f, 0.02f, 1.0f) : EUINEO::Color(1.0f, 1.0f, 1.0f, 1.0f);
    palette.searchBorder = nightMode ? EUINEO::Color(0.25f, 0.25f, 0.25f, 1.0f) : EUINEO::Color(0.92f, 0.92f, 0.92f, 1.0f);
    return palette;
}
void NormalizeFavorites(DemoState& state) {
    std::vector<int> normalized;
    normalized.reserve(std::min<std::size_t>(state.favoriteCityIndices.size(), kMaxFavoriteCities));
    std::vector<bool> seen(kCities.size(), false);
    for (int index : state.favoriteCityIndices) {
        if (index < 0 || index >= static_cast<int>(kCities.size())) {
            continue;
        }
        if (seen[static_cast<std::size_t>(index)]) {
            continue;
        }
        seen[static_cast<std::size_t>(index)] = true;
        normalized.push_back(index);
        if (normalized.size() >= kMaxFavoriteCities) {
            break;
        }
    }
    state.favoriteCityIndices.swap(normalized);
    if (state.favoriteCityIndices.empty()) {
        state.selectedCityIndex = kDefaultCityIndex;
    }
}
std::vector<int> BuildSearchMatches(const std::string& queryRaw) {
    std::vector<int> matches;
    if (queryRaw.empty()) {
        return matches;
    }
    const std::string queryLower = ToLowerAscii(queryRaw);
    for (std::size_t i = 0; i < kCities.size(); ++i) {
        if (MatchesCity(kCities[i], queryRaw, queryLower)) {
            matches.push_back(static_cast<int>(i));
        }
    }
    return matches;
}
void DrawSearchResults(EUINEO::UIContext& ui, const UiPalette& palette, float searchX, float resultY, float searchW,
                       const std::vector<int>& matches, DemoState& state) {
    if (matches.empty()) {
        return;
    }
    const int visibleCount = std::min<int>(8, static_cast<int>(matches.size()));
    const float rowHeight = 28.0f;
    ui.panel("clock.search.results")
        .position(searchX, resultY)
        .size(searchW, static_cast<float>(visibleCount) * rowHeight + 8.0f)
        .rounding(12.0f)
        .background(palette.searchBg)
        .border(1.0f, palette.searchBorder)
        .popupLayer()
        .zIndex(2000)
        .build();
    for (int row = 0; row < visibleCount; ++row) {
        const int cityIndex = matches[static_cast<std::size_t>(row)];
        const CitySpec& city = kCities[static_cast<std::size_t>(cityIndex)];
        const float rowY = resultY + 5.0f + static_cast<float>(row) * rowHeight;
        const std::string key = "clock.search.row." + std::to_string(row);
        const std::string text = std::string(city.nameEn) + " / " + city.nameZh + "  " + city.utcText;
        ui.label(key + ".text")
            .position(searchX + 12.0f, rowY + 20.0f)
            .fontSize(16.0f)
            .color(palette.ink)
            .text(text)
            .popupLayer()
            .zIndex(2001)
            .build();
        ui.button(key + ".hit")
            .position(searchX + 4.0f, rowY - 2.0f)
            .size(searchW - 8.0f, rowHeight)
            .opacity(0.0f)
            .text("")
            .popupLayer()
            .zIndex(2002)
            .onClick([&state, cityIndex]() {
                state.selectedCityIndex = cityIndex;
                state.searchText = kCities[static_cast<std::size_t>(cityIndex)].nameEn;
            })
            .build();
    }
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
        static DemoState state;
        state.selectedCityIndex = std::clamp(state.selectedCityIndex, 0, static_cast<int>(kCities.size()) - 1);
        ApplyTheme(state.nightMode);
        if (GLFWwindow* window = EUINEO::ActiveDslWindowState().window) {
            glfwSetWindowOpacity(window, 1.0f);
        }
        NormalizeFavorites(state);
        const CitySpec& selectedCity = kCities[static_cast<std::size_t>(state.selectedCityIndex)];
        const std::vector<int> searchMatches = BuildSearchMatches(state.searchText);
        const UiPalette palette = BuildPalette(state.nightMode);
        const std::tm heroNow = TimeAtOffset(selectedCity.offsetMinutes);
        const std::string heroTime = FormatTime(heroNow, true, state.use24Hour);
        const std::string dateText = FormatDate(heroNow);
        const std::string cityTitleLine1 = std::string(selectedCity.nameEn) + " · " + selectedCity.nameZh + ",";
        ui.requestVisualRefresh(1.05f);
        const float contentW = std::min(1640.0f, std::max(360.0f, screen.width - 48.0f));
        const float contentX = (screen.width - contentW) * 0.5f;
        const float rightX = contentX + contentW;
        const float navY = 20.0f;
        const bool compactNav = screen.width < 1720.0f;
        auto label = [&](const std::string& key, float x, float y, float size, const EUINEO::Color& color, const std::string& text) {
            ui.label(key).position(x, y).fontSize(size).color(color).text(text).build();
        };
        auto panel = [&](const std::string& key, float x, float y, float w, float h, float r, const EUINEO::Color& color) {
            ui.panel(key).position(x, y).size(w, h).rounding(r).background(color).build();
        };
        panel("clock.brand.dot", contentX, navY + 2.0f, 34.0f, 34.0f, 17.0f, palette.strongPill);
        panel("clock.brand.hand.v", contentX + 16.0f, navY + 10.0f, 2.0f, 10.0f, 1.0f, palette.brightText);
        panel("clock.brand.hand.h", contentX + 16.0f, navY + 19.0f, 7.0f, 2.0f, 1.0f, palette.brightText);
        panel("clock.brand.center", contentX + 15.0f, navY + 18.0f, 4.0f, 4.0f, 2.0f, palette.brightText);
        label("clock.brand.text", contentX + 46.0f, navY + 27.0f, 28.0f, palette.ink, "TimeSpot");

        const float searchW = 360.0f;
        const float searchX = (screen.width - searchW) * 0.5f;
        ui.input("clock.search.input")
            .position(searchX, navY + 2.0f)
            .size(searchW, 34.0f)
            .placeholder("Search city / 搜索城市")
            .fontSize(18.0f)
            .text(state.searchText)
            .onChange([&](const std::string& text) { state.searchText = text; })
            .build();
        DrawSearchResults(ui, palette, searchX, navY + 42.0f, searchW, searchMatches, state);
        if (!compactNav) {
            label("clock.login", rightX - 230.0f, navY + 26.0f, 24.0f, palette.ink, "Log In");
        }
        const bool fullscreenNow = EUINEO::IsDslWindowFullscreen();
        const float appButtonWidth = compactNav ? 116.0f : 80.0f;
        const std::string appButtonText = compactNav ? (fullscreenNow ? "Windowed" : "Fullscreen") : "Get the App";
        ui.segmented("clock.mode.segmented")
            .position(rightX - appButtonWidth - 142.0f, navY + 2.0f)
            .size(132.0f, 36.0f)
            .rounding(18.0f)
            .items(std::vector<std::string>{"Day", "Night"})
            .selected(state.nightMode ? 1 : 0)
            .fontSize(18.0f)
            .onChange([&](int index) { state.nightMode = index == 1; })
            .build();
        panel("clock.getapp.bg", rightX - appButtonWidth, navY + 2.0f, appButtonWidth, 36.0f, 18.0f, palette.strongPill);
        label("clock.getapp.text", rightX - appButtonWidth + 16.0f, navY + 26.0f, 20.0f, palette.brightText, appButtonText);
        ui.button("clock.getapp.hit")
            .position(rightX - appButtonWidth, navY + 2.0f)
            .size(appButtonWidth, 36.0f)
            .opacity(0.0f)
            .text("")
            .onClick([compactNav]() {
                if (compactNav) {
                    EUINEO::ToggleDslWindowFullscreen();
                }
            })
            .build();
        const float heroY = navY + 78.0f;
        const float metaY = heroY + 220.0f;
        label("clock.hero.time", contentX + 4.0f, heroY + 172.0f, 230.0f, palette.ink, heroTime);
        label("clock.current", contentX, metaY + 26.0f, 22.0f, palette.softText, "Current");
        label("clock.sun", contentX + contentW * 0.33f, metaY + 24.0f, 26.0f, palette.ink, "Sun 07:12 - 17:17 (10h 06m)");
        label("clock.date", contentX + contentW * 0.33f, metaY + 54.0f, 30.0f, palette.ink, dateText);
        ui.segmented("clock.toggle.segmented")
            .position(rightX - 122.0f, metaY + 8.0f)
            .size(118.0f, 42.0f)
            .rounding(21.0f)
            .items(std::vector<std::string>{"12h", "24h"})
            .selected(state.use24Hour ? 1 : 0)
            .fontSize(20.0f)
            .onChange([&](int index) { state.use24Hour = index == 1; })
            .build();
        const float splitY = metaY + 84.0f;
        const float cityTitleY = splitY + 28.0f;
        ui.panel("clock.split.1").position(contentX, splitY).size(contentW, 1.0f).background(palette.hairline).build();
        label("clock.city.title.line1", contentX, cityTitleY + 52.0f, 72.0f, palette.ink, cityTitleLine1);
        label("clock.city.title.line2", contentX, cityTitleY + 122.0f, 72.0f, palette.ink, selectedCity.countryEn);
        label("clock.city.note", contentX + contentW * 0.33f, cityTitleY + 88.0f, 30.0f, palette.softText, "小手拍拍，大家的笑容闪闪发亮!");
        const bool alreadyFavorite = std::find(state.favoriteCityIndices.begin(), state.favoriteCityIndices.end(), state.selectedCityIndex) != state.favoriteCityIndices.end();
        label("clock.city.add", rightX - 250.0f, cityTitleY + 88.0f, 30.0f, palette.ink, alreadyFavorite ? "Already Added" : "Add Another City +");
        ui.button("clock.city.add.hit")
            .position(rightX - 254.0f, cityTitleY + 58.0f)
            .size(250.0f, 38.0f)
            .opacity(0.0f)
            .text("")
            .onClick([&]() {
                if (state.favoriteCityIndices.size() < kMaxFavoriteCities &&
                    std::find(state.favoriteCityIndices.begin(), state.favoriteCityIndices.end(), state.selectedCityIndex) == state.favoriteCityIndices.end()) {
                    state.favoriteCityIndices.push_back(state.selectedCityIndex);
                }
            })
            .build();
        const float cardsY = cityTitleY + 154.0f;
        const float cardW = (contentW - 36.0f) * 0.25f;
        const float cardH = 156.0f;
        for (std::size_t i = 0; i < state.favoriteCityIndices.size(); ++i) {
            const int cityIndex = state.favoriteCityIndices[i];
            const CitySpec& city = kCities[static_cast<std::size_t>(cityIndex)];
            const float x = contentX + static_cast<float>(static_cast<int>(i) % 4) * (cardW + 12.0f);
            const float y = cardsY + static_cast<float>(static_cast<int>(i) / 4) * (cardH + 12.0f);
            const bool isSelected = cityIndex == state.selectedCityIndex;
            const std::tm cityTm = TimeAtOffset(city.offsetMinutes);
            const std::string cityTime = FormatTime(cityTm, false, state.use24Hour);
            const std::string stateText = state.use24Hour ? (cityTm.tm_hour >= 6 && cityTm.tm_hour < 18 ? "Day" : "Night") : (cityTm.tm_hour >= 12 ? "PM" : "AM");
            const EUINEO::Color bg = isSelected ? (state.nightMode ? EUINEO::Color(0.98f, 0.98f, 0.98f, 1.0f) : EUINEO::Color(0.035f, 0.035f, 0.040f, 1.0f))
                                               : (state.nightMode ? EUINEO::Color(0.0f, 0.0f, 0.0f, 1.0f) : EUINEO::Color(1.0f, 1.0f, 1.0f, 1.0f));
            const EUINEO::Color mainText = isSelected ? palette.brightText : palette.ink;
            const EUINEO::Color subText = isSelected ? (state.nightMode ? EUINEO::Color(0.10f, 0.10f, 0.10f, 1.0f) : EUINEO::Color(0.91f, 0.91f, 0.91f, 1.0f)) : palette.softText;
            ui.panel(std::string(city.key) + ".card").position(x, y).size(cardW, cardH).rounding(20.0f).background(bg).border(isSelected ? 0.0f : 1.0f, palette.hairline).build();
            label(std::string(city.key) + ".name", x + 20.0f, y + 34.0f, 30.0f, mainText, city.nameEn);
            label(std::string(city.key) + ".name.zh", x + 20.0f, y + 58.0f, 20.0f, subText, city.nameZh);
            label(std::string(city.key) + ".utc", x + cardW - 92.0f, y + 34.0f, 24.0f, subText, city.utcText);
            label(std::string(city.key) + ".time", x + 20.0f, y + 108.0f, 54.0f, mainText, cityTime);
            label(std::string(city.key) + ".state", x + cardW - 84.0f, y + 108.0f, 24.0f, subText, stateText);
            label(std::string(city.key) + ".remove.text", x + cardW - 26.0f, y + 24.0f, 24.0f, subText, "×");
            ui.button(std::string(city.key) + ".remove.hit")
                .position(x + cardW - 36.0f, y + 2.0f)
                .size(30.0f, 26.0f)
                .opacity(0.0f)
                .text("")
                .onClick([&, cityIndex]() {
                    state.favoriteCityIndices.erase(
                        std::remove(state.favoriteCityIndices.begin(), state.favoriteCityIndices.end(), cityIndex),
                        state.favoriteCityIndices.end()
                    );
                    if (state.favoriteCityIndices.empty()) {
                        state.selectedCityIndex = kDefaultCityIndex;
                    }
                })
                .build();
            ui.button(std::string(city.key) + ".hit")
                .position(x, y)
                .size(cardW, cardH)
                .opacity(0.0f)
                .text("")
                .onClick([&, cityIndex]() { state.selectedCityIndex = cityIndex; })
                .build();
        }
    });
}
