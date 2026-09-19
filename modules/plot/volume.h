#pragma once

#include "modules/plot/scene3d.h"
#include <functional>

namespace modules::plot {

/** Float64 grid points, x fastest: (z * ny + y) * nx + x. Positive spacing. */
struct VolumeLayout {
    std::array<std::size_t, 3> dimensions{0, 0, 0};
    Vec3 spacing{1, 1, 1}, origin;
    void validate() const;
    std::size_t sampleCount() const;
    Bounds3D bounds() const;
};
struct VolumeBrick {
    std::array<std::size_t, 3> first{}, size{};
};
/** UI-thread LRU cache. Loader fills one brick, including edge bricks, in x-fast order.
 * Shared dense storage is never cloned. Missing values are NaN/Inf. Revisions invalidate cached
 * bricks explicitly; asynchronous loaders must publish completed snapshots on the UI thread.
 */
class VolumeData {
  public:
    using Loader = std::function<void(const VolumeBrick &, std::vector<double> &)>;
    VolumeData(VolumeLayout layout, std::shared_ptr<const std::vector<double>> values);
    VolumeData(VolumeLayout layout, Loader loader, std::size_t cacheBytes = 16 * 1024 * 1024,
               std::size_t brickEdge = 16);
    ~VolumeData();
    const VolumeLayout &layout() const;
    double value(std::size_t x, std::size_t y, std::size_t z) const;
    std::optional<double> sample(Vec3 world) const;
    Vec3 gradient(Vec3 world) const;
    std::size_t cacheBytes() const;
    std::size_t loads() const;
    std::uint64_t revision() const;
    void invalidate(const VolumeBrick &region);
    void clearCache();

  private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
};
struct TransferStop {
    double value = 0;
    std::array<float, 4> rgba{0, 0, 0, 0};
};
/** Strictly increasing finite knots; RGBA linearly interpolated, clamped outside range. */
class TransferFunction {
  public:
    explicit TransferFunction(std::vector<TransferStop> stops = {{0, {0, 0, 0, 0}}, {1, {1, 1, 1, 0.3f}}});
    std::array<float, 4> map(double value) const;
    const std::vector<TransferStop> &stops() const { return stops_; }

  private:
    std::vector<TransferStop> stops_;
};
struct VolumeLayer {
    std::shared_ptr<VolumeData> data;
    TransferFunction transfer;
    double referenceStep = 1;
};
struct SlicePlane {
    Vec3 origin, u{1, 0, 0}, v{0, 1, 0};
    double width = 1, height = 1;
    std::size_t columns = 64, rows = 64;
};
Geometry3D volumeSlice(const VolumeData &volume, const SlicePlane &plane,
                       std::size_t budgetBytes = 64 * 1024 * 1024);
/** Shared cursor for axial views; world coordinates, finite and clamped to data bounds. */
class SliceLink {
  public:
    explicit SliceLink(VolumeLayout layout);
    void setPosition(Vec3 point);
    Vec3 position() const { return position_; }
    std::uint64_t revision() const { return revision_; }
    SlicePlane plane(std::size_t normalAxis) const;

  private:
    VolumeLayout layout_;
    Vec3 position_;
    std::uint64_t revision_ = 0;
};
/** Consistent six-tetrahedron grid decomposition, shared edge vertices, normals toward increasing
 * scalar. Missing cubes skipped; boundary surfaces left open. Memory bound includes edge table.
 */
Geometry3D isoSurface(const VolumeData &volume, double level, std::size_t budgetBytes = 64 * 1024 * 1024);

} // namespace modules::plot
