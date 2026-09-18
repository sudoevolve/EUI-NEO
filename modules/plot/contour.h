#pragma once

#include "modules/plot/field.h"

#include <array>
#include <vector>

namespace modules::plot {

/** @brief 单条数据空间等高线线段，可直接转换为 Path 或矢量导出路径。 */
struct ContourSegment {
    Point from;
    Point to;
};
/** @brief 一个指定等值的全部线段；鞍点按单元中心值确定连接关系。 */
struct ContourLines {
    double level = 0;
    std::vector<ContourSegment> segments;
};
/** @brief 数据空间等高线标签锚点。 */
struct ContourLabel {
    double level = 0;
    Point position;
};
/** @brief 一个填充等高带的数据空间三角形；范围下界含、上界不含，最后带含上界。 */
struct ContourBand {
    double lower = 0;
    double upper = 0;
    std::vector<std::array<Point, 3>> triangles;
};

/**
 * @brief 使用 marching squares 提取指定等值线。
 *
 * levels 必须严格递增且有限。任何含 NaN 或 Inf 顶点的单元均不输出，避免跨越缺失区域。
 */
std::vector<ContourLines> marchingSquares(const ScalarField& field, const std::vector<double>& levels);
/** @brief 从有限场范围等距生成指定数量的内部等值；空场或常量场返回空。 */
std::vector<double> automaticContourLevels(const ScalarField& field, std::size_t count = 10);
/** @brief 为每个有线段的等值选择不超过 maxPerLevel 个标签锚点。 */
std::vector<ContourLabel> contourLabels(const std::vector<ContourLines>& lines, std::size_t maxPerLevel = 1);
/** @brief 生成由 levels 分隔的填充等高带；非有限单元完全跳过。 */
std::vector<ContourBand> filledContours(const ScalarField& field, const std::vector<double>& levels);

} // namespace modules::plot