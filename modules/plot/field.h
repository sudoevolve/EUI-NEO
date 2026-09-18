#pragma once

#include "modules/plot/axes.h"
#include "modules/plot/geometry.h"

#include <array>
#include <cstddef>
#include <optional>
#include <vector>

namespace modules::plot {

/** @brief 标量场在屏幕 Y 方向的行原点。 */
enum class FieldOrigin { LowerLeft, UpperLeft };
/** @brief 标量值对应网格顶点还是单元中心。 */
enum class FieldSampling { GridPoints, CellCenters };
/** @brief 标量到色图位置的变换。 */
enum class ColorScaleMode { Linear, Log10 };

/**
 * @brief 不可变二维标量场快照，值以 row * columns + column 的行优先顺序存储。
 *
 * 值可为 NaN 或 Inf，渲染和分析将它们视为缺失；坐标范围始终是有限严格递增区间。
 */
class ScalarField {
  public:
    ScalarField(std::size_t rows, std::size_t columns, std::vector<double> values, Range x = {0, 1},
                Range y = {0, 1}, FieldOrigin origin = FieldOrigin::LowerLeft,
                FieldSampling sampling = FieldSampling::CellCenters);
    std::size_t rows() const noexcept;
    std::size_t columns() const noexcept;
    /** @brief 返回 row/column 处的原始标量；越界抛出 std::out_of_range。 */
    double value(std::size_t row, std::size_t column) const;
    const std::vector<double>& values() const noexcept;
    Range xRange() const noexcept;
    Range yRange() const noexcept;
    FieldOrigin origin() const noexcept;
    FieldSampling sampling() const noexcept;
    /** @brief 有限样本的最小和最大值；全缺失场返回空。 */
    std::optional<Range> finiteRange() const noexcept;

  private:
    std::size_t rows_ = 0;
    std::size_t columns_ = 0;
    std::vector<double> values_;
    Range x_;
    Range y_;
    FieldOrigin origin_ = FieldOrigin::LowerLeft;
    FieldSampling sampling_ = FieldSampling::CellCenters;
};

/** @brief 坐标单调的规则直角网格；values 按 y 行、x 列存储且表示网格顶点。 */
class RectilinearField {
  public:
    RectilinearField(std::vector<double> x, std::vector<double> y, std::vector<double> values);
    std::size_t rows() const noexcept;
    std::size_t columns() const noexcept;
    const std::vector<double>& x() const noexcept;
    const std::vector<double>& y() const noexcept;
    double value(std::size_t row, std::size_t column) const;
    /** @brief 在包含点的网格单元双线性插值；边界外或任一顶点缺失返回空。 */
    std::optional<double> interpolate(Point point) const;

  private:
    std::vector<double> x_;
    std::vector<double> y_;
    std::vector<double> values_;
};

/** @brief 显式三角拓扑的非规则标量场；不自动重采样或推断网格连接关系。 */
class TriangulatedField {
  public:
    TriangulatedField(std::vector<Point> points, std::vector<double> values,
                      std::vector<std::array<std::size_t, 3>> triangles);
    const std::vector<Point>& points() const noexcept;
    const std::vector<double>& values() const noexcept;
    const std::vector<std::array<std::size_t, 3>>& triangles() const noexcept;
    /** @brief 在包含点的第一个有效三角形内重心插值；外部或缺失顶点返回空。 */
    std::optional<double> interpolate(Point point) const;

  private:
    std::vector<Point> points_;
    std::vector<double> values_;
    std::vector<std::array<std::size_t, 3>> triangles_;
};

/** @brief 后端无关的标量色阶；非有限值和对数轴非正值使用缺失颜色。 */
class ColorScale {
  public:
    void setColorMap(ColorMap map);
    ColorMap colorMap() const noexcept;
    void setMode(ColorScaleMode mode);
    ColorScaleMode mode() const noexcept;
    /** @brief 设定严格递增范围并关闭自动范围。 */
    void setRange(Range range);
    Range range() const noexcept;
    void resetAuto() noexcept;
    bool automatic() const noexcept;
    /** @brief 用有限标量范围拟合自动范围；对数色阶忽略非正值。 */
    void fit(const std::optional<Range>& range);
    /** @brief 用场中的有效样本拟合自动范围；对数色阶仅使用正值。 */
    void fit(const ScalarField& field);
    /** @brief 设置离散色阶数量；零表示连续，范围为 [2,256]。 */
    void setDiscreteLevels(std::size_t levels);
    std::size_t discreteLevels() const noexcept;
    void setMissingColor(std::array<float, 4> color);
    std::array<float, 4> missingColor() const noexcept;
    /** @brief 映射单个标量为 straight RGBA；无效值返回缺失颜色。 */
    std::array<float, 4> map(double value) const;

  private:
    ColorMap map_ = ColorMap::Viridis;
    ColorScaleMode mode_ = ColorScaleMode::Linear;
    Range range_;
    bool automatic_ = true;
    std::size_t discreteLevels_ = 0;
    std::array<float, 4> missingColor_{0.35f, 0.35f, 0.35f, 1};
};

  /** @brief 单个同色标量场或颜色条图元批次。 */
  struct FieldTile {
    std::vector<Vertex> vertices;
    std::array<float, 4> color;
  };
  /** @brief 将标量场转换为有色矩形瓦片；GridPoints 单元使用四个有限顶点的算术平均值。 */
  std::vector<FieldTile> heatmapTiles(const ScalarField& field, const ColorScale& scale, Viewport viewport);
  /** @brief 将规则直角网格转换为按实际 x/y 坐标定位的有色三角形。 */
  std::vector<FieldTile> rectilinearTiles(const RectilinearField& field, const ColorScale& scale,
                                          const Axes& axes, Viewport viewport);
  /** @brief 将显式三角非规则网格转换为有色三角形；不执行插值或重网格化。 */
  std::vector<FieldTile> triangulatedTiles(const TriangulatedField& field, const ColorScale& scale,
                                           const Axes& axes, Viewport viewport);
  /** @brief 生成纵向颜色条；离散色阶保留每个色阶，连续色阶默认分为 128 段。 */
  std::vector<FieldTile> colorbarTiles(const ColorScale& scale, Viewport viewport, std::size_t segments = 128);

} // namespace modules::plot