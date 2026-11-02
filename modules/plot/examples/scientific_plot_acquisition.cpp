#include "eui_neo.h"
#include "modules/plot/plot.h"
#include "modules/plot/session.h"
#include "modules/plot/waveform.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <thread>

namespace app {
namespace {
using namespace modules::plot;
struct Frame {
    WaveformBuffer::Snapshot snapshot;
    std::string error;
};
struct Application {
    PlotSession session;
    std::shared_ptr<Plot> waveform = session.make<Plot>();
    std::shared_ptr<HeatmapPlot> heatmap = session.make<HeatmapPlot>();
    std::shared_ptr<LatestValue<Frame>> incoming = session.mailbox<Frame>();
    std::shared_ptr<WaveformBuffer> buffer = std::make_shared<WaveformBuffer>(
        WaveformBuffer::Config::forDuration(2, 200000, 1, SampleFormat::Int16));
    std::atomic<bool> stopping{false}, acquiring{true};
    std::thread worker;
    bool follow = true;
    std::uint64_t latestSample = 0;
    std::string error;

    Application() {
        Axes axes;
        axes.x.setRange({0, .01});
        axes.x.label = "Time";
        axes.x.unit = "s";
        axes.y.setRange({-32768, 32767});
        axes.y.label = "ADC (Int16)";
        waveform->setAxes(axes);
        waveform->setTitle("Two-channel acquisition | Drag / wheel / Shift+drag");
        std::vector<double> cells(64 * 64);
        for (int row = 0; row < 64; ++row)
            for (int col = 0; col < 64; ++col)
                cells[row * 64 + col] = std::sin(col * .13) * std::cos(row * .1);
        heatmap->setField(ScalarField(64, 64, std::move(cells), {-4, 4}, {-4, 4}));
        heatmap->setTitle("Heatmap | Drag / wheel / right-click reset");
        worker = std::thread([this] { acquire(); });
    }
    ~Application() { stop(); }
    void stop() {
        stopping = true;
        if (worker.joinable())
            worker.join();
    }
    void acquire() {
        try {
            WaveformInput input(buffer);
            // Replace this simulated device packet with a read from your device.
            // Packet size intentionally differs from the history block size.
            std::vector<std::int16_t> packet(640 * 2);
            std::uint64_t sample = 0;
            auto next = std::chrono::steady_clock::now(), publishAt = next;
            while (!stopping) {
                if (!acquiring) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    next = std::chrono::steady_clock::now();
                    continue;
                }
                for (std::size_t i = 0; i < 640; ++i) {
                    const auto t = double(sample + i) / buffer->config().sampleRate;
                    packet[i * 2] = std::int16_t(24000 * std::sin(t * 6.28318530718 * 500));
                    packet[i * 2 + 1] = std::int16_t(18000 * std::cos(t * 6.28318530718 * 900));
                }
                const auto* bytes = reinterpret_cast<const std::uint8_t*>(packet.data());
                const auto count = packet.size() * sizeof(packet[0]);
                std::size_t accepted = 0;
                while (accepted < count && !stopping) {
                    const auto n = input.write(bytes + accepted, count - accepted);
                    accepted += n;
                    if (!n) {
                        // A slow UI may still pin an old history snapshot. Publish the current
                        // accepted history even under backpressure so the UI can release it.
                        if (!incoming->publish({buffer->snapshot(), {}}))
                            return;
                        std::this_thread::sleep_for(std::chrono::milliseconds(2));
                    }
                }
                sample += 640;
                if (std::chrono::steady_clock::now() >= publishAt) {
                    if (!incoming->publish({buffer->snapshot(), {}}))
                        break;
                    publishAt = std::chrono::steady_clock::now() + std::chrono::milliseconds(16);
                }
                next += std::chrono::microseconds(3200);
                std::this_thread::sleep_until(next);
            }
        } catch (const std::exception& e) {
            incoming->publish({{}, e.what()});
        }
    }
    void update() {
        // The timer invokes this on the UI thread; no queue grows with acquisition rate.
        if (auto frame = incoming->take()) {
            if (!frame->error.empty()) {
                error = frame->error;
                return;
            }
            latestSample = frame->snapshot.nextSample;
            std::vector<Series> series(2);
            for (int ch = 0; ch < 2; ++ch) {
                series[ch].data = frame->snapshot.channels[ch];
                series[ch].name = ch ? "CH2" : "CH1";
            }
            series[0].style.color = {.2f, .8f, 1, 1};
            series[1].style.color = {1, .65f, .2f, 1};
            waveform->setSeries(std::move(series));
            if (follow) {
                auto axes = waveform->axes();
                const auto span = axes.x.range().max - axes.x.range().min;
                const double end = double(latestSample) / buffer->config().sampleRate;
                axes.x.setRange({end - span, end});
                waveform->setAxes(axes);
            }
        }
    }
};
Application& model() {
    static Application value;
    return value;
}
} // namespace
const DslAppConfig& dslAppConfig() {
    static const auto config = DslAppConfig{}
                                   .title("Scientific acquisition starter")
                                   .windowSize(1200, 800)
                                   .minWindowSize(800, 600)
                                   .fps(60)
                                   .onShutdown(model().session.shutdownHandler([] { model().stop(); }));
    return config;
}
void compose(eui::Ui& ui, const eui::Screen& screen) {
    auto& m = model();
    ui.stack("page")
        .size(screen.width, screen.height)
        .onTimer(1.f / 60, [&m] { m.update(); })
        .content([&] {
            components::button(ui, "pause")
                .position(12, 10)
                .size(170, 36)
                .text(m.acquiring ? "Pause + inspect" : "Resume + follow")
                .onClick([&m] {
                    const bool running = !m.acquiring.load();
                    m.acquiring = running;
                    m.follow = running;
                })
                .build();
            components::button(ui, "reset")
                .position(194, 10)
                .size(120, 36)
                .text("Reset view")
                .onClick([&m] {
                    auto a = m.waveform->axes();
                    const double end = double(m.latestSample) / 200000;
                    a.x.setRange({end - .01, end});
                    a.y.setRange({-32768, 32767});
                    m.waveform->setAxes(a);
                    m.heatmap->resetView();
                })
                .build();
            ui.text("status")
                .position(326, 18)
                .size(screen.width - 342, 22)
                .fontSize(13)
                .text(m.error.empty() ? "Int16 | 200 kS/s/channel | 1 s history | pool " +
                                            std::to_string(m.buffer->reservedBytes() / 1024) + " KiB"
                                      : m.error)
                .build();
            const float w = screen.width - 24, h = (screen.height - 112) / 2;
            ui.stack("wave.slot")
                .position(12, 58)
                .size(w, h)
                .content([&] { m.waveform->compose(ui, "wave", w, h, 2); })
                .build();
            ui.stack("heat.slot")
                .position(12, 68 + h)
                .size(w, h)
                .content([&] { m.heatmap->compose(ui, "heat", w, h, 2); })
                .build();
            const auto p = m.heatmap->probe();
            ui.text("probe")
                .position(16, screen.height - 30)
                .size(w, 24)
                .fontSize(13)
                .text(
                    p ? "Original field sample: row " + std::to_string(p->row) + ", column " +
                            std::to_string(p->column) + ", value " + std::to_string(p->value)
                      : "Hover heatmap to inspect original cells; pause acquisition to inspect the waveform.")
                .build();
        })
        .build();
}
} // namespace app
