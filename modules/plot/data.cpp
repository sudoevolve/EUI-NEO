#include "modules/plot/data_storage.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace modules::plot {
namespace detail {
void include(DataSummary& s, Point p, std::size_t i) {
    if (!std::isfinite(p.x) || !std::isfinite(p.y))
        return;
    if (!s.valid) {
        s.min = s.max = p;
        s.minYIndex = s.maxYIndex = i;
    } else {
        s.min.x = std::min(s.min.x, p.x);
        s.max.x = std::max(s.max.x, p.x);
        if (p.y < s.min.y || (p.y == s.min.y && i < s.minYIndex)) {
            s.min.y = p.y;
            s.minYIndex = i;
        }
        if (p.y > s.max.y || (p.y == s.max.y && i < s.maxYIndex)) {
            s.max.y = p.y;
            s.maxYIndex = i;
        }
    }
    ++s.valid;
}
DataSummary combine(DataSummary a, const DataSummary& b) {
    if (!a.valid)
        return b;
    if (!b.valid)
        return a;
    a.min.x = std::min(a.min.x, b.min.x);
    a.max.x = std::max(a.max.x, b.max.x);
    if (b.min.y < a.min.y || (b.min.y == a.min.y && b.minYIndex < a.minYIndex)) {
        a.min.y = b.min.y;
        a.minYIndex = b.minYIndex;
    }
    if (b.max.y > a.max.y || (b.max.y == a.max.y && b.maxYIndex < a.maxYIndex)) {
        a.max.y = b.max.y;
        a.maxYIndex = b.maxYIndex;
    }
    a.valid += b.valid;
    return a;
}
namespace {
constexpr std::size_t leaf = 32;
struct Block {
    std::vector<Point> points;
    std::vector<DataSummary> tree;
    bool ordered = true;
    void build() {
        constexpr std::size_t leaves = Data::blockCapacity / leaf;
        tree.assign(leaves * 2, {});
        ordered = true;
        for (std::size_t i = 0; i < points.size(); ++i) {
            include(tree[leaves + i / leaf], points[i], i);
            ordered = ordered && std::isfinite(points[i].x) && (!i || points[i - 1].x <= points[i].x);
        }
        for (std::size_t i = leaves - 1; i > 0; --i)
            tree[i] = combine(tree[i * 2], tree[i * 2 + 1]);
    }
    DataSummary query(std::size_t first, std::size_t end) const {
        DataSummary s;
        while (first < end && first % leaf) {
            include(s, points[first], first);
            ++first;
        }
        const auto fullEnd = end - end % leaf;
        if (first < fullEnd) {
            auto a = first / leaf + Data::blockCapacity / leaf,
                 b = fullEnd / leaf + Data::blockCapacity / leaf;
            while (a < b) {
                if (a & 1)
                    s = combine(s, tree[a++]);
                if (b & 1)
                    s = combine(s, tree[--b]);
                a /= 2;
                b /= 2;
            }
            first = fullEnd;
        }
        while (first < end) {
            include(s, points[first], first);
            ++first;
        }
        return s;
    }
};
struct XYStorage final : DataStorage {
    std::vector<std::shared_ptr<const Block>> data;
    std::vector<DataSummary> tree;
    std::size_t count = 0, leaves = 1;
    bool sorted = true;
    std::size_t size() const override { return count; }
    Point at(std::size_t i) const override {
        return data[i / Data::blockCapacity]->points[i % Data::blockCapacity];
    }
    double xAt(std::size_t i) const override {
        return data[i / Data::blockCapacity]->points[i % Data::blockCapacity].x;
    }
    bool ordered() const override { return sorted; }
    std::size_t blocks() const override { return data.size(); }
    const void* identity(std::size_t i) const override { return data.at(i).get(); }
    void build() {
        leaves = 1;
        while (leaves < data.size())
            leaves *= 2;
        tree.assign(leaves * 2, {});
        sorted = true;
        for (std::size_t i = 0; i < data.size(); ++i) {
            auto s = data[i]->tree[1];
            s.minYIndex += i * Data::blockCapacity;
            s.maxYIndex += i * Data::blockCapacity;
            tree[leaves + i] = s;
            sorted = sorted && data[i]->ordered &&
                     (!i || data[i - 1]->points.back().x <= data[i]->points.front().x);
        }
        for (std::size_t i = leaves - 1; i > 0; --i)
            tree[i] = combine(tree[2 * i], tree[2 * i + 1]);
    }
    DataSummary summary(std::size_t first, std::size_t end) const override {
        DataSummary s;
        auto partial = [&](std::size_t a, std::size_t b) {
            const auto base = a / Data::blockCapacity * Data::blockCapacity;
            auto t = data[a / Data::blockCapacity]->query(a - base, b - base);
            t.minYIndex += base;
            t.maxYIndex += base;
            s = combine(s, t);
        };
        if (first < end && first % Data::blockCapacity) {
            auto stop = std::min(end, (first / Data::blockCapacity + 1) * Data::blockCapacity);
            partial(first, stop);
            first = stop;
        }
        const auto fullEnd = end - end % Data::blockCapacity;
        if (first < fullEnd) {
            auto a = first / Data::blockCapacity + leaves, b = fullEnd / Data::blockCapacity + leaves;
            while (a < b) {
                if (a & 1)
                    s = combine(s, tree[a++]);
                if (b & 1)
                    s = combine(s, tree[--b]);
                a /= 2;
                b /= 2;
            }
            first = fullEnd;
        }
        if (first < end)
            partial(first, end);
        return s;
    }
    std::shared_ptr<const DataStorage> edit(std::size_t first, const std::vector<double>& x,
                                            const std::vector<double>& y, bool append) const override {
        auto result = std::make_shared<XYStorage>(*this);
        std::size_t input = 0;
        while (input < x.size()) {
            const auto index = first + input, blockIndex = index / Data::blockCapacity,
                       offset = index % Data::blockCapacity;
            auto block = blockIndex < result->data.size() ? std::make_shared<Block>(*result->data[blockIndex])
                                                          : std::make_shared<Block>();
            const auto n = std::min(Data::blockCapacity - offset, x.size() - input);
            if (append)
                block->points.resize(offset + n);
            for (std::size_t i = 0; i < n; ++i, ++input)
                block->points[offset + i] = {x[input], y[input]};
            block->build();
            if (blockIndex == result->data.size())
                result->data.push_back(block);
            else
                result->data[blockIndex] = block;
        }
        if (append)
            result->count += x.size();
        result->build();
        return result;
    }
};
} // namespace
} // namespace detail

Data::Data() = default;
Data::Data(std::shared_ptr<const detail::DataStorage> storage, std::uint64_t revision)
    : storage_(std::move(storage)), revision_(revision) {}
Data::Data(const std::vector<double>& x, const std::vector<double>& y) { *this = append(x, y); }
std::size_t Data::size() const noexcept { return storage_ ? storage_->size() : 0; }
Point Data::at(std::size_t i) const {
    if (i >= size())
        throw std::out_of_range("plot: sample index");
    return storage_->at(i);
}
double Data::xAt(std::size_t i) const {
    if (i >= size())
        throw std::out_of_range("plot: sample index");
    return storage_->xAt(i);
}
std::uint64_t Data::revision() const noexcept { return revision_; }
std::size_t Data::blockCount() const noexcept { return storage_ ? storage_->blocks() : 0; }
const void* Data::blockIdentity(std::size_t i) const {
    if (!storage_)
        throw std::out_of_range("plot: block index");
    return storage_->identity(i);
}
Data Data::append(const std::vector<double>& x, const std::vector<double>& y) const {
    if (x.size() != y.size())
        throw std::invalid_argument("plot: XY lengths differ");
    if (x.empty())
        return *this;
    if (x.size() > std::numeric_limits<std::size_t>::max() - size())
        throw std::length_error("plot: data size overflow");
    if (revision_ == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("plot: data revision exhausted");
    auto source = storage_ ? storage_ : std::make_shared<detail::XYStorage>();
    return Data(source->edit(size(), x, y, true), revision_ + 1);
}
Data Data::replace(std::size_t first, const std::vector<double>& x, const std::vector<double>& y) const {
    if (x.size() != y.size())
        throw std::invalid_argument("plot: XY lengths differ");
    if (first > size() || x.size() > size() - first)
        throw std::out_of_range("plot: replacement interval");
    if (x.empty())
        return *this;
    if (revision_ == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("plot: data revision exhausted");
    return Data(storage_->edit(first, x, y, false), revision_ + 1);
}
std::vector<Point> Data::points() const {
    std::vector<Point> out;
    out.reserve(size());
    for (std::size_t i = 0; i < size(); ++i)
        out.push_back(at(i));
    return out;
}
DataSummary Data::summary(std::size_t first, std::size_t end) const {
    if (first > end || end > size())
        throw std::out_of_range("plot: summary interval");
    return first == end ? DataSummary{} : storage_->summary(first, end);
}
bool Data::orderedX() const noexcept { return !storage_ || storage_->ordered(); }
std::size_t Data::boundX(double value, bool upper) const {
    if (!orderedX() || std::isnan(value))
        throw std::invalid_argument("plot: X search requires ordered data");
    std::size_t a = 0, b = size();
    while (a < b) {
        const auto m = a + (b - a) / 2;
        const auto x = xAt(m);
        if (x < value || (upper && x == value))
            a = m + 1;
        else
            b = m;
    }
    return a;
}
std::vector<std::size_t> Data::missingIndices() const {
    std::vector<std::size_t> out;
    const auto visit = [&](const auto& self, std::size_t a, std::size_t b) -> void {
        if (a == b || summary(a, b).valid == b - a)
            return;
        if (b - a <= 32) {
            for (auto i = a; i < b; ++i) {
                auto p = at(i);
                if (!std::isfinite(p.x) || !std::isfinite(p.y))
                    out.push_back(i);
            }
            return;
        }
        auto m = a + (b - a) / 2;
        self(self, a, m);
        self(self, m, b);
    };
    visit(visit, 0, size());
    return out;
}
} // namespace modules::plot
