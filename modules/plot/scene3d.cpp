#include "modules/plot/scene3d.h"
#include "modules/plot/contour.h"
#include "modules/plot/volume.h"
#include "modules/plot/gpu_scene3d.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <stdexcept>

namespace modules::plot {
Vec3 operator+(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 operator-(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 operator*(Vec3 a, double b) { return {a.x * b, a.y * b, a.z * b}; }
Vec3 operator/(Vec3 a, double b) { return a * (1 / b); }
double dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
double length(Vec3 a) { return std::hypot(a.x, a.y, a.z); }
bool finite(Vec3 a) { return std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z); }
Vec3 normalized(Vec3 a) {
    const double n = length(a);
    if (!std::isfinite(n) || n <= 0)
        throw std::invalid_argument("plot: invalid direction");
    return a / n;
}
void Bounds3D::validate() const {
    if (!finite(min) || !finite(max) || max.x <= min.x || max.y <= min.y || max.z <= min.z ||
        !finite(max - min))
        throw std::invalid_argument("plot: invalid 3D bounds");
}
bool Bounds3D::contains(Vec3 p) const {
    return finite(p) && p.x >= min.x && p.x <= max.x && p.y >= min.y && p.y <= max.y && p.z >= min.z &&
           p.z <= max.z;
}
namespace {
constexpr double pi = 3.14159265358979323846;
Vec3 backward(const Camera3D &c) {
    return {std::cos(c.pitch) * std::sin(c.yaw), std::sin(c.pitch), std::cos(c.pitch) * std::cos(c.yaw)};
}
Vec3 right(const Camera3D &c) { return {std::cos(c.yaw), 0, -std::sin(c.yaw)}; }
Vec3 up(const Camera3D &c) { return cross(backward(c), right(c)); }
void dimensions(double w, double h) {
    if (!std::isfinite(w) || !std::isfinite(h) || w <= 0 || h <= 0)
        throw std::invalid_argument("plot: invalid viewport");
}
void checkColor(const std::array<float, 4> &c) {
    for (auto v : c)
        if (!std::isfinite(v) || v < 0 || v > 1)
            throw std::invalid_argument("plot: invalid RGBA");
}
} // namespace
void Camera3D::validate() const {
    if (!finite(target) || !std::isfinite(yaw) || !std::isfinite(pitch) || std::abs(pitch) >= pi / 2 ||
        !std::isfinite(distance) || distance <= 0 || !std::isfinite(verticalSpan) || verticalSpan <= 0 ||
        !std::isfinite(fieldOfView) || fieldOfView <= 0 || fieldOfView >= 179 || !std::isfinite(nearPlane) ||
        !std::isfinite(farPlane) || nearPlane <= 0 || farPlane <= nearPlane ||
        (projection != Projection3D::Orthographic && projection != Projection3D::Perspective))
        throw std::invalid_argument("plot: invalid camera");
}
Vec3 Camera3D::eye() const { return target + backward(*this) * distance; }
Ray3D Camera3D::ray(Point pixel, double width, double height) const {
    validate();
    dimensions(width, height);
    if (!std::isfinite(pixel.x) || !std::isfinite(pixel.y))
        throw std::invalid_argument("plot: invalid pixel");
    const double x = (2 * pixel.x / width - 1) * width / height, y = 1 - 2 * pixel.y / height;
    if (projection == Projection3D::Orthographic)
        return {eye() + right(*this) * (x * verticalSpan / 2) + up(*this) * (y * verticalSpan / 2),
                backward(*this) * -1};
    const double scale = std::tan(fieldOfView * pi / 360);
    return {eye(), normalized(right(*this) * (x * scale) + up(*this) * (y * scale) - backward(*this))};
}
std::optional<Projected3D> Camera3D::project(Vec3 p, double w, double h) const {
    validate();
    dimensions(w, h);
    if (!finite(p))
        return {};
    const Vec3 relative = p - eye();
    const double depth = -dot(relative, backward(*this));
    if (depth < nearPlane || depth > farPlane)
        return {};
    const double half = projection == Projection3D::Orthographic ? verticalSpan / 2
                                                                 : depth * std::tan(fieldOfView * pi / 360);
    Point pixel{w / 2 + dot(relative, right(*this)) * h / (2 * half),
                h / 2 - dot(relative, up(*this)) * h / (2 * half)};
    if (pixel.x < 0 || pixel.y < 0 || pixel.x >= w || pixel.y >= h)
        return {};
    return Projected3D{pixel, depth};
}
void Camera3D::orbit(double x, double y) {
    if (!std::isfinite(x) || !std::isfinite(y))
        throw std::invalid_argument("plot: invalid orbit");
    yaw = std::remainder(yaw + x, 2 * pi);
    pitch = std::clamp(pitch + y, -pi / 2 + 0.001, pi / 2 - 0.001);
}
void Camera3D::pan(double x, double y) {
    if (!std::isfinite(x) || !std::isfinite(y))
        throw std::invalid_argument("plot: invalid pan");
    const double span = projection == Projection3D::Orthographic
                            ? verticalSpan
                            : 2 * distance * std::tan(fieldOfView * pi / 360);
    const auto next = target + right(*this) * (x * span) + up(*this) * (y * span);
    if (!finite(next))
        throw std::invalid_argument("plot: camera overflow");
    target = next;
}
void Camera3D::zoom(double factor) {
    if (!std::isfinite(factor) || factor <= 0)
        throw std::invalid_argument("plot: invalid zoom");
    auto next = *this;
    if (projection == Projection3D::Orthographic)
        next.verticalSpan *= factor;
    else
        next.distance *= factor;
    next.validate();
    *this = next;
}
void Camera3D::fit(Bounds3D b) {
    b.validate();
    target = b.min + (b.max - b.min) / 2;
    const double diagonal = length(b.max - b.min);
    verticalSpan = diagonal * 1.2;
    distance = diagonal / (2 * std::tan(fieldOfView * pi / 360)) * 1.3;
    nearPlane = diagonal * 0.0001;
    farPlane = distance + diagonal * 3;
    validate();
}
void Geometry3D::validate() const {
    const auto n = positions.size();
    if ((!normals.empty() && normals.size() != n) || (!scalars.empty() && scalars.size() != n) ||
        (!colors.empty() && colors.size() != n) || (!sourceIndices.empty() && sourceIndices.size() != n))
        throw std::invalid_argument("plot: geometry attribute length mismatch");
    for (auto normal : normals)
        if (!finite(normal))
            throw std::invalid_argument("plot: nonfinite normal");
    for (const auto &c : colors)
        checkColor(c);
    for (auto t : triangles)
        for (auto i : t)
            if (i >= n)
                throw std::out_of_range("plot: triangle index");
    for (auto t : lines)
        for (auto i : t)
            if (i >= n)
                throw std::out_of_range("plot: line index");
    if (primitive != Primitive3D::Triangles && primitive != Primitive3D::Lines &&
        primitive != Primitive3D::Points)
        throw std::invalid_argument("plot: unknown 3D primitive");
}
std::size_t Geometry3D::bytes() const {
    return positions.capacity() * sizeof(Vec3) + normals.capacity() * sizeof(Vec3) +
           scalars.capacity() * sizeof(double) + colors.capacity() * sizeof(std::array<float, 4>) +
           triangles.capacity() * sizeof(triangles[0]) + lines.capacity() * sizeof(lines[0]) +
           sourceIndices.capacity() * sizeof(std::size_t);
}
Geometry3D curve3D(const std::vector<double> &x, const std::vector<double> &y, const std::vector<double> &z,
                   bool scatter) {
    if (x.size() != y.size() || x.size() != z.size())
        throw std::invalid_argument("plot: XYZ length mismatch");
    Geometry3D g;
    g.primitive = scatter ? Primitive3D::Points : Primitive3D::Lines;
    for (std::size_t i = 0; i < x.size(); ++i) {
        g.positions.push_back({x[i], y[i], z[i]});
        if (i && !scatter)
            g.lines.push_back({i - 1, i});
    }
    return g;
}
void computeNormals(Geometry3D &g) {
    g.validate();
    g.normals.assign(g.positions.size(), {});
    for (auto t : g.triangles) {
        if (!finite(g.positions[t[0]]) || !finite(g.positions[t[1]]) || !finite(g.positions[t[2]]))
            continue;
        auto n = cross(g.positions[t[1]] - g.positions[t[0]], g.positions[t[2]] - g.positions[t[0]]);
        for (auto i : t)
            g.normals[i] = g.normals[i] + n;
    }
    for (auto &n : g.normals) {
        const auto size = length(n);
        if (size > 0 && std::isfinite(size))
            n = n / size;
        else
            n = {0, 0, 1};
    }
}
Geometry3D surface3D(const RectilinearField &f, bool wireframe) {
    Geometry3D g;
    g.primitive = wireframe ? Primitive3D::Lines : Primitive3D::Triangles;
    for (std::size_t y = 0; y < f.rows(); ++y)
        for (std::size_t x = 0; x < f.columns(); ++x) {
            const auto i = g.positions.size();
            const auto z = f.value(y, x);
            g.positions.push_back({f.x()[x], f.y()[y], z});
            g.scalars.push_back(z);
            if (wireframe) {
                if (x)
                    g.lines.push_back({i - 1, i});
                if (y)
                    g.lines.push_back({i - f.columns(), i});
            } else if (x && y) {
                const auto a = i - f.columns() - 1, b = i - f.columns(), c = i - 1;
                g.triangles.push_back({a, b, i});
                g.triangles.push_back({a, i, c});
            }
        }
    if (!wireframe)
        computeNormals(g);
    return g;
}
Geometry3D contourProjection3D(const ScalarField &f, const std::vector<double> &levels, double z) {
    if (!std::isfinite(z))
        throw std::invalid_argument("plot: invalid contour plane");
    Geometry3D g;
    g.primitive = Primitive3D::Lines;
    for (const auto &contour : marchingSquares(f, levels))
        for (const auto &line : contour.segments) {
            const auto i = g.positions.size();
            g.positions.push_back({line.from.x, line.from.y, z});
            g.positions.push_back({line.to.x, line.to.y, z});
            g.scalars.push_back(contour.level);
            g.scalars.push_back(contour.level);
            g.lines.push_back({i, i + 1});
        }
    return g;
}

std::string saveView3D(const ViewState3D &state) {
    state.camera.validate();
    const auto &c = state.camera;
    const auto &s = state.colorScale;
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(17);
    out << "EUI_PLOT_VIEW 1\n"
        << c.target.x << ' ' << c.target.y << ' ' << c.target.z << ' ' << c.yaw << ' ' << c.pitch << ' '
        << c.distance << ' ' << c.verticalSpan << ' ' << c.fieldOfView << ' ' << c.nearPlane << ' '
        << c.farPlane << ' ' << int(c.projection) << '\n';
    out << int(s.colorMap()) << ' ' << int(s.mode()) << ' ' << s.range().min << ' ' << s.range().max << ' '
        << s.automatic() << ' ' << s.discreteLevels();
    for (auto v : s.missingColor())
        out << ' ' << v;
    out << '\n' << state.annotations.size() << '\n';
    for (const auto &a : state.annotations) {
        if (!finite(a.position))
            throw std::invalid_argument("plot: invalid annotation");
        out << a.position.x << ' ' << a.position.y << ' ' << a.position.z << ' ' << std::quoted(a.text)
            << '\n';
    }
    return out.str();
}
ViewState3D restoreView3D(const std::string &text) {
    std::istringstream in(text);
    in.imbue(std::locale::classic());
    ViewState3D state;
    std::string magic;
    int version = 0, projection = 0, map = 0, mode = 0;
    bool automatic = false;
    std::size_t discrete = 0, count = 0;
    Range range;
    std::array<float, 4> missing{};
    auto &c = state.camera;
    in >> magic >> version >> c.target.x >> c.target.y >> c.target.z >> c.yaw >> c.pitch >> c.distance >>
        c.verticalSpan >> c.fieldOfView >> c.nearPlane >> c.farPlane >> projection >> map >> mode >>
        range.min >> range.max >> automatic >> discrete;
    for (auto &v : missing)
        in >> v;
    in >> count;
    if (!in || magic != "EUI_PLOT_VIEW" || version != 1 || projection < 0 || projection > 1 || map < 0 ||
        map > 4 || mode < 0 || mode > 1 || count > 100000)
        throw std::invalid_argument("plot: invalid saved view");
    c.projection = static_cast<Projection3D>(projection);
    c.validate();
    auto &s = state.colorScale;
    s.setColorMap(static_cast<ColorMap>(map));
    s.setMode(static_cast<ColorScaleMode>(mode));
    s.setRange(range);
    if (automatic)
        s.resetAuto();
    s.setDiscreteLevels(discrete);
    s.setMissingColor(missing);
    for (std::size_t i = 0; i < count; ++i) {
        Annotation3D a;
        in >> a.position.x >> a.position.y >> a.position.z >> std::quoted(a.text);
        if (!in || !finite(a.position))
            throw std::invalid_argument("plot: invalid saved annotation");
        state.annotations.push_back(std::move(a));
    }
    in >> std::ws;
    if (!in.eof())
        throw std::invalid_argument("plot: trailing saved view data");
    return state;
}

namespace {
struct Primitive {
    Bounds3D box;
    std::size_t object, index;
};
struct BvhNode {
    Bounds3D box;
    std::size_t first = 0, count = 0, left = 0, right = 0;
};
struct Hit {
    Pick3D pick;
    std::array<float, 4> color;
};
Bounds3D unionBox(Bounds3D a, Bounds3D b) {
    for (int i = 0; i < 3; ++i) {
        a.min[i] = std::min(a.min[i], b.min[i]);
        a.max[i] = std::max(a.max[i], b.max[i]);
    }
    return a;
}
bool intersectBox(const Ray3D &r, Bounds3D b, double &lo, double &hi) {
    for (int i = 0; i < 3; ++i) {
        if (std::abs(r.direction[i]) < 1e-300) {
            if (r.origin[i] < b.min[i] || r.origin[i] > b.max[i])
                return false;
            continue;
        }
        double a = (b.min[i] - r.origin[i]) / r.direction[i], z = (b.max[i] - r.origin[i]) / r.direction[i];
        if (a > z)
            std::swap(a, z);
        lo = std::max(lo, a);
        hi = std::min(hi, z);
        if (lo > hi)
            return false;
    }
    return true;
}
double sphere(const Ray3D &r, Vec3 center, double radius) {
    const auto oc = r.origin - center;
    const double b = dot(oc, r.direction), c = dot(oc, oc) - radius * radius, h = b * b - c;
    if (h < 0)
        return -1;
    const double a = -b - std::sqrt(h);
    return a >= 0 ? a : -b + std::sqrt(h);
}
double capsule(const Ray3D &r, Vec3 a, Vec3 b, double radius, double &weight) {
    const auto ba = b - a, oa = r.origin - a;
    const double baba = dot(ba, ba);
    if (baba <= 1e-30) {
        weight = 0;
        return sphere(r, a, radius);
    }
    const double bard = dot(ba, r.direction), baoa = dot(ba, oa), rdoa = dot(r.direction, oa),
                 oaoa = dot(oa, oa);
    const double A = baba - bard * bard, B = baba * rdoa - baoa * bard,
                 C = baba * oaoa - baoa * baoa - radius * radius * baba;
    const double h = B * B - A * C;
    if (h >= 0 && A > 1e-30) {
        const double t = (-B - std::sqrt(h)) / A, y = baoa + t * bard;
        if (t >= 0 && y >= 0 && y <= baba) {
            weight = y / baba;
            return t;
        }
    }
    const double ta = sphere(r, a, radius), tb = sphere(r, b, radius);
    if (ta >= 0 && (tb < 0 || ta <= tb)) {
        weight = 0;
        return ta;
    }
    weight = 1;
    return tb;
}
} // namespace

struct SceneRenderer3D::Impl {
    Scene3D scene;
    RenderSettings3D settings;
    std::vector<Primitive> primitives;
    std::vector<BvhNode> nodes;
    std::size_t originalObjects = 0, workingBytes = 0;

    std::size_t build(std::size_t first, std::size_t count) {
        const auto index = nodes.size();
        nodes.push_back({});
        auto box = primitives[first].box;
        for (std::size_t i = first + 1; i < first + count; ++i)
            box = unionBox(box, primitives[i].box);
        nodes[index].box = box;
        if (count <= 8) {
            nodes[index].first = first;
            nodes[index].count = count;
            return index;
        }
        const auto extent = box.max - box.min;
        int axis = extent.x > extent.y ? 0 : 1;
        if (extent.z > extent[axis])
            axis = 2;
        const auto middle = first + count / 2;
        std::nth_element(primitives.begin() + first, primitives.begin() + middle,
                         primitives.begin() + first + count, [axis](const Primitive &a, const Primitive &b) {
                             return a.box.min[axis] / 2 + a.box.max[axis] / 2 <
                                    b.box.min[axis] / 2 + b.box.max[axis] / 2;
                         });
        const auto left = build(first, count / 2), rightNode = build(middle, count - count / 2);
        nodes[index].left = left;
        nodes[index].right = rightNode;
        return index;
    }
    std::optional<Hit> intersect(const Primitive &p, const Ray3D &ray, double lo, double hi) const {
        const auto &object = scene.objects[p.object];
        const auto &g = *object.geometry;
        const auto &m = object.material;
        std::array<std::size_t, 3> indices{};
        std::array<double, 3> weights{1, 0, 0};
        Vec3 normal;
        double t = -1;
        if (g.primitive == Primitive3D::Triangles) {
            indices = g.triangles[p.index];
            const auto a = g.positions[indices[0]], e1 = g.positions[indices[1]] - a,
                       e2 = g.positions[indices[2]] - a;
            const auto h = cross(ray.direction, e2);
            const double det = dot(e1, h);
            const double tolerance = length(e1) * length(e2) * 1e-12;
            if (std::abs(det) <= tolerance || (m.cullBackFaces && det <= tolerance))
                return {};
            const auto s = ray.origin - a;
            const double u = dot(s, h) / det;
            if (u < 0 || u > 1)
                return {};
            const auto q = cross(s, e1);
            const double v = dot(ray.direction, q) / det;
            if (v < 0 || u + v > 1)
                return {};
            t = dot(e2, q) / det;
            weights = {1 - u - v, u, v};
            normal = normalized(cross(e1, e2));
            if (!g.normals.empty()) {
                const auto n = g.normals[indices[0]] * weights[0] + g.normals[indices[1]] * u +
                               g.normals[indices[2]] * v;
                if (length(n) > 0)
                    normal = normalized(n);
            }
        } else if (g.primitive == Primitive3D::Lines) {
            indices = {g.lines[p.index][0], g.lines[p.index][1], g.lines[p.index][1]};
            double w = 0;
            const auto a = g.positions[indices[0]], b = g.positions[indices[1]];
            t = capsule(ray, a, b, m.radius, w);
            weights = {1 - w, w, 0};
            const auto n = ray.origin + ray.direction * t - (a + (b - a) * w);
            if (length(n) > 0)
                normal = normalized(n);
        } else {
            indices = {p.index, p.index, p.index};
            t = sphere(ray, g.positions[p.index], m.radius);
            const auto n = ray.origin + ray.direction * t - g.positions[p.index];
            if (length(n) > 0)
                normal = normalized(n);
        }
        if (t < lo || t > hi)
            return {};
        if (scene.clipToBounds && p.object < originalObjects &&
            !scene.bounds.contains(ray.origin + ray.direction * t))
            return {};
        auto color = m.color;
        if (m.scalarColors && !g.scalars.empty()) {
            double scalar = 0;
            bool valid = true;
            for (int j = 0; j < 3; ++j)
                if (weights[j] > 0) {
                    valid = valid && std::isfinite(g.scalars[indices[j]]);
                    scalar += g.scalars[indices[j]] * weights[j];
                }
            const auto c = m.colorScale.map(valid ? scalar : std::numeric_limits<double>::quiet_NaN());
            for (int j = 0; j < 4; ++j)
                color[j] *= c[j];
        } else if (!g.colors.empty()) {
            for (int j = 0; j < 4; ++j) {
                double c = 0;
                for (int k = 0; k < 3; ++k)
                    c += g.colors[indices[k]][j] * weights[k];
                color[j] *= float(c);
            }
        }
        if (color[3] <= 0)
            return {};
        if (m.lighting) {
            const double light =
                m.ambient + (1 - m.ambient) * std::abs(dot(normal, normalized(scene.lightDirection)));
            for (int j = 0; j < 3; ++j)
                color[j] *= float(light);
        }
        const auto vertex =
            indices[std::size_t(std::max_element(weights.begin(), weights.end()) - weights.begin())];
        return Hit{{p.object, p.index, g.sourceIndices.empty() ? vertex : g.sourceIndices[vertex],
                    ray.origin + ray.direction * t, normal, weights, t},
                   color};
    }
    void traverse(std::size_t index, const Ray3D &ray, double lo, double hi, std::vector<Hit> &hits) const {
        const auto &n = nodes[index];
        double a = lo, b = hi;
        if (!intersectBox(ray, n.box, a, b))
            return;
        if (n.count)
            for (std::size_t i = n.first; i < n.first + n.count; ++i) {
                auto hit = intersect(primitives[i], ray, lo, hi);
                if (hit) {
                    if (hits.size() >= settings.maxFragmentsPerRay)
                        throw std::length_error("plot: fragment budget exceeded");
                    hits.push_back(*hit);
                }
            }
        else {
            traverse(n.left, ray, lo, hi, hits);
            traverse(n.right, ray, lo, hi, hits);
        }
    }
    void hits(const Ray3D &ray, double lo, double hi, std::vector<Hit> &result) const {
        result.clear();
        if (!nodes.empty())
            traverse(0, ray, lo, hi, result);
        std::sort(result.begin(), result.end(), [](const Hit &a, const Hit &b) {
            if (a.pick.distance != b.pick.distance)
                return a.pick.distance < b.pick.distance;
            return a.pick.object < b.pick.object ||
                   (a.pick.object == b.pick.object && a.pick.primitive < b.pick.primitive);
        });
        // Shared triangle edges represent one surface, not two alpha layers.
        result.erase(std::unique(result.begin(), result.end(),
                                 [](const Hit &a, const Hit &b) {
                                     return a.pick.object == b.pick.object &&
                                            std::abs(a.pick.distance - b.pick.distance) <=
                                                1e-10 * std::max(1.0, std::abs(a.pick.distance));
                                 }),
                     result.end());
    }
};

SceneRenderer3D::SceneRenderer3D(Scene3D scene, RenderSettings3D settings) : impl_(std::make_unique<Impl>()) {
    scene.bounds.validate();
    normalized(scene.lightDirection);
    checkColor(settings.background);
    if (settings.maxFragmentsPerRay == 0 ||
        settings.maxFragmentsPerRay > settings.workingBytes / sizeof(Hit) ||
        !std::isfinite(settings.volumeStep) || settings.volumeStep <= 0 || !settings.maxStepsPerRay)
        throw std::invalid_argument("plot: invalid render settings");
    if (scene.volume && (!scene.volume->data || !std::isfinite(scene.volume->referenceStep) ||
                         scene.volume->referenceStep <= 0))
        throw std::invalid_argument("plot: invalid volume layer");
    impl_->originalObjects = scene.objects.size();
    impl_->settings = settings;
    if (scene.showAxes) {
        Geometry3D box;
        box.primitive = Primitive3D::Lines;
        for (int i = 0; i < 8; ++i)
            box.positions.push_back({i & 1 ? scene.bounds.max.x : scene.bounds.min.x,
                                     i & 2 ? scene.bounds.max.y : scene.bounds.min.y,
                                     i & 4 ? scene.bounds.max.z : scene.bounds.min.z});
        for (std::size_t i = 0; i < 8; ++i)
            for (std::size_t bit : {1u, 2u, 4u})
                if (!(i & bit))
                    box.lines.push_back({i, i | bit});
        for (int dimension = 0; dimension < 3; ++dimension) {
            Axis axis;
            axis.setRange({scene.bounds.min[dimension], scene.bounds.max[dimension]});
            for (const auto &tick : axis.ticks(4))
                if (tick.major) {
                    auto a = scene.bounds.min;
                    a[dimension] = tick.value;
                    auto b = a;
                    const int perpendicular = (dimension + 1) % 3;
                    b[perpendicular] +=
                        (scene.bounds.max[perpendicular] - scene.bounds.min[perpendicular]) * 0.025;
                    const auto first = box.positions.size();
                    box.positions.push_back(a);
                    box.positions.push_back(b);
                    box.lines.push_back({first, first + 1});
                }
        }
        Material3D material;
        material.color = {0.5f, 0.55f, 0.65f, 1};
        material.lighting = false;
        material.radius = length(scene.bounds.max - scene.bounds.min) * 0.0045;
        scene.objects.push_back({std::make_shared<Geometry3D>(std::move(box)), material, true});
    }
    impl_->scene = std::move(scene);
    std::size_t count = 0;
    for (auto &object : impl_->scene.objects) {
        if (!object.geometry || !object.visible)
            continue;
        const auto &g = *object.geometry;
        g.validate();
        auto &m = object.material;
        checkColor(m.color);
        if (!std::isfinite(m.radius) || m.radius <= 0 || !std::isfinite(m.ambient) || m.ambient < 0 ||
            m.ambient > 1)
            throw std::invalid_argument("plot: invalid material");
        if (m.scalarColors && m.colorScale.automatic()) {
            std::optional<Range> range;
            for (auto v : g.scalars)
                if (std::isfinite(v) && (m.colorScale.mode() != ColorScaleMode::Log10 || v > 0)) {
                    if (!range)
                        range = Range{v, v};
                    else {
                        range->min = std::min(range->min, v);
                        range->max = std::max(range->max, v);
                    }
                }
            m.colorScale.fit(range);
        }
        const auto n = g.primitive == Primitive3D::Triangles ? g.triangles.size()
                       : g.primitive == Primitive3D::Lines   ? g.lines.size()
                                                             : g.positions.size();
        if (g.bytes() > settings.workingBytes - impl_->workingBytes)
            throw std::length_error("plot: scene data budget exceeded");
        impl_->workingBytes += g.bytes();
        if (n > (settings.workingBytes - impl_->workingBytes) / (sizeof(Primitive) + 2 * sizeof(BvhNode)))
            throw std::length_error("plot: acceleration budget exceeded");
        impl_->workingBytes += n * (sizeof(Primitive) + 2 * sizeof(BvhNode));
        count += n;
    }
    const auto fragmentBytes = settings.maxFragmentsPerRay * sizeof(Hit);
    if (fragmentBytes > settings.workingBytes - impl_->workingBytes)
        throw std::length_error("plot: fragment storage budget exceeded");
    impl_->workingBytes += fragmentBytes;
    impl_->primitives.reserve(count);
    impl_->nodes.reserve(count * 2);
    for (std::size_t object = 0; object < impl_->scene.objects.size(); ++object) {
        const auto &o = impl_->scene.objects[object];
        if (!o.geometry || !o.visible)
            continue;
        const auto &g = *o.geometry;
        const auto n = g.primitive == Primitive3D::Triangles ? g.triangles.size()
                       : g.primitive == Primitive3D::Lines   ? g.lines.size()
                                                             : g.positions.size();
        for (std::size_t i = 0; i < n; ++i) {
            std::array<std::size_t, 3> indices =
                g.primitive == Primitive3D::Triangles ? g.triangles[i]
                : g.primitive == Primitive3D::Lines
                    ? std::array<std::size_t, 3>{g.lines[i][0], g.lines[i][1], g.lines[i][1]}
                    : std::array<std::size_t, 3>{i, i, i};
            Bounds3D b{g.positions[indices[0]], g.positions[indices[0]]};
            bool valid = true;
            for (auto index : indices) {
                const auto p = g.positions[index];
                if (!finite(p)) {
                    valid = false;
                    break;
                }
                b = unionBox(b, {p, p});
            }
            if (!valid)
                continue;
            if (g.primitive != Primitive3D::Triangles) {
                Vec3 r{o.material.radius, o.material.radius, o.material.radius};
                b.min = b.min - r;
                b.max = b.max + r;
            }
            impl_->primitives.push_back({b, object, i});
        }
    }
    if (!impl_->primitives.empty())
        impl_->build(0, impl_->primitives.size());
}
SceneRenderer3D::~SceneRenderer3D() = default;
detail::GpuSceneData SceneRenderer3D::gpuData() const {
    detail::GpuSceneData out;
    const auto &s = *impl_;
    out.scene = s.scene;
    out.settings = s.settings;
    out.origin = s.scene.bounds.min + (s.scene.bounds.max - s.scene.bounds.min) / 2;
    out.scale = length(s.scene.bounds.max - s.scene.bounds.min);
    const auto vector = [&](Vec3 p, float w) {
        return std::array<float, 4>{float(p.x), float(p.y), float(p.z), w};
    };
    for (const auto &n : s.nodes) {
        out.nodes.push_back(vector((n.box.min - out.origin) / out.scale, 0));
        out.nodes.push_back(vector((n.box.max - out.origin) / out.scale, 0));
        out.nodes.push_back({float(n.first), float(n.count), float(n.left), float(n.right)});
    }
    for (const auto &p : s.primitives) {
        const auto &o = s.scene.objects[p.object];
        const auto &g = *o.geometry;
        const auto &m = o.material;
        const auto indices =
            g.primitive == Primitive3D::Triangles ? g.triangles[p.index]
            : g.primitive == Primitive3D::Lines
                ? std::array<std::size_t, 3>{g.lines[p.index][0], g.lines[p.index][1], g.lines[p.index][1]}
                : std::array<std::size_t, 3>{p.index, p.index, p.index};
        out.primitives.push_back(
            vector((g.positions[indices[0]] - out.origin) / out.scale, float(g.primitive)));
        out.primitives.push_back(
            vector((g.positions[indices[1]] - out.origin) / out.scale, float(m.radius / out.scale)));
        out.primitives.push_back(vector((g.positions[indices[2]] - out.origin) / out.scale, float(p.object)));
        for (auto i : indices) {
            const auto local = (g.positions[i] - out.origin) / out.scale;
            for (int axis = 0; axis < 3; ++axis)
                out.representable = out.representable && std::isfinite(float(local[axis]));
        }
        Vec3 face{0, 0, 1};
        if (g.primitive == Primitive3D::Triangles) {
            auto n = cross(g.positions[indices[1]] - g.positions[indices[0]],
                           g.positions[indices[2]] - g.positions[indices[0]]);
            if (length(n) > 0)
                face = normalized(n);
        }
        out.primitives.push_back(
            vector(g.normals.empty() ? face : g.normals[indices[0]], m.lighting ? 1.f : 0.f));
        out.primitives.push_back(vector(g.normals.empty() ? face : g.normals[indices[1]], float(m.ambient)));
        out.primitives.push_back(
            vector(g.normals.empty() ? face : g.normals[indices[2]], m.cullBackFaces ? 1.f : 0.f));
        const bool scalar = m.scalarColors && !g.scalars.empty();
        for (auto i : indices) {
            auto c = m.color;
            if (!scalar && !g.colors.empty())
                for (int j = 0; j < 4; ++j)
                    c[j] *= g.colors[i][j];
            out.primitives.push_back(c);
        }
        const auto range = m.colorScale.range();
        std::array<float, 4> values{0, 0, 0, scalar ? 1.f : 0.f};
        if (scalar)
            for (int j = 0; j < 3; ++j)
                values[j] = float((g.scalars[indices[j]] - range.min) / (range.max - range.min));
        if (scalar)
            for (int j = 0; j < 3; ++j)
                if (std::isfinite(g.scalars[indices[j]]) && !std::isfinite(values[j]))
                    out.representable = false;
        out.primitives.push_back(values);
        const float ratio =
            m.colorScale.mode() == ColorScaleMode::Log10 ? float(range.min / range.max) : -1.f;
        if (scalar && m.colorScale.mode() == ColorScaleMode::Log10 && !(ratio > 0 && ratio < 1))
            out.representable = false;
        out.primitives.push_back({float(m.colorScale.colorMap()), float(m.colorScale.discreteLevels()), ratio,
                                  s.scene.clipToBounds && p.object < s.originalObjects ? 1.f : 0.f});
        auto missing = m.colorScale.missingColor();
        for (int j = 0; j < 4; ++j)
            missing[j] *= m.color[j];
        out.primitives.push_back(missing);
    }
    return out;
}
SceneRenderer3D::SceneRenderer3D(SceneRenderer3D &&) noexcept = default;
SceneRenderer3D &SceneRenderer3D::operator=(SceneRenderer3D &&) noexcept = default;

Rendered3D SceneRenderer3D::render(const Camera3D &camera, std::uint32_t w, std::uint32_t h) const {
    camera.validate();
    dimensions(w, h);
    const auto &s = *impl_;
    const auto &settings = s.settings;
    const auto available = settings.workingBytes - s.workingBytes;
    if (std::uint64_t(w) * h > available / (4 + sizeof(double)))
        throw std::length_error("plot: image working budget exceeded");
    Rendered3D result;
    result.width = w;
    result.height = h;
    result.rgba.resize(std::size_t(w) * h * 4);
    result.depth.assign(std::size_t(w) * h, std::numeric_limits<double>::infinity());
    std::vector<Hit> hits;
    hits.reserve(settings.maxFragmentsPerRay);
    const auto start = std::chrono::steady_clock::now();
    for (std::uint32_t y = 0; y < h; ++y)
        for (std::uint32_t x = 0; x < w; ++x) {
            const auto ray = camera.ray({x + 0.5, y + 0.5}, w, h);
            const double cosine = -dot(ray.direction, backward(camera));
            double lo = camera.nearPlane / cosine, hi = camera.farPlane / cosine;
            const auto index = std::size_t(y) * w + x;
            std::array<double, 3> rgb{};
            double transmission = 1;
            double depth = std::numeric_limits<double>::infinity();
            auto blend = [&](std::array<float, 4> color, double distance) {
                if (color[3] > 0 && !std::isfinite(depth))
                    depth = distance * cosine;
                for (int j = 0; j < 3; ++j)
                    rgb[j] += transmission * color[3] * color[j];
                transmission *= 1 - color[3];
            };
            {
                s.hits(ray, lo, hi, hits);
                double volumeStart = lo, volumeEnd = hi;
                bool hasVolume = false;
                if (s.scene.volume)
                    hasVolume =
                        intersectBox(ray, s.scene.volume->data->layout().bounds(), volumeStart, volumeEnd);
                if (hasVolume && s.scene.clipToBounds)
                    hasVolume = intersectBox(ray, s.scene.bounds, volumeStart, volumeEnd);
                std::size_t steps = 0;
                auto integrate = [&](double begin, double end) {
                    if (!hasVolume || transmission == 0)
                        return;
                    begin = std::max(begin, volumeStart);
                    end = std::min(end, volumeEnd);
                    if (begin >= end)
                        return;
                    const double samples = std::ceil((end - begin) / settings.volumeStep);
                    if (!std::isfinite(samples) || samples > double(settings.maxStepsPerRay - steps))
                        throw std::length_error("plot: volume ray step budget exceeded");
                    const auto count = std::size_t(samples);
                    const double step = (end - begin) / double(count);
                    for (std::size_t i = 0; i < count && transmission > 0; ++i) {
                        const double t = begin + (i + 0.5) * step;
                        const auto value = s.scene.volume->data->sample(ray.origin + ray.direction * t);
                        ++result.volumeSamples;
                        ++steps;
                        if (value) {
                            auto color = s.scene.volume->transfer.map(*value);
                            color[3] = float(-std::expm1(std::log1p(-double(color[3])) * step /
                                                         s.scene.volume->referenceStep));
                            blend(color, t);
                        }
                    }
                };
                double previous = lo;
                for (const auto &hit : hits) {
                    integrate(previous, hit.pick.distance);
                    blend(hit.color, hit.pick.distance);
                    previous = hit.pick.distance;
                    if (transmission == 0)
                        break;
                }
                integrate(previous, hi);
            }
            const double alpha = 1 - transmission + transmission * settings.background[3];
            for (int j = 0; j < 3; ++j) {
                rgb[j] += transmission * settings.background[3] * settings.background[j];
                result.rgba[index * 4 + j] =
                    std::uint8_t(std::lround(std::clamp(alpha > 0 ? rgb[j] / alpha : 0.0, 0.0, 1.0) * 255));
            }
            result.rgba[index * 4 + 3] = std::uint8_t(std::lround(alpha * 255));
            result.depth[index] = depth;
        }
    result.rays = std::size_t(w) * h;
    result.milliseconds =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    return result;
}
std::optional<Pick3D> SceneRenderer3D::pick(const Camera3D &camera, Point pixel, double w, double h) const {
    const auto ray = camera.ray(pixel, w, h);
    const double cosine = -dot(ray.direction, backward(camera));
    if (pixel.x < 0 || pixel.y < 0 || pixel.x >= w || pixel.y >= h)
        return {};
    double lo = camera.nearPlane / cosine, hi = camera.farPlane / cosine;
    std::vector<Hit> hits;
    hits.reserve(impl_->settings.maxFragmentsPerRay);
    impl_->hits(ray, lo, hi, hits);
    for (const auto &hit : hits)
        if (hit.pick.object < impl_->originalObjects)
            return hit.pick;
    return {};
}
} // namespace modules::plot
