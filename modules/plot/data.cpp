#include "modules/plot/data.h"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace modules::plot {
namespace {
void validateLength(const std::vector<double>& x, const std::vector<double>& y) {
    if (x.size() != y.size())
        throw std::invalid_argument("plot: XY lengths differ");
}
std::uint64_t nextRevision(std::uint64_t revision) {
    if (revision == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("plot: data revision exhausted");
    return revision + 1;
}
} // namespace

Data::Data() = default;
Data::Data(const std::vector<double>& x, const std::vector<double>& y) { *this = append(x, y); }
std::size_t Data::size() const noexcept { return size_; }
std::uint64_t Data::revision() const noexcept { return revision_; }
std::size_t Data::blockCount() const noexcept { return blocks_.size(); }
const void* Data::blockIdentity(std::size_t block) const { return blocks_.at(block).get(); }
Point Data::at(std::size_t index) const {
    if (index >= size_)
        throw std::out_of_range("plot: sample index");
    return (*blocks_[index / blockCapacity])[index % blockCapacity];
}
Data Data::append(const std::vector<double>& x, const std::vector<double>& y) const {
    validateLength(x, y);
    if (x.empty())
        return *this;
    if (x.size() > std::numeric_limits<std::size_t>::max() - size_)
        throw std::length_error("plot: data size overflow");
    Data result = *this;
    result.revision_ = nextRevision(revision_);
    std::size_t input = 0;
    if (size_ % blockCapacity != 0) {
        auto block = std::make_shared<Block>(*blocks_.back());
        const auto count = std::min(blockCapacity - block->size(), x.size());
        for (; input < count; ++input)
            block->push_back({x[input], y[input]});
        result.blocks_.back() = std::move(block);
    }
    while (input < x.size()) {
        auto block = std::make_shared<Block>();
        const auto count = std::min(blockCapacity, x.size() - input);
        block->reserve(count);
        for (std::size_t i = 0; i < count; ++i, ++input)
            block->push_back({x[input], y[input]});
        result.blocks_.push_back(std::move(block));
    }
    result.size_ += x.size();
    return result;
}
Data Data::replace(std::size_t first, const std::vector<double>& x, const std::vector<double>& y) const {
    validateLength(x, y);
    if (first > size_ || x.size() > size_ - first)
        throw std::out_of_range("plot: replacement interval");
    if (x.empty())
        return *this;
    Data result = *this;
    result.revision_ = nextRevision(revision_);
    std::size_t input = 0;
    while (input < x.size()) {
        const auto index = first + input;
        const auto offset = index % blockCapacity;
        auto block = std::make_shared<Block>(*blocks_[index / blockCapacity]);
        const auto count = std::min(block->size() - offset, x.size() - input);
        for (std::size_t i = 0; i < count; ++i, ++input)
            (*block)[offset + i] = {x[input], y[input]};
        result.blocks_[index / blockCapacity] = std::move(block);
    }
    return result;
}
std::vector<Point> Data::points() const {
    std::vector<Point> result;
    result.reserve(size_);
    for (const auto& block : blocks_)
        result.insert(result.end(), block->begin(), block->end());
    return result;
}
} // namespace modules::plot
