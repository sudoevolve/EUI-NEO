#include "eui_neo.h"
#include "core/platform/network.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace app {
namespace {

#if defined(_WIN32)
using SocketHandle = SOCKET;
using SocketLength = int;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
#else
using SocketHandle = int;
using SocketLength = socklen_t;
constexpr SocketHandle kInvalidSocket = -1;
#endif

constexpr eui::Color kBg{0.925f, 0.940f, 0.950f, 1.0f};
constexpr eui::Color kSurface{0.985f, 0.990f, 0.992f, 1.0f};
constexpr eui::Color kSurfaceAlt{0.900f, 0.925f, 0.935f, 1.0f};
constexpr eui::Color kInk{0.045f, 0.060f, 0.070f, 1.0f};
constexpr eui::Color kMuted{0.390f, 0.455f, 0.480f, 1.0f};
constexpr eui::Color kBorder{0.720f, 0.785f, 0.805f, 0.95f};
constexpr eui::Color kBlue{0.110f, 0.360f, 0.820f, 1.0f};
constexpr eui::Color kGreen{0.040f, 0.520f, 0.390f, 1.0f};
constexpr eui::Color kRed{0.780f, 0.200f, 0.285f, 1.0f};
constexpr eui::Color kClear{0.0f, 0.0f, 0.0f, 0.0f};

struct ChatSettings {
    bool dark = false;
    int accent = 0;
    std::string dialogOwner;
};

ChatSettings settings;

std::vector<eui::Color> accentColors() {
    return {
        kBlue,
        {0.580f, 0.250f, 0.840f, 1.0f},
        kGreen,
        {0.900f, 0.420f, 0.120f, 1.0f}
    };
}

eui::Color alpha(eui::Color color, float value) {
    color.a = std::clamp(value, 0.0f, 1.0f);
    return color;
}

components::theme::ThemeColorTokens themeTokens() {
    const std::vector<eui::Color> accents = accentColors();
    const eui::Color primary = accents[static_cast<std::size_t>(std::clamp(settings.accent, 0, static_cast<int>(accents.size()) - 1))];
    if (settings.dark) {
        return {
            {0.070f, 0.082f, 0.092f, 1.0f},
            primary,
            {0.120f, 0.140f, 0.155f, 1.0f},
            {0.170f, 0.200f, 0.220f, 1.0f},
            {0.220f, 0.255f, 0.280f, 1.0f},
            {0.930f, 0.960f, 0.970f, 1.0f},
            {0.310f, 0.370f, 0.390f, 1.0f},
            true
        };
    }
    return {kBg, primary, kSurface, kSurfaceAlt, {0.835f, 0.880f, 0.905f, 1.0f}, kInk, kBorder, false};
}

eui::Color backgroundColor() {
    return themeTokens().background;
}

eui::Color surfaceColor() {
    return themeTokens().surface;
}

eui::Color surfaceAltColor(float opacity = 1.0f) {
    return alpha(themeTokens().surfaceHover, opacity);
}

eui::Color textColor(float opacity = 1.0f) {
    return alpha(themeTokens().text, opacity);
}

eui::Color mutedTextColor() {
    return alpha(themeTokens().text, settings.dark ? 0.66f : 0.58f);
}

eui::Color borderColor() {
    return alpha(themeTokens().border, settings.dark ? 0.90f : 0.95f);
}

eui::Color shadowColor() {
    return settings.dark
        ? eui::Color{0.0f, 0.0f, 0.0f, 0.26f}
        : eui::Color{0.045f, 0.060f, 0.070f, 0.09f};
}

std::string trimLine(std::string value) {
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r')) {
        value.pop_back();
    }
    return value;
}

std::string toText(int value) {
    char buffer[32]{};
    std::snprintf(buffer, sizeof(buffer), "%d", value);
    return buffer;
}

int toPort(const std::string& text, int fallback) {
    char* end = nullptr;
    const long value = std::strtol(text.c_str(), &end, 10);
    if (end == text.c_str() || value <= 0 || value > 65535) {
        return fallback;
    }
    while (end != nullptr && *end != '\0') {
        if (*end != ' ' && *end != '\t' && *end != '\r' && *end != '\n') {
            return fallback;
        }
        ++end;
    }
    return static_cast<int>(value);
}

std::string shortText(std::string value, int limit) {
    if (static_cast<int>(value.size()) <= limit || limit < 4) {
        return value;
    }
    value.resize(static_cast<std::size_t>(limit - 3));
    return value + "...";
}

void requestChatUpdate() {
    app::requestUpdate();
}

void closeSocket(SocketHandle socket) {
    if (socket == kInvalidSocket) {
        return;
    }
#if defined(_WIN32)
    closesocket(socket);
#else
    close(socket);
#endif
}

void setNonBlocking(SocketHandle socket) {
#if defined(_WIN32)
    u_long mode = 1;
    ioctlsocket(socket, FIONBIO, &mode);
#else
    const int flags = fcntl(socket, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(socket, F_SETFL, flags | O_NONBLOCK);
    }
#endif
}

bool wouldBlock() {
#if defined(_WIN32)
    const int error = WSAGetLastError();
    return error == WSAEWOULDBLOCK || error == WSAEINPROGRESS || error == WSAEALREADY;
#else
    return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINPROGRESS;
#endif
}

bool connectInProgress() {
#if defined(_WIN32)
    const int error = WSAGetLastError();
    return error == WSAEWOULDBLOCK || error == WSAEINPROGRESS || error == WSAEALREADY;
#else
    return errno == EINPROGRESS || errno == EALREADY;
#endif
}

bool waitForConnect(SocketHandle socket, int timeoutMs) {
    fd_set writeSet;
    FD_ZERO(&writeSet);
    FD_SET(socket, &writeSet);

    timeval timeout{};
    timeout.tv_sec = timeoutMs / 1000;
    timeout.tv_usec = (timeoutMs % 1000) * 1000;
    const int ready = select(static_cast<int>(socket + 1), nullptr, &writeSet, nullptr, &timeout);
    if (ready <= 0 || !FD_ISSET(socket, &writeSet)) {
        return false;
    }

    int socketError = 0;
    SocketLength errorSize = sizeof(socketError);
    if (getsockopt(socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&socketError), &errorSize) != 0) {
        return false;
    }
    return socketError == 0;
}

struct SocketRuntime {
    SocketRuntime() {
#if defined(_WIN32)
        static std::once_flag once;
        std::call_once(once, [] {
            WSADATA data{};
            WSAStartup(MAKEWORD(2, 2), &data);
        });
#endif
    }
};

SocketRuntime& socketRuntime() {
    static SocketRuntime runtime;
    return runtime;
}

enum class Protocol {
    Tcp,
    Udp
};

enum class Role {
    Server,
    Client
};

struct ChatLine {
    std::string tag;
    std::string text;
    eui::Color color = kMuted;
};

class ChatEndpoint {
public:
    ~ChatEndpoint() {
        stop();
    }

    bool running() const {
        return running_.load();
    }

    Protocol protocol() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return protocol_;
    }

    Role role() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return role_;
    }

    std::string status() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return status_;
    }

    std::vector<ChatLine> lines() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return std::vector<ChatLine>(lines_.begin(), lines_.end());
    }

    void start(Protocol protocol, Role role, std::string host, int port) {
        stop();
        socketRuntime();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            protocol_ = protocol;
            role_ = role;
            host_ = std::move(host);
            port_ = port;
            status_ = "Starting";
            lines_.clear();
        }
        running_.store(true);
        worker_ = std::thread([this] {
            run();
        });
        requestChatUpdate();
    }

    void stop() {
        running_.store(false);
        if (worker_.joinable()) {
            worker_.join();
        }
        setStatus("Stopped");
    }

    void send(std::string text) {
        text = trimLine(std::move(text));
        if (text.empty()) {
            return;
        }
        {
            std::lock_guard<std::mutex> lock(outgoingMutex_);
            outgoing_.push_back(text);
        }
        requestChatUpdate();
    }

private:
    void run() {
        Protocol protocol;
        Role role;
        std::string host;
        int port = 0;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            protocol = protocol_;
            role = role_;
            host = host_;
            port = port_;
        }

        if (protocol == Protocol::Tcp && role == Role::Server) {
            runTcpServer(port);
        } else if (protocol == Protocol::Tcp && role == Role::Client) {
            runTcpClient(host, port);
        } else if (protocol == Protocol::Udp && role == Role::Server) {
            runUdpServer(port);
        } else {
            runUdpClient(host, port);
        }
        running_.store(false);
        requestChatUpdate();
    }

    void runTcpServer(int port) {
        SocketHandle listenSocket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        setListenSocket(listenSocket);
        if (listenSocket == kInvalidSocket) {
            setStatus("TCP listen socket failed");
            return;
        }
        setNonBlocking(listenSocket);

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        address.sin_port = htons(static_cast<unsigned short>(port));
        int yes = 1;
        setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof(yes));
        if (::bind(listenSocket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 ||
            ::listen(listenSocket, 1) != 0) {
            closeSocket(listenSocket);
            setListenSocket(kInvalidSocket);
            setStatus("TCP bind/listen failed");
            return;
        }

        setStatus("TCP server listening on " + toText(port));
        SocketHandle client = kInvalidSocket;
        std::string rxBuffer;
        while (running_.load()) {
            if (client == kInvalidSocket) {
                client = ::accept(listenSocket, nullptr, nullptr);
                if (client != kInvalidSocket) {
                    setClientSocket(client);
                    setNonBlocking(client);
                    setStatus("TCP client connected");
                    addLine("SYS", "Client connected", kGreen);
                } else if (!wouldBlock()) {
                    setStatus("TCP accept failed");
                }
            } else {
                if (!pumpTcpSocket(client, rxBuffer)) {
                    closeSocket(client);
                    client = kInvalidSocket;
                    setClientSocket(kInvalidSocket);
                    rxBuffer.clear();
                    setStatus("TCP peer closed");
                } else {
                    flushTcpOutgoing(client);
                }
            }
            sleepBriefly();
        }
        closeSocket(client);
        setClientSocket(kInvalidSocket);
        closeSocket(listenSocket);
        setListenSocket(kInvalidSocket);
    }

    void runTcpClient(const std::string& host, int port) {
        SocketHandle socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        setClientSocket(socket);
        if (socket == kInvalidSocket) {
            setStatus("TCP socket failed");
            return;
        }
        setNonBlocking(socket);

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(static_cast<unsigned short>(port));
        if (inet_pton(AF_INET, host.c_str(), &address.sin_addr) != 1) {
            setStatus("Invalid host");
            closeSocket(socket);
            setClientSocket(kInvalidSocket);
            return;
        }

        setStatus("Connecting " + host + ":" + toText(port));
        if (::connect(socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 &&
            (!connectInProgress() || !waitForConnect(socket, 2200))) {
            setStatus("TCP connect failed");
            closeSocket(socket);
            setClientSocket(kInvalidSocket);
            return;
        }
        setStatus("TCP connected");
        addLine("SYS", "Connected to server", kGreen);

        std::string rxBuffer;
        while (running_.load()) {
            if (!pumpTcpSocket(socket, rxBuffer)) {
                setStatus("TCP peer closed");
                break;
            }
            flushTcpOutgoing(socket);
            sleepBriefly();
        }
        closeSocket(socket);
        setClientSocket(kInvalidSocket);
    }

    void runUdpServer(int port) {
        SocketHandle socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        setClientSocket(socket);
        if (socket == kInvalidSocket) {
            setStatus("UDP socket failed");
            return;
        }
        setNonBlocking(socket);

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_ANY);
        address.sin_port = htons(static_cast<unsigned short>(port));
        if (::bind(socket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
            setStatus("UDP bind failed");
            closeSocket(socket);
            setClientSocket(kInvalidSocket);
            return;
        }

        setStatus("UDP server listening on " + toText(port));
        sockaddr_in peer{};
        SocketLength peerSize = sizeof(peer);
        bool hasPeer = false;
        while (running_.load()) {
            char buffer[1024]{};
            sockaddr_in from{};
            SocketLength fromSize = sizeof(from);
            const int count = recvfrom(socket, buffer, sizeof(buffer) - 1, 0, reinterpret_cast<sockaddr*>(&from), &fromSize);
            if (count > 0) {
                peer = from;
                peerSize = fromSize;
                hasPeer = true;
                addLine("RX", std::string(buffer, static_cast<std::size_t>(count)), kBlue);
            }
            if (hasPeer) {
                flushUdpOutgoing(socket, peer, peerSize);
            } else {
                discardOutgoingUntilPeer();
            }
            sleepBriefly();
        }
        closeSocket(socket);
        setClientSocket(kInvalidSocket);
    }

    void runUdpClient(const std::string& host, int port) {
        SocketHandle socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        setClientSocket(socket);
        if (socket == kInvalidSocket) {
            setStatus("UDP socket failed");
            return;
        }
        setNonBlocking(socket);

        sockaddr_in peer{};
        peer.sin_family = AF_INET;
        peer.sin_port = htons(static_cast<unsigned short>(port));
        if (inet_pton(AF_INET, host.c_str(), &peer.sin_addr) != 1) {
            setStatus("Invalid host");
            closeSocket(socket);
            setClientSocket(kInvalidSocket);
            return;
        }
        setStatus("UDP ready -> " + host + ":" + toText(port));

        while (running_.load()) {
            char buffer[1024]{};
            const int count = recv(socket, buffer, sizeof(buffer) - 1, 0);
            if (count > 0) {
                addLine("RX", std::string(buffer, static_cast<std::size_t>(count)), kBlue);
            }
            flushUdpOutgoing(socket, peer, sizeof(peer));
            sleepBriefly();
        }
        closeSocket(socket);
        setClientSocket(kInvalidSocket);
    }

    bool pumpTcpSocket(SocketHandle socket, std::string& rxBuffer) {
        char buffer[512]{};
        const int count = recv(socket, buffer, sizeof(buffer), 0);
        if (count > 0) {
            rxBuffer.append(buffer, static_cast<std::size_t>(count));
            std::size_t newline = std::string::npos;
            while ((newline = rxBuffer.find('\n')) != std::string::npos) {
                addLine("RX", trimLine(rxBuffer.substr(0, newline)), kBlue);
                rxBuffer.erase(0, newline + 1);
            }
            return true;
        } else if (count == 0) {
            addLine("SYS", "Peer closed", kRed);
            return false;
        }
        return wouldBlock();
    }

    void flushTcpOutgoing(SocketHandle socket) {
        for (std::string message = popOutgoing(); !message.empty(); message = popOutgoing()) {
            const std::string wire = message + "\n";
            const int sent = ::send(socket, wire.data(), static_cast<int>(wire.size()), 0);
            if (sent > 0) {
                addLine("TX", message, kGreen);
            } else {
                addLine("ERR", "Send failed", kRed);
            }
        }
    }

    void flushUdpOutgoing(SocketHandle socket, const sockaddr_in& peer, SocketLength peerSize) {
        for (std::string message = popOutgoing(); !message.empty(); message = popOutgoing()) {
            const int sent = sendto(socket,
                                    message.data(),
                                    static_cast<int>(message.size()),
                                    0,
                                    reinterpret_cast<const sockaddr*>(&peer),
                                    peerSize);
            if (sent > 0) {
                addLine("TX", message, kGreen);
            } else {
                addLine("ERR", "Send failed", kRed);
            }
        }
    }

    void discardOutgoingUntilPeer() {
        for (std::string message = popOutgoing(); !message.empty(); message = popOutgoing()) {
            addLine("ERR", "UDP server has no peer yet: " + message, kRed);
        }
    }

    std::string popOutgoing() {
        std::lock_guard<std::mutex> lock(outgoingMutex_);
        if (outgoing_.empty()) {
            return {};
        }
        std::string value = std::move(outgoing_.front());
        outgoing_.pop_front();
        return value;
    }

    void addLine(std::string tag, std::string text, eui::Color color) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            lines_.push_back({std::move(tag), std::move(text), color});
            while (lines_.size() > 80u) {
                lines_.pop_front();
            }
        }
        requestChatUpdate();
    }

    void setStatus(std::string status) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            status_ = std::move(status);
        }
        requestChatUpdate();
    }

    void setListenSocket(SocketHandle socket) {
        std::lock_guard<std::mutex> lock(socketMutex_);
        listenSocket_ = socket;
    }

    void setClientSocket(SocketHandle socket) {
        std::lock_guard<std::mutex> lock(socketMutex_);
        clientSocket_ = socket;
    }

    static void sleepBriefly() {
        std::this_thread::sleep_for(std::chrono::milliseconds(12));
    }

    mutable std::mutex mutex_;
    std::mutex outgoingMutex_;
    std::mutex socketMutex_;
    std::thread worker_;
    std::atomic<bool> running_{false};
    Protocol protocol_ = Protocol::Tcp;
    Role role_ = Role::Server;
    std::string host_ = "127.0.0.1";
    int port_ = 39001;
    std::string status_ = "Stopped";
    std::deque<ChatLine> lines_;
    std::deque<std::string> outgoing_;
    SocketHandle listenSocket_ = kInvalidSocket;
    SocketHandle clientSocket_ = kInvalidSocket;
};

struct ChatState {
    int protocol = 0;
    int role = 0;
    std::string host = "127.0.0.1";
    std::string port = "39001";
    std::string message = "hello from EUI-NEO";
    float scroll = 0.0f;
    ChatEndpoint endpoint;
};

ChatState mainState;
ChatState serverState;
ChatState clientState;

Protocol selectedProtocol(const ChatState& state) {
    return state.protocol == 0 ? Protocol::Tcp : Protocol::Udp;
}

Role selectedRole(const ChatState& state) {
    return state.role == 0 ? Role::Server : Role::Client;
}

void startEndpoint(ChatState& state) {
    state.endpoint.start(selectedProtocol(state), selectedRole(state), state.host, toPort(state.port, 39001));
}

void stopEndpoint(ChatState& state) {
    state.endpoint.stop();
}

void sendMessage(ChatState& state) {
    state.endpoint.send(state.message);
}

void openServerWindow();
void openClientWindow();
void openPairWindows();

void textBlock(eui::Ui& ui,
               const std::string& id,
               float width,
               float height,
               const std::string& text,
               float font,
               eui::Color color,
               eui::HorizontalAlign align = eui::HorizontalAlign::Left) {
    ui.text(id)
        .size(width, height)
        .text(text)
        .fontSize(font)
        .lineHeight(font + 4.0f)
        .color(color)
        .horizontalAlign(align)
        .verticalAlign(eui::VerticalAlign::Center)
        .build();
}

void button(eui::Ui& ui,
            const std::string& id,
            float width,
            const std::string& text,
            bool primary,
            std::function<void()> onClick) {
    ui.stack(id + ".wrap")
        .size(width, 38.0f)
        .content([&] {
            components::button(ui, id)
                .theme(themeTokens(), primary)
                .size(width, 38.0f)
                .text(text)
                .fontSize(14.0f)
                .radius(10.0f)
                .transition(0.12f)
                .onClick(std::move(onClick))
                .build();
        })
        .build();
}

void composeMessages(eui::Ui& ui, ChatState& state, float width, float height) {
    const std::vector<ChatLine> lines = state.endpoint.lines();
    const float rowH = 34.0f;
    const float contentH = std::max(height, 16.0f + static_cast<float>(lines.size()) * rowH);
    const int textLimit = std::max(18, static_cast<int>((width - 92.0f) / 8.0f));

    components::scrollView(ui, "messages." + std::to_string(reinterpret_cast<std::uintptr_t>(&state)))
        .size(width, height)
        .offset(state.scroll)
        .contentKey(std::to_string(lines.size()) + "." + state.endpoint.status())
        .theme(themeTokens())
        .onChange([&state](float value) {
            state.scroll = value;
        })
        .content([&](eui::Ui& contentUi, float contentW, float) {
            contentUi.stack("message.area")
                .size(contentW, contentH)
                .content([&] {
                    if (lines.empty()) {
                        contentUi.text("empty")
                            .position(0.0f, 20.0f)
                            .size(contentW, 26.0f)
                            .text("No messages yet.")
                            .fontSize(14.0f)
                            .lineHeight(18.0f)
                            .fontWeight(680)
                            .color(mutedTextColor())
                            .horizontalAlign(eui::HorizontalAlign::Center)
                            .verticalAlign(eui::VerticalAlign::Center)
                            .build();
                    }
                    for (std::size_t i = 0; i < lines.size(); ++i) {
                        const ChatLine& line = lines[i];
                        const float rowY = 8.0f + static_cast<float>(i) * rowH;
                        contentUi.rect("row." + std::to_string(i))
                            .x(0.0f)
                            .y(rowY)
                            .size(contentW, 28.0f)
                            .color(i % 2u == 0u ? surfaceAltColor(0.48f) : surfaceAltColor(0.24f))
                            .radius(8.0f)
                            .build();
                        contentUi.text("tag." + std::to_string(i))
                            .position(10.0f, rowY)
                            .size(56.0f, 28.0f)
                            .text(line.tag)
                            .fontSize(12.0f)
                            .lineHeight(16.0f)
                            .fontWeight(760)
                            .color(line.color)
                            .verticalAlign(eui::VerticalAlign::Center)
                            .build();
                        contentUi.text("text." + std::to_string(i))
                            .position(70.0f, rowY)
                            .size(contentW - 82.0f, 28.0f)
                            .text(shortText(line.text, textLimit))
                            .fontSize(13.0f)
                            .lineHeight(17.0f)
                            .fontWeight(620)
                            .color(textColor())
                            .verticalAlign(eui::VerticalAlign::Center)
                            .build();
                    }
                })
                .build();
        })
        .build();
}

void composeEndpoint(eui::Ui& ui, const eui::Screen& screen, ChatState& state, const std::string& idPrefix) {
    const float margin = std::clamp(screen.width * 0.035f, 18.0f, 32.0f);
    const float width = std::max(360.0f, screen.width - margin * 2.0f);
    const float x = (screen.width - width) * 0.5f;
    const float top = 22.0f;
    const float gap = 12.0f;
    const float contentH = std::max(360.0f, screen.height - top * 2.0f);
    const float cardInset = 16.0f;
    const float fieldH = 38.0f;
    const float settingsH = 112.0f;
    const float actionH = 42.0f;
    const float titleH = 34.0f;
    const float subtitleH = 24.0f;
    const float logH = std::max(170.0f, contentH - titleH - subtitleH - settingsH - actionH - gap * 4.0f);
    const float settingsContentW = std::max(0.0f, width - cardInset * 2.0f);
    const float fieldW = std::max(0.0f, (settingsContentW - gap) * 0.5f);
    const float logContentW = std::max(0.0f, width - cardInset * 2.0f);
    const float actionButtonW = std::clamp((width - gap * 3.0f) * 0.18f, 64.0f, 82.0f);
    const float actionInputW = std::max(0.0f, width - actionButtonW * 3.0f - gap * 3.0f);

    ui.stack(idPrefix + ".root")
        .size(screen.width, screen.height)
        .content([&] {
            ui.rect(idPrefix + ".bg")
                .size(screen.width, screen.height)
                .color(backgroundColor())
                .build();

            ui.column(idPrefix + ".content")
                .position(x, top)
                .size(width, contentH)
                .gap(gap)
                .content([&] {
                    textBlock(ui, idPrefix + ".title", width, titleH, state.role == 0 ? "Chat Server" : "Chat Client", 27.0f, textColor());
                    textBlock(ui, idPrefix + ".subtitle", width, subtitleH, state.endpoint.status(), 14.0f, state.endpoint.running() ? kGreen : mutedTextColor());

                    components::card(ui, idPrefix + ".settings")
                        .width(width)
                        .height(settingsH)
                        .padding(cardInset)
                        .color(surfaceColor())
                        .radius(14.0f)
                        .border(1.0f, borderColor())
                        .shadow({true, {0.0f, 5.0f}, 18.0f, 0.0f, shadowColor()})
                        .content([&] {
                            ui.column(idPrefix + ".settings.layout")
                                .size(settingsContentW, settingsH - cardInset * 2.0f)
                                .gap(10.0f)
                                .content([&] {
                                    ui.row(idPrefix + ".mode.row")
                                        .size(settingsContentW, fieldH)
                                        .gap(gap)
                                        .content([&] {
                                            components::segmented(ui, idPrefix + ".protocol")
                                                .theme(themeTokens())
                                                .size(fieldW, fieldH)
                                                .items({"TCP", "UDP"})
                                                .selected(state.protocol)
                                                .onChange([&state](int value) { state.protocol = value; })
                                                .build();
                                            components::segmented(ui, idPrefix + ".role")
                                                .theme(themeTokens())
                                                .size(fieldW, fieldH)
                                                .items({"Server", "Client"})
                                                .selected(state.role)
                                                .onChange([&state](int value) { state.role = value; })
                                                .build();
                                        })
                                        .build();

                                    ui.row(idPrefix + ".address.row")
                                        .size(settingsContentW, fieldH)
                                        .gap(gap)
                                        .content([&] {
                                            components::input(ui, idPrefix + ".host")
                                                .theme(themeTokens())
                                                .size(fieldW, fieldH)
                                                .value(state.host)
                                                .placeholder("127.0.0.1")
                                                .onChange([&state](const std::string& value) { state.host = value; })
                                                .build();
                                            components::input(ui, idPrefix + ".port")
                                                .theme(themeTokens())
                                                .size(fieldW, fieldH)
                                                .value(state.port)
                                                .placeholder("39001")
                                                .onChange([&state](const std::string& value) { state.port = value; })
                                                .onEnter([&state] { startEndpoint(state); })
                                                .build();
                                        })
                                        .build();
                                })
                                .build();
                        })
                        .build();

                    components::card(ui, idPrefix + ".log")
                        .width(width)
                        .height(logH)
                        .padding(cardInset)
                        .color(surfaceColor())
                        .radius(14.0f)
                        .border(1.0f, borderColor())
                        .shadow({true, {0.0f, 5.0f}, 18.0f, 0.0f, shadowColor()})
                        .content([&] {
                            composeMessages(ui, state, logContentW, logH - cardInset * 2.0f);
                        })
                        .build();

                    ui.row(idPrefix + ".actions")
                        .size(width, actionH)
                        .gap(gap)
                        .content([&] {
                            components::input(ui, idPrefix + ".message")
                                .theme(themeTokens())
                                .size(actionInputW, actionH)
                                .value(state.message)
                                .placeholder("Message")
                                .onChange([&state](const std::string& value) { state.message = value; })
                                .onEnter([&state] { sendMessage(state); })
                                .build();
                            button(ui, idPrefix + ".start", actionButtonW, "Start", true, [&state] { startEndpoint(state); });
                            button(ui, idPrefix + ".send", actionButtonW, "Send", false, [&state] { sendMessage(state); });
                            button(ui, idPrefix + ".stop", actionButtonW, "Stop", false, [&state] { stopEndpoint(state); });
                        })
                        .build();
                })
                .build();
        })
        .build();
}

void composeSettingsDialog(eui::Ui& ui, const eui::Screen& screen, const std::string& owner) {
    const bool open = settings.dialogOwner == owner;
    const float panelW = std::min(420.0f, std::max(0.0f, screen.width - 48.0f));
    const float panelH = 260.0f;
    const float inset = 20.0f;
    const float contentW = std::max(0.0f, panelW - inset * 2.0f);

    components::dialog(ui, owner + ".settings.dialog")
        .theme(themeTokens())
        .screen(screen.width, screen.height)
        .size(panelW, panelH)
        .open(open)
        .zIndex(5000)
        .onOpenChange([](bool open) {
            if (!open) {
                settings.dialogOwner.clear();
            }
        })
        .content([&] {
            ui.column(owner + ".settings.dialog.content")
                .position(inset, inset)
                .size(contentW, panelH - inset * 2.0f)
                .gap(14.0f)
                .content([&] {
                    textBlock(ui, owner + ".settings.title", contentW, 32.0f, "Settings", 24.0f, textColor());
                    textBlock(ui, owner + ".settings.note", contentW, 24.0f, "Theme changes apply to every chat window.", 14.0f, mutedTextColor());

                    components::toggleSwitch(ui, owner + ".settings.dark")
                        .theme(themeTokens())
                        .size(contentW, 34.0f)
                        .trackSize(42.0f, 24.0f)
                        .checked(settings.dark)
                        .text("Night mode")
                        .onChange([](bool value) {
                            settings.dark = value;
                            requestChatUpdate();
                        })
                        .build();

                    ui.row(owner + ".settings.accent.row")
                        .size(contentW, 42.0f)
                        .gap(12.0f)
                        .content([&] {
                            textBlock(ui, owner + ".settings.accent.label", 86.0f, 38.0f, "Theme", 15.0f, textColor());
                            components::segmented(ui, owner + ".settings.accent")
                                .theme(themeTokens())
                                .size(std::max(0.0f, contentW - 98.0f), 38.0f)
                                .items({"Blue", "Purple", "Green", "Amber"})
                                .selected(settings.accent)
                                .fontSize(13.0f)
                                .onChange([](int value) {
                                    settings.accent = value;
                                    requestChatUpdate();
                                })
                                .build();
                        })
                        .build();

                    ui.row(owner + ".settings.actions")
                        .size(contentW, 38.0f)
                        .justifyContent(eui::Align::END)
                        .content([&] {
                            button(ui, owner + ".settings.close", 96.0f, "Done", true, [] {
                                settings.dialogOwner.clear();
                            });
                        })
                        .build();
                })
                .build();
        })
        .build();
}

void openServerWindow() {
    serverState.role = 0;
    app::openWindow(
        app::DslWindowConfig{}.title("Chat Server").pageId("chat_server").windowSize(660, 540).clearColor(backgroundColor()),
        [](eui::Ui& childUi, const eui::Screen& childScreen) {
            composeEndpoint(childUi, childScreen, serverState, "server");
        });
}

void openClientWindow() {
    clientState.role = 1;
    app::openWindow(
        app::DslWindowConfig{}.title("Chat Client").pageId("chat_client").windowSize(660, 540).clearColor(backgroundColor()),
        [](eui::Ui& childUi, const eui::Screen& childScreen) {
            composeEndpoint(childUi, childScreen, clientState, "client");
        });
}

void openPairWindows() {
    openServerWindow();
    openClientWindow();
}

void composeLauncher(eui::Ui& ui, const eui::Screen& screen) {
    const float margin = std::clamp(screen.width * 0.05f, 24.0f, 42.0f);
    const float width = std::min(620.0f, std::max(360.0f, screen.width - margin * 2.0f));
    const float x = (screen.width - width) * 0.5f;
    const float top = std::max(28.0f, (screen.height - 390.0f) * 0.5f);
    const float gap = 14.0f;
    const float cardInset = 18.0f;
    const float cardW = std::max(0.0f, width - cardInset * 2.0f);

    ui.stack("launcher.root")
        .size(screen.width, screen.height)
        .content([&] {
            ui.rect("launcher.bg")
                .size(screen.width, screen.height)
                .color(backgroundColor())
                .build();

            ui.column("launcher.content")
                .position(x, top)
                .size(width, 390.0f)
                .gap(gap)
                .content([&] {
                    textBlock(ui, "launcher.title", width, 42.0f, "EUI-NEO Chat Lab", 31.0f, textColor());
                    textBlock(ui, "launcher.subtitle", width, 28.0f, "Open server and client windows to test TCP or UDP messaging.", 15.0f, mutedTextColor());

                    components::card(ui, "launcher.panel")
                        .width(width)
                        .height(220.0f)
                        .padding(cardInset)
                        .color(surfaceColor())
                        .radius(18.0f)
                        .border(1.0f, borderColor())
                        .shadow({true, {0.0f, 8.0f}, 24.0f, 0.0f, shadowColor()})
                        .content([&] {
                            ui.column("launcher.panel.content")
                                .size(cardW, 220.0f - cardInset * 2.0f)
                                .gap(14.0f)
                                .content([&] {
                                    ui.row("launcher.actions.primary")
                                        .size(cardW, 44.0f)
                                        .gap(12.0f)
                                        .content([&] {
                                            button(ui, "launcher.server", (cardW - 12.0f) * 0.5f, "Open Server", true, [] { openServerWindow(); });
                                            button(ui, "launcher.client", (cardW - 12.0f) * 0.5f, "Open Client", false, [] { openClientWindow(); });
                                        })
                                        .build();

                                    button(ui, "launcher.pair", cardW, "Open Server + Client", true, [] { openPairWindows(); });

                                    ui.row("launcher.actions.secondary")
                                        .size(cardW, 44.0f)
                                        .gap(12.0f)
                                        .content([&] {
                                            button(ui, "launcher.settings", (cardW - 12.0f) * 0.5f, "Settings", false, [] {
                                                settings.dialogOwner = "launcher";
                                            });
                                            button(ui, "launcher.stop", (cardW - 12.0f) * 0.5f, "Stop All", false, [] {
                                                stopEndpoint(serverState);
                                                stopEndpoint(clientState);
                                            });
                                        })
                                        .build();

                                    textBlock(ui, "launcher.status", cardW, 24.0f,
                                              std::string("Server: ") + serverState.endpoint.status() + "  /  Client: " + clientState.endpoint.status(),
                                              13.0f,
                                              mutedTextColor());
                                })
                                .build();
                        })
                        .build();
                })
                .build();

            composeSettingsDialog(ui, screen, "launcher");
        })
        .build();
}

} // namespace

const DslAppConfig& dslAppConfig() {
    static const DslAppConfig config = DslAppConfig{}
        .title("Chat")
        .pageId("chat")
        .clearColor(backgroundColor())
        .windowSize(700, 560)
        .fps(90.0);
    return config;
}

void compose(eui::Ui& ui, const eui::Screen& screen) {
    composeLauncher(ui, screen);
}

} // namespace app
