#include "eui_neo.h"
#include "vcd_model.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace app {

const DslAppConfig& dslAppConfig() {
    static const DslAppConfig config = DslAppConfig{}
        .title("EUI VCD Viewer (binary import)")
        .pageId("vcd_viewer")
        .clearColor({0.035f, 0.045f, 0.06f, 1.0f})
        .windowSize(1320, 820)
        .fps(60.0);
    return config;
}

namespace {

struct ViewerState {
    vcd::Document document;
    eui::Signal<std::string> search;
    eui::Signal<float> zoom{0.0f};
    eui::Signal<float> pan{0.0f};
    float waveScroll = 0.0f;
    std::size_t selectedSignal = 0;
    std::vector<std::size_t> visibleSignals;
    std::string visibleSignalsSearch;
    int visibleSignalsGeneration = -1;
    std::string status = "Open a VCD file to inspect its waveforms.";
    std::string error;
    int generation = 0;
};

const components::theme::ThemeColorTokens& theme() {
    static const components::theme::ThemeColorTokens tokens = [] {
        auto value = components::theme::dark();
        value.primary = {0.20f, 0.74f, 0.92f, 1.0f};
        return value;
    }();
    return tokens;
}

constexpr eui::Color kTransparent{0.0f, 0.0f, 0.0f, 0.0f};

eui::Color alpha(const eui::Color& color, float opacity) {
    return {color.r, color.g, color.b, color.a * opacity};
}

std::string shortPath(const std::string& path) {
    if (path.size() <= 58) {
        return path;
    }
    return "..." + path.substr(path.size() - 55);
}

std::string numberText(double value) {
    std::ostringstream output;
    output << std::fixed << std::setprecision(value >= 100.0 ? 0 : 2) << value;
    return output.str();
}

bool highValue(const std::string& value) {
    return value == "1" || (!value.empty() && value[0] == '1');
}

bool lowValue(const std::string& value) {
    return value == "0" || (!value.empty() && value[0] == '0');
}

eui::Color valueColor(const std::string& value) {
    if (highValue(value)) {
        return {0.30f, 0.88f, 0.62f, 1.0f};
    }
    if (lowValue(value)) {
        return {0.30f, 0.55f, 0.72f, 1.0f};
    }
    return {0.96f, 0.62f, 0.30f, 1.0f};
}

std::size_t utf8PrefixBoundary(const std::string& text, std::size_t offset) {
    offset = std::min(offset, text.size());
    while (offset > 0 && offset < text.size() &&
           (static_cast<unsigned char>(text[offset]) & 0xC0u) == 0x80u) {
        --offset;
    }
    return offset;
}

std::size_t utf8SuffixBoundary(const std::string& text, std::size_t offset) {
    offset = std::min(offset, text.size());
    while (offset < text.size() && (static_cast<unsigned char>(text[offset]) & 0xC0u) == 0x80u) {
        ++offset;
    }
    return offset;
}

std::string displayLabelText(const std::string& text, float width, float fontSize) {
    // 文本框只裁剪最终图像，TextPrimitive 仍会为全部字符生成顶点。
    // VCD 总线值和变量名可能极长，因此必须在进入渲染器前限制显示文本。
    const float estimatedGlyphWidth = std::max(1.0f, fontSize * 0.55f);
    const std::size_t limit = static_cast<std::size_t>(std::clamp(
        std::floor(width / estimatedGlyphWidth), 8.0f, 160.0f));
    if (text.size() <= limit) {
        return text;
    }

    const std::size_t visible = limit - 3;
    const std::size_t prefixEnd = utf8PrefixBoundary(text, (visible + 1) / 2);
    const std::size_t suffixStart = utf8SuffixBoundary(text, text.size() - visible / 2);
    return text.substr(0, prefixEnd) + "..." + text.substr(suffixStart);
}

void drawWaveSegment(eui::Ui& ui, const std::string& id, const eui::Vec2& from,
                     const eui::Vec2& to, const eui::Color& color, float thickness) {
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    const bool visible = std::sqrt(dx * dx + dy * dy) > 0.001f;
    const float radius = thickness * 0.5f;
    const float x = std::min(from.x, to.x) - radius;
    const float y = std::min(from.y, to.y) - radius;
    const float w = std::max(std::fabs(dx), thickness) + (std::fabs(dx) > 0.001f ? thickness : 0.0f);
    const float h = std::max(std::fabs(dy), thickness) + (std::fabs(dy) > 0.001f ? thickness : 0.0f);
    ui.rect(id).position(x, y).size(w, h).color(visible ? color : kTransparent).radius(radius).build();
}

void openVcd(ViewerState& state) {
    const eui::platform::FileDialogResult result = eui::platform::openFileDialog({
        "Open VCD waveform",
        {"vcd", "VCD"},
        {},
        "Value Change Dump",
        false
    });
    if (!result.selected()) {
        return;
    }

    vcd::Document document;
    std::string error;
    if (!vcd::loadFile(result.paths.front(), document, error)) {
        state.error = error;
        state.status = "Failed to load the selected file.";
        return;
    }
    state.document = std::move(document);
    state.selectedSignal = 0;
    state.waveScroll = 0.0f;
    state.zoom.set(0.0f);
    state.pan.set(0.0f);
    state.error.clear();
    state.status = std::to_string(state.document.signals.size()) + " signals loaded";
    if (state.document.timescale.find("binary waveform") != std::string::npos) {
        state.status += " (binary waveform import)";
    }
    ++state.generation;
}

const std::vector<std::size_t>& visibleSignalsFor(ViewerState& state) {
    const std::string& search = state.search.get();
    if (state.visibleSignalsGeneration == state.generation &&
        state.visibleSignalsSearch == search) {
        return state.visibleSignals;
    }

    state.visibleSignals.clear();
    state.visibleSignals.reserve(state.document.signals.size());
    for (std::size_t index = 0; index < state.document.signals.size(); ++index) {
        if (vcd::containsInsensitive(state.document.signals[index].name, search)) {
            state.visibleSignals.push_back(index);
        }
    }
    state.visibleSignalsSearch = search;
    state.visibleSignalsGeneration = state.generation;
    return state.visibleSignals;
}

void label(eui::Ui& ui, const std::string& id, const std::string& text,
           float x, float y, float width, float height, float size, const eui::Color& color) {
    ui.text(id).position(x, y).size(width, height).text(displayLabelText(text, width, size))
        .fontSize(size).lineHeight(size * 1.3f).color(color).build();
}

void drawWaveform(eui::Ui& ui, const vcd::Signal& signal, float width, float height,
                  std::uint64_t start, std::uint64_t end, const std::string& id) {
    const double span = static_cast<double>(std::max<std::uint64_t>(1, end - start));
    const float highY = 8.0f;
    const float lowY = height - 10.0f;
    const float unknownY = height * 0.5f;
    const auto xFor = [=](std::uint64_t time) {
        const double normalized = (static_cast<double>(time) - static_cast<double>(start)) / span;
        return static_cast<float>(std::clamp(normalized, 0.0, 1.0) * width);
    };
    const std::string& initial = vcd::valueAt(signal, start);

    const auto levelY = [](const std::string& value, float high, float low, float unknown) {
        return highValue(value) ? high : (lowValue(value) ? low : unknown);
    };
    // The runtime walks every retained primitive on each interactive frame.
    // Keep the step waveform bounded by the available pixel width instead of
    // retaining thousands of sub-pixel segments from a dense VCD trace.
    // A waveform row is repeated for every virtual-list slot. Twenty-four
    // samples are enough at this row height, while keeping scroll-time tree
    // traversal and primitive submission bounded.
    constexpr std::size_t kMaxTransitions = 24;
    const auto first = std::lower_bound(
        signal.changes.begin(), signal.changes.end(), start,
        [](const vcd::ValueChange& change, std::uint64_t time) { return change.time < time; });
    const auto last = std::upper_bound(
        first, signal.changes.end(), end,
        [](std::uint64_t time, const vcd::ValueChange& change) { return time < change.time; });
    const std::size_t changeCount = static_cast<std::size_t>(last - first);
    const std::size_t sampleCount = std::min(changeCount, kMaxTransitions);
    float previousX = 0.0f;
    float previousY = levelY(initial, highY, lowY, unknownY);
    const std::string* previousValue = &initial;
    std::size_t segment = 0;
    for (std::size_t sample = 0; sample < kMaxTransitions; ++sample) {
        if (sample < sampleCount) {
            const std::size_t index = sampleCount <= 1
                ? 0
                : (changeCount - 1) * sample / (sampleCount - 1);
            const vcd::ValueChange& change = first[index];
            const float x = xFor(change.time);
            const float nextY = levelY(change.value, highY, lowY, unknownY);
            const eui::Color color = valueColor(*previousValue);
            drawWaveSegment(ui, id + ".h." + std::to_string(segment),
                            {previousX, previousY}, {x, previousY}, color, 3.0f);
            drawWaveSegment(ui, id + ".v." + std::to_string(segment),
                            {x, previousY}, {x, nextY}, valueColor(change.value), 3.0f);
            previousX = x;
            previousY = nextY;
            previousValue = &change.value;
        } else {
            // Keep the declarative subtree shape invariant while slots are
            // rebound to signals with fewer transitions.
            drawWaveSegment(ui, id + ".h." + std::to_string(segment),
                            {previousX, previousY}, {previousX, previousY}, kTransparent, 3.0f);
            drawWaveSegment(ui, id + ".v." + std::to_string(segment),
                            {previousX, previousY}, {previousX, previousY}, kTransparent, 3.0f);
        }
        ++segment;
    }
    drawWaveSegment(ui, id + ".h.end", {previousX, previousY}, {width, previousY},
                    valueColor(*previousValue), 3.0f);

    struct ValueLabel {
        std::string text;
        float x = 0.0f;
    };
    std::array<ValueLabel, 6> valueLabels;
    if (signal.width > 1) {
        std::uint64_t lastLabelTime = start;
        int labels = 0;
        for (auto it = first; it != last && labels < 6; ++it) {
            const vcd::ValueChange& change = *it;
            if (change.time - lastLabelTime < 1) {
                continue;
            }
            const float x = xFor(change.time) + 4.0f;
            if (x < width - 42.0f) {
                valueLabels[static_cast<std::size_t>(labels)] = {change.value, x};
            }
            lastLabelTime = change.time;
            ++labels;
        }
    }
    for (std::size_t labelIndex = 0; labelIndex < valueLabels.size(); ++labelIndex) {
        const ValueLabel& valueLabel = valueLabels[labelIndex];
        label(ui, id + ".value." + std::to_string(labelIndex), valueLabel.text,
              valueLabel.x, height * 0.5f - 10.0f, 54.0f, 20.0f, 11.0f,
              alpha(theme().text, 0.82f));
    }
}

void composeTimeline(eui::Ui& ui, ViewerState& state, float width, float height) {
    const std::uint64_t total = std::max<std::uint64_t>(1, state.document.endTime);
    const double zoom = state.zoom.get();
    const std::uint64_t window = std::max<std::uint64_t>(1, static_cast<std::uint64_t>(
        std::ceil(static_cast<double>(total) * (1.0 - 0.92 * zoom))));
    const std::uint64_t maxStart = total > window ? total - window : 0;
    const std::uint64_t start = static_cast<std::uint64_t>(state.pan.get() * static_cast<float>(maxStart));
    const std::uint64_t end = std::min(total, start + window);

    const std::vector<std::size_t>& visibleSignals = visibleSignalsFor(state);

    constexpr float rulerHeight = 44.0f;
    constexpr float rowHeight = 42.0f;
    const float nameWidth = std::clamp(width * 0.22f, 170.0f, 250.0f);
    const float waveformWidth = std::max(1.0f, width - nameWidth);
    ui.rect("wave.ruler.bg").size(width, rulerHeight).color(theme().surface).build();
    ui.rect("wave.ruler.separator").position(0.0f, rulerHeight - 1.0f).size(width, 1.0f)
        .color(alpha(theme().border, 0.84f)).build();
    label(ui, "wave.ruler.signal", "SIGNAL", 16.0f, 13.0f, nameWidth - 32.0f, 18.0f,
          11.0f, alpha(theme().text, 0.54f));
    label(ui, "wave.ruler.time", "TIME · " + state.document.timescale, nameWidth + 16.0f, 13.0f,
          waveformWidth - 32.0f, 18.0f, 11.0f, alpha(theme().text, 0.54f));
    for (int tick = 0; tick <= 5; ++tick) {
        const float x = nameWidth + waveformWidth * static_cast<float>(tick) / 5.0f;
        const auto time = start + static_cast<std::uint64_t>((end - start) * tick / 5.0);
        ui.rect("wave.ruler.tick." + std::to_string(tick)).position(x, 28.0f)
            .size(1.0f, 8.0f).color(alpha(theme().border, 0.85f)).build();
        label(ui, "wave.timeline.label." + std::to_string(tick), numberText(static_cast<double>(time)),
              std::clamp(x - 22.0f, nameWidth, std::max(nameWidth, width - 48.0f)), 4.0f, 48.0f, 17.0f,
              10.0f, alpha(theme().text, 0.62f));
    }

    constexpr float horizontalScrollHeight = 34.0f;
    const float rowsHeight = std::max(0.0f, height - rulerHeight - horizontalScrollHeight);
    if (visibleSignals.empty()) {
        label(ui, "wave.rows.empty", "No signals match this filter.", 18.0f, rulerHeight + 18.0f,
              width - 36.0f, 24.0f, 14.0f, alpha(theme().text, 0.56f));
    } else {
        components::virtualList(ui, "wave.rows.virtual")
            .theme(theme()).position(0.0f, rulerHeight).size(width, rowsHeight)
            .itemCount(static_cast<std::int64_t>(visibleSignals.size()))
            .rowHeight(rowHeight).offset(state.waveScroll).step(rowHeight * 2.0f)
            .overscanViewports(0.25f)
            .onChange([&state](float value) { state.waveScroll = value; })
            .row([&](eui::Ui& rowUi, const std::string& rowId, std::int64_t rowIndex,
                     float contentWidth, float itemHeight) {
                const std::size_t row = static_cast<std::size_t>(rowIndex);
                const std::size_t index = visibleSignals[row];
                const vcd::Signal& signal = state.document.signals[index];
                const float contentNameWidth = std::min(nameWidth, contentWidth);
                const float contentWaveWidth = std::max(1.0f, contentWidth - contentNameWidth);
                const bool selected = index == state.selectedSignal;
                const eui::Color rowColor = selected ? alpha(theme().primary, 0.14f) :
                    (row % 2 == 0 ? alpha(theme().surface, 0.46f) : kTransparent);
                rowUi.stack(rowId + ".body").size(contentWidth, itemHeight).clip().content([&] {
                    rowUi.rect(rowId + ".bg").size(contentWidth, itemHeight).color(rowColor)
                        .onClick([&state, index] {
                            state.selectedSignal = index;
                            requestUpdate();
                        }).build();
                    rowUi.rect(rowId + ".name.bg").size(contentNameWidth, itemHeight)
                        .color(selected ? alpha(theme().primary, 0.10f) : alpha(theme().surface, 0.68f)).build();
                    label(rowUi, rowId + ".name", signal.name, 14.0f, 11.0f,
                          std::max(0.0f, contentNameWidth - 50.0f), 20.0f, 12.0f,
                          selected ? theme().text : alpha(theme().text, 0.78f));
                    label(rowUi, rowId + ".width", std::to_string(signal.width) + "b",
                          contentNameWidth - 34.0f, 12.0f, 24.0f, 18.0f, 10.0f,
                          alpha(theme().text, 0.42f));
                    rowUi.rect(rowId + ".divider").position(contentNameWidth, 0.0f)
                        .size(1.0f, itemHeight).color(alpha(theme().border, 0.68f)).build();
                    rowUi.stack(rowId + ".graph").position(contentNameWidth, 0.0f)
                        .size(contentWaveWidth, itemHeight).clip().content([&] {
                            for (int tick = 0; tick <= 5; ++tick) {
                                const float gridX = contentWaveWidth * static_cast<float>(tick) / 5.0f;
                                rowUi.rect(rowId + ".grid." + std::to_string(tick))
                                    .position(gridX, 0.0f).size(1.0f, itemHeight)
                                    .color(alpha(theme().border, tick == 0 ? 0.64f : 0.20f)).build();
                            }
                            drawWaveform(rowUi, signal, contentWaveWidth, itemHeight, start, end,
                                         rowId + ".wave");
                        }).build();
                    rowUi.rect(rowId + ".line").position(0.0f, itemHeight - 1.0f)
                        .size(contentWidth, 1.0f).color(alpha(theme().border, 0.30f)).build();
                }).build();
            }).build();
    }

    ui.stack("wave.horizontal.wrap").position(nameWidth, height - horizontalScrollHeight)
        .size(waveformWidth, horizontalScrollHeight).content([&] {
            components::slider(ui, "wave.horizontal").size(waveformWidth, horizontalScrollHeight)
                .theme(theme()).value(state.pan.get())
                .onChange([&state](float value) {
                    state.pan.set(value);
                    requestUpdate();
                }).build();
        }).build();
    label(ui, "wave.horizontal.label", "TIME WINDOW", 16.0f, height - horizontalScrollHeight + 7.0f,
          nameWidth - 32.0f, 18.0f, 10.0f, alpha(theme().text, 0.48f));
}

void composeViewer(eui::Ui& ui, const eui::Screen& screen) {
    ViewerState& state = ui.state<ViewerState>("vcd.viewer.state");
    const float toolbarHeight = 66.0f;
    const float sidebarWidth = std::clamp(screen.width * 0.23f, 230.0f, 330.0f);
    const float contentWidth = std::max(0.0f, screen.width - sidebarWidth);
    const float contentHeight = std::max(0.0f, screen.height - toolbarHeight);

    ui.stack("root").size(screen.width, screen.height).content([&] {
        ui.rect("background").size(screen.width, screen.height).color(theme().background).build();
        ui.rect("toolbar").size(screen.width, toolbarHeight).color(theme().surface).build();
        components::button(ui, "toolbar.open").position(18.0f, 14.0f).size(132.0f, 38.0f)
            .theme(theme()).text("Open VCD").icon(0xF07C)
            .onClick([&state] { openVcd(state); requestUpdate(); }).build();
        label(ui, "toolbar.file", state.document.path.empty() ? "No file selected" : shortPath(state.document.path),
              166.0f, 14.0f, std::max(180.0f, screen.width - 520.0f), 20.0f, 14.0f, theme().text);
        label(ui, "toolbar.status", state.error.empty() ? state.status : state.error,
              166.0f, 37.0f, std::max(180.0f, screen.width - 520.0f), 17.0f, 11.0f,
              state.error.empty() ? alpha(theme().text, 0.56f) : eui::Color{0.98f, 0.68f, 0.26f, 1.0f});

        ui.row("toolbar.zoom.group").position(screen.width - 340.0f, 0.0f).size(174.0f, toolbarHeight)
            .gap(10.0f).alignItems(eui::Align::CENTER).content([&] {
                ui.text("toolbar.zoom.label").size(38.0f, 20.0f).text("Zoom")
                    .fontSize(11.0f).lineHeight(15.0f).color(alpha(theme().text, 0.64f)).build();
                ui.stack("toolbar.zoom.wrap").size(126.0f, 30.0f).content([&] {
                    components::slider(ui, "toolbar.zoom").size(126.0f, 30.0f)
                        .theme(theme()).value(state.zoom.get())
                        .onChange([&state](float value) {
                            state.zoom.set(value);
                            requestUpdate();
                        }).build();
                }).build();
            }).build();
        components::button(ui, "toolbar.pan.left").position(screen.width - 132.0f, 14.0f).size(48.0f, 38.0f)
            .theme(theme(), false).text("<").onClick([&state] {
                state.pan.set(std::max(0.0f, state.pan.get() - 0.12f));
            }).build();
        components::button(ui, "toolbar.pan.right").position(screen.width - 78.0f, 14.0f).size(48.0f, 38.0f)
            .theme(theme(), false).text(">").onClick([&state] {
                state.pan.set(std::min(1.0f, state.pan.get() + 0.12f));
            }).build();

        ui.rect("sidebar").position(0.0f, toolbarHeight).size(sidebarWidth, contentHeight)
            .color(theme().surface).border(1.0f, alpha(theme().border, 0.75f)).build();
        label(ui, "signals.title", "Signal filter", 18.0f, toolbarHeight + 16.0f, sidebarWidth - 36.0f, 26.0f,
              20.0f, theme().text);
        components::input(ui, "signals.search").position(16.0f, toolbarHeight + 52.0f)
            .size(sidebarWidth - 32.0f, 36.0f).placeholder("Filter signals").bind(state.search).theme(theme()).build();
        const std::vector<std::size_t>& visibleSignals = visibleSignalsFor(state);
        label(ui, "signals.count", std::to_string(visibleSignals.size()) + " / " +
              std::to_string(state.document.signals.size()) + " signals", 18.0f,
              toolbarHeight + 104.0f, sidebarWidth - 36.0f, 20.0f, 12.0f, alpha(theme().text, 0.56f));
        if (!state.document.signals.empty() && state.selectedSignal < state.document.signals.size()) {
            ui.rect("signals.selected.card").position(16.0f, toolbarHeight + 144.0f)
                .size(sidebarWidth - 32.0f, 92.0f).color(alpha(theme().surfaceHover, 0.48f))
                .border(1.0f, alpha(theme().border, 0.58f)).radius(10.0f).build();
            label(ui, "signals.selected.caption", "SELECTED", 28.0f, toolbarHeight + 160.0f,
                  sidebarWidth - 56.0f, 16.0f, 10.0f, alpha(theme().text, 0.46f));
            label(ui, "signals.selected.name", state.document.signals[state.selectedSignal].name,
                  28.0f, toolbarHeight + 183.0f, sidebarWidth - 56.0f, 20.0f, 13.0f, theme().text);
            label(ui, "signals.selected.meta", std::to_string(state.document.signals[state.selectedSignal].changes.size()) +
                  " transitions", 28.0f, toolbarHeight + 211.0f, sidebarWidth - 56.0f, 16.0f,
                  11.0f, alpha(theme().text, 0.52f));
        }

        ui.stack("wave.panel").position(sidebarWidth, toolbarHeight).size(contentWidth, contentHeight)
            .content([&] {
                ui.rect("wave.panel.bg").size(contentWidth, contentHeight)
                    .color(alpha(theme().background, 0.72f)).build();
                if (state.document.signals.empty()) {
                    label(ui, "wave.empty.title", "VCD waveform viewer", 36.0f, contentHeight * 0.42f,
                          contentWidth - 72.0f, 32.0f, 24.0f, theme().text);
                    label(ui, "wave.empty.body", "Open a .vcd file to inspect signal transitions.", 38.0f,
                          contentHeight * 0.42f + 38.0f, contentWidth - 76.0f, 26.0f, 14.0f,
                          alpha(theme().text, 0.58f));
                } else {
                    composeTimeline(ui, state, contentWidth, contentHeight);
                }
            }).build();
    }).build();
}

} // namespace

void compose(eui::Ui& ui, const eui::Screen& screen) {
    composeViewer(ui, screen);
}

} // namespace app
