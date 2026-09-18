#pragma once

#include "modules/plot/renderer.h"
#include "eui/dsl.h"

namespace modules::plot {

/** @brief 一个原始数据探针命中，series 为当前序列下标。 */
struct Probe {
    std::size_t series = 0;
    Pick point;
};

/** @brief 数据空间路径或多边形，不做曲线拟合。 */
struct Path {
    std::vector<Point> points;
    Style style;
    bool closed = false;
    bool filled = false;
};
/** @brief 数据位置处的普通 UI 文本标注。 */
struct Annotation {
    Point position;
    std::string text;
};
/** @brief 两次 Ctrl 点击得到的数据坐标测距。 */
struct Measurement {
    Point from;
    Point to;
    double distance = 0;
};

/**
 * @brief 科学二维绘图区：普通 UI 排版坐标与标签，离屏纹理批量绘制数据。
 *
 * 所有修改和 compose 都在所属窗口 UI 线程调用；不复制控制器。Runtime 回调使用
 * weak_ptr，控制器销毁后不再访问状态。窗口关闭前必须调用 releaseGpu()。
 * 修改数据不播放插值动画；异步计算产生 Data 后由应用投递到 UI 线程 setSeries()。
 */
class Plot {
  public:
    explicit Plot(Budget budget = {});
    ~Plot();
    Plot(const Plot&) = delete;
    Plot& operator=(const Plot&) = delete;
    /** @brief 替换序列列表，快照的原始块继续共享；自动轴重新拟合。 */
    void setSeries(std::vector<Series> series);
    /** @brief 返回当前序列的只读引用，有效至下一次修改。 */
    const std::vector<Series>& series() const;
    /** @brief 设置轴配置，自动轴重新拟合当前有效可见样本。 */
    void setAxes(Axes axes);
    const Axes& axes() const;
    /** @brief 显隐序列，索引越界抛出 std::out_of_range。 */
    void setVisible(std::size_t series, bool visible);
    /** @brief 禁用时取消拖动与探针，保留绘图内容。 */
    void setEnabled(bool enabled);
    /** @brief 设置标题。 */
    void setTitle(std::string title);
    /** @brief 设置数据空间路径（支持折线、箭头和简单凸多边形）。 */
    void setPaths(std::vector<Path> paths);
    /** @brief 设置数据空间文本，绘图区外不显示。 */
    void setAnnotations(std::vector<Annotation> annotations);
    /** @brief 最新点击选择的原始点；替换数据或隐藏选中曲线时清空。 */
    std::optional<Probe> selection() const;
    /** @brief 获取最近完成的 Ctrl 点击测距。 */
    std::optional<Measurement> measurement() const;
    /** @brief 重置两轴为自动范围。 */
    void resetView();
    /** @brief 按单位轴坐标平移；不合法或溢出时保持原范围。 */
    void pan(double horizontal, double vertical);
    /** @brief 以单位轴坐标 anchor 为中心缩放，factor < 1 放大。 */
    void zoom(Point anchor, double factor);
    /** @brief 获取当前最近原始点；鼠标移出时为空。 */
    std::optional<Probe> probe() const;
    /** @brief 组合一个固定逻辑尺寸绘图区；dpi 为纹理分辨率倍率，必须为正有限数。 */
    void compose(eui::Ui& ui, const std::string& id, float width, float height, double dpi = 1);
    /** @brief 在设备销毁前释放本对象持有的 GPU 资源，可重新 compose。 */
    void releaseGpu();

  private:
    struct State;
    std::shared_ptr<State> state_;
};

} // namespace modules::plot
