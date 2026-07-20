#pragma once

#include "eui/types.h"

#include <string>
#include <vector>

namespace app::designer {

enum class NodeType {
    Row, Column, Stack, Scroll, ScrollView, VirtualList, Rect, Polygon, Svg, Panel, Card, Text,
    Markdown, Button, Input, Checkbox, Radio, Switch, Slider, Progress, Segmented, Stepper, Tabs,
    Dropdown, DatePicker, TimePicker, ColorPicker, DataTable, Dialog, Sidebar, Navbar, Toast,
    Tooltip, ContextMenu, Carousel, LineChart, BarChart, PieChart, MouseArea, Image, HeartSwitch,
    NeumorphicButton, CardSlider, TiltCard
};

struct Node {
    int uid = 0;
    NodeType type = NodeType::Text;
    std::string id;
    std::string text;
    std::string itemsText = "One, Two, Three";
    std::string labelsText = "A, B, C, D";
    std::string valuesText = "18, 42, 31, 58";
    std::string messageText = "Component content";
    std::string primaryText = "Confirm";
    std::string secondaryText = "Cancel";
    std::string valueText;
    std::string prefixText;
    std::string eyebrowText = "MENU";
    std::string fontFamily;
    float x = 40.0f;
    float y = 40.0f;
    float width = 180.0f;
    float height = 48.0f;
    float radius = 8.0f;
    float fontSize = 18.0f;
    float value = 0.55f;
    float gap = 12.0f;
    float padding = 12.0f;
    float borderWidth = 1.0f;
    float opacity = 1.0f;
    float rotation = 0.0f;
    float blur = 0.0f;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
    float shadowBlur = 16.0f;
    float shadowOffsetX = 0.0f;
    float shadowOffsetY = 6.0f;
    float scrollStep = 40.0f;
    float scrollbarWidth = 6.0f;
    float rowHeight = 34.0f;
    float cardWidthRatio = 0.78f;
    float overlap = 0.20f;
    float parallax = 0.12f;
    float duration = 0.45f;
    float interval = 4.0f;
    float cardSpacing = 24.0f;
    float iconSize = 18.0f;
    float controlSize = 20.0f;
    float trackWidth = 40.0f;
    float trackHeight = 22.0f;
    float inset = 12.0f;
    float itemHeight = 40.0f;
    float drawerWidth = 360.0f;
    float pressScale = 0.965f;
    float maxTilt = 0.12f;
    float dragThreshold = 4.0f;
    float maxScrollStep = 120.0f;
    float margin = 0.0f;
    float toastDuration = 4.0f;
    float lineHeight = 23.4f;
    eui::Color color{0.16f, 0.48f, 0.94f, 1.0f};
    eui::Color gradientColor{0.12f, 0.68f, 0.50f, 1.0f};
    eui::Color borderColor{0.12f, 0.14f, 0.18f, 0.55f};
    eui::Color shadowColor{0.0f, 0.0f, 0.0f, 0.28f};
    eui::Color foregroundColor{0.06f, 0.075f, 0.10f, 1.0f};
    int parentUid = -1;
    int horizontalAlign = 1;
    int verticalAlign = 1;
    int mainAlign = 0;
    int crossAlign = 0;
    int polygonSides = 3;
    int year = 2026;
    int month = 7;
    int day = 17;
    int hour = 10;
    int minute = 30;
    int minuteStep = 5;
    int selectedIndex = 0;
    int itemCount = 8;
    int numberBase = 10;
    int digits = 0;
    int bitWidth = 0;
    int fontWeight = 700;
    unsigned int icon = 0;
    long long minimum = 0;
    long long maximum = 100;
    long long step = 1;
    bool checked = true;
    bool multiline = false;
    bool compact = false;
    bool autoPlay = true;
    bool participatesInLayout = true;
    bool locked = false;
    bool gradientEnabled = false;
    bool gradientHorizontal = false;
    bool shadowEnabled = false;
    bool insetShadow = false;
    bool backgroundEnabled = true;
    bool tiltEnabled = true;
    bool disabled = false;
    bool showBasePrefix = false;
    bool uppercase = false;
    bool wrapText = false;
};

struct CodegenDocument {
    const std::vector<Node>& nodes;
    std::string title;
    std::string pageId;
    int width = 960;
    int height = 640;
    eui::Color background;
};

std::string generatedCode(const CodegenDocument& document);

} // namespace app::designer
