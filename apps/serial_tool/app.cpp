#include "eui_neo.h"

#include "modules/serial/serial.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace app {
namespace {

// =============================================================================
// 常量定义
// =============================================================================
constexpr eui::Color kBackground  {0.930f, 0.945f, 0.940f, 1.0f};
constexpr eui::Color kSurface     {0.990f, 0.992f, 0.988f, 1.0f};
constexpr eui::Color kSurfaceHover{0.900f, 0.928f, 0.920f, 1.0f};
constexpr eui::Color kSurfaceAct  {0.830f, 0.875f, 0.865f, 1.0f};
constexpr eui::Color kInk         {0.055f, 0.068f, 0.074f, 1.0f};
constexpr eui::Color kMuted       {0.420f, 0.470f, 0.480f, 1.0f};
constexpr eui::Color kBorder      {0.720f, 0.780f, 0.770f, 0.92f};
constexpr eui::Color kTeal        {0.035f, 0.520f, 0.445f, 1.0f};
constexpr eui::Color kBlue        {0.205f, 0.390f, 0.810f, 1.0f};
constexpr eui::Color kAmber       {0.930f, 0.540f, 0.130f, 1.0f};
constexpr eui::Color kRose        {0.810f, 0.235f, 0.345f, 1.0f};
constexpr eui::Color kGreen       {0.180f, 0.630f, 0.350f, 1.0f};
constexpr eui::Color kClear       {0.0f,   0.0f,   0.0f,   0.0f};
constexpr float kConfigCollapsedHeight = 42.0f;
constexpr float kConfigExpandedHeight = 194.0f;

// =============================================================================
// 日志条目
// =============================================================================
struct LogEntry {
    std::string time;
    std::string dir;     // TX / RX / SYS / ERR
    std::string mode;    // TEXT / HEX
    std::string bytes;
    std::string payload;
};

// =============================================================================
// 串口配置
// =============================================================================
struct SerialConfig {
    std::string portName = "COM3";
    unsigned int baudRate = 115200;
    core::serial::DataBits dataBits = core::serial::DataBits::Bits8;
    core::serial::Parity parity = core::serial::Parity::None;
    core::serial::StopBits stopBits = core::serial::StopBits::One;
};

// =============================================================================
// 应用状态
// =============================================================================
struct AppState {
    // 串口对象与配置
    core::serial::SerialPort serial;
    SerialConfig config;
    bool connected = false;
    bool opening = false;          // 正在打开连接中
    std::string lastError;

    // 发送模式: 0=Text, 1=HEX
    int mode = 0;
    std::string textPayload = "Hello EUI";
    std::string hexPayload = "AA 55 01 02 03";

    // 自动接收
    bool autoReceive = true;

    // 统计
    int tick = 0;
    int txFrames = 0;
    int rxFrames = 0;
    int txBytes = 0;
    int rxBytes = 0;
    int textBytes = 0;
    int hexBytes = 0;
    std::array<int, 4> frameMix{{0, 0, 0, 0}};
    std::vector<float> throughput{0.10f, 0.14f, 0.18f, 0.16f, 0.24f,
                                  0.22f, 0.30f, 0.28f, 0.36f, 0.32f};

    // 日志
    std::vector<LogEntry> logs;

    // 接收缓冲区
    std::vector<char> rxBuffer;
    bool rxHexMode = false;

    // 配置面板展开
    bool configExpanded = false;

    // Dropdown 状态
    bool baudDropdownOpen = false;

    // 图表样式
    components::LineStyle chartStyle = components::LineStyle::Linear;
};

AppState state;

// =============================================================================
// 工具函数
// =============================================================================

std::string number(int value) {
    char buf[32]{};
    std::snprintf(buf, sizeof(buf), "%d", value);
    return buf;
}

std::string bytesText(int value) {
    char buf[32]{};
    if (value >= 1024 * 1024) {
        std::snprintf(buf, sizeof(buf), "%.1f MB", value / (1024.0f * 1024.0f));
    } else if (value >= 1024) {
        std::snprintf(buf, sizeof(buf), "%.1f KB", value / 1024.0f);
    } else {
        std::snprintf(buf, sizeof(buf), "%d B", value);
    }
    return buf;
}

std::string stamp() {
    char buf[16]{};
    std::snprintf(buf, sizeof(buf), "T+%03d", state.tick++);
    return buf;
}

std::string byteHex(int value) {
    static constexpr char kDigits[] = "0123456789ABCDEF";
    std::string out;
    out.push_back(kDigits[(value >> 4) & 0x0F]);
    out.push_back(kDigits[value & 0x0F]);
    return out;
}

std::string trim(std::string value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
        value.erase(value.begin());
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
        value.pop_back();
    return value;
}

std::string shorten(std::string value, int limit) {
    if (static_cast<int>(value.size()) <= limit || limit < 4)
        return value;
    value.resize(static_cast<std::size_t>(limit - 3));
    return value + "...";
}

// 将二进制数据转为 HEX 字符串
std::string bytesToHex(const unsigned char* data, int len) {
    std::string out;
    for (int i = 0; i < len; ++i) {
        if (!out.empty()) out.push_back(' ');
        out.push_back("0123456789ABCDEF"[(data[i] >> 4) & 0x0F]);
        out.push_back("0123456789ABCDEF"[data[i] & 0x0F]);
    }
    return out;
}

// 将二进制数据转为可打印字符串（不可打印字符替换为 .）
std::string bytesToSafeText(const unsigned char* data, int len) {
    std::string out;
    for (int i = 0; i < len; ++i) {
        char c = static_cast<char>(data[i]);
        out.push_back((c >= 32 && c < 127) ? c : '.');
    }
    return out;
}

// 将 HEX 输入字符串规范化: "AA 55 01" -> "AA 55 01"
std::string normalizeHex(const std::string& input, bool& ok, int& byteCount) {
    std::string digits;
    ok = true;
    byteCount = 0;
    for (char ch : input) {
        if (std::isxdigit(static_cast<unsigned char>(ch))) {
            digits.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
        } else if (std::isspace(static_cast<unsigned char>(ch)) || ch == ',' || ch == ':' || ch == '-') {
            continue;
        } else {
            ok = false;
            return {};
        }
    }
    if (digits.empty() || digits.size() % 2 != 0) {
        ok = false;
        return {};
    }
    std::string result;
    for (std::size_t i = 0; i < digits.size(); i += 2) {
        if (!result.empty()) result.push_back(' ');
        result.push_back(digits[i]);
        result.push_back(digits[i + 1]);
        ++byteCount;
    }
    return result;
}

int countHexBytes(const std::string& value) {
    if (value.empty()) return 0;
    int count = 1;
    for (char ch : value) {
        if (ch == ' ') ++count;
    }
    return count;
}

// 将 HEX 字符串转为二进制数据
std::vector<unsigned char> hexToBytes(const std::string& hex) {
    std::vector<unsigned char> bytes;
    std::string digits;
    for (char ch : hex) {
        if (std::isxdigit(static_cast<unsigned char>(ch))) {
            digits.push_back(ch);
        }
    }
    for (std::size_t i = 0; i + 1 < digits.size(); i += 2) {
        unsigned int byte = 0;
        std::sscanf(digits.c_str() + i, "%2x", &byte);
        bytes.push_back(static_cast<unsigned char>(byte));
    }
    return bytes;
}

// =============================================================================
// 统计更新
// =============================================================================

void pushTrafficValue(int byteCount) {
    float value = std::clamp(0.08f + static_cast<float>(byteCount) / 110.0f, 0.04f, 1.0f);
    if (state.throughput.size() < 10u)
        state.throughput.assign(10, 0.06f);
    state.throughput.erase(state.throughput.begin());
    state.throughput.push_back(value);
}

void addLog(const std::string& dir, const std::string& mode,
            const std::string& payload, int byteCount) {
    state.logs.insert(state.logs.begin(), {stamp(), dir, mode, number(byteCount), payload});
    if (state.logs.size() > 100u)
        state.logs.pop_back();
}

void record(bool tx, bool hex, int byteCount) {
    if (tx) {
        ++state.txFrames;
        state.txBytes += byteCount;
    } else {
        ++state.rxFrames;
        state.rxBytes += byteCount;
    }
    if (hex)
        state.hexBytes += byteCount;
    else
        state.textBytes += byteCount;

    int mixIndex = tx ? (hex ? 1 : 0) : (hex ? 3 : 2);
    ++state.frameMix[static_cast<std::size_t>(mixIndex)];
    pushTrafficValue(byteCount);
}

void clearData() {
    state.txFrames = 0;
    state.rxFrames = 0;
    state.txBytes = 0;
    state.rxBytes = 0;
    state.textBytes = 0;
    state.hexBytes = 0;
    state.frameMix = {{0, 0, 0, 0}};
    state.throughput.assign(10, 0.06f);
    state.logs.clear();
}

// =============================================================================
// 串口操作
// =============================================================================

std::string configToString() {
    const auto& cfg = state.config;
    static const char* kDataBits[] = {"5", "6", "7", "8", "16"};
    static const char* kParity[]   = {"N", "E", "O", "M", "S"};
    static const char* kStopBits[] = {"1", "1.5", "2"};

    char buf[64]{};
    std::snprintf(buf, sizeof(buf), "%s  %u  %s%s%s",
                  cfg.portName.c_str(),
                  cfg.baudRate,
                  kDataBits[static_cast<int>(cfg.dataBits)],
                  kParity[static_cast<int>(cfg.parity)],
                  kStopBits[static_cast<int>(cfg.stopBits)]);
    return buf;
}

void openPort() {
    if (state.connected) {
        addLog("SYS", "INFO", "Port already open", 0);
        return;
    }

    state.opening = true;
    state.lastError.clear();

    const auto& cfg = state.config;
    auto result = state.serial.open(
        cfg.portName.c_str(),
        cfg.baudRate,
        cfg.dataBits,
        cfg.parity,
        cfg.stopBits
    );

    if (result.ok) {
        state.connected = true;
        addLog("SYS", "INFO", "Opened " + configToString(), 0);
    } else {
        state.lastError = result.error == core::serial::OpenError::DeviceNotFound  ? "Device not found" :
                          result.error == core::serial::OpenError::OpenFailed      ? "Open failed" :
                          result.error == core::serial::OpenError::PortParamsFailed ? "Port params failed" :
                          result.error == core::serial::OpenError::SpeedNotRecognized ? "Speed not recognized" :
                          result.error == core::serial::OpenError::WriteParamsFailed ? "Write params failed" :
                          result.error == core::serial::OpenError::TimeoutParamsFailed ? "Timeout params failed" :
                          result.error == core::serial::OpenError::DatabitsNotRecognized ? "Databits invalid" :
                          result.error == core::serial::OpenError::StopbitsNotRecognized ? "Stopbits invalid" :
                          result.error == core::serial::OpenError::ParityNotRecognized ? "Parity invalid" :
                          "Unknown error";
        addLog("ERR", "INFO", "Open failed: " + state.lastError, 0);
    }

    state.opening = false;
}

void closePort() {
    if (!state.connected) return;

    state.serial.close();
    state.connected = false;
    addLog("SYS", "INFO", "Closed", 0);
}

void togglePort() {
    if (state.connected) {
        closePort();
    } else {
        openPort();
    }
}

// 发送数据
void sendData() {
    if (!state.connected) {
        addLog("SYS", "INFO", "Open port before sending", 0);
        pushTrafficValue(0);
        return;
    }

    bool hex = state.mode == 1;
    bool ok = true;

    if (hex) {
        int byteCount = 0;
        std::string normalized = normalizeHex(state.hexPayload, ok, byteCount);
        if (!ok) {
            addLog("ERR", "HEX", "Invalid hex — use complete byte pairs, e.g. AA 55 01", 0);
            pushTrafficValue(0);
            return;
        }
        state.hexPayload = normalized;

        auto bytes = hexToBytes(normalized);
        bool sent = state.serial.writeBytes(bytes.data(), static_cast<unsigned int>(bytes.size()));
        if (sent) {
            addLog("TX", "HEX", normalized, byteCount);
            record(true, true, byteCount);
        } else {
            addLog("ERR", "HEX", "Write failed", 0);
        }
    } else {
        std::string payload = trim(state.textPayload);
        if (payload.empty()) payload = "Hello EUI";
        state.textPayload = payload;

        bool sent = state.serial.writeString(payload.c_str());
        if (sent) {
            int byteCount = static_cast<int>(payload.size());
            addLog("TX", "TEXT", payload, byteCount);
            record(true, false, byteCount);
        } else {
            addLog("ERR", "TEXT", "Write failed", 0);
        }
    }
}

// 尝试从串口读取数据
void tryRead() {
    if (!state.connected) return;

    // 先检查有多少字节待读
    int avail = state.serial.available();
    if (avail <= 0) return;

    // 读取最多 1024 字节
    unsigned char buffer[1024];
    int bytesRead = state.serial.readBytes(buffer, std::min(avail, 1024), 100);

    if (bytesRead > 0) {
        // 判断显示模式: 如果所有字节都可打印，显示为 TEXT，否则显示 HEX
        bool printable = true;
        for (int i = 0; i < bytesRead; ++i) {
            char c = static_cast<char>(buffer[i]);
            if (c != '\n' && c != '\r' && c != '\t' && (c < 32 || c >= 127)) {
                printable = false;
                break;
            }
        }

        std::string payload;
        std::string mode;
        if (printable && bytesRead <= 256) {
            // 文本模式
            payload = bytesToSafeText(buffer, bytesRead);
            mode = "TEXT";
        } else {
            // HEX 模式
            payload = bytesToHex(buffer, bytesRead);
            mode = "HEX";
        }

        addLog("RX", mode, shorten(payload, 120), bytesRead);
        record(false, mode == "HEX", bytesRead);
    }
}

// =============================================================================
// 图表数据
// =============================================================================

std::vector<float> barValues() {
    int maxValue = std::max({state.txBytes, state.rxBytes, state.textBytes, state.hexBytes, 1});
    return {
        static_cast<float>(state.txBytes) / static_cast<float>(maxValue),
        static_cast<float>(state.rxBytes) / static_cast<float>(maxValue),
        static_cast<float>(state.textBytes) / static_cast<float>(maxValue),
        static_cast<float>(state.hexBytes) / static_cast<float>(maxValue)
    };
}

std::vector<float> pieValues() {
    int total = state.frameMix[0] + state.frameMix[1] + state.frameMix[2] + state.frameMix[3];
    if (total == 0) return {1.0f, 1.0f, 1.0f, 1.0f};
    return {
        static_cast<float>(state.frameMix[0]),
        static_cast<float>(state.frameMix[1]),
        static_cast<float>(state.frameMix[2]),
        static_cast<float>(state.frameMix[3])
    };
}

std::string chartStyleText() {
    switch (state.chartStyle) {
        case components::LineStyle::Curve:  return "Curve";
        case components::LineStyle::Step:   return "Step";
        default:                            return "Linear";
    }
}

void nextChartStyle() {
    switch (state.chartStyle) {
        case components::LineStyle::Linear: state.chartStyle = components::LineStyle::Curve; break;
        case components::LineStyle::Curve:  state.chartStyle = components::LineStyle::Step;  break;
        case components::LineStyle::Step:   state.chartStyle = components::LineStyle::Linear; break;
    }
}

// =============================================================================
// 主题
// =============================================================================

components::theme::ThemeColorTokens themeTokens() {
    return {kBackground, kTeal, kSurface, kSurfaceHover, kSurfaceAct, kInk, kBorder, false};
}

eui::Transition transition() {
    return eui::Transition::make(0.16f, eui::Ease::OutCubic);
}

eui::Transition chartTransition() {
    return eui::Transition::make(0.52f, eui::Ease::InOutCubic);
}

eui::Color alpha(eui::Color color, float value) {
    color.a = std::clamp(value, 0.0f, 1.0f);
    return color;
}

eui::Color soft(eui::Color color, float value) {
    color.a = std::clamp(value, 0.0f, 1.0f);
    return color;
}

// =============================================================================
// UI 组件
// =============================================================================

void label(eui::Ui& ui, const std::string& id, float x, float y,
           float width, float height, const std::string& value,
           float fontSize, eui::Color color,
           eui::HorizontalAlign align = eui::HorizontalAlign::Left) {
    ui.text(id)
        .x(x).y(y).size(width, height)
        .text(value)
        .fontSize(fontSize)
        .lineHeight(fontSize + 5.0f)
        .color(color)
        .horizontalAlign(align)
        .verticalAlign(eui::VerticalAlign::Center)
        .build();
}

void button(eui::Ui& ui, const std::string& id, float x, float y,
            float width, float height, const std::string& text,
            unsigned int icon, bool primary, std::function<void()> onClick) {
    eui::Color base    = primary ? kTeal : kSurfaceHover;
    eui::Color hover   = primary ? eui::mixColor(kTeal, {1.0f, 1.0f, 1.0f, 1.0f}, 0.12f)
                                 : eui::mixColor(kSurfaceHover, kTeal, 0.08f);
    eui::Color pressed = primary ? eui::mixColor(kTeal, {0.0f, 0.0f, 0.0f, 1.0f}, 0.12f) : kSurfaceAct;
    eui::Color textCol = primary ? eui::Color{0.98f, 1.0f, 0.99f, 1.0f} : kInk;

    ui.stack(id + ".wrap")
        .x(x).y(y).size(width, height)
        .content([&] {
            components::button(ui, id)
                .size(width, height)
                .icon(icon)
                .iconSize(13.0f)
                .fontSize(13.0f)
                .text(text)
                .colors(base, hover, pressed)
                .textColor(textCol)
                .iconColor(primary ? textCol : kTeal)
                .radius(8.0f)
                .border(1.0f, primary ? alpha(kTeal, 0.58f) : alpha(kBorder, 0.70f))
                .shadow(0.0f, 0.0f, 0.0f, kClear)
                .transition(transition())
                .onClick(std::move(onClick))
                .build();
        })
        .build();
}

void panel(eui::Ui& ui, const std::string& id, float x, float y, float width, float height) {
    ui.rect(id)
        .x(x).y(y).size(width, height)
        .color(kSurface)
        .radius(10.0f)
        .border(1.0f, alpha(kBorder, 0.78f))
        .build();
}

eui::Color dirColor(const std::string& dir) {
    if (dir == "TX")  return kTeal;
    if (dir == "RX")  return kBlue;
    if (dir == "ERR") return kRose;
    return kMuted;
}

// =============================================================================
// 配置面板 — 串口参数设置
// =============================================================================

void composeConfig(eui::Ui& ui, float x, float y, float width) {
    const float panelH = state.configExpanded ? kConfigExpandedHeight : kConfigCollapsedHeight;

    panel(ui, "config.bg", x, y, width, panelH);

    // 标题栏 / 折叠按钮
    ui.stack("config.header")
        .x(x).y(y).size(width, kConfigCollapsedHeight)
        .content([&] {
            label(ui, "config.title", 14.0f, 0.0f, 140.0f, kConfigCollapsedHeight,
                  "Serial Configuration", 15.0f, kInk);

            if (!state.connected) {
                label(ui, "config.hint", 160.0f, 0.0f, width - 212.0f, kConfigCollapsedHeight,
                      state.lastError.empty()
                          ? "Click to expand and configure port settings"
                          : "Last error: " + state.lastError,
                      12.0f, state.lastError.empty() ? kMuted : kRose);
            }

            ui.rect("config.expand.hit")
                .x(width - 38.0f).y(4.0f).size(34.0f, 34.0f)
                .states(kClear, alpha(kSurfaceHover, 0.5f), kSurfaceAct)
                .radius(6.0f)
                .onClick([&] {
                    state.configExpanded = !state.configExpanded;
                    if (!state.configExpanded) state.baudDropdownOpen = false;
                })
                .build();
            ui.text("config.expand.icon")
                .x(width - 38.0f).y(4.0f).size(34.0f, 34.0f)
                .icon(state.configExpanded ? 0xF077 : 0xF078)
                .fontSize(14.0f)
                .color(kMuted)
                .horizontalAlign(eui::HorizontalAlign::Center)
                .verticalAlign(eui::VerticalAlign::Center)
                .build();
        })
        .build();

    if (!state.configExpanded) return;

    const float inset = 14.0f;
    const float fieldGap = 18.0f;
    const float labelW = 52.0f;
    const float fieldW = (width - inset * 2.0f - fieldGap) * 0.5f;
    const float controlW = fieldW - labelW;
    const float leftX = x + inset;
    const float rightX = leftX + fieldW + fieldGap;
    const float row1 = y + 52.0f;
    const float row2 = row1 + 44.0f;
    const float row3 = row2 + 44.0f;

    // ---- Port Name ----
    label(ui, "cfg.port.label", leftX, row1, labelW, 36.0f, "Port", 13.0f, kInk);
    ui.stack("cfg.port.wrap")
        .x(leftX + labelW).y(row1).size(controlW, 36.0f)
        .content([&] {
            components::input(ui, "cfg.port")
                .theme(themeTokens())
                .size(controlW, 36.0f)
                .fontSize(14.0f)
                .placeholder("COM3 or /dev/ttyUSB0")
                .value(state.config.portName)
                .onChange([](const std::string& v) { state.config.portName = v; })
                .build();
        })
        .build();

    // ---- Baud Rate ----
    label(ui, "cfg.baud.label", rightX, row1, labelW, 36.0f, "Baud", 13.0f, kInk);
    static const std::vector<int> kBaudRates = {
        110, 300, 600, 1200, 2400, 4800, 9600, 19200,
        38400, 57600, 115200, 230400, 460800, 921600
    };
    int baudIdx = 0;
    for (int i = 0; i < static_cast<int>(kBaudRates.size()); ++i) {
        if (kBaudRates[i] == static_cast<int>(state.config.baudRate)) {
            baudIdx = i;
            break;
        }
    }
    const float baudX = rightX + labelW;
    ui.rect("cfg.baud.field")
        .x(baudX).y(row1).size(controlW, 36.0f)
        .color(kSurface)
        .radius(8.0f)
        .border(1.0f, alpha(kBorder, 0.78f))
        .build();
    const float baudArrowW = 32.0f;
    ui.rect("cfg.baud.previous.hit")
        .x(baudX + 2.0f).y(row1 + 2.0f).size(baudArrowW, 32.0f)
        .states(kClear, alpha(kTeal, 0.10f), alpha(kTeal, 0.18f))
        .radius(6.0f)
        .onClick([baudIdx] {
            const int previous = std::max(0, baudIdx - 1);
            state.config.baudRate = static_cast<unsigned int>(kBaudRates[static_cast<std::size_t>(previous)]);
        })
        .build();
    ui.text("cfg.baud.previous.icon")
        .x(baudX + 2.0f).y(row1 + 2.0f).size(baudArrowW, 32.0f)
        .icon(0xF053)
        .fontSize(13.0f)
        .color(kTeal)
        .horizontalAlign(eui::HorizontalAlign::Center)
        .verticalAlign(eui::VerticalAlign::Center)
        .build();
    label(ui, "cfg.baud.value", baudX + baudArrowW + 4.0f, row1,
          controlW - (baudArrowW + 4.0f) * 2.0f, 36.0f,
          number(kBaudRates[static_cast<std::size_t>(baudIdx)]), 14.0f, kInk,
          eui::HorizontalAlign::Center);
    ui.rect("cfg.baud.next.hit")
        .x(baudX + controlW - baudArrowW - 2.0f).y(row1 + 2.0f).size(baudArrowW, 32.0f)
        .states(kClear, alpha(kTeal, 0.10f), alpha(kTeal, 0.18f))
        .radius(6.0f)
        .onClick([baudIdx] {
            const int next = std::min(static_cast<int>(kBaudRates.size()) - 1, baudIdx + 1);
            state.config.baudRate = static_cast<unsigned int>(kBaudRates[static_cast<std::size_t>(next)]);
        })
        .build();
    ui.text("cfg.baud.next.icon")
        .x(baudX + controlW - baudArrowW - 2.0f).y(row1 + 2.0f).size(baudArrowW, 32.0f)
        .icon(0xF054)
        .fontSize(13.0f)
        .color(kTeal)
        .horizontalAlign(eui::HorizontalAlign::Center)
        .verticalAlign(eui::VerticalAlign::Center)
        .build();

    // ---- Data Bits ----
    label(ui, "cfg.databits.label", leftX, row2, labelW, 36.0f, "Data", 13.0f, kInk);
    ui.stack("cfg.databits.wrap")
        .x(leftX + labelW).y(row2).size(controlW, 36.0f)
        .content([&] {
            components::segmented(ui, "cfg.databits")
                .theme(themeTokens())
                .size(controlW, 36.0f)
                .items({"5", "6", "7", "8"})
                .selected(static_cast<int>(state.config.dataBits))
                .fontSize(13.0f)
                .onChange([](int idx) {
                    state.config.dataBits = static_cast<core::serial::DataBits>(idx);
                })
                .build();
        })
        .build();

    // ---- Parity ----
    label(ui, "cfg.parity.label", rightX, row2, labelW, 36.0f, "Parity", 13.0f, kInk);
    ui.stack("cfg.parity.wrap")
        .x(rightX + labelW).y(row2).size(controlW, 36.0f)
        .content([&] {
            std::vector<std::string> items = {"None", "Even", "Odd"};
            int pIdx = static_cast<int>(state.config.parity);
            if (pIdx > 2) pIdx = 0;
            components::segmented(ui, "cfg.parity")
                .theme(themeTokens())
                .size(controlW, 36.0f)
                .items(items)
                .selected(pIdx)
                .fontSize(13.0f)
                .onChange([](int idx) {
                    state.config.parity = static_cast<core::serial::Parity>(idx);
                })
                .build();
        })
        .build();

    // ---- Stop Bits ----
    label(ui, "cfg.stopbits.label", leftX, row3, labelW, 36.0f, "Stop", 13.0f, kInk);
    ui.stack("cfg.stopbits.wrap")
        .x(leftX + labelW).y(row3).size(controlW, 36.0f)
        .content([&] {
            components::segmented(ui, "cfg.stopbits")
                .theme(themeTokens())
                .size(controlW, 36.0f)
                .items({"1", "2"})
                .selected(static_cast<int>(state.config.stopBits) == 2 ? 1 : 0)
                .fontSize(13.0f)
                .onChange([](int idx) {
                    state.config.stopBits = idx == 1
                        ? core::serial::StopBits::Two
                        : core::serial::StopBits::One;
                })
                .build();
        })
        .build();

    // ---- Open / Close Button ----
    button(ui, "cfg.openclose", x + width - 146.0f, row3, 130.0f, 38.0f,
           state.connected ? "Close Port" : "Open Port",
           state.connected ? 0xF00D : 0xF011,
           !state.connected,
           [] { togglePort(); });

    // ---- Status indicator ----
    label(ui, "cfg.status.label", rightX, row3, 52.0f, 30.0f, "Status", 13.0f, kInk);
    ui.rect("cfg.status.dot")
        .x(rightX + 54.0f).y(row3 + 7.0f).size(16.0f, 16.0f)
        .color(state.connected ? kGreen : kRose)
        .radius(8.0f)
        .build();
    label(ui, "cfg.status.text", rightX + 76.0f, row3, 170.0f, 30.0f,
          state.connected ? "Connected — " + configToString() : "Closed",
          13.0f, state.connected ? kGreen : kMuted);

}

// =============================================================================
// 头部
// =============================================================================

void composeHeader(eui::Ui& ui, float x, float y, float width) {
    label(ui, "header.title", x, y, 170.0f, 34.0f, "Serial Tool", 24.0f, kInk);

    const std::string cfgStr = state.connected ? configToString() : "Not connected";
    label(ui, "header.port", x + 176.0f, y, 300.0f, 34.0f, cfgStr, 14.0f, kMuted);

    ui.rect("header.status.bg")
        .x(x + width - 266.0f).y(y).size(118.0f, 34.0f)
        .color(state.connected ? soft(kTeal, 0.12f) : soft(kRose, 0.12f))
        .radius(8.0f)
        .border(1.0f, state.connected ? alpha(kTeal, 0.32f) : alpha(kRose, 0.32f))
        .build();
    ui.text("header.status.text")
        .x(x + width - 266.0f).y(y).size(118.0f, 34.0f)
        .text(state.connected ? "Connected" : "Closed")
        .fontSize(13.0f)
        .lineHeight(13.0f)
        .color(state.connected ? kTeal : kRose)
        .horizontalAlign(eui::HorizontalAlign::Center)
        .verticalAlign(eui::VerticalAlign::Center)
        .build();

    button(ui, "header.clear", x + width - 130.0f, y, 130.0f, 34.0f,
           "Clear Data", 0xF12D, false, [] { clearData(); });
}

// =============================================================================
// 指标栏
// =============================================================================

void composeMetricBar(eui::Ui& ui, float x, float y, float width) {
    const float h = 58.0f;
    const float cellW = width / 6.0f;
    panel(ui, "metrics.bg", x, y, width, h);

    auto cell = [&](const std::string& id, int index, const std::string& name,
                    const std::string& value, eui::Color color) {
        float cx = x + static_cast<float>(index) * cellW;
        if (index > 0) {
            ui.rect(id + ".line")
                .x(cx).y(y + 11.0f).size(1.0f, h - 22.0f)
                .color(alpha(kBorder, 0.52f))
                .build();
        }
        ui.rect(id + ".dot")
            .x(cx + 14.0f).y(y + 16.0f).size(7.0f, 7.0f)
            .color(color).radius(3.5f)
            .build();
        label(ui, id + ".name", cx + 28.0f, y + 8.0f, cellW - 36.0f, 20.0f, name, 12.0f, kMuted);
        label(ui, id + ".value", cx + 28.0f, y + 30.0f, cellW - 36.0f, 22.0f, value, 18.0f, kInk);
    };

    cell("metric.tx.frames", 0, "TX Frames", number(state.txFrames), kTeal);
    cell("metric.rx.frames", 1, "RX Frames", number(state.rxFrames), kBlue);
    cell("metric.tx.bytes",  2, "TX Bytes",  bytesText(state.txBytes), kTeal);
    cell("metric.rx.bytes",  3, "RX Bytes",  bytesText(state.rxBytes), kBlue);
    cell("metric.text.bytes",4, "Text Bytes",number(state.textBytes), kAmber);
    cell("metric.hex.bytes", 5, "HEX Bytes", number(state.hexBytes), kRose);
}

// =============================================================================
// Modem 信号状态
// =============================================================================

void composeModemStatus(eui::Ui& ui, float x, float y, float width, float height) {
    panel(ui, "modem.bg", x, y, width, height);
    label(ui, "modem.title", x + 14.0f, y + 10.0f, width - 28.0f, 28.0f, "Modem Signals", 17.0f, kInk);

    const float rowH = 26.0f;
    const float startY = y + 46.0f;
    const float contentW = width - 28.0f;
    const float colW = contentW * 0.5f;
    const float valueW = 34.0f;

    // 实时读取信号状态
    bool cts = state.connected ? state.serial.isCts() : false;
    bool dsr = state.connected ? state.serial.isDsr() : false;
    bool dcd = state.connected ? state.serial.isDcd() : false;
    bool ri  = state.connected ? state.serial.isRi()  : false;
    bool dtr = state.connected ? state.serial.isDtr() : false;
    bool rts = state.connected ? state.serial.isRts() : false;

    auto signalRow = [&](const std::string& id, float sx, float sy,
                         const std::string& name, bool on) {
        ui.rect(id + ".dot")
            .x(sx).y(sy + 5.0f).size(14.0f, 14.0f)
            .color(on ? kGreen : alpha(kRose, 0.5f))
            .radius(7.0f)
            .build();
        label(ui, id + ".name", sx + 22.0f, sy, colW - 22.0f - valueW, rowH, name, 14.0f,
              on ? kInk : kMuted);
        label(ui, id + ".val", sx + colW - valueW, sy, valueW, rowH, on ? "ON" : "OFF", 14.0f,
              on ? kGreen : kMuted, eui::HorizontalAlign::Right);
    };

    // 第一列
    signalRow("modem.cts",  x + 14.0f, startY,              "CTS", cts);
    signalRow("modem.dsr",  x + 14.0f, startY + rowH,       "DSR", dsr);
    signalRow("modem.dcd",  x + 14.0f, startY + rowH * 2,   "DCD", dcd);

    // 第二列
    signalRow("modem.ri",   x + 14.0f + colW, startY,       "RI",  ri);
    signalRow("modem.dtr",  x + 14.0f + colW, startY + rowH,"DTR", dtr);
    signalRow("modem.rts",  x + 14.0f + colW, startY + rowH * 2, "RTS", rts);
}

// =============================================================================
// 图表
// =============================================================================

void composeCharts(eui::Ui& ui, float x, float y, float width, float height) {
    const float gap = 12.0f;
    const float lineW = std::max(280.0f, width - 170.0f * 2.0f - gap * 2.0f);
    const std::vector<eui::Color> colors{kTeal, kBlue, kAmber, kRose};

    // 折线图 — 吞吐量
    ui.stack("charts.line.wrap")
        .x(x).y(y).size(lineW, height)
        .content([&] {
            components::lineChart(ui, "charts.line")
                .theme(themeTokens())
                .size(lineW, height)
                .title("Throughput")
                .values(state.throughput)
                .labels({"-9","-8","-7","-6","-5","-4","-3","-2","-1","Now"})
                .style(state.chartStyle)
                .transition(chartTransition())
                .build();

            button(ui, "charts.line.style", lineW - 116.0f, 16.0f, 96.0f, 30.0f,
                   chartStyleText(), 0xF1FC, false, [] { nextChartStyle(); });
        })
        .build();

    // 柱状图
    ui.stack("charts.bar.wrap")
        .x(x + lineW + gap).y(y).size(170.0f, height)
        .content([&] {
            components::barChart(ui, "charts.bar")
                .theme(themeTokens())
                .size(170.0f, height)
                .title("Bytes")
                .values(barValues())
                .labels({"TX","RX","TXT","HEX"})
                .colors(colors)
                .transition(chartTransition())
                .build();
        })
        .build();

    // 饼图
    ui.stack("charts.pie.wrap")
        .x(x + lineW + gap + 170.0f + gap).y(y).size(170.0f, height)
        .content([&] {
            components::pieChart(ui, "charts.pie")
                .theme(themeTokens())
                .size(170.0f, height)
                .title("Mix")
                .values(pieValues())
                .labels({"TX Text","TX HEX","RX Text","RX HEX"})
                .colors(colors)
                .transition(chartTransition())
                .build();
        })
        .build();
}

// =============================================================================
// 发送面板
// =============================================================================

void composeTransmit(eui::Ui& ui, float x, float y, float width, float height) {
    panel(ui, "tx.bg", x, y, width, height);
    label(ui, "tx.title", x + 14.0f, y + 10.0f, 86.0f, 28.0f, "Transmit", 15.0f, kInk);

    // 模式选择: Text / HEX
    ui.stack("tx.mode.wrap")
        .x(x + 104.0f).y(y + 9.0f).size(120.0f, 30.0f)
        .content([&] {
            components::segmented(ui, "tx.mode")
                .theme(themeTokens())
                .size(120.0f, 30.0f)
                .items({"Text", "HEX"})
                .selected(state.mode)
                .fontSize(12.0f)
                .onChange([](int v) { state.mode = v; })
                .build();
        })
        .build();

    // Auto RX 开关
    ui.stack("tx.auto.wrap")
        .x(x + width - 118.0f).y(y + 10.0f).size(112.0f, 28.0f)
        .content([&] {
            components::toggleSwitch(ui, "tx.auto")
                .theme(themeTokens())
                .size(112.0f, 28.0f)
                .trackSize(36.0f, 20.0f)
                .fontSize(11.0f)
                .text("Auto RX")
                .checked(state.autoReceive)
                .onChange([](bool v) { state.autoReceive = v; })
                .build();
        })
        .build();

    // 输入框
    ui.stack("tx.input.wrap")
        .x(x + 14.0f).y(y + 52.0f).size(width - 28.0f, 36.0f)
        .content([&] {
            components::input(ui, "tx.input")
                .theme(themeTokens())
                .size(width - 28.0f, 36.0f)
                .fontSize(14.0f)
                .placeholder(state.mode == 0 ? "Text payload" : "AA 55 01 02")
                .value(state.mode == 0 ? state.textPayload : state.hexPayload)
                .onChange([](const std::string& v) {
                    if (state.mode == 0) state.textPayload = v;
                    else state.hexPayload = v;
                })
                .onEnter([] { sendData(); })
                .build();
        })
        .build();

    const float txGap = 10.0f;
    const float btnW = (width - 28.0f - txGap * 2.0f) / 3.0f;
    const float by = y + height - 42.0f;

    button(ui, "tx.send", x + 14.0f, by, btnW, 32.0f,
           "Send", 0xF1D8, true, [] { sendData(); });
    button(ui, "tx.rx", x + 14.0f + btnW + txGap, by, btnW, 32.0f,
           "Poll RX", 0xF2F1, false, [] { tryRead(); });
    button(ui, "tx.clear", x + 14.0f + (btnW + txGap) * 2.0f, by, btnW, 32.0f,
           "Clear", 0xF12D, false, [] { clearData(); });
}

// =============================================================================
// 日志面板
// =============================================================================

void composeLog(eui::Ui& ui, float x, float y, float width, float height) {
    panel(ui, "log.bg", x, y, width, height);
    label(ui, "log.title", x + 14.0f, y + 10.0f, width - 28.0f, 28.0f,
          "Traffic Data", 15.0f, kInk);

    const float tx = x + 14.0f;
    const float ty = y + 42.0f;
    const float tw = width - 28.0f;
    const float hh = 24.0f;
    const float rowH = 22.0f;
    const float timeW = 68.0f;
    const float dirW = 52.0f;
    const float modeW = 62.0f;
    const float bytesW = 58.0f;
    const float payloadW = std::max(80.0f, tw - timeW - dirW - modeW - bytesW);

    // 表头
    ui.rect("log.header.bg")
        .x(tx).y(ty).size(tw, hh)
        .color(kSurfaceHover)
        .radius(7.0f)
        .border(1.0f, alpha(kBorder, 0.58f))
        .build();
    label(ui, "log.h.time",   tx + 10.0f, ty, timeW - 10.0f, hh, "Time",  12.0f, kMuted);
    label(ui, "log.h.dir",    tx + timeW, ty, dirW, hh, "Dir",   12.0f, kMuted);
    label(ui, "log.h.mode",   tx + timeW + dirW, ty, modeW, hh, "Mode",  12.0f, kMuted);
    label(ui, "log.h.bytes",  tx + timeW + dirW + modeW, ty, bytesW, hh, "Bytes", 12.0f, kMuted);
    label(ui, "log.h.payload", tx + timeW + dirW + modeW + bytesW, ty, payloadW - 8.0f, hh, "Payload", 12.0f, kMuted);

    if (state.logs.empty()) {
        label(ui, "log.empty", tx, ty + 54.0f, tw, 24.0f,
              "No traffic yet. Open a port and send/receive data.",
              13.0f, kMuted, eui::HorizontalAlign::Center);
        return;
    }

    const float rowGap = 4.0f;
    const float rowsY = ty + hh + 6.0f;
    const float availableRowsHeight = std::max(0.0f, y + height - rowsY);
    const int visibleRows = static_cast<int>((availableRowsHeight + rowGap) / (rowH + rowGap));
    const int maxRows = std::min(visibleRows, static_cast<int>(state.logs.size()));
    const int payloadChars = std::max(10, static_cast<int>(payloadW / 8.0f));
    for (int i = 0; i < maxRows; ++i) {
        const LogEntry& item = state.logs[static_cast<std::size_t>(i)];
        const float ry = rowsY + static_cast<float>(i) * (rowH + rowGap);

        ui.rect("log.row." + std::to_string(i))
            .x(tx).y(ry).size(tw, rowH)
            .color(i % 2 == 0 ? alpha(kSurfaceHover, 0.42f) : alpha(kSurfaceAct, 0.25f))
            .radius(7.0f)
            .build();
        label(ui, "log.time." + std::to_string(i),  tx + 10.0f, ry, timeW - 10.0f, rowH, item.time, 12.0f, kMuted);
        label(ui, "log.dir." + std::to_string(i),   tx + timeW, ry, dirW, rowH, item.dir, 12.0f, dirColor(item.dir));
        label(ui, "log.mode." + std::to_string(i),  tx + timeW + dirW, ry, modeW, rowH, item.mode, 12.0f, kMuted);
        label(ui, "log.bytes." + std::to_string(i), tx + timeW + dirW + modeW, ry, bytesW, rowH, item.bytes, 12.0f, kMuted);
        label(ui, "log.payload." + std::to_string(i), tx + timeW + dirW + modeW + bytesW, ry, payloadW - 8.0f, rowH,
              shorten(item.payload, payloadChars), 12.0f, kInk);
    }
}

} // anonymous namespace

// =============================================================================
// DSL App Config
// =============================================================================

const DslAppConfig& dslAppConfig() {
    static const DslAppConfig config = DslAppConfig{}
        .title("Serial Tool")
        .pageId("serial_tool")
        .clearColor(kBackground)
        .windowSize(1440, 1080)
        .fps(90.0)
        .tray(true);
    return config;
}

// =============================================================================
// Main compose function
// =============================================================================

void compose(eui::Ui& ui, const eui::Screen& screen) {
    const float margin = std::clamp(screen.width * 0.025f, 18.0f, 30.0f);
    const float contentW = std::max(960.0f, screen.width - margin * 2.0f);
    const float x = (screen.width - contentW) * 0.5f;

    const float configY = 16.0f;
    const float configH = (state.configExpanded ? kConfigExpandedHeight : kConfigCollapsedHeight) + 10.0f;
    const float headerY = configY + configH + 8.0f;
    const float metricsY = headerY + 42.0f;
    const float chartsY = metricsY + 66.0f;
    const float chartsH = 280.0f;
    const float modemW = 240.0f;
    const float bottomY = chartsY + chartsH + 12.0f;
    const float bottomH = 220.0f;
    const float txW = std::clamp(contentW * 0.34f, 400.0f, 480.0f);
    const float gap = 12.0f;

    ui.stack("root")
        .size(screen.width, screen.height)
        .content([&] {
            ui.rect("background")
                .size(screen.width, screen.height)
                .color(kBackground)
                .build();

            // 配置面板
            composeConfig(ui, x, configY, contentW);

            // 头部
            composeHeader(ui, x, headerY, contentW);

            // 指标栏
            composeMetricBar(ui, x, metricsY, contentW);

            // 图表区域
            composeCharts(ui, x, chartsY, contentW, chartsH);

            // 底部 — 发送面板 + 日志 + Modem 状态
            composeTransmit(ui, x, bottomY, txW, bottomH);
            composeLog(ui, x + txW + gap, bottomY,
                       contentW - txW - gap - modemW - gap, bottomH);
            composeModemStatus(ui, x + contentW - modemW, bottomY, modemW, bottomH);

            // 定时器 — 自动接收
            ui.stack("auto.receive.timer")
                .size(0.0f, 0.0f)
                .onTimer(0.25f, [] {
                    if (state.connected && state.autoReceive) {
                        tryRead();
                    }
                })
                .build();

            // 定时器 — 定期刷新 Modem 信号显示
            ui.stack("modem.refresh.timer")
                .size(0.0f, 0.0f)
                .onTimer(0.5f, [] {
                    // 触发重绘 (通过修改 tick 实现)
                    if (state.connected) {
                        state.tick++;
                    }
                })
                .build();
        })
        .build();
}

} // namespace app
