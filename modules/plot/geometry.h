#pragma once

#include "modules/plot/axes.h"

#include <array>
#include <optional>
#include <vector>

namespace modules::plot {

/** @brief 可批量生成的二维数据图形。 */
enum class Graph {
    Line,      ///< 按原始顺序连接，缺失点断开。
    Scatter,   ///< 仅绘制原始样本标记。
    Step,      ///< 先水平后垂直的后置阶梯。
    Stem,      ///< 从 baseline 连接到各样本。
    Bar,       ///< 以数据单位指定宽度的正负柱。
    Area,      ///< 折线和 baseline 之间的面积。
    ErrorBars, ///< lower/upper 为非负误差。
    Band       ///< lower/upper 为绝对包络 Y 值。
};

/** @brief 序列使用的垂直坐标轴。 */
enum class YAxis { Primary, Secondary };

/** @brief 数据标记形状，尺寸为包围盒边长。 */
enum class Marker { Square, Circle, Diamond, Cross };
/** @brief 线型，虚线长度以线宽为基准。 */
enum class LinePattern { Solid, Dashed, Dotted };
/** @brief 线段端点样式。 */
enum class LineCap { Butt, Square, Round };
/** @brief 折线拐角样式。 */
enum class LineJoin { Miter, Bevel, Round };
/** @brief 颜色映射命名，供统一样式和后端扩展使用。 */
enum class ColorMap { Solid, Grayscale, Viridis, Plasma, Turbo };
/** @brief 曲线数据表达方式；非 Raw 模式只改变显示快照，不改变原始 Data。 */
enum class CurveKind { Raw, Interpolated, Fitted };

struct ColorRange {
    double min = 0;
    double max = 1;
};

/** @brief 图形外观；宽度、标记尺寸以逻辑像素计，柱宽和基线以数据单位计。 */
struct Style {
    std::array<float, 4> color{0.2f, 0.65f, 1.f, 1.f};
    double lineWidth = 1.5;
    double markerSize = 5;
    double barWidth = 0.8;
    double baseline = 0;
    bool markers = false;
    Marker marker = Marker::Square;
    LinePattern pattern = LinePattern::Solid;
    /** @brief 线段端点和拐角的统一样式配置。 */
    LineCap cap = LineCap::Butt;
    LineJoin join = LineJoin::Miter;
    /** @brief 色图、色阶范围和缺失值显示颜色。 */
    ColorMap colorMap = ColorMap::Solid;
    ColorRange colorRange;
    std::array<float, 4> missingColor{0.95f, 0.65f, 0.2f, 0.9f};
};

/** @brief 单条图形及其只读原始数据；修改后由调用方触发视图更新。 */
struct Series {
    Data data;
    Graph graph = Graph::Line;
    Style style;
    std::string name;
    bool visible = true;
    YAxis yAxis = YAxis::Primary;
    /** @brief 显示模式；原始数据快照始终保留并用于拾取。 */
    CurveKind curve = CurveKind::Raw;
    std::vector<double> lower;     ///< ErrorBars 或 Band 的下界参数。
    std::vector<double> upper;     ///< ErrorBars 或 Band 的上界参数。
    std::vector<double> baselines; ///< Bar 的逐柱基线；空时使用 style.baseline。
};

/** @brief 屏幕局部三角形顶点；原始 double 在映射后才转换成 float。 */
struct Vertex {
    float x = 0;
    float y = 0;
};

/** @brief 显示点保留原始索引，缺失点不进入本结构。 */
struct DisplayPoint {
    Point screen;
    std::size_t index = 0;
};

/** @brief 一段连续有效数据的显示近似；不同段之间禁止连线。 */
using DisplayRun = std::vector<DisplayPoint>;

/** @brief 将连续屏幕列内的样本缩减到首、末、极小和极大值，保持原始次序。 */
std::vector<DisplayRun> selectDisplay(const Data& data, const Axes& axes, Viewport viewport);

/** @brief 使用矩形裁剪生成批量三角形；非法样式抛出 std::invalid_argument。 */
std::vector<Vertex> tessellate(const Series& series, const Axes& axes, Viewport viewport);
/** @brief 使用 PolarAxes 生成极坐标折线或散点；其他 Graph 类型抛出 std::invalid_argument。 */
std::vector<Vertex> polarTessellate(const Series& series, const PolarAxes& axes, Viewport viewport);
std::vector<Vertex> missingGeometry(const Series& series, const Axes& axes, Viewport viewport);
/** @brief 数据路径及凸多边形裁剪；非凸填充抛出 std::invalid_argument。 */
std::vector<Vertex> pathGeometry(const std::vector<Point>& points, const Style& style, bool closed,
                                 bool filled, const Axes& axes, Viewport viewport);

/** @brief 按独立基线上绘制柱，用于分组或正负堆叠。数组长度必须一致。 */
std::vector<Vertex> bars(const Data& tops, const std::vector<double>& baselines, double width,
                         const Axes& axes, Viewport viewport);
/** @brief 每个原始点的非负上下误差；缺失误差跳过该点，非法长度或负误差抛出异常。 */
std::vector<Vertex> errorBars(const Data& centers, const std::vector<double>& lower,
                              const std::vector<double>& upper, const Axes& axes, Viewport viewport,
                              double capSize = 6, double lineWidth = 1);
/** @brief 填充相同 X 数组上的上下包络；缺失值断开，不做插值拟合。 */
std::vector<Vertex> band(const std::vector<double>& x, const std::vector<double>& lower,
                         const std::vector<double>& upper, const Axes& axes, Viewport viewport);
/** @brief 分组/堆叠柱的数据几何，正负值分别累加。 */
struct BarGroup {
    Data tops;
    std::vector<double> baselines;
    double width = 0;
};
/** @brief 将各系列相同 X 上的值转换为可绘制柱；缺失值不参与累计。 */
std::vector<BarGroup> arrangeBars(const std::vector<double>& x,
                                  const std::vector<std::vector<double>>& values, bool stacked,
                                  double totalWidth = 0.8);

/** @brief 最近原始样本命中；不是显示近似或插值点。 */
struct Pick {
    std::size_t index = 0;
    Point data;
    Point screen;
    double distance = 0;
    enum class Kind { Original, Interpolated } kind = Kind::Original;
};

/** @brief 屏幕空间桶索引，在视图或数据变化时重建，鼠标移动仅检查相邻桶。 */
class PointIndex {
  public:
    /** @brief 索引绘图区内的全部原始有效点，保留快照以支持原始值回查。 */
    void rebuild(const Data& data, const Axes& axes, Viewport viewport);
    /** @brief 在逻辑像素半径内查找最近原始点，同距时选择较小索引。 */
    std::optional<Pick> nearest(Point screen, double radius = 8) const;
    /** @brief 清理全部桶、样本引用和分配容量。 */
    void clear();

  private:
    Data data_;
    Viewport viewport_;
    std::size_t columns_ = 0;
    std::size_t rows_ = 0;
    std::vector<std::vector<DisplayPoint>> buckets_;
};

/** @brief 直方图归一化方式，分箱为左闭右开，最后一箱包含右端点。 */
enum class Normalization { Count, Probability, Density };
/** @brief 直方图数值结果，edges.size() == values.size() + 1。 */
struct Histogram {
    std::vector<double> edges;
    std::vector<double> values;
};
/** @brief 有限样本落在 edges 范围内才计数；概率/密度分母为纳入样本数。 */
Histogram histogram(const std::vector<double>& samples, const std::vector<double>& edges,
                    Normalization normalization = Normalization::Count);

} // namespace modules::plot
