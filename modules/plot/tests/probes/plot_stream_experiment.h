#pragma once
// Bounded experimental UInt8 store used only by the throughput probe.
// Publication is immutable. One producer owns mutation; UI snapshots pin blocks in a fixed pool.
#include <array>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <mutex>
#include <random>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace stream_experiment {
using Clock = std::chrono::steady_clock;
struct Deadline {
#if defined(_WIN32)
    HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, 0x00000002, TIMER_ALL_ACCESS);
    ~Deadline() {
        if (timer)
            CloseHandle(timer);
    }
#endif
    void wait(Clock::time_point target) {
        auto remaining = target - Clock::now();
        if (remaining <= Clock::duration::zero())
            return;
#if defined(_WIN32)
        if (timer) {
            LARGE_INTEGER due;
            due.QuadPart = -std::max<long long>(
                1, std::chrono::duration_cast<std::chrono::nanoseconds>(remaining).count() / 100);
            if (SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE)) {
                WaitForSingleObject(timer, INFINITE);
                return;
            }
        }
#endif
        std::this_thread::sleep_until(target);
    }
};
constexpr std::size_t blockSamples = 65536, blockBytes = blockSamples * 2, leafSamples = 256;
struct Extremum {
    std::uint32_t lowAt = 0, highAt = 0;
    std::uint8_t low = 255, high = 0;
};
using Pair = std::array<Extremum, 2>;
inline void add(Extremum& e, std::uint8_t value, std::uint32_t index) {
    if (value < e.low || (value == e.low && index < e.lowAt)) {
        e.low = value;
        e.lowAt = index;
    }
    if (value > e.high || (value == e.high && index < e.highAt)) {
        e.high = value;
        e.highAt = index;
    }
}
inline void combine(Extremum& e, const Extremum& other) {
    add(e, other.low, other.lowAt);
    add(e, other.high, other.highAt);
}
struct Block {
    std::uint64_t first = 0;
    Clock::time_point ready;
    std::vector<std::uint8_t> raw = std::vector<std::uint8_t>(blockBytes);
    std::array<std::vector<Pair>, 3> levels{std::vector<Pair>(256), std::vector<Pair>(16),
                                            std::vector<Pair>(1)};
    std::array<std::uint64_t, 2> sums{};
    void build() {
        sums = {};
        for (std::size_t group = 0; group < 256; ++group) {
            Pair pair;
            for (int ch = 0; ch < 2; ++ch)
                pair[ch].lowAt = pair[ch].highAt = std::uint32_t(group * 256);
            for (std::size_t i = group * 256; i < (group + 1) * 256; ++i)
                for (int ch = 0; ch < 2; ++ch) {
                    const auto v = raw[i * 2 + ch];
                    sums[ch] += v;
                    add(pair[ch], v, std::uint32_t(i));
                }
            levels[0][group] = pair;
        }
        for (int level = 1; level < 3; ++level)
            for (std::size_t group = 0; group < levels[level].size(); ++group) {
                auto pair = levels[level - 1][group * 16];
                for (std::size_t i = 1; i < 16; ++i)
                    for (int ch = 0; ch < 2; ++ch)
                        combine(pair[ch], levels[level - 1][group * 16 + i][ch]);
                levels[level][group] = pair;
            }
    }
    Pair query(std::size_t begin, std::size_t end) const {
        Pair result;
        for (auto& e : result)
            e.lowAt = e.highAt = std::uint32_t(begin);
        while (begin < end) {
            int level = 2;
            std::size_t stride = blockSamples;
            while (level >= 0 && (begin % stride || stride > end - begin)) {
                --level;
                stride /= 16;
            }
            if (level >= 0) {
                for (int ch = 0; ch < 2; ++ch)
                    combine(result[ch], levels[level][begin / stride][ch]);
                begin += stride;
            } else {
                for (int ch = 0; ch < 2; ++ch)
                    add(result[ch], raw[begin * 2 + ch], std::uint32_t(begin));
                ++begin;
            }
        }
        return result;
    }
};
struct Sample {
    std::uint64_t index;
    std::uint8_t value;
};
using Snapshot = std::vector<std::shared_ptr<const Block>>;
inline std::array<std::vector<Sample>, 2> display(const Snapshot& blocks, std::uint64_t first,
                                                  std::uint64_t end, std::size_t columns) {
    std::array<std::vector<Sample>, 2> out;
    if (blocks.empty())
        return out;
    first = std::max(first, blocks.front()->first);
    end = std::min(end, blocks.back()->first + blockSamples);
    if (end <= first)
        return out;
    columns = std::min<std::uint64_t>(columns, end - first);
    const auto base = blocks.front()->first;
    auto at = [&](std::uint64_t i, int ch) {
        return blocks[(i - base) / blockSamples]->raw[((i - base) % blockSamples) * 2 + ch];
    };
    for (std::size_t column = 0; column < columns; ++column) {
        auto a = first + (end - first) * column / columns, b = first + (end - first) * (column + 1) / columns;
        std::array<Sample, 2> low{{{a, at(a, 0)}, {a, at(a, 1)}}}, high = low;
        auto part = a;
        while (part < b) {
            const auto& block = *blocks[(part - base) / blockSamples];
            const auto stop = std::min(b, block.first + blockSamples);
            const auto range = block.query(std::size_t(part - block.first), std::size_t(stop - block.first));
            for (int ch = 0; ch < 2; ++ch) {
                if (range[ch].low < low[ch].value)
                    low[ch] = {block.first + range[ch].lowAt, range[ch].low};
                if (range[ch].high > high[ch].value)
                    high[ch] = {block.first + range[ch].highAt, range[ch].high};
            }
            part = stop;
        }
        for (int ch = 0; ch < 2; ++ch) {
            std::array<Sample, 4> points{{{a, at(a, ch)}, low[ch], high[ch], {b - 1, at(b - 1, ch)}}};
            std::sort(points.begin(), points.end(), [](auto a, auto b) { return a.index < b.index; });
            for (auto p : points)
                if (out[ch].empty() || out[ch].back().index != p.index)
                    out[ch].push_back(p);
        }
    }
    return out;
}
inline void validate() {
    auto block = std::make_shared<Block>();
    std::mt19937 random(7);
    for (auto& value : block->raw)
        value = std::uint8_t(random());
    block->raw[123 * 2] = 255;
    block->raw[32769 * 2 + 1] = 0;
    block->build();
    for (int trial = 0; trial < 2000; ++trial) {
        std::size_t a = random() % blockSamples, b = a + 1 + random() % (blockSamples - a);
        auto result = block->query(a, b);
        for (int ch = 0; ch < 2; ++ch) {
            Extremum brute;
            brute.lowAt = brute.highAt = std::uint32_t(a);
            for (auto i = a; i < b; ++i)
                add(brute, block->raw[i * 2 + ch], std::uint32_t(i));
            if (result[ch].low != brute.low || result[ch].high != brute.high ||
                result[ch].lowAt != brute.lowAt || result[ch].highAt != brute.highAt)
                throw std::runtime_error("stream index disagrees with original UInt8 samples");
        }
    }
    auto selected = display({block}, 123, 250, 1920);
    if (selected[0].size() != 127 || selected[0][0].index != 123 || selected[0][0].value != block->raw[246])
        throw std::runtime_error("zoom to original samples failed");
}
struct Store {
    std::vector<std::shared_ptr<Block>> pool;
    std::deque<std::shared_ptr<const Block>> ring;
    std::mutex mutex;
    std::thread producer;
    std::atomic<bool> cancel{false}, done{false};
    std::atomic<std::uint64_t> accepted{0};
    std::size_t keepBlocks;
    std::uint64_t blocksPlanned = 0, integrityErrors = 0, allocationStalls = 0;
    double elapsed = 0, maxBacklogMs = 0;
    std::vector<double> ingestion;
    Store(std::size_t keep) : keepBlocks(keep) {
        for (std::size_t i = 0; i < keep + 256; ++i)
            pool.push_back(std::make_shared<Block>());
    }
    ~Store() {
        cancel = true;
        if (producer.joinable())
            producer.join();
    }
    std::size_t bytes() const { return pool.size() * (blockBytes + 273 * sizeof(Pair)); }
    Snapshot snapshot() {
        std::lock_guard<std::mutex> lock(mutex);
        return {ring.begin(), ring.end()};
    }
    void start(const std::vector<std::uint8_t>& source, double seconds, double rate = 250000000.) {
        std::vector<std::array<std::uint64_t, 2>> sums(source.size() / blockBytes);
        for (std::size_t i = 0; i < source.size(); ++i)
            sums[i / blockBytes][i % 2] += source[i];
        blocksPlanned = std::uint64_t(std::ceil(seconds * rate / blockBytes));
        producer = std::thread([&, seconds, rate, sums = std::move(sums)] {
            const auto start = Clock::now();
            std::size_t cursor = 0;
            Deadline deadline;
            for (std::uint64_t sequence = 0; sequence < blocksPlanned && !cancel; ++sequence) {
                const double scheduled = double(sequence) * blockBytes / rate;
                deadline.wait(start + std::chrono::duration_cast<Clock::duration>(
                                          std::chrono::duration<double>(scheduled)));
                maxBacklogMs = std::max(
                    maxBacklogMs, std::chrono::duration<double, std::milli>(Clock::now() - start).count() -
                                      scheduled * 1000);
                std::shared_ptr<Block> block;
                for (std::size_t attempt = 0; attempt < pool.size(); ++attempt) {
                    cursor = (cursor + 1) % pool.size();
                    if (pool[cursor].use_count() == 1) {
                        block = pool[cursor];
                        break;
                    }
                }
                if (!block) {
                    ++allocationStalls;
                    break;
                }
                const auto begin = Clock::now();
                auto sourceBlock = sequence % sums.size();
                std::memcpy(block->raw.data(), source.data() + sourceBlock * blockBytes, blockBytes);
                block->first = sequence * blockSamples;
                block->build();
                if (block->sums != sums[sourceBlock] ||
                    std::memcmp(block->raw.data(), source.data() + sourceBlock * blockBytes, blockBytes))
                    ++integrityErrors;
                block->ready = Clock::now();
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    ring.push_back(block);
                    if (ring.size() > keepBlocks)
                        ring.pop_front();
                }
                accepted += blockBytes;
                ingestion.push_back(std::chrono::duration<double, std::milli>(Clock::now() - begin).count());
            }
            elapsed = std::chrono::duration<double>(Clock::now() - start).count();
            done = true;
        });
    }
};
} // namespace stream_experiment
