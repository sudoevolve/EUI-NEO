#pragma once

#include "modules/plot/plot.h"

#include <cstddef>
#include <memory>
#include <vector>

namespace modules::plot {

/** @brief 多子图容器；同一链接组在下一次 compose 前共享 X 或 Y 的当前范围。 */
class Figure {
  public:
    /** @brief 添加一个由 Figure 管理生命周期的子图。 */
    Plot& addPlot(Budget budget = {});
    /** @brief 返回子图；索引越界抛出 std::out_of_range。 */
    Plot& plot(std::size_t index);
    const Plot& plot(std::size_t index) const;
    /** @brief 子图数量。 */
    std::size_t size() const noexcept;
    /** @brief 设置布局列数；必须大于零。 */
    void setColumns(std::size_t columns);
    std::size_t columns() const noexcept;
    /** @brief 将列出的子图链接到同一个 X 范围；少于两个索引时移除该组。 */
    void linkX(std::vector<std::size_t> plots);
    /** @brief 将列出的子图链接到同一个 Y 范围；少于两个索引时移除该组。 */
    void linkY(std::vector<std::size_t> plots);
    /** @brief 将列出的子图链接到同一个数据坐标十字光标。 */
    void linkCursor(std::vector<std::size_t> plots);
    /** @brief 应用当前链接范围；compose 会自动调用，供无窗口测试及外部批量更新使用。 */
    void synchronizeLinks();
    /** @brief 组合均分网格；链接范围在子图组合前同步，不使用递归回调。 */
    void compose(eui::Ui& ui, const std::string& id, float width, float height, double dpi = 1);
    /** @brief 在所属窗口设备销毁前释放所有子图 GPU 资源。 */
    void releaseGpu();

  private:
    struct Link {
        std::vector<std::size_t> plots;
        std::vector<Range> previous;
    };
    struct CursorLink {
      std::vector<std::size_t> plots;
      std::vector<std::optional<Point>> previous;
    };

    void synchronize(std::vector<Link>& links, bool horizontal);
    void synchronizeCursors();
    std::vector<std::unique_ptr<Plot>> plots_;
    std::vector<Link> xLinks_;
    std::vector<Link> yLinks_;
    std::vector<CursorLink> cursorLinks_;
    std::size_t columns_ = 1;
};

} // namespace modules::plot