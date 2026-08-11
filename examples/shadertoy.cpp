#include "eui_neo.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>

namespace app {
namespace {

bool paused = false;
std::uint64_t resetKey = 0;

enum class Preset { Demo, Blackhole, Fish };

Preset selectedPreset = Preset::Demo;
std::string presetError;

std::string resourcePath(const std::string& path) {
    const std::string resolved = eui::platform::resolveResourcePath(path);
    return resolved.empty() ? path : resolved;
}

std::string demoNoisePath() {
#if defined(EUI_SHADERTOY_PRESETS_DIR)
    return resourcePath(
        (std::filesystem::u8path(EUI_SHADERTOY_PRESETS_DIR) /
         "blackhole" / "color_noise.png").u8string());
#else
    return resourcePath(
        "assets/shaders/shadertoy/blackhole/color_noise.png");
#endif
}

eui::ShaderToyGraph demoGraph() {
    eui::ShaderToyGraph graph;
    graph.addPass("image", resourcePath(EUI_SHADERTOY_DEMO_SOURCE),
                  resourcePath(EUI_SHADERTOY_DEMO_SPIRV));
    graph.setChannel(
        "image", 0,
        eui::ShaderToyChannel::image(demoNoisePath()));
    return graph;
}

const char* presetName(Preset preset) {
    switch (preset) {
    case Preset::Demo: return "Demo";
    case Preset::Blackhole: return "blackhole";
    case Preset::Fish: return "fish";
    }
    return "Demo";
}

eui::ShaderToyGraph loadPreset(Preset preset) {
    if (preset == Preset::Demo) return demoGraph();
#if defined(EUI_SHADERTOY_PRESETS_DIR)
    const std::string name = presetName(preset);
    const std::filesystem::path directory =
        std::filesystem::u8path(resourcePath(EUI_SHADERTOY_PRESETS_DIR)) /
        name;
    eui::ShaderToyGraph graph;
    eui::ShaderToyError error;
    if (!eui::loadShaderToyGraphJson(
            (directory / "config.json").u8string(), graph, error)) {
        presetError = error.message;
        return demoGraph();
    }
    presetError.clear();
    return graph;
#else
    presetError = "Built-in presets are unavailable in this build.";
    return demoGraph();
#endif
}

const eui::ShaderToyGraph& shaderGraph() {
    static std::array<eui::ShaderToyGraph, 3> graphs;
    static std::array<bool, 3> loaded{};
    const std::size_t index = static_cast<std::size_t>(selectedPreset);
    if (!loaded[index]) {
        graphs[index] = loadPreset(selectedPreset);
        loaded[index] = true;
    }
    return graphs[index];
}

void selectPreset(Preset preset) {
    if (selectedPreset == preset) return;
    selectedPreset = preset;
    ++resetKey;
}

std::string presetLabel(Preset preset) {
    const std::string name = presetName(preset);
    if (name == "blackhole") return "Blackhole";
    if (name == "fish") return "Fish";
    return name;
}

void presetButton(eui::Ui& ui, Preset preset,
                  float x, float y, float width) {
    const bool selected = selectedPreset == preset;
    const std::string id = std::string("preset.") + presetName(preset);
    const eui::Color normal = selected
        ? eui::Color{0.18f, 0.48f, 0.64f, 1.0f}
        : eui::Color{0.14f, 0.16f, 0.20f, 1.0f};
    const eui::Color hover{0.18f, 0.48f, 0.64f, 1.0f};
    ui.rect(id)
        .position(x, y)
        .size(width, 34.0f)
        .states(normal, hover, hover)
        .radius(4.0f)
        .onClick([preset] { selectPreset(preset); })
        .build();
    ui.text(id + ".label")
        .position(x, y)
        .size(width, 34.0f)
        .text(presetLabel(preset))
        .fontSize(14.0f)
        .color({0.96f, 0.97f, 0.98f, 1.0f})
        .horizontalAlign(eui::HorizontalAlign::Center)
        .verticalAlign(eui::VerticalAlign::Center)
        .build();
}

} // namespace

const DslAppConfig& dslAppConfig() {
    static const DslAppConfig config = DslAppConfig{}
        .title("Shadertoy")
        .pageId("shadertoy")
        .windowSize(960, 600)
        .clearColor({0.025f, 0.03f, 0.04f, 1.0f});
    return config;
}

void compose(eui::Ui& ui, const eui::Screen& screen) {
    const float margin = 28.0f;
    const float toolbarHeight = 88.0f;
    const float canvasWidth = std::max(1.0f, screen.width - margin * 2.0f);
    const float canvasHeight =
        std::max(1.0f, screen.height - margin * 2.0f - toolbarHeight);

    ui.shadertoy("shader")
        .position(margin, margin + toolbarHeight)
        .size(canvasWidth, canvasHeight)
        .graph(shaderGraph())
        .radius(8.0f)
        .opacity(0.96f)
        .resolutionScale(1.0f)
        .timeScale(1.0f)
        .paused(paused)
        .resetKey(resetKey)
        .onCompileError([](const eui::ShaderToyError& error) {
            std::cerr << error.sourcePath << ":" << error.line
                      << " [" << error.passName << "/" << error.stage << "] "
                      << error.message << "\n";
        })
        .build();

    const float gap = 8.0f;
    const float presetWidth = std::max(
        64.0f, (canvasWidth - gap * 2.0f) / 3.0f);

    const std::array<Preset, 3> presets{
        Preset::Demo, Preset::Blackhole, Preset::Fish};
    for (std::size_t index = 0; index < presets.size(); ++index) {
        presetButton(ui, presets[index],
                     margin + static_cast<float>(index) * (presetWidth + gap),
                     margin, presetWidth);
    }

    const float controlsY = margin + 42.0f;
    ui.rect("pause")
        .position(margin, controlsY)
        .size(96.0f, 34.0f)
        .states({0.14f, 0.16f, 0.20f, 1.0f},
                {0.20f, 0.23f, 0.28f, 1.0f},
                {0.10f, 0.12f, 0.15f, 1.0f})
        .radius(5.0f)
        .onClick([] { paused = !paused; })
        .build();
    ui.text("pause.label")
        .position(margin, controlsY)
        .size(96.0f, 34.0f)
        .text(paused ? "Resume" : "Pause")
        .fontSize(15.0f)
        .color({0.96f, 0.97f, 0.98f, 1.0f})
        .horizontalAlign(eui::HorizontalAlign::Center)
        .verticalAlign(eui::VerticalAlign::Center)
        .build();

    ui.rect("reset")
        .position(margin + 108.0f, controlsY)
        .size(96.0f, 34.0f)
        .states({0.14f, 0.16f, 0.20f, 1.0f},
                {0.20f, 0.23f, 0.28f, 1.0f},
                {0.10f, 0.12f, 0.15f, 1.0f})
        .radius(5.0f)
        .onClick([] { ++resetKey; })
        .build();
    ui.text("reset.label")
        .position(margin + 108.0f, controlsY)
        .size(96.0f, 34.0f)
        .text("Reset")
        .fontSize(15.0f)
        .color({0.96f, 0.97f, 0.98f, 1.0f})
        .horizontalAlign(eui::HorizontalAlign::Center)
        .verticalAlign(eui::VerticalAlign::Center)
        .build();

    ui.text("preset.status")
        .position(margin + 220.0f, controlsY)
        .size(std::max(1.0f, canvasWidth - 220.0f), 34.0f)
        .text(presetError.empty()
                  ? std::string("Running ") + presetLabel(selectedPreset)
                  : presetError)
        .fontSize(14.0f)
        .color(presetError.empty()
                   ? eui::Color{0.72f, 0.76f, 0.82f, 1.0f}
                   : eui::Color{1.0f, 0.42f, 0.34f, 1.0f})
        .verticalAlign(eui::VerticalAlign::Center)
        .build();
}

} // namespace app
