#include "modules/plot/vector.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
}

int main() {
    using namespace modules::plot;
    try {
        RectilinearField x({0, 1, 2}, {0, 1, 2}, {1, 1, 1, 1, 1, 1, 1, 1, 1});
        RectilinearField y({0, 1, 2}, {0, 1, 2}, {0, 0, 0, 0, 0, 0, 0, 0, 0});
        VectorField field(x, y);
        const auto arrows = vectorArrows(field, 0.25);
        require(arrows.size() == 9 && arrows[4].sourceIndex == 4 && arrows[4].to.x == 1.25,
                "constant vector arrows");
        const auto lines = streamlines(field, {{0.5, 1}}, 0.25, 4, 1e-12, false);
        require(lines.size() == 1 && lines[0].sourceIndex == 0 && lines[0].points.size() == 5,
                "forward RK4 streamline");
        require(std::abs(lines[0].points.back().x - 1.5) < 1e-12 && lines[0].points.back().y == 1,
                "RK4 displacement");
        const auto both = streamlines(field, {{1, 1}}, 0.25, 2, 1e-12, true);
        require(both.size() == 1 && both[0].points.size() == 5 && both[0].points[2].x == 1,
                "bidirectional RK4 streamline");
        RectilinearField zero({0, 1}, {0, 1}, {0, 0, 0, 0});
        require(streamlines(VectorField(zero, zero), {{0.5, 0.5}}, 0.1, 10).front().points.size() == 1,
                "low speed termination");
        Axes axes;
        axes.x.setRange({0, 2});
        axes.y.setRange({0, 2});
        require(arrowGeometry(arrows, axes, {0, 0, 100, 100}).size() % 3 == 0, "arrow triangles");
        require(streamlineGeometry(lines[0], axes, {0, 0, 100, 100}).size() % 3 == 0,
                "streamline triangles");
        try {
            streamlines(field, {{0, 0}}, 0, 1);
            throw std::runtime_error("invalid streamline accepted");
        } catch (const std::invalid_argument&) {
        }
        try {
            VectorField(x, RectilinearField({0, 2}, {0, 1}, {0, 0, 0, 0}));
            throw std::runtime_error("mismatched vector grids accepted");
        } catch (const std::invalid_argument&) {
        }
        std::cout << "plot_vector: passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}