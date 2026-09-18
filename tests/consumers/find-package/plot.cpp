#include "modules/plot/plot.h"

int main() {
    modules::plot::Plot plot;
    modules::plot::Series line;
    line.data = modules::plot::Data({0, 1}, {-2, 3});
    plot.setSeries({line});
    return plot.axes().y.range().min == -2 && plot.axes().y.range().max == 3 ? 0 : 1;
}
