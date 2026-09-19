#pragma once

#include "modules/plot/field.h"

#include <limits>
#include <memory>

namespace modules::plot {
namespace detail { struct GpuSceneData; }
class GpuSceneRenderer3D;

struct Vec3 {
    double x = 0, y = 0, z = 0;
    double operator[](std::size_t i) const { return i == 0 ? x : i == 1 ? y : z; }
    double &operator[](std::size_t i) { return i == 0 ? x : i == 1 ? y : z; }
};
Vec3 operator+(Vec3 a, Vec3 b);
Vec3 operator-(Vec3 a, Vec3 b);
Vec3 operator*(Vec3 a, double b);
Vec3 operator/(Vec3 a, double b);
double dot(Vec3 a, Vec3 b);
Vec3 cross(Vec3 a, Vec3 b);
double length(Vec3 a);
Vec3 normalized(Vec3 a);
bool finite(Vec3 a);

struct Bounds3D {
    Vec3 min{-1, -1, -1}, max{1, 1, 1};
    void validate() const;
    bool contains(Vec3 p) const;
};
struct Ray3D {
    Vec3 origin, direction;
};
struct Projected3D {
    Point pixel;
    double depth = 0;
};
enum class Projection3D { Orthographic, Perspective };

/** Double precision world camera; rays and projection use top-left physical pixels. */
struct Camera3D {
    Vec3 target;
    double yaw = 0.65, pitch = 0.4, distance = 5;
    double verticalSpan = 3, fieldOfView = 45;
    double nearPlane = 0.001, farPlane = 10000;
    Projection3D projection = Projection3D::Orthographic;
    void validate() const;
    Vec3 eye() const;
    Ray3D ray(Point pixel, double width, double height) const;
    std::optional<Projected3D> project(Vec3 point, double width, double height) const;
    void orbit(double horizontal, double vertical);
    void pan(double horizontal, double vertical);
    void zoom(double factor);
    void fit(Bounds3D bounds);
};

enum class Primitive3D { Triangles, Lines, Points };
/** Immutable when shared with a scene. Missing positions break lines and suppress faces. */
struct Geometry3D {
    Primitive3D primitive = Primitive3D::Triangles;
    std::vector<Vec3> positions;
    std::vector<Vec3> normals;
    std::vector<double> scalars;
    std::vector<std::array<float, 4>> colors;
    std::vector<std::array<std::size_t, 3>> triangles;
    std::vector<std::array<std::size_t, 2>> lines;
    // Optional mapping from generated vertices to source samples (volume: x-fast voxel index).
    std::vector<std::size_t> sourceIndices;
    void validate() const;
    std::size_t bytes() const;
};
Geometry3D curve3D(const std::vector<double> &x, const std::vector<double> &y, const std::vector<double> &z,
                   bool scatter = false);
Geometry3D surface3D(const RectilinearField &field, bool wireframe = false);
Geometry3D contourProjection3D(const ScalarField &field, const std::vector<double> &levels, double z);
void computeNormals(Geometry3D &geometry);

struct Material3D {
    std::array<float, 4> color{0.25f, 0.7f, 1, 1};
    ColorScale colorScale;
    bool scalarColors = false, lighting = true, cullBackFaces = false;
    double ambient = 0.25;
    // Lines are world-space capsules, points are spheres; independent of resolution.
    double radius = 0.008;
};
struct Object3D {
    std::shared_ptr<const Geometry3D> geometry;
    Material3D material;
    bool visible = true;
};
struct Annotation3D {
    Vec3 position;
    std::string text;
};
struct Pick3D {
    std::size_t object = 0, primitive = 0, sourceIndex = 0;
    Vec3 position, normal;
    std::array<double, 3> weights{1, 0, 0};
    double distance = 0;
};

class VolumeData;
struct VolumeLayer;
/** Scene snapshots share source arrays; submit mutations on the owning UI thread. */
struct Scene3D {
    std::vector<Object3D> objects;
    std::shared_ptr<const VolumeLayer> volume;
    Bounds3D bounds;
    bool showAxes = true, clipToBounds = true;
    Vec3 lightDirection{1, 2, 3};
};
struct ViewState3D {
    Camera3D camera;
    ColorScale colorScale;
    std::vector<Annotation3D> annotations;
};
// Versioned text format; no source arrays, pointers, or GPU resources are serialized.
std::string saveView3D(const ViewState3D &state);
ViewState3D restoreView3D(const std::string &text);

struct RenderSettings3D {
    std::size_t workingBytes = 128 * 1024 * 1024;
    std::size_t maxFragmentsPerRay = 4096;
    // World-space sampling distance, opacity correction uses VolumeLayer::referenceStep.
    double volumeStep = 0.04;
    std::size_t maxStepsPerRay = 100000;
    std::array<float, 4> background{0.04f, 0.05f, 0.07f, 1};
};
struct Rendered3D {
    std::uint32_t width = 0, height = 0;
    std::vector<std::uint8_t> rgba;
    std::vector<double> depth;
    double milliseconds = 0;
    std::size_t rays = 0, volumeSamples = 0;
};
/** Reference ray renderer with BVH geometry, exact surface ordering and sampled volume integration.
 * No graphics context required. Capacity errors throw length_error; never truncate fragments.
 */
class SceneRenderer3D {
  public:
    explicit SceneRenderer3D(Scene3D scene, RenderSettings3D settings = {});
    ~SceneRenderer3D();
    SceneRenderer3D(SceneRenderer3D &&) noexcept;
    SceneRenderer3D &operator=(SceneRenderer3D &&) noexcept;
    Rendered3D render(const Camera3D &camera, std::uint32_t width, std::uint32_t height) const;
    std::optional<Pick3D> pick(const Camera3D &camera, Point pixel, double width, double height) const;

  private:
    friend class GpuSceneRenderer3D;
    detail::GpuSceneData gpuData() const;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace modules::plot
