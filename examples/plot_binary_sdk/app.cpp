#include "eui_neo.h"
#include "modules/plot/plot.h"
#include "modules/plot/session.h"
namespace app {
namespace {
modules::plot::PlotSession charts;
auto chart = charts.make<modules::plot::Plot>();
bool initialized = false;
}
const DslAppConfig& dslAppConfig() {
    static const auto config = DslAppConfig{}
        .title("Plot binary SDK example")
        .windowSize(900, 600)
        .minWindowSize(320, 240)
        .onShutdown(charts.shutdownHandler());
    return config;
}
void compose(eui::Ui& ui, const eui::Screen& screen) {
    if (!initialized) {
        modules::plot::Series line;
        line.name = "Original measurement";
        line.data = modules::plot::Data({0, 1, 2, 3}, {1.25, -2.5, 3.75, 4});
        line.curve = modules::plot::CurveKind::Raw;
        chart->setSeries({line});
        chart->setTitle("Drag: pan | Wheel: zoom | Right click: reset");
        initialized = true;
    }
    // 1.0 is the example's fixed offscreen pixel ratio, not automatic DPI detection.
    chart->compose(ui, "measurement", screen.width, screen.height, 1.0);
}
}
