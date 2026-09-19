#pragma once
#include "modules/plot/data.h"

namespace modules::plot {

/** Compact, bounded, interleaved UInt8 acquisition history. X is time in seconds. */
class WaveformBuffer {
  public:
    struct Config {
        std::size_t channels = 2;
        double sampleRate = 125000000;
        std::size_t retainedSamples = 625000000;
        std::size_t blockSamples = 65536; // Power of two, at least 256.
        std::size_t spareBlocks = 256;    // Old snapshots pin blocks; exhaustion is explicit backpressure.
        std::size_t budgetBytes = 1536ull * 1024 * 1024;
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
    bool tryAppend(const std::uint8_t* interleaved, std::size_t bytes);
    /** Atomic channel snapshots. Retention rounds UP to full blocks. Old snapshots remain immutable. */
    Snapshot snapshot() const;
    /** Reserved block payload + extrema + block/pool bookkeeping (excludes consumer snapshots). */
    std::size_t reservedBytes() const noexcept;
    static std::size_t requiredBytes(const Config& config);

  private:
    struct State;
    std::unique_ptr<State> state_;
};
} // namespace modules::plot
