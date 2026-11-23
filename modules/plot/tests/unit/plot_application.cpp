#include "modules/plot/plot.h"
#include "modules/plot/session.h"
#include "modules/plot/waveform.h"
#include <atomic>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
#include <thread>

using namespace modules::plot;
namespace {
void require(bool v, const char* message) {
    if (!v)
        throw std::runtime_error(message);
}
template <class E, class F> void throws(F f) {
    try {
        f();
    } catch (const E&) {
        return;
    }
    throw std::runtime_error("missing exception");
}
struct Resource {
    int releases = 0;
    void releaseGpu() { ++releases; }
};
template <class T> void packets(SampleFormat format) {
    auto config = WaveformBuffer::Config::forDuration(2, 1000, 3, format);
    auto buffer = std::make_shared<WaveformBuffer>(config);
    WaveformInput input(buffer);
    std::vector<T> original(2048 * 2);
    for (std::size_t i = 0; i < original.size(); ++i)
        original[i] = T((i * 71) % 60000 - 30000);
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(original.data());
    const auto size = original.size() * sizeof(T);
    std::size_t accepted = 0;
    while (accepted < size) {
        const auto n = std::min(std::size_t(137), size - accepted);
        require(input.write(bytes + accepted, n) == n, "split packet lost bytes");
        accepted += n;
    }
    require(input.flush() && input.pendingBytes() == 0, "complete stream left partial data");
    auto snap = buffer->snapshot();
    require(snap.firstSample == 0 && snap.nextSample == 2048, "packet sequence mismatch");
    for (int ch = 0; ch < 2; ++ch) {
        for (std::size_t i = 0; i < 2048; ++i)
            require(snap.channels[ch].at(i).y == original[i * 2 + ch], "typed sample corruption");
        for (std::size_t a = 0; a < 1900; a += 37) {
            const auto end = std::min(std::size_t(2048), a + 437);
            std::size_t low = a, high = a;
            for (auto i = a; i < end; ++i) {
                if (original[i * 2 + ch] < original[low * 2 + ch])
                    low = i;
                if (original[i * 2 + ch] > original[high * 2 + ch])
                    high = i;
            }
            const auto s = snap.channels[ch].summary(a, end);
            require(s.valid == end - a && s.minYIndex == low && s.maxYIndex == high,
                    "typed extrema mismatch");
        }
    }
    require(input.write(bytes, 1) == 1 && !input.flush() && input.pendingBytes() == 1,
            "partial packet padded");
}
} // namespace
int main() {
    try {
        require(WaveformBuffer::requiredBytes({}) < 1024 * 1024, "default pool must remain small");
        auto fast = WaveformBuffer::Config::dual125MS();
        require(fast.sampleRate == 125000000 && fast.retainedSamples == 625000000 &&
                    fast.blockSamples == 65536,
                "fast preset changed");
        require(WaveformBuffer::requiredBytes(fast) < fast.budgetBytes, "preset budget");
        throws<std::length_error>([] { WaveformBuffer::Config::forDuration(2, 125000000, 5); });
        throws<std::invalid_argument>([] { WaveformBuffer::Config::forDuration(0, 1000, 1); });
        throws<std::invalid_argument>(
            [] { WaveformBuffer::Config::forDuration(2, 1, std::numeric_limits<double>::infinity()); });
        packets<std::uint8_t>(SampleFormat::UInt8);
        packets<std::int16_t>(SampleFormat::Int16);
        packets<std::uint16_t>(SampleFormat::UInt16);
        auto c = WaveformBuffer::Config::forDuration(1, 1000, .1);
        c.spareBlocks = 1;
        auto buffer = std::make_shared<WaveformBuffer>(c);
        WaveformInput input(buffer);
        std::vector<std::uint8_t> raw(c.blockSamples, 123);
        require(input.write(raw.data(), raw.size()) == raw.size(), "first block");
        auto held = buffer->snapshot();
        require(input.write(raw.data(), raw.size()) == raw.size(), "spare block");
        require(input.write(raw.data(), raw.size()) == 0, "full-block backpressure must consume nothing");
        require(input.write(raw.data(), 17) == 17, "adapter partial acceptance");
        require(input.write(raw.data() + 17, raw.size() - 17) == raw.size() - 17 && !input.flush(),
                "pending full block");
        held = {};
        require(input.flush() && buffer->snapshot().nextSample == raw.size() * 3, "retry pending block");

        PlotSession session;
        auto resource = session.make<Resource>();
        auto mailbox = session.mailbox<int>();
        std::atomic<bool> wrongThreadRejected{false};
        std::thread producer([&] {
            for (int i = 0; i < 10000; ++i)
                require(mailbox->publish(i), "early mailbox close");
            try {
                session.make<Resource>();
            } catch (const std::logic_error&) {
                wrongThreadRejected = true;
            }
        });
        producer.join();
        require(wrongThreadRejected, "session thread ownership");
        require(mailbox->take() == 9999 && !mailbox->take(), "latest mailbox grew into a queue");
        int stopped = 0;
        auto shutdown = session.shutdownHandler([&] {
            ++stopped;
            require(!mailbox->publish(3), "mailbox open during stop");
        });
        mailbox->publish(1);
        shutdown();
        shutdown();
        require(stopped == 1 && resource->releases == 1 && !mailbox->take() && !mailbox->publish(2),
                "shutdown order/idempotency");
        throws<std::logic_error>([&] { session.make<Resource>(); });

        ScalarField field(2, 2, {1, 2, 3, 4}, {10, 30}, {100, 140});
        ColorScale scale;
        scale.fit(field);
        Axes axes;
        axes.x.setRange({20, 30});
        axes.y.setRange({120, 140});
        auto tiles = heatmapTiles(field, scale, axes, {0, 0, 100, 100});
        require(tiles.size() == 1 && tiles[0].color == scale.map(4), "heatmap crop selected wrong cell");
        for (auto v : tiles[0].vertices)
            require(v.x >= 0 && v.x <= 100 && v.y >= 0 && v.y <= 100, "heatmap viewport clipping");
        axes.x.setReversed(true);
        axes.y.setScale(Scale::Log10);
        require(heatmapTiles(field, scale, axes, {0, 0, 100, 100}).size() == 1, "reversed/log heatmap");
        auto pick = pickField(field, {25, 130});
        require(pick && pick->row == 1 && pick->column == 1 && pick->value == 4, "original field pick");
        require(!pickField(field, {31, 130}), "field outside pick");
        ScalarField upper(2, 2, {1, 2, 3, 4}, {10, 30}, {100, 140}, FieldOrigin::UpperLeft);
        require(pickField(upper, {25, 130})->value == 2, "upper-left origin pick");
        ScalarField vertices(2, 2, {1, 2, 3, 4}, {10, 30}, {100, 140}, FieldOrigin::LowerLeft,
                             FieldSampling::GridPoints);
        require(pickField(vertices, {29, 139})->value == 4,
                "grid pick must return original vertex, not mean");
        HeatmapPlot plot;
        plot.setField(field);
        plot.zoom({.5, .5}, .5);
        require(plot.axes().x.range().min == 15 && plot.axes().x.range().max == 25, "heatmap zoom");
        plot.pan(.5, 0);
        require(plot.axes().x.range().min == 20, "heatmap pan");
        plot.setField(upper);
        require(plot.axes().x.range().min == 20, "new field reset manual view");
        plot.resetView();
        require(plot.axes().x.range().min == 10 && plot.axes().x.range().max == 30, "heatmap reset");
        std::cout << "Plot application integration checks passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
