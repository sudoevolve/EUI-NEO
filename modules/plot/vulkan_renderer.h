#pragma once

#if defined(EUI_RENDER_BACKEND_VULKAN)

#include "modules/plot/renderer.h"
#include "eui/image.h"

#include <memory>
#include <vector>

namespace modules::plot::detail {

class VulkanPlotRenderer {
  public:
    struct Texture;

    explicit VulkanPlotRenderer(const eui::GpuDeviceInfo& device);
    ~VulkanPlotRenderer();
    VulkanPlotRenderer(const VulkanPlotRenderer&) = delete;
    VulkanPlotRenderer& operator=(const VulkanPlotRenderer&) = delete;

    std::shared_ptr<Texture> createTexture(std::uint32_t width, std::uint32_t height);
    std::uint64_t imageViewHandle(const std::shared_ptr<Texture>& texture) const;
    bool supportsSubpixelPrecision() const noexcept;
    void upload(const std::shared_ptr<Texture>& texture, const std::vector<std::uint8_t>& rgba);
    void render(const std::shared_ptr<Texture>& texture, const std::vector<Batch>& batches,
                int pixelsX, int pixelsY, double dpi);

  private:
    struct Resources;
    std::shared_ptr<Resources> resources_;
};

} // namespace modules::plot::detail

#endif
