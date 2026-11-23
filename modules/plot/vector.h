#pragma once

#include "modules/plot/field.h"

#include <cstddef>
#include <vector>

namespace modules::plot {

/** @brief 规则网格二维矢量场；两个分量共享严格相同的坐标和布局。 */
class VectorField {
  public:
    VectorField(RectilinearField x, RectilinearField y);
    const RectilinearField& xComponent() const noexcept;
    const RectilinearField& yComponent() const noexcept;
    /** @brief 双线性采样；任一分量缺失或越界返回空。 */
    std::optional<Point> sample(Point position) const;

  private:
    RectilinearField x_;
    RectilinearField y_;
};

/** @brief 数据空间箭头，sourceIndex 对应输入 seed 或网格样本索引。 */
struct VectorArrow {
    Point from;
    Point to;
    std::size_t sourceIndex = 0;
};

/** @brief 固定步 RK4 流线及其起始 seed 索引。 */
struct Streamline {
    std::size_t sourceIndex = 0;
    std::vector<Point> points;
};

/** @brief 将规则网格样本转换为固定屏幕长度比例的箭头。 */
std::vector<VectorArrow> vectorArrows(const VectorField& field, double scale = 0.1,
                                      double minimumMagnitude = 0);
/** @brief 从 seeds 正向/反向用固定步长 RK4 积分，越界、缺失、低速或步数耗尽时终止。 */
std::vector<Streamline> streamlines(const VectorField& field, const std::vector<Point>& seeds,
                                    double step, std::size_t maxSteps, double minimumMagnitude = 1e-12,
                                    bool bothDirections = true);
/** @brief 把箭头和流线转换为可提交给 Renderer 的屏幕三角形。 */
std::vector<Vertex> arrowGeometry(const std::vector<VectorArrow>& arrows, const Axes& axes, Viewport viewport,
                                  double width = 1.5, double headLength = 6, double headWidth = 4);
std::vector<Vertex> streamlineGeometry(const Streamline& line, const Axes& axes, Viewport viewport,
                                       double width = 1.5);

} // namespace modules::plot