#pragma once
#include "modules/plot/data.h"

namespace modules::plot {

enum class SampleFormat { UInt8, Int16, UInt16 };

/** Compact, bounded, interleaved integer acquisition history. X is time in seconds.
 * 16-bit samples use native byte order; decode device endianness before publishing.
 */
class WaveformBuffer {
  public:
    struct Config {
        std::size_t channels = 2;
        double sampleRate = 100000;
        std::size_t retainedSamples = 100000;
        std::size_t blockSamples = 4096; // Power of two, at least 256.
        std::size_t spareBlocks = 16;    // Old snapshots pin blocks; exhaustion is explicit backpressure.
        std::size_t budgetBytes = 64ull * 1024 * 1024;
        SampleFormat format = SampleFormat::UInt8;
        /** Calculate rounded retention from seconds; validates against budget before allocation. */
        static Config forDuration(std::size_t channels, double sampleRate, double seconds,
                                  SampleFormat format = SampleFormat::UInt8,
                                  std::size_t budgetBytes = 64ull * 1024 * 1024);
        /** Explicit 250 MB/s, two-channel UInt8, five-second preset (~1.37 GB pool). */
        static Config dual125MS();
    };
    struct Snapshot {
        std::uint64_t firstSample = 0, nextSample = 0;
        std::vector<Data> channels;
        // Pick.index is local to this snapshot; firstSample + Pick.index is the acquisition index.
    };
    explicit WaveformBuffer(Config config);
    ~WaveformBuffer();
    WaveformBuffer(const WaveformBuffer&) = delete;
    WaveformBuffer& operator=(const WaveformBuffer&) = delete;

    /** One complete interleaved block. Copies before return. False means no space: nothing accepted.
     * May run on an acquisition thread concurrently with snapshot(); multiple writers are serialized.
     * Callers must handle backpressure/retry, never silently discard an unaccepted block.
     * Invalid block size throws. No partial acceptance, allocation, or raw-history expansion.
     */
    bool tryAppend(const void* interleaved, std::size_t bytes);
    std::size_t blockBytes() const noexcept;
    const Config& config() const noexcept;
    /** Atomic channel snapshots. Retention rounds UP to full blocks. Old snapshots remain immutable. */
    Snapshot snapshot() const;
    /** Reserved block payload + extrema + block/pool bookkeeping (excludes consumer snapshots). */
    std::size_t reservedBytes() const noexcept;
    static std::size_t requiredBytes(const Config& config);

  private:
    struct State;
    std::unique_ptr<State> state_;
};

/** One producer's packet adapter. Accepts arbitrary byte chunks, including split 16-bit samples.
 * Return value is the accepted byte prefix; retry the remainder on backpressure. A buffered full
 * block can be retried with flush(). Partial blocks stay pending until completed; no zero padding.
 * Do not mix writers/direct tryAppend calls into the same buffer while this adapter is in use.
 */
class WaveformInput {
  public:
    explicit WaveformInput(std::shared_ptr<WaveformBuffer> buffer);
    WaveformInput(const WaveformInput&) = delete;
    WaveformInput& operator=(const WaveformInput&) = delete;
    std::size_t write(const void* bytes, std::size_t count);
    bool flush();
    std::size_t pendingBytes() const noexcept { return used_; }

  private:
    std::shared_ptr<WaveformBuffer> buffer_;
    std::vector<std::uint8_t> pending_;
    std::size_t used_ = 0;
};
} // namespace modules::plot
