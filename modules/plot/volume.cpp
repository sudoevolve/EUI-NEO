#include "modules/plot/volume.h"

#include <algorithm>
#include <cmath>
#include <list>
#include <map>
#include <stdexcept>

namespace modules::plot {
void VolumeLayout::validate() const {
    if (!finite(origin) || !finite(spacing) || spacing.x <= 0 || spacing.y <= 0 || spacing.z <= 0)
        throw std::invalid_argument("plot: invalid volume origin/spacing");
    std::size_t count = 1;
    for (auto n : dimensions) {
        if (n < 2)
            throw std::invalid_argument("plot: volume dimensions must be at least two");
        if (n > std::numeric_limits<std::size_t>::max() / sizeof(double) / count)
            throw std::length_error("plot: volume dimensions overflow");
        count *= n;
    }
    const auto end =
        origin + Vec3{spacing.x * double(dimensions[0] - 1), spacing.y * double(dimensions[1] - 1),
                      spacing.z * double(dimensions[2] - 1)};
    Bounds3D{origin, end}.validate();
}
std::size_t VolumeLayout::sampleCount() const {
    validate();
    return dimensions[0] * dimensions[1] * dimensions[2];
}
Bounds3D VolumeLayout::bounds() const {
    validate();
    return {origin,
            origin + Vec3{spacing.x * double(dimensions[0] - 1), spacing.y * double(dimensions[1] - 1),
                          spacing.z * double(dimensions[2] - 1)}};
}

struct VolumeData::Impl {
    VolumeLayout layout;
    std::shared_ptr<const std::vector<double>> dense;
    Loader loader;
    std::size_t edge = 16, budget = 0, bytes = 0, loads = 0;
    std::uint64_t revision = 1;
    struct Entry {
        VolumeBrick brick;
        std::vector<double> data;
    };
    // List nodes have a bounded per-brick accounting charge. No unbounded lookup table.
    std::list<Entry> cache;
    static constexpr std::size_t entryBytes = sizeof(Entry) + 4 * sizeof(void *);
};
VolumeData::VolumeData(VolumeLayout layout, std::shared_ptr<const std::vector<double>> values)
    : impl_(std::make_shared<Impl>()) {
    layout.validate();
    if (!values || values->size() != layout.sampleCount())
        throw std::invalid_argument("plot: volume storage length mismatch");
    impl_->layout = layout;
    impl_->dense = std::move(values);
}
VolumeData::VolumeData(VolumeLayout layout, Loader loader, std::size_t budget, std::size_t edge)
    : impl_(std::make_shared<Impl>()) {
    layout.validate();
    if (!loader || !edge || edge > 1024)
        throw std::invalid_argument("plot: invalid brick loader/edge");
    std::size_t maximum = sizeof(double);
    for (auto d : layout.dimensions)
        maximum *= std::min(d, edge);
    if (budget < maximum + Impl::entryBytes)
        throw std::length_error("plot: cache cannot hold one brick");
    impl_->layout = layout;
    impl_->loader = std::move(loader);
    impl_->edge = edge;
    impl_->budget = budget;
}
VolumeData::~VolumeData() = default;
const VolumeLayout &VolumeData::layout() const { return impl_->layout; }
std::size_t VolumeData::cacheBytes() const { return impl_->bytes; }
std::size_t VolumeData::loads() const { return impl_->loads; }
std::uint64_t VolumeData::revision() const { return impl_->revision; }
void VolumeData::clearCache() {
    impl_->cache.clear();
    impl_->bytes = 0;
}
void VolumeData::invalidate(const VolumeBrick &region) {
    const auto &dims = impl_->layout.dimensions;
    for (int i = 0; i < 3; ++i)
        if (!region.size[i] || region.first[i] >= dims[i] || region.size[i] > dims[i] - region.first[i])
            throw std::out_of_range("plot: volume update region");
    for (auto it = impl_->cache.begin(); it != impl_->cache.end();) {
        bool overlaps = true;
        for (int i = 0; i < 3; ++i)
            overlaps = overlaps && it->brick.first[i] < region.first[i] + region.size[i] &&
                       region.first[i] < it->brick.first[i] + it->brick.size[i];
        if (overlaps) {
            impl_->bytes -= it->data.capacity() * sizeof(double) + Impl::entryBytes;
            it = impl_->cache.erase(it);
        } else
            ++it;
    }
    ++impl_->revision;
}
double VolumeData::value(std::size_t x, std::size_t y, std::size_t z) const {
    auto &s = *impl_;
    const auto &d = s.layout.dimensions;
    if (x >= d[0] || y >= d[1] || z >= d[2])
        throw std::out_of_range("plot: voxel index");
    if (s.dense)
        return (*s.dense)[(z * d[1] + y) * d[0] + x];
    const std::array<std::size_t, 3> first{x / s.edge * s.edge, y / s.edge * s.edge, z / s.edge * s.edge};
    auto it = std::find_if(s.cache.begin(), s.cache.end(),
                           [&](const Impl::Entry &e) { return e.brick.first == first; });
    if (it == s.cache.end()) {
        VolumeBrick b;
        b.first = first;
        for (int i = 0; i < 3; ++i)
            b.size[i] = std::min(s.edge, d[i] - first[i]);
        const auto count = b.size[0] * b.size[1] * b.size[2],
                   required = count * sizeof(double) + Impl::entryBytes;
        while (s.bytes > s.budget - required) {
            s.bytes -= s.cache.back().data.capacity() * sizeof(double) + Impl::entryBytes;
            s.cache.pop_back();
        }
        std::vector<double> data(count);
        s.loader(b, data);
        if (data.size() != count)
            throw std::invalid_argument("plot: brick loader changed output length");
        const auto actual = data.capacity() * sizeof(double) + Impl::entryBytes;
        if (actual > s.budget - s.bytes)
            throw std::length_error("plot: brick loader exceeded cache budget");
        s.cache.push_front({b, std::move(data)});
        s.bytes += actual;
        ++s.loads;
        it = s.cache.begin();
    } else
        s.cache.splice(s.cache.begin(), s.cache, it);
    const auto &e = s.cache.front();
    return e.data[((z - first[2]) * e.brick.size[1] + y - first[1]) * e.brick.size[0] + x - first[0]];
}
std::optional<double> VolumeData::sample(Vec3 world) const {
    const auto &l = impl_->layout;
    if (!finite(world))
        return {};
    std::array<std::size_t, 3> base{};
    Vec3 f;
    for (int i = 0; i < 3; ++i) {
        const double u = (world[i] - l.origin[i]) / l.spacing[i];
        if (!std::isfinite(u) || u < 0 || u > double(l.dimensions[i] - 1))
            return {};
        base[i] = std::min(std::size_t(u), l.dimensions[i] - 2);
        f[i] = u - double(base[i]);
    }
    double result = 0;
    for (int z = 0; z < 2; ++z)
        for (int y = 0; y < 2; ++y)
            for (int x = 0; x < 2; ++x) {
                const double weight = (x ? f.x : 1 - f.x) * (y ? f.y : 1 - f.y) * (z ? f.z : 1 - f.z);
                if (weight == 0)
                    continue;
                const double v = value(base[0] + x, base[1] + y, base[2] + z);
                if (!std::isfinite(v))
                    return {};
                result += weight * v;
            }
    return std::isfinite(result) ? std::optional<double>(result) : std::nullopt;
}
Vec3 VolumeData::gradient(Vec3 p) const {
    Vec3 result;
    const auto b = layout().bounds();
    for (int i = 0; i < 3; ++i) {
        auto a = p, c = p;
        a[i] = std::max(b.min[i], p[i] - layout().spacing[i] * 0.5);
        c[i] = std::min(b.max[i], p[i] + layout().spacing[i] * 0.5);
        const auto va = sample(a), vc = sample(c);
        if (va && vc && c[i] > a[i])
            result[i] = (*vc - *va) / (c[i] - a[i]);
    }
    return result;
}
TransferFunction::TransferFunction(std::vector<TransferStop> stops) : stops_(std::move(stops)) {
    if (stops_.empty())
        throw std::invalid_argument("plot: empty transfer function");
    for (std::size_t i = 0; i < stops_.size(); ++i) {
        if (!std::isfinite(stops_[i].value) || (i && stops_[i].value <= stops_[i - 1].value))
            throw std::invalid_argument("plot: unordered transfer knots");
        for (auto c : stops_[i].rgba)
            if (!std::isfinite(c) || c < 0 || c > 1)
                throw std::invalid_argument("plot: invalid transfer color");
    }
}
std::array<float, 4> TransferFunction::map(double value) const {
    if (!std::isfinite(value))
        return {0, 0, 0, 0};
    if (value <= stops_.front().value)
        return stops_.front().rgba;
    if (value >= stops_.back().value)
        return stops_.back().rgba;
    auto it = std::upper_bound(stops_.begin(), stops_.end(), value,
                               [](double v, const TransferStop &s) { return v < s.value; });
    const auto &a = *(it - 1);
    const double t = (value - a.value) / (it->value - a.value);
    std::array<float, 4> result;
    for (int i = 0; i < 4; ++i)
        result[i] = float(a.rgba[i] * (1 - t) + it->rgba[i] * t);
    return result;
}

Geometry3D volumeSlice(const VolumeData &volume, const SlicePlane &p, std::size_t budget) {
    if (!finite(p.origin) || !finite(p.u) || !finite(p.v) || !std::isfinite(p.width) ||
        !std::isfinite(p.height) || p.width <= 0 || p.height <= 0 || p.rows < 2 || p.columns < 2)
        throw std::invalid_argument("plot: invalid slice plane");
    const auto u = normalized(p.u), v = normalized(p.v);
    if (std::abs(dot(u, v)) > 1e-8)
        throw std::invalid_argument("plot: slice basis must be orthogonal");
    constexpr std::size_t perVertex =
        2 * sizeof(Vec3) + sizeof(double) + sizeof(std::size_t) + 2 * sizeof(std::array<std::size_t, 3>);
    if (p.columns > budget / perVertex / p.rows)
        throw std::length_error("plot: slice budget exceeded");
    Geometry3D g;
    const auto count = p.rows * p.columns;
    g.positions.reserve(count);
    g.normals.reserve(count);
    g.scalars.reserve(count);
    g.sourceIndices.reserve(count);
    g.triangles.reserve(2 * (p.rows - 1) * (p.columns - 1));
    const auto &l = volume.layout();
    const auto n = cross(u, v);
    for (std::size_t y = 0; y < p.rows; ++y)
        for (std::size_t x = 0; x < p.columns; ++x) {
            const auto world = p.origin + u * (p.width * double(x) / double(p.columns - 1)) +
                               v * (p.height * double(y) / double(p.rows - 1));
            const auto scalar = volume.sample(world);
            const double missing = std::numeric_limits<double>::quiet_NaN();
            const auto i = g.positions.size();
            g.positions.push_back(scalar ? world : Vec3{missing, missing, missing});
            g.normals.push_back(n);
            g.scalars.push_back(scalar.value_or(missing));
            std::array<std::size_t, 3> voxel{};
            for (int a = 0; a < 3; ++a)
                voxel[a] = std::size_t(std::clamp(std::round((world[a] - l.origin[a]) / l.spacing[a]), 0.0,
                                                  double(l.dimensions[a] - 1)));
            g.sourceIndices.push_back((voxel[2] * l.dimensions[1] + voxel[1]) * l.dimensions[0] + voxel[0]);
            if (x && y) {
                const auto a = i - p.columns - 1, b = i - p.columns, c = i - 1;
                g.triangles.push_back({a, b, i});
                g.triangles.push_back({a, i, c});
            }
        }
    return g;
}
SliceLink::SliceLink(VolumeLayout layout) : layout_(layout) {
    layout.validate();
    const auto b = layout.bounds();
    position_ = b.min + (b.max - b.min) / 2;
}
void SliceLink::setPosition(Vec3 p) {
    if (!finite(p))
        throw std::invalid_argument("plot: invalid slice cursor");
    const auto b = layout_.bounds();
    for (int i = 0; i < 3; ++i)
        p[i] = std::clamp(p[i], b.min[i], b.max[i]);
    if (p.x != position_.x || p.y != position_.y || p.z != position_.z) {
        position_ = p;
        ++revision_;
    }
}
SlicePlane SliceLink::plane(std::size_t axis) const {
    if (axis > 2)
        throw std::out_of_range("plot: slice axis");
    const std::size_t a = (axis + 1) % 3, b = (axis + 2) % 3;
    SlicePlane p;
    p.origin = layout_.origin;
    p.origin[axis] = position_[axis];
    p.u = {};
    p.v = {};
    p.u[a] = 1;
    p.v[b] = 1;
    p.width = layout_.spacing[a] * double(layout_.dimensions[a] - 1);
    p.height = layout_.spacing[b] * double(layout_.dimensions[b] - 1);
    p.columns = layout_.dimensions[a];
    p.rows = layout_.dimensions[b];
    return p;
}

Geometry3D isoSurface(const VolumeData &volume, double level, std::size_t budget) {
    if (!std::isfinite(level))
        throw std::invalid_argument("plot: invalid isovalue");
    Geometry3D g;
    std::map<std::pair<std::size_t, std::size_t>, std::size_t> edges;
    const auto &l = volume.layout();
    const auto &d = l.dimensions;
    // Same body diagonal in every cube makes shared face diagonals consistent.
    constexpr int tetrahedra[6][4] = {{0, 1, 3, 7}, {0, 3, 2, 7}, {0, 2, 6, 7},
                                      {0, 6, 4, 7}, {0, 4, 5, 7}, {0, 5, 1, 7}};
    constexpr int tetraEdges[6][2] = {{0, 1}, {0, 2}, {0, 3}, {1, 2}, {1, 3}, {2, 3}};
    constexpr std::size_t edgeCharge =
        sizeof(std::pair<const std::pair<std::size_t, std::size_t>, std::size_t>) + 4 * sizeof(void *);
    auto check = [&](std::size_t extra) {
        const auto used = g.bytes() + edges.size() * edgeCharge;
        if (used > budget || extra > budget - used)
            throw std::length_error("plot: isosurface budget exceeded");
    };
    auto grow = [&](auto &values) {
        if (values.size() == values.capacity()) {
            using Value = typename std::decay_t<decltype(values)>::value_type;
            const auto capacity = values.capacity() ? values.capacity() * 2 : 16;
            check((capacity - values.capacity()) * sizeof(Value));
            values.reserve(capacity);
        }
    };
    for (std::size_t z = 0; z + 1 < d[2]; ++z)
        for (std::size_t y = 0; y + 1 < d[1]; ++y)
            for (std::size_t x = 0; x + 1 < d[0]; ++x) {
                std::array<Vec3, 8> p;
                std::array<double, 8> values;
                std::array<std::size_t, 8> ids;
                bool valid = true;
                for (int i = 0; i < 8; ++i) {
                    const auto xx = x + std::size_t(i & 1), yy = y + std::size_t((i >> 1) & 1),
                               zz = z + std::size_t((i >> 2) & 1);
                    p[i] = l.origin +
                           Vec3{double(xx) * l.spacing.x, double(yy) * l.spacing.y, double(zz) * l.spacing.z};
                    values[i] = volume.value(xx, yy, zz);
                    ids[i] = (zz * d[1] + yy) * d[0] + xx;
                    valid = valid && std::isfinite(values[i]);
                }
                if (!valid)
                    continue;
                for (const auto &tetra : tetrahedra) {
                    std::vector<std::size_t> polygon;
                    polygon.reserve(4);
                    Vec3 low{}, high{};
                    int nl = 0, nh = 0;
                    for (auto i : tetra)
                        if (values[i] < level) {
                            low = low + p[i];
                            ++nl;
                        } else {
                            high = high + p[i];
                            ++nh;
                        }
                    if (!nl || !nh)
                        continue;
                    for (const auto &edge : tetraEdges) {
                        const int a = tetra[edge[0]], b = tetra[edge[1]];
                        if ((values[a] < level) == (values[b] < level))
                            continue;
                        const double t = (level - values[a]) / (values[b] - values[a]);
                        auto key = std::minmax(ids[a], ids[b]);
                        // Exact grid-vertex intersections use one canonical key across all incident edges.
                        std::pair<std::size_t, std::size_t> owned{key.first, key.second};
                        if (t == 0)
                            owned = {ids[a], ids[a]};
                        if (t == 1)
                            owned = {ids[b], ids[b]};
                        auto found = edges.find(owned);
                        std::size_t index;
                        if (found != edges.end())
                            index = found->second;
                        else {
                            grow(g.positions);
                            grow(g.normals);
                            grow(g.scalars);
                            grow(g.sourceIndices);
                            check(edgeCharge);
                            index = g.positions.size();
                            const auto point = p[a] + (p[b] - p[a]) * t;
                            const auto gradient = volume.gradient(point);
                            g.positions.push_back(point);
                            g.normals.push_back(length(gradient) > 0
                                                    ? normalized(gradient)
                                                    : normalized(high / double(nh) - low / double(nl)));
                            g.scalars.push_back(level);
                            g.sourceIndices.push_back(t <= 0.5 ? ids[a] : ids[b]);
                            edges.emplace(owned, index);
                        }
                        if (std::find(polygon.begin(), polygon.end(), index) == polygon.end())
                            polygon.push_back(index);
                    }
                    if (polygon.size() < 3)
                        continue;
                    Vec3 center{};
                    for (auto i : polygon)
                        center = center + g.positions[i];
                    center = center / double(polygon.size());
                    const auto normal = normalized(high / double(nh) - low / double(nl));
                    const auto tangent = normalized(g.positions[polygon[0]] - center),
                               bitangent = cross(normal, tangent);
                    std::sort(polygon.begin(), polygon.end(), [&](std::size_t a, std::size_t b) {
                        const auto pa = g.positions[a] - center, pb = g.positions[b] - center;
                        return std::atan2(dot(pa, bitangent), dot(pa, tangent)) <
                               std::atan2(dot(pb, bitangent), dot(pb, tangent));
                    });
                    for (std::size_t i = 1; i + 1 < polygon.size(); ++i) {
                        std::array<std::size_t, 3> triangle{polygon[0], polygon[i], polygon[i + 1]};
                        const auto n = cross(g.positions[triangle[1]] - g.positions[triangle[0]],
                                             g.positions[triangle[2]] - g.positions[triangle[0]]);
                        if (length(n) == 0)
                            continue;
                        if (dot(n, normal) < 0)
                            std::swap(triangle[1], triangle[2]);
                        grow(g.triangles);
                        g.triangles.push_back(triangle);
                    }
                }
            }
    g.validate();
    return g;
}
} // namespace modules::plot
