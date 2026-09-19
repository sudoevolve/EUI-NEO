#include "modules/plot/export.h"
#include "3rd/stb_image.h"

#include <cstdio>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
std::string read(const char* filename) {
    std::ifstream file(filename, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
}

int main() {
    using namespace modules::plot;
    try {
        ExportScene scene;
        scene.width = 100;
        scene.height = 80;
        scene.paths.push_back({{{10, 10}, {90, 60}, {10, 60}}, true, true, {1, 0, 0, 1}, 2});
        scene.texts.push_back({{12, 24}, "x < y & test", 14, {1, 1, 1, 1}});
        writeSvg(scene, "plot_export_test.svg");
        writePdf(scene, "plot_export_test.pdf");
        writePng("plot_export_test.png", 2, 3,
                 {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 0, 255, 20, 20, 20, 255,
                  80, 80, 80, 255},
                 144);
        const auto svg = read("plot_export_test.svg");
        const auto pdf = read("plot_export_test.pdf");
        const auto png = read("plot_export_test.png");
        require(svg.find("<path") != std::string::npos && svg.find("&lt;") != std::string::npos,
                "SVG structure");
        require(pdf.find("%PDF-1.4") == 0 && pdf.find("/Type /Page") != std::string::npos &&
                    pdf.find("x < y") != std::string::npos,
                "PDF structure");
        require(png.size() > 24 && png.compare(1, 3, "PNG") == 0 && png[16] == 0 && png[19] == 2 && png[23] == 3,
                "PNG signature and dimensions");
        ExportScene raster;
        raster.width = 100;
        raster.height = 80;
        raster.rasters.push_back({{5, 6}, 90, 60, 2, 2,
            {255,0,0,255, 0,255,0,128, 0,0,255,0, 255,255,255,255}});
        raster.texts.push_back({{12, 24}, "alpha (a) \\ beta", 14, {1,1,1,1}});
        writeSvg(raster, "plot_export_test.svg");
        writePdf(raster, "plot_export_test.pdf");
        const auto rasterSvg = read("plot_export_test.svg");
        const auto rasterPdf = read("plot_export_test.pdf");
        require(rasterSvg.find("<image x=\"5\" y=\"6\"") != std::string::npos &&
                rasterSvg.find("data:image/png;base64,iVBOR") != std::string::npos &&
                rasterSvg.find("<text") != std::string::npos, "SVG embedded PNG/text");
        require(rasterPdf.find("/Subtype /Image") != std::string::npos &&
                rasterPdf.find("/SMask 7 0 R") != std::string::npos &&
                rasterPdf.find("/Im0 6 0 R") != std::string::npos &&
                rasterPdf.find("alpha \\(a\\) \\\\ beta") != std::string::npos, "PDF raster/alpha/text resources");
        const auto font = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() /
                          "assets/JingNanJunJunTi-JinNanJunJunTi-Bold-2.ttf";
        writeRasterPng(raster, "plot_export_test.png", font.string(), 192);
        const auto annotatedPng = read("plot_export_test.png");
        require(annotatedPng[19] == 100 && annotatedPng[23] == 80 && annotatedPng.find("pHYs") != std::string::npos,
                "annotated PNG dimensions/DPI");
        int decodedWidth=0,decodedHeight=0,channels=0;
        auto decoded=stbi_load("plot_export_test.png",&decodedWidth,&decodedHeight,&channels,4);
        require(decoded&&decodedWidth==100&&decodedHeight==80,"PNG decode failed");
        const bool validPixels=decoded[(10*100+10)*4]==255 && decoded[(10*100+90)*4+1]==128 &&
                               decoded[(55*100+10)*4+2]==0;
        stbi_image_free(decoded);
        require(validPixels,"PNG raster position/alpha pixels");
        bool rejected = false;
        try { writeRasterPng(raster, "plot_export_test.png", font.string(), 96, 10); }
        catch (const std::length_error&) { rejected = true; }
        require(rejected, "PNG budget not enforced");
        std::remove("plot_export_test.svg");
        std::remove("plot_export_test.pdf");
        std::remove("plot_export_test.png");
        std::cout << "plot_export: passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
