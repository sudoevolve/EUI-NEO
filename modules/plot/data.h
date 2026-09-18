#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace modules::plot {

/** @brief 原始数据坐标；非有限分量代表缺失点，不会被转换为零。 */
struct Point {
    double x = 0;
    double y = 0;
};

/**
 * @brief 不可变 XY 数据快照，保留原始顺序和索引。
 *
 * 快照可跨线程只读共享。更新返回新快照，旧快照保持有效；调用方在 UI 更新边界
 * 应用异步计算结果。内部块由快照共享拥有，不向调用方暴露可写数据。
 */
class Data {
  public:
    static constexpr std::size_t blockCapacity = 4096;

    /** @brief 创建空数据，不注入默认样本。 */
    Data();
    /** @brief 复制独立 XY 数组；长度不同抛出 std::invalid_argument。 */
    Data(const std::vector<double>& x, const std::vector<double>& y);
    /** @brief 返回原始样本数，包括缺失点。 */
    std::size_t size() const noexcept;
    /** @brief 读取原始点；越界抛出 std::out_of_range。 */
    Point at(std::size_t index) const;
    /** @brief 返回版本号；每次非空更新加一，溢出抛出 std::overflow_error。 */
    std::uint64_t revision() const noexcept;
    /** @brief 追加样本；复制末尾未满块及新增数据，已有完整块继续共享。 */
    Data append(const std::vector<double>& x, const std::vector<double>& y) const;
    /** @brief 替换指定区间，只复制受影响块；非法区间抛出 std::out_of_range。 */
    Data replace(std::size_t first, const std::vector<double>& x, const std::vector<double>& y) const;
    /** @brief 导出全部原始样本，包括缺失点，不经过显示降采样。 */
    std::vector<Point> points() const;
    /** @brief 返回只读块数，用于资源预算与测试。 */
    std::size_t blockCount() const noexcept;
    /** @brief 返回块的稳定身份；仅用于判断快照间复用，不得解引用。 */
    const void* blockIdentity(std::size_t block) const;

  private:
    using Block = std::vector<Point>;
    std::vector<std::shared_ptr<const Block>> blocks_;
    std::size_t size_ = 0;
    std::uint64_t revision_ = 0;
};

} // namespace modules::plot
