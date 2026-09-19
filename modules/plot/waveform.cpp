#include "modules/plot/waveform.h"
#include "modules/plot/data_storage.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <mutex>

namespace modules::plot {
namespace {
constexpr std::size_t leafSamples = 256;
struct Extrema {
    std::uint32_t low = 0, high = 0;
};
struct Block {
    std::vector<std::uint8_t> raw;
    std::vector<Extrema> tree;
    std::size_t channels, samples;
    Block(std::size_t c, std::size_t n)
        : raw(c * n), tree(c * 2 * (n / leafSamples)), channels(c), samples(n) {}
    std::uint8_t at(std::size_t i, std::size_t c) const { return raw[i * channels + c]; }
    Extrema merge(Extrema a, Extrema b, std::size_t c) const {
        const auto al = at(a.low, c), bl = at(b.low, c), ah = at(a.high, c), bh = at(b.high, c);
        if (bl < al || (bl == al && b.low < a.low))
            a.low = b.low;
        if (bh > ah || (bh == ah && b.high < a.high))
            a.high = b.high;
        return a;
    }
    void build() {
        const auto leaves = samples / leafSamples;
        for (std::size_t c = 0; c < channels; ++c) {
            auto* nodes = tree.data() + c * 2 * leaves;
            for (std::size_t l = 0; l < leaves; ++l) {
                const auto first = std::uint32_t(l * leafSamples);
                Extrema e{first, first};
                auto low = at(first, c), high = low;
                for (auto i = first + 1; i < first + leafSamples; ++i) {
                    const auto value = at(i, c);
                    if (value < low) {
                        low = value;
                        e.low = i;
                    }
                    if (value > high) {
                        high = value;
                        e.high = i;
                    }
                }
                nodes[leaves + l] = e;
            }
            for (auto i = leaves - 1; i > 0; --i)
                nodes[i] = merge(nodes[i * 2], nodes[i * 2 + 1], c);
        }
    }
    Extrema query(std::size_t first, std::size_t end, std::size_t c) const {
        Extrema e{std::uint32_t(first), std::uint32_t(first)};
        const auto scan = [&](std::size_t a, std::size_t b) {
            auto low = at(e.low, c), high = at(e.high, c);
            for (auto i = a; i < b; ++i) {
                const auto v = at(i, c);
                if (v < low || (v == low && i < e.low)) {
                    low = v;
                    e.low = std::uint32_t(i);
                }
                if (v > high || (v == high && i < e.high)) {
                    high = v;
                    e.high = std::uint32_t(i);
                }
            }
        };
        const auto boundary = std::min(end, first + (leafSamples - first % leafSamples) % leafSamples);
        scan(first, boundary);
        first = boundary;
        const auto fullEnd = end - end % leafSamples, leaves = samples / leafSamples;
        if (first < fullEnd) {
            auto a = leaves + first / leafSamples, b = leaves + fullEnd / leafSamples;
            const auto* nodes = tree.data() + c * 2 * leaves;
            while (a < b) {
                if (a & 1)
                    e = merge(e, nodes[a++], c);
                if (b & 1)
                    e = merge(e, nodes[--b], c);
                a /= 2;
                b /= 2;
            }
            first = fullEnd;
        }
        scan(first, end);
        return e;
    }
};
struct SampleStorage final : detail::DataStorage {
    std::shared_ptr<const std::vector<std::shared_ptr<const Block>>> data;
    std::vector<DataSummary> tree;
    std::uint64_t first;
    std::size_t channel, blockSamples, leaves = 1;
    double rate;
    SampleStorage(decltype(data) blocks, std::uint64_t start, std::size_t c, std::size_t n, double r)
        : data(std::move(blocks)), first(start), channel(c), blockSamples(n), rate(r) {
        while (leaves < data->size())
            leaves *= 2;
        tree.resize(leaves * 2);
        for (std::size_t b = 0; b < data->size(); ++b)
            tree[leaves + b] = partial(b, 0, blockSamples);
        for (auto i = leaves - 1; i > 0; --i)
            tree[i] = detail::combine(tree[i * 2], tree[i * 2 + 1]);
    }
    std::size_t size() const override { return data->size() * blockSamples; }
    Point at(std::size_t i) const override {
        return {double(first + i) / rate, double((*data)[i / blockSamples]->at(i % blockSamples, channel))};
    }
    double xAt(std::size_t i) const override { return double(first + i) / rate; }
    bool ordered() const override { return true; }
    std::size_t blocks() const override { return data->size(); }
    const void* identity(std::size_t i) const override { return data->at(i).get(); }
    DataSummary partial(std::size_t b, std::size_t a, std::size_t end) const {
        const auto& block = *(*data)[b];
        const auto e = block.query(a, end, channel);
        const auto base = b * blockSamples;
        return {end - a,
                base + e.low,
                base + e.high,
                {double(first + base + a) / rate, double(block.at(e.low, channel))},
                {double(first + base + end - 1) / rate, double(block.at(e.high, channel))}};
    }
    DataSummary summary(std::size_t a, std::size_t end) const override {
        DataSummary s;
        if (a < end && a % blockSamples) {
            auto stop = std::min(end, (a / blockSamples + 1) * blockSamples);
            s = partial(a / blockSamples, a % blockSamples, stop - a + a % blockSamples);
            a = stop;
        }
        const auto fullEnd = end - end % blockSamples;
        if (a < fullEnd) {
            auto x = leaves + a / blockSamples, y = leaves + fullEnd / blockSamples;
            while (x < y) {
                if (x & 1)
                    s = detail::combine(s, tree[x++]);
                if (y & 1)
                    s = detail::combine(s, tree[--y]);
                x /= 2;
                y /= 2;
            }
            a = fullEnd;
        }
        if (a < end)
            s = detail::combine(s, partial(a / blockSamples, 0, end - a));
        return s;
    }
};
std::size_t checkedAdd(std::size_t a, std::size_t b) {
    if (b > std::numeric_limits<std::size_t>::max() - a)
        throw std::length_error("plot: waveform size overflow");
    return a + b;
}
std::size_t checkedMultiply(std::size_t a, std::size_t b) {
    if (a && b > std::numeric_limits<std::size_t>::max() / a)
        throw std::length_error("plot: waveform size overflow");
    return a * b;
}
std::size_t retainedBlocks(const WaveformBuffer::Config& c) {
    return 1 + (c.retainedSamples - 1) / c.blockSamples;
}
} // namespace
struct WaveformBuffer::State {
    Config config;
    std::size_t bytes, head = 0, count = 0, cursor = 0;
    std::uint64_t next = 0, revision = 0;
    mutable std::mutex publication;
    std::mutex producer;
    std::vector<std::shared_ptr<Block>> pool;
    std::vector<std::shared_ptr<const Block>> ring;
    State(Config c, std::size_t reserved) : config(c), bytes(reserved), ring(retainedBlocks(c)) {
        pool.reserve(ring.size() + c.spareBlocks);
        for (std::size_t i = 0; i < ring.size() + c.spareBlocks; ++i)
            pool.push_back(std::make_shared<Block>(c.channels, c.blockSamples));
    }
};
std::size_t WaveformBuffer::requiredBytes(const Config& c) {
    if (!c.channels || c.channels > 64 || !std::isfinite(c.sampleRate) || c.sampleRate <= 0 ||
        !c.retainedSamples || !c.spareBlocks || c.blockSamples < leafSamples || c.blockSamples > 1u << 24 ||
        (c.blockSamples & (c.blockSamples - 1)))
        throw std::invalid_argument("plot: invalid waveform configuration");
    const auto count = retainedBlocks(c), pool = checkedAdd(count, c.spareBlocks);
    checkedMultiply(count, c.blockSamples);
    const auto payload = checkedMultiply(c.channels, c.blockSamples);
    const auto summaries = checkedMultiply(c.channels * 2 * (c.blockSamples / leafSamples), sizeof(Extrema));
    return checkedAdd(checkedMultiply(pool, checkedAdd(checkedAdd(payload, summaries),
                                                       sizeof(Block) + sizeof(std::shared_ptr<Block>) + 64)),
                      checkedMultiply(count, sizeof(std::shared_ptr<const Block>)));
}
WaveformBuffer::WaveformBuffer(Config config) {
    const auto bytes = requiredBytes(config);
    if (bytes > config.budgetBytes)
        throw std::length_error("plot: waveform pool exceeds budget");
    state_ = std::make_unique<State>(config, bytes);
}
WaveformBuffer::~WaveformBuffer() = default;
std::size_t WaveformBuffer::reservedBytes() const noexcept { return state_->bytes; }
bool WaveformBuffer::tryAppend(const std::uint8_t* input, std::size_t bytes) {
    auto& s = *state_;
    if (!input || bytes != s.config.channels * s.config.blockSamples)
        throw std::invalid_argument("plot: expected one complete interleaved waveform block");
    std::lock_guard<std::mutex> writer(s.producer);
    // Stay in the exact integer range of double time coordinates.
    if (s.next > (1ull << 53) - s.config.blockSamples ||
        !std::isfinite(double(s.next + s.config.blockSamples) / s.config.sampleRate))
        throw std::overflow_error("plot: waveform time exhausted");
    std::shared_ptr<Block> block;
    for (std::size_t attempt = 0; attempt < s.pool.size(); ++attempt) {
        auto& candidate = s.pool[s.cursor];
        s.cursor = (s.cursor + 1) % s.pool.size();
        if (candidate.use_count() == 1) {
            block = candidate;
            break;
        }
    }
    if (!block)
        return false;
    std::memcpy(block->raw.data(), input, bytes);
    block->build();
    std::lock_guard<std::mutex> publish(s.publication);
    const auto slot = (s.head + s.count) % s.ring.size();
    s.ring[slot] = std::move(block);
    if (s.count == s.ring.size())
        s.head = (s.head + 1) % s.ring.size();
    else
        ++s.count;
    s.next += s.config.blockSamples;
    ++s.revision;
    return true;
}
WaveformBuffer::Snapshot WaveformBuffer::snapshot() const {
    auto& s = *state_;
    auto blocks = std::make_shared<std::vector<std::shared_ptr<const Block>>>();
    Snapshot result;
    std::uint64_t revision;
    {
        std::lock_guard<std::mutex> publish(s.publication);
        blocks->reserve(s.count);
        for (std::size_t i = 0; i < s.count; ++i)
            blocks->push_back(s.ring[(s.head + i) % s.ring.size()]);
        result.nextSample = s.next;
        result.firstSample = s.next - s.count * s.config.blockSamples;
        revision = s.revision;
    }
    for (std::size_t c = 0; c < s.config.channels; ++c)
        result.channels.push_back(
            Data(std::make_shared<SampleStorage>(blocks, result.firstSample, c, s.config.blockSamples,
                                                 s.config.sampleRate),
                 revision));
    return result;
}
} // namespace modules::plot
