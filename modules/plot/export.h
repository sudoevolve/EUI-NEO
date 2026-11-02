#pragma once

#include "modules/plot/geometry.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace modules::plot {

struct ExportText {
    Point position;
    std::string text;
    float fontSize = 12;
    std::array<float, 4> color{1, 1, 1, 1};
};
struct ExportPath {
    std::vector<Point> points;
    bool closed = false;
    bool filled = false;
    std::array<float, 4> color{1, 1, 1, 1};
    float lineWidth = 1;
};
struct ExportRaster {
    Point position;
    double width = 1, height = 1;
    std::uint32_t pixelWidth = 0, pixelHeight = 0;
    std::vector<std::uint8_t> rgba;
};
/** @brief 与窗口和 GPU 无关的二维导出场景。 */
struct ExportScene {
    double width = 1;
    double height = 1;
    std::array<float, 4> background{0, 0, 0, 1};
    std::vector<ExportPath> paths;
    std::vector<ExportText> texts;
    std::vector<ExportRaster> rasters;
};

/** @brief 导出 SVG 路径、UTF-8 文本及内嵌栅格，保留 RGBA 透明度；非法数值抛出 std::invalid_argument。 */
void writeSvg(const ExportScene& scene, const std::string& filename);
/** @brief 导出 PDF 路径、Helvetica 文本及栅格，保留透明度；不嵌入 Unicode 字体。 */
void writePdf(const ExportScene& scene, const std::string& filename);
/** @brief Raster layers and UTF-8 text to PNG; requires an explicit font file, rejects paths.
 * Coordinates/font sizes are pixels at the requested export size. No complex-script shaping.
 */
void writeRasterPng(const ExportScene& scene, const std::string& filename, const std::string& fontFile,
                    double dpi = 96, std::size_t budgetBytes = 128 * 1024 * 1024);
/** @brief 写出 RGBA8 行优先像素；DPI 仅记录为 PNG 物理像素密度元数据。 */
void writePng(const std::string& filename, std::uint32_t width, std::uint32_t height,
              const std::vector<std::uint8_t>& rgba, double dpi = 96);

} // namespace modules::plot
