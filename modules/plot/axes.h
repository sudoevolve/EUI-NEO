#pragma once

#include "modules/plot/data.h"

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace modules::plot {

/** @brief 轴变换类型。 */
enum class Scale {
    Linear, ///< 线性映射。
    Log10   ///< 十进制对数；非正值为缺失数据。
};

  /** @brief 极坐标角度数据的输入和输出单位。 */
  enum class AngleUnit { Radians, Degrees };

/** @brief 有序有限数据范围；Axis 的入口负责校验。 */
struct Range {
    double min = 0;
    double max = 1;
};

/** @brief 刻度及其显示文本；次刻度默认没有文本。 */
struct Tick {
    double value = 0;
    bool major = true;
    std::string label;
};

/** @brief 后端无关的单轴范围、格式与变换。修改仅在所属 UI 线程进行。 */
class Axis {
  public:
    /** @brief 配置变换；手动范围与新变换不兼容时抛出 std::invalid_argument。 */
    void setScale(Scale scale);
    Scale scale() const noexcept;
    /** @brief 设置严格递增的有限范围并关闭自动范围，否则抛出 std::invalid_argument。 */
    void setRange(Range range);
    Range range() const noexcept;
    /** @brief 恢复自动范围，下一次 fit() 应用数据范围。 */
    void resetAuto() noexcept;
    bool automatic() const noexcept;
    /** @brief 使用有效数据范围；常量自动扩展，空数据使用线性 [0,1] 或对数 [1,10]。 */
    void fit(const std::optional<Range>& extent);
    /** @brief 反转视觉方向，不交换原始 min/max。 */
    void setReversed(bool reversed) noexcept;
    bool reversed() const noexcept;
    /** @brief 映射到单位坐标；无效数据返回 nullopt，范围外可返回 [0,1] 外的值。 */
    std::optional<double> normalize(double value) const;
    /** @brief 从单位坐标还原；溢出、非有限输入返回 nullopt。 */
    std::optional<double> denormalize(double fraction) const;
    /** @brief 生成数量受限的主次刻度；目标主刻度数限制在 [2,20]。 */
    std::vector<Tick> ticks(int targetCount = 6) const;

    std::string label;                            ///< 轴标题，由 UI 排版。
    std::string unit;                             ///< 单位，由 UI 排版。
    std::function<std::string(double)> formatter; ///< 可选主刻度格式函数，在调用线程执行。

  private:
    Scale scale_ = Scale::Linear;
    Range range_;
    bool automatic_ = true;
    bool reversed_ = false;
};

/** @brief 独立于 GPU float 的屏幕逻辑坐标区域，原点位于左上角。 */
struct Viewport {
    double x = 0;
    double y = 0;
    double width = 1;
    double height = 1;
};

/** @brief 二维轴组合及屏幕映射，不持有原始数据或 GPU 资源。 */
class Axes {
  public:
    Axis x;
    Axis y;
    /** @brief 由多份数据计算自动范围，缺失点和对数轴非正点不参与。 */
    void fit(const std::vector<Data>& data);
    /** @brief 数据转屏幕，Y 轴向上；无效坐标或视口返回 nullopt。 */
    std::optional<Point> toScreen(Point point, Viewport viewport) const;
    /** @brief 屏幕转数据，不自动夹到绘图区；无效视口返回 nullopt。 */
    std::optional<Point> toData(Point point, Viewport viewport) const;
    /** @brief 扩展线性轴范围以实现等比例单位；对数轴返回 false，成功后两轴为手动范围。 */
    bool equalize(Viewport viewport);
};

/** @brief 后端无关的极坐标变换；Point.x 为角度，Point.y 为非负半径。 */
class PolarAxes {
  public:
    /** @brief 径向范围和刻度；自动范围从零开始覆盖全部有效半径。 */
    Axis radius;
    void setAngleUnit(AngleUnit unit);
    AngleUnit angleUnit() const noexcept;
    /** @brief 配置零角，参数及返回值均使用当前角度单位。 */
    void setZeroAngle(double angle);
    double zeroAngle() const noexcept;
    /** @brief 设为 true 时，正角度方向顺时针旋转。 */
    void setClockwise(bool clockwise) noexcept;
    bool clockwise() const noexcept;
    /** @brief 使用有限且半径非负的数据拟合径向轴；角度不参与范围计算。 */
    void fit(const std::vector<Data>& data);
    /** @brief 极坐标数据转圆形视口屏幕坐标；圆盘外、负半径或无效输入返回空。 */
    std::optional<Point> toScreen(Point point, Viewport viewport) const;
    /** @brief 圆形视口屏幕坐标转极坐标数据；圆盘外或无效输入返回空。 */
    std::optional<Point> toData(Point point, Viewport viewport) const;

  private:
    AngleUnit unit_ = AngleUnit::Radians;
    double zeroAngleRadians_ = 0;
    bool clockwise_ = false;
};

} // namespace modules::plot
