#include "eui_neo.h"
#include "modules/plot/plot.h"
#include "modules/plot/session.h"

namespace app {
namespace {
modules::plot::PlotSession charts;
auto plot = charts.make<modules::plot::Plot>();
} // namespace
const DslAppConfig& dslAppConfig() {
    static const auto config = DslAppConfig{}
                                   .title("Minimal scientific plot")
                                   .windowSize(900, 600)
                                   .minWindowSize(320, 240)
                                   .onShutdown(charts.shutdownHandler());
    return config;
}
void compose(eui::Ui& ui, const eui::Screen& screen) {
    if (plot->series().empty()) {
        modules::plot::Series line;
        line.name = "Measured signal";
        line.data = modules::plot::Data({0, 1, 2, 3, 4}, {1, 3, 2, 5, 4});
        plot->setSeries({line});
        plot->setTitle("Drag to pan | Wheel to zoom | Right click to reset");
    }
    plot->compose(ui, "plot", screen.width, screen.height, 2);
}
} // namespace app
