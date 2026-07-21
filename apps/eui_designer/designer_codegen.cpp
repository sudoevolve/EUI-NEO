#include "designer_codegen.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <sstream>

namespace app::designer {
namespace {

std::string escape(const std::string& value) {
    std::string result;
    for (unsigned char ch : value) {
        switch (ch) {
        case '\\': result += "\\\\"; break;
        case '"': result += "\\\""; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default:
            if (ch < 0x20) {
                std::ostringstream escaped;
                escaped << "\\x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(ch);
                result += escaped.str();
            } else {
                result.push_back(static_cast<char>(ch));
            }
            break;
        }
    }
    return result;
}

std::string number(float value) {
    if (!std::isfinite(value) || std::abs(value) < 0.0005f) value = 0.0f;
    std::ostringstream output;
    output << std::fixed << std::setprecision(3) << value;
    std::string text = output.str();
    while (text.size() > 2 && text.back() == '0' && text[text.size() - 2] != '.') text.pop_back();
    return text + "f";
}

std::string color(const eui::Color& value) {
    return "{" + number(value.r) + ", " + number(value.g) + ", " +
           number(value.b) + ", " + number(value.a) + "}";
}

std::string colorList(const std::vector<eui::Color>& values) {
    std::ostringstream output;
    output << "std::vector<eui::Color>{";
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index > 0) output << ", ";
        output << color(values[index]);
    }
    output << "}";
    return output.str();
}

eui::Color styleColor(const Node& node, const char* name, eui::Color fallback) {
    const auto found = std::find_if(node.styleColors.begin(), node.styleColors.end(), [name](const auto& entry) {
        return entry.name == name;
    });
    return found == node.styleColors.end() ? fallback : found->value;
}

std::string componentStyleCall(const Node& node, const char* styleType, bool supportsShadow = false) {
    const bool hasShadow = supportsShadow && node.componentShadowOverride;
    const bool hasButtonShadow = node.type == NodeType::Button;
    const bool hasCardSliderButtonShadow = node.type == NodeType::CardSlider && node.componentShadowOverride;
    if (node.styleColors.empty() && node.styleFloats.empty() && node.styleStrings.empty() && !hasShadow && !hasButtonShadow && !hasCardSliderButtonShadow) return {};
    std::ostringstream output;
    const std::string themeExpression = "componentTheme(" + color(node.color) + ")";
    const bool styleFactory = std::string(styleType) == "components::workshop::HeartSwitchStyle" ||
                              std::string(styleType) == "components::workshop::NeumorphicButtonStyle" ||
                              std::string(styleType) == "components::workshop::TiltCardStyle";
    const std::string initializer = !styleFactory ? std::string(styleType) + "(" + themeExpression + ")" :
        std::string(styleType == std::string("components::workshop::HeartSwitchStyle") ? "components::workshop::heartSwitchStyle" :
                    styleType == std::string("components::workshop::NeumorphicButtonStyle") ? "components::workshop::neumorphicButtonStyle" :
                    "components::workshop::tiltCardStyle") + "(" + themeExpression + ")";
    output << ".style([&] {\n        auto style = " << initializer << ";\n";
    for (const auto& entry : node.styleColors) output << "        style." << entry.name << " = " << color(entry.value) << ";\n";
    for (const auto& entry : node.styleFloats) {
        const char* member = (entry.name == "styleRadius" || entry.name == "scrollRadius") ? "radius" : entry.name.c_str();
        output << "        style." << member << " = " << number(entry.value) << ";\n";
    }
    for (const auto& entry : node.styleStrings) output << "        style." << entry.name << " = \"" << escape(entry.value) << "\";\n";
    if (hasShadow || hasButtonShadow) {
        output << "        style.shadow = {" << (node.shadowEnabled ? "true" : "false") << ", {"
               << number(node.shadowOffsetX) << ", " << number(node.shadowOffsetY) << "}, "
               << number(node.shadowBlur) << ", " << number(node.shadowSpread) << ", " << color(node.shadowColor) << ", "
               << (node.insetShadow ? "true" : "false") << "};\n";
    }
    if (hasCardSliderButtonShadow) {
        output << "        style.button.shadow = {" << (node.shadowEnabled ? "true" : "false") << ", {"
               << number(node.shadowOffsetX) << ", " << number(node.shadowOffsetY) << "}, "
               << number(node.shadowBlur) << ", " << number(node.shadowSpread) << ", " << color(node.shadowColor) << ", "
               << (node.insetShadow ? "true" : "false") << "};\n";
    }
    output << "        return style;\n    }())";
    return output.str();
}

const char* componentStyleType(NodeType type) {
    switch (type) {
    case NodeType::Button: return "components::ButtonStyle";
    case NodeType::Scroll: case NodeType::ScrollView: case NodeType::VirtualList: return "components::ScrollStyle";
    case NodeType::Input: return "components::InputStyle";
    case NodeType::Checkbox: return "components::CheckboxStyle";
    case NodeType::Radio: return "components::RadioStyle";
    case NodeType::Switch: return "components::SwitchStyle";
    case NodeType::Slider: return "components::SliderStyle";
    case NodeType::Progress: return "components::ProgressStyle";
    case NodeType::Segmented: return "components::SegmentedStyle";
    case NodeType::Stepper: return "components::StepperStyle";
    case NodeType::Tabs: return "components::TabsStyle";
    case NodeType::Dropdown: return "components::DropdownStyle";
    case NodeType::DataTable: return "components::DataTableStyle";
    case NodeType::LineChart: return "components::LineChartStyle";
    case NodeType::BarChart: return "components::BarChartStyle";
    case NodeType::PieChart: return "components::PieChartStyle";
    case NodeType::Carousel: return "components::CarouselStyle";
    case NodeType::Markdown: return "components::MarkdownStyle";
    case NodeType::DatePicker: return "components::DatePickerStyle";
    case NodeType::TimePicker: return "components::TimePickerStyle";
    case NodeType::ColorPicker: return "components::ColorPickerStyle";
    case NodeType::Dialog: return "components::DialogStyle";
    case NodeType::Toast: return "components::ToastStyle";
    case NodeType::Tooltip: return "components::TooltipStyle";
    case NodeType::ContextMenu: return "components::ContextMenuStyle";
    case NodeType::Image: return "components::ImageStyle";
    case NodeType::HeartSwitch: return "components::workshop::HeartSwitchStyle";
    case NodeType::NeumorphicButton: return "components::workshop::NeumorphicButtonStyle";
    case NodeType::TiltCard: return "components::workshop::TiltCardStyle";
    case NodeType::CardSlider: return "components::workshop::CardSliderStyle";
    default: return nullptr;
    }
}

bool componentStyleSupportsShadow(NodeType type) {
    return type == NodeType::Input || type == NodeType::Stepper || type == NodeType::Dropdown ||
           type == NodeType::LineChart || type == NodeType::BarChart || type == NodeType::PieChart ||
           type == NodeType::Carousel || type == NodeType::DatePicker || type == NodeType::TimePicker ||
           type == NodeType::ColorPicker || type == NodeType::Dialog || type == NodeType::Toast ||
           type == NodeType::Tooltip || type == NodeType::ContextMenu;
}

std::string componentStyleCall(const Node& node) {
    const char* type = componentStyleType(node.type);
    return type == nullptr ? std::string{} : componentStyleCall(node, type, componentStyleSupportsShadow(node.type));
}

void generateShapeStyle(std::ostringstream& code, const Node& node, const std::string& pad, bool radius) {
    code << pad << ".color(" << color(node.color) << ")\n";
    if (radius) code << pad << ".radius(" << number(node.radius) << ")\n";
    code << pad << ".border(" << number(node.borderWidth) << ", " << color(node.borderColor) << ")\n";
    if (node.gradientEnabled) {
        code << pad << ".gradient(" << color(node.color) << ", " << color(node.gradientColor) << ", "
             << (node.gradientHorizontal ? "core::GradientDirection::Horizontal" : "core::GradientDirection::Vertical")
             << ")\n";
    }
    if (node.shadowEnabled) {
        code << pad << ".shadow({true, {" << number(node.shadowOffsetX) << ", " << number(node.shadowOffsetY)
             << "}, " << number(node.shadowBlur) << ", " << number(node.shadowSpread) << ", "
             << color(node.shadowColor) << ", " << (node.insetShadow ? "true" : "false") << "})\n";
    }
    code << pad << ".blur(" << number(node.blur) << ")\n"
         << pad << ".opacity(" << number(node.opacity) << ")\n"
         << pad << ".scale(" << number(node.scaleX) << ", " << number(node.scaleY) << ")\n"
         << pad << ".rotate(" << number(node.rotation * 0.0174532925f) << ")";
}

std::string indentation(int level) {
    return std::string(static_cast<std::size_t>(level) * 4, ' ');
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
        char* end = nullptr;
        const float parsed = std::strtof(item.c_str(), &end);
        if (end != item.c_str() && end != nullptr && *end == '\0' && std::isfinite(parsed)) values.push_back(parsed);
    }
    return values;
}

std::string stringList(const std::string& value) {
    std::ostringstream output;
    output << "std::vector<std::string>{";
    const std::vector<std::string> items = splitList(value);
    for (std::size_t index = 0; index < items.size(); ++index) {
        if (index > 0) output << ", ";
        output << "\"" << escape(items[index]) << "\"";
    }
    output << "}";
    return output.str();
}

std::string floatList(const std::string& value) {
    std::ostringstream output;
    output << "std::vector<float>{";
    const std::vector<float> values = splitValues(value);
    for (std::size_t index = 0; index < values.size(); ++index) {
        if (index > 0) output << ", ";
        output << number(values[index]);
    }
    output << "}";
    return output.str();
}

std::string carouselItems(const Node& node, bool cardSlider) {
    std::ostringstream output;
    output << (cardSlider ? "std::vector<components::workshop::CardSliderItem>{"
                         : "std::vector<components::CarouselItem>{");
    const std::vector<std::string> items = splitList(node.itemsText);
    for (std::size_t index = 0; index < items.size(); ++index) {
        if (index > 0) output << ", ";
        output << "{\"\", \"" << escape(items[index]) << "\", \""
               << (cardSlider ? "Card slider" : "Carousel item") << "\"";
        if (cardSlider) output << ", \"Preview\"";
        output << "}";
    }
    output << "}";
    return output.str();
}

std::string navbarItems(const Node& node) {
    std::ostringstream output;
    output << "std::vector<components::NavbarItem>{";
    const std::vector<std::string> items = splitList(node.itemsText);
    for (std::size_t index = 0; index < items.size(); ++index) {
        if (index > 0) output << ", ";
        output << "{\"item_" << index << "\", \"" << escape(items[index]) << "\", "
               << node.itemIcon << ", " << index << "}";
    }
    output << "}";
    return output.str();
}

std::string tableRows(const Node& node) {
    std::ostringstream output;
    output << "std::vector<std::vector<std::string>>{";
    const std::size_t columns = std::max<std::size_t>(1, splitList(node.labelsText).size());
    const std::vector<std::string> cells = splitList(node.itemsText);
    for (std::size_t offset = 0; offset < cells.size(); offset += columns) {
        if (offset > 0) output << ", ";
        output << "{";
        for (std::size_t column = 0; column < columns; ++column) {
            if (column > 0) output << ", ";
            const std::string cell = offset + column < cells.size() ? cells[offset + column] : std::string{};
            output << "\"" << escape(cell) << "\"";
        }
        output << "}";
    }
    output << "}";
    return output.str();
}

const Node* findNode(const std::vector<Node>& nodes, int uid) {
    const auto found = std::find_if(nodes.begin(), nodes.end(), [uid](const Node& node) { return node.uid == uid; });
    return found == nodes.end() ? nullptr : &*found;
}

eui::Vec2 absolutePosition(const std::vector<Node>& nodes, const Node& node) {
    eui::Vec2 result{node.x, node.y};
    int parentUid = node.parentUid;
    int guard = 0;
    while (parentUid >= 0 && guard++ < 32) {
        const Node* parent = findNode(nodes, parentUid);
        if (parent == nullptr) break;
        result.x += parent->x;
        result.y += parent->y;
        parentUid = parent->parentUid;
    }
    return result;
}

bool isLayout(NodeType type) {
    return type == NodeType::Row || type == NodeType::Column || type == NodeType::Stack;
}

const char* layoutFactory(NodeType type) {
    return type == NodeType::Row ? "row" : type == NodeType::Column ? "column" : "stack";
}

const char* horizontalAlign(int value) {
    return value == 0 ? "eui::HorizontalAlign::Left" :
           value == 2 ? "eui::HorizontalAlign::Right" : "eui::HorizontalAlign::Center";
}

const char* verticalAlign(int value) {
    return value == 0 ? "eui::VerticalAlign::Top" :
           value == 2 ? "eui::VerticalAlign::Bottom" : "eui::VerticalAlign::Center";
}

const char* layoutAlign(int value) {
    return value == 1 ? "core::Align::CENTER" : value == 2 ? "core::Align::END" : "core::Align::START";
}

const char* typeName(NodeType type) {
    switch (type) {
    case NodeType::Scroll: return "Scroll";
    case NodeType::ScrollView: return "ScrollView";
    case NodeType::VirtualList: return "VirtualList";
    case NodeType::Markdown: return "Markdown";
    case NodeType::DatePicker: return "DatePicker";
    case NodeType::TimePicker: return "TimePicker";
    case NodeType::ColorPicker: return "ColorPicker";
    case NodeType::DataTable: return "DataTable";
    case NodeType::Dialog: return "Dialog";
    case NodeType::Sidebar: return "Sidebar";
    case NodeType::Navbar: return "Navbar";
    case NodeType::Toast: return "Toast";
    case NodeType::Tooltip: return "Tooltip";
    case NodeType::ContextMenu: return "ContextMenu";
    case NodeType::Carousel: return "Carousel";
    case NodeType::LineChart: return "LineChart";
    case NodeType::BarChart: return "BarChart";
    case NodeType::PieChart: return "PieChart";
    case NodeType::MouseArea: return "MouseArea";
    case NodeType::HeartSwitch: return "HeartSwitch";
    case NodeType::NeumorphicButton: return "NeumorphicButton";
    case NodeType::CardSlider: return "CardSlider";
    case NodeType::TiltCard: return "TiltCard";
    default: return "Component";
    }
}

bool isStatefulOverlay(NodeType type) {
    return type == NodeType::DatePicker || type == NodeType::TimePicker || type == NodeType::ColorPicker ||
           type == NodeType::Dialog || type == NodeType::Sidebar || type == NodeType::Toast ||
           type == NodeType::ContextMenu;
}

bool isOverlay(NodeType type) {
    return type == NodeType::Tooltip || isStatefulOverlay(type);
}

std::string overlayStateName(const Node& node) {
    return "element_open_" + std::to_string(node.uid);
}

std::string nodePosition(const Node& node) {
    if (node.parentUid >= 0 && node.participatesInLayout) return {};
    return ".position(" + number(node.x) + ", " + number(node.y) + ")";
}

std::string nodeSize(const Node& node) {
    return ".size(" + number(node.width) + ", " + number(node.height) + ")";
}

void generateNode(std::ostringstream& code, const std::vector<Node>& nodes, const Node& node, int level) {
    const std::string pad = indentation(level);
    const std::string childPad = indentation(level + 1);
    const std::string id = escape(node.id.empty() ? "element_" + std::to_string(node.uid) : node.id);
    const std::string pos = nodePosition(node);
    const std::string size = nodeSize(node);
    const std::string nodeTheme = "componentTheme(" + color(node.color) + ")";

    if (node.parentUid >= 0 && !node.participatesInLayout) {
        code << pad << "ui.stack(\"" << id << ".absolute\").position(" << number(node.x) << ", " << number(node.y)
             << ").size(" << number(node.width) << ", " << number(node.height) << ").ignoreLayout().content([&] {\n";
        Node inner = node;
        inner.participatesInLayout = true;
        generateNode(code, nodes, inner, level + 1);
        code << pad << "}).build();\n";
        return;
    }

    if (isLayout(node.type)) {
        code << pad << "ui." << layoutFactory(node.type) << "(\"" << id << "\")" << pos << size
             << ".gap(" << number(node.gap) << ").padding(" << number(node.padding) << ")\n"
             << childPad << ".justifyContent(" << layoutAlign(node.mainAlign) << ")\n"
             << childPad << ".alignItems(" << layoutAlign(node.crossAlign) << ")\n"
             << childPad << ".content([&] {\n";
        for (const Node& child : nodes) {
            if (child.parentUid == node.uid) generateNode(code, nodes, child, level + 2);
        }
        code << childPad << "}).build();\n";
        return;
    }

    if (node.type == NodeType::Text) {
        code << pad << "ui.text(\"" << id << "\")" << pos << size << "\n"
             << childPad << ".text(\"" << escape(node.text) << "\")\n"
             << childPad << ".fontFamily(\"" << escape(node.fontFamily) << "\").fontSize(" << number(node.fontSize) << ")\n"
             << childPad << ".fontWeight(" << node.fontWeight << ").lineHeight(" << number(node.lineHeight) << ")\n"
             << childPad << ".wrap(" << (node.wrapText ? "true" : "false") << ").opacity(" << number(node.opacity) << ")\n"
             << childPad << ".scale(" << number(node.scaleX) << ", " << number(node.scaleY) << ")\n"
             << childPad << ".rotate(" << number(node.rotation * 0.0174532925f) << ")\n"
             << childPad << ".color({" << number(node.color.r) << ", " << number(node.color.g) << ", "
             << number(node.color.b) << ", " << number(node.color.a) << "})\n"
             << childPad << ".horizontalAlign(" << horizontalAlign(node.horizontalAlign) << ")\n"
             << childPad << ".verticalAlign(" << verticalAlign(node.verticalAlign) << ").build();\n";
    } else if (node.type == NodeType::Button) {
        const eui::Color buttonText = styleColor(node, "text", node.foregroundColor);
        const eui::Color buttonIcon = styleColor(node, "icon", buttonText);
        code << pad << "components::button(ui, \"" << id << "\")" << pos << size << ".theme(" << nodeTheme << ")"
             << componentStyleCall(node) << "\n"
             << childPad << ".text(\"" << escape(node.text) << "\").fontSize(" << number(node.fontSize) << ")\n"
             << childPad << ".iconSize(" << number(node.iconSize) << ").textColor(" << color(buttonText)
             << ").iconColor(" << color(buttonIcon) << ")\n";
        if (node.icon != 0) code << childPad << ".icon(" << node.icon << ")\n";
        code << childPad << ".radius(" << number(node.radius) << ").opacity(" << number(node.opacity)
             << ").disabled(" << (node.disabled ? "true" : "false")
             << ").preserveFocusOnPress(" << (node.preserveFocusOnPress ? "true" : "false")
             << ").pressScale(" << number(node.pressScale) << ")\n"
             << childPad << ".border(" << number(node.borderWidth) << ", " << color(node.borderColor) << ")\n";
        if (node.buttonStyle == 1) {
            code << childPad << ".colors({" << number(node.color.r) << ", " << number(node.color.g) << ", " << number(node.color.b) << ", 0.0f}, "
                 << "{" << number(node.color.r) << ", " << number(node.color.g) << ", " << number(node.color.b) << ", " << number(node.color.a * 0.10f) << "}, "
                 << "{" << number(node.color.r) << ", " << number(node.color.g) << ", " << number(node.color.b) << ", " << number(node.color.a * 0.18f) << "})\n"
                 << childPad << ".textColor(" << color(node.color) << ").iconColor(" << color(node.color) << ")\n"
                 << childPad << ".border(1.0f, {" << number(node.color.r) << ", " << number(node.color.g) << ", " << number(node.color.b) << ", " << number(node.color.a * 0.78f) << "})\n"
                 << childPad << ".shadow(0.0f, 0.0f, 0.0f, {0.0f, 0.0f, 0.0f, 0.0f})\n";
        } else if (node.buttonStyle == 2) {
            code << childPad << ".colors({" << number(node.color.r) << ", " << number(node.color.g) << ", " << number(node.color.b) << ", 0.0f}, "
                 << "{" << number(node.color.r) << ", " << number(node.color.g) << ", " << number(node.color.b) << ", " << number(node.color.a * 0.08f) << "}, "
                 << "{" << number(node.color.r) << ", " << number(node.color.g) << ", " << number(node.color.b) << ", " << number(node.color.a * 0.14f) << "})\n"
                 << childPad << ".textColor(" << color(node.color) << ").iconColor(" << color(node.color) << ")\n"
                 << childPad << ".border(0.0f, {0.0f, 0.0f, 0.0f, 0.0f})\n"
                 << childPad << ".shadow(0.0f, 0.0f, 0.0f, {0.0f, 0.0f, 0.0f, 0.0f})\n";
        }
        code << childPad << ".build();\n";
    } else if (node.type == NodeType::Input) {
        code << pad << "components::input(ui, \"" << id << "\")" << pos << size << ".theme(" << nodeTheme << ")\n"
             << childPad << ".value(\"" << escape(node.valueText) << "\").placeholder(\"" << escape(node.text)
             << "\").fontFamily(\"" << escape(node.fontFamily) << "\").fontSize(" << number(node.fontSize)
             << ").inset(" << number(node.inset) << ").multiline(" << (node.multiline ? "true" : "false") << ")"
             << componentStyleCall(node) << ".build();\n";
    } else if (node.type == NodeType::Checkbox || node.type == NodeType::Radio || node.type == NodeType::Switch) {
        const char* factory = node.type == NodeType::Checkbox ? "checkbox" : node.type == NodeType::Radio ? "radio" : "toggleSwitch";
        const char* stateMethod = node.type == NodeType::Radio ? "selected" : "checked";
        code << pad << "ui.stack(\"" << id << ".host\")" << pos << size << ".content([&] {\n"
             << childPad << "components::" << factory << "(ui, \"" << id << "\")" << size
             << ".theme(" << nodeTheme << ").text(\"" << escape(node.text) << "\")." << stateMethod << "("
             << (node.checked ? "true" : "false") << ").fontSize(" << number(node.fontSize) << ")";
        if (node.type == NodeType::Checkbox) code << ".boxSize(" << number(node.controlSize) << ")";
        else if (node.type == NodeType::Radio) code << ".dotSize(" << number(node.controlSize) << ")";
        else code << ".trackSize(" << number(node.trackWidth) << ", " << number(node.trackHeight) << ")";
        code << componentStyleCall(node) << ".build();\n"
             << pad << "}).build();\n";
    } else if (node.type == NodeType::Slider || node.type == NodeType::Progress) {
        code << pad << "ui.stack(\"" << id << ".host\")" << pos << size << ".content([&] {\n"
             << childPad << "components::" << (node.type == NodeType::Slider ? "slider" : "progress") << "(ui, \"" << id << "\")"
             << size << ".theme(" << nodeTheme << ").value(" << number(node.value) << ")"
             << componentStyleCall(node) << ".build();\n" << pad << "}).build();\n";
    } else if (node.type == NodeType::Segmented || node.type == NodeType::Tabs || node.type == NodeType::Dropdown) {
        const char* factory = node.type == NodeType::Segmented ? "segmented" : node.type == NodeType::Tabs ? "tabs" : "dropdown";
        code << pad << "ui.stack(\"" << id << ".host\")" << pos << size << ".content([&] {\n"
             << childPad;
        if (node.type == NodeType::Dropdown) {
            code << "static eui::Signal<bool> element_dropdown_open_" << node.uid << "{false};\n" << childPad;
        }
        code << "components::" << factory << "(ui, \"" << id << "\")" << size
             << ".theme(" << nodeTheme << ").items(" << stringList(node.itemsText) << ").selected(" << node.selectedIndex << ")";
        if (node.type == NodeType::Segmented) code << ".fontSize(" << number(node.fontSize) << ")";
        if (node.type == NodeType::Tabs) code << ".fontSize(" << number(node.fontSize) << ")";
        if (node.type == NodeType::Dropdown) {
            code << ".placeholder(\"" << escape(node.messageText) << "\").itemHeight(" << number(node.itemHeight)
                 << ").bindOpen(element_dropdown_open_" << node.uid << ").onChange([](int) {})";
        }
        code << componentStyleCall(node) << ".build();\n" << pad << "}).build();\n";
    } else if (node.type == NodeType::Stepper) {
        code << pad << "ui.stack(\"" << id << ".host\")" << pos << size << ".content([&] {\n"
             << childPad << "components::stepper(ui, \"" << id << "\")" << size
             << ".theme(" << nodeTheme << ").value(" << node.selectedIndex << ").min(" << node.minimum << ").max("
             << node.maximum << ").step(" << node.step << ").base(" << node.numberBase << ").digits(" << node.digits
             << ").bitWidth(" << node.bitWidth << ").showBasePrefix(" << (node.showBasePrefix ? "true" : "false")
             << ").prefix(\"" << escape(node.prefixText) << "\").uppercase(" << (node.uppercase ? "true" : "false")
             << ").fontSize(" << number(node.fontSize) << ")" << componentStyleCall(node)
             << ".build();\n" << pad << "}).build();\n";
    } else if (node.type == NodeType::Image || node.type == NodeType::Svg) {
        code << pad << "ui." << (node.type == NodeType::Svg ? "svg" : "image") << "(\"" << id << "\")" << pos << size
             << ".source(\"" << escape(node.text) << "\")\n"
             << childPad << ".radius(" << number(node.radius) << ").opacity(" << number(node.opacity)
             << ").scale(" << number(node.scaleX) << ", " << number(node.scaleY) << ")"
             << ".rotate(" << number(node.rotation * 0.0174532925f) << ")";
        if (node.type == NodeType::Image) code << ".tint(" << color(styleColor(node, "tint", {1.0f, 1.0f, 1.0f, 1.0f})) << ")";
        code << ".build();\n";
    } else if (node.type == NodeType::Rect || node.type == NodeType::Panel) {
        code << pad << "ui.rect(\"" << id << "\")" << pos << size << "\n";
        generateShapeStyle(code, node, childPad, true);
        code << ".build();\n";
    } else if (node.type == NodeType::Polygon) {
        code << pad << "ui.polygon(\"" << id << "\")" << pos << size << ".points({";
        const int sides = std::clamp(node.polygonSides, 3, 32);
        for (int index = 0; index < sides; ++index) {
            if (index > 0) code << ", ";
            const float angle = -1.5707963268f + 6.2831853072f * static_cast<float>(index) / static_cast<float>(sides);
            code << "{" << number(node.width * (0.5f + 0.5f * std::cos(angle))) << ", "
                 << number(node.height * (0.5f + 0.5f * std::sin(angle))) << "}";
        }
        code << "})\n";
        generateShapeStyle(code, node, childPad, false);
        code << ".build();\n";
    } else if (node.type == NodeType::Card) {
        code << pad << "ui.stack(\"" << id << ".host\")" << pos << size << ".content([&] {\n"
             << childPad << "components::card(ui, \"" << id << "\").width(" << number(node.width) << ")"
             << (node.wrapContentHeight ? ".wrapContentHeight()" : ".height(" + number(node.height) + ")")
             << ".theme(" << nodeTheme << ")";
        if (node.gradientEnabled) {
            code << ".style([&] {\n"
                 << indentation(level + 3) << "auto style = components::CardStyle(" << nodeTheme << ");\n"
                 << indentation(level + 3) << "style.gradient = {true, " << color(node.color) << ", " << color(node.gradientColor)
                 << ", " << (node.gradientHorizontal ? "core::GradientDirection::Horizontal" : "core::GradientDirection::Vertical") << "};\n"
                 << indentation(level + 3) << "return style;\n" << indentation(level + 2) << "}())";
        }
        code << ".radius("
             << number(node.radius) << ").padding(" << number(node.padding) << ").color(" << color(node.color) << ")\n"
             << indentation(level + 2) << ".border(" << number(node.borderWidth) << ", " << color(node.borderColor)
             << ").opacity(" << number(node.opacity) << ")";
        if (node.shadowEnabled) {
            code << ".shadow({true, {" << number(node.shadowOffsetX) << ", " << number(node.shadowOffsetY) << "}, "
                 << number(node.shadowBlur) << ", " << number(node.shadowSpread) << ", " << color(node.shadowColor) << ", "
                 << (node.insetShadow ? "true" : "false") << "})";
        } else code << ".shadow({})";
        code << ".content([&] {}).build();\n"
             << pad << "}).build();\n";
    } else if (node.type == NodeType::Scroll || node.type == NodeType::ScrollView) {
        code << pad << "components::scrollView(ui, \"" << id << "\")" << pos << size << ".theme(" << nodeTheme << ")\n"
             << childPad << ".gap(0.0f).step(" << number(node.scrollStep)
             << ").scrollbarWidth(" << number(node.scrollbarWidth) << ").scrollbarGap(" << number(node.scrollbarGap) << ")"
             << componentStyleCall(node) << "\n"
             << childPad << ".content([&](eui::Ui& ui, float contentWidth, float viewportHeight) {\n"
             << indentation(level + 2) << "ui.column(\"" << id << ".content\").width(contentWidth).height(core::SizeValue::wrapContent())\n"
             << indentation(level + 3) << ".minHeight(viewportHeight).padding(" << number(node.padding) << ").gap(" << number(node.gap) << ")\n"
             << indentation(level + 3) << ".justifyContent(" << layoutAlign(node.mainAlign) << ")\n"
             << indentation(level + 3) << ".alignItems(" << layoutAlign(node.crossAlign) << ")\n"
             << indentation(level + 3) << ".content([&] {\n";
        for (const Node& child : nodes) {
            if (child.parentUid == node.uid) generateNode(code, nodes, child, level + 4);
        }
        code << indentation(level + 3) << "}).build();\n"
             << childPad << "}).build();\n";
    } else if (node.type == NodeType::VirtualList) {
        code << pad << "components::virtualList(ui, \"" << id << "\")" << pos << size << ".theme(" << nodeTheme << ")\n"
             << childPad << ".itemCount(" << node.itemCount << ").rowHeight(" << number(node.rowHeight)
             << ").step(" << number(node.scrollStep) << ").scrollbarWidth(" << number(node.scrollbarWidth)
             << ").scrollbarGap(" << number(node.scrollbarGap) << ").overscanViewports(" << number(node.overscanViewports) << ")"
             << componentStyleCall(node) << "\n"
             << childPad << ".row([](eui::Ui& rowUi, const std::string& rowId, std::int64_t index, float width, float height) {\n"
             << indentation(level + 2) << "rowUi.text(rowId + \".text\").size(width, height).text(\"Item \" + std::to_string(index))\n"
             << indentation(level + 3) << ".fontSize(13.0f).lineHeight(18.0f).color({0.1f, 0.12f, 0.16f, 1.0f})\n"
             << indentation(level + 3) << ".verticalAlign(eui::VerticalAlign::Center).build();\n"
             << childPad << "}).build();\n";
    } else if (node.type == NodeType::Markdown) {
        code << pad << "components::markdown(ui, \"" << id << "\")" << pos << ".width(" << number(node.width) << ")"
             << (node.wrapContentHeight ? ".wrapContentHeight()" : ".height(" + number(node.height) + ")")
             << ".theme(" << nodeTheme << ")\n"
             << childPad << ".markdown(\"" << escape(node.text) << "\").margin(" << number(node.margin) << ")"
             << componentStyleCall(node) << ".build();\n";
    } else if (node.type == NodeType::DataTable) {
        code << pad << "ui.stack(\"" << id << ".host\")" << pos << size << ".content([&] {\n"
             << childPad << "components::dataTable(ui, \"" << id << "\")" << size << ".theme(" << nodeTheme << ")\n"
             << indentation(level + 2) << ".columns(" << stringList(node.labelsText) << ")\n"
             << indentation(level + 2) << ".rows(" << tableRows(node) << ")" << componentStyleCall(node) << ".build();\n"
             << pad << "}).build();\n";
    } else if (node.type == NodeType::LineChart || node.type == NodeType::BarChart || node.type == NodeType::PieChart) {
        const char* factory = node.type == NodeType::LineChart ? "lineChart" : node.type == NodeType::BarChart ? "barChart" : "pieChart";
        code << pad << "ui.stack(\"" << id << ".host\")" << pos << size << ".content([&] {\n"
             << childPad << "components::" << factory << "(ui, \"" << id << "\")" << size << ".theme(" << nodeTheme << ")\n"
             << indentation(level + 2) << ".title(\"" << escape(node.text) << "\").values(" << floatList(node.valuesText) << ")\n"
             << indentation(level + 2) << ".labels(" << stringList(node.labelsText) << ")";
        if (node.type == NodeType::BarChart || node.type == NodeType::PieChart) {
            code << ".colors(" << colorList(node.palette) << ")";
        }
        code << componentStyleCall(node) << ".build();\n" << pad << "}).build();\n";
    } else if (node.type == NodeType::Carousel) {
        code << pad << "ui.stack(\"" << id << ".host\")" << pos << size << ".content([&] {\n"
             << childPad << "components::carousel(ui, \"" << id << "\")" << size << ".theme(" << nodeTheme << ")\n"
             << indentation(level + 2) << ".items(" << carouselItems(node, false) << ")\n"
             << indentation(level + 2) << ".index(" << number(static_cast<float>(node.selectedIndex)) << ")"
             << ".cardWidthRatio(" << number(node.cardWidthRatio) << ").overlap(" << number(node.overlap)
             << ").parallax(" << number(node.parallax) << ")" << componentStyleCall(node)
             << ".onChange([](float) {}).build();\n"
             << pad << "}).build();\n";
    } else if (node.type == NodeType::Navbar) {
        code << pad << "ui.stack(\"" << id << ".host\")" << pos << size << ".content([&] {\n"
             << childPad << "components::navbar(ui, \"" << id << "\")" << size << ".theme(" << nodeTheme << ").compact("
             << (node.compact ? "true" : "false") << ").brand(\"" << escape(node.text) << "\", " << node.brandIcon << ")\n"
             << indentation(level + 2) << ".subtitle(\"" << escape(node.messageText) << "\").selected(" << node.selectedIndex << ")\n"
             << indentation(level + 2) << ".items(" << navbarItems(node) << ")\n"
             << indentation(level + 2) << ".footer(\"" << escape(node.footerText) << "\", " << node.footerIcon
             << ", [] {}).onChange([](int) {}).build();\n" << pad << "}).build();\n";
    } else if (node.type == NodeType::HeartSwitch) {
        code << pad << "ui.stack(\"" << id << ".host\")" << pos << size << ".content([&] {\n"
             << childPad << "components::workshop::heartSwitch(ui, \"" << id << "\")" << size
             << ".theme(" << nodeTheme << ")" << componentStyleCall(node) << ".checked(" << (node.checked ? "true" : "false") << ").disabled("
             << (node.disabled ? "true" : "false") << ").build();\n" << pad << "}).build();\n";
    } else if (node.type == NodeType::NeumorphicButton) {
        code << pad << "ui.stack(\"" << id << ".host\")" << pos << size << ".content([&] {\n"
             << childPad << "components::workshop::neumorphicButton(ui, \"" << id << "\")" << size
             << ".theme(" << nodeTheme << ")" << componentStyleCall(node) << ".text(\"" << escape(node.text) << "\").fontSize(" << number(node.fontSize)
             << ").radius(" << number(node.radius) << ").pressScale(" << number(node.pressScale)
             << ").disabled(" << (node.disabled ? "true" : "false") << ").onClick([] {}).build();\n" << pad << "}).build();\n";
    } else if (node.type == NodeType::TiltCard) {
        code << pad << "ui.stack(\"" << id << ".host\")" << pos << size << ".content([&] {\n"
             << childPad << "components::workshop::tiltCard(ui, \"" << id << "\")" << size
             << ".theme(" << nodeTheme << ")" << componentStyleCall(node) << ".title(\"" << escape(node.text) << "\").subtitle(\"" << escape(node.messageText)
             << "\").maxTilt(" << number(node.maxTilt) << ").build();\n"
             << pad << "}).build();\n";
    } else if (node.type == NodeType::CardSlider) {
        code << pad << "ui.stack(\"" << id << ".host\")" << pos << size << ".content([&] {\n"
             << childPad << "components::workshop::cardSlider(ui, \"" << id << "\")" << size << ".theme(" << nodeTheme << ")"
             << componentStyleCall(node) << "\n"
             << indentation(level + 2) << ".items(" << carouselItems(node, true) << ")\n"
             << indentation(level + 2) << ".currentIndex(" << node.selectedIndex << ").duration(" << number(node.duration)
             << ").interval(" << number(node.interval) << ").cardSpacing(" << number(node.cardSpacing) << ")\n"
             << indentation(level + 2) << ".autoPlay(" << (node.autoPlay ? "true" : "false")
             << ").background(" << (node.backgroundEnabled ? "true" : "false")
             << ").tilt(" << (node.tiltEnabled ? "true" : "false")
             << ").onChange([](int) {}).build();\n" << pad << "}).build();\n";
    } else if (node.type == NodeType::MouseArea) {
        code << pad << "components::mouseArea(ui, \"" << id << "\")" << pos << size
             << ".color({" << number(node.color.r) << ", " << number(node.color.g) << ", " << number(node.color.b)
             << ", " << number(node.color.a * node.opacity * 0.18f) << "}).radius(" << number(node.radius)
             << ").cursor(" << (node.cursorShape == 0 ? "core::CursorShape::Arrow" : "core::CursorShape::Hand")
             << ").disabled(" << (node.disabled ? "true" : "false")
             << ").preserveFocusOnPress(" << (node.preserveFocusOnPress ? "true" : "false")
             << ").scrollStep(" << number(node.scrollStep) << ").maxScrollStep(" << number(node.maxScrollStep)
             << ").dragThreshold(" << number(node.dragThreshold)
             << ").suppressClickAfterDrag(" << (node.suppressClickAfterDrag ? "true" : "false")
             << ").onTap([] {}).build();\n";
    } else if (node.type == NodeType::Tooltip) {
        code << pad << "components::button(ui, \"" << id << ".source\")" << pos << size
             << ".theme(" << nodeTheme << ", false).text(\"Hover for tooltip\").build();\n";
    } else if (isStatefulOverlay(node.type)) {
        const std::string stateName = overlayStateName(node);
        code << pad << "components::button(ui, \"" << id << ".trigger\")" << pos << size << ".theme(" << nodeTheme << ", false)\n"
             << childPad << ".text(\"Open " << typeName(node.type) << "\").onClick([] { " << stateName << ".set(true); }).build();\n";
    } else {
        code << pad << "#error Unsupported EUI Designer node type\n";
    }
}

void generateOverlay(std::ostringstream& code, const std::vector<Node>& nodes, const Node& node, int level) {
    if (!isOverlay(node.type)) return;
    const std::string pad = indentation(level);
    const std::string childPad = indentation(level + 1);
    const std::string id = escape(node.id.empty() ? "element_" + std::to_string(node.uid) : node.id);
    const std::string nodeTheme = "componentTheme(" + color(node.color) + ")";
    const eui::Vec2 absolute = absolutePosition(nodes, node);

    if (node.type == NodeType::Tooltip) {
        code << pad << "components::tooltip(ui, \"" << id << "\").source(\"" << id << ".source\")\n"
             << childPad << ".value(\"" << escape(node.text) << "\").anchor("
             << number(absolute.x + node.width * 0.5f) << ", " << number(absolute.y) << ")\n"
             << childPad << ".bounds(screen.width, screen.height).theme(" << nodeTheme << ")"
             << componentStyleCall(node) << ".build();\n";
        return;
    }

    const std::string stateName = overlayStateName(node);
    if (node.type == NodeType::DatePicker) {
        code << pad << "components::datePicker(ui, \"" << id << "\").screen(screen.width, screen.height).size(420.0f, 270.0f)\n"
             << childPad << ".theme(" << nodeTheme << ")" << componentStyleCall(node)
             << ".date(" << node.year << ", " << node.month << ", " << node.day << ").bindOpen(" << stateName << ")\n"
             << childPad << ".onChange([](int, int, int) {}).build();\n";
    } else if (node.type == NodeType::TimePicker) {
        code << pad << "components::timePicker(ui, \"" << id << "\").screen(screen.width, screen.height).size(330.0f, 264.0f)\n"
             << childPad << ".theme(" << nodeTheme << ")" << componentStyleCall(node)
             << ".time(" << node.hour << ", " << node.minute << ").minuteStep(" << node.minuteStep << ").bindOpen(" << stateName << ")\n"
             << childPad << ".onChange([](int, int) {}).build();\n";
    } else if (node.type == NodeType::ColorPicker) {
        code << pad << "static eui::Color element_color_" << node.uid << "{" << number(node.color.r) << ", " << number(node.color.g)
             << ", " << number(node.color.b) << ", " << number(node.color.a) << "};\n"
             << pad << "components::colorPicker(ui, \"" << id << "\").screen(screen.width, screen.height).size(420.0f, 320.0f)\n"
             << childPad << ".theme(" << nodeTheme << ")" << componentStyleCall(node)
             << ".colors(" << colorList(node.palette) << ")\n"
             << childPad << ".value(element_color_" << node.uid << ").bindOpen(" << stateName << ")\n"
             << childPad << ".onChange([](eui::Color value) { element_color_" << node.uid << " = value; }).build();\n";
    } else if (node.type == NodeType::Dialog) {
        code << pad << "components::dialog(ui, \"" << id << "\").screen(screen.width, screen.height).size(430.0f, 228.0f)\n"
             << childPad << ".theme(" << nodeTheme << ")" << componentStyleCall(node)
             << ".title(\"" << escape(node.text) << "\").message(\"" << escape(node.messageText) << "\").bindOpen(" << stateName << ")\n"
             << childPad << ".primaryText(\"" << escape(node.primaryText) << "\").secondaryText(\""
             << escape(node.secondaryText) << "\").onPrimary([] { " << stateName << ".set(false); }).build();\n";
    } else if (node.type == NodeType::Sidebar) {
        code << pad << "components::sidebar(ui, \"" << id << "\").size(screen.width, screen.height).drawerWidth("
             << number(node.drawerWidth) << ").theme(" << nodeTheme << ").title(\""
             << escape(node.text) << "\").eyebrow(\"" << escape(node.eyebrowText) << "\").bindOpen(" << stateName << ")\n"
             << childPad << ".content([](eui::Ui& ui, float width, float height) { ui.text(\"" << id
             << ".body\").size(width, height).text(\"" << escape(node.messageText)
             << "\").fontSize(18.0f).color({0.08f, 0.1f, 0.14f, 1.0f}).build(); }).build();\n";
    } else if (node.type == NodeType::Toast) {
        code << pad << "components::toast(ui, \"" << id << "\").screen(screen.width, screen.height).size(420.0f, 110.0f)\n"
             << childPad << ".theme(" << nodeTheme << ")" << componentStyleCall(node)
             << ".title(\"" << escape(node.text) << "\").message(\"" << escape(node.messageText)
             << "\").duration(" << number(node.toastDuration) << ")";
        if (node.icon != 0) code << ".icon(" << node.icon << ")";
        code << ".bindVisible(" << stateName << ").build();\n";
    } else if (node.type == NodeType::ContextMenu) {
        code << pad << "components::contextMenu(ui, \"" << id << "\").screen(screen.width, screen.height).position("
             << number(absolute.x) << ", " << number(absolute.y + node.height) << ").size(" << number(node.width) << ", "
             << number(node.height) << ").theme(" << nodeTheme << ")" << componentStyleCall(node)
             << ".items(" << stringList(node.itemsText) << ")\n"
             << childPad << ".bindOpen(" << stateName << ").onSelect([](int) {}).build();\n";
    }
}

} // namespace

std::string generatedCode(const CodegenDocument& document) {
    std::ostringstream code;
    code << "#include \"eui_neo.h\"\n\nnamespace app {\n\n"
         << "const DslAppConfig& dslAppConfig() {\n"
         << "    static const DslAppConfig config = DslAppConfig{}.title(\"" << escape(document.title)
         << "\").pageId(\"" << escape(document.pageId) << "\").windowSize(" << document.width << ", " << document.height
         << ");\n    return config;\n}\n\n"
         << "void compose(eui::Ui& ui, const eui::Screen& screen) {\n"
         << "    const auto theme = components::theme::light();\n"
         << "    const auto componentTheme = [&](const eui::Color& primary) {\n"
         << "        auto value = theme;\n"
         << "        value.primary = primary;\n"
         << "        return value;\n"
         << "    };\n";
    for (const Node& node : document.nodes) {
        if (isStatefulOverlay(node.type)) {
            code << "    static eui::Signal<bool> " << overlayStateName(node) << "{false};\n";
        }
    }
    code << "    ui.rect(\"page.background\").size(screen.width, screen.height).color({"
         << number(document.background.r) << ", " << number(document.background.g) << ", "
         << number(document.background.b) << ", " << number(document.background.a) << "}).build();\n\n";
    for (const Node& node : document.nodes) {
        if (node.parentUid < 0 || !findNode(document.nodes, node.parentUid)) generateNode(code, document.nodes, node, 1);
    }
    code << "\n";
    for (const Node& node : document.nodes) generateOverlay(code, document.nodes, node, 1);
    code << "}\n\n} // namespace app\n";
    return code.str();
}

} // namespace app::designer
