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
std::size_t sampleBytes(SampleFormat format) {
    switch (format) {
    case SampleFormat::UInt8:
        return 1;
    case SampleFormat::Int16:
    case SampleFormat::UInt16:
        return 2;
    default:
        throw std::invalid_argument("plot: unsupported waveform sample format");
    }
}
struct Block {
    std::vector<std::uint8_t> raw;
    std::vector<Extrema> tree;
    std::size_t channels, samples;
    SampleFormat format;
    Block(std::size_t c, std::size_t n, SampleFormat f)
        : raw(c * n * sampleBytes(f)), tree(c * 2 * (n / leafSamples)), channels(c), samples(n), format(f) {}
    template <class T> T typedAt(std::size_t i, std::size_t c) const {
        T value;
        std::memcpy(&value, raw.data() + (i * channels + c) * sizeof(T), sizeof(T));
        return value;
    }
    double at(std::size_t i, std::size_t c) const {
        if (format == SampleFormat::UInt8)
            return typedAt<std::uint8_t>(i, c);
        if (format == SampleFormat::Int16)
            return typedAt<std::int16_t>(i, c);
        return typedAt<std::uint16_t>(i, c);
    }
    Extrema merge(Extrema a, Extrema b, std::size_t c) const {
        const auto al = at(a.low, c), bl = at(b.low, c), ah = at(a.high, c), bh = at(b.high, c);
        if (bl < al || (bl == al && b.low < a.low))
            a.low = b.low;
        if (bh > ah || (bh == ah && b.high < a.high))
            a.high = b.high;
        return a;
    }
    template <class T> void buildTyped() {
        const auto leaves = samples / leafSamples;
        for (std::size_t c = 0; c < channels; ++c) {
            auto* nodes = tree.data() + c * 2 * leaves;
            for (std::size_t l = 0; l < leaves; ++l) {
                const auto first = std::uint32_t(l * leafSamples);
                Extrema e{first, first};
                auto low = typedAt<T>(first, c), high = low;
                for (auto i = first + 1; i < first + leafSamples; ++i) {
                    const auto value = typedAt<T>(i, c);
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
    void build() {
        switch (format) {
        case SampleFormat::UInt8:
            buildTyped<std::uint8_t>();
            break;
        case SampleFormat::Int16:
            buildTyped<std::int16_t>();
            break;
        case SampleFormat::UInt16:
            buildTyped<std::uint16_t>();
            break;
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
            pool.push_back(std::make_shared<Block>(c.channels, c.blockSamples, c.format));
    }
};
std::size_t WaveformBuffer::requiredBytes(const Config& c) {
    if (!c.channels || c.channels > 64 || !std::isfinite(c.sampleRate) || c.sampleRate <= 0 ||
        !c.retainedSamples || !c.spareBlocks || c.blockSamples < leafSamples || c.blockSamples > 1u << 24 ||
        (c.blockSamples & (c.blockSamples - 1)))
        throw std::invalid_argument("plot: invalid waveform configuration");
    const auto count = retainedBlocks(c), pool = checkedAdd(count, c.spareBlocks);
    checkedMultiply(count, c.blockSamples);
    const auto payload = checkedMultiply(checkedMultiply(c.channels, c.blockSamples), sampleBytes(c.format));
    const auto summaries = checkedMultiply(c.channels * 2 * (c.blockSamples / leafSamples), sizeof(Extrema));
    return checkedAdd(checkedMultiply(pool, checkedAdd(checkedAdd(payload, summaries),
                                                       sizeof(Block) + sizeof(std::shared_ptr<Block>) + 64)),
                      checkedMultiply(count, sizeof(std::shared_ptr<const Block>)));
}
WaveformBuffer::Config WaveformBuffer::Config::forDuration(std::size_t channels, double rate, double seconds,
                                                           SampleFormat format, std::size_t budget) {
    const long double count = std::ceil(static_cast<long double>(rate) * seconds);
    if (!std::isfinite(rate) || rate <= 0 || !std::isfinite(seconds) || seconds <= 0 ||
        !std::isfinite(count) || count < 1 ||
        count >= static_cast<long double>(std::numeric_limits<std::size_t>::max()))
        throw std::invalid_argument("plot: invalid waveform duration/rate");
    Config c;
    c.channels = channels;
    c.sampleRate = rate;
    c.retainedSamples = static_cast<std::size_t>(count);
    c.format = format;
    c.budgetBytes = budget;
    // Around a millisecond per publication, clamped to the supported block range.
    c.blockSamples = 256;
    while (c.blockSamples < 65536 && c.blockSamples < rate / 1000.)
        c.blockSamples *= 2;
    // Leave at least 100 ms of UI snapshot overlap at the requested sample rate.
    const long double overlap = std::ceil(static_cast<long double>(rate) * .1L / c.blockSamples) + 1;
    if (overlap >= static_cast<long double>(std::numeric_limits<std::size_t>::max()))
        throw std::length_error("plot: waveform overlap size overflow");
    c.spareBlocks = std::max(c.spareBlocks, static_cast<std::size_t>(overlap));
    if (WaveformBuffer::requiredBytes(c) > budget)
        throw std::length_error("plot: requested waveform duration exceeds budget");
    return c;
}
WaveformBuffer::Config WaveformBuffer::Config::dual125MS() {
    Config c = forDuration(2, 125000000, 5, SampleFormat::UInt8, 1536ull * 1024 * 1024);
    c.spareBlocks = 256;
    return c;
}
const WaveformBuffer::Config& WaveformBuffer::config() const noexcept { return state_->config; }
std::size_t WaveformBuffer::blockBytes() const noexcept {
    const auto& c = state_->config;
    return c.channels * c.blockSamples * (c.format == SampleFormat::UInt8 ? 1 : 2);
}
WaveformBuffer::WaveformBuffer(Config config) {
    const auto bytes = requiredBytes(config);
    if (bytes > config.budgetBytes)
        throw std::length_error("plot: waveform pool exceeds budget");
    state_ = std::make_unique<State>(config, bytes);
}
WaveformBuffer::~WaveformBuffer() = default;
std::size_t WaveformBuffer::reservedBytes() const noexcept { return state_->bytes; }
bool WaveformBuffer::tryAppend(const void* input, std::size_t bytes) {
    auto& s = *state_;
    if (!input || bytes != blockBytes())
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
WaveformInput::WaveformInput(std::shared_ptr<WaveformBuffer> buffer) : buffer_(std::move(buffer)) {
    if (!buffer_)
        throw std::invalid_argument("plot: waveform input needs a buffer");
    pending_.resize(buffer_->blockBytes());
}
bool WaveformInput::flush() {
    if (used_ == pending_.size()) {
        if (!buffer_->tryAppend(pending_.data(), used_))
            return false;
        used_ = 0;
    }
    return used_ == 0;
}
std::size_t WaveformInput::write(const void* bytes, std::size_t count) {
    if (!bytes && count)
        throw std::invalid_argument("plot: null waveform packet");
    auto* source = static_cast<const std::uint8_t*>(bytes);
    std::size_t accepted = 0;
    while (accepted < count) {
        if (used_ == pending_.size() && !flush())
            break;
        // Full aligned packets bypass the staging copy.
        if (!used_ && count - accepted >= pending_.size()) {
            if (!buffer_->tryAppend(source + accepted, pending_.size()))
                break;
            accepted += pending_.size();
            continue;
        }
        const auto n = std::min(count - accepted, pending_.size() - used_);
        std::memcpy(pending_.data() + used_, source + accepted, n);
        used_ += n;
        accepted += n;
    }
    if (used_ == pending_.size())
        flush();
    return accepted;
}
} // namespace modules::plot
