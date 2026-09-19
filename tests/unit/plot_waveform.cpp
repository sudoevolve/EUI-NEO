#include "modules/plot/waveform.h"
#include "modules/plot/geometry.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <thread>

using namespace modules::plot;
namespace {
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class E, class F> void throws(F f) {
    try {
        f();
    } catch (const E&) {
        return;
    }
    throw std::runtime_error("expected exception");
}
void verifySummary(const Data& data, std::size_t a, std::size_t b) {
    DataSummary expected;
    for (auto i = a; i < b; ++i) {
        const auto p = data.at(i);
        if (!std::isfinite(p.x) || !std::isfinite(p.y))
            continue;
        if (!expected.valid) {
            expected.min = expected.max = p;
            expected.minYIndex = expected.maxYIndex = i;
        } else {
            expected.min.x = std::min(expected.min.x, p.x);
            expected.max.x = std::max(expected.max.x, p.x);
            if (p.y < expected.min.y) {
                expected.min.y = p.y;
                expected.minYIndex = i;
            }
            if (p.y > expected.max.y) {
                expected.max.y = p.y;
                expected.maxYIndex = i;
            }
        }
        ++expected.valid;
    }
    const auto s = data.summary(a, b);
    require(s.valid == expected.valid, "summary valid count");
    if (s.valid)
        require(s.min.x == expected.min.x && s.max.x == expected.max.x && s.min.y == expected.min.y &&
                    s.max.y == expected.max.y && s.minYIndex == expected.minYIndex &&
                    s.maxYIndex == expected.maxYIndex,
                "summary extrema/index");
}
void verifyPick(const Data& data, Axes axes, std::mt19937& random) {
    const Viewport v{7, 11, 300, 200};
    PointIndex index;
    index.rebuild(data, axes, v);
    for (int trial = 0; trial < 40; ++trial) {
        Point p{double(random() % 340), double(random() % 240)};
        double radius = 5 + random() % 50;
        std::optional<Pick> expected;
        for (std::size_t i = 0; i < data.size(); ++i) {
            const auto original = data.at(i);
            const auto s = axes.toScreen(original, v);
            if (!s || s->x < v.x || s->x > v.x + v.width || s->y < v.y || s->y > v.y + v.height)
                continue;
            const auto distance = std::hypot(p.x - s->x, p.y - s->y);
            if (distance <= radius && (!expected || distance < expected->distance))
                expected = Pick{i, original, *s, distance};
        }
        auto actual = index.nearest(p, radius);
        require(bool(actual) == bool(expected), "pick presence");
        if (actual)
            require(actual->index == expected->index && actual->distance == expected->distance,
                    "raw nearest pick mismatch");
    }
}
} // namespace
int main() {
    try {
        std::mt19937 random(729);
        std::vector<double> x(12345), y(x.size());
        for (std::size_t i = 0; i < x.size(); ++i) {
            x[i] = double(random() % 300) - 100;
            y[i] = double(random() % 100) - 20;
            if (i % 97 == 0)
                y[i] = std::numeric_limits<double>::quiet_NaN();
        }
        Data data(x, y);
        for (int i = 0; i < 1200; ++i) {
            auto a = random() % (x.size() + 1), b = random() % (x.size() + 1);
            verifySummary(data, std::min(a, b), std::max(a, b));
        }
        auto edited = data.replace(4090, std::vector<double>(35, 1), std::vector<double>(35, 999));
        verifySummary(edited, 4000, 9000);
        verifySummary(data, 4000, 9000);
        verifySummary(data.append({1, 2, 3}, {-100, 1000, 0}), 0, data.size() + 3);
        Axes axes;
        axes.x.setRange({-30, 200});
        axes.y.setRange({-10, 70});
        verifyPick(data, axes, random);
        axes.x.setReversed(true);
        axes.y.setReversed(true);
        verifyPick(data, axes, random);
        axes.x.setRange({1, 200});
        axes.y.setRange({1, 70});
        axes.x.setScale(Scale::Log10);
        axes.y.setScale(Scale::Log10);
        verifyPick(data, axes, random);
        for (std::size_t i = 0; i < x.size(); ++i)
            x[i] = 1e12 + i;
        Data ordered(x, y);
        axes = Axes{};
        axes.x.setRange({1e12 + 4000, 1e12 + 5000});
        axes.y.setRange({-20, 100});
        verifyPick(ordered, axes, random);
        // Compare extrema reduction to a brute-force screen-column reference, including missing runs.
        for (bool reversed : {false, true}) {
            axes.x.setReversed(reversed);
            const Viewport v{7, 11, 300, 200};
            auto runs = selectDisplay(ordered, axes, v);
            std::vector<DisplayRun> expected;
            DisplayRun run;
            std::vector<DisplayPoint> column;
            const auto flush = [&] {
                if (column.empty())
                    return;
                auto low = column.front(), high = low;
                for (auto p : column) {
                    if (p.screen.y < low.screen.y)
                        low = p;
                    if (p.screen.y > high.screen.y)
                        high = p;
                }
                std::vector<DisplayPoint> points{column.front(), low, high, column.back()};
                std::sort(points.begin(), points.end(), [](auto a, auto b) { return a.index < b.index; });
                for (auto p : points)
                    if (run.empty() || run.back().index != p.index)
                        run.push_back(p);
                column.clear();
            };
            for (std::size_t i = 3999; i < 5002; ++i) {
                const auto p = axes.toScreen(ordered.at(i), v);
                if (!p) {
                    flush();
                    if (!run.empty()) {
                        expected.push_back(run);
                        run.clear();
                    }
                    continue;
                }
                if (!column.empty() && std::floor(column.back().screen.x - v.x) != std::floor(p->x - v.x))
                    flush();
                column.push_back({*p, i});
            }
            flush();
            if (!run.empty())
                expected.push_back(run);
            require(runs.size() == expected.size(), "missing runs changed");
            for (std::size_t r = 0; r < runs.size(); ++r) {
                require(runs[r].size() == expected[r].size(), "column size changed");
                for (std::size_t i = 0; i < runs[r].size(); ++i)
                    require(runs[r][i].index == expected[r][i].index, "column peak changed");
            }
        }
        WaveformBuffer::Config config;
        config.channels = 3;
        config.sampleRate = 1000;
        config.retainedSamples = 700;
        config.blockSamples = 256;
        config.spareBlocks = 2;
        config.budgetBytes = 100000;
        WaveformBuffer buffer(config);
        std::vector<std::uint8_t> raw(768);
        auto fill = [&](std::uint64_t first) {
            for (std::size_t i = 0; i < 256; ++i)
                for (int c = 0; c < 3; ++c)
                    raw[i * 3 + c] = std::uint8_t((first + i) * 7 + c * 31);
        };
        for (int b = 0; b < 3; ++b) {
            fill(b * 256);
            require(buffer.tryAppend(raw.data(), raw.size()), "initial append");
        }
        auto pinned = buffer.snapshot();
        require(pinned.firstSample == 0 && pinned.nextSample == 768 && pinned.channels.size() == 3,
                "atomic channels");
        for (int b = 3; b < 5; ++b) {
            fill(b * 256);
            require(buffer.tryAppend(raw.data(), raw.size()), "spare append");
        }
        require(!buffer.tryAppend(raw.data(), raw.size()), "pinned pool must backpressure");
        for (int c = 0; c < 3; ++c) {
            for (std::size_t i = 0; i < 768; ++i)
                require(pinned.channels[c].at(i).y == std::uint8_t(i * 7 + c * 31), "pinned data mutated");
            for (int trial = 0; trial < 300; ++trial) {
                auto a = random() % 769, b = random() % 769;
                verifySummary(pinned.channels[c], std::min(a, b), std::max(a, b));
            }
        }
        auto retained = buffer.snapshot();
        require(retained.firstSample == 512 && retained.nextSample == 1280, "rounded retention");
        require(retained.channels[0].at(0).x == .512, "absolute sample time");
        throws<std::logic_error>([&] { retained.channels[0].append({1}, {2}); });
        pinned = {};
        retained = {};
        fill(1280);
        require(buffer.tryAppend(raw.data(), raw.size()), "released pool reuse");
        throws<std::invalid_argument>([&] { buffer.tryAppend(raw.data(), raw.size() - 1); });
        config.budgetBytes = 1;
        throws<std::length_error>([&] { WaveformBuffer tooLarge(config); });
        config.budgetBytes = 100000;
        config.spareBlocks = 16;
        WaveformBuffer concurrent(config);
        std::atomic<bool> done{false};
        std::atomic<std::size_t> accepted{0};
        std::thread writer([&] {
            std::vector<std::uint8_t> bytes(768);
            for (int b = 0; b < 2000; ++b) {
                for (int i = 0; i < 256; ++i)
                    for (int c = 0; c < 3; ++c)
                        bytes[i * 3 + c] = std::uint8_t((b * 256 + i) * 7 + c * 31);
                while (!concurrent.tryAppend(bytes.data(), bytes.size()))
                    std::this_thread::yield();
                ++accepted;
            }
            done = true;
        });
        bool intact = true;
        do {
            auto snap = concurrent.snapshot();
            for (int c = 0; c < 3; ++c)
                for (std::size_t i = 0; i < snap.channels[c].size(); ++i)
                    intact = intact &&
                             snap.channels[c].at(i).y == std::uint8_t((snap.firstSample + i) * 7 + c * 31);
        } while (!done);
        writer.join();
        require(intact && accepted == 2000, "concurrent snapshot corruption");
        std::cout << "Waveform retention, backpressure, concurrent publication, summaries, display and exact "
                     "picking passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
