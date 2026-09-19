#include "modules/plot/export.h"

#include <png.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace modules::plot {
namespace {
void validateScene(const ExportScene& scene) {
    if (!std::isfinite(scene.width) || !std::isfinite(scene.height) || scene.width <= 0 || scene.height <= 0)
        throw std::invalid_argument("plot: invalid export scene size");
    const auto validColor = [](const std::array<float, 4>& color) {
        for (float c : color)
            if (!std::isfinite(c) || c < 0 || c > 1) throw std::invalid_argument("plot: invalid export color");
    };
    validColor(scene.background);
    for (const auto& text : scene.texts) {
        validColor(text.color);
        if (!std::isfinite(text.fontSize) || text.fontSize <= 0 || !std::isfinite(text.position.x) || !std::isfinite(text.position.y))
            throw std::invalid_argument("plot: invalid export text");
    }
    for (const auto& raster : scene.rasters)
        if (!raster.pixelWidth || !raster.pixelHeight || !std::isfinite(raster.width) ||
            !std::isfinite(raster.height) || raster.width <= 0 || raster.height <= 0 ||
            !std::isfinite(raster.position.x) || !std::isfinite(raster.position.y) ||
            std::uint64_t(raster.pixelWidth) * raster.pixelHeight > std::numeric_limits<std::size_t>::max() / 4 ||
            std::uint64_t(raster.pixelWidth) * raster.pixelHeight * 4 != raster.rgba.size())
            throw std::invalid_argument("plot: invalid export raster");
}
std::vector<unsigned char> pngBytes(const ExportRaster& raster) {
    png_image image{};
    image.version = PNG_IMAGE_VERSION;
    image.width = raster.pixelWidth;
    image.height = raster.pixelHeight;
    image.format = PNG_FORMAT_RGBA;
    png_alloc_size_t size = 0;
    if (!png_image_write_to_memory(&image, nullptr, &size, 0, raster.rgba.data(), 0, nullptr))
        throw std::runtime_error("plot: PNG memory size failed");
    std::vector<unsigned char> result(size);
    if (!png_image_write_to_memory(&image, result.data(), &size, 0, raster.rgba.data(), 0, nullptr))
        throw std::runtime_error("plot: PNG memory write failed");
    result.resize(size);
    return result;
}
std::string base64(const std::vector<unsigned char>& bytes) {
    static constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    result.reserve((bytes.size() + 2) / 3 * 4);
    for (std::size_t i = 0; i < bytes.size(); i += 3) {
        const auto a = bytes[i];
        const auto b = i + 1 < bytes.size() ? bytes[i + 1] : 0;
        const auto c = i + 2 < bytes.size() ? bytes[i + 2] : 0;
        result += alphabet[a >> 2]; result += alphabet[((a & 3) << 4) | (b >> 4)];
        result += i + 1 < bytes.size() ? alphabet[((b & 15) << 2) | (c >> 6)] : '=';
        result += i + 2 < bytes.size() ? alphabet[c & 63] : '=';
    }
    return result;
}
std::string escapePdf(const std::string& value) {
    std::string result;
    for (unsigned char c : value) {
        if (c == '(' || c == ')' || c == '\\') result += '\\';
        if (c == '\r' || c == '\n') result += ' ';
        else result += char(c);
    }
    return result;
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
    file << "<rect width=\"100%\" height=\"100%\" fill=\"" << color(scene.background)
         << "\" fill-opacity=\"" << scene.background[3] << "\"/>\n";
    for (const auto& raster : scene.rasters)
        file << "<image x=\"" << raster.position.x << "\" y=\"" << raster.position.y
             << "\" width=\"" << raster.width << "\" height=\"" << raster.height
             << "\" preserveAspectRatio=\"none\" href=\"data:image/png;base64," << base64(pngBytes(raster)) << "\"/>\n";
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
    content << "q /Background gs " << scene.background[0] << " " << scene.background[1] << " " << scene.background[2] << " rg 0 0 "
            << scene.width << " " << scene.height << " re f Q\n";
    for (std::size_t i = 0; i < scene.rasters.size(); ++i) {
        const auto& r = scene.rasters[i];
        content << "q " << r.width << " 0 0 " << r.height << " " << r.position.x << " "
                << scene.height - r.position.y - r.height << " cm /Im" << i << " Do Q\n";
    }
    for (const auto& path : scene.paths) {
        if (path.points.empty()) continue;
        content << path.color[0] << " " << path.color[1] << " " << path.color[2] << " RG " << path.lineWidth << " w\n"
                << path.color[0] << " " << path.color[1] << " " << path.color[2] << " rg\n"
                << path.points.front().x << " " << scene.height - path.points.front().y << " m\n";
        for (std::size_t index = 1; index < path.points.size(); ++index)
            content << path.points[index].x << " " << scene.height - path.points[index].y << " l\n";
        if (path.closed) content << "h\n";
        content << (path.filled ? "B\n" : "S\n");
    }
    for (const auto& text : scene.texts)
        content << text.color[0] << " " << text.color[1] << " " << text.color[2] << " rg BT /F1 " << text.fontSize << " Tf " << text.position.x << " " << scene.height - text.position.y
                << " Td (" << escapePdf(text.text) << ") Tj ET\n";
    const std::string stream = content.str();
    std::ofstream file(filename, std::ios::binary);
    checkFile(file, "plot: cannot open PDF output");
    const auto objectCount = 5 + scene.rasters.size() * 2;
    std::vector<std::streamoff> offsets(objectCount + 1);
    file << "%PDF-1.4\n%\xE2\xE3\xCF\xD3\n";
    auto object = [&](int number, const std::string& body) {
        offsets[number] = file.tellp();
        file << number << " 0 obj\n" << body << "\nendobj\n";
    };
    object(1, "<< /Type /Catalog /Pages 2 0 R >>");
    object(2, "<< /Type /Pages /Kids [3 0 R] /Count 1 >>");
    std::string resources = " /XObject <<";
    for (std::size_t i = 0; i < scene.rasters.size(); ++i)
        resources += " /Im" + std::to_string(i) + " " + std::to_string(6 + i * 2) + " 0 R";
    resources += " >> /ExtGState << /Background << /Type /ExtGState /ca " + std::to_string(scene.background[3]) + " >> >>";
    object(3, "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 " + std::to_string(scene.width) + " " +
                  std::to_string(scene.height) + "] /Resources << /Font << /F1 4 0 R >>" + resources + " >> /Contents 5 0 R >>");
    object(4, "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>");
    object(5, "<< /Length " + std::to_string(stream.size()) + " >>\nstream\n" + stream + "endstream");
    for (std::size_t i = 0; i < scene.rasters.size(); ++i) {
        const auto& r = scene.rasters[i];
        std::string rgb, alpha;
        rgb.reserve(r.rgba.size() / 4 * 3); alpha.reserve(r.rgba.size() / 4);
        for (std::size_t p = 0; p < r.rgba.size(); p += 4) {
            rgb.append(reinterpret_cast<const char*>(r.rgba.data() + p), 3);
            alpha += static_cast<char>(r.rgba[p + 3]);
        }
        const std::string dimensions = " /Width " + std::to_string(r.pixelWidth) + " /Height " + std::to_string(r.pixelHeight);
        object(static_cast<int>(6 + i * 2), "<< /Type /XObject /Subtype /Image" + dimensions +
            " /ColorSpace /DeviceRGB /BitsPerComponent 8 /SMask " + std::to_string(7 + i * 2) +
            " 0 R /Length " + std::to_string(rgb.size()) + " >>\nstream\n" + rgb + "\nendstream");
        object(static_cast<int>(7 + i * 2), "<< /Type /XObject /Subtype /Image" + dimensions +
            " /ColorSpace /DeviceGray /BitsPerComponent 8 /Length " + std::to_string(alpha.size()) +
            " >>\nstream\n" + alpha + "\nendstream");
    }
    const auto xref = file.tellp();
    file << "xref\n0 " << objectCount + 1 << "\n0000000000 65535 f \n";
    for (std::size_t index = 1; index <= objectCount; ++index) file << std::setw(10) << std::setfill('0') << offsets[index] << " 00000 n \n";
    file << "trailer << /Size " << objectCount + 1 << " /Root 1 0 R >>\nstartxref\n" << xref << "\n%%EOF\n";
    checkFile(file, "plot: failed writing PDF output");
}
void writePng(const std::string& filename, std::uint32_t width, std::uint32_t height,
              const std::vector<std::uint8_t>& rgba, double dpi) {
    if (!width || !height || std::uint64_t(width) * height > std::numeric_limits<std::size_t>::max() / 4 ||
        rgba.size() != std::size_t(width) * height * 4 || !std::isfinite(dpi) || dpi <= 0 || dpi > 1000000)
        throw std::invalid_argument("plot: invalid PNG image");
    std::vector<png_bytep> rows(height);
    for (std::uint32_t row = 0; row < height; ++row) rows[row] = const_cast<png_bytep>(rgba.data() + std::size_t(row) * width * 4);
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
    png_write_image(png, rows.data());
    png_write_end(png, nullptr);
    png_destroy_write_struct(&png, &info);
    std::fclose(raw);
}
void writeRasterPng(const ExportScene& scene, const std::string& filename, const std::string& fontFile,
                    double dpi, std::size_t budgetBytes) {
    validateScene(scene);
    if (!scene.paths.empty() || scene.width > 32768 || scene.height > 32768)
        throw std::invalid_argument("plot: raster PNG requires raster/text layers and bounded dimensions");
    const auto width = static_cast<std::uint32_t>(std::ceil(scene.width));
    const auto height = static_cast<std::uint32_t>(std::ceil(scene.height));
    if (std::uint64_t(width) * height > budgetBytes / 4)
        throw std::length_error("plot: PNG raster budget exceeded");
    std::vector<std::uint8_t> pixels(std::size_t(width) * height * 4);
    for (std::size_t i = 0; i < pixels.size(); ++i)
        pixels[i] = static_cast<std::uint8_t>(std::lround(std::clamp(scene.background[i % 4], 0.f, 1.f) * 255));
    auto blend = [&](int x, int y, std::array<float, 4> color) {
        if (x < 0 || y < 0 || x >= static_cast<int>(width) || y >= static_cast<int>(height)) return;
        const auto p = (std::size_t(y) * width + x) * 4;
        const double destination = pixels[p + 3] / 255.0;
        const double alpha = color[3] + destination * (1 - color[3]);
        for (int c = 0; c < 3; ++c) {
            const double value = color[c] * color[3] + pixels[p + c] / 255.0 * destination * (1 - color[3]);
            pixels[p + c] = std::uint8_t(std::lround(std::clamp(alpha > 0 ? value / alpha : 0.0, 0.0, 1.0) * 255));
        }
        pixels[p + 3] = std::uint8_t(std::lround(std::clamp(alpha, 0.0, 1.0) * 255));
    };
    for (const auto& r : scene.rasters) {
        for (std::uint32_t y = 0; y < height; ++y) for (std::uint32_t x = 0; x < width; ++x) {
            const double u = (x + 0.5 - r.position.x) / r.width, v = (y + 0.5 - r.position.y) / r.height;
            if (u < 0 || v < 0 || u >= 1 || v >= 1) continue;
            const auto source = (std::size_t(v * r.pixelHeight) * r.pixelWidth + std::size_t(u * r.pixelWidth)) * 4;
            std::array<float, 4> color;
            for (int c = 0; c < 4; ++c) color[c] = r.rgba[source + c] / 255.f;
            blend(static_cast<int>(x), static_cast<int>(y), color);
        }
    }
    struct Font {
        FT_Library library = nullptr;
        FT_Face face = nullptr;
        ~Font() { if (face) FT_Done_Face(face); if (library) FT_Done_FreeType(library); }
    } font;
    if (!scene.texts.empty() && (FT_Init_FreeType(&font.library) || FT_New_Face(font.library, fontFile.c_str(), 0, &font.face)))
        throw std::runtime_error("plot: cannot load PNG export font");
    for (const auto& text : scene.texts) {
        if (!std::isfinite(text.fontSize) || text.fontSize <= 0 || text.fontSize > 4096 ||
            !std::isfinite(text.position.x) || !std::isfinite(text.position.y))
            throw std::invalid_argument("plot: invalid export text");
        if (FT_Set_Pixel_Sizes(font.face, 0, static_cast<FT_UInt>(std::ceil(text.fontSize))))
            throw std::runtime_error("plot: cannot size PNG export font");
        double cursor = text.position.x;
        FT_UInt previous = 0;
        for (std::size_t i = 0; i < text.text.size();) {
            const auto first = static_cast<unsigned char>(text.text[i++]);
            unsigned long code = first; int count = 0;
            if (first >= 0xC2 && first <= 0xDF) { code = first & 31; count = 1; }
            else if (first >= 0xE0 && first <= 0xEF) { code = first & 15; count = 2; }
            else if (first >= 0xF0 && first <= 0xF4) { code = first & 7; count = 3; }
            else if (first >= 0x80) throw std::invalid_argument("plot: invalid UTF-8 text");
            for (int k = 0; k < count; ++k) {
                if (i >= text.text.size() || (static_cast<unsigned char>(text.text[i]) & 0xC0) != 0x80)
                    throw std::invalid_argument("plot: incomplete UTF-8 text");
                code = (code << 6) | (static_cast<unsigned char>(text.text[i++]) & 63);
            }
            if ((count == 1 && code < 0x80) || (count == 2 && code < 0x800) ||
                (count == 3 && code < 0x10000) || code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF))
                throw std::invalid_argument("plot: invalid UTF-8 codepoint");
            auto glyph = FT_Get_Char_Index(font.face, code);
            if (!glyph) glyph = FT_Get_Char_Index(font.face, 0xFFFD);
            if (!glyph) glyph = FT_Get_Char_Index(font.face, '?');
            if (previous && glyph && FT_HAS_KERNING(font.face)) {
                FT_Vector delta{}; FT_Get_Kerning(font.face, previous, glyph, FT_KERNING_DEFAULT, &delta);
                cursor += delta.x / 64.0;
            }
            if (FT_Load_Glyph(font.face, glyph, FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL))
                throw std::runtime_error("plot: cannot rasterize export glyph");
            const auto& bitmap = font.face->glyph->bitmap;
            if (bitmap.pixel_mode != FT_PIXEL_MODE_GRAY && bitmap.width)
                throw std::runtime_error("plot: unsupported export glyph format");
            for (unsigned y = 0; y < bitmap.rows; ++y) for (unsigned x = 0; x < bitmap.width; ++x) {
                const auto row = bitmap.pitch >= 0 ? y : bitmap.rows - 1 - y;
                auto color = text.color;
                color[3] *= bitmap.buffer[row * std::abs(bitmap.pitch) + x] / 255.f;
                const double px = std::floor(cursor) + font.face->glyph->bitmap_left + x;
                const double py = text.position.y - font.face->glyph->bitmap_top + y;
                if (px >= 0 && py >= 0 && px < width && py < height) blend(static_cast<int>(px), static_cast<int>(py), color);
            }
            cursor += font.face->glyph->advance.x / 64.0; previous = glyph;
        }
    }
    writePng(filename, width, height, pixels, dpi);
}
} // namespace modules::plot
