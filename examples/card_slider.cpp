#include "eui_neo.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <string>
#include <vector>

namespace app {

const DslAppConfig& dslAppConfig() {
    static const DslAppConfig config = DslAppConfig{}
        .title("Card Slider")
        .pageId("card_slider")
        .clearColor({0.03f, 0.035f, 0.045f, 1.0f})
        .windowSize(1180, 760)
        .fps(90.0);
    return config;
}

namespace {

struct PoemText {
    std::string title = "诗词";
    std::string author = "佚名";
    std::string content = "正在加载今日诗词";
};

struct PageState {
    int selectedIndex = 0;
    bool autoPlay = true;
    int imageRefreshGeneration = 0;
    float animationSpeed = 0.55f;
    int preparedGeneration = -1;
    std::vector<components::workshop::CardSliderItem> items;
};

constexpr std::array<eui::Color, 4> kFallbackImageThemes{{
    {0.38f, 0.72f, 0.96f, 1.0f},
    {0.74f, 0.58f, 0.34f, 1.0f},
    {0.38f, 0.70f, 0.52f, 1.0f},
    {0.72f, 0.46f, 0.84f, 1.0f},
}};

std::string sessionId() {
    static const std::string value = std::to_string(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    return value;
}

std::string jsonStringValue(const std::string& json, const std::string& key) {
    const std::string token = "\"" + key + "\"";
    const std::size_t begin = json.find(token);
    if (begin == std::string::npos) {
        return {};
    }

    std::size_t valueBegin = begin + token.size();
    while (valueBegin < json.size() &&
           (json[valueBegin] == ' ' || json[valueBegin] == '\t' ||
            json[valueBegin] == '\r' || json[valueBegin] == '\n')) {
        ++valueBegin;
    }
    if (valueBegin >= json.size() || json[valueBegin] != ':') {
        return {};
    }
    ++valueBegin;
    while (valueBegin < json.size() &&
           (json[valueBegin] == ' ' || json[valueBegin] == '\t' ||
            json[valueBegin] == '\r' || json[valueBegin] == '\n')) {
        ++valueBegin;
    }
    if (valueBegin >= json.size() || json[valueBegin] != '"') {
        return {};
    }
    ++valueBegin;

    std::string value;
    bool escaping = false;
    for (std::size_t i = valueBegin; i < json.size(); ++i) {
        const char ch = json[i];
        if (escaping) {
            value.push_back(ch == '/' ? '/' : ch);
            escaping = false;
            continue;
        }
        if (ch == '\\') {
            escaping = true;
            continue;
        }
        if (ch == '"') {
            break;
        }
        value.push_back(ch);
    }
    return value;
}

PoemText poemText(int generation, int index) {
    const std::string poemKey = "card.slider.poem." +
                                 std::to_string(generation) +
                                 "." +
                                 std::to_string(index);
    const std::string url = "https://v1.jinrishici.com/all.json?session=" +
                            sessionId() +
                            "&refresh=" +
                            std::to_string(generation) +
                            "&slot=" +
                            std::to_string(index);
    eui::network::requestText(poemKey, url);
    const eui::network::TextResult result = eui::network::textResult(poemKey);
    if (!result.ready || !result.ok) {
        return {};
    }

    PoemText poem;
    const std::string title = jsonStringValue(result.body, "origin");
    const std::string author = jsonStringValue(result.body, "author");
    const std::string content = jsonStringValue(result.body, "content");
    if (!title.empty()) {
        poem.title = title;
    }
    if (!author.empty()) {
        poem.author = author;
    }
    if (!content.empty()) {
        poem.content = content;
    }
    return poem;
}

std::vector<PoemText> poemTexts(int generation) {
    return {
        poemText(generation, 0),
        poemText(generation, 1),
        poemText(generation, 2),
        poemText(generation, 3)
    };
}

std::vector<std::string> imageSources(int generation) {
    return {
        "https://uapis.cn/api/v1/random/image?category=acg&type=mb&eui_card=0&session=" + sessionId() + "&refresh=" + std::to_string(generation),
        "https://uapis.cn/api/v1/random/image?category=acg&type=mb&eui_card=1&session=" + sessionId() + "&refresh=" + std::to_string(generation),
        "https://uapis.cn/api/v1/random/image?category=acg&type=mb&eui_card=2&session=" + sessionId() + "&refresh=" + std::to_string(generation),
        "https://uapis.cn/api/v1/random/image?category=acg&type=mb&eui_card=3&session=" + sessionId() + "&refresh=" + std::to_string(generation)
    };
}

std::vector<components::workshop::CardSliderItem> sliderItems(const std::vector<std::string>& imageSources,
                                                              const std::vector<PoemText>& poems) {
    return {
        {
            imageSources.size() > 0 ? imageSources[0] : std::string{},
            poems[0].title,
            poems[0].author,
            poems[0].content
        },
        {
            imageSources.size() > 1 ? imageSources[1] : std::string{},
            poems[1].title,
            poems[1].author,
            poems[1].content
        },
        {
            imageSources.size() > 2 ? imageSources[2] : std::string{},
            poems[2].title,
            poems[2].author,
            poems[2].content
        },
        {
            imageSources.size() > 3 ? imageSources[3] : std::string{},
            poems[3].title,
            poems[3].author,
            poems[3].content
        }
    };
}

const std::vector<components::workshop::CardSliderItem>& currentItems(PageState& state) {
    if (state.preparedGeneration != state.imageRefreshGeneration || state.items.empty()) {
        state.preparedGeneration = state.imageRefreshGeneration;
        state.items = sliderItems(imageSources(state.preparedGeneration), poemTexts(state.preparedGeneration));
    } else {
        const std::vector<PoemText> poems = poemTexts(state.preparedGeneration);
        for (std::size_t index = 0; index < state.items.size() && index < poems.size(); ++index) {
            state.items[index].title = poems[index].title;
            state.items[index].subtitle = poems[index].author;
            state.items[index].description = poems[index].content;
        }
        const std::vector<std::string> sources = imageSources(state.preparedGeneration);
        for (std::size_t index = 0; index < state.items.size() && index < sources.size(); ++index) {
            if (!sources[index].empty()) {
                state.items[index].source = sources[index];
            }
        }
    }
    return state.items;
}
eui::Color imageThemeFallback(int index) {
    const eui::Color fallback = kFallbackImageThemes[static_cast<std::size_t>(
        std::clamp(index, 0, static_cast<int>(kFallbackImageThemes.size()) - 1))];
    return fallback;
}

void preloadImages(const std::vector<components::workshop::CardSliderItem>& items) {
    for (std::size_t index = 0; index < items.size(); ++index) {
        bool pending = false;
        eui::image::themeColor(items[index].source,
                               imageThemeFallback(static_cast<int>(index)),
                               false,
                               &pending);
    }
}

components::ButtonStyle themedButtonStyle(const components::theme::ThemeColorTokens& theme,
                                          eui::Color imageAccent,
                                          bool filled) {
    components::ButtonStyle style(theme, false);
    const eui::Color base = filled
        ? imageAccent
        : eui::mixColor(imageAccent, eui::Color{0.04f, 0.055f, 0.075f, 1.0f}, 0.54f);
    style.normal = base;
    style.hover = eui::mixColor(base, eui::Color{1.0f, 1.0f, 1.0f, 1.0f}, filled ? 0.14f : 0.10f);
    style.pressed = eui::mixColor(base, eui::Color{0.0f, 0.0f, 0.0f, 1.0f}, 0.18f);
    style.text = {0.96f, 0.985f, 1.0f, 1.0f};
    style.icon = style.text;
    style.border = {1.0f, components::theme::withOpacity(imageAccent, filled ? 0.62f : 0.48f)};
    style.shadow = {true, {0.0f, 8.0f}, 22.0f, 0.0f, components::theme::withOpacity(imageAccent, 0.20f)};
    style.radius = 13.0f;
    return style;
}

components::theme::ThemeColorTokens tokens(eui::Color imageAccent) {
    auto value = components::theme::dark();
    value.primary = imageAccent;
    return value;
}

eui::Transition motion() {
    auto transition = eui::Transition::make(0.18f, eui::Ease::OutCubic);
    transition.animate(eui::AnimProperty::Color | eui::AnimProperty::Border |
                       eui::AnimProperty::Shadow | eui::AnimProperty::TextColor);
    return transition;
}

float slideDuration(float animationSpeed) {
    return 1.18f - std::clamp(animationSpeed, 0.0f, 1.0f) * 0.86f;
}

} // namespace

void compose(eui::Ui& ui, const eui::Screen& screen) {
    PageState* state = &ui.state<PageState>("page");
    const auto transition = motion();
    const float safeW = std::max(320.0f, screen.width);
    const float safeH = std::max(420.0f, screen.height);
    const float controlsW = std::min(680.0f, std::max(300.0f, safeW - 48.0f));
    const float controlsX = (safeW - controlsW) * 0.5f;
    const float controlsY = safeH - 88.0f;
    const float gap = safeW < 720.0f ? 7.0f : 10.0f;
    const float prevNextW = safeW < 720.0f ? 70.0f : 92.0f;
    const float autoW = safeW < 720.0f ? 94.0f : 118.0f;
    const float refreshW = safeW < 720.0f ? 86.0f : 108.0f;
    const float sliderW = std::max(
        70.0f,
        controlsW - 20.0f - prevNextW * 2.0f - autoW - refreshW - gap * 5.0f - 48.0f);
    const std::vector<components::workshop::CardSliderItem>& items = currentItems(*state);
    const int count = static_cast<int>(items.size());
    const int activeIndex = count > 0 ? std::clamp(state->selectedIndex, 0, count - 1) : 0;
    preloadImages(items);
    const eui::Color imageAccent = count > 0
        ? eui::image::themeColor(items[static_cast<std::size_t>(activeIndex)].source, imageThemeFallback(activeIndex))
        : kFallbackImageThemes[0];
    const auto theme = tokens(imageAccent);
    const auto visuals = components::theme::pageVisuals(theme);

    ui.stack("root")
        .size(screen.width, screen.height)
        .content([&] {
            components::workshop::cardSlider(ui, "poem.card.slider")
                .size(screen.width, screen.height)
                .items(items)
                .currentIndex(state->selectedIndex)
                .autoPlay(state->autoPlay)
                .interval(3.2f)
                .duration(slideDuration(state->animationSpeed))
                .cardSpacing(safeW < 760.0f ? 10.0f : 26.0f)
                .background(true)
                .tilt(true)
                .theme(theme)
                .onChange([state, count](int index) {
                    state->selectedIndex = count > 0 ? std::clamp(index, 0, count - 1) : 0;
                })
                .build();

            ui.rect("controls.bg")
                .position(controlsX, controlsY)
                .size(controlsW, 64.0f)
                .color(components::theme::withOpacity(eui::mixColor(visuals.cardColor, imageAccent, 0.16f), 0.78f))
                .radius(18.0f)
                .border(1.0f, components::theme::withOpacity(imageAccent, 0.48f))
                .shadow(24.0f, 0.0f, 8.0f, components::theme::withOpacity(imageAccent, 0.20f))
                .transition(transition)
                .animate(eui::AnimProperty::Color | eui::AnimProperty::Border | eui::AnimProperty::Shadow)
                .build();

            ui.row("controls.row")
                .position(controlsX + 10.0f, controlsY + 11.0f)
                .size(std::max(0.0f, controlsW - 20.0f), 42.0f)
                .alignItems(eui::Align::CENTER)
                .gap(gap)
                .content([&] {
                    components::button(ui, "controls.prev")
                        .size(prevNextW, 36.0f)
                        .text("Prev")
                        .fontSize(13.0f)
                        .style(themedButtonStyle(theme, imageAccent, false))
                        .transition(transition)
                        .onClick([state, count] {
                            if (count > 0) {
                                state->selectedIndex = (state->selectedIndex + count - 1) % count;
                            }
                        })
                        .build();

                    components::button(ui, "controls.auto")
                        .size(autoW, 36.0f)
                        .text(state->autoPlay ? "Auto On" : "Auto Off")
                        .fontSize(13.0f)
                        .style(themedButtonStyle(theme, imageAccent, state->autoPlay))
                        .transition(transition)
                        .onClick([state] {
                            state->autoPlay = !state->autoPlay;
                        })
                        .build();

                    components::button(ui, "controls.refresh")
                        .size(refreshW, 36.0f)
                        .text("Refresh")
                        .fontSize(13.0f)
                        .style(themedButtonStyle(theme, imageAccent, false))
                        .transition(transition)
                        .onClick([state] {
                            ++state->imageRefreshGeneration;
                        })
                        .build();

                    components::button(ui, "controls.next")
                        .size(prevNextW, 36.0f)
                        .text("Next")
                        .fontSize(13.0f)
                        .style(themedButtonStyle(theme, imageAccent, false))
                        .transition(transition)
                        .onClick([state, count] {
                            if (count > 0) {
                                state->selectedIndex = (state->selectedIndex + 1) % count;
                            }
                        })
                        .build();

                    ui.text("controls.speed.label")
                        .size(42.0f, 36.0f)
                        .text("Speed")
                        .fontSize(12.0f)
                        .lineHeight(16.0f)
                        .color(components::theme::withOpacity(theme.text, 0.76f))
                        .verticalAlign(eui::VerticalAlign::Center)
                        .build();

                    components::slider(ui, "controls.speed")
                        .size(sliderW, 34.0f)
                        .value(state->animationSpeed)
                        .theme(theme)
                        .transition(transition)
                        .onChange([state](float value) {
                            state->animationSpeed = std::clamp(value, 0.0f, 1.0f);
                        })
                        .build();
                })
                .build();
        })
        .build();
}

} // namespace app
