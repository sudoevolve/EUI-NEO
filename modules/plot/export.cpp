#include "modules/plot/export.h"

#include <png.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace modules::plot {
namespace {
void validateScene(const ExportScene& scene) {
    if (!std::isfinite(scene.width) || !std::isfinite(scene.height) || scene.width <= 0 || scene.height <= 0)
        throw std::invalid_argument("plot: invalid export scene size");
}
std::string color(std::array<float, 4> value) {
    std::ostringstream stream;
    stream << "rgb(" << int(std::clamp(value[0], 0.f, 1.f) * 255) << "," << int(std::clamp(value[1], 0.f, 1.f) * 255)
           << "," << int(std::clamp(value[2], 0.f, 1.f) * 255) << ")";
    return stream.str();
}
std::string escapeXml(const std::string& text) {
    std::string result;
    for (const char character : text) {
        if (character == '&') result += "&amp;";
        else if (character == '<') result += "&lt;";
        else if (character == '>') result += "&gt;";
        else if (character == '"') result += "&quot;";
        else result += character;
    }
    return result;
}
void checkFile(const std::ofstream& file, const char* message) {
    if (!file)
        throw std::runtime_error(message);
}
} // namespace

void writeSvg(const ExportScene& scene, const std::string& filename) {
    validateScene(scene);
    std::ofstream file(filename, std::ios::binary);
    checkFile(file, "plot: cannot open SVG output");
    file << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << scene.width << "\" height=\"" << scene.height
         << "\" viewBox=\"0 0 " << scene.width << " " << scene.height << "\">\n";
    file << "<rect width=\"100%\" height=\"100%\" fill=\"" << color(scene.background) << "\"/>\n";
    for (const auto& path : scene.paths) {
        if (path.points.empty()) continue;
        file << "<path d=\"M " << path.points.front().x << " " << path.points.front().y;
        for (std::size_t index = 1; index < path.points.size(); ++index)
            file << " L " << path.points[index].x << " " << path.points[index].y;
        if (path.closed) file << " Z";
        file << "\" fill=\"" << (path.filled ? color(path.color) : "none") << "\" stroke=\"" << color(path.color)
             << "\" stroke-width=\"" << path.lineWidth << "\"/>\n";
    }
    for (const auto& text : scene.texts)
        file << "<text x=\"" << text.position.x << "\" y=\"" << text.position.y << "\" font-size=\"" << text.fontSize
             << "\" fill=\"" << color(text.color) << "\">" << escapeXml(text.text) << "</text>\n";
    file << "</svg>\n";
    checkFile(file, "plot: failed writing SVG output");
}
void writePdf(const ExportScene& scene, const std::string& filename) {
    validateScene(scene);
    std::ostringstream content;
    content << scene.background[0] << " " << scene.background[1] << " " << scene.background[2] << " rg 0 0 "
            << scene.width << " " << scene.height << " re f\n";
    for (const auto& path : scene.paths) {
        if (path.points.empty()) continue;
        content << path.color[0] << " " << path.color[1] << " " << path.color[2] << " RG " << path.lineWidth << " w\n"
                << path.points.front().x << " " << scene.height - path.points.front().y << " m\n";
        for (std::size_t index = 1; index < path.points.size(); ++index)
            content << path.points[index].x << " " << scene.height - path.points[index].y << " l\n";
        if (path.closed) content << "h\n";
        content << (path.filled ? "B\n" : "S\n");
    }
    for (const auto& text : scene.texts)
        content << "BT /F1 " << text.fontSize << " Tf " << text.position.x << " " << scene.height - text.position.y
                << " Td (" << text.text << ") Tj ET\n";
    const std::string stream = content.str();
    std::ofstream file(filename, std::ios::binary);
    checkFile(file, "plot: cannot open PDF output");
    std::vector<std::streamoff> offsets(6);
    file << "%PDF-1.4\n%\xE2\xE3\xCF\xD3\n";
    auto object = [&](int number, const std::string& body) {
        offsets[number] = file.tellp();
        file << number << " 0 obj\n" << body << "\nendobj\n";
    };
    object(1, "<< /Type /Catalog /Pages 2 0 R >>");
    object(2, "<< /Type /Pages /Kids [3 0 R] /Count 1 >>");
    object(3, "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 " + std::to_string(scene.width) + " " +
                  std::to_string(scene.height) + "] /Resources << /Font << /F1 4 0 R >> >> /Contents 5 0 R >>");
    object(4, "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>");
    object(5, "<< /Length " + std::to_string(stream.size()) + " >>\nstream\n" + stream + "endstream");
    const auto xref = file.tellp();
    file << "xref\n0 6\n0000000000 65535 f \n";
    for (int index = 1; index <= 5; ++index) file << std::setw(10) << std::setfill('0') << offsets[index] << " 00000 n \n";
    file << "trailer << /Size 6 /Root 1 0 R >>\nstartxref\n" << xref << "\n%%EOF\n";
    checkFile(file, "plot: failed writing PDF output");
}
void writePng(const std::string& filename, std::uint32_t width, std::uint32_t height,
              const std::vector<std::uint8_t>& rgba, double dpi) {
    if (!width || !height || rgba.size() != std::size_t(width) * height * 4 || !std::isfinite(dpi) || dpi <= 0)
        throw std::invalid_argument("plot: invalid PNG image");
    FILE* raw = std::fopen(filename.c_str(), "wb");
    if (!raw) throw std::runtime_error("plot: cannot open PNG output");
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    png_infop info = png_create_info_struct(png);
    if (!png || !info) { if (png) png_destroy_write_struct(&png, &info); std::fclose(raw); throw std::runtime_error("plot: PNG initialization failed"); }
    if (setjmp(png_jmpbuf(png))) { png_destroy_write_struct(&png, &info); std::fclose(raw); throw std::runtime_error("plot: PNG write failed"); }
    png_init_io(png, raw);
    png_set_IHDR(png, info, width, height, 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    const png_uint_32 pixelsPerMeter = static_cast<png_uint_32>(dpi / 0.0254);
    png_set_pHYs(png, info, pixelsPerMeter, pixelsPerMeter, PNG_RESOLUTION_METER);
    png_write_info(png, info);
    std::vector<png_bytep> rows(height);
    for (std::uint32_t row = 0; row < height; ++row) rows[row] = const_cast<png_bytep>(rgba.data() + std::size_t(row) * width * 4);
    png_write_image(png, rows.data());
    png_write_end(png, nullptr);
    png_destroy_write_struct(&png, &info);
    std::fclose(raw);
}
} // namespace modules::plot