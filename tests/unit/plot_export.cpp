#include "modules/plot/export.h"
#include "3rd/stb_image.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <locale>
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
        scene.paths[0].color[3] = .5f;
        scene.paths[0].points[0].x = 10.123456789012345;
        scene.texts[0].color[3] = .25f;
        struct Comma : std::numpunct<char> {
            char do_decimal_point() const override { return ','; }
        };
        const auto previousLocale = std::locale();
        std::locale::global(std::locale(std::locale::classic(), new Comma));
        writeSvg(scene, "plot_export_test.svg");
        writePdf(scene, "plot_export_test.pdf");
        std::locale::global(previousLocale);
        const auto alphaSvg = read("plot_export_test.svg"), alphaPdf = read("plot_export_test.pdf");
        require(alphaSvg.find("opacity=\"0.5\"") != std::string::npos &&
                    alphaSvg.find("fill-opacity=\"0.25\"") != std::string::npos,
                "SVG path/text alpha");
        require(alphaSvg.find("10.123456789012") != std::string::npos &&
                    alphaPdf.find("10.123456789012") != std::string::npos,
                "export coordinate precision/locale");
        require(alphaPdf.find("/CA 0.5 /ca 0.5") != std::string::npos &&
                    alphaPdf.find("/ca 0.25") != std::string::npos &&
                    alphaPdf.find("q /Path0 gs") != std::string::npos &&
                    alphaPdf.find("q /Text0 gs") != std::string::npos,
                "PDF isolated alpha states");
        for (int invalid = 0; invalid < 4; ++invalid) {
            auto broken = scene;
            if (invalid == 0)
                broken.paths[0].points[0].x = std::numeric_limits<double>::quiet_NaN();
            if (invalid == 1)
                broken.paths[0].lineWidth = -1;
            if (invalid == 2)
                broken.paths[0].color[0] = 2;
            if (invalid == 3)
                broken.paths[0].lineWidth = std::numeric_limits<float>::infinity();
            for (bool pdfOutput : {false, true}) {
                bool rejected = false;
                try {
                    if (pdfOutput)
                        writePdf(broken, "plot_export_test.pdf");
                    else
                        writeSvg(broken, "plot_export_test.svg");
                } catch (const std::invalid_argument&) {
                    rejected = true;
                }
                require(rejected, "invalid export path accepted");
            }
        }
        scene.paths[0].points[0].x = 1e-25;
        writePdf(scene, "plot_export_test.pdf");
        require(read("plot_export_test.pdf").find("0.0000000000000000000000001") != std::string::npos,
                "tiny PDF coordinate truncated or scientific notation emitted");
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
