#include "modules/plot/software_raster.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>

using namespace modules::plot;
namespace {
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
void rectangle(bool reverse, double dpi) {
    Batch square{{{1, 1}, {9, 1}, {9, 9}, {1, 1}, {9, 9}, {1, 9}}, {1, 0, 0, .5f}};
    if (reverse)
        for (std::size_t i = 0; i < square.vertices.size(); i += 3)
            std::swap(square.vertices[i], square.vertices[i + 2]);
    const int size = static_cast<int>(10 * dpi);
    const auto pixels = detail::rasterize({square}, size, size, dpi);
    const auto flipped = detail::rasterize({square}, size, size, dpi, true);
    for (int y = 0; y < size; ++y)
        for (int x = 0; x < size; ++x) {
            const bool covered = x + .5 >= dpi && x + .5 < 9 * dpi && y + .5 >= dpi && y + .5 < 9 * dpi;
            const auto p = static_cast<std::size_t>(y * size + x) * 4;
            const auto q = static_cast<std::size_t>((size - 1 - y) * size + x) * 4;
            require(std::equal(pixels.begin() + p, pixels.begin() + p + 4, flipped.begin() + q),
                    "bottom-up texture differs from top-left raster output");
            require(pixels[p] == (covered ? 133 : 10) && pixels[p + 1] == (covered ? 7 : 13) &&
                        pixels[p + 2] == (covered ? 9 : 18) && pixels[p + 3] == 255,
                    "shared triangle edge blended twice or left a crack");
        }
}
} // namespace
int main() {
    try {
        for (double dpi : {1., 1.25, 1.5, 2.})
            for (bool reverse : {false, true})
                rectangle(reverse, dpi);
        Batch a{{{0, 0}, {4, 0}, {0, 4}}, {1, 0, 0, .5f}};
        auto pixels = detail::rasterize({a, a}, 4, 4, 1);
        require(pixels[0] == 194, "distinct overlapping primitives must blend independently");
        std::cout << "plot_raster: shared edges, winding, DPI and overlap passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
