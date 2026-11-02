#include "modules/plot/data.h"
#include "modules/plot/axes.h"
#include "modules/plot/field.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
int main() {
    try {
        using namespace modules::plot;
        Data raw({0, 1, 2, 3}, {1.25, -2.5, 3.75, 4.0});
        const Data updated = raw.replace(1, {1}, {-3.0});
        if (raw.at(1).y != -2.5 || updated.at(1).y != -3.0)
            throw std::runtime_error("Immutable snapshot contract failed");
        Axes axes;
        axes.fit({updated});
        const auto screen = axes.toScreen(updated.at(2), {0, 0, 800, 400});
        if (!screen) throw std::runtime_error("Mapping failed");
        const auto restored = axes.toData(*screen, {0, 0, 800, 400});
        if (!restored || std::abs(restored->y - updated.at(2).y) > 1e-12)
            throw std::runtime_error("Round-trip exceeded this example's tolerance");
        ColorScale scale;
        scale.setColorMap(ColorMap::Grayscale);
        scale.setRange({-1e308, 1e308});
        if (scale.map(0)[0] != 0.5f || scale.map(1e308)[0] != 1.f)
            throw std::runtime_error("Large color range mapping failed");
        std::cout << "Installed binary SDK: data, axes and color scale passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
