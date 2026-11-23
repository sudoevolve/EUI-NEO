#pragma once

#include "modules/plot/scene3d.h"
#include "modules/plot/renderer.h"

namespace modules::plot {
namespace detail {
// RGBA32F buffer records; positions are translated/scaled in double before float conversion.
struct GpuSceneData {
    Vec3 origin;
    double scale = 1;
    bool representable = true;
    std::vector<std::array<float, 4>> nodes, primitives;
    Scene3D scene;
    RenderSettings3D settings;
};
} // namespace detail
/** OpenGL 3.3 BVH ray renderer. CPU reference remains responsible for exact source picking/export.
 * GPU storage is lazy and reused across camera/DPI changes. release() precedes device destruction.
 */
class GpuSceneRenderer3D {
  public:
    explicit GpuSceneRenderer3D(Budget budget = {});
    ~GpuSceneRenderer3D();
    GpuSceneRenderer3D(const GpuSceneRenderer3D&) = delete;
    GpuSceneRenderer3D& operator=(const GpuSceneRenderer3D&) = delete;
    // false means the scene exceeds GPU precision/storage limits; caller may use the CPU reference.
    bool render(const SceneRenderer3D& scene, std::uint64_t sceneRevision, const Camera3D& camera,
                std::uint32_t width, std::uint32_t height);
    std::shared_ptr<const eui::GpuImage> image() const;
    std::uint64_t revision() const;
    std::size_t uploads() const;
    double depthAt(std::uint32_t x, std::uint32_t y) const;
    void release();

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace modules::plot
