#pragma once
#include "modules/plot/data.h"
#include <stdexcept>

namespace modules::plot::detail {
// Immutable storage. Summaries include points with BOTH coordinates finite.
// Ordered X permits duplicate X and missing Y, but not missing X.
struct DataStorage {
    virtual ~DataStorage() = default;
    virtual std::size_t size() const = 0;
    virtual Point at(std::size_t index) const = 0;
    virtual double xAt(std::size_t index) const { return at(index).x; }
    virtual DataSummary summary(std::size_t first, std::size_t end) const = 0;
    virtual bool ordered() const = 0;
    virtual std::size_t blocks() const = 0;
    virtual const void* identity(std::size_t block) const = 0;
    virtual std::shared_ptr<const DataStorage> edit(std::size_t, const std::vector<double>&,
                                                    const std::vector<double>&, bool) const {
        throw std::logic_error("plot: sampled snapshots are read-only; publish through WaveformBuffer");
    }
};
void include(DataSummary& summary, Point point, std::size_t index);
DataSummary combine(DataSummary a, const DataSummary& b);
} // namespace modules::plot::detail
