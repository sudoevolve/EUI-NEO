#include "eui_neo.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <functional>
#include <string>
#include <vector>

namespace app {
namespace {

struct CitySpec {
    const char* id;
    const char* name;
    const char* country;
    const char* utcText;
    int offsetMinutes;
};

struct ClockState {
    bool night = false;
    bool use24Hour = true;
    int selectedCity = 20;
    int tick = 0;
    std::string searchText;
    std::vector<int> favorites{4, 7, 20, 13};
};

struct Palette {
    eui::Color bg;
    eui::Color surface;
    eui::Color surfaceHover;
    eui::Color surfacePressed;
    eui::Color text;
    eui::Color muted;
    eui::Color border;
    eui::Color strong;
    eui::Color strongText;
    eui::Color soft;
    eui::Color selectedMuted;
    eui::Shadow panelShadow;
};

constexpr std::array<CitySpec, 27> kCities{{
    {"baker", "Baker Island", "United States", "UTC-12", -12 * 60},
    {"pago", "Pago Pago", "American Samoa", "UTC-11", -11 * 60},
    {"honolulu", "Honolulu", "United States", "UTC-10", -10 * 60},
    {"anchorage", "Anchorage", "United States", "UTC-9", -9 * 60},
    {"la", "Los Angeles", "United States", "UTC-8", -8 * 60},
    {"denver", "Denver", "United States", "UTC-7", -7 * 60},
    {"chicago", "Chicago", "United States", "UTC-6", -6 * 60},
    {"ny", "New York", "United States", "UTC-5", -5 * 60},
    {"halifax", "Halifax", "Canada", "UTC-4", -4 * 60},
    {"buenos", "Buenos Aires", "Argentina", "UTC-3", -3 * 60},
    {"southgeo", "South Georgia", "United Kingdom", "UTC-2", -2 * 60},
    {"azores", "Azores", "Portugal", "UTC-1", -60},
    {"london", "London", "United Kingdom", "UTC+0", 0},
    {"paris", "Paris", "France", "UTC+1", 60},
    {"cairo", "Cairo", "Egypt", "UTC+2", 2 * 60},
    {"moscow", "Moscow", "Russia", "UTC+3", 3 * 60},
    {"dubai", "Dubai", "United Arab Emirates", "UTC+4", 4 * 60},
    {"karachi", "Karachi", "Pakistan", "UTC+5", 5 * 60},
    {"dhaka", "Dhaka", "Bangladesh", "UTC+6", 6 * 60},
    {"bangkok", "Bangkok", "Thailand", "UTC+7", 7 * 60},
    {"beijing", "Beijing", "China", "UTC+8", 8 * 60},
    {"tokyo", "Tokyo", "Japan", "UTC+9", 9 * 60},
    {"sydney", "Sydney", "Australia", "UTC+10", 10 * 60},
    {"noumea", "Noumea", "New Caledonia", "UTC+11", 11 * 60},
    {"auckland", "Auckland", "New Zealand", "UTC+12", 12 * 60},
    {"tongatapu", "Nuku'alofa", "Tonga", "UTC+13", 13 * 60},
    {"kiritimati", "Kiritimati", "Kiribati", "UTC+14", 14 * 60}
}};

constexpr std::size_t kCityCount = kCities.size();
constexpr int kDefaultCity = 20;
constexpr std::size_t kMaxFavorites = 8;
constexpr float kPi = 3.14159265358979323846f;
constexpr eui::Color kClear{0.0f, 0.0f, 0.0f, 0.0f};

ClockState state;

eui::Color color(float r, float g, float b, float a = 1.0f) {
    return {r, g, b, a};
}

eui::Color alpha(eui::Color value, float amount) {
    value.a *= std::clamp(amount, 0.0f, 1.0f);
    return value;
}

Palette palette() {
    if (state.night) {
        return {
            color(0.025f, 0.027f, 0.032f),
            color(0.065f, 0.068f, 0.076f),
            color(0.105f, 0.110f, 0.122f),
            color(0.140f, 0.145f, 0.160f),
            color(0.940f, 0.945f, 0.950f),
            color(0.610f, 0.625f, 0.650f),
            color(0.220f, 0.230f, 0.250f, 0.92f),
            color(0.930f, 0.940f, 0.930f),
            color(0.035f, 0.036f, 0.040f),
            color(0.170f, 0.210f, 0.260f, 0.70f),
            color(0.160f, 0.165f, 0.175f),
            {true, {0.0f, 12.0f}, 34.0f, 0.0f, color(0.0f, 0.0f, 0.0f, 0.34f)}
        };
    }
    return {
        color(0.965f, 0.966f, 0.970f),
        color(1.000f, 1.000f, 1.000f),
        color(0.930f, 0.935f, 0.945f),
        color(0.880f, 0.890f, 0.905f),
        color(0.040f, 0.042f, 0.048f),
        color(0.550f, 0.565f, 0.590f),
        color(0.840f, 0.850f, 0.870f, 0.96f),
        color(0.035f, 0.037f, 0.044f),
        color(0.980f, 0.982f, 0.985f),
        color(0.900f, 0.930f, 0.975f, 0.88f),
        color(0.910f, 0.915f, 0.925f),
        {true, {0.0f, 12.0f}, 30.0f, 0.0f, color(0.100f, 0.120f, 0.180f, 0.13f)}
    };
}

components::theme::ThemeColorTokens themeTokens() {
    const Palette p = palette();
    return {p.bg, p.strong, p.surface, p.surfaceHover, p.surfacePressed, p.text, p.border, state.night};
}

std::tm utcTime(std::time_t value) {
    std::tm out{};
#ifdef _WIN32
    gmtime_s(&out, &value);
#else
    gmtime_r(&value, &out);
#endif
    return out;
}

std::tm timeAtOffset(int offsetMinutes) {
    return utcTime(std::time(nullptr) + static_cast<std::time_t>(offsetMinutes) * 60);
}

std::string formatTime(const std::tm& tm, bool seconds, bool use24Hour) {
    int hour = tm.tm_hour;
    if (!use24Hour) {
        hour %= 12;
        if (hour == 0) {
            hour = 12;
        }
    }
    char buffer[32]{};
    if (seconds) {
        std::snprintf(buffer, sizeof(buffer), use24Hour ? "%02d:%02d:%02d" : "%d:%02d:%02d", hour, tm.tm_min, tm.tm_sec);
    } else {
        std::snprintf(buffer, sizeof(buffer), use24Hour ? "%02d:%02d" : "%d:%02d", hour, tm.tm_min);
    }
    return buffer;
}

std::string formatDate(const std::tm& tm) {
    static constexpr std::array<const char*, 7> kDays{"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
    static constexpr std::array<const char*, 12> kMonths{"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    char buffer[64]{};
    std::snprintf(buffer, sizeof(buffer), "%s, %s %02d %04d",
                  kDays[std::clamp(tm.tm_wday, 0, 6)],
                  kMonths[std::clamp(tm.tm_mon, 0, 11)],
                  tm.tm_mday,
                  tm.tm_year + 1900);
    return buffer;
}

std::string lowerAscii(std::string value) {
    for (char& ch : value) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return value;
}

bool containsLower(const std::string& source, const std::string& query) {
    return query.empty() || lowerAscii(source).find(query) != std::string::npos;
}

bool matchesCity(const CitySpec& city, const std::string& query) {
    if (query.empty()) {
        return false;
    }
    const std::string lower = lowerAscii(query);
    return containsLower(city.name, lower) ||
           containsLower(city.country, lower) ||
           containsLower(city.utcText, lower);
}

std::vector<int> searchMatches() {
    std::vector<int> result;
    for (std::size_t i = 0; i < kCityCount; ++i) {
        if (matchesCity(kCities[i], state.searchText)) {
            result.push_back(static_cast<int>(i));
        }
    }
    return result;
}

void normalizeFavorites() {
    state.selectedCity = std::clamp(state.selectedCity, 0, static_cast<int>(kCityCount) - 1);
    std::vector<int> normalized;
    std::array<bool, kCityCount> seen{};
    for (int index : state.favorites) {
        if (index < 0 || index >= static_cast<int>(kCityCount) || seen[static_cast<std::size_t>(index)]) {
            continue;
        }
        seen[static_cast<std::size_t>(index)] = true;
        normalized.push_back(index);
        if (normalized.size() >= kMaxFavorites) {
            break;
        }
    }
    state.favorites.swap(normalized);
    if (state.favorites.empty()) {
        state.favorites.push_back(kDefaultCity);
    }
}

bool isFavorite(int cityIndex) {
    return std::find(state.favorites.begin(), state.favorites.end(), cityIndex) != state.favorites.end();
}

void addFavorite(int cityIndex) {
    if (!isFavorite(cityIndex) && state.favorites.size() < kMaxFavorites) {
        state.favorites.push_back(cityIndex);
    }
}

void removeFavorite(int cityIndex) {
    state.favorites.erase(std::remove(state.favorites.begin(), state.favorites.end(), cityIndex), state.favorites.end());
    if (state.favorites.empty()) {
        state.favorites.push_back(kDefaultCity);
    }
    if (!isFavorite(state.selectedCity)) {
        state.selectedCity = state.favorites.front();
    }
}

std::string trim(std::string value) {
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r' || value.back() == ' ' || value.back() == '\t')) {
        value.pop_back();
    }
    while (!value.empty() && (value.front() == '\n' || value.front() == '\r' || value.front() == ' ' || value.front() == '\t')) {
        value.erase(value.begin());
    }
    return value;
}

std::string quoteText() {
    static bool requested = false;
    if (!requested) {
        eui::network::requestText("clock.quote", "https://v1.hitokoto.cn/?c=f&encode=text");
        requested = true;
    }
    const eui::network::TextResult result = eui::network::textResult("clock.quote");
    if (result.ready && result.ok) {
        std::string body = trim(result.body);
        if (!body.empty()) {
            if (body.size() > 120) {
                body.resize(120);
            }
            return body;
        }
    }
    return "Every city keeps the same minute in a different light.";
}

void label(eui::Ui& ui,
           const std::string& id,
           float x,
           float y,
           float width,
           float height,
           const std::string& text,
           float fontSize,
           eui::Color textColor,
           eui::HorizontalAlign align = eui::HorizontalAlign::Left,
           bool titleFont = false) {
    auto builder = ui.text(id)
        .x(x)
        .y(y)
        .size(width, height)
        .text(text)
        .fontSize(fontSize)
        .lineHeight(height)
        .color(textColor)
        .horizontalAlign(align)
        .verticalAlign(eui::VerticalAlign::Center);
    if (titleFont) {
        builder.fontFamily("YouSheBiaoTiHei");
    }
    builder.build();
}

void panel(eui::Ui& ui,
           const std::string& id,
           float x,
           float y,
           float width,
           float height,
           float radius,
           eui::Color fill,
           eui::Color border = kClear,
           eui::Shadow shadow = {}) {
    auto builder = ui.rect(id)
        .x(x)
        .y(y)
        .size(width, height)
        .color(fill)
        .radius(radius);
    if (border.a > 0.001f) {
        builder.border(1.0f, border);
    }
    if (shadow.enabled && shadow.color.a > 0.001f && shadow.blur > 0.0f) {
        builder.shadow(shadow);
    }
    builder.build();
}

void composeBrand(eui::Ui& ui, float x, float y, const Palette& p) {
    panel(ui, "brand.dot", x, y + 2.0f, 34.0f, 34.0f, 17.0f, p.strong);
    panel(ui, "brand.hand.v", x + 16.0f, y + 10.0f, 2.0f, 10.0f, 1.0f, p.strongText);
    panel(ui, "brand.hand.h", x + 16.0f, y + 19.0f, 7.0f, 2.0f, 1.0f, p.strongText);
    panel(ui, "brand.center", x + 15.0f, y + 18.0f, 4.0f, 4.0f, 2.0f, p.strongText);
    label(ui, "brand.text", x + 46.0f, y + 3.0f, 170.0f, 34.0f, "TimeSpot", 28.0f, p.text, eui::HorizontalAlign::Left, true);
}

void composeSegmented(eui::Ui& ui,
                      const std::string& id,
                      float x,
                      float y,
                      float width,
                      float height,
                      const std::vector<std::string>& items,
                      int selected,
                      std::function<void(int)> onChange) {
    const Palette p = palette();
    components::SegmentedStyle style(themeTokens());
    style.background = p.surface;
    style.hover = p.surfaceHover;
    style.selected = p.strong;
    style.text = p.text;
    style.selectedText = p.strongText;
    style.border = p.border;

    ui.stack(id + ".wrap")
        .x(x)
        .y(y)
        .size(width, height)
        .content([&] {
            components::segmented(ui, id)
                .style(style)
                .size(width, height)
                .items(items)
                .selected(selected)
                .fontSize(15.0f)
                .onChange(std::move(onChange))
                .build();
        })
        .build();
}

void composeSearch(eui::Ui& ui, float x, float y, float width, const Palette& p) {
    ui.stack("search.wrap")
        .x(x)
        .y(y)
        .size(width, 40.0f)
        .zIndex(80)
        .content([&] {
            components::input(ui, "search.input")
                .theme(themeTokens())
                .size(width, 40.0f)
                .placeholder("Search city or UTC")
                .value(state.searchText)
                .fontSize(16.0f)
                .onChange([](const std::string& value) {
                    state.searchText = value;
                })
                .build();
        })
        .build();

    const std::vector<int> matches = searchMatches();
    if (matches.empty()) {
        return;
    }

    const int visible = std::min<int>(7, static_cast<int>(matches.size()));
    const float rowHeight = 34.0f;
    const float height = static_cast<float>(visible) * rowHeight + 12.0f;
    ui.stack("search.results")
        .x(x)
        .y(y + 48.0f)
        .size(width, height)
        .zIndex(200)
        .content([&] {
            ui.rect("search.results.bg")
                .size(width, height)
                .color(p.surface)
                .radius(14.0f)
                .border(1.0f, p.border)
                .shadow(p.panelShadow)
                .build();

            for (int row = 0; row < visible; ++row) {
                const int cityIndex = matches[static_cast<std::size_t>(row)];
                const CitySpec& city = kCities[static_cast<std::size_t>(cityIndex)];
                const float rowY = 6.0f + static_cast<float>(row) * rowHeight;
                const float textY = rowY + (rowHeight - 18.0f) * 0.5f;
                const std::string id = "search.row." + std::to_string(row);
                ui.rect(id + ".hit")
                    .x(6.0f)
                    .y(rowY)
                    .size(width - 12.0f, rowHeight)
                    .states(kClear, p.surfaceHover, p.surfacePressed)
                    .radius(9.0f)
                    .instantStates()
                    .onClick([cityIndex] {
                        state.selectedCity = cityIndex;
                        state.searchText = kCities[static_cast<std::size_t>(cityIndex)].name;
                    })
                    .build();

                ui.text(id + ".name")
                    .x(18.0f)
                    .y(textY)
                    .size(width - 126.0f, 18.0f)
                    .text(city.name)
                    .fontSize(15.0f)
                    .lineHeight(18.0f)
                    .color(p.text)
                    .verticalAlign(eui::VerticalAlign::Top)
                    .build();

                ui.text(id + ".utc")
                    .x(width - 96.0f)
                    .y(textY)
                    .size(78.0f, 18.0f)
                    .text(city.utcText)
                    .fontSize(14.0f)
                    .lineHeight(18.0f)
                    .color(p.muted)
                    .horizontalAlign(eui::HorizontalAlign::Right)
                    .verticalAlign(eui::VerticalAlign::Top)
                    .build();
            }
        })
        .build();
}

void composeAnalogClock(eui::Ui& ui, float x, float y, float size, const std::tm& tm, const Palette& p) {
    const float cx = x + size * 0.5f;
    const float cy = y + size * 0.5f;
    const float radius = size * 0.5f;
    const eui::Color face = state.night ? color(0.080f, 0.084f, 0.096f) : color(0.990f, 0.992f, 0.996f);

    panel(ui, "analog.face", x, y, size, size, radius, face, p.border, p.panelShadow);
    panel(ui, "analog.inner", x + radius * 0.18f, y + radius * 0.18f, size - radius * 0.36f, size - radius * 0.36f, radius, p.soft, kClear);

    for (int index = 0; index < 12; ++index) {
        const float angle = static_cast<float>(index) * kPi / 6.0f;
        const float tickW = index % 3 == 0 ? 4.0f : 2.0f;
        const float tickH = index % 3 == 0 ? 16.0f : 9.0f;
        const float tx = cx + std::sin(angle) * (radius - 20.0f) - tickW * 0.5f;
        const float ty = cy - std::cos(angle) * (radius - 20.0f) - tickH * 0.5f;
        ui.rect("analog.tick." + std::to_string(index))
            .x(tx)
            .y(ty)
            .size(tickW, tickH)
            .color(index % 3 == 0 ? p.text : p.muted)
            .radius(tickW * 0.5f)
            .rotate(angle)
            .transformOrigin(0.5f, 0.5f)
            .build();
    }

    auto hand = [&](const std::string& id, float angle, float width, float length, eui::Color handColor) {
        const float tail = length * 0.14f;
        ui.rect(id)
            .x(cx - width * 0.5f)
            .y(cy - length + tail)
            .size(width, length)
            .color(handColor)
            .radius(width * 0.5f)
            .rotate(angle)
            .transformOrigin(0.5f, (length - tail) / length)
            .build();
    };

    const float hourAngle = (static_cast<float>(tm.tm_hour % 12) + static_cast<float>(tm.tm_min) / 60.0f) * kPi / 6.0f;
    const float minuteAngle = (static_cast<float>(tm.tm_min) + static_cast<float>(tm.tm_sec) / 60.0f) * kPi / 30.0f;
    const float secondAngle = static_cast<float>(tm.tm_sec) * kPi / 30.0f;
    hand("analog.hour", hourAngle, 8.0f, radius * 0.46f, p.text);
    hand("analog.minute", minuteAngle, 5.0f, radius * 0.68f, p.text);
    hand("analog.second", secondAngle, 2.5f, radius * 0.72f, color(0.91f, 0.25f, 0.27f));
    panel(ui, "analog.pin", cx - 6.0f, cy - 6.0f, 12.0f, 12.0f, 6.0f, p.strong);
}

void composeHero(eui::Ui& ui, float x, float y, float width, const CitySpec& city, const Palette& p) {
    const std::tm now = timeAtOffset(city.offsetMinutes);
    const std::string time = formatTime(now, true, state.use24Hour);
    const float analogSize = std::clamp(width * 0.22f, 180.0f, 240.0f);
    const float analogX = x + width - analogSize - 12.0f;
    const float timeWidth = std::max(360.0f, analogX - x - 40.0f);
    const float timeSize = std::clamp(timeWidth / 5.4f, 104.0f, 178.0f);

    label(ui, "hero.city.kicker", x + 4.0f, y, 260.0f, 28.0f, "Current", 22.0f, p.muted);
    label(ui, "hero.time", x, y + 26.0f, timeWidth, 180.0f, time, timeSize, p.text, eui::HorizontalAlign::Left, true);
    label(ui, "hero.location", x + 4.0f, y + 198.0f, width * 0.42f, 34.0f, std::string(city.name) + ", " + city.country, 24.0f, p.text);
    label(ui, "hero.date", x + width * 0.36f, y + 198.0f, width * 0.34f, 34.0f, formatDate(now), 24.0f, p.muted);
    label(ui, "hero.utc", analogX, y + analogSize + 12.0f, analogSize, 28.0f, city.utcText, 18.0f, p.muted, eui::HorizontalAlign::Center);

    composeAnalogClock(ui, analogX, y + 8.0f, analogSize, now, p);
}

void composeCityTitle(eui::Ui& ui, float x, float y, float width, const CitySpec& city, const Palette& p) {
    const bool favorite = isFavorite(state.selectedCity);
    label(ui, "city.title", x, y, width * 0.56f, 64.0f, std::string(city.name) + ",", 58.0f, p.text, eui::HorizontalAlign::Left, true);
    label(ui, "city.country", x, y + 58.0f, width * 0.56f, 58.0f, city.country, 52.0f, p.text, eui::HorizontalAlign::Left, true);
    label(ui, "city.note", x + width * 0.38f, y + 54.0f, width * 0.38f, 48.0f, quoteText(), 21.0f, p.muted);

    const float buttonW = 200.0f;
    const float buttonH = 38.0f;
    const float buttonX = x + width - buttonW;
    const float buttonY = y + 58.0f;
    ui.rect("city.add.hit")
        .x(buttonX)
        .y(buttonY)
        .size(buttonW, buttonH)
        .states(favorite ? kClear : p.soft,
                favorite ? kClear : eui::mixColor(p.soft, p.strong, 0.08f),
                favorite ? kClear : eui::mixColor(p.soft, p.strong, 0.16f))
        .radius(19.0f)
        .border(favorite ? 0.0f : 1.0f, favorite ? kClear : alpha(p.strong, 0.22f))
        .disabled(favorite)
        .onClick([] {
            addFavorite(state.selectedCity);
        })
        .build();
    ui.text("city.add.text")
        .x(buttonX)
        .y(buttonY)
        .size(buttonW, buttonH)
        .text(favorite ? "Already Added" : "Add City +")
        .fontSize(18.0f)
        .lineHeight(20.0f)
        .color(favorite ? p.muted : p.text)
        .horizontalAlign(eui::HorizontalAlign::Center)
        .verticalAlign(eui::VerticalAlign::Center)
        .build();
}

void composeCityCard(eui::Ui& ui, float x, float y, float width, float height, int cityIndex, const Palette& p) {
    const CitySpec& city = kCities[static_cast<std::size_t>(cityIndex)];
    const bool selected = cityIndex == state.selectedCity;
    const std::tm tm = timeAtOffset(city.offsetMinutes);
    const bool day = tm.tm_hour >= 6 && tm.tm_hour < 18;
    const eui::Color bg = selected ? p.strong : p.surface;
    const eui::Color hover = selected ? eui::mixColor(p.strong, p.surface, state.night ? 0.08f : 0.05f) : p.surfaceHover;
    const eui::Color pressed = selected ? eui::mixColor(p.strong, color(0.0f, 0.0f, 0.0f, p.strong.a), 0.14f) : p.surfacePressed;
    const eui::Color mainText = selected ? p.strongText : p.text;
    const eui::Color subText = selected ? p.selectedMuted : p.muted;
    const std::string id = std::string("city.card.") + city.id;
    const float closeSize = 26.0f;
    const float closeX = width - closeSize - 8.0f;
    const float closeY = 7.0f;

    ui.stack(id)
        .x(x)
        .y(y)
        .size(width, height)
        .visualStateFrom(id + ".bg", 0.975f)
        .transition(0.14f, eui::Ease::OutCubic)
        .content([&] {
            ui.rect(id + ".bg")
                .size(width, height)
                .states(bg, hover, pressed)
                .radius(20.0f)
                .border(selected ? 0.0f : 1.0f, selected ? kClear : p.border)
                .onClick([cityIndex] {
                    state.selectedCity = cityIndex;
                })
                .build();

            label(ui, id + ".name", 20.0f, 16.0f, width - 110.0f, 28.0f, city.name, height < 112.0f ? 21.0f : 25.0f, mainText, eui::HorizontalAlign::Left, true);
            label(ui, id + ".country", 20.0f, 43.0f, width - 110.0f, 24.0f, city.country, 15.0f, subText);
            label(ui, id + ".utc", width - 132.0f, 17.0f, 80.0f, 28.0f, city.utcText, 16.0f, subText, eui::HorizontalAlign::Right);
            label(ui, id + ".time", 20.0f, height - 55.0f, width * 0.62f, 44.0f, formatTime(tm, false, state.use24Hour), height < 112.0f ? 32.0f : 40.0f, mainText, eui::HorizontalAlign::Left, true);
            label(ui, id + ".state", width - 86.0f, height - 45.0f, 64.0f, 28.0f, day ? "Day" : "Night", 16.0f, subText, eui::HorizontalAlign::Right);

            ui.rect(id + ".remove.hit")
                .x(closeX)
                .y(closeY)
                .size(closeSize, closeSize)
                .states(kClear, alpha(subText, 0.18f), alpha(subText, 0.28f))
                .radius(8.0f)
                .zIndex(20)
                .onClick([cityIndex] {
                    removeFavorite(cityIndex);
                })
                .build();
            ui.text(id + ".remove.text")
                .x(closeX)
                .y(closeY)
                .size(closeSize, closeSize)
                .icon(0xF00D)
                .fontSize(12.0f)
                .lineHeight(14.0f)
                .color(subText)
                .horizontalAlign(eui::HorizontalAlign::Center)
                .verticalAlign(eui::VerticalAlign::Center)
                .zIndex(21)
                .build();
        })
        .build();
}

void composeCards(eui::Ui& ui, float x, float y, float width, const Palette& p) {
    const int count = static_cast<int>(state.favorites.size());
    const int columns = width < 860.0f ? 2 : 4;
    const float gap = 12.0f;
    const float cardH = width < 860.0f ? 132.0f : 140.0f;
    const float cardW = (width - gap * static_cast<float>(columns - 1)) / static_cast<float>(columns);

    for (int index = 0; index < count; ++index) {
        const int col = index % columns;
        const int row = index / columns;
        composeCityCard(ui,
                        x + static_cast<float>(col) * (cardW + gap),
                        y + static_cast<float>(row) * (cardH + gap),
                        cardW,
                        cardH,
                        state.favorites[static_cast<std::size_t>(index)],
                        p);
    }
}

} // namespace

const DslAppConfig& dslAppConfig() {
    static DslAppConfig config = DslAppConfig{}
        .title("Clock")
        .pageId("clock")
        .clearColor({0.965f, 0.966f, 0.970f, 1.0f})
        .windowSize(1600, 1080)
        .fps(90.0)
        .textFont("YouSheBiaoTiHei-2.ttf");
    config.clearColor(palette().bg);
    return config;
}

void compose(eui::Ui& ui, const eui::Screen& screen) {
    normalizeFavorites();
    const Palette p = palette();
    const CitySpec& city = kCities[static_cast<std::size_t>(state.selectedCity)];
    const float contentW = std::min(1340.0f, std::max(360.0f, screen.width - 48.0f));
    const float contentX = (screen.width - contentW) * 0.5f;
    const float rightX = contentX + contentW;
    const float navY = 24.0f;
    const float searchW = std::clamp(contentW * 0.30f, 250.0f, 360.0f);
    const float searchX = contentX + (contentW - searchW) * 0.5f;
    const float heroY = 112.0f;
    const float splitY = 388.0f;
    const float cityY = 416.0f;
    const float cardsY = 548.0f;

    ui.stack("root")
        .size(screen.width, screen.height)
        .content([&] {
            ui.rect("bg")
                .size(screen.width, screen.height)
                .color(p.bg)
                .build();

            composeBrand(ui, contentX, navY, p);
            composeSearch(ui, searchX, navY, searchW, p);
            composeSegmented(ui, "mode", rightX - 260.0f, navY + 2.0f, 124.0f, 36.0f, {"Day", "Night"}, state.night ? 1 : 0, [](int index) {
                state.night = index == 1;
            });
            composeSegmented(ui, "hour", rightX - 124.0f, navY + 2.0f, 124.0f, 36.0f, {"12h", "24h"}, state.use24Hour ? 1 : 0, [](int index) {
                state.use24Hour = index == 1;
            });

            composeHero(ui, contentX, heroY, contentW, city, p);

            ui.rect("split")
                .x(contentX)
                .y(splitY)
                .size(contentW, 1.0f)
                .color(p.border)
                .build();

            composeCityTitle(ui, contentX, cityY, contentW, city, p);
            composeCards(ui, contentX, cardsY, contentW, p);

            ui.stack("tick")
                .size(0.0f, 0.0f)
                .onTimer(1.0f, [] {
                    ++state.tick;
                })
                .build();
        })
        .build();
}

} // namespace app
