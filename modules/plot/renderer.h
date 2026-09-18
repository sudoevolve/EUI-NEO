#pragma once

#include "modules/plot/geometry.h"
#include "eui/image.h"

#include <memory>

namespace modules::plot {

/** @brief 绘图资源预算；超额时抛出 std::length_error，不截断数据。 */
struct Budget {
    std::size_t vertexBytes = 64 * 1024 * 1024;
    std::size_t textureBytes = 64 * 1024 * 1024;
};

/** @brief 一次同色三角形批次。 */
struct Batch {
    std::vector<Vertex> vertices;
    std::array<float, 4> color;
};

/**
 * @brief 可选模块的离屏渲染器；仅在创建它的窗口 UI/渲染线程使用。
 *
 * GPU 类型隐藏在实现内。必须在窗口设备销毁前 release()；UI 持有的图像引用由
 * 核心外部图像接口延迟退休。当前仅支持 OpenGL，其他后端明确抛出 runtime_error。
 */
class Renderer {
  public:
    explicit Renderer(Budget budget = {});
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    /** @brief 按逻辑尺寸绘制到物理像素纹理，复用缓冲容量；颜色为 straight RGBA。 */
    void render(const std::vector<Batch>& batches, double width, double height, double dpi = 1);
    /** @brief 返回只读纹理引用；尚未渲染或释放后为空。 */
    std::shared_ptr<const eui::GpuImage> image() const;
    /** @brief 每次成功绘制后增加，用于 image.texture() 的内容失效。 */
    std::uint64_t revision() const noexcept;
    /** @brief 释放模块持有的 GPU 资源与容量，可在同设备重新使用。 */
    void release();

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace modules::plot
