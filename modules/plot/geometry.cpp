#include "modules/plot/geometry.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace modules::plot {
namespace {
bool inside(Point p, Viewport v) {
    return p.x >= v.x && p.y >= v.y && p.x <= v.x + v.width && p.y <= v.y + v.height;
}
bool clipLine(Point& a, Point& b, Viewport v) {
    const double dx = b.x - a.x, dy = b.y - a.y;
    if (!std::isfinite(dx) || !std::isfinite(dy))
        return false;
    double begin = 0, end = 1;
    const double p[] = {-dx, dx, -dy, dy};
    const double q[] = {a.x - v.x, v.x + v.width - a.x, a.y - v.y, v.y + v.height - a.y};
    for (int i = 0; i < 4; ++i) {
        if (p[i] == 0) {
            if (q[i] < 0)
                return false;
        } else {
            const double t = q[i] / p[i];
            if (p[i] < 0)
                begin = std::max(begin, t);
            else
                end = std::min(end, t);
            if (begin > end)
                return false;
        }
    }
    b = {a.x + end * dx, a.y + end * dy};
    a = {a.x + begin * dx, a.y + begin * dy};
    return true;
}
void polygon(std::vector<Vertex>& output, std::vector<Point> points, Viewport v) {
    // Sutherland-Hodgman 在 double 屏幕坐标中裁剪，避免生成范围外的 float 顶点。
    for (int side = 0; side < 4 && !points.empty(); ++side) {
        const auto distance = [&](Point p) {
            switch (side) {
            case 0:
                return p.x - v.x;
            case 1:
                return v.x + v.width - p.x;
            case 2:
                return p.y - v.y;
            default:
                return v.y + v.height - p.y;
            }
        };
        std::vector<Point> clipped;
        auto previous = points.back();
        double previousDistance = distance(previous);
        for (const auto current : points) {
            const double currentDistance = distance(current);
            if ((currentDistance >= 0) != (previousDistance >= 0)) {
                const double t = previousDistance / (previousDistance - currentDistance);
                clipped.push_back(
                    {previous.x + (current.x - previous.x) * t, previous.y + (current.y - previous.y) * t});
            }
            if (currentDistance >= 0)
                clipped.push_back(current);
            previous = current;
            previousDistance = currentDistance;
        }
        points = std::move(clipped);
    }
    const auto add = [&](Point p) {
        output.push_back({static_cast<float>(p.x - v.x), static_cast<float>(p.y - v.y)});
    };
    for (std::size_t i = 1; i + 1 < points.size(); ++i) {
        add(points[0]);
        add(points[i]);
        add(points[i + 1]);
    }
}
void marker(std::vector<Vertex>& output, Point p, double size, Marker shape, Viewport v);
void segment(std::vector<Vertex>& output, Point a, Point b, double width, Viewport v,
             LineCap cap = LineCap::Butt) {
    if (!clipLine(a, b, {v.x - width, v.y - width, v.width + 2 * width, v.height + 2 * width}))
        return;
    const double dx = b.x - a.x, dy = b.y - a.y, length = std::hypot(dx, dy);
    if (length == 0 || width == 0)
        return;
    if (cap == LineCap::Square) {
        const double ex = dx / length * width / 2, ey = dy / length * width / 2;
        a.x -= ex;
        a.y -= ey;
        b.x += ex;
        b.y += ey;
    }
    const double nx = -dy / length * width / 2, ny = dx / length * width / 2;
    const Point corners[] = {
        {a.x + nx, a.y + ny}, {b.x + nx, b.y + ny}, {b.x - nx, b.y - ny}, {a.x - nx, a.y - ny}};
    if (std::all_of(std::begin(corners), std::end(corners), [&](Point p) { return inside(p, v); })) {
        for (int i : {0, 1, 2, 0, 2, 3})
            output.push_back({float(corners[i].x - v.x), float(corners[i].y - v.y)});
    } else
        polygon(output, {std::begin(corners), std::end(corners)}, v);
    if (cap == LineCap::Round) {
        marker(output, a, width, Marker::Circle, v);
        marker(output, b, width, Marker::Circle, v);
    }
}
void patternedSegment(std::vector<Vertex>& output, Point a, Point b, const Style& style, Viewport v) {
    if (style.pattern == LinePattern::Solid) {
        segment(output, a, b, style.lineWidth, v, style.cap);
        return;
    }
    // 先裁剪避免不可见的超长线段生成无界虚线数；虚线相位以裁剪起点为零。
    if (!clipLine(a, b, v) || style.lineWidth == 0)
        return;
    const double length = std::hypot(b.x - a.x, b.y - a.y);
    const double dash = std::max(1.0, style.lineWidth) * (style.pattern == LinePattern::Dashed ? 5 : 1);
    for (double start = 0; start < length; start += dash * 2) {
        const double stop = std::min(length, start + dash);
        segment(output, {a.x + (b.x - a.x) * start / length, a.y + (b.y - a.y) * start / length},
                {a.x + (b.x - a.x) * stop / length, a.y + (b.y - a.y) * stop / length}, style.lineWidth, v,
                style.cap);
    }
}
void marker(std::vector<Vertex>& output, Point p, double size, Marker shape, Viewport v) {
    const double r = size / 2;
    if (shape == Marker::Square)
        polygon(output, {{p.x - r, p.y - r}, {p.x + r, p.y - r}, {p.x + r, p.y + r}, {p.x - r, p.y + r}}, v);
    else if (shape == Marker::Diamond)
        polygon(output, {{p.x, p.y - r}, {p.x + r, p.y}, {p.x, p.y + r}, {p.x - r, p.y}}, v);
    else if (shape == Marker::Cross) {
        segment(output, {p.x - r, p.y - r}, {p.x + r, p.y + r}, std::max(1.0, size / 5), v);
        segment(output, {p.x - r, p.y + r}, {p.x + r, p.y - r}, std::max(1.0, size / 5), v);
    } else {
        std::vector<Point> points;
        for (int i = 0; i < 16; ++i) {
            const double angle = i * 6.283185307179586 / 16;
            points.push_back({p.x + r * std::cos(angle), p.y + r * std::sin(angle)});
        }
        polygon(output, std::move(points), v);
    }
}
Data curveData(const Series& series) {
    if (series.curve == CurveKind::Raw || series.data.size() < 2)
        return series.data;
    std::vector<double> x;
    std::vector<double> y;
    x.reserve(series.data.size() * (series.curve == CurveKind::Interpolated ? 2 : 1));
    y.reserve(x.capacity());
    if (series.curve == CurveKind::Interpolated) {
        for (std::size_t i = 0; i + 1 < series.data.size(); ++i) {
            const auto a = series.data.at(i);
            const auto b = series.data.at(i + 1);
            x.push_back(a.x);
            y.push_back(a.y);
            if (std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(b.x) && std::isfinite(b.y)) {
                x.push_back((a.x + b.x) * 0.5);
                y.push_back((a.y + b.y) * 0.5);
            }
        }
        x.push_back(series.data.at(series.data.size() - 1).x);
        y.push_back(series.data.at(series.data.size() - 1).y);
    } else {
        for (std::size_t i = 0; i < series.data.size(); ++i) {
            const auto point = series.data.at(i);
            x.push_back(point.x);
            if (!std::isfinite(point.y)) {
                y.push_back(point.y);
                continue;
            }
            double sum = 0;
            int count = 0;
            for (std::size_t j = (i > 2 ? i - 2 : 0); j <= std::min(series.data.size() - 1, i + 2); ++j) {
                const auto neighbor = series.data.at(j);
                if (std::isfinite(neighbor.y)) {
                    sum += neighbor.y;
                    ++count;
                }
            }
            y.push_back(count ? sum / count : point.y);
        }
    }
    return Data(x, y);
}
} // namespace

std::vector<DisplayRun> selectDisplay(const Data& data, const Axes& axes, Viewport viewport) {
    std::vector<DisplayRun> runs;
    DisplayRun run;
    DisplayPoint first, last, low, high;
    bool occupied = false;
    double column = 0;
    const auto flush = [&] {
        if (!occupied)
            return;
        std::array<DisplayPoint, 4> selected{first, low, high, last};
        std::sort(selected.begin(), selected.end(),
                  [](const auto& a, const auto& b) { return a.index < b.index; });
        const auto end = std::unique(selected.begin(), selected.end(),
                                     [](const auto& a, const auto& b) { return a.index == b.index; });
        run.insert(run.end(), selected.begin(), end);
        occupied = false;
    };
    const auto consume = [&](std::size_t i) {
        const auto mapped = axes.toScreen(data.at(i), viewport);
        if (!mapped) {
            flush();
            if (!run.empty()) {
                runs.push_back(std::move(run));
                run.clear();
            }
            return;
        }
        const double next = std::floor(mapped->x - viewport.x);
        if (occupied && next != column)
            flush();
        column = next;
        const DisplayPoint point{*mapped, i};
        if (!occupied) {
            first = low = high = point;
            occupied = true;
        } else {
            if (mapped->y < low.screen.y)
                low = point;
            if (mapped->y > high.screen.y)
                high = point;
        }
        last = point;
    };
    std::size_t begin = 0, end = data.size();
    if (data.orderedX()) {
        const auto range = axes.x.range();
        begin = data.boundX(range.min);
        if (begin)
            --begin; // Keep the segment crossing the left boundary.
        end = data.boundX(range.max, true);
        if (end < data.size())
            ++end;
    }
    for (auto i = begin; i < end;) {
        // Ordered X lets us query extrema of an entire pixel column without visiting its samples.
        const auto x = axes.x.normalize(data.xAt(i));
        if (!data.orderedX() || !x || !std::isfinite(*x * viewport.width)) {
            consume(i++);
            continue;
        }
        const auto col = std::floor(*x * viewport.width);
        auto a = i + 1, b = end;
        while (a < b) {
            const auto m = a + (b - a) / 2;
            const auto mx = axes.x.normalize(data.xAt(m));
            if (mx && std::floor(*mx * viewport.width) == col)
                a = m + 1;
            else
                b = m;
        }
        const auto s = data.summary(i, a);
        if (s.valid == a - i && (axes.y.scale() == Scale::Linear || s.min.y > 0)) {
            std::array<std::size_t, 4> indices{i, s.minYIndex, s.maxYIndex, a - 1};
            std::sort(indices.begin(), indices.end());
            const auto stop = std::unique(indices.begin(), indices.end());
            for (auto it = indices.begin(); it != stop; ++it)
                consume(*it);
        } else {
            // Missing values and log-domain breaks must retain their exact run boundaries.
            for (auto j = i; j < a; ++j)
                consume(j);
        }
        i = a;
    }
    flush();
    if (!run.empty())
        runs.push_back(std::move(run));
    return runs;
}

std::vector<Vertex> tessellate(const Series& series, const Axes& axes, Viewport viewport) {
    const auto& style = series.style;
    if (!std::isfinite(viewport.width) || !std::isfinite(viewport.height) || !std::isfinite(viewport.x) ||
        !std::isfinite(viewport.y) || viewport.width <= 0 || viewport.height <= 0 || viewport.width > 32768 ||
        viewport.height > 32768)
        throw std::invalid_argument("plot: unsupported viewport dimensions");
    if (!std::isfinite(style.lineWidth) || style.lineWidth < 0 || style.lineWidth > 1024 ||
        !std::isfinite(style.markerSize) || style.markerSize < 0 || style.markerSize > 1024 ||
        !std::isfinite(style.barWidth) || style.barWidth <= 0 || !std::isfinite(style.baseline))
        throw std::invalid_argument("plot: invalid geometry style");
    for (float value : style.color)
        if (!std::isfinite(value) || value < 0 || value > 1)
            throw std::invalid_argument("plot: invalid color");
    switch (series.graph) {
    case Graph::Line:
    case Graph::Scatter:
    case Graph::Step:
    case Graph::Stem:
    case Graph::Bar:
    case Graph::Area:
    case Graph::ErrorBars:
    case Graph::Band:
        break;
    default:
        throw std::invalid_argument("plot: unknown graph kind");
    }
    switch (style.marker) {
    case Marker::Square:
    case Marker::Circle:
    case Marker::Diamond:
    case Marker::Cross:
        break;
    default:
        throw std::invalid_argument("plot: unknown marker");
    }
    switch (style.pattern) {
    case LinePattern::Solid:
    case LinePattern::Dashed:
    case LinePattern::Dotted:
        break;
    default:
        throw std::invalid_argument("plot: unknown line pattern");
    }
    switch (style.cap) {
    case LineCap::Butt:
    case LineCap::Square:
    case LineCap::Round:
        break;
    default:
        throw std::invalid_argument("plot: unknown line cap");
    }
    switch (style.join) {
    case LineJoin::Miter:
    case LineJoin::Bevel:
    case LineJoin::Round:
        break;
    default:
        throw std::invalid_argument("plot: unknown line join");
    }
    switch (series.curve) {
    case CurveKind::Raw:
    case CurveKind::Interpolated:
    case CurveKind::Fitted:
        break;
    default:
        throw std::invalid_argument("plot: unknown curve kind");
    }
    for (float value : style.missingColor)
        if (!std::isfinite(value) || value < 0 || value > 1)
            throw std::invalid_argument("plot: invalid missing color");
    if (!std::isfinite(style.colorRange.min) || !std::isfinite(style.colorRange.max) ||
        style.colorRange.min >= style.colorRange.max)
        throw std::invalid_argument("plot: invalid color range");
    std::vector<Vertex> output;
    if (!series.visible)
        return output;
    if (series.graph == Graph::ErrorBars)
        return errorBars(series.data, series.lower, series.upper, axes, viewport, style.markerSize,
                         style.lineWidth);
    if (series.graph == Graph::Band) {
        std::vector<double> x;
        x.reserve(series.data.size());
        for (std::size_t i = 0; i < series.data.size(); ++i)
            x.push_back(series.data.at(i).x);
        return band(x, series.lower, series.upper, axes, viewport);
    }
    if (series.graph == Graph::Bar && !series.baselines.empty())
        return bars(series.data, series.baselines, style.barWidth, axes, viewport);
    if (series.graph == Graph::Line || series.graph == Graph::Step || series.graph == Graph::Area) {
        const Data displayData = curveData(series);
        Series displaySeries = series;
        displaySeries.data = displayData;
        displaySeries.curve = CurveKind::Raw;
        for (const auto& run : selectDisplay(displayData, axes, viewport)) {
            for (std::size_t i = 1; i < run.size(); ++i) {
                const auto a = run[i - 1].screen, b = run[i].screen;
                if (series.graph == Graph::Step) {
                    patternedSegment(output, a, {b.x, a.y}, style, viewport);
                    patternedSegment(output, {b.x, a.y}, b, style, viewport);
                } else if (series.graph == Graph::Area) {
                    const auto baseline = axes.y.normalize(style.baseline);
                    if (baseline) {
                        const double y = viewport.y + (1 - *baseline) * viewport.height;
                        // 跨基线必须在交点拆开，否则三角形重叠导致半透明面积变深。
                        if ((a.y < y && b.y > y) || (a.y > y && b.y < y)) {
                            const double t = (y - a.y) / (b.y - a.y);
                            const Point crossing{a.x + (b.x - a.x) * t, y};
                            polygon(output, {a, crossing, {a.x, y}}, viewport);
                            polygon(output, {crossing, b, {b.x, y}}, viewport);
                        } else
                            polygon(output, {a, b, {b.x, y}, {a.x, y}}, viewport);
                    }
                } else
                    patternedSegment(output, a, b, style, viewport);
                if (style.join == LineJoin::Round)
                    marker(output, b, style.lineWidth, Marker::Circle, viewport);
            }
        }
        if (!style.markers && displayData.size() != 1)
            return output;
    }
    for (std::size_t i = 0; i < series.data.size(); ++i) {
        const auto point = series.data.at(i);
        const auto screen = axes.toScreen(point, viewport);
        if (!screen)
            continue;
        if (series.graph == Graph::Stem) {
            const auto base = axes.toScreen({point.x, style.baseline}, viewport);
            if (base)
                segment(output, *base, *screen, style.lineWidth, viewport, style.cap);
        }
        if (series.graph == Graph::Bar) {
            const auto a = axes.toScreen({point.x - style.barWidth / 2, style.baseline}, viewport);
            const auto b = axes.toScreen({point.x + style.barWidth / 2, point.y}, viewport);
            if (a && b)
                polygon(output, {*a, {b->x, a->y}, *b, {a->x, b->y}}, viewport);
        }
        if (series.graph == Graph::Scatter || style.markers || series.data.size() == 1)
            marker(output, *screen, style.markerSize, style.marker, viewport);
    }
    return output;
}

std::vector<Vertex> polarTessellate(const Series& series, const PolarAxes& axes, Viewport viewport) {
    if (series.graph != Graph::Line && series.graph != Graph::Scatter)
        throw std::invalid_argument("plot: polar geometry supports line and scatter only");
    std::vector<double> x;
    std::vector<double> y;
    x.reserve(series.data.size());
    y.reserve(series.data.size());
    for (std::size_t i = 0; i < series.data.size(); ++i) {
        const auto mapped = axes.toScreen(series.data.at(i), viewport);
        if (mapped) {
            x.push_back(mapped->x);
            y.push_back(-mapped->y);
        } else {
            x.push_back(std::numeric_limits<double>::quiet_NaN());
            y.push_back(std::numeric_limits<double>::quiet_NaN());
        }
    }
    Axes screenAxes;
    screenAxes.x.setRange({viewport.x, viewport.x + viewport.width});
    screenAxes.y.setRange({-viewport.y - viewport.height, -viewport.y});
    Series mapped = series;
    mapped.data = Data(x, y);
    return tessellate(mapped, screenAxes, viewport);
}

std::vector<Vertex> missingGeometry(const Series& series, const Axes& axes, Viewport viewport) {
    std::vector<Vertex> output;
    for (const auto i : series.data.missingIndices()) {
        const auto point = series.data.at(i);
        if (std::isfinite(point.x) && std::isfinite(point.y))
            continue;
        if (!std::isfinite(point.x))
            continue;
        const auto screen = axes.toScreen({point.x, 0}, viewport);
        if (screen)
            marker(output, *screen, std::max(4.0, series.style.markerSize), Marker::Cross, viewport);
    }
    return output;
}

std::vector<Vertex> pathGeometry(const std::vector<Point>& points, const Style& style, bool closed,
                                 bool filled, const Axes& axes, Viewport viewport) {
    if (!filled) {
        std::vector<double> x, y;
        for (const auto p : points) {
            x.push_back(p.x);
            y.push_back(p.y);
        }
        if (closed && !points.empty()) {
            x.push_back(points[0].x);
            y.push_back(points[0].y);
        }
        Series line;
        line.data = Data(x, y);
        line.style = style;
        return tessellate(line, axes, viewport);
    }
    std::vector<Point> mapped;
    for (const auto p : points) {
        const auto screen = axes.toScreen(p, viewport);
        if (!screen)
            return {};
        mapped.push_back(*screen);
    }
    // 同一转向并不足以排除星形自交；每条边还必须把其余点放在同一半平面。
    double orientation = 0;
    for (std::size_t i = 0; i < mapped.size(); ++i) {
        const auto a = mapped[i], b = mapped[(i + 1) % mapped.size()];
        for (const auto c : mapped) {
            const double cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
            if (!std::isfinite(cross))
                throw std::invalid_argument("plot: polygon coordinates overflow");
            if (cross != 0) {
                if (orientation != 0 && std::signbit(orientation) != std::signbit(cross))
                    throw std::invalid_argument("plot: filled path must be convex");
                orientation = cross;
            }
        }
    }
    std::vector<Vertex> output;
    polygon(output, std::move(mapped), viewport);
    return output;
}

std::vector<Vertex> bars(const Data& tops, const std::vector<double>& baselines, double width,
                         const Axes& axes, Viewport viewport) {
    if (tops.size() != baselines.size() || !std::isfinite(width) || width <= 0)
        throw std::invalid_argument("plot: invalid bars");
    std::vector<Vertex> output;
    for (std::size_t i = 0; i < tops.size(); ++i) {
        const auto top = tops.at(i);
        const auto a = axes.toScreen({top.x - width / 2, baselines[i]}, viewport);
        const auto b = axes.toScreen({top.x + width / 2, top.y}, viewport);
        if (a && b)
            polygon(output, {*a, {b->x, a->y}, *b, {a->x, b->y}}, viewport);
    }
    return output;
}
std::vector<Vertex> errorBars(const Data& centers, const std::vector<double>& lower,
                              const std::vector<double>& upper, const Axes& axes, Viewport viewport,
                              double capSize, double lineWidth) {
    if (centers.size() != lower.size() || lower.size() != upper.size() || !std::isfinite(capSize) ||
        capSize < 0 || !std::isfinite(lineWidth) || lineWidth < 0 || lineWidth > 1024 || capSize > 1024)
        throw std::invalid_argument("plot: invalid error bars");
    std::vector<Vertex> output;
    for (std::size_t i = 0; i < centers.size(); ++i) {
        if (lower[i] < 0 || upper[i] < 0)
            throw std::invalid_argument("plot: negative error");
        if (!std::isfinite(lower[i]) || !std::isfinite(upper[i]))
            continue;
        const auto center = centers.at(i);
        const auto a = axes.toScreen({center.x, center.y - lower[i]}, viewport);
        const auto b = axes.toScreen({center.x, center.y + upper[i]}, viewport);
        if (!a || !b)
            continue;
        segment(output, *a, *b, lineWidth, viewport);
        for (const auto p : {*a, *b})
            segment(output, {p.x - capSize / 2, p.y}, {p.x + capSize / 2, p.y}, lineWidth, viewport);
    }
    return output;
}
std::vector<Vertex> band(const std::vector<double>& x, const std::vector<double>& lower,
                         const std::vector<double>& upper, const Axes& axes, Viewport viewport) {
    if (x.size() != lower.size() || x.size() != upper.size())
        throw std::invalid_argument("plot: band lengths differ");
    std::vector<Vertex> output;
    for (std::size_t i = 0; i < x.size(); ++i)
        if (std::isfinite(lower[i]) && std::isfinite(upper[i]) && lower[i] > upper[i])
            throw std::invalid_argument("plot: inverted band");
    for (std::size_t i = 1; i < x.size(); ++i) {
        const auto a = axes.toScreen({x[i - 1], lower[i - 1]}, viewport),
                   b = axes.toScreen({x[i], lower[i]}, viewport);
        const auto c = axes.toScreen({x[i], upper[i]}, viewport),
                   d = axes.toScreen({x[i - 1], upper[i - 1]}, viewport);
        if (a && b && c && d)
            polygon(output, {*a, *b, *c, *d}, viewport);
    }
    return output;
}
std::vector<BarGroup> arrangeBars(const std::vector<double>& x,
                                  const std::vector<std::vector<double>>& values, bool stacked,
                                  double totalWidth) {
    if (!std::isfinite(totalWidth) || totalWidth <= 0)
        throw std::invalid_argument("plot: invalid group width");
    std::vector<double> positive(x.size(), 0), negative(x.size(), 0);
    std::vector<BarGroup> result;
    for (std::size_t group = 0; group < values.size(); ++group) {
        if (values[group].size() != x.size())
            throw std::invalid_argument("plot: group lengths differ");
        const double width = stacked ? totalWidth : totalWidth / values.size();
        auto positions = x, heights = values[group];
        std::vector<double> bases(x.size(), 0);
        for (std::size_t i = 0; i < x.size(); ++i) {
            if (stacked) {
                if (!std::isfinite(heights[i]) || !std::isfinite(x[i]))
                    continue;
                auto& cumulative = heights[i] >= 0 ? positive[i] : negative[i];
                bases[i] = cumulative;
                cumulative += heights[i];
                if (!std::isfinite(cumulative))
                    throw std::overflow_error("plot: stacked bars overflow");
                heights[i] = cumulative;
            } else
                positions[i] += width * (group + 0.5) - totalWidth / 2;
        }
        result.push_back({Data(positions, heights), std::move(bases), width});
    }
    return result;
}

void PointIndex::clear() { data_ = Data{}; }
void PointIndex::rebuild(const Data& data, const Axes& axes, Viewport viewport) {
    if (!std::isfinite(viewport.x) || !std::isfinite(viewport.y) || !std::isfinite(viewport.width) ||
        !std::isfinite(viewport.height) || viewport.width <= 0 || viewport.height <= 0 ||
        viewport.width > 32768 || viewport.height > 32768)
        throw std::invalid_argument("plot: invalid index viewport");
    data_ = data;
    axes_ = axes;
    viewport_ = viewport;
}
std::optional<Pick> PointIndex::nearest(Point screen, double radius) const {
    if (!std::isfinite(screen.x) || !std::isfinite(screen.y) || !std::isfinite(radius) || radius < 0)
        return std::nullopt;
    std::optional<Pick> result;
    const auto visit = [&](const auto& self, std::size_t a, std::size_t b) -> void {
        if (a == b)
            return;
        const auto s = data_.summary(a, b);
        if (!s.valid)
            return;
        // DataSummary stores independent X and Y extrema. Build the screen-space rectangle
        // from the four combinations; pairing min X with min Y would under-bound the node.
        const auto x0n = axes_.x.normalize(s.min.x), x1n = axes_.x.normalize(s.max.x);
        const auto y0n = axes_.y.normalize(s.min.y), y1n = axes_.y.normalize(s.max.y);
        if (x0n && x1n && y0n && y1n) {
            const double x0 = std::max(viewport_.x, viewport_.x + std::min(*x0n, *x1n) * viewport_.width);
            const double x1 =
                std::min(viewport_.x + viewport_.width, viewport_.x + std::max(*x0n, *x1n) * viewport_.width);
            const double y0 =
                std::max(viewport_.y, viewport_.y + (1 - std::max(*y0n, *y1n)) * viewport_.height);
            const double y1 = std::min(viewport_.y + viewport_.height,
                                       viewport_.y + (1 - std::min(*y0n, *y1n)) * viewport_.height);
            if (x0 > x1 || y0 > y1)
                return;
            const double distance =
                std::hypot(screen.x - std::clamp(screen.x, x0, x1), screen.y - std::clamp(screen.y, y0, y1));
            if (distance > (result ? result->distance : radius))
                return;
        } else if ((axes_.x.scale() == Scale::Log10 && s.max.x <= 0) ||
                   (axes_.y.scale() == Scale::Log10 && s.max.y <= 0))
            return;
        if (b - a > 32) {
            const auto m = a + (b - a) / 2;
            self(self, a, m);
            self(self, m, b);
            return;
        }
        for (auto i = a; i < b; ++i) {
            const auto point = data_.at(i);
            const auto p = axes_.toScreen(point, viewport_);
            if (!p || !inside(*p, viewport_))
                continue;
            const double distance = std::hypot(screen.x - p->x, screen.y - p->y);
            if (distance <= radius && (!result || distance < result->distance ||
                                       (distance == result->distance && i < result->index)))
                result = Pick{i, point, *p, distance, Pick::Kind::Original};
        }
    };
    visit(visit, 0, data_.size());
    return result;
}
Histogram histogram(const std::vector<double>& samples, const std::vector<double>& edges,
                    Normalization normalization) {
    if (normalization != Normalization::Count && normalization != Normalization::Probability &&
        normalization != Normalization::Density)
        throw std::invalid_argument("plot: unknown histogram normalization");
    if (edges.size() < 2)
        throw std::invalid_argument("plot: histogram needs two edges");
    for (std::size_t i = 0; i < edges.size(); ++i)
        if (!std::isfinite(edges[i]) ||
            (i && (edges[i] <= edges[i - 1] || !std::isfinite(edges[i] - edges[i - 1]))))
            throw std::invalid_argument("plot: histogram edges must increase");
    Histogram result{edges, std::vector<double>(edges.size() - 1, 0)};
    std::size_t count = 0;
    for (double value : samples) {
        if (!std::isfinite(value) || value < edges.front() || value > edges.back())
            continue;
        const auto upper = std::upper_bound(edges.begin(), edges.end(), value);
        const auto index =
            std::min(result.values.size() - 1, static_cast<std::size_t>(upper - edges.begin() - 1));
        result.values[index] += 1;
        ++count;
    }
    if (count && normalization != Normalization::Count) {
        for (std::size_t i = 0; i < result.values.size(); ++i) {
            result.values[i] /= count;
            if (normalization == Normalization::Density)
                result.values[i] /= edges[i + 1] - edges[i];
        }
    }
    return result;
}
} // namespace modules::plot
