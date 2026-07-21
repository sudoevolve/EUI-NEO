#include "eui_neo.h"
#include "core/window/window_backend.h"
#include "designer_codegen.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace app {

const DslAppConfig& dslAppConfig() {
    static const DslAppConfig config = DslAppConfig{}
        .title("EUI-NEO Designer")
        .pageId("eui_designer")
        .clearColor({0.035f, 0.043f, 0.055f, 1.0f})
        .windowSize(1680, 960)
        .fps(90.0);
    return config;
}

namespace {

using designer::Node;
using designer::NodeType;

struct DesignerState {
    std::vector<Node> nodes;
    int selectedUid = -1;
    int nextUid = 1;
    float canvasWidth = 960.0f;
    float canvasHeight = 640.0f;
    std::string pageTitle = "My EUI Page";
    std::string pageId = "my_page";
    eui::Color backgroundColor{0.965f, 0.972f, 0.982f, 1.0f};
    bool backgroundPickerOpen = false;
    bool elementPickerOpen = false;
    int elementColorTarget = 0;
    std::string elementStyleColorKey;
    int previewOverlayUid = -1;
    float screenWidth = 0.0f;
    float screenHeight = 0.0f;
    std::string status = "Ready";
    float dragStartX = 0.0f;
    float dragStartY = 0.0f;
    float resizeStartWidth = 0.0f;
    float resizeStartHeight = 0.0f;
    bool dragging = false;
    bool layoutDirty = false;
    bool elementMenuOpen = false;
    int elementMenuUid = -1;
    float elementMenuX = 0.0f;
    float elementMenuY = 0.0f;
    eui::Signal<float> paletteScroll{0.0f};
    eui::Signal<float> elementsScroll{0.0f};
    eui::Signal<float> propertiesScroll{0.0f};
    std::vector<int> elementTreeOrder;
};

DesignerState state;
std::unordered_map<std::string, std::string> numericDrafts;
std::unordered_set<std::string> focusedNumericInputs;

struct PreviewBuildResult {
    bool launched = false;
    std::string message;
};

constexpr float kTopBarHeight = 58.0f;
constexpr float kLeftWidth = 272.0f;
constexpr float kRightWidth = 342.0f;
constexpr float kPanelPadding = 18.0f;

const eui::Color kAccent{0.16f, 0.48f, 0.94f, 1.0f};
const eui::Color kWorkspace{0.055f, 0.065f, 0.082f, 1.0f};
const eui::Color kPanel{0.075f, 0.086f, 0.106f, 1.0f};
const eui::Color kPanelRaised{0.095f, 0.108f, 0.132f, 1.0f};
const eui::Color kBorder{0.16f, 0.18f, 0.22f, 1.0f};
const eui::Color kText{0.91f, 0.93f, 0.97f, 1.0f};
const eui::Color kMuted{0.55f, 0.60f, 0.69f, 1.0f};

components::theme::ThemeColorTokens designerTheme() {
    auto tokens = components::theme::dark();
    tokens.primary = kAccent;
    tokens.background = kWorkspace;
    tokens.surface = kPanelRaised;
    tokens.surfaceHover = {0.12f, 0.14f, 0.18f, 1.0f};
    tokens.surfaceActive = {0.15f, 0.18f, 0.23f, 1.0f};
    tokens.border = kBorder;
    tokens.text = kText;
    return tokens;
}

components::theme::ThemeColorTokens canvasTheme(const eui::Color& accent) {
    auto tokens = components::theme::light();
    tokens.primary = accent;
    return tokens;
}

const eui::Color* findStyleColor(const Node& node, const std::string& name) {
    const auto found = std::find_if(node.styleColors.begin(), node.styleColors.end(), [&](const auto& entry) {
        return entry.name == name;
    });
    return found == node.styleColors.end() ? nullptr : &found->value;
}

eui::Color styleColor(const Node& node, const std::string& name, eui::Color fallback) {
    if (const eui::Color* value = findStyleColor(node, name)) return *value;
    return fallback;
}

float styleFloat(const Node& node, const std::string& name, float fallback) {
    const auto found = std::find_if(node.styleFloats.begin(), node.styleFloats.end(), [&](const auto& entry) {
        return entry.name == name;
    });
    return found == node.styleFloats.end() ? fallback : found->value;
}

std::string styleString(const Node& node, const std::string& name, const std::string& fallback) {
    const auto found = std::find_if(node.styleStrings.begin(), node.styleStrings.end(), [&](const auto& entry) {
        return entry.name == name;
    });
    return found == node.styleStrings.end() ? fallback : found->value;
}

void setStyleColor(Node& node, std::string name, eui::Color value) {
    for (auto& entry : node.styleColors) {
        if (entry.name == name) { entry.value = value; return; }
    }
    node.styleColors.push_back({std::move(name), value});
}

void setStyleFloat(Node& node, std::string name, float value) {
    for (auto& entry : node.styleFloats) {
        if (entry.name == name) { entry.value = value; return; }
    }
    node.styleFloats.push_back({std::move(name), value});
}

void setStyleString(Node& node, std::string name, std::string value) {
    for (auto& entry : node.styleStrings) {
        if (entry.name == name) { entry.value = std::move(value); return; }
    }
    node.styleStrings.push_back({std::move(name), std::move(value)});
}

template <typename Style>
Style applyStyleColors(const Node& node, Style style,
                       std::initializer_list<std::pair<const char*, eui::Color Style::*>> fields) {
    for (const auto& [name, member] : fields) {
        if (const eui::Color* value = findStyleColor(node, name)) style.*member = *value;
    }
    return style;
}

template <typename Style>
Style applyStyleFloats(const Node& node, Style style,
                       std::initializer_list<std::pair<const char*, float Style::*>> fields) {
    for (const auto& [name, member] : fields) style.*member = styleFloat(node, name, style.*member);
    return style;
}

template <typename Style>
Style applyStyleStrings(const Node& node, Style style,
                        std::initializer_list<std::pair<const char*, std::string Style::*>> fields) {
    for (const auto& [name, member] : fields) style.*member = styleString(node, name, style.*member);
    return style;
}

template <typename Style>
Style applyStyleShadow(const Node& node, Style style, core::Shadow Style::* member) {
    if (node.componentShadowOverride) {
        style.*member = {node.shadowEnabled, {node.shadowOffsetX, node.shadowOffsetY},
                         node.shadowBlur, node.shadowSpread, node.shadowColor, node.insetShadow};
    }
    return style;
}

struct StyleColorField { std::string key; std::string label; eui::Color value; };
struct StyleFloatField { std::string key; std::string label; float value; };
struct StyleStringField { std::string key; std::string label; std::string value; };
struct ComponentStyleFields {
    std::vector<StyleColorField> colors;
    std::vector<StyleFloatField> floats;
    std::vector<StyleStringField> strings;
    bool hasShadow = false;
};

ComponentStyleFields componentStyleFields(const Node& node) {
    ComponentStyleFields result;
    const auto theme = canvasTheme(node.color);
#define EUI_STYLE_COLOR(field, labelText) result.colors.push_back({#field, labelText, style.field})
#define EUI_STYLE_FLOAT(keyText, field, labelText) result.floats.push_back({keyText, labelText, style.field})
    switch (node.type) {
    case NodeType::Button: {
        components::ButtonStyle style(canvasTheme(node.color));
        EUI_STYLE_COLOR(normal, "Normal"); EUI_STYLE_COLOR(hover, "Hover"); EUI_STYLE_COLOR(pressed, "Pressed");
        EUI_STYLE_COLOR(text, "Text"); EUI_STYLE_COLOR(icon, "Icon"); break;
    }
    case NodeType::Scroll: case NodeType::ScrollView: case NodeType::VirtualList: {
        components::ScrollStyle style(theme);
        EUI_STYLE_COLOR(track, "Track"); EUI_STYLE_COLOR(thumb, "Thumb");
        EUI_STYLE_COLOR(thumbHover, "Thumb hover"); EUI_STYLE_COLOR(thumbPressed, "Thumb pressed");
        EUI_STYLE_FLOAT("scrollRadius", radius, "Thumb radius"); break;
    }
    case NodeType::Input: {
        components::InputStyle style(theme);
        EUI_STYLE_COLOR(background, "Background"); EUI_STYLE_COLOR(focused, "Focused background");
        EUI_STYLE_COLOR(border, "Border"); EUI_STYLE_COLOR(focusBorder, "Focus border");
        EUI_STYLE_COLOR(text, "Text"); EUI_STYLE_COLOR(placeholder, "Placeholder"); EUI_STYLE_COLOR(cursor, "Cursor");
        EUI_STYLE_FLOAT("styleRadius", radius, "Style radius"); result.hasShadow = true; break;
    }
    case NodeType::Checkbox: {
        components::CheckboxStyle style(theme);
        EUI_STYLE_COLOR(box, "Box"); EUI_STYLE_COLOR(boxHover, "Box hover"); EUI_STYLE_COLOR(boxPressed, "Box pressed");
        EUI_STYLE_COLOR(checked, "Checked"); EUI_STYLE_COLOR(checkedHover, "Checked hover"); EUI_STYLE_COLOR(checkedPressed, "Checked pressed");
        EUI_STYLE_COLOR(border, "Border"); EUI_STYLE_COLOR(mark, "Mark"); EUI_STYLE_COLOR(text, "Text");
        EUI_STYLE_COLOR(rowHover, "Row hover"); EUI_STYLE_COLOR(rowPressed, "Row pressed");
        EUI_STYLE_FLOAT("styleRadius", radius, "Style radius"); break;
    }
    case NodeType::Radio: {
        components::RadioStyle style(theme);
        EUI_STYLE_COLOR(outer, "Outer"); EUI_STYLE_COLOR(outerHover, "Outer hover"); EUI_STYLE_COLOR(selected, "Selected");
        EUI_STYLE_COLOR(border, "Border"); EUI_STYLE_COLOR(text, "Text"); EUI_STYLE_COLOR(rowHover, "Row hover");
        EUI_STYLE_COLOR(rowPressed, "Row pressed"); break;
    }
    case NodeType::Switch: {
        components::SwitchStyle style(theme);
        EUI_STYLE_COLOR(off, "Off"); EUI_STYLE_COLOR(on, "On"); EUI_STYLE_COLOR(knob, "Knob"); EUI_STYLE_COLOR(text, "Text");
        EUI_STYLE_COLOR(rowHover, "Row hover"); EUI_STYLE_COLOR(rowPressed, "Row pressed"); break;
    }
    case NodeType::Slider: {
        components::SliderStyle style(theme); EUI_STYLE_COLOR(track, "Track"); EUI_STYLE_COLOR(fill, "Fill"); EUI_STYLE_COLOR(knob, "Knob"); break;
    }
    case NodeType::Progress: {
        components::ProgressStyle style(theme); EUI_STYLE_COLOR(track, "Track"); EUI_STYLE_COLOR(fill, "Fill"); break;
    }
    case NodeType::Segmented: {
        components::SegmentedStyle style(theme);
        EUI_STYLE_COLOR(background, "Background"); EUI_STYLE_COLOR(hover, "Hover"); EUI_STYLE_COLOR(selected, "Selected");
        EUI_STYLE_COLOR(text, "Text"); EUI_STYLE_COLOR(selectedText, "Selected text"); EUI_STYLE_COLOR(border, "Border"); break;
    }
    case NodeType::Stepper: {
        components::StepperStyle style(theme);
        EUI_STYLE_COLOR(button, "Button"); EUI_STYLE_COLOR(buttonHover, "Button hover"); EUI_STYLE_COLOR(buttonPressed, "Button pressed");
        EUI_STYLE_COLOR(buttonDisabled, "Button disabled"); EUI_STYLE_COLOR(field, "Field"); EUI_STYLE_COLOR(fieldBorder, "Field border");
        EUI_STYLE_COLOR(text, "Text"); EUI_STYLE_COLOR(mutedText, "Muted text"); EUI_STYLE_COLOR(accent, "Accent");
        EUI_STYLE_FLOAT("styleRadius", radius, "Style radius"); result.hasShadow = true; break;
    }
    case NodeType::Tabs: {
        components::TabsStyle style(theme); EUI_STYLE_COLOR(text, "Text"); EUI_STYLE_COLOR(hover, "Hover");
        EUI_STYLE_COLOR(selectedText, "Selected text"); EUI_STYLE_COLOR(indicator, "Indicator"); EUI_STYLE_COLOR(border, "Border"); break;
    }
    case NodeType::Dropdown: {
        components::DropdownStyle style(theme);
        EUI_STYLE_COLOR(field, "Field"); EUI_STYLE_COLOR(fieldHover, "Field hover"); EUI_STYLE_COLOR(fieldPressed, "Field pressed");
        EUI_STYLE_COLOR(popup, "Popup"); EUI_STYLE_COLOR(optionHover, "Option hover"); EUI_STYLE_COLOR(optionPressed, "Option pressed");
        EUI_STYLE_COLOR(selected, "Selected"); EUI_STYLE_COLOR(text, "Text"); EUI_STYLE_COLOR(mutedText, "Muted text");
        EUI_STYLE_COLOR(accent, "Accent"); EUI_STYLE_COLOR(border, "Border"); EUI_STYLE_FLOAT("styleRadius", radius, "Style radius");
        result.hasShadow = true; break;
    }
    case NodeType::DataTable: {
        components::DataTableStyle style(theme);
        EUI_STYLE_COLOR(background, "Background"); EUI_STYLE_COLOR(header, "Header"); EUI_STYLE_COLOR(row, "Row");
        EUI_STYLE_COLOR(rowAlt, "Alternate row"); EUI_STYLE_COLOR(rowHover, "Row hover"); EUI_STYLE_COLOR(text, "Text");
        EUI_STYLE_COLOR(mutedText, "Muted text"); EUI_STYLE_COLOR(accent, "Accent"); EUI_STYLE_COLOR(border, "Border");
        EUI_STYLE_COLOR(divider, "Divider"); EUI_STYLE_FLOAT("styleRadius", radius, "Style radius"); break;
    }
    case NodeType::LineChart: {
        components::LineChartStyle style(theme);
        EUI_STYLE_COLOR(background, "Background"); EUI_STYLE_COLOR(title, "Title"); EUI_STYLE_COLOR(label, "Label"); EUI_STYLE_COLOR(grid, "Grid");
        EUI_STYLE_COLOR(line, "Line"); EUI_STYLE_COLOR(point, "Point"); EUI_STYLE_COLOR(pointHover, "Point hover");
        EUI_STYLE_COLOR(pointPressed, "Point pressed"); EUI_STYLE_COLOR(tooltipBackground, "Tooltip background");
        EUI_STYLE_COLOR(tooltipText, "Tooltip text"); EUI_STYLE_COLOR(border, "Border"); EUI_STYLE_FLOAT("styleRadius", radius, "Style radius");
        result.hasShadow = true; break;
    }
    case NodeType::BarChart: {
        components::BarChartStyle style(theme);
        EUI_STYLE_COLOR(background, "Background"); EUI_STYLE_COLOR(title, "Title"); EUI_STYLE_COLOR(label, "Label"); EUI_STYLE_COLOR(grid, "Grid");
        EUI_STYLE_COLOR(tooltipBackground, "Tooltip background"); EUI_STYLE_COLOR(tooltipText, "Tooltip text"); EUI_STYLE_COLOR(border, "Border");
        EUI_STYLE_FLOAT("styleRadius", radius, "Style radius"); result.hasShadow = true; break;
    }
    case NodeType::PieChart: {
        components::PieChartStyle style(theme);
        EUI_STYLE_COLOR(background, "Background"); EUI_STYLE_COLOR(title, "Title"); EUI_STYLE_COLOR(tooltipBackground, "Tooltip background");
        EUI_STYLE_COLOR(tooltipText, "Tooltip text"); EUI_STYLE_COLOR(border, "Border"); EUI_STYLE_FLOAT("styleRadius", radius, "Style radius");
        result.hasShadow = true; break;
    }
    case NodeType::Carousel: {
        components::CarouselStyle style(theme);
        EUI_STYLE_COLOR(background, "Background"); EUI_STYLE_COLOR(border, "Border"); EUI_STYLE_COLOR(text, "Text");
        EUI_STYLE_COLOR(mutedText, "Muted text"); EUI_STYLE_COLOR(overlayTop, "Overlay top"); EUI_STYLE_COLOR(overlayBottom, "Overlay bottom");
        EUI_STYLE_COLOR(indicator, "Indicator"); EUI_STYLE_COLOR(activeIndicator, "Active indicator");
        EUI_STYLE_FLOAT("styleRadius", radius, "Style radius"); result.hasShadow = true; break;
    }
    case NodeType::Markdown: {
        components::MarkdownStyle style(theme);
        EUI_STYLE_COLOR(text, "Text"); EUI_STYLE_COLOR(heading, "Heading"); EUI_STYLE_COLOR(muted, "Muted"); EUI_STYLE_COLOR(accent, "Accent");
        EUI_STYLE_COLOR(codeText, "Code text"); EUI_STYLE_COLOR(codeBackground, "Code background");
        EUI_STYLE_COLOR(quoteBackground, "Quote background"); EUI_STYLE_COLOR(divider, "Divider");
        EUI_STYLE_FLOAT("bodySize", bodySize, "Body size"); EUI_STYLE_FLOAT("bodyLineHeight", bodyLineHeight, "Body line height");
        EUI_STYLE_FLOAT("h1Size", h1Size, "H1 size"); EUI_STYLE_FLOAT("h2Size", h2Size, "H2 size"); EUI_STYLE_FLOAT("h3Size", h3Size, "H3 size");
        EUI_STYLE_FLOAT("codeSize", codeSize, "Code size"); EUI_STYLE_FLOAT("blockGap", blockGap, "Block gap");
        EUI_STYLE_FLOAT("listIndent", listIndent, "List indent"); EUI_STYLE_FLOAT("codePadding", codePadding, "Code padding");
        EUI_STYLE_FLOAT("quotePadding", quotePadding, "Quote padding"); EUI_STYLE_FLOAT("tableCellPadding", tableCellPadding, "Cell padding");
        EUI_STYLE_FLOAT("styleRadius", radius, "Style radius");
        result.strings.push_back({"fontFamily", "Font family", style.fontFamily});
        result.strings.push_back({"codeFontFamily", "Code font family", style.codeFontFamily}); break;
    }
    case NodeType::DatePicker: {
        components::DatePickerStyle style(theme);
        EUI_STYLE_COLOR(backdrop, "Backdrop"); EUI_STYLE_COLOR(surface, "Surface"); EUI_STYLE_COLOR(column, "Column");
        EUI_STYLE_COLOR(selected, "Selected"); EUI_STYLE_COLOR(text, "Text"); EUI_STYLE_COLOR(mutedText, "Muted text");
        EUI_STYLE_COLOR(accent, "Accent"); EUI_STYLE_COLOR(border, "Border"); EUI_STYLE_FLOAT("styleRadius", radius, "Style radius"); result.hasShadow = true; break;
    }
    case NodeType::TimePicker: {
        components::TimePickerStyle style(theme);
        EUI_STYLE_COLOR(backdrop, "Backdrop"); EUI_STYLE_COLOR(surface, "Surface"); EUI_STYLE_COLOR(column, "Column");
        EUI_STYLE_COLOR(selected, "Selected"); EUI_STYLE_COLOR(text, "Text"); EUI_STYLE_COLOR(mutedText, "Muted text");
        EUI_STYLE_COLOR(accent, "Accent"); EUI_STYLE_COLOR(border, "Border"); EUI_STYLE_FLOAT("styleRadius", radius, "Style radius"); result.hasShadow = true; break;
    }
    case NodeType::ColorPicker: {
        components::ColorPickerStyle style(theme);
        EUI_STYLE_COLOR(backdrop, "Backdrop"); EUI_STYLE_COLOR(surface, "Surface"); EUI_STYLE_COLOR(track, "Track");
        EUI_STYLE_COLOR(text, "Text"); EUI_STYLE_COLOR(mutedText, "Muted text"); EUI_STYLE_COLOR(accent, "Accent");
        EUI_STYLE_COLOR(border, "Border"); EUI_STYLE_COLOR(knob, "Knob"); EUI_STYLE_FLOAT("styleRadius", radius, "Style radius"); result.hasShadow = true; break;
    }
    case NodeType::Dialog: {
        components::DialogStyle style(theme);
        EUI_STYLE_COLOR(backdrop, "Backdrop"); EUI_STYLE_COLOR(surface, "Surface"); EUI_STYLE_COLOR(border, "Border");
        EUI_STYLE_COLOR(title, "Title"); EUI_STYLE_COLOR(message, "Message"); EUI_STYLE_COLOR(primary, "Primary"); EUI_STYLE_COLOR(secondary, "Secondary");
        EUI_STYLE_COLOR(primaryHover, "Primary hover"); EUI_STYLE_COLOR(primaryPressed, "Primary pressed");
        EUI_STYLE_COLOR(secondaryHover, "Secondary hover"); EUI_STYLE_COLOR(secondaryPressed, "Secondary pressed");
        EUI_STYLE_FLOAT("styleRadius", radius, "Style radius"); result.hasShadow = true; break;
    }
    case NodeType::Toast: {
        components::ToastStyle style(theme);
        EUI_STYLE_COLOR(background, "Background"); EUI_STYLE_COLOR(border, "Border"); EUI_STYLE_COLOR(text, "Text");
        EUI_STYLE_COLOR(mutedText, "Muted text"); EUI_STYLE_COLOR(accent, "Accent"); EUI_STYLE_FLOAT("styleRadius", radius, "Style radius"); result.hasShadow = true; break;
    }
    case NodeType::Tooltip: {
        components::TooltipStyle style(theme);
        EUI_STYLE_COLOR(background, "Background"); EUI_STYLE_COLOR(text, "Text"); EUI_STYLE_COLOR(border, "Border");
        EUI_STYLE_FLOAT("styleRadius", radius, "Style radius"); result.hasShadow = true; break;
    }
    case NodeType::ContextMenu: {
        components::ContextMenuStyle style(theme);
        EUI_STYLE_COLOR(background, "Background"); EUI_STYLE_COLOR(hover, "Hover"); EUI_STYLE_COLOR(pressed, "Pressed");
        EUI_STYLE_COLOR(text, "Text"); EUI_STYLE_COLOR(mutedText, "Muted text"); EUI_STYLE_COLOR(border, "Border");
        EUI_STYLE_FLOAT("styleRadius", radius, "Style radius"); result.hasShadow = true; break;
    }
    case NodeType::Image: {
        components::ImageStyle style(theme);
        EUI_STYLE_COLOR(tint, "Tint"); break;
    }
    case NodeType::HeartSwitch: {
        const auto style = components::workshop::heartSwitchStyle(theme);
        EUI_STYLE_COLOR(heart, "Heart"); EUI_STYLE_COLOR(hover, "Hover"); EUI_STYLE_COLOR(pressed, "Pressed"); break;
    }
    case NodeType::NeumorphicButton: {
        const auto style = components::workshop::neumorphicButtonStyle(theme);
        EUI_STYLE_COLOR(surface, "Surface"); EUI_STYLE_COLOR(hover, "Hover"); EUI_STYLE_COLOR(pressed, "Pressed");
        EUI_STYLE_COLOR(text, "Text"); EUI_STYLE_COLOR(pressedText, "Pressed text"); EUI_STYLE_COLOR(border, "Border");
        EUI_STYLE_COLOR(darkShadow, "Dark shadow"); EUI_STYLE_COLOR(lightShadow, "Light shadow");
        EUI_STYLE_COLOR(innerDark, "Inner dark"); EUI_STYLE_COLOR(innerLight, "Inner light"); break;
    }
    case NodeType::TiltCard: {
        const auto style = components::workshop::tiltCardStyle(theme);
        EUI_STYLE_COLOR(surface, "Surface"); EUI_STYLE_COLOR(surfaceTop, "Surface top"); EUI_STYLE_COLOR(accent, "Accent");
        EUI_STYLE_COLOR(text, "Text"); EUI_STYLE_COLOR(muted, "Muted"); EUI_STYLE_COLOR(border, "Border"); EUI_STYLE_COLOR(shadow, "Shadow");
        EUI_STYLE_FLOAT("styleRadius", radius, "Style radius"); break;
    }
    case NodeType::CardSlider: {
        components::workshop::CardSliderStyle style(theme);
        EUI_STYLE_COLOR(background, "Background"); EUI_STYLE_COLOR(overlay, "Overlay"); EUI_STYLE_COLOR(title, "Title");
        EUI_STYLE_COLOR(subtitle, "Subtitle"); EUI_STYLE_COLOR(description, "Description"); EUI_STYLE_COLOR(accent, "Accent");
        EUI_STYLE_COLOR(shadow, "Shadow"); EUI_STYLE_FLOAT("styleRadius", radius, "Style radius");
        result.colors.push_back({"button.normal", "Action normal", style.button.normal});
        result.colors.push_back({"button.hover", "Action hover", style.button.hover});
        result.colors.push_back({"button.pressed", "Action pressed", style.button.pressed});
        result.colors.push_back({"button.text", "Action text", style.button.text});
        result.colors.push_back({"button.icon", "Action icon", style.button.icon});
        result.colors.push_back({"button.border.color", "Action border", style.button.border.color});
        result.floats.push_back({"button.border.width", "Action border width", style.button.border.width});
        result.floats.push_back({"button.radius", "Action radius", style.button.radius});
        result.floats.push_back({"button.opacity", "Action opacity", style.button.opacity});
        result.floats.push_back({"button.pressScale", "Action press scale", style.button.pressScale});
        result.hasShadow = true;
        break;
    }
    default: break;
    }
#undef EUI_STYLE_FLOAT
#undef EUI_STYLE_COLOR
    for (auto& field : result.colors) if (const eui::Color* value = findStyleColor(node, field.key)) field.value = *value;
    for (auto& field : result.floats) field.value = styleFloat(node, field.key, field.value);
    for (auto& field : result.strings) field.value = styleString(node, field.key, field.value);
    return result;
}

std::vector<eui::Color> colorPresets() {
    return {
        {0.0f, 0.0f, 0.0f, 1.0f},
        {1.0f, 1.0f, 1.0f, 1.0f},
        {0.16f, 0.48f, 0.94f, 1.0f},
        {0.12f, 0.68f, 0.50f, 1.0f},
        {0.95f, 0.65f, 0.12f, 1.0f},
        {0.90f, 0.24f, 0.42f, 1.0f},
        {0.63f, 0.34f, 0.92f, 1.0f}
    };
}

eui::Transition motion() {
    return eui::Transition::make(0.14f, eui::Ease::OutCubic);
}

bool parseInteger(const std::string& text, long long& value) {
    if (text.empty()) return false;
    errno = 0;
    char* end = nullptr;
    const char* begin = text.c_str();
    const long long parsed = std::strtoll(begin, &end, 10);
    if (errno == ERANGE || end == begin || end == nullptr || *end != '\0') return false;
    value = parsed;
    return true;
}

bool parseNumber(const std::string& text, float& value) {
    if (text.empty()) return false;
    errno = 0;
    char* end = nullptr;
    const float parsed = std::strtof(text.c_str(), &end);
    if (errno == ERANGE || end == text.c_str() || end == nullptr || *end != '\0' || !std::isfinite(parsed)) return false;
    value = parsed;
    return true;
}

std::string editableNumber(float value) {
    std::ostringstream output;
    output << value;
    return output.str();
}

void numberInput(eui::Ui& ui, const std::string& id, float x, float y, float width, float height,
                 float value, float minValue, float maxValue, const std::function<void(float)>& onChange) {
    if (focusedNumericInputs.find(id) == focusedNumericInputs.end()) numericDrafts[id] = editableNumber(value);
    components::input(ui, id).position(x, y).size(width, height).theme(designerTheme())
        .value(numericDrafts[id])
        .onChange([id, minValue, maxValue, onChange](const std::string& text) {
            numericDrafts[id] = text;
            float parsed = 0.0f;
            if (parseNumber(text, parsed)) onChange(std::clamp(parsed, minValue, maxValue));
        })
        .onFocus([id](bool focused) {
            if (focused) focusedNumericInputs.insert(id);
            else {
                focusedNumericInputs.erase(id);
                numericDrafts.erase(id);
            }
        }).build();
}

std::vector<std::string> splitList(const std::string& value) {
    std::vector<std::string> items;
    std::size_t start = 0;
    while (start <= value.size()) {
        const std::size_t end = value.find(',', start);
        std::string item = value.substr(start, end == std::string::npos ? std::string::npos : end - start);
        const auto first = std::find_if_not(item.begin(), item.end(), [](unsigned char ch) { return std::isspace(ch) != 0; });
        const auto last = std::find_if_not(item.rbegin(), item.rend(), [](unsigned char ch) { return std::isspace(ch) != 0; }).base();
        if (first < last) items.emplace_back(first, last);
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return items;
}

std::vector<float> splitValues(const std::string& value) {
    std::vector<float> values;
    for (const std::string& item : splitList(value)) {
        float parsed = 0.0f;
        if (parseNumber(item, parsed)) values.push_back(parsed);
    }
    return values;
}

std::vector<components::CarouselItem> carouselItems(const Node& node) {
    std::vector<components::CarouselItem> result;
    for (const std::string& item : splitList(node.itemsText)) result.push_back({"", item, "Carousel item"});
    return result;
}

std::vector<components::workshop::CardSliderItem> cardSliderItems(const Node& node) {
    std::vector<components::workshop::CardSliderItem> result;
    for (const std::string& item : splitList(node.itemsText)) result.push_back({"", item, "Card slider", "Preview"});
    return result;
}

std::vector<components::NavbarItem> navbarItems(const Node& node) {
    std::vector<components::NavbarItem> result;
    const std::vector<std::string> items = splitList(node.itemsText);
    for (std::size_t index = 0; index < items.size(); ++index) {
        result.push_back({"item_" + std::to_string(index), items[index], node.itemIcon, static_cast<int>(index)});
    }
    return result;
}

std::vector<std::vector<std::string>> tableRows(const Node& node) {
    const std::size_t columns = std::max<std::size_t>(1, splitList(node.labelsText).size());
    const std::vector<std::string> cells = splitList(node.itemsText);
    std::vector<std::vector<std::string>> rows;
    for (std::size_t offset = 0; offset < cells.size(); offset += columns) {
        std::vector<std::string> row;
        for (std::size_t column = 0; column < columns; ++column) {
            row.push_back(offset + column < cells.size() ? cells[offset + column] : std::string{});
        }
        rows.push_back(std::move(row));
    }
    return rows;
}

void numericInput(eui::Ui& ui, const std::string& id, float x, float y, float width, float height,
                  long long value, long long minValue, long long maxValue,
                  const std::function<void(long long)>& onChange) {
    if (focusedNumericInputs.find(id) == focusedNumericInputs.end()) {
        numericDrafts[id] = std::to_string(value);
    }
    components::input(ui, id).position(x, y).size(width, height).theme(designerTheme())
        .value(numericDrafts[id])
        .onChange([id, minValue, maxValue, onChange](const std::string& text) {
            numericDrafts[id] = text;
            long long parsed = 0;
            if (parseInteger(text, parsed)) onChange(std::clamp(parsed, minValue, maxValue));
        })
        .onFocus([id](bool focused) {
            if (focused) focusedNumericInputs.insert(id);
            else {
                focusedNumericInputs.erase(id);
                numericDrafts.erase(id);
            }
        }).build();
}

const char* typeName(NodeType type) {
    switch (type) {
    case NodeType::Row: return "Row layout";
    case NodeType::Column: return "Column layout";
    case NodeType::Stack: return "Stack layout";
    case NodeType::Scroll: return "Scroll layout";
    case NodeType::ScrollView: return "Scroll view";
    case NodeType::VirtualList: return "Virtual list";
    case NodeType::Rect: return "Rectangle";
    case NodeType::Polygon: return "Polygon";
    case NodeType::Svg: return "SVG";
    case NodeType::Panel: return "Panel";
    case NodeType::Card: return "Card";
    case NodeType::Text: return "Text";
    case NodeType::Markdown: return "Markdown";
    case NodeType::Button: return "Button";
    case NodeType::Input: return "Input";
    case NodeType::Checkbox: return "Checkbox";
    case NodeType::Radio: return "Radio";
    case NodeType::Switch: return "Switch";
    case NodeType::Slider: return "Slider";
    case NodeType::Progress: return "Progress";
    case NodeType::Segmented: return "Segmented";
    case NodeType::Stepper: return "Stepper";
    case NodeType::Tabs: return "Tabs";
    case NodeType::Dropdown: return "Dropdown";
    case NodeType::DatePicker: return "Date picker";
    case NodeType::TimePicker: return "Time picker";
    case NodeType::ColorPicker: return "Color picker";
    case NodeType::DataTable: return "Data table";
    case NodeType::Dialog: return "Dialog";
    case NodeType::Sidebar: return "Sidebar";
    case NodeType::Navbar: return "Navbar";
    case NodeType::Toast: return "Toast";
    case NodeType::Tooltip: return "Tooltip";
    case NodeType::ContextMenu: return "Context menu";
    case NodeType::Carousel: return "Carousel";
    case NodeType::LineChart: return "Line chart";
    case NodeType::BarChart: return "Bar chart";
    case NodeType::PieChart: return "Pie chart";
    case NodeType::MouseArea: return "Mouse area";
    case NodeType::Image: return "Image";
    case NodeType::HeartSwitch: return "Heart switch";
    case NodeType::NeumorphicButton: return "Neumorphic button";
    case NodeType::CardSlider: return "Card slider";
    case NodeType::TiltCard: return "Tilt card";
    }
    return "Element";
}

unsigned int typeIcon(NodeType type) {
    switch (type) {
    case NodeType::Row: return 0xF58D;
    case NodeType::Column: return 0xF58E;
    case NodeType::Stack: return 0xF5FD;
    case NodeType::Rect: return 0xF0C8;
    case NodeType::Polygon: return 0xF5EE;
    case NodeType::Svg: return 0xF1C9;
    case NodeType::Image: return 0xF03E;
    case NodeType::Panel: return 0xF2D0;
    case NodeType::Card: return 0xF2BB;
    case NodeType::Text: return 0xF031;
    case NodeType::Markdown: return 0xF036;
    case NodeType::Button: return 0xF0A6;
    case NodeType::Input: return 0xF11C;
    case NodeType::Checkbox: return 0xF14A;
    case NodeType::Radio: return 0xF192;
    case NodeType::Switch: return 0xF205;
    case NodeType::Slider: return 0xF1DE;
    case NodeType::Progress: return 0xF110;
    case NodeType::Scroll:
    case NodeType::ScrollView: return 0xF338;
    case NodeType::VirtualList: return 0xF03A;
    case NodeType::LineChart: return 0xF201;
    case NodeType::BarChart: return 0xF080;
    case NodeType::PieChart: return 0xF200;
    }
    return 0xF1B2;
}

std::string defaultText(NodeType type) {
    switch (type) {
    case NodeType::Text: return "Heading text";
    case NodeType::Button: return "Continue";
    case NodeType::Input: return "Type here";
    case NodeType::Checkbox: return "Remember choice";
    case NodeType::Switch: return "Notifications";
    case NodeType::Image: return "assets/icon.png";
    case NodeType::Svg: return "assets/icon.svg";
    case NodeType::Markdown: return "# Markdown\nDesigner content";
    default: return typeName(type);
    }
}

void defaultSize(NodeType type, float& width, float& height) {
    switch (type) {
    case NodeType::Row: width = 520.0f; height = 160.0f; break;
    case NodeType::Column: width = 220.0f; height = 520.0f; break;
    case NodeType::Stack:
    case NodeType::Scroll:
    case NodeType::ScrollView:
    case NodeType::VirtualList: width = 320.0f; height = 240.0f; break;
    case NodeType::Rect:
    case NodeType::Polygon:
    case NodeType::Svg: width = 180.0f; height = 120.0f; break;
    case NodeType::Panel: width = 320.0f; height = 180.0f; break;
    case NodeType::Card: width = 320.0f; height = 180.0f; break;
    case NodeType::Text: width = 260.0f; height = 44.0f; break;
    case NodeType::Markdown: width = 360.0f; height = 120.0f; break;
    case NodeType::Button: width = 180.0f; height = 54.0f; break;
    case NodeType::Input: width = 300.0f; height = 44.0f; break;
    case NodeType::Checkbox:
    case NodeType::Radio: width = 220.0f; height = 30.0f; break;
    case NodeType::Switch: width = 220.0f; height = 32.0f; break;
    case NodeType::Slider: width = 300.0f; height = 32.0f; break;
    case NodeType::Progress: width = 300.0f; height = 16.0f; break;
    case NodeType::Segmented: width = 300.0f; height = 38.0f; break;
    case NodeType::Stepper: width = 180.0f; height = 40.0f; break;
    case NodeType::Tabs: width = 300.0f; height = 42.0f; break;
    case NodeType::Dropdown: width = 260.0f; height = 44.0f; break;
    case NodeType::DatePicker:
    case NodeType::TimePicker: width = 190.0f; height = 44.0f; break;
    case NodeType::ColorPicker: width = 190.0f; height = 44.0f; break;
    case NodeType::DataTable: width = 520.0f; height = 174.0f; break;
    case NodeType::Dialog:
    case NodeType::Sidebar: width = 180.0f; height = 54.0f; break;
    case NodeType::Navbar: width = 300.0f; height = 620.0f; break;
    case NodeType::Toast: width = 180.0f; height = 54.0f; break;
    case NodeType::Tooltip: width = 180.0f; height = 40.0f; break;
    case NodeType::ContextMenu: width = 240.0f; height = 44.0f; break;
    case NodeType::Carousel:
    case NodeType::CardSlider: width = 520.0f; height = 320.0f; break;
    case NodeType::LineChart:
    case NodeType::BarChart:
    case NodeType::PieChart: width = 360.0f; height = 220.0f; break;
    case NodeType::Image: width = 220.0f; height = 150.0f; break;
    case NodeType::MouseArea: width = 180.0f; height = 120.0f; break;
    case NodeType::HeartSwitch: width = 84.0f; height = 44.0f; break;
    case NodeType::NeumorphicButton: width = 180.0f; height = 54.0f; break;
    case NodeType::TiltCard: width = 320.0f; height = 220.0f; break;
    default: width = 280.0f; height = 96.0f; break;
    }
}

Node* selectedNode() {
    const auto found = std::find_if(state.nodes.begin(), state.nodes.end(), [](const Node& node) {
        return node.uid == state.selectedUid;
    });
    return found == state.nodes.end() ? nullptr : &*found;
}

bool isLayout(NodeType type) {
    return type == NodeType::Row || type == NodeType::Column || type == NodeType::Stack ||
           type == NodeType::Scroll || type == NodeType::ScrollView;
}

bool isOverlayPreview(NodeType type) {
    return type == NodeType::DatePicker || type == NodeType::TimePicker || type == NodeType::ColorPicker ||
           type == NodeType::Dialog || type == NodeType::Toast || type == NodeType::ContextMenu ||
           type == NodeType::Sidebar;
}

bool hasTextApi(NodeType type) {
    return type == NodeType::Text || type == NodeType::Markdown || type == NodeType::Button ||
           type == NodeType::Input || type == NodeType::Checkbox || type == NodeType::Radio ||
           type == NodeType::Switch || type == NodeType::Image || type == NodeType::Svg ||
           type == NodeType::NeumorphicButton || type == NodeType::TiltCard ||
           type == NodeType::LineChart || type == NodeType::BarChart || type == NodeType::PieChart ||
           type == NodeType::Dialog || type == NodeType::Toast || type == NodeType::Navbar || type == NodeType::Sidebar ||
           type == NodeType::Tooltip;
}

bool hasCheckedApi(NodeType type) {
    return type == NodeType::Checkbox || type == NodeType::Radio || type == NodeType::Switch ||
           type == NodeType::HeartSwitch;
}

bool hasValueApi(NodeType type) {
    return type == NodeType::Slider || type == NodeType::Progress;
}

bool hasSelectedApi(NodeType type) {
    return type == NodeType::Segmented || type == NodeType::Tabs || type == NodeType::Dropdown ||
           type == NodeType::Navbar || type == NodeType::Carousel || type == NodeType::CardSlider;
}

bool hasFontSizeApi(NodeType type) {
    return type == NodeType::Text || type == NodeType::Button || type == NodeType::Input ||
           type == NodeType::Checkbox || type == NodeType::Radio || type == NodeType::Switch ||
           type == NodeType::Segmented || type == NodeType::Tabs || type == NodeType::Stepper ||
           type == NodeType::NeumorphicButton;
}

bool hasRadiusApi(NodeType type) {
    return type == NodeType::Rect || type == NodeType::Panel || type == NodeType::Card ||
           type == NodeType::Button || type == NodeType::Image || type == NodeType::Svg ||
           type == NodeType::NeumorphicButton || type == NodeType::MouseArea;
}

bool hasColorApi(NodeType type) {
    return type != NodeType::Row && type != NodeType::Column && type != NodeType::Stack &&
           type != NodeType::Scroll && type != NodeType::ScrollView &&
           type != NodeType::Image && type != NodeType::Svg;
}

Node* findNode(int uid) {
    const auto found = std::find_if(state.nodes.begin(), state.nodes.end(), [uid](const Node& node) {
        return node.uid == uid;
    });
    return found == state.nodes.end() ? nullptr : &*found;
}

bool nodeIdInUse(const std::string& id, int exceptUid = -1) {
    return !id.empty() && std::any_of(state.nodes.begin(), state.nodes.end(), [&](const Node& node) {
        if (node.uid == exceptUid) return false;
        return node.id == id || node.id.rfind(id + ".", 0) == 0 || id.rfind(node.id + ".", 0) == 0;
    });
}

std::string uniqueNodeId(std::string base) {
    if (base.empty()) base = "element";
    if (!nodeIdInUse(base)) return base;
    for (int suffix = 2;; ++suffix) {
        const std::string candidate = base + "_" + std::to_string(suffix);
        if (!nodeIdInUse(candidate)) return candidate;
    }
}

eui::Vec2 absolutePosition(const Node& node) {
    eui::Vec2 result{node.x, node.y};
    int parentUid = node.parentUid;
    int guard = 0;
    while (parentUid >= 0 && guard++ < 32) {
        const Node* parent = findNode(parentUid);
        if (!parent) break;
        result.x += parent->x;
        result.y += parent->y;
        parentUid = parent->parentUid;
    }
    return result;
}

int nodeDepth(const Node& node) {
    int depth = 0;
    int parentUid = node.parentUid;
    while (parentUid >= 0 && depth < 16) {
        const Node* parent = findNode(parentUid);
        if (!parent) break;
        ++depth;
        parentUid = parent->parentUid;
    }
    return depth;
}

void rebuildElementTreeOrder() {
    state.elementTreeOrder.clear();
    const auto appendChildren = [&](const auto& self, int parentUid, int depth) -> void {
        if (depth > 16) return;
        for (const Node& node : state.nodes) {
            if (node.parentUid != parentUid) continue;
            state.elementTreeOrder.push_back(node.uid);
            self(self, node.uid, depth + 1);
        }
    };
    appendChildren(appendChildren, -1, 0);
}

void applyLayoutPositions() {
    for (int pass = 0; pass < 16; ++pass) {
        bool changed = false;
        for (Node& parent : state.nodes) {
            if (!isLayout(parent.type)) continue;
            std::vector<Node*> children;
            for (Node& child : state.nodes) {
                if (child.parentUid == parent.uid && child.participatesInLayout) children.push_back(&child);
            }
            float contentSpan = 0.0f;
            for (const Node* child : children) {
                contentSpan += parent.type == NodeType::Row ? child->width : child->height;
            }
            const float gap = children.size() > 1 ? std::min(parent.gap,
                std::max(0.0f, (parent.type == NodeType::Row ? parent.width : parent.height) - parent.padding * 2.0f) /
                static_cast<float>(children.size() - 1)) : 0.0f;
            if (children.size() > 1) contentSpan += gap * static_cast<float>(children.size() - 1);
            const float availableMain = (parent.type == NodeType::Row ? parent.width : parent.height) - parent.padding * 2.0f;
            const float availableChildren = std::max(0.0f, availableMain - gap * std::max(0, static_cast<int>(children.size()) - 1));
            if ((parent.type == NodeType::Row || parent.type == NodeType::Column) && contentSpan > availableMain && contentSpan > 0.0f) {
                const float scale = availableChildren / std::max(1.0f, contentSpan - gap * std::max(0, static_cast<int>(children.size()) - 1));
                for (Node* child : children) {
                    if (parent.type == NodeType::Row) child->width = std::max(1.0f, child->width * scale);
                    else child->height = std::max(1.0f, child->height * scale);
                }
                contentSpan = availableMain;
            }
            const float freeMain = std::max(0.0f, availableMain - contentSpan);
            float cursor = parent.padding + (parent.mainAlign == 1 ? freeMain * 0.5f : parent.mainAlign == 2 ? freeMain : 0.0f);
            for (Node* child : children) {
                float nextX = parent.padding;
                float nextY = parent.padding;
                if (parent.type == NodeType::Row) {
                    nextX = cursor;
                    const float freeCross = std::max(0.0f, parent.height - parent.padding * 2.0f - child->height);
                    nextY += parent.crossAlign == 1 ? freeCross * 0.5f : parent.crossAlign == 2 ? freeCross : 0.0f;
                    cursor += child->width + gap;
                } else if (parent.type == NodeType::Column || parent.type == NodeType::Scroll || parent.type == NodeType::ScrollView) {
                    nextY = cursor;
                    const float freeCross = std::max(0.0f, parent.width - parent.padding * 2.0f - child->width);
                    nextX += parent.crossAlign == 1 ? freeCross * 0.5f : parent.crossAlign == 2 ? freeCross : 0.0f;
                    cursor += child->height + gap;
                } else {
                    const float freeX = std::max(0.0f, parent.width - parent.padding * 2.0f - child->width);
                    const float freeY = std::max(0.0f, parent.height - parent.padding * 2.0f - child->height);
                    nextX += parent.crossAlign == 1 ? freeX * 0.5f : parent.crossAlign == 2 ? freeX : 0.0f;
                    nextY += parent.mainAlign == 1 ? freeY * 0.5f : parent.mainAlign == 2 ? freeY : 0.0f;
                }
                if (child->x != nextX || child->y != nextY) {
                    child->x = nextX;
                    child->y = nextY;
                    changed = true;
                }
            }
        }
        if (!changed) break;
    }
}

void clearPage() {
    state.nodes.clear();
    state.elementTreeOrder.clear();
    state.selectedUid = -1;
    state.previewOverlayUid = -1;
    state.elementPickerOpen = false;
    state.elementMenuOpen = false;
    state.elementMenuUid = -1;
    state.layoutDirty = false;
    numericDrafts.clear();
    focusedNumericInputs.clear();
    state.status = "Page cleared";
}

void moveSelectedSibling(int direction) {
    Node* selected = selectedNode();
    if (!selected) return;
    const int uid = selected->uid;
    const int parentUid = selected->parentUid;
    std::vector<std::size_t> siblings;
    for (std::size_t index = 0; index < state.nodes.size(); ++index) {
        if (state.nodes[index].parentUid == parentUid) siblings.push_back(index);
    }
    const auto current = std::find_if(siblings.begin(), siblings.end(), [uid](std::size_t index) {
        return state.nodes[index].uid == uid;
    });
    if (current == siblings.end()) return;
    const auto offset = static_cast<std::ptrdiff_t>(std::distance(siblings.begin(), current));
    const auto target = offset + direction;
    if (target < 0 || target >= static_cast<std::ptrdiff_t>(siblings.size())) return;
    std::iter_swap(state.nodes.begin() + static_cast<std::ptrdiff_t>(*current),
                   state.nodes.begin() + static_cast<std::ptrdiff_t>(siblings[static_cast<std::size_t>(target)]));
}

void reparentSelected(bool indent) {
    Node* selected = selectedNode();
    if (!selected) return;
    const eui::Vec2 absolute = absolutePosition(*selected);
    int nextParentUid = -1;
    if (indent) {
        Node* previous = nullptr;
        for (Node& candidate : state.nodes) {
            if (candidate.parentUid != selected->parentUid) continue;
            if (candidate.uid == selected->uid) break;
            previous = &candidate;
        }
        if (!previous || !isLayout(previous->type)) return;
        nextParentUid = previous->uid;
    } else {
        if (selected->parentUid < 0) return;
        const Node* parent = findNode(selected->parentUid);
        nextParentUid = parent ? parent->parentUid : -1;
    }
    selected = selectedNode();
    selected->parentUid = nextParentUid;
    selected->participatesInLayout = nextParentUid >= 0;
    state.layoutDirty = selected->participatesInLayout;
    const Node* nextParent = findNode(nextParentUid);
    const eui::Vec2 parentAbsolute = nextParent ? absolutePosition(*nextParent) : eui::Vec2{};
    selected->x = absolute.x - parentAbsolute.x;
    selected->y = absolute.y - parentAbsolute.y;
}

void addNode(NodeType type) {
    Node node;
    node.uid = state.nextUid++;
    node.type = type;
    node.id = "element_" + std::to_string(node.uid);
    node.text = defaultText(type);
    if (type == NodeType::Tabs) node.itemsText = "Overview, Details, Settings";
    else if (type == NodeType::Dropdown) {
        node.itemsText = "First option, Second option, Third option";
        node.messageText = "Select an option";
    }
    else if (type == NodeType::ContextMenu) node.itemsText = "Edit, Duplicate, Delete";
    else if (type == NodeType::Carousel || type == NodeType::CardSlider) node.itemsText = "First card, Second card, Third card";
    else if (type == NodeType::Navbar) {
        node.itemsText = "Home, Settings";
        node.messageText = "Navigation";
    }
    else if (type == NodeType::DataTable) {
        node.labelsText = "Name, State";
        node.itemsText = "Alpha, Ready, Beta, Draft";
    }
    if (type == NodeType::ColorPicker) node.palette = colorPresets();
    if (type == NodeType::Dialog) node.messageText = "Dialog content";
    else if (type == NodeType::Toast) node.messageText = "Toast message";
    else if (type == NodeType::Sidebar) node.messageText = "Sidebar content";
    else if (type == NodeType::TiltCard) node.messageText = "Interactive card";
    if (type == NodeType::Text || type == NodeType::Markdown) {
        node.color = {0.06f, 0.075f, 0.10f, 1.0f};
    }
    if (type == NodeType::Button) node.foregroundColor = {1.0f, 1.0f, 1.0f, 1.0f};
    if (type == NodeType::Card) node.color = {1.0f, 1.0f, 1.0f, 1.0f};
    defaultSize(type, node.width, node.height);
    node.parentUid = -1;
    node.participatesInLayout = false;
    if (isLayout(type)) {
        const float offset = static_cast<float>((node.uid - 1) % 6) * 36.0f;
        node.x = std::min(std::max(20.0f, offset + 40.0f), std::max(20.0f, state.canvasWidth - node.width));
        node.y = std::min(std::max(20.0f, offset + 40.0f), std::max(20.0f, state.canvasHeight - node.height));
    } else {
        node.x = std::max(20.0f, (state.canvasWidth - node.width) * 0.5f + static_cast<float>((node.uid % 4) * 14));
        node.y = std::max(20.0f, (state.canvasHeight - node.height) * 0.36f + static_cast<float>((node.uid % 4) * 14));
    }
    state.nodes.push_back(node);
    state.selectedUid = node.uid;
    state.layoutDirty = false;
    state.status = std::string(typeName(type)) + " added";
}

void removeSelected() {
    const int uid = state.selectedUid;
    std::vector<int> removed{uid};
    bool changed = true;
    while (changed) {
        changed = false;
        for (const Node& node : state.nodes) {
            if (std::find(removed.begin(), removed.end(), node.parentUid) != removed.end() &&
                std::find(removed.begin(), removed.end(), node.uid) == removed.end()) {
                removed.push_back(node.uid);
                changed = true;
            }
        }
    }
    state.nodes.erase(std::remove_if(state.nodes.begin(), state.nodes.end(), [&removed](const Node& node) {
        return std::find(removed.begin(), removed.end(), node.uid) != removed.end();
    }), state.nodes.end());
    state.selectedUid = -1;
    state.previewOverlayUid = -1;
    state.elementPickerOpen = false;
    state.elementMenuOpen = false;
    state.elementMenuUid = -1;
    state.status = "Element removed";
}

void duplicateSelected() {
    const Node* selected = selectedNode();
    if (!selected) return;
    const int sourceRootUid = selected->uid;
    const std::vector<Node> source = state.nodes;
    std::vector<Node> copies;
    const auto copySubtree = [&](const auto& self, int sourceUid, int copiedParentUid) -> void {
        const auto found = std::find_if(source.begin(), source.end(), [sourceUid](const Node& node) { return node.uid == sourceUid; });
        if (found == source.end()) return;
        Node copy = *found;
        copy.uid = state.nextUid++;
        copy.id = uniqueNodeId(found->id + "_copy");
        copy.parentUid = copiedParentUid;
        copy.locked = false;
        if (sourceUid == sourceRootUid) {
            copy.x += 16.0f;
            copy.y += 16.0f;
        }
        copies.push_back(copy);
        const int nextParentUid = copy.uid;
        for (const Node& child : source) {
            if (child.parentUid == sourceUid) self(self, child.uid, nextParentUid);
        }
    };
    copySubtree(copySubtree, sourceRootUid, selected->parentUid);
    if (copies.empty()) return;
    state.selectedUid = copies.front().uid;
    state.nodes.insert(state.nodes.end(), copies.begin(), copies.end());
    state.layoutDirty = copies.front().parentUid >= 0 && copies.front().participatesInLayout;
    state.status = "Element duplicated";
}

#if 0
std::string escaped(const std::string& value) {
    std::string output;
    output.reserve(value.size());
    for (char ch : value) {
        if (ch == '\\' || ch == '"') {
            output.push_back('\\');
        }
        if (ch == '\n') {
            output += "\\n";
        } else if (ch != '\r') {
            output.push_back(ch);
        }
    }
    return output;
}

std::string safeId(const Node& node) {
    return node.id.empty() ? "element_" + std::to_string(node.uid) : node.id;
}

std::string floatText(float value) {
    std::ostringstream stream;
    stream.setf(std::ios::fixed);
    stream.precision(1);
    stream << value;
    return stream.str();
}

void generateNode(std::ostringstream& code, const Node& node) {
    const std::string id = escaped(safeId(node));
    const eui::Vec2 absolute = absolutePosition(node);
    const std::string position = ".position(" + floatText(absolute.x) + "f, " + floatText(absolute.y) + "f)";
    const std::string size = ".size(" + floatText(node.width) + "f, " + floatText(node.height) + "f)";
    const eui::Color color = node.color;
    switch (node.type) {
    case NodeType::Row:
        code << "    ui.row(\"" << id << "\")\n        " << position << size
             << ".gap(12.0f).padding(12.0f).content([&] {\n"
             << "            // Add row children here.\n        }).build();\n\n";
        break;
    case NodeType::Column:
        code << "    ui.column(\"" << id << "\")\n        " << position << size
             << ".gap(12.0f).padding(12.0f).content([&] {\n"
             << "            // Add column children here.\n        }).build();\n\n";
        break;
    case NodeType::Stack:
        code << "    ui.stack(\"" << id << "\")\n        " << position << size
             << ".content([&] {\n            // Add layered children here.\n        }).build();\n\n";
        break;
    case NodeType::Panel:
    case NodeType::Rect:
        code << "    ui.rect(\"" << id << "\")\n        " << position << size
             << ".color({" << color.r << "f, " << color.g << "f, " << color.b << "f, 0.14f})"
             << ".radius(" << floatText(node.radius) << "f).border(1.0f, {0.24f, 0.28f, 0.36f, 1.0f}).build();\n\n";
        break;
    case NodeType::Text:
        code << "    ui.text(\"" << id << "\")\n        " << position << size
             << ".text(\"" << escaped(node.text) << "\").fontSize(" << floatText(node.fontSize)
             << "f).lineHeight(" << floatText(node.fontSize * 1.3f) << "f).color({0.92f, 0.94f, 0.98f, 1.0f})"
             << ".horizontalAlign(static_cast<eui::HorizontalAlign>(" << node.horizontalAlign << "))"
             << ".verticalAlign(static_cast<eui::VerticalAlign>(" << node.verticalAlign << ")).build();\n\n";
        break;
    case NodeType::Polygon:
        code << "    ui.polygon(\"" << id << "\")\n        " << position << size
             << ".points({{" << node.width * 0.5f << "f, 0.0f}, {" << node.width << "f, " << node.height
             << "f}, {0.0f, " << node.height << "f}}).color({" << color.r << "f, " << color.g << "f, " << color.b << "f, 1.0f}).build();\n\n";
        break;
    case NodeType::Svg:
        code << "    ui.svg(\"" << id << "\")\n        " << position << size
             << ".source(\"" << escaped(node.text) << "\").build();\n\n";
        break;
    case NodeType::Card:
        code << "    components::card(ui, \"" << id << "\")\n        " << position << size
             << ".theme(theme).radius(" << floatText(node.radius) << "f).content([&] {}).build();\n\n";
        break;
    case NodeType::Markdown:
        code << "    components::markdown(ui, \"" << id << "\")\n        " << position << size
             << ".theme(theme).markdown(\"" << escaped(node.text) << "\").build();\n\n";
        break;
    case NodeType::Button:
        code << "    components::button(ui, \"" << id << "\")\n        " << position << size
             << ".theme(theme).text(\"" << escaped(node.text) << "\").radius(" << floatText(node.radius)
             << "f).onClick([] {}).build();\n\n";
        break;
    case NodeType::Input:
        code << "    components::input(ui, \"" << id << "\")\n        " << position << size
             << ".theme(theme).placeholder(\"" << escaped(node.text) << "\").build();\n\n";
        break;
    case NodeType::Checkbox:
        code << "    ui.stack(\"" << id << ".wrap\")\n        " << position << size << ".content([&] {\n"
             << "        components::checkbox(ui, \"" << id << "\")\n            " << size
             << ".theme(theme).text(\"" << escaped(node.text) << "\").checked(" << (node.checked ? "true" : "false")
             << ").onChange([](bool) {}).build();\n    }).build();\n\n";
        break;
    case NodeType::Radio:
        code << "    ui.stack(\"" << id << ".wrap\")\n        " << position << size << ".content([&] {\n"
             << "        components::radio(ui, \"" << id << "\").size(" << node.width << "f, " << node.height
             << "f).theme(theme).text(\"" << escaped(node.text) << "\").selected(" << (node.checked ? "true" : "false")
             << ").onChange([](bool) {}).build();\n    }).build();\n\n";
        break;
    case NodeType::Switch:
        code << "    ui.stack(\"" << id << ".wrap\")\n        " << position << size << ".content([&] {\n"
             << "        components::toggleSwitch(ui, \"" << id << "\")\n            " << size
             << ".theme(theme).text(\"" << escaped(node.text) << "\").checked(" << (node.checked ? "true" : "false")
             << ").onChange([](bool) {}).build();\n    }).build();\n\n";
        break;
    case NodeType::Slider:
        code << "    ui.stack(\"" << id << ".wrap\")\n        " << position << size << ".content([&] {\n"
             << "        components::slider(ui, \"" << id << "\")\n            " << size
             << ".theme(theme).value(" << node.value << "f).onChange([](float) {}).build();\n    }).build();\n\n";
        break;
    case NodeType::Progress:
        code << "    ui.stack(\"" << id << ".wrap\")\n        " << position << size << ".content([&] {\n"
             << "        components::progress(ui, \"" << id << "\")\n            " << size
             << ".theme(theme).value(" << node.value << "f).build();\n    }).build();\n\n";
        break;
    case NodeType::Image:
        code << "    ui.image(\"" << id << "\")\n        " << position << size
             << ".source(\"" << escaped(node.text) << "\").radius(" << floatText(node.radius) << "f).build();\n\n";
        break;
    case NodeType::Segmented:
    case NodeType::Tabs:
    case NodeType::Dropdown:
        code << "    ui.stack(\"" << id << ".wrap\")\n        " << position << size << ".content([&] {\n        components::"
             << (node.type == NodeType::Segmented ? "segmented" : node.type == NodeType::Tabs ? "tabs" : "dropdown")
             << "(ui, \"" << id << "\").size(" << node.width << "f, " << node.height
             << "f).theme(theme).items({\"One\", \"Two\", \"Three\"}).selected(0).onChange([](int) {}).build();\n    }).build();\n\n";
        break;
    case NodeType::Stepper:
        code << "    ui.stack(\"" << id << ".wrap\")\n        " << position << size
             << ".content([&] { components::stepper(ui, \"" << id << "\").size(" << node.width << "f, " << node.height
             << "f).theme(theme).value(1).min(0).max(100).onChange([](long long) {}).build(); }).build();\n\n";
        break;
    case NodeType::LineChart:
    case NodeType::BarChart:
    case NodeType::PieChart:
        code << "    ui.stack(\"" << id << ".wrap\")\n        " << position << size << ".content([&] {\n        components::"
             << (node.type == NodeType::LineChart ? "lineChart" : node.type == NodeType::BarChart ? "barChart" : "pieChart")
             << "(ui, \"" << id << "\").size(" << node.width << "f, " << node.height
             << "f).theme(theme).title(\"Chart\").values({24.0f, 52.0f, 38.0f}).labels({\"A\", \"B\", \"C\"}).build();\n    }).build();\n\n";
        break;
    default:
        code << "    // " << typeName(node.type) << " placeholder: configure its specialized API here.\n"
             << "    ui.rect(\"" << id << "\")\n        " << position << size
             << ".color({0.12f, 0.14f, 0.18f, 1.0f}).radius(" << node.radius << "f).build();\n\n";
        break;
    }
}

std::string generatedCode() {
    std::ostringstream code;
    code << "#include \"eui_neo.h\"\n\nnamespace app {\n\n";
    code << "const DslAppConfig& dslAppConfig() {\n"
         << "    static const DslAppConfig config = DslAppConfig{}\n"
         << "        .title(\"" << escaped(state.pageTitle) << "\")\n"
         << "        .pageId(\"" << escaped(state.pageId) << "\")\n"
         << "        .windowSize(" << static_cast<int>(state.canvasWidth) << ", " << static_cast<int>(state.canvasHeight) << ");\n"
         << "    return config;\n}\n\n";
    const eui::Color background = state.backgroundColor;
    code << "void compose(eui::Ui& ui, const eui::Screen& screen) {\n"
         << "    const auto theme = components::theme::dark();\n"
         << "    ui.rect(\"page.background\").size(screen.width, screen.height)\n"
         << "        .color({" << background.r << "f, " << background.g << "f, " << background.b << "f, 1.0f}).build();\n\n";
    for (const Node& node : state.nodes) {
        generateNode(code, node);
    }
    code << "}\n\n} // namespace app\n";
    return code.str();
}

#endif

std::string generatedCode() {
    return designer::generatedCode({
        state.nodes,
        state.pageTitle,
        state.pageId,
        static_cast<int>(state.canvasWidth),
        static_cast<int>(state.canvasHeight),
        state.backgroundColor
    });
}

void copyCode() {
    core::window::setClipboardText(generatedCode());
    state.status = "C++ copied to clipboard";
}

void exportCode() {
    std::ofstream output("generated_page.cpp", std::ios::binary | std::ios::trunc);
    if (!output) {
        state.status = "Could not write generated_page.cpp";
        return;
    }
    output << generatedCode();
    state.status = "Exported generated_page.cpp";
}

std::string quotedCommandPath(const std::filesystem::path& path) {
    return "\"" + path.string() + "\"";
}

std::filesystem::path cmakeExecutable() {
#if defined(_WIN32)
    const std::filesystem::path standardInstall{"C:/Program Files/CMake/bin/cmake.exe"};
    if (std::filesystem::exists(standardInstall)) return standardInstall;
#endif
    return "cmake";
}

std::string commandError(const std::string& stage, const core::platform::CommandResult& result) {
    return stage + " failed (" + std::to_string(result.exitCode) + "); see build-designer-preview/build.log";
}

void buildAndRunPreview() {
    if (async::running("designer.preview.build")) {
        state.status = "Preview build is already running";
        return;
    }

    const std::string pageCode = generatedCode();
    state.status = "Configuring preview...";
    async::restart(
        "designer.preview.build",
        [pageCode] {
#if !defined(EUI_PROJECT_SOURCE_DIR)
            return async::failure<PreviewBuildResult>("Designer build paths are not configured");
#else
            namespace fs = std::filesystem;
            const fs::path projectRoot{EUI_PROJECT_SOURCE_DIR};
            const fs::path previewRoot = projectRoot / "build-designer-preview";
            const fs::path sourceDir = previewRoot / "source";
            const fs::path buildDir = previewRoot / "build";
            const fs::path logPath = previewRoot / "build.log";
            std::ostringstream targetName;
            targetName << "eui_designer_preview_" << std::hex << std::hash<std::string>{}(pageCode) << "_"
                       << std::chrono::steady_clock::now().time_since_epoch().count();
            const std::string previewTarget = targetName.str();
            std::error_code error;
            fs::create_directories(sourceDir, error);
            if (error) return async::failure<PreviewBuildResult>("Could not create preview directory: " + error.message());

            std::ofstream page(sourceDir / "generated_page.cpp", std::ios::binary | std::ios::trunc);
            page << pageCode;
            page.close();
            if (!page) return async::failure<PreviewBuildResult>("Could not write preview page");

            const std::string root = projectRoot.generic_string();
            std::ofstream cmake(sourceDir / "CMakeLists.txt", std::ios::binary | std::ios::trunc);
            cmake << "cmake_minimum_required(VERSION 3.20)\n"
                  << "project(eui_designer_preview LANGUAGES C CXX)\n"
                  << "set(EUI_BUILD_APPS OFF CACHE BOOL \"\" FORCE)\n"
                  << "set(EUI_BUILD_USER_APPS OFF CACHE BOOL \"\" FORCE)\n"
                  << "set(EUI_BUILD_TEST_FIXTURES OFF CACHE BOOL \"\" FORCE)\n"
                  << "set(EUI_BUILD_SHARED OFF CACHE BOOL \"\" FORCE)\n"
                  << "add_subdirectory(\"" << root << "\" eui_neo)\n"
                  << "add_executable(" << previewTarget << " \"" << root << "/core/app/glfw_app_main.cpp\" generated_page.cpp)\n"
                  << "eui_neo_configure_app(" << previewTarget << ")\n";
            cmake.close();
            if (!cmake) return async::failure<PreviewBuildResult>("Could not write preview CMakeLists.txt");

            const fs::path cmakeCommand = cmakeExecutable();
            const std::string configure = quotedCommandPath(cmakeCommand) + " -S " + quotedCommandPath(sourceDir) +
                " -B " + quotedCommandPath(buildDir) + " -DCMAKE_BUILD_TYPE=Release 2>&1";
            std::ofstream log(logPath, std::ios::binary | std::ios::trunc);
            log << "$ " << configure << "\n";
            core::platform::CommandResult configured = core::platform::executeCommand(configure);
            log << configured.output << "\n";
            log.flush();
            if (!configured.succeeded()) {
                std::error_code cleanupError;
                fs::remove_all(buildDir, cleanupError);
                log << "\nRetrying with a clean CMake cache\n$ " << configure << "\n";
                log.flush();
                configured = core::platform::executeCommand(configure);
                log << configured.output << "\n";
                log.flush();
            }
            if (!configured.succeeded()) return async::failure<PreviewBuildResult>(commandError("Configure", configured));

            const std::string build = quotedCommandPath(cmakeCommand) + " --build " + quotedCommandPath(buildDir) +
                " --config Release --target " + previewTarget + " --parallel 2 2>&1";
            const core::platform::CommandResult built = core::platform::executeCommand(build);
            log << "\n$ " << build << "\n" << built.output << "\n";
            log.flush();
            if (!built.succeeded()) return async::failure<PreviewBuildResult>(commandError("Build", built));

            std::string executableName = previewTarget;
#if defined(_WIN32)
            executableName += ".exe";
#endif
            fs::path executable = buildDir / "Release" / executableName;
            if (!fs::exists(executable)) executable = buildDir / executableName;
            if (!fs::exists(executable)) return async::failure<PreviewBuildResult>("Build succeeded but preview executable was not found");
            core::platform::terminateProcessesByExecutablePrefix("eui_designer_preview");
            if (!core::platform::launchDetached(executable.string())) {
                return async::failure<PreviewBuildResult>("Preview built but could not be launched");
            }
            return async::success(PreviewBuildResult{true, "Preview launched"});
#endif
        },
        [](const async::Result<PreviewBuildResult>& result) {
            state.status = result.ok ? result.value.message : result.error;
            core::platform::requestUiUpdate();
        });
}

void sectionLabel(eui::Ui& ui, const std::string& id, const std::string& text, float x, float y, float width) {
    ui.text(id).position(x, y).size(width, 24.0f).text(text).fontSize(12.0f).lineHeight(18.0f)
        .fontWeight(760).color(kMuted).build();
}

void smallLabel(eui::Ui& ui, const std::string& id, const std::string& text, float x, float y, float width) {
    ui.text(id).position(x, y).size(width, 20.0f).text(text).fontSize(12.0f).lineHeight(18.0f)
        .color(kMuted).build();
}

void composeTopBar(eui::Ui& ui, float width) {
    ui.stack("topbar").size(width, kTopBarHeight).content([&] {
        ui.rect("topbar.bg").size(width, kTopBarHeight).color({0.045f, 0.053f, 0.067f, 1.0f})
            .border(0.0f, {}).build();
        ui.rect("topbar.rule").position(0.0f, kTopBarHeight - 1.0f).size(width, 1.0f).color(kBorder).build();
        ui.text("topbar.logo").position(20.0f, 0.0f).size(36.0f, kTopBarHeight).icon(0xF5FD)
            .fontSize(22.0f).lineHeight(22.0f).color(kAccent).horizontalAlign(eui::HorizontalAlign::Center)
            .verticalAlign(eui::VerticalAlign::Center).build();
        ui.text("topbar.title").position(64.0f, 0.0f).size(kLeftWidth - 76.0f, kTopBarHeight).text("EUI Designer")
            .fontSize(17.0f).lineHeight(22.0f).fontWeight(780).color(kText)
            .verticalAlign(eui::VerticalAlign::Center).build();
        const float centerWidth = std::max(0.0f, width - kLeftWidth - kRightWidth);
        const float settingsX = kLeftWidth + 12.0f;
        if (centerWidth >= 250.0f) {
            const bool showDimensions = centerWidth >= 430.0f;
            const bool showPageId = centerWidth >= 610.0f;
            const float titleWidth = showDimensions ? std::min(180.0f, centerWidth - (showPageId ? 354.0f : 190.0f))
                                                    : std::max(120.0f, centerWidth - 62.0f);
            components::input(ui, "topbar.page.title").position(settingsX, 10.0f).size(titleWidth, 38.0f)
                .theme(designerTheme()).value(state.pageTitle).placeholder("Window title")
                .onChange([](const std::string& value) { state.pageTitle = value; }).build();
            float cursor = settingsX + titleWidth + 8.0f;
            if (showDimensions) {
                numericInput(ui, "topbar.page.width", cursor, 10.0f, 64.0f, 38.0f,
                    static_cast<long long>(state.canvasWidth), 320, 1920,
                    [](long long parsed) { state.canvasWidth = static_cast<float>(parsed); });
                cursor += 72.0f;
                numericInput(ui, "topbar.page.height", cursor, 10.0f, 64.0f, 38.0f,
                    static_cast<long long>(state.canvasHeight), 240, 1200,
                    [](long long parsed) { state.canvasHeight = static_cast<float>(parsed); });
                cursor += 72.0f;
            }
            ui.rect("topbar.page.color").position(cursor, 10.0f).size(38.0f, 38.0f)
                .states(state.backgroundColor, state.backgroundColor, state.backgroundColor).radius(7.0f)
                .border(2.0f, kBorder).onClick([] { state.backgroundPickerOpen = true; }).build();
            cursor += 46.0f;
            if (showPageId) {
                components::input(ui, "topbar.page.id").position(cursor, 10.0f).size(142.0f, 38.0f)
                    .theme(designerTheme()).value(state.pageId).placeholder("Page ID")
                    .onChange([](const std::string& value) { state.pageId = value; }).build();
            }
        }
        const float actionsX = width - kRightWidth + 12.0f;
        components::button(ui, "topbar.run").position(actionsX, 10.0f).size(38.0f, 38.0f)
            .theme(designerTheme()).text("").icon(0xF04B).onClick(buildAndRunPreview).build();
        components::button(ui, "topbar.copy").position(actionsX + 46.0f, 10.0f).size(112.0f, 38.0f)
            .theme(designerTheme(), false).text("Copy C++").icon(0xF0C5).onClick(copyCode).build();
        components::button(ui, "topbar.export").position(actionsX + 166.0f, 10.0f).size(146.0f, 38.0f)
            .theme(designerTheme()).text("Export .cpp").icon(0xF56E).onClick(exportCode).build();
    }).build();
}

const std::vector<NodeType>& libraryItems() {
    static const std::vector<NodeType> items = {
        NodeType::Row, NodeType::Column, NodeType::Stack, NodeType::Scroll, NodeType::ScrollView,
        NodeType::VirtualList, NodeType::Rect, NodeType::Text, NodeType::Image, NodeType::Svg,
        NodeType::Polygon, NodeType::Panel, NodeType::Card, NodeType::Markdown, NodeType::Button,
        NodeType::Input, NodeType::Checkbox, NodeType::Radio, NodeType::Switch, NodeType::Slider,
        NodeType::Progress, NodeType::Segmented, NodeType::Stepper, NodeType::Tabs, NodeType::Dropdown,
        NodeType::DatePicker, NodeType::TimePicker, NodeType::ColorPicker, NodeType::DataTable,
        NodeType::Dialog, NodeType::Sidebar, NodeType::Navbar, NodeType::Toast, NodeType::Tooltip,
        NodeType::ContextMenu, NodeType::Carousel, NodeType::LineChart, NodeType::BarChart,
        NodeType::PieChart, NodeType::MouseArea, NodeType::HeartSwitch, NodeType::NeumorphicButton,
        NodeType::CardSlider, NodeType::TiltCard
    };
    return items;
}

void composePalette(eui::Ui& ui, float height) {
    rebuildElementTreeOrder();
    ui.stack("palette").position(0.0f, kTopBarHeight).size(kLeftWidth, height).content([&] {
        ui.rect("palette.bg").size(kLeftWidth, height).color(kPanel).build();
        ui.rect("palette.rule").position(kLeftWidth - 1.0f, 0.0f).size(1.0f, height).color(kBorder).build();
        ui.text("elements.title").position(kPanelPadding, 14.0f).size(72.0f, 26.0f).text("Elements")
            .fontSize(17.0f).lineHeight(22.0f).fontWeight(780).color(kText).build();
        const float toolX = 92.0f;
        components::button(ui, "elements.up").position(toolX, 10.0f).size(30.0f, 30.0f).theme(designerTheme(), false)
            .icon(0xF062).text("").onClick([] { moveSelectedSibling(-1); }).build();
        components::button(ui, "elements.down").position(toolX + 34.0f, 10.0f).size(30.0f, 30.0f).theme(designerTheme(), false)
            .icon(0xF063).text("").onClick([] { moveSelectedSibling(1); }).build();
        components::button(ui, "elements.indent").position(toolX + 68.0f, 10.0f).size(30.0f, 30.0f).theme(designerTheme(), false)
            .icon(0xF03C).text("").onClick([] { reparentSelected(true); }).build();
        components::button(ui, "elements.outdent").position(toolX + 102.0f, 10.0f).size(30.0f, 30.0f).theme(designerTheme(), false)
            .icon(0xF03B).text("").onClick([] { reparentSelected(false); }).build();
        components::button(ui, "elements.clear").position(toolX + 136.0f, 10.0f).size(30.0f, 30.0f).theme(designerTheme(), false)
            .icon(0xF2ED).text("").onClick(clearPage).build();
        const float treeHeight = std::clamp(height * 0.46f, 220.0f, 430.0f);
        components::virtualList(ui, "elements.list").position(8.0f, 44.0f).size(kLeftWidth - 16.0f, treeHeight - 50.0f)
            .itemCount(static_cast<std::int64_t>(state.elementTreeOrder.size())).rowHeight(44.0f).bind(state.elementsScroll)
            .theme(designerTheme()).scrollbarWidth(6.0f).scrollbarGap(4.0f).overscanViewports(0.4f)
            .row([](eui::Ui& rowUi, const std::string& rowId, std::int64_t index, float width, float height) {
                if (index < 0 || static_cast<std::size_t>(index) >= state.elementTreeOrder.size()) return;
                const Node* nodePtr = findNode(state.elementTreeOrder[static_cast<std::size_t>(index)]);
                if (!nodePtr) return;
                const Node& node = *nodePtr;
                const bool selected = node.uid == state.selectedUid;
                components::button(rowUi, rowId + ".button").size(width - 8.0f, height - 4.0f)
                    .theme(designerTheme(), selected).text(std::string(static_cast<std::size_t>(nodeDepth(node) * 2), ' ') + node.id)
                    .icon(typeIcon(node.type)).fontSize(14.0f).onClick([uid = node.uid] { state.selectedUid = uid; }).build();
            }).build();
        ui.rect("palette.split").position(0.0f, treeHeight).size(kLeftWidth, 1.0f).color(kBorder).build();
        ui.text("library.title").position(kPanelPadding, treeHeight + 12.0f).size(220.0f, 26.0f).text("Component library")
            .fontSize(17.0f).lineHeight(22.0f).fontWeight(780).color(kText).build();
        const float libraryY = treeHeight + 44.0f;
        const auto& items = libraryItems();
        components::virtualList(ui, "library.list").position(8.0f, libraryY).size(kLeftWidth - 16.0f, std::max(80.0f, height - libraryY))
            .itemCount(static_cast<std::int64_t>(items.size())).rowHeight(46.0f).bind(state.paletteScroll)
            .theme(designerTheme()).scrollbarWidth(6.0f).scrollbarGap(4.0f).overscanViewports(0.5f)
            .row([&items](eui::Ui& rowUi, const std::string& rowId, std::int64_t index, float width, float height) {
                if (index < 0 || static_cast<std::size_t>(index) >= items.size()) return;
                const NodeType type = items[static_cast<std::size_t>(index)];
                components::button(rowUi, rowId + ".button").size(width - 8.0f, height - 4.0f)
                    .theme(designerTheme(), false).text(typeName(type)).icon(typeIcon(type)).fontSize(13.0f)
                    .onClick([type] { addNode(type); }).build();
            }).build();
    }).build();
}

void composeSelectedComponentPreview(eui::Ui& ui, float canvasWidth, float canvasHeight, float scale);

void composeNodeVisual(eui::Ui& ui, Node& node, float scale, bool nested = false) {
    const std::string base = "design.node." + std::to_string(node.uid);
    const eui::Vec2 position = nested ? eui::Vec2{node.x, node.y} : absolutePosition(node);
    const float x = position.x * scale;
    const float y = position.y * scale;
    const float width = node.width * scale;
    const float height = node.height * scale;
    const bool selected = node.uid == state.selectedUid;
    const bool opensComponentPreview = isOverlayPreview(node.type) || node.type == NodeType::Dropdown;
    const eui::Color color = node.color;
    auto host = ui.stack(base).size(width, height);
    if (!nested || !node.participatesInLayout) host.position(x, y);
    if (nested && !node.participatesInLayout) host.ignoreLayout();
    host.content([&] {
        const auto composeChildren = [&](eui::Ui& childUi) {
            for (Node& child : state.nodes) {
                if (child.parentUid == node.uid) composeNodeVisual(childUi, child, scale, true);
            }
        };
        if (node.type == NodeType::Scroll || node.type == NodeType::ScrollView) {
            auto scrollStyle = applyStyleFloats(node, applyStyleColors(node, components::ScrollStyle(canvasTheme(node.color)), {
                {"track", &components::ScrollStyle::track}, {"thumb", &components::ScrollStyle::thumb},
                {"thumbHover", &components::ScrollStyle::thumbHover}, {"thumbPressed", &components::ScrollStyle::thumbPressed}
            }), {{"scrollRadius", &components::ScrollStyle::radius}});
            components::scrollView(ui, base + ".scrollView").size(width, height).theme(canvasTheme(node.color))
                .gap(0.0f).step(node.scrollStep * scale).scrollbarWidth(node.scrollbarWidth * scale)
                .scrollbarGap(node.scrollbarGap * scale).style(scrollStyle)
                .content([&](eui::Ui& contentUi, float contentWidth, float viewportHeight) {
                    contentUi.column(base + ".scrollView.content").width(contentWidth).height(core::SizeValue::wrapContent())
                        .minHeight(viewportHeight).padding(node.padding * scale).gap(node.gap * scale)
                        .justifyContent(static_cast<core::Align>(node.mainAlign))
                        .alignItems(static_cast<core::Align>(node.crossAlign))
                        .content([&] { composeChildren(contentUi); }).build();
                }).build();
        } else if (node.type == NodeType::VirtualList) {
            auto scrollStyle = applyStyleFloats(node, applyStyleColors(node, components::ScrollStyle(canvasTheme(node.color)), {
                {"track", &components::ScrollStyle::track}, {"thumb", &components::ScrollStyle::thumb},
                {"thumbHover", &components::ScrollStyle::thumbHover}, {"thumbPressed", &components::ScrollStyle::thumbPressed}
            }), {{"scrollRadius", &components::ScrollStyle::radius}});
            components::virtualList(ui, base + ".virtualList").size(width, height).theme(canvasTheme(node.color))
                .itemCount(node.itemCount).rowHeight(node.rowHeight * scale).step(node.scrollStep * scale)
                .scrollbarWidth(node.scrollbarWidth * scale).scrollbarGap(node.scrollbarGap * scale)
                .overscanViewports(node.overscanViewports).style(scrollStyle)
                .row([](eui::Ui& rowUi, const std::string& rowId, std::int64_t index, float rowWidth, float rowHeight) {
                    rowUi.text(rowId + ".text").size(rowWidth, rowHeight).text("Item " + std::to_string(index))
                        .fontSize(13.0f).lineHeight(18.0f).color({0.10f, 0.12f, 0.16f, 1.0f}).verticalAlign(eui::VerticalAlign::Center).build();
                }).build();
        } else if (node.type == NodeType::Row || node.type == NodeType::Column || node.type == NodeType::Stack) {
            ui.rect(base + ".layout").size(width, height).color({color.r, color.g, color.b, 0.08f * node.opacity})
                .radius(node.radius * scale).border(node.borderWidth * scale, {color.r, color.g, color.b, 0.52f}).build();
            ui.text(base + ".layout.label").position(10.0f * scale, 7.0f * scale).size(width - 20.0f * scale, 24.0f * scale)
                .text(typeName(node.type)).fontSize(12.0f * scale).lineHeight(16.0f * scale).fontWeight(720)
                .color({color.r, color.g, color.b, 0.90f}).build();
            if (node.type == NodeType::Row) {
                ui.row(base + ".layout.content").size(width, height).gap(node.gap * scale).padding(node.padding * scale)
                    .justifyContent(static_cast<core::Align>(node.mainAlign)).alignItems(static_cast<core::Align>(node.crossAlign))
                    .content([&] { composeChildren(ui); }).build();
            } else if (node.type == NodeType::Column) {
                ui.column(base + ".layout.content").size(width, height).gap(node.gap * scale).padding(node.padding * scale)
                    .justifyContent(static_cast<core::Align>(node.mainAlign)).alignItems(static_cast<core::Align>(node.crossAlign))
                    .content([&] { composeChildren(ui); }).build();
            } else {
                ui.stack(base + ".layout.content").size(width, height).gap(node.gap * scale).padding(node.padding * scale)
                    .justifyContent(static_cast<core::Align>(node.mainAlign)).alignItems(static_cast<core::Align>(node.crossAlign))
                    .content([&] { composeChildren(ui); }).build();
            }
        } else if (node.type == NodeType::Rect || node.type == NodeType::Panel) {
            auto shape = ui.rect(base + ".panel").size(width, height).color(color)
                .radius(node.radius * scale).border(node.borderWidth * scale, node.borderColor)
                .blur(node.blur * scale).opacity(node.opacity).scale(node.scaleX, node.scaleY)
                .rotate(node.rotation * 0.0174532925f);
            if (node.gradientEnabled) {
                shape.gradient(color, node.gradientColor, node.gradientHorizontal ? core::GradientDirection::Horizontal : core::GradientDirection::Vertical);
            }
            if (node.shadowEnabled) {
                shape.shadow({true, {node.shadowOffsetX * scale, node.shadowOffsetY * scale}, node.shadowBlur * scale,
                    node.shadowSpread * scale, node.shadowColor, node.insetShadow});
            }
            shape.build();
        } else if (node.type == NodeType::Card) {
            components::CardStyle cardStyle(canvasTheme(node.color));
            if (node.gradientEnabled) {
                cardStyle.gradient = {true, node.color, node.gradientColor,
                    node.gradientHorizontal ? core::GradientDirection::Horizontal : core::GradientDirection::Vertical};
            }
            auto card = components::card(ui, base + ".card").theme(canvasTheme(node.color))
                .width(width).style(cardStyle).color(node.color).radius(node.radius * scale).padding(node.padding * scale)
                .border(node.borderWidth * scale, node.borderColor).opacity(node.opacity);
            if (node.wrapContentHeight) card.wrapContentHeight();
            else card.height(height);
            if (node.shadowEnabled) {
                card.shadow({true, {node.shadowOffsetX * scale, node.shadowOffsetY * scale},
                    node.shadowBlur * scale, node.shadowSpread * scale, node.shadowColor, node.insetShadow});
            } else card.shadow({});
            card.content([&] {}).build();
        } else if (node.type == NodeType::Markdown) {
            auto markdownStyle = applyStyleStrings(node, applyStyleFloats(node, applyStyleColors(node,
                components::MarkdownStyle(canvasTheme(node.color)), {
                    {"text", &components::MarkdownStyle::text}, {"heading", &components::MarkdownStyle::heading},
                    {"muted", &components::MarkdownStyle::muted}, {"accent", &components::MarkdownStyle::accent},
                    {"codeText", &components::MarkdownStyle::codeText}, {"codeBackground", &components::MarkdownStyle::codeBackground},
                    {"quoteBackground", &components::MarkdownStyle::quoteBackground}, {"divider", &components::MarkdownStyle::divider}
                }), {
                    {"bodySize", &components::MarkdownStyle::bodySize}, {"bodyLineHeight", &components::MarkdownStyle::bodyLineHeight},
                    {"h1Size", &components::MarkdownStyle::h1Size}, {"h2Size", &components::MarkdownStyle::h2Size},
                    {"h3Size", &components::MarkdownStyle::h3Size}, {"codeSize", &components::MarkdownStyle::codeSize},
                    {"blockGap", &components::MarkdownStyle::blockGap}, {"listIndent", &components::MarkdownStyle::listIndent},
                    {"codePadding", &components::MarkdownStyle::codePadding}, {"quotePadding", &components::MarkdownStyle::quotePadding},
                    {"tableCellPadding", &components::MarkdownStyle::tableCellPadding}, {"styleRadius", &components::MarkdownStyle::radius}
                }), {
                    {"fontFamily", &components::MarkdownStyle::fontFamily}, {"codeFontFamily", &components::MarkdownStyle::codeFontFamily}
                });
            auto markdown = components::markdown(ui, base + ".markdown").width(width).theme(canvasTheme(node.color))
                .markdown(node.text).margin(node.margin * scale).style(markdownStyle);
            if (node.wrapContentHeight) markdown.wrapContentHeight();
            else markdown.height(height);
            markdown.build();
        } else if (node.type == NodeType::Text) {
            ui.text(base + ".text").size(width, height).text(node.text).fontSize(node.fontSize * scale)
                .fontFamily(node.fontFamily).lineHeight(node.lineHeight * scale).fontWeight(node.fontWeight)
                .wrap(node.wrapText).opacity(node.opacity).scale(node.scaleX, node.scaleY)
                .rotate(node.rotation * 0.0174532925f).color(node.color)
                .horizontalAlign(static_cast<eui::HorizontalAlign>(node.horizontalAlign))
                .verticalAlign(static_cast<eui::VerticalAlign>(node.verticalAlign)).build();
        } else if (node.type == NodeType::Button) {
            auto buttonVisualStyle = applyStyleColors(node, components::ButtonStyle(canvasTheme(node.color)), {
                {"normal", &components::ButtonStyle::normal}, {"hover", &components::ButtonStyle::hover},
                {"pressed", &components::ButtonStyle::pressed}, {"text", &components::ButtonStyle::text},
                {"icon", &components::ButtonStyle::icon}
            });
            buttonVisualStyle.shadow = node.shadowEnabled
                ? core::Shadow{true, {node.shadowOffsetX * scale, node.shadowOffsetY * scale}, node.shadowBlur * scale,
                               node.shadowSpread * scale, node.shadowColor, node.insetShadow}
                : core::Shadow{};
            const eui::Color buttonTextColor = findStyleColor(node, "text") ? *findStyleColor(node, "text") : node.foregroundColor;
            const eui::Color buttonIconColor = findStyleColor(node, "icon") ? *findStyleColor(node, "icon") : buttonTextColor;
            auto button = components::button(ui, base + ".button").size(width, height).text(node.text)
                .theme(canvasTheme(node.color)).style(buttonVisualStyle).radius(node.radius * scale).fontSize(node.fontSize * scale)
                .iconSize(node.iconSize * scale).textColor(buttonTextColor).iconColor(buttonIconColor)
                .opacity(node.opacity).disabled(node.disabled).preserveFocusOnPress(node.preserveFocusOnPress)
                .pressScale(node.pressScale)
                .border(node.borderWidth * scale, node.borderColor);
            if (node.icon != 0) button.icon(node.icon);
            if (node.buttonStyle == 1) {
                button.colors({node.color.r, node.color.g, node.color.b, 0.0f},
                    {node.color.r, node.color.g, node.color.b, node.color.a * 0.10f},
                    {node.color.r, node.color.g, node.color.b, node.color.a * 0.18f})
                    .textColor(node.color).iconColor(node.color)
                    .border(1.0f * scale, {node.color.r, node.color.g, node.color.b, node.color.a * 0.78f})
                    .shadow(0.0f, 0.0f, 0.0f, {0.0f, 0.0f, 0.0f, 0.0f});
            } else if (node.buttonStyle == 2) {
                button.colors({node.color.r, node.color.g, node.color.b, 0.0f},
                    {node.color.r, node.color.g, node.color.b, node.color.a * 0.08f},
                    {node.color.r, node.color.g, node.color.b, node.color.a * 0.14f})
                    .textColor(node.color).iconColor(node.color)
                    .border(0.0f, {0.0f, 0.0f, 0.0f, 0.0f})
                    .shadow(0.0f, 0.0f, 0.0f, {0.0f, 0.0f, 0.0f, 0.0f});
            }
            button.build();
        } else if (node.type == NodeType::Input) {
            auto inputStyle = applyStyleShadow(node, applyStyleFloats(node, applyStyleColors(node, components::InputStyle(canvasTheme(node.color)), {
                {"background", &components::InputStyle::background}, {"focused", &components::InputStyle::focused},
                {"border", &components::InputStyle::border}, {"focusBorder", &components::InputStyle::focusBorder},
                {"text", &components::InputStyle::text}, {"placeholder", &components::InputStyle::placeholder},
                {"cursor", &components::InputStyle::cursor}
            }), {{"styleRadius", &components::InputStyle::radius}}), &components::InputStyle::shadow);
            components::input(ui, base + ".input").size(width, height).value(node.valueText).placeholder(node.text)
                .theme(canvasTheme(node.color)).fontFamily(node.fontFamily).fontSize(node.fontSize * scale).inset(node.inset * scale)
                .multiline(node.multiline).style(inputStyle).build();
        } else if (node.type == NodeType::Checkbox) {
            auto checkboxStyle = applyStyleFloats(node, applyStyleColors(node, components::CheckboxStyle(canvasTheme(node.color)), {
                {"box", &components::CheckboxStyle::box}, {"boxHover", &components::CheckboxStyle::boxHover},
                {"boxPressed", &components::CheckboxStyle::boxPressed}, {"checked", &components::CheckboxStyle::checked},
                {"checkedHover", &components::CheckboxStyle::checkedHover}, {"checkedPressed", &components::CheckboxStyle::checkedPressed},
                {"border", &components::CheckboxStyle::border}, {"mark", &components::CheckboxStyle::mark},
                {"text", &components::CheckboxStyle::text}, {"rowHover", &components::CheckboxStyle::rowHover},
                {"rowPressed", &components::CheckboxStyle::rowPressed}
            }), {{"styleRadius", &components::CheckboxStyle::radius}});
            components::checkbox(ui, base + ".checkbox").size(width, height).text(node.text).checked(node.checked)
                .theme(canvasTheme(node.color)).fontSize(node.fontSize * scale).boxSize(node.controlSize * scale).style(checkboxStyle).build();
        } else if (node.type == NodeType::Radio) {
            auto radioStyle = applyStyleColors(node, components::RadioStyle(canvasTheme(node.color)), {
                {"outer", &components::RadioStyle::outer}, {"outerHover", &components::RadioStyle::outerHover},
                {"selected", &components::RadioStyle::selected}, {"border", &components::RadioStyle::border},
                {"text", &components::RadioStyle::text}, {"rowHover", &components::RadioStyle::rowHover},
                {"rowPressed", &components::RadioStyle::rowPressed}
            });
            components::radio(ui, base + ".radio").size(width, height).text(node.text).selected(node.checked)
                .theme(canvasTheme(node.color)).fontSize(node.fontSize * scale).dotSize(node.controlSize * scale).style(radioStyle).build();
        } else if (node.type == NodeType::Switch) {
            auto switchStyle = applyStyleColors(node, components::SwitchStyle(canvasTheme(node.color)), {
                {"off", &components::SwitchStyle::off}, {"on", &components::SwitchStyle::on},
                {"knob", &components::SwitchStyle::knob}, {"text", &components::SwitchStyle::text},
                {"rowHover", &components::SwitchStyle::rowHover}, {"rowPressed", &components::SwitchStyle::rowPressed}
            });
            components::toggleSwitch(ui, base + ".switch").size(width, height).text(node.text).checked(node.checked)
                .theme(canvasTheme(node.color)).fontSize(node.fontSize * scale)
                .trackSize(node.trackWidth * scale, node.trackHeight * scale).style(switchStyle).build();
        } else if (node.type == NodeType::Slider) {
            auto sliderStyle = applyStyleColors(node, components::SliderStyle(canvasTheme(node.color)), {
                {"track", &components::SliderStyle::track}, {"fill", &components::SliderStyle::fill}, {"knob", &components::SliderStyle::knob}
            });
            components::slider(ui, base + ".slider").size(width, height).value(node.value).theme(canvasTheme(node.color)).style(sliderStyle).build();
        } else if (node.type == NodeType::Progress) {
            auto progressStyle = applyStyleColors(node, components::ProgressStyle(canvasTheme(node.color)), {
                {"track", &components::ProgressStyle::track}, {"fill", &components::ProgressStyle::fill}
            });
            components::progress(ui, base + ".progress").size(width, height).value(node.value).theme(canvasTheme(node.color)).style(progressStyle).build();
        } else if (node.type == NodeType::Segmented) {
            auto segmentedStyle = applyStyleColors(node, components::SegmentedStyle(canvasTheme(node.color)), {
                {"background", &components::SegmentedStyle::background}, {"hover", &components::SegmentedStyle::hover},
                {"selected", &components::SegmentedStyle::selected}, {"text", &components::SegmentedStyle::text},
                {"selectedText", &components::SegmentedStyle::selectedText}, {"border", &components::SegmentedStyle::border}
            });
            components::segmented(ui, base + ".segmented").size(width, height).theme(canvasTheme(node.color))
                .items(splitList(node.itemsText)).selected(node.selectedIndex).fontSize(node.fontSize * scale).style(segmentedStyle).build();
        } else if (node.type == NodeType::Stepper) {
            auto stepperStyle = applyStyleShadow(node, applyStyleFloats(node, applyStyleColors(node, components::StepperStyle(canvasTheme(node.color)), {
                {"button", &components::StepperStyle::button}, {"buttonHover", &components::StepperStyle::buttonHover},
                {"buttonPressed", &components::StepperStyle::buttonPressed}, {"buttonDisabled", &components::StepperStyle::buttonDisabled},
                {"field", &components::StepperStyle::field}, {"fieldBorder", &components::StepperStyle::fieldBorder},
                {"text", &components::StepperStyle::text}, {"mutedText", &components::StepperStyle::mutedText}, {"accent", &components::StepperStyle::accent}
            }), {{"styleRadius", &components::StepperStyle::radius}}), &components::StepperStyle::shadow);
            components::stepper(ui, base + ".stepper").size(width, height).theme(canvasTheme(node.color))
                .value(node.selectedIndex).step(node.step).min(node.minimum).max(node.maximum).base(node.numberBase)
                .digits(node.digits).bitWidth(node.bitWidth).showBasePrefix(node.showBasePrefix).prefix(node.prefixText)
                .uppercase(node.uppercase).fontSize(node.fontSize * scale).style(stepperStyle).build();
        } else if (node.type == NodeType::Tabs) {
            auto tabsStyle = applyStyleColors(node, components::TabsStyle(canvasTheme(node.color)), {
                {"text", &components::TabsStyle::text}, {"hover", &components::TabsStyle::hover},
                {"selectedText", &components::TabsStyle::selectedText}, {"indicator", &components::TabsStyle::indicator},
                {"border", &components::TabsStyle::border}
            });
            components::tabs(ui, base + ".tabs").size(width, height).theme(canvasTheme(node.color))
                .items(splitList(node.itemsText)).selected(node.selectedIndex).fontSize(node.fontSize * scale).style(tabsStyle).build();
        } else if (node.type == NodeType::Dropdown) {
            auto dropdownStyle = applyStyleShadow(node, applyStyleFloats(node, applyStyleColors(node, components::DropdownStyle(canvasTheme(node.color)), {
                {"field", &components::DropdownStyle::field}, {"fieldHover", &components::DropdownStyle::fieldHover},
                {"fieldPressed", &components::DropdownStyle::fieldPressed}, {"popup", &components::DropdownStyle::popup},
                {"optionHover", &components::DropdownStyle::optionHover}, {"optionPressed", &components::DropdownStyle::optionPressed},
                {"selected", &components::DropdownStyle::selected}, {"text", &components::DropdownStyle::text},
                {"mutedText", &components::DropdownStyle::mutedText}, {"accent", &components::DropdownStyle::accent}, {"border", &components::DropdownStyle::border}
            }), {{"styleRadius", &components::DropdownStyle::radius}}), &components::DropdownStyle::shadow);
            const bool previewOpen = state.previewOverlayUid == node.uid;
            components::dropdown(ui, base + ".dropdown").size(width, height).theme(canvasTheme(node.color))
                .items(splitList(node.itemsText)).selected(node.selectedIndex).placeholder(node.messageText)
                .itemHeight(node.itemHeight * scale).open(previewOpen).zIndex(previewOpen ? 300 : 0)
                .style(dropdownStyle)
                .onChange([uid = node.uid](int value) {
                    if (Node* current = findNode(uid)) current->selectedIndex = value;
                })
                .onOpenChange([uid = node.uid](bool open) {
                    if (!open && state.previewOverlayUid == uid) state.previewOverlayUid = -1;
                }).build();
        } else if (node.type == NodeType::DataTable) {
            auto tableStyle = applyStyleFloats(node, applyStyleColors(node, components::DataTableStyle(canvasTheme(node.color)), {
                {"background", &components::DataTableStyle::background}, {"header", &components::DataTableStyle::header},
                {"row", &components::DataTableStyle::row}, {"rowAlt", &components::DataTableStyle::rowAlt},
                {"rowHover", &components::DataTableStyle::rowHover}, {"text", &components::DataTableStyle::text},
                {"mutedText", &components::DataTableStyle::mutedText}, {"accent", &components::DataTableStyle::accent},
                {"border", &components::DataTableStyle::border}, {"divider", &components::DataTableStyle::divider}
            }), {{"styleRadius", &components::DataTableStyle::radius}});
            components::dataTable(ui, base + ".table").size(width, height).theme(canvasTheme(node.color))
                .columns(splitList(node.labelsText)).rows(tableRows(node)).style(tableStyle).build();
        } else if (node.type == NodeType::LineChart) {
            auto lineStyle = applyStyleShadow(node, applyStyleFloats(node, applyStyleColors(node, components::LineChartStyle(canvasTheme(node.color)), {
                {"background", &components::LineChartStyle::background}, {"title", &components::LineChartStyle::title}, {"label", &components::LineChartStyle::label},
                {"grid", &components::LineChartStyle::grid}, {"line", &components::LineChartStyle::line}, {"point", &components::LineChartStyle::point},
                {"pointHover", &components::LineChartStyle::pointHover}, {"pointPressed", &components::LineChartStyle::pointPressed},
                {"tooltipBackground", &components::LineChartStyle::tooltipBackground}, {"tooltipText", &components::LineChartStyle::tooltipText},
                {"border", &components::LineChartStyle::border}
            }), {{"styleRadius", &components::LineChartStyle::radius}}), &components::LineChartStyle::shadow);
            components::lineChart(ui, base + ".line").size(width, height).theme(canvasTheme(node.color))
                .title(node.text).values(splitValues(node.valuesText)).labels(splitList(node.labelsText)).style(lineStyle).build();
        } else if (node.type == NodeType::BarChart) {
            auto barStyle = applyStyleShadow(node, applyStyleFloats(node, applyStyleColors(node, components::BarChartStyle(canvasTheme(node.color)), {
                {"background", &components::BarChartStyle::background}, {"title", &components::BarChartStyle::title},
                {"label", &components::BarChartStyle::label}, {"grid", &components::BarChartStyle::grid},
                {"tooltipBackground", &components::BarChartStyle::tooltipBackground}, {"tooltipText", &components::BarChartStyle::tooltipText},
                {"border", &components::BarChartStyle::border}
            }), {{"styleRadius", &components::BarChartStyle::radius}}), &components::BarChartStyle::shadow);
            components::barChart(ui, base + ".bar").size(width, height).theme(canvasTheme(node.color))
                .title(node.text).values(splitValues(node.valuesText)).labels(splitList(node.labelsText))
                .colors(node.palette).style(barStyle).build();
        } else if (node.type == NodeType::PieChart) {
            auto pieStyle = applyStyleShadow(node, applyStyleFloats(node, applyStyleColors(node, components::PieChartStyle(canvasTheme(node.color)), {
                {"background", &components::PieChartStyle::background}, {"title", &components::PieChartStyle::title},
                {"tooltipBackground", &components::PieChartStyle::tooltipBackground}, {"tooltipText", &components::PieChartStyle::tooltipText},
                {"border", &components::PieChartStyle::border}
            }), {{"styleRadius", &components::PieChartStyle::radius}}), &components::PieChartStyle::shadow);
            components::pieChart(ui, base + ".pie").size(width, height).theme(canvasTheme(node.color))
                .title(node.text).values(splitValues(node.valuesText)).labels(splitList(node.labelsText))
                .colors(node.palette).style(pieStyle).build();
        } else if (node.type == NodeType::Navbar) {
            components::navbar(ui, base + ".navbar").size(width, height).theme(canvasTheme(node.color))
                .compact(node.compact).brand(node.text, node.brandIcon).subtitle(node.messageText)
                .selected(node.selectedIndex).items(navbarItems(node))
                .footer(node.footerText, node.footerIcon, [] {}).onChange([](int) {}).build();
        } else if (node.type == NodeType::HeartSwitch) {
            auto heartStyle = applyStyleColors(node, components::workshop::heartSwitchStyle(canvasTheme(node.color)), {
                {"heart", &components::workshop::HeartSwitchStyle::heart}, {"hover", &components::workshop::HeartSwitchStyle::hover},
                {"pressed", &components::workshop::HeartSwitchStyle::pressed}
            });
            components::workshop::heartSwitch(ui, base + ".heart").size(width, height)
                .theme(canvasTheme(node.color)).style(heartStyle).checked(node.checked).disabled(node.disabled).build();
        } else if (node.type == NodeType::NeumorphicButton) {
            auto neoStyle = applyStyleColors(node,
                components::workshop::neumorphicButtonStyle(canvasTheme(node.color)), {
                    {"surface", &components::workshop::NeumorphicButtonStyle::surface}, {"hover", &components::workshop::NeumorphicButtonStyle::hover},
                    {"pressed", &components::workshop::NeumorphicButtonStyle::pressed}, {"text", &components::workshop::NeumorphicButtonStyle::text},
                    {"pressedText", &components::workshop::NeumorphicButtonStyle::pressedText}, {"border", &components::workshop::NeumorphicButtonStyle::border},
                    {"darkShadow", &components::workshop::NeumorphicButtonStyle::darkShadow}, {"lightShadow", &components::workshop::NeumorphicButtonStyle::lightShadow},
                    {"innerDark", &components::workshop::NeumorphicButtonStyle::innerDark}, {"innerLight", &components::workshop::NeumorphicButtonStyle::innerLight}
                });
            components::workshop::neumorphicButton(ui, base + ".neo").size(width, height)
                .theme(canvasTheme(node.color)).text(node.text).fontSize(node.fontSize * scale)
                .style(neoStyle).radius(node.radius * scale).pressScale(node.pressScale).disabled(node.disabled).build();
        } else if (node.type == NodeType::TiltCard) {
            auto tiltStyle = applyStyleFloats(node, applyStyleColors(node,
                components::workshop::tiltCardStyle(canvasTheme(node.color)), {
                    {"surface", &components::workshop::TiltCardStyle::surface}, {"surfaceTop", &components::workshop::TiltCardStyle::surfaceTop},
                    {"accent", &components::workshop::TiltCardStyle::accent}, {"text", &components::workshop::TiltCardStyle::text},
                    {"muted", &components::workshop::TiltCardStyle::muted}, {"border", &components::workshop::TiltCardStyle::border},
                    {"shadow", &components::workshop::TiltCardStyle::shadow}
                }), {{"styleRadius", &components::workshop::TiltCardStyle::radius}});
            components::workshop::tiltCard(ui, base + ".tilt").size(width, height)
                .theme(canvasTheme(node.color)).style(tiltStyle).title(node.text).subtitle(node.messageText).maxTilt(node.maxTilt).build();
        } else if (node.type == NodeType::Carousel) {
            auto carouselStyle = applyStyleShadow(node, applyStyleFloats(node, applyStyleColors(node, components::CarouselStyle(canvasTheme(node.color)), {
                {"background", &components::CarouselStyle::background}, {"border", &components::CarouselStyle::border},
                {"text", &components::CarouselStyle::text}, {"mutedText", &components::CarouselStyle::mutedText},
                {"overlayTop", &components::CarouselStyle::overlayTop}, {"overlayBottom", &components::CarouselStyle::overlayBottom},
                {"indicator", &components::CarouselStyle::indicator}, {"activeIndicator", &components::CarouselStyle::activeIndicator}
            }), {{"styleRadius", &components::CarouselStyle::radius}}), &components::CarouselStyle::shadow);
            components::carousel(ui, base + ".carousel").size(width, height).theme(canvasTheme(node.color))
                .items(carouselItems(node)).index(static_cast<float>(node.selectedIndex))
                .cardWidthRatio(node.cardWidthRatio).overlap(node.overlap).parallax(node.parallax).style(carouselStyle).build();
        } else if (node.type == NodeType::CardSlider) {
            auto cardSliderStyle = applyStyleFloats(node, applyStyleColors(node,
                components::workshop::CardSliderStyle(canvasTheme(node.color)), {
                    {"background", &components::workshop::CardSliderStyle::background}, {"overlay", &components::workshop::CardSliderStyle::overlay},
                    {"title", &components::workshop::CardSliderStyle::title}, {"subtitle", &components::workshop::CardSliderStyle::subtitle},
                    {"description", &components::workshop::CardSliderStyle::description}, {"accent", &components::workshop::CardSliderStyle::accent},
                    {"shadow", &components::workshop::CardSliderStyle::shadow}
                }), {{"styleRadius", &components::workshop::CardSliderStyle::radius}});
            cardSliderStyle.button.normal = styleColor(node, "button.normal", cardSliderStyle.button.normal);
            cardSliderStyle.button.hover = styleColor(node, "button.hover", cardSliderStyle.button.hover);
            cardSliderStyle.button.pressed = styleColor(node, "button.pressed", cardSliderStyle.button.pressed);
            cardSliderStyle.button.text = styleColor(node, "button.text", cardSliderStyle.button.text);
            cardSliderStyle.button.icon = styleColor(node, "button.icon", cardSliderStyle.button.icon);
            cardSliderStyle.button.border.color = styleColor(node, "button.border.color", cardSliderStyle.button.border.color);
            cardSliderStyle.button.border.width = styleFloat(node, "button.border.width", cardSliderStyle.button.border.width);
            cardSliderStyle.button.radius = styleFloat(node, "button.radius", cardSliderStyle.button.radius);
            cardSliderStyle.button.opacity = styleFloat(node, "button.opacity", cardSliderStyle.button.opacity);
            cardSliderStyle.button.pressScale = styleFloat(node, "button.pressScale", cardSliderStyle.button.pressScale);
            if (node.componentShadowOverride) {
                cardSliderStyle.button.shadow = {node.shadowEnabled, {node.shadowOffsetX * scale, node.shadowOffsetY * scale},
                    node.shadowBlur * scale, node.shadowSpread * scale, node.shadowColor, node.insetShadow};
            }
            components::workshop::cardSlider(ui, base + ".cardSlider").size(width, height).theme(canvasTheme(node.color))
                .items(cardSliderItems(node)).currentIndex(node.selectedIndex).duration(node.duration).interval(node.interval)
                .cardSpacing(node.cardSpacing * scale).autoPlay(node.autoPlay).background(node.backgroundEnabled)
                .tilt(node.tiltEnabled).style(cardSliderStyle).build();
        } else if (node.type == NodeType::MouseArea) {
            components::mouseArea(ui, base + ".mouseArea").size(width, height)
                .color({node.color.r, node.color.g, node.color.b, 0.18f * node.opacity}).radius(node.radius * scale)
                .cursor(node.cursorShape == 0 ? core::CursorShape::Arrow : core::CursorShape::Hand)
                .disabled(node.disabled).preserveFocusOnPress(node.preserveFocusOnPress)
                .scrollStep(node.scrollStep * scale).maxScrollStep(node.maxScrollStep * scale)
                .dragThreshold(node.dragThreshold * scale).suppressClickAfterDrag(node.suppressClickAfterDrag)
                .onTap([] {}).build();
        } else if (node.type == NodeType::Tooltip) {
            components::button(ui, base + ".tooltip.source").size(width, height)
                .theme(canvasTheme(node.color), false).text("Hover for tooltip")
                .onClick([uid = node.uid] { state.selectedUid = uid; }).build();
        } else if (node.type == NodeType::Image) {
            auto imageStyle = applyStyleColors(node, components::ImageStyle(canvasTheme(node.color)), {
                {"tint", &components::ImageStyle::tint}
            });
            components::image(ui, base + ".image", imageStyle).size(width, height).source(node.text).radius(node.radius * scale)
                .opacity(node.opacity).scale(node.scaleX, node.scaleY).rotate(node.rotation * 0.0174532925f).build();
        } else if (node.type == NodeType::Polygon) {
            std::vector<eui::Vec2> points;
            const int sides = std::clamp(node.polygonSides, 3, 32);
            const float radians = -1.5707963268f;
            for (int index = 0; index < sides; ++index) {
                const float angle = radians + 6.2831853072f * static_cast<float>(index) / static_cast<float>(sides);
                points.push_back({width * (0.5f + 0.5f * std::cos(angle)), height * (0.5f + 0.5f * std::sin(angle))});
            }
            auto shape = ui.polygon(base + ".polygon").size(width, height).points(std::move(points)).color(color)
                .border(node.borderWidth * scale, node.borderColor).blur(node.blur * scale).opacity(node.opacity)
                .scale(node.scaleX, node.scaleY).rotate(node.rotation * 0.0174532925f);
            if (node.gradientEnabled) {
                shape.gradient(color, node.gradientColor, node.gradientHorizontal ? core::GradientDirection::Horizontal : core::GradientDirection::Vertical);
            }
            if (node.shadowEnabled) {
                shape.shadow({true, {node.shadowOffsetX * scale, node.shadowOffsetY * scale}, node.shadowBlur * scale,
                    node.shadowSpread * scale, node.shadowColor, node.insetShadow});
            }
            shape.build();
        } else if (node.type == NodeType::Svg) {
            ui.svg(base + ".svg").size(width, height).source(node.text)
                .radius(node.radius * scale).opacity(node.opacity).scale(node.scaleX, node.scaleY)
                .rotate(node.rotation * 0.0174532925f).build();
        } else if (isOverlayPreview(node.type)) {
            components::button(ui, base + ".preview.trigger").size(width, height).text("Open " + std::string(typeName(node.type)))
                .fontSize(std::min(22.0f * scale, height * 0.46f)).theme(canvasTheme(node.color), false)
                .onClick([uid = node.uid] { state.selectedUid = uid; state.previewOverlayUid = uid; }).build();
        } else {
            ui.rect(base + ".component.bg").size(width, height).color({0.93f, 0.95f, 0.98f, 1.0f})
                .radius(node.radius * scale).border(1.0f, {0.74f, 0.78f, 0.84f, 1.0f}).build();
            ui.text(base + ".component.label").size(width, height).text(typeName(node.type))
                .fontSize(std::max(10.0f, node.fontSize * scale)).lineHeight(20.0f * scale).fontWeight(680)
                .color({0.22f, 0.27f, 0.35f, 1.0f}).horizontalAlign(eui::HorizontalAlign::Center)
                .verticalAlign(eui::VerticalAlign::Center).build();
        }
        if (selected) {
            ui.rect(base + ".selection").size(width, height).color({0.0f, 0.0f, 0.0f, 0.0f})
                .radius(node.radius * scale).border(2.0f, kAccent).build();
            ui.rect(base + ".handle").position(width - 5.0f, height - 5.0f).size(10.0f, 10.0f)
                .color(kAccent).radius(2.0f).build();
        }
        components::mouseArea(ui, base + ".drag").size(width, height)
            .zIndex(isLayout(node.type) ? 40 : 50)
            .color({0.0f, 0.0f, 0.0f, 0.0f}).cursor(core::CursorShape::Hand).suppressClickAfterDrag(true)
            .onTap([uid = node.uid, opensComponentPreview] {
                state.selectedUid = uid;
                if (opensComponentPreview) state.previewOverlayUid = uid;
            })
            .onContextMenu([uid = node.uid](const components::MouseEvent& event) {
                state.selectedUid = uid;
                state.elementMenuUid = uid;
                state.elementMenuX = event.globalX;
                state.elementMenuY = event.globalY;
                state.elementMenuOpen = true;
            })
            .onDragStart([uid = node.uid](const components::MouseEvent&) {
                state.selectedUid = uid;
                if (const Node* current = findNode(uid); current && current->locked) return;
                state.dragging = true;
                if (Node* current = selectedNode()) {
                    state.dragStartX = current->x;
                    state.dragStartY = current->y;
                }
            })
            .onDrag([uid = node.uid, scale](const components::MouseDragEvent& event) {
                if (Node* current = selectedNode(); current && current->uid == uid) {
                    if (current->locked) return;
                    if (current->parentUid >= 0 && current->participatesInLayout) return;
                    const Node* parent = findNode(current->parentUid);
                    const float boundsWidth = parent ? parent->width : state.canvasWidth;
                    const float boundsHeight = parent ? parent->height : state.canvasHeight;
                    current->x = std::clamp(state.dragStartX + event.totalX / scale, 0.0f,
                        std::max(0.0f, boundsWidth - current->width));
                    current->y = std::clamp(state.dragStartY + event.totalY / scale, 0.0f,
                        std::max(0.0f, boundsHeight - current->height));
                }
            })
            .onDragEnd([](const components::MouseDragEvent&) {
                state.dragging = false;
            }).build();
        if (selected && !node.locked) {
            components::mouseArea(ui, base + ".resize").position(width - 10.0f, height - 10.0f).size(20.0f, 20.0f).zIndex(100)
                .color({0.0f, 0.0f, 0.0f, 0.0f}).cursor(core::CursorShape::Hand)
                .onDragStart([uid = node.uid](const components::MouseEvent&) {
                    state.selectedUid = uid;
                    state.dragging = true;
                    if (Node* current = findNode(uid)) {
                        state.resizeStartWidth = current->width;
                        state.resizeStartHeight = current->height;
                    }
                })
                .onDrag([uid = node.uid, scale](const components::MouseDragEvent& event) {
                    Node* current = findNode(uid);
                    if (!current) return;
                    const Node* parent = findNode(current->parentUid);
                    const float boundsWidth = parent ? parent->width : state.canvasWidth;
                    const float boundsHeight = parent ? parent->height : state.canvasHeight;
                    current->width = std::clamp(state.resizeStartWidth + event.totalX / scale, 1.0f,
                        std::max(1.0f, boundsWidth - current->x));
                    current->height = std::clamp(state.resizeStartHeight + event.totalY / scale, 1.0f,
                        std::max(1.0f, boundsHeight - current->y));
                })
                .onDragEnd([](const components::MouseDragEvent&) { state.dragging = false; }).build();
        }
    }).build();
}

void composeCanvas(eui::Ui& ui, float width, float height) {
    const float availableWidth = std::max(200.0f, width - 64.0f);
    const float availableHeight = std::max(160.0f, height - 116.0f);
    const float scale = std::max(0.25f, std::min({1.0f, availableWidth / state.canvasWidth, availableHeight / state.canvasHeight}));
    const float canvasWidth = state.canvasWidth * scale;
    const float canvasHeight = state.canvasHeight * scale;
    const float canvasX = (width - canvasWidth) * 0.5f;
    const float canvasY = 74.0f + std::max(0.0f, (availableHeight - canvasHeight) * 0.5f);

    ui.stack("workspace").position(kLeftWidth, kTopBarHeight).size(width, height).content([&] {
        ui.rect("workspace.bg").size(width, height).color(kWorkspace).build();
        if (state.dragging) {
            ui.rect("workspace.drag.frame").size(1.0f, 1.0f).opacity(0.0f).onFrame([](float) {}).build();
        }
        ui.text("workspace.label").position(24.0f, 18.0f).size(220.0f, 24.0f).text("Canvas")
            .fontSize(15.0f).lineHeight(20.0f).fontWeight(760).color(kText).build();
        ui.text("workspace.status").position(92.0f, 19.0f).size(std::max(80.0f, width - 286.0f), 22.0f)
            .text(state.status).fontSize(11.0f).lineHeight(16.0f).color(kMuted)
            .verticalAlign(eui::VerticalAlign::Center).build();
        ui.text("workspace.zoom").position(width - 170.0f, 18.0f).size(146.0f, 24.0f)
            .text(std::to_string(static_cast<int>(std::round(scale * 100.0f))) + "%  |  " +
                  std::to_string(static_cast<int>(state.canvasWidth)) + " x " + std::to_string(static_cast<int>(state.canvasHeight)))
            .fontSize(12.0f).lineHeight(18.0f).color(kMuted).horizontalAlign(eui::HorizontalAlign::Right).build();
        ui.stack("canvas.shadow").position(canvasX - 8.0f, canvasY - 8.0f).size(canvasWidth + 16.0f, canvasHeight + 16.0f)
            .content([&] {
                ui.rect("canvas.shadow.rect").size(canvasWidth + 16.0f, canvasHeight + 16.0f).color({0.0f, 0.0f, 0.0f, 0.0f})
                    .radius(8.0f).shadow(30.0f, 0.0f, 10.0f, {0.0f, 0.0f, 0.0f, 0.42f}).build();
            }).build();
        ui.stack("canvas.window").position(canvasX, canvasY).size(canvasWidth, canvasHeight).clip().content([&] {
            ui.rect("canvas.page").size(canvasWidth, canvasHeight).color(state.backgroundColor).build();
            for (Node& node : state.nodes) {
                if (node.parentUid < 0 || findNode(node.parentUid) == nullptr) composeNodeVisual(ui, node, scale);
            }
            for (const Node& node : state.nodes) {
                if (node.type != NodeType::Tooltip) continue;
                const eui::Vec2 absolute = absolutePosition(node);
                const std::string base = "design.node." + std::to_string(node.uid);
                components::tooltip(ui, base + ".tooltip").source(base + ".drag")
                    .value(node.text).anchor((absolute.x + node.width * 0.5f) * scale, absolute.y * scale)
                    .bounds(canvasWidth, canvasHeight).theme(canvasTheme(node.color)).build();
            }
            composeSelectedComponentPreview(ui, canvasWidth, canvasHeight, scale);
        }).build();
        components::mouseArea(ui, "canvas.resize").position(canvasX + canvasWidth - 10.0f, canvasY + canvasHeight - 10.0f)
            .size(20.0f, 20.0f).zIndex(100).color(kAccent).radius(4.0f).cursor(core::CursorShape::Hand)
            .onDragStart([](const components::MouseEvent&) {
                state.dragging = true;
                state.resizeStartWidth = state.canvasWidth;
                state.resizeStartHeight = state.canvasHeight;
            })
            .onDrag([scale](const components::MouseDragEvent& event) {
                state.canvasWidth = std::clamp(state.resizeStartWidth + event.totalX / scale, 320.0f, 1920.0f);
                state.canvasHeight = std::clamp(state.resizeStartHeight + event.totalY / scale, 240.0f, 1200.0f);
            }).onDragEnd([](const components::MouseDragEvent&) { state.dragging = false; }).build();
    }).build();
}

void propertyStepper(eui::Ui& ui, const std::string& id, const std::string& label, float y,
                     long long value, long long minValue, long long maxValue,
                     const std::function<void(long long)>& onChange) {
    smallLabel(ui, id + ".label", label, kPanelPadding, y, 72.0f);
    ui.stack(id + ".wrap").position(92.0f, y - 8.0f).size(kRightWidth - 110.0f, 36.0f)
        .content([&] {
            numericInput(ui, id, 0.0f, 0.0f, kRightWidth - 110.0f, 36.0f,
                value, minValue, maxValue, onChange);
        }).build();
}

void propertyNumber(eui::Ui& ui, const std::string& id, const std::string& label, float y,
                    float value, float minValue, float maxValue, const std::function<void(float)>& onChange) {
    smallLabel(ui, id + ".label", label, kPanelPadding, y, 72.0f);
    ui.stack(id + ".wrap").position(92.0f, y - 8.0f).size(kRightWidth - 110.0f, 36.0f).content([&] {
        numberInput(ui, id, 0.0f, 0.0f, kRightWidth - 110.0f, 36.0f, value, minValue, maxValue, onChange);
    }).build();
}

void propertyTextInput(eui::Ui& ui, const std::string& id, const std::string& label, float y,
                       const std::string& value, const std::function<void(const std::string&)>& onChange) {
    smallLabel(ui, id + ".label", label, kPanelPadding, y, kRightWidth - kPanelPadding * 2.0f);
    components::input(ui, id).position(kPanelPadding, y + 22.0f).size(kRightWidth - kPanelPadding * 2.0f, 40.0f)
        .theme(designerTheme()).value(value).onChange(onChange).build();
}

void propertyToggle(eui::Ui& ui, const std::string& id, const std::string& label, float y, bool value,
                    const std::function<void(bool)>& onChange) {
    ui.stack(id + ".wrap").position(kPanelPadding, y).size(kRightWidth - kPanelPadding * 2.0f, 36.0f).content([&] {
        components::checkbox(ui, id).size(kRightWidth - kPanelPadding * 2.0f, 36.0f).theme(designerTheme())
            .text(label).checked(value).onChange(onChange).build();
    }).build();
}

void propertyColorSwatch(eui::Ui& ui, const std::string& id, const std::string& label, float y,
                         const eui::Color& color, int target) {
    smallLabel(ui, id + ".label", label, kPanelPadding, y + 8.0f, 92.0f);
    ui.rect(id + ".swatch").position(112.0f, y).size(kRightWidth - 130.0f, 36.0f)
        .states(color, color, color).radius(7.0f).border(2.0f, kBorder)
        .onClick([target] { state.elementColorTarget = target; state.elementPickerOpen = true; }).build();
}

void propertyStyleColorSwatch(eui::Ui& ui, const std::string& id, const std::string& label, float y,
                              const eui::Color& color, std::string key) {
    smallLabel(ui, id + ".label", label, kPanelPadding, y + 8.0f, 92.0f);
    ui.rect(id + ".swatch").position(112.0f, y).size(kRightWidth - 130.0f, 36.0f)
        .states(color, color, color).radius(7.0f).border(2.0f, kBorder)
        .onClick([key = std::move(key)] {
            state.elementColorTarget = 1000;
            state.elementStyleColorKey = key;
            state.elementPickerOpen = true;
        }).build();
}

void composeProperties(eui::Ui& ui, float height) {
    ui.stack("properties").position(0.0f, kTopBarHeight).size(kRightWidth, height).content([&] {
        ui.rect("properties.bg").size(kRightWidth, height).color(kPanel).build();
        ui.rect("properties.rule").size(1.0f, height).color(kBorder).build();
        components::scrollView(ui, "properties.scroll").size(kRightWidth, height).bind(state.propertiesScroll)
            .theme(designerTheme()).step(46.0f).scrollbarWidth(7.0f).scrollbarGap(5.0f)
            .content([&](eui::Ui& contentUi, float contentWidth, float) {
        contentUi.stack("properties.content").size(contentWidth, 10000.0f).content([&] {
        eui::Ui& ui = contentUi;
        ui.text("properties.title").position(kPanelPadding, 18.0f).size(250.0f, 30.0f)
            .text(selectedNode() ? "Element properties" : "No element selected").fontSize(20.0f).lineHeight(26.0f)
            .fontWeight(790).color(kText).build();

        Node* node = selectedNode();
        float codeSectionY = 1540.0f;
        if (!node) {
            ui.text("properties.empty").position(kPanelPadding, 64.0f).size(kRightWidth - kPanelPadding * 2.0f, 72.0f)
                .text("Select an item in the Elements panel or on the canvas. Page properties are available in the top bar.")
                .fontSize(12.0f).lineHeight(18.0f).wrap(true).color(kMuted).build();
        } else {
            ui.text("properties.type").position(kPanelPadding, 50.0f).size(260.0f, 22.0f).text(typeName(node->type))
                .fontSize(13.0f).lineHeight(18.0f).color(kAccent).build();
            sectionLabel(ui, "node.common.label", "COMMON PROPERTIES", kPanelPadding, 78.0f, 180.0f);
            smallLabel(ui, "node.id.label", "Element ID", kPanelPadding, 104.0f, 150.0f);
            components::input(ui, "node.id.input").position(kPanelPadding, 126.0f).size(kRightWidth - kPanelPadding * 2.0f, 40.0f)
                .theme(designerTheme()).value(node->id).onChange([uid = node->uid](const std::string& value) {
                    if (Node* current = selectedNode(); current && current->uid == uid) {
                        if (value.empty()) {
                            state.status = "Element ID cannot be empty";
                        } else if (nodeIdInUse(value, uid)) {
                            state.status = "Element ID already exists: " + value;
                        } else {
                            current->id = value;
                            state.status = "Element ID updated";
                        }
                    }
                }).build();
            propertyNumber(ui, "node.x", "X", 190.0f, node->x, -1920.0f, 1920.0f,
                [](float value) {
                    if (Node* current = selectedNode()) {
                        if (current->parentUid >= 0 && current->participatesInLayout) current->participatesInLayout = false;
                        current->x = value;
                        state.layoutDirty = true;
                    }
                });
            propertyNumber(ui, "node.y", "Y", 236.0f, node->y, -1200.0f, 1200.0f,
                [](float value) {
                    if (Node* current = selectedNode()) {
                        if (current->parentUid >= 0 && current->participatesInLayout) current->participatesInLayout = false;
                        current->y = value;
                        state.layoutDirty = true;
                    }
                });
            propertyNumber(ui, "node.width", "Width", 282.0f, node->width, 1.0f, 1920.0f,
                [](float value) { if (Node* current = selectedNode()) { current->width = value; state.layoutDirty = true; } });
            propertyNumber(ui, "node.height", "Height", 328.0f, node->height, 1.0f, 1200.0f,
                [](float value) { if (Node* current = selectedNode()) { current->height = value; state.layoutDirty = true; } });
            if (hasRadiusApi(node->type)) {
                propertyNumber(ui, "node.radius", "Radius", 374.0f, node->radius, 0.0f, 256.0f,
                    [](float value) { if (Node* current = selectedNode()) current->radius = value; });
            }
            if (hasColorApi(node->type)) {
                sectionLabel(ui, "node.color.label", "COLOR", kPanelPadding, 418.0f, 120.0f);
                ui.rect("node.color.current").position(kPanelPadding, 446.0f)
                    .size(kRightWidth - kPanelPadding * 2.0f, 40.0f).states(node->color, node->color, node->color)
                    .radius(7.0f).border(2.0f, kBorder).onClick([] { state.elementColorTarget = 0; state.elementPickerOpen = true; }).build();
            }

            sectionLabel(ui, "node.component.label", "COMPONENT PROPERTIES", kPanelPadding, 510.0f, 200.0f);
            float componentY = 542.0f;
            if (node->parentUid >= 0) {
                ui.stack("node.participates.wrap").position(kPanelPadding, componentY)
                    .size(kRightWidth - kPanelPadding * 2.0f, 36.0f).content([&] {
                        components::checkbox(ui, "node.participates").size(kRightWidth - kPanelPadding * 2.0f, 36.0f)
                            .theme(designerTheme()).text("Participate in parent layout").checked(node->participatesInLayout)
                            .onChange([](bool value) {
                                if (Node* current = selectedNode()) {
                                    current->participatesInLayout = value;
                                    state.layoutDirty = true;
                                }
                            }).build();
                    }).build();
                componentY += 48.0f;
            }
            if (isLayout(node->type)) {
                propertyNumber(ui, "node.gap", "Gap", componentY + 8.0f, node->gap, 0.0f, 512.0f,
                    [](float value) { if (Node* current = selectedNode()) { current->gap = value; state.layoutDirty = true; } });
                propertyNumber(ui, "node.padding", "Padding", componentY + 56.0f, node->padding, 0.0f, 512.0f,
                    [](float value) { if (Node* current = selectedNode()) { current->padding = value; state.layoutDirty = true; } });
                componentY += 104.0f;
                smallLabel(ui, "node.mainAlign.label", "Main", kPanelPadding, componentY + 8.0f, 72.0f);
                ui.stack("node.mainAlign.wrap").position(92.0f, componentY).size(kRightWidth - 110.0f, 36.0f).content([&] {
                    components::segmented(ui, "node.mainAlign").size(kRightWidth - 110.0f, 36.0f).theme(designerTheme())
                        .items({"Start", "Center", "End"}).selected(node->mainAlign)
                        .onChange([](int value) { if (Node* current = selectedNode()) { current->mainAlign = value; state.layoutDirty = true; } }).build();
                }).build();
                componentY += 48.0f;
                smallLabel(ui, "node.crossAlign.label", "Cross", kPanelPadding, componentY + 8.0f, 72.0f);
                ui.stack("node.crossAlign.wrap").position(92.0f, componentY).size(kRightWidth - 110.0f, 36.0f).content([&] {
                    components::segmented(ui, "node.crossAlign").size(kRightWidth - 110.0f, 36.0f).theme(designerTheme())
                        .items({"Start", "Center", "End"}).selected(node->crossAlign)
                        .onChange([](int value) { if (Node* current = selectedNode()) { current->crossAlign = value; state.layoutDirty = true; } }).build();
                }).build();
                componentY += 48.0f;
            }
            if (node->type == NodeType::Scroll || node->type == NodeType::ScrollView) {
                propertyNumber(ui, "node.scrollStep", "Scroll step", componentY + 8.0f,
                    node->scrollStep, 1.0f, 512.0f,
                    [](float value) { if (Node* current = selectedNode()) current->scrollStep = value; });
                propertyNumber(ui, "node.scrollbarWidth", "Scrollbar", componentY + 56.0f,
                    node->scrollbarWidth, 0.0f, 64.0f,
                    [](float value) { if (Node* current = selectedNode()) current->scrollbarWidth = value; });
                propertyNumber(ui, "node.scrollbarGap", "Scrollbar gap", componentY + 104.0f,
                    node->scrollbarGap, 0.0f, 64.0f,
                    [](float value) { if (Node* current = selectedNode()) current->scrollbarGap = value; });
                componentY += 152.0f;
            }
            if (node->type == NodeType::VirtualList) {
                propertyStepper(ui, "node.itemCount", "Items", componentY + 8.0f, node->itemCount, 0, 1000000,
                    [](long long value) { if (Node* current = selectedNode()) current->itemCount = static_cast<int>(value); });
                propertyNumber(ui, "node.rowHeight", "Row height", componentY + 56.0f, node->rowHeight, 1.0f, 1000.0f,
                    [](float value) { if (Node* current = selectedNode()) current->rowHeight = value; });
                propertyNumber(ui, "node.listStep", "Scroll step", componentY + 104.0f, node->scrollStep, 1.0f, 512.0f,
                    [](float value) { if (Node* current = selectedNode()) current->scrollStep = value; });
                propertyNumber(ui, "node.listScrollbar", "Scrollbar", componentY + 152.0f, node->scrollbarWidth, 0.0f, 64.0f,
                    [](float value) { if (Node* current = selectedNode()) current->scrollbarWidth = value; });
                propertyNumber(ui, "node.listScrollbarGap", "Scrollbar gap", componentY + 200.0f, node->scrollbarGap, 0.0f, 64.0f,
                    [](float value) { if (Node* current = selectedNode()) current->scrollbarGap = value; });
                propertyNumber(ui, "node.listOverscan", "Overscan", componentY + 248.0f, node->overscanViewports, 0.0f, 16.0f,
                    [](float value) { if (Node* current = selectedNode()) current->overscanViewports = value; });
                componentY += 296.0f;
            }
            if (node->type == NodeType::Segmented || node->type == NodeType::Tabs || node->type == NodeType::Dropdown ||
                node->type == NodeType::ContextMenu || node->type == NodeType::Carousel || node->type == NodeType::CardSlider ||
                node->type == NodeType::Navbar) {
                propertyTextInput(ui, "node.itemsText", "Items (comma separated)", componentY, node->itemsText,
                    [uid = node->uid](const std::string& value) {
                        if (Node* current = selectedNode(); current && current->uid == uid) current->itemsText = value;
                    });
                componentY += 76.0f;
            }
            if (node->type == NodeType::DataTable) {
                propertyTextInput(ui, "node.tableColumns", "Columns (comma separated)", componentY, node->labelsText,
                    [uid = node->uid](const std::string& value) {
                        if (Node* current = selectedNode(); current && current->uid == uid) current->labelsText = value;
                    });
                componentY += 76.0f;
                propertyTextInput(ui, "node.tableCells", "Cells (row order)", componentY, node->itemsText,
                    [uid = node->uid](const std::string& value) {
                        if (Node* current = selectedNode(); current && current->uid == uid) current->itemsText = value;
                    });
                componentY += 76.0f;
            }
            if (node->type == NodeType::LineChart || node->type == NodeType::BarChart || node->type == NodeType::PieChart) {
                propertyTextInput(ui, "node.chartLabels", "Labels (comma separated)", componentY, node->labelsText,
                    [uid = node->uid](const std::string& value) {
                        if (Node* current = selectedNode(); current && current->uid == uid) current->labelsText = value;
                    });
                componentY += 76.0f;
                propertyTextInput(ui, "node.chartValues", "Values (comma separated)", componentY, node->valuesText,
                    [uid = node->uid](const std::string& value) {
                        if (Node* current = selectedNode(); current && current->uid == uid) current->valuesText = value;
                    });
                componentY += 76.0f;
                if (node->type == NodeType::BarChart || node->type == NodeType::PieChart) {
                    const std::size_t paletteCount = 4;
                    while (node->palette.size() < paletteCount) node->palette.push_back(kAccent);
                    for (std::size_t index = 0; index < paletteCount; ++index) {
                        propertyColorSwatch(ui, "node.chartPalette." + std::to_string(index),
                            "Palette " + std::to_string(index + 1), componentY,
                            node->palette[index], 100 + static_cast<int>(index));
                        componentY += 48.0f;
                    }
                }
            }
            if (node->type == NodeType::ColorPicker) {
                for (std::size_t index = 0; index < node->palette.size(); ++index) {
                    propertyColorSwatch(ui, "node.colorPickerPalette." + std::to_string(index),
                        "Palette " + std::to_string(index + 1), componentY,
                        node->palette[index], 100 + static_cast<int>(index));
                    componentY += 48.0f;
                }
            }
            if (node->type == NodeType::Carousel) {
                propertyNumber(ui, "node.cardWidthRatio", "Card ratio", componentY + 8.0f, node->cardWidthRatio, 0.32f, 1.0f,
                    [](float value) { if (Node* current = selectedNode()) current->cardWidthRatio = value; });
                propertyNumber(ui, "node.overlap", "Overlap", componentY + 56.0f, node->overlap, 0.0f, 0.60f,
                    [](float value) { if (Node* current = selectedNode()) current->overlap = value; });
                propertyNumber(ui, "node.parallax", "Parallax", componentY + 104.0f, node->parallax, 0.0f, 10.0f,
                    [](float value) { if (Node* current = selectedNode()) current->parallax = value; });
                componentY += 144.0f;
            }
            if (node->type == NodeType::CardSlider) {
                propertyNumber(ui, "node.duration", "Duration", componentY + 8.0f, node->duration, 0.05f, 60.0f,
                    [](float value) { if (Node* current = selectedNode()) current->duration = value; });
                propertyNumber(ui, "node.interval", "Interval", componentY + 56.0f, node->interval, 0.10f, 3600.0f,
                    [](float value) { if (Node* current = selectedNode()) current->interval = value; });
                propertyNumber(ui, "node.cardSpacing", "Spacing", componentY + 104.0f, node->cardSpacing, 0.0f, 512.0f,
                    [](float value) { if (Node* current = selectedNode()) current->cardSpacing = value; });
                componentY += 144.0f;
                propertyToggle(ui, "node.backgroundEnabled", "Background", componentY, node->backgroundEnabled,
                    [](bool value) { if (Node* current = selectedNode()) current->backgroundEnabled = value; });
                componentY += 44.0f;
                propertyToggle(ui, "node.tiltEnabled", "Tilt", componentY, node->tiltEnabled,
                    [](bool value) { if (Node* current = selectedNode()) current->tiltEnabled = value; });
                componentY += 44.0f;
            }
            if (node->type == NodeType::Button) {
                smallLabel(ui, "node.buttonStyle.label", "Style", kPanelPadding, componentY + 8.0f, 72.0f);
                ui.stack("node.buttonStyle.wrap").position(92.0f, componentY).size(kRightWidth - 110.0f, 36.0f).content([&] {
                    components::segmented(ui, "node.buttonStyle").size(kRightWidth - 110.0f, 36.0f).theme(designerTheme())
                        .items({"Filled", "Outline", "Ghost"}).selected(node->buttonStyle)
                        .onChange([](int value) { if (Node* current = selectedNode()) current->buttonStyle = value; }).build();
                }).build();
                componentY += 48.0f;
                propertyStepper(ui, "node.buttonIcon", "Icon", componentY + 8.0f, node->icon, 0, 0x10FFFF,
                    [](long long value) { if (Node* current = selectedNode()) current->icon = static_cast<unsigned int>(value); });
                propertyNumber(ui, "node.buttonIconSize", "Icon size", componentY + 56.0f, node->iconSize, 1.0f, 256.0f,
                    [](float value) { if (Node* current = selectedNode()) current->iconSize = value; });
                componentY += 96.0f;
                propertyColorSwatch(ui, "node.foregroundColor", "Text color", componentY, node->foregroundColor, 4);
                componentY += 48.0f;
                propertyNumber(ui, "node.pressScale", "Press scale", componentY + 8.0f, node->pressScale, 0.80f, 1.0f,
                    [](float value) { if (Node* current = selectedNode()) current->pressScale = value; });
                componentY += 48.0f;
                propertyToggle(ui, "node.buttonPreserveFocus", "Preserve focus on press", componentY,
                    node->preserveFocusOnPress,
                    [](bool value) { if (Node* current = selectedNode()) current->preserveFocusOnPress = value; });
                componentY += 44.0f;
                propertyNumber(ui, "node.buttonBorder", "Border", componentY + 8.0f, node->borderWidth, 0.0f, 64.0f,
                    [](float value) { if (Node* current = selectedNode()) current->borderWidth = value; });
                componentY += 48.0f;
                propertyColorSwatch(ui, "node.buttonBorderColor", "Border color", componentY, node->borderColor, 2);
                componentY += 48.0f;
                propertyToggle(ui, "node.buttonShadow", "Shadow", componentY, node->shadowEnabled,
                    [](bool value) { if (Node* current = selectedNode()) current->shadowEnabled = value; });
                componentY += 44.0f;
                if (node->shadowEnabled) {
                    propertyColorSwatch(ui, "node.buttonShadowColor", "Shadow color", componentY, node->shadowColor, 3);
                    componentY += 48.0f;
                    propertyNumber(ui, "node.buttonShadowBlur", "Shadow blur", componentY + 8.0f, node->shadowBlur, 0.0f, 256.0f,
                        [](float value) { if (Node* current = selectedNode()) current->shadowBlur = value; });
                    propertyNumber(ui, "node.buttonShadowX", "Offset X", componentY + 56.0f, node->shadowOffsetX, -512.0f, 512.0f,
                        [](float value) { if (Node* current = selectedNode()) current->shadowOffsetX = value; });
                    propertyNumber(ui, "node.buttonShadowY", "Offset Y", componentY + 104.0f, node->shadowOffsetY, -512.0f, 512.0f,
                        [](float value) { if (Node* current = selectedNode()) current->shadowOffsetY = value; });
                    componentY += 144.0f;
                }
                propertyToggle(ui, "node.cardGradient", "Gradient", componentY, node->gradientEnabled,
                    [](bool value) { if (Node* current = selectedNode()) current->gradientEnabled = value; });
                componentY += 44.0f;
                if (node->gradientEnabled) {
                    propertyColorSwatch(ui, "node.cardGradientColor", "Gradient end", componentY, node->gradientColor, 1);
                    componentY += 48.0f;
                    smallLabel(ui, "node.cardGradientDirection.label", "Direction", kPanelPadding, componentY + 8.0f, 72.0f);
                    ui.stack("node.cardGradientDirection.wrap").position(92.0f, componentY).size(kRightWidth - 110.0f, 36.0f).content([&] {
                        components::segmented(ui, "node.cardGradientDirection").size(kRightWidth - 110.0f, 36.0f).theme(designerTheme())
                            .items({"Vertical", "Horizontal"}).selected(node->gradientHorizontal ? 1 : 0)
                            .onChange([](int value) { if (Node* current = selectedNode()) current->gradientHorizontal = value == 1; }).build();
                    }).build();
                    componentY += 48.0f;
                }
            }
            if (node->type == NodeType::Dialog || node->type == NodeType::Toast || node->type == NodeType::Sidebar ||
                node->type == NodeType::Navbar || node->type == NodeType::TiltCard || node->type == NodeType::Dropdown) {
                const char* messageLabel = node->type == NodeType::Dropdown ? "Placeholder" :
                    node->type == NodeType::Navbar || node->type == NodeType::TiltCard ? "Subtitle" : "Message";
                propertyTextInput(ui, "node.messageText", messageLabel,
                    componentY, node->messageText, [uid = node->uid](const std::string& value) {
                        if (Node* current = selectedNode(); current && current->uid == uid) current->messageText = value;
                    });
                componentY += 76.0f;
            }
            if (node->type == NodeType::Navbar) {
                propertyStepper(ui, "node.navbarBrandIcon", "Brand icon", componentY + 8.0f,
                    node->brandIcon, 0, 0x10FFFF,
                    [](long long value) { if (Node* current = selectedNode()) current->brandIcon = static_cast<unsigned int>(value); });
                propertyStepper(ui, "node.navbarItemIcon", "Item icon", componentY + 56.0f,
                    node->itemIcon, 0, 0x10FFFF,
                    [](long long value) { if (Node* current = selectedNode()) current->itemIcon = static_cast<unsigned int>(value); });
                componentY += 96.0f;
                propertyTextInput(ui, "node.navbarFooterText", "Footer", componentY, node->footerText,
                    [uid = node->uid](const std::string& value) {
                        if (Node* current = selectedNode(); current && current->uid == uid) current->footerText = value;
                    });
                componentY += 76.0f;
                propertyStepper(ui, "node.navbarFooterIcon", "Footer icon", componentY + 8.0f,
                    node->footerIcon, 0, 0x10FFFF,
                    [](long long value) { if (Node* current = selectedNode()) current->footerIcon = static_cast<unsigned int>(value); });
                componentY += 48.0f;
            }
            if (node->type == NodeType::Input) {
                propertyTextInput(ui, "node.inputFontFamily", "Font family", componentY, node->fontFamily,
                    [uid = node->uid](const std::string& value) {
                        if (Node* current = selectedNode(); current && current->uid == uid) current->fontFamily = value;
                    });
                componentY += 76.0f;
                propertyTextInput(ui, "node.inputValue", "Value", componentY, node->valueText,
                    [uid = node->uid](const std::string& value) {
                        if (Node* current = selectedNode(); current && current->uid == uid) current->valueText = value;
                    });
                componentY += 76.0f;
                propertyNumber(ui, "node.inputInset", "Inset", componentY + 8.0f, node->inset, 0.0f, 256.0f,
                    [](float value) { if (Node* current = selectedNode()) current->inset = value; });
                componentY += 48.0f;
            }
            if (node->type == NodeType::Checkbox || node->type == NodeType::Radio) {
                propertyNumber(ui, "node.controlSize", node->type == NodeType::Checkbox ? "Box size" : "Dot size",
                    componentY + 8.0f, node->controlSize, 10.0f, 256.0f,
                    [](float value) { if (Node* current = selectedNode()) current->controlSize = value; });
                componentY += 48.0f;
            }
            if (node->type == NodeType::Switch) {
                propertyNumber(ui, "node.trackWidth", "Track width", componentY + 8.0f, node->trackWidth, 1.0f, 512.0f,
                    [](float value) { if (Node* current = selectedNode()) current->trackWidth = value; });
                propertyNumber(ui, "node.trackHeight", "Track height", componentY + 56.0f, node->trackHeight, 1.0f, 512.0f,
                    [](float value) { if (Node* current = selectedNode()) current->trackHeight = value; });
                componentY += 96.0f;
            }
            if (node->type == NodeType::Dropdown) {
                propertyNumber(ui, "node.itemHeight", "Item height", componentY + 8.0f, node->itemHeight, 1.0f, 512.0f,
                    [](float value) { if (Node* current = selectedNode()) current->itemHeight = value; });
                componentY += 48.0f;
            }
            if (node->type == NodeType::Dialog) {
                propertyTextInput(ui, "node.primaryText", "Primary action", componentY, node->primaryText,
                    [uid = node->uid](const std::string& value) {
                        if (Node* current = selectedNode(); current && current->uid == uid) current->primaryText = value;
                    });
                componentY += 76.0f;
                propertyTextInput(ui, "node.secondaryText", "Secondary action", componentY, node->secondaryText,
                    [uid = node->uid](const std::string& value) {
                        if (Node* current = selectedNode(); current && current->uid == uid) current->secondaryText = value;
                    });
                componentY += 76.0f;
            }
            if (node->type == NodeType::Sidebar) {
                propertyNumber(ui, "node.drawerWidth", "Drawer width", componentY + 8.0f, node->drawerWidth, 280.0f, 1920.0f,
                    [](float value) { if (Node* current = selectedNode()) current->drawerWidth = value; });
                componentY += 48.0f;
                propertyTextInput(ui, "node.eyebrowText", "Eyebrow", componentY, node->eyebrowText,
                    [uid = node->uid](const std::string& value) {
                        if (Node* current = selectedNode(); current && current->uid == uid) current->eyebrowText = value;
                    });
                componentY += 76.0f;
            }
            if (node->type == NodeType::TiltCard) {
                propertyNumber(ui, "node.maxTilt", "Max tilt", componentY + 8.0f, node->maxTilt, 0.0f, 3.141593f,
                    [](float value) { if (Node* current = selectedNode()) current->maxTilt = value; });
                componentY += 48.0f;
            }
            if (node->type == NodeType::NeumorphicButton) {
                propertyNumber(ui, "node.neoPressScale", "Press scale", componentY + 8.0f, node->pressScale, 0.80f, 1.0f,
                    [](float value) { if (Node* current = selectedNode()) current->pressScale = value; });
                componentY += 48.0f;
            }
            if (node->type == NodeType::MouseArea) {
                smallLabel(ui, "node.mouseCursor.label", "Cursor", kPanelPadding, componentY + 8.0f, 72.0f);
                ui.stack("node.mouseCursor.wrap").position(92.0f, componentY).size(kRightWidth - 110.0f, 36.0f).content([&] {
                    components::segmented(ui, "node.mouseCursor").size(kRightWidth - 110.0f, 36.0f).theme(designerTheme())
                        .items({"Arrow", "Hand"}).selected(node->cursorShape)
                        .onChange([](int value) { if (Node* current = selectedNode()) current->cursorShape = value; }).build();
                }).build();
                componentY += 48.0f;
                propertyToggle(ui, "node.mousePreserveFocus", "Preserve focus on press", componentY,
                    node->preserveFocusOnPress,
                    [](bool value) { if (Node* current = selectedNode()) current->preserveFocusOnPress = value; });
                componentY += 44.0f;
                propertyToggle(ui, "node.mouseSuppressClick", "Suppress click after drag", componentY,
                    node->suppressClickAfterDrag,
                    [](bool value) { if (Node* current = selectedNode()) current->suppressClickAfterDrag = value; });
                componentY += 44.0f;
                propertyNumber(ui, "node.mouseScrollStep", "Scroll step", componentY + 8.0f, node->scrollStep, 0.0f, 4096.0f,
                    [](float value) { if (Node* current = selectedNode()) current->scrollStep = value; });
                propertyNumber(ui, "node.mouseMaxScroll", "Max scroll", componentY + 56.0f, node->maxScrollStep, 0.0f, 4096.0f,
                    [](float value) { if (Node* current = selectedNode()) current->maxScrollStep = value; });
                propertyNumber(ui, "node.dragThreshold", "Drag threshold", componentY + 104.0f, node->dragThreshold, 0.0f, 512.0f,
                    [](float value) { if (Node* current = selectedNode()) current->dragThreshold = value; });
                componentY += 144.0f;
            }
            if (node->type == NodeType::Markdown) {
                propertyNumber(ui, "node.markdownMargin", "Margin", componentY + 8.0f, node->margin, 0.0f, 512.0f,
                    [](float value) { if (Node* current = selectedNode()) current->margin = value; });
                componentY += 48.0f;
            }
            if (node->type == NodeType::Card || node->type == NodeType::Markdown) {
                propertyToggle(ui, "node.wrapContentHeight", "Wrap content height", componentY,
                    node->wrapContentHeight,
                    [](bool value) { if (Node* current = selectedNode()) current->wrapContentHeight = value; });
                componentY += 44.0f;
            }
            if (node->type == NodeType::Toast) {
                propertyStepper(ui, "node.toastIcon", "Icon", componentY + 8.0f, node->icon, 0, 0x10FFFF,
                    [](long long value) { if (Node* current = selectedNode()) current->icon = static_cast<unsigned int>(value); });
                propertyNumber(ui, "node.toastDuration", "Duration", componentY + 56.0f, node->toastDuration, 0.0f, 3600.0f,
                    [](float value) { if (Node* current = selectedNode()) current->toastDuration = value; });
                componentY += 96.0f;
            }
            if (node->type == NodeType::Rect || node->type == NodeType::Panel || node->type == NodeType::Polygon) {
                propertyNumber(ui, "node.borderWidth", "Border", componentY + 8.0f, node->borderWidth, 0.0f, 64.0f,
                    [](float value) { if (Node* current = selectedNode()) current->borderWidth = value; });
                componentY += 48.0f;
                propertyColorSwatch(ui, "node.borderColor", "Border color", componentY, node->borderColor, 2);
                componentY += 48.0f;
                propertyToggle(ui, "node.gradientEnabled", "Gradient", componentY, node->gradientEnabled,
                    [](bool value) { if (Node* current = selectedNode()) current->gradientEnabled = value; });
                componentY += 44.0f;
                if (node->gradientEnabled) {
                    propertyColorSwatch(ui, "node.gradientColor", "Gradient end", componentY, node->gradientColor, 1);
                    componentY += 48.0f;
                    smallLabel(ui, "node.gradientDirection.label", "Direction", kPanelPadding, componentY + 8.0f, 80.0f);
                    ui.stack("node.gradientDirection.wrap").position(92.0f, componentY).size(kRightWidth - 110.0f, 36.0f).content([&] {
                        components::segmented(ui, "node.gradientDirection").size(kRightWidth - 110.0f, 36.0f).theme(designerTheme())
                            .items({"Vertical", "Horizontal"}).selected(node->gradientHorizontal ? 1 : 0)
                            .onChange([](int value) { if (Node* current = selectedNode()) current->gradientHorizontal = value == 1; }).build();
                    }).build();
                    componentY += 48.0f;
                }
                propertyToggle(ui, "node.shadowEnabled", "Shadow", componentY, node->shadowEnabled,
                    [](bool value) { if (Node* current = selectedNode()) current->shadowEnabled = value; });
                componentY += 44.0f;
                if (node->shadowEnabled) {
                    propertyToggle(ui, "node.insetShadow", "Inset shadow", componentY, node->insetShadow,
                        [](bool value) { if (Node* current = selectedNode()) current->insetShadow = value; });
                    componentY += 44.0f;
                    propertyColorSwatch(ui, "node.shadowColor", "Shadow color", componentY, node->shadowColor, 3);
                    componentY += 48.0f;
                    propertyNumber(ui, "node.shadowBlur", "Blur", componentY + 8.0f, node->shadowBlur, 0.0f, 256.0f,
                        [](float value) { if (Node* current = selectedNode()) current->shadowBlur = value; });
                    propertyNumber(ui, "node.shadowX", "Offset X", componentY + 56.0f, node->shadowOffsetX, -512.0f, 512.0f,
                        [](float value) { if (Node* current = selectedNode()) current->shadowOffsetX = value; });
                    propertyNumber(ui, "node.shadowY", "Offset Y", componentY + 104.0f, node->shadowOffsetY, -512.0f, 512.0f,
                        [](float value) { if (Node* current = selectedNode()) current->shadowOffsetY = value; });
                    componentY += 144.0f;
                }
                propertyNumber(ui, "node.blur", "Blur", componentY + 8.0f, node->blur, 0.0f, 256.0f,
                    [](float value) { if (Node* current = selectedNode()) current->blur = value; });
                propertyNumber(ui, "node.scaleX", "Scale X", componentY + 56.0f, node->scaleX, 0.01f, 10.0f,
                    [](float value) { if (Node* current = selectedNode()) current->scaleX = value; });
                propertyNumber(ui, "node.scaleY", "Scale Y", componentY + 104.0f, node->scaleY, 0.01f, 10.0f,
                    [](float value) { if (Node* current = selectedNode()) current->scaleY = value; });
                componentY += 144.0f;
            }
            if (node->type == NodeType::Card) {
                propertyNumber(ui, "node.cardPadding", "Padding", componentY + 8.0f, node->padding, 0.0f, 512.0f,
                    [](float value) { if (Node* current = selectedNode()) current->padding = value; });
                componentY += 48.0f;
                propertyNumber(ui, "node.cardBorder", "Border", componentY + 8.0f, node->borderWidth, 0.0f, 64.0f,
                    [](float value) { if (Node* current = selectedNode()) current->borderWidth = value; });
                componentY += 48.0f;
                propertyColorSwatch(ui, "node.cardBorderColor", "Border color", componentY, node->borderColor, 2);
                componentY += 48.0f;
                propertyToggle(ui, "node.cardShadow", "Shadow", componentY, node->shadowEnabled,
                    [](bool value) { if (Node* current = selectedNode()) current->shadowEnabled = value; });
                componentY += 44.0f;
                if (node->shadowEnabled) {
                    propertyToggle(ui, "node.cardInsetShadow", "Inset shadow", componentY, node->insetShadow,
                        [](bool value) { if (Node* current = selectedNode()) current->insetShadow = value; });
                    componentY += 44.0f;
                    propertyColorSwatch(ui, "node.cardShadowColor", "Shadow color", componentY, node->shadowColor, 3);
                    componentY += 48.0f;
                    propertyNumber(ui, "node.cardShadowBlur", "Shadow blur", componentY + 8.0f, node->shadowBlur, 0.0f, 256.0f,
                        [](float value) { if (Node* current = selectedNode()) current->shadowBlur = value; });
                    propertyNumber(ui, "node.cardShadowX", "Offset X", componentY + 56.0f, node->shadowOffsetX, -512.0f, 512.0f,
                        [](float value) { if (Node* current = selectedNode()) current->shadowOffsetX = value; });
                    propertyNumber(ui, "node.cardShadowY", "Offset Y", componentY + 104.0f, node->shadowOffsetY, -512.0f, 512.0f,
                        [](float value) { if (Node* current = selectedNode()) current->shadowOffsetY = value; });
                    componentY += 144.0f;
                }
            }
            if (node->type == NodeType::Polygon) {
                propertyStepper(ui, "node.polygonSides", "Sides", componentY + 8.0f, node->polygonSides, 3, 32,
                    [](long long value) { if (Node* current = selectedNode()) current->polygonSides = static_cast<int>(value); });
                componentY += 48.0f;
            }
            if (node->type == NodeType::Rect || node->type == NodeType::Panel || node->type == NodeType::Polygon ||
                node->type == NodeType::Image || node->type == NodeType::Svg || node->type == NodeType::Button ||
                node->type == NodeType::Card || node->type == NodeType::MouseArea || node->type == NodeType::Text) {
                propertyNumber(ui, "node.opacity", "Opacity", componentY + 8.0f, node->opacity, 0.0f, 1.0f,
                    [](float value) { if (Node* current = selectedNode()) current->opacity = value; });
                componentY += 48.0f;
            }
            if (node->type == NodeType::Rect || node->type == NodeType::Panel || node->type == NodeType::Polygon ||
                node->type == NodeType::Image || node->type == NodeType::Svg || node->type == NodeType::Text) {
                propertyNumber(ui, "node.rotation", "Rotation", componentY + 8.0f, node->rotation, -3600.0f, 3600.0f,
                    [](float value) { if (Node* current = selectedNode()) current->rotation = value; });
                componentY += 48.0f;
            }
            if (node->type == NodeType::Text || node->type == NodeType::Image || node->type == NodeType::Svg) {
                propertyNumber(ui, "node.mediaScaleX", "Scale X", componentY + 8.0f, node->scaleX, 0.01f, 10.0f,
                    [](float value) { if (Node* current = selectedNode()) current->scaleX = value; });
                propertyNumber(ui, "node.mediaScaleY", "Scale Y", componentY + 56.0f, node->scaleY, 0.01f, 10.0f,
                    [](float value) { if (Node* current = selectedNode()) current->scaleY = value; });
                componentY += 96.0f;
            }
            if (hasTextApi(node->type)) {
                smallLabel(ui, "node.text.label", node->type == NodeType::Image || node->type == NodeType::Svg ? "Source" : "Text",
                    kPanelPadding, componentY, 150.0f);
                components::input(ui, "node.text.input").position(kPanelPadding, componentY + 22.0f)
                    .size(kRightWidth - kPanelPadding * 2.0f, node->type == NodeType::Markdown ? 88.0f : 40.0f)
                    .multiline(node->type == NodeType::Markdown).theme(designerTheme()).value(node->text)
                    .onChange([uid = node->uid](const std::string& value) {
                        if (Node* current = selectedNode(); current && current->uid == uid) current->text = value;
                    }).build();
                componentY += node->type == NodeType::Markdown ? 124.0f : 76.0f;
            }
            if (hasFontSizeApi(node->type)) {
                propertyNumber(ui, "node.font", "Font", componentY + 8.0f, node->fontSize, 1.0f, 256.0f,
                    [](float value) { if (Node* current = selectedNode()) current->fontSize = value; });
                componentY += 48.0f;
            }
            if (hasValueApi(node->type)) {
                propertyNumber(ui, "node.value", "Value", componentY + 8.0f, node->value, 0.0f, 1.0f,
                    [](float value) { if (Node* current = selectedNode()) current->value = value; });
                componentY += 48.0f;
            }
            if (hasCheckedApi(node->type)) {
                ui.stack("node.checked.wrap").position(kPanelPadding, componentY)
                    .size(kRightWidth - kPanelPadding * 2.0f, 36.0f).content([&] {
                        components::checkbox(ui, "node.checked").size(kRightWidth - kPanelPadding * 2.0f, 36.0f)
                            .theme(designerTheme()).text("Checked").checked(node->checked).onChange([](bool value) {
                                if (Node* current = selectedNode()) current->checked = value;
                            }).build();
                    }).build();
                componentY += 48.0f;
            }
            if (node->type == NodeType::Button || node->type == NodeType::HeartSwitch ||
                node->type == NodeType::NeumorphicButton || node->type == NodeType::MouseArea) {
                propertyToggle(ui, "node.disabled", "Disabled", componentY, node->disabled,
                    [](bool value) { if (Node* current = selectedNode()) current->disabled = value; });
                componentY += 44.0f;
            }
            if (node->type == NodeType::Input || node->type == NodeType::Navbar || node->type == NodeType::CardSlider) {
                const bool enabled = node->type == NodeType::Input ? node->multiline :
                                     node->type == NodeType::Navbar ? node->compact : node->autoPlay;
                const std::string label = node->type == NodeType::Input ? "Multiline" :
                                          node->type == NodeType::Navbar ? "Compact" : "Auto play";
                ui.stack("node.option.wrap").position(kPanelPadding, componentY)
                    .size(kRightWidth - kPanelPadding * 2.0f, 36.0f).content([&, enabled, label] {
                        components::checkbox(ui, "node.option").size(kRightWidth - kPanelPadding * 2.0f, 36.0f)
                            .theme(designerTheme()).text(label).checked(enabled).onChange([](bool value) {
                                if (Node* current = selectedNode()) {
                                    if (current->type == NodeType::Input) current->multiline = value;
                                    else if (current->type == NodeType::Navbar) current->compact = value;
                                    else if (current->type == NodeType::CardSlider) current->autoPlay = value;
                                }
                            }).build();
                    }).build();
                componentY += 48.0f;
            }
            if (hasSelectedApi(node->type)) {
                propertyStepper(ui, "node.selected", "Selected", componentY + 8.0f, node->selectedIndex, 0, 64,
                    [](long long value) { if (Node* current = selectedNode()) current->selectedIndex = static_cast<int>(value); });
                componentY += 48.0f;
            }
            if (node->type == NodeType::Stepper) {
                propertyStepper(ui, "node.minimum", "Minimum", componentY + 8.0f, node->minimum, -1000000, 1000000,
                    [](long long value) { if (Node* current = selectedNode()) current->minimum = value; });
                propertyStepper(ui, "node.maximum", "Maximum", componentY + 56.0f, node->maximum, -1000000, 1000000,
                    [](long long value) { if (Node* current = selectedNode()) current->maximum = value; });
                propertyStepper(ui, "node.step", "Step", componentY + 104.0f, node->step, 1, 1000000,
                    [](long long value) { if (Node* current = selectedNode()) current->step = value; });
                componentY += 144.0f;
                propertyStepper(ui, "node.numberBase", "Base", componentY + 8.0f, node->numberBase, 2, 36,
                    [](long long value) { if (Node* current = selectedNode()) current->numberBase = static_cast<int>(value); });
                propertyStepper(ui, "node.digits", "Digits", componentY + 56.0f, node->digits, 0, 128,
                    [](long long value) { if (Node* current = selectedNode()) current->digits = static_cast<int>(value); });
                propertyStepper(ui, "node.bitWidth", "Bit width", componentY + 104.0f, node->bitWidth, 0, 128,
                    [](long long value) { if (Node* current = selectedNode()) current->bitWidth = static_cast<int>(value); });
                componentY += 144.0f;
                propertyTextInput(ui, "node.prefixText", "Prefix", componentY, node->prefixText,
                    [uid = node->uid](const std::string& value) {
                        if (Node* current = selectedNode(); current && current->uid == uid) current->prefixText = value;
                    });
                componentY += 76.0f;
                propertyToggle(ui, "node.showBasePrefix", "Show base prefix", componentY, node->showBasePrefix,
                    [](bool value) { if (Node* current = selectedNode()) current->showBasePrefix = value; });
                componentY += 44.0f;
                propertyToggle(ui, "node.uppercase", "Uppercase", componentY, node->uppercase,
                    [](bool value) { if (Node* current = selectedNode()) current->uppercase = value; });
                componentY += 44.0f;
            }
            if (node->type == NodeType::DatePicker) {
                propertyStepper(ui, "node.year", "Year", componentY + 8.0f, node->year, 1, 9999,
                    [](long long value) { if (Node* current = selectedNode()) current->year = static_cast<int>(value); });
                propertyStepper(ui, "node.month", "Month", componentY + 56.0f, node->month, 1, 12,
                    [](long long value) { if (Node* current = selectedNode()) current->month = static_cast<int>(value); });
                propertyStepper(ui, "node.day", "Day", componentY + 104.0f, node->day, 1, 31,
                    [](long long value) { if (Node* current = selectedNode()) current->day = static_cast<int>(value); });
                componentY += 144.0f;
            }
            if (node->type == NodeType::TimePicker) {
                propertyStepper(ui, "node.hour", "Hour", componentY + 8.0f, node->hour, 0, 23,
                    [](long long value) { if (Node* current = selectedNode()) current->hour = static_cast<int>(value); });
                propertyStepper(ui, "node.minute", "Minute", componentY + 56.0f, node->minute, 0, 59,
                    [](long long value) { if (Node* current = selectedNode()) current->minute = static_cast<int>(value); });
                propertyStepper(ui, "node.minuteStep", "Step", componentY + 104.0f, node->minuteStep, 1, 30,
                    [](long long value) { if (Node* current = selectedNode()) current->minuteStep = static_cast<int>(value); });
                componentY += 144.0f;
            }
            if (node->type == NodeType::Text) {
                propertyTextInput(ui, "node.fontFamily", "Font family", componentY, node->fontFamily,
                    [uid = node->uid](const std::string& value) {
                        if (Node* current = selectedNode(); current && current->uid == uid) current->fontFamily = value;
                    });
                componentY += 76.0f;
                propertyStepper(ui, "node.fontWeight", "Weight", componentY + 8.0f, node->fontWeight, 1, 1000,
                    [](long long value) { if (Node* current = selectedNode()) current->fontWeight = static_cast<int>(value); });
                propertyNumber(ui, "node.lineHeight", "Line height", componentY + 56.0f, node->lineHeight, 0.0f, 1024.0f,
                    [](float value) { if (Node* current = selectedNode()) current->lineHeight = value; });
                componentY += 96.0f;
                propertyToggle(ui, "node.wrapText", "Wrap text", componentY, node->wrapText,
                    [](bool value) { if (Node* current = selectedNode()) current->wrapText = value; });
                componentY += 44.0f;
                smallLabel(ui, "node.align.h.label", "Horizontal", kPanelPadding, componentY + 8.0f, 80.0f);
                ui.stack("node.align.h.wrap").position(92.0f, componentY).size(kRightWidth - 110.0f, 36.0f).content([&] {
                    components::segmented(ui, "node.align.h").size(kRightWidth - 110.0f, 36.0f)
                        .theme(designerTheme()).items({"Left", "Center", "Right"}).selected(node->horizontalAlign)
                        .onChange([](int value) { if (Node* current = selectedNode()) current->horizontalAlign = value; }).build();
                }).build();
                componentY += 48.0f;
                smallLabel(ui, "node.align.v.label", "Vertical", kPanelPadding, componentY + 8.0f, 80.0f);
                ui.stack("node.align.v.wrap").position(92.0f, componentY).size(kRightWidth - 110.0f, 36.0f).content([&] {
                    components::segmented(ui, "node.align.v").size(kRightWidth - 110.0f, 36.0f)
                        .theme(designerTheme()).items({"Top", "Center", "Bottom"}).selected(node->verticalAlign)
                        .onChange([](int value) { if (Node* current = selectedNode()) current->verticalAlign = value; }).build();
                }).build();
                componentY += 48.0f;
            }
            const ComponentStyleFields styleFields = componentStyleFields(*node);
            if (!styleFields.colors.empty() || !styleFields.floats.empty() || !styleFields.strings.empty() || styleFields.hasShadow) {
                sectionLabel(ui, "node.componentStyle.label", "COMPONENT STYLE", kPanelPadding, componentY + 4.0f, 180.0f);
                componentY += 38.0f;
                for (std::size_t index = 0; index < styleFields.colors.size(); ++index) {
                    const auto& field = styleFields.colors[index];
                    propertyStyleColorSwatch(ui, "node.styleColor." + std::to_string(index), field.label,
                        componentY, field.value, field.key);
                    componentY += 48.0f;
                }
                for (std::size_t index = 0; index < styleFields.floats.size(); ++index) {
                    const auto& field = styleFields.floats[index];
                    propertyNumber(ui, "node.styleFloat." + std::to_string(index), field.label, componentY + 8.0f,
                        field.value, 0.0f, 4096.0f, [key = field.key](float value) {
                            if (Node* current = selectedNode()) setStyleFloat(*current, key, value);
                        });
                    componentY += 48.0f;
                }
                for (std::size_t index = 0; index < styleFields.strings.size(); ++index) {
                    const auto& field = styleFields.strings[index];
                    propertyTextInput(ui, "node.styleString." + std::to_string(index), field.label, componentY,
                        field.value, [key = field.key](const std::string& value) {
                            if (Node* current = selectedNode()) setStyleString(*current, key, value);
                        });
                    componentY += 76.0f;
                }
                if (styleFields.hasShadow) {
                    propertyToggle(ui, "node.styleShadowOverride", "Override component shadow", componentY,
                        node->componentShadowOverride,
                        [](bool value) { if (Node* current = selectedNode()) current->componentShadowOverride = value; });
                    componentY += 44.0f;
                    if (node->componentShadowOverride) {
                        propertyToggle(ui, "node.styleShadowEnabled", "Shadow enabled", componentY, node->shadowEnabled,
                            [](bool value) { if (Node* current = selectedNode()) current->shadowEnabled = value; });
                        componentY += 44.0f;
                        propertyColorSwatch(ui, "node.styleShadowColor", "Shadow color", componentY, node->shadowColor, 3);
                        componentY += 48.0f;
                        propertyNumber(ui, "node.styleShadowBlur", "Shadow blur", componentY + 8.0f, node->shadowBlur, 0.0f, 256.0f,
                            [](float value) { if (Node* current = selectedNode()) current->shadowBlur = value; });
                        propertyNumber(ui, "node.styleShadowX", "Shadow X", componentY + 56.0f, node->shadowOffsetX, -512.0f, 512.0f,
                            [](float value) { if (Node* current = selectedNode()) current->shadowOffsetX = value; });
                        propertyNumber(ui, "node.styleShadowY", "Shadow Y", componentY + 104.0f, node->shadowOffsetY, -512.0f, 512.0f,
                            [](float value) { if (Node* current = selectedNode()) current->shadowOffsetY = value; });
                        componentY += 144.0f;
                    }
                }
            }
            if (node->shadowEnabled || node->componentShadowOverride) {
                propertyNumber(ui, "node.shadowSpread", "Shadow spread", componentY + 8.0f, node->shadowSpread, -512.0f, 512.0f,
                    [](float value) { if (Node* current = selectedNode()) current->shadowSpread = value; });
                componentY += 48.0f;
            }
            const float actionsY = std::max(650.0f, componentY + 12.0f);
            if (node->parentUid >= 0) {
                ui.text("node.parent").position(kPanelPadding, actionsY).size(kRightWidth - kPanelPadding * 2.0f, 24.0f)
                    .text("Inside: " + (findNode(node->parentUid) ? findNode(node->parentUid)->id : std::string("unknown")))
                    .fontSize(12.0f).lineHeight(18.0f).color(kMuted).build();
            }
            float buttonY = actionsY + 32.0f;
            if (isOverlayPreview(node->type) || node->type == NodeType::Dropdown || node->type == NodeType::Tooltip) {
                components::button(ui, "node.preview").position(kPanelPadding, buttonY).size(kRightWidth - kPanelPadding * 2.0f, 40.0f)
                    .theme(designerTheme()).text("Open component preview").icon(0xF06E)
                    .onClick([uid = node->uid] { state.previewOverlayUid = uid; }).build();
                buttonY += 48.0f;
            }
            components::button(ui, "node.page").position(kPanelPadding, buttonY).size(kRightWidth - kPanelPadding * 2.0f, 38.0f)
                .theme(designerTheme(), false).text("Deselect element").icon(0xF00D)
                .onClick([] { state.selectedUid = -1; state.previewOverlayUid = -1; }).build();
            codeSectionY = buttonY + 58.0f;
        }

        sectionLabel(ui, "code.label", "CODE EXPORT", kPanelPadding, codeSectionY, 180.0f);
        ui.text("code.note").position(kPanelPadding, codeSectionY + 28.0f).size(kRightWidth - kPanelPadding * 2.0f, 52.0f)
            .text("Code is generated only when Copy or Export is pressed, keeping canvas drag updates responsive.")
            .fontSize(12.0f).lineHeight(18.0f).wrap(true).color(kMuted).build();
        }).build();
            }).build();
    }).build();
}

void composeColorPickers(eui::Ui& ui) {
    components::colorPicker(ui, "designer.background.picker")
        .screen(state.screenWidth, state.screenHeight).size(340.0f, 500.0f)
        .theme(designerTheme()).colors(colorPresets()).value(state.backgroundColor).open(state.backgroundPickerOpen)
        .onChange([](eui::Color color) { state.backgroundColor = color; })
        .onOpenChange([](bool open) { state.backgroundPickerOpen = open; }).zIndex(500).build();

    eui::Color elementColor = kAccent;
    if (const Node* node = selectedNode()) {
        if (state.elementColorTarget == 1000) {
            if (const eui::Color* value = findStyleColor(*node, state.elementStyleColorKey)) elementColor = *value;
            else {
                const ComponentStyleFields fields = componentStyleFields(*node);
                const auto found = std::find_if(fields.colors.begin(), fields.colors.end(), [&](const auto& field) {
                    return field.key == state.elementStyleColorKey;
                });
                if (found != fields.colors.end()) elementColor = found->value;
            }
        } else if (state.elementColorTarget >= 100 &&
            static_cast<std::size_t>(state.elementColorTarget - 100) < node->palette.size()) {
            elementColor = node->palette[static_cast<std::size_t>(state.elementColorTarget - 100)];
        } else if (state.elementColorTarget == 1) elementColor = node->gradientColor;
        else if (state.elementColorTarget == 2) elementColor = node->borderColor;
        else if (state.elementColorTarget == 3) elementColor = node->shadowColor;
        else if (state.elementColorTarget == 4) elementColor = node->foregroundColor;
        else elementColor = node->color;
    }
    components::colorPicker(ui, "designer.element.picker")
        .screen(state.screenWidth, state.screenHeight).size(340.0f, 500.0f)
        .theme(designerTheme()).colors(colorPresets()).value(elementColor).open(state.elementPickerOpen && selectedNode() != nullptr)
        .onChange([](eui::Color color) {
            if (Node* node = selectedNode()) {
                if (state.elementColorTarget == 1000) setStyleColor(*node, state.elementStyleColorKey, color);
                else if (state.elementColorTarget >= 100 &&
                    static_cast<std::size_t>(state.elementColorTarget - 100) < node->palette.size()) {
                    node->palette[static_cast<std::size_t>(state.elementColorTarget - 100)] = color;
                } else if (state.elementColorTarget == 1) node->gradientColor = color;
                else if (state.elementColorTarget == 2) node->borderColor = color;
                else if (state.elementColorTarget == 3) node->shadowColor = color;
                else if (state.elementColorTarget == 4) node->foregroundColor = color;
                else node->color = color;
            }
        })
        .onOpenChange([](bool open) { state.elementPickerOpen = open; }).zIndex(510).build();
}

void composeSelectedComponentPreview(eui::Ui& ui, float canvasWidth, float canvasHeight, float scale) {
    const Node* node = selectedNode();
    if (!node || node->uid != state.previewOverlayUid) return;
    const auto theme = canvasTheme(node->color);
    const float overlayWidth = std::max(1.0f, canvasWidth);
    const float overlayHeight = std::max(1.0f, canvasHeight);
    switch (node->type) {
    case NodeType::DatePicker: {
        auto style = applyStyleShadow(*node, applyStyleFloats(*node, applyStyleColors(*node, components::DatePickerStyle(theme), {
            {"backdrop", &components::DatePickerStyle::backdrop}, {"surface", &components::DatePickerStyle::surface},
            {"column", &components::DatePickerStyle::column}, {"selected", &components::DatePickerStyle::selected},
            {"text", &components::DatePickerStyle::text}, {"mutedText", &components::DatePickerStyle::mutedText},
            {"accent", &components::DatePickerStyle::accent}, {"border", &components::DatePickerStyle::border}
        }), {{"styleRadius", &components::DatePickerStyle::radius}}), &components::DatePickerStyle::shadow);
        components::datePicker(ui, "preview.datePicker").screen(overlayWidth, overlayHeight)
            .size(420.0f * scale, 270.0f * scale).theme(theme).style(style).date(node->year, node->month, node->day).open(true)
            .onChange([](int year, int month, int day) { if (Node* current = selectedNode()) { current->year = year; current->month = month; current->day = day; } })
            .onOpenChange([](bool open) { if (!open) state.previewOverlayUid = -1; }).build();
        break; }
    case NodeType::TimePicker: {
        auto style = applyStyleShadow(*node, applyStyleFloats(*node, applyStyleColors(*node, components::TimePickerStyle(theme), {
            {"backdrop", &components::TimePickerStyle::backdrop}, {"surface", &components::TimePickerStyle::surface},
            {"column", &components::TimePickerStyle::column}, {"selected", &components::TimePickerStyle::selected},
            {"text", &components::TimePickerStyle::text}, {"mutedText", &components::TimePickerStyle::mutedText},
            {"accent", &components::TimePickerStyle::accent}, {"border", &components::TimePickerStyle::border}
        }), {{"styleRadius", &components::TimePickerStyle::radius}}), &components::TimePickerStyle::shadow);
        components::timePicker(ui, "preview.timePicker").screen(overlayWidth, overlayHeight)
            .size(330.0f * scale, 264.0f * scale).theme(theme).style(style).time(node->hour, node->minute).minuteStep(node->minuteStep).open(true)
            .onChange([](int hour, int minute) { if (Node* current = selectedNode()) { current->hour = hour; current->minute = minute; } })
            .onOpenChange([](bool open) { if (!open) state.previewOverlayUid = -1; }).build();
        break; }
    case NodeType::ColorPicker: {
        auto style = applyStyleShadow(*node, applyStyleFloats(*node, applyStyleColors(*node, components::ColorPickerStyle(theme), {
            {"backdrop", &components::ColorPickerStyle::backdrop}, {"surface", &components::ColorPickerStyle::surface},
            {"track", &components::ColorPickerStyle::track}, {"text", &components::ColorPickerStyle::text},
            {"mutedText", &components::ColorPickerStyle::mutedText}, {"accent", &components::ColorPickerStyle::accent},
            {"border", &components::ColorPickerStyle::border}, {"knob", &components::ColorPickerStyle::knob}
        }), {{"styleRadius", &components::ColorPickerStyle::radius}}), &components::ColorPickerStyle::shadow);
        components::colorPicker(ui, "preview.colorPicker").screen(overlayWidth, overlayHeight)
            .size(420.0f * scale, 320.0f * scale).theme(theme).style(style).colors(node->palette).value(node->color).open(true)
            .onChange([uid = node->uid](eui::Color value) { if (Node* current = findNode(uid)) current->color = value; })
            .onOpenChange([](bool open) { if (!open) state.previewOverlayUid = -1; }).build();
        break; }
    case NodeType::Dialog: {
        auto style = applyStyleShadow(*node, applyStyleFloats(*node, applyStyleColors(*node, components::DialogStyle(theme), {
            {"backdrop", &components::DialogStyle::backdrop}, {"surface", &components::DialogStyle::surface}, {"border", &components::DialogStyle::border},
            {"title", &components::DialogStyle::title}, {"message", &components::DialogStyle::message}, {"primary", &components::DialogStyle::primary},
            {"secondary", &components::DialogStyle::secondary}, {"primaryHover", &components::DialogStyle::primaryHover},
            {"primaryPressed", &components::DialogStyle::primaryPressed}, {"secondaryHover", &components::DialogStyle::secondaryHover},
            {"secondaryPressed", &components::DialogStyle::secondaryPressed}
        }), {{"styleRadius", &components::DialogStyle::radius}}), &components::DialogStyle::shadow);
        components::dialog(ui, "preview.dialog").screen(overlayWidth, overlayHeight)
            .size(430.0f * scale, 228.0f * scale).theme(theme).style(style).title(node->text).message(node->messageText)
            .primaryText(node->primaryText).secondaryText(node->secondaryText).open(true)
            .onPrimary([] { state.previewOverlayUid = -1; }).onSecondary([] { state.previewOverlayUid = -1; })
            .onOpenChange([](bool open) { if (!open) state.previewOverlayUid = -1; }).build();
        break; }
    case NodeType::Toast:
        {
        auto style = applyStyleShadow(*node, applyStyleFloats(*node, applyStyleColors(*node, components::ToastStyle(theme), {
            {"background", &components::ToastStyle::background}, {"border", &components::ToastStyle::border},
            {"text", &components::ToastStyle::text}, {"mutedText", &components::ToastStyle::mutedText}, {"accent", &components::ToastStyle::accent}
        }), {{"styleRadius", &components::ToastStyle::radius}}), &components::ToastStyle::shadow);
        auto toast = components::toast(ui, "preview.toast").screen(overlayWidth, overlayHeight)
            .size(420.0f * scale, 110.0f * scale).theme(theme).title(node->text).message(node->messageText)
            .duration(node->toastDuration).visible(true).style(style).onDismiss([] { state.previewOverlayUid = -1; });
        if (node->icon != 0) toast.icon(node->icon);
        toast.build();
        }
        break;
    case NodeType::ContextMenu:
        {
        const eui::Vec2 absolute = absolutePosition(*node);
        auto style = applyStyleShadow(*node, applyStyleFloats(*node, applyStyleColors(*node, components::ContextMenuStyle(theme), {
            {"background", &components::ContextMenuStyle::background}, {"hover", &components::ContextMenuStyle::hover},
            {"pressed", &components::ContextMenuStyle::pressed}, {"text", &components::ContextMenuStyle::text},
            {"mutedText", &components::ContextMenuStyle::mutedText}, {"border", &components::ContextMenuStyle::border}
        }), {{"styleRadius", &components::ContextMenuStyle::radius}}), &components::ContextMenuStyle::shadow);
        components::contextMenu(ui, "preview.contextMenu").screen(overlayWidth, overlayHeight)
            .position(absolute.x * scale, (absolute.y + node->height) * scale)
            .size(node->width * scale, node->height * scale).theme(theme)
            .items(splitList(node->itemsText)).style(style).open(true)
            .onSelect([](int) { state.previewOverlayUid = -1; })
            .onOpenChange([](bool open) { if (!open) state.previewOverlayUid = -1; }).build();
        break;
        }
    case NodeType::Sidebar:
        components::sidebar(ui, "preview.sidebar").size(overlayWidth, overlayHeight)
            .drawerWidth(node->drawerWidth * scale).theme(theme).title(node->text).eyebrow(node->eyebrowText).open(true)
            .onOpenChange([](bool open) { if (!open) state.previewOverlayUid = -1; })
            .content([message = node->messageText](eui::Ui& contentUi, float contentWidth, float contentHeight) {
                contentUi.text("preview.sidebar.body").size(contentWidth, contentHeight).text(message)
                    .fontSize(18.0f).color({0.08f, 0.10f, 0.14f, 1.0f}).build();
            }).build();
        break;
    case NodeType::Tooltip:
        {
        const eui::Vec2 absolute = absolutePosition(*node);
        auto style = applyStyleShadow(*node, applyStyleFloats(*node, applyStyleColors(*node, components::TooltipStyle(theme), {
            {"background", &components::TooltipStyle::background}, {"text", &components::TooltipStyle::text}, {"border", &components::TooltipStyle::border}
        }), {{"styleRadius", &components::TooltipStyle::radius}}), &components::TooltipStyle::shadow);
        components::tooltip(ui, "preview.tooltip")
            .value(node->text).anchor((absolute.x + node->width * 0.5f) * scale, absolute.y * scale)
            .bounds(overlayWidth, overlayHeight).theme(theme).style(style).zIndex(320).build();
        break;
        }
    default:
        break;
    }
}

void composeElementContextMenu(eui::Ui& ui) {
    const Node* target = findNode(state.elementMenuUid);
    const bool locked = target && target->locked;
    components::contextMenu(ui, "designer.element.context")
        .screen(state.screenWidth, state.screenHeight).position(state.elementMenuX, state.elementMenuY)
        .size(220.0f, 42.0f).theme(designerTheme()).zIndex(2400).open(state.elementMenuOpen && target != nullptr)
        .items({"Duplicate", locked ? "Unlock movement" : "Lock movement", "Delete"})
        .onSelect([](int index) {
            Node* current = findNode(state.elementMenuUid);
            if (!current) return;
            state.selectedUid = current->uid;
            if (index == 0) duplicateSelected();
            else if (index == 1) {
                current = selectedNode();
                current->locked = !current->locked;
                state.status = current->locked ? "Element locked" : "Element unlocked";
            } else if (index == 2) removeSelected();
            state.elementMenuOpen = false;
        })
        .onOpenChange([](bool open) { state.elementMenuOpen = open; }).build();
}

} // namespace

void compose(eui::Ui& ui, const eui::Screen& screen) {
    state.screenWidth = screen.width;
    state.screenHeight = screen.height;
    if (state.layoutDirty) {
        applyLayoutPositions();
        state.layoutDirty = false;
    }
    const float bodyHeight = std::max(0.0f, screen.height - kTopBarHeight);
    const float centerWidth = std::max(320.0f, screen.width - kLeftWidth - kRightWidth);
    composeTopBar(ui, screen.width);
    composePalette(ui, bodyHeight);
    composeCanvas(ui, centerWidth, bodyHeight);
    ui.stack("properties.host").position(screen.width - kRightWidth, 0.0f).size(kRightWidth, screen.height)
        .content([&] { composeProperties(ui, bodyHeight); }).build();
    composeColorPickers(ui);
    composeElementContextMenu(ui);
}

} // namespace app
