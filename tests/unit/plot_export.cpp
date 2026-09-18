#include "modules/plot/export.h"

#include <cstdio>
#include <fstream>
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