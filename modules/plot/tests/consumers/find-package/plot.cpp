#include "modules/plot/plot.h"
#include "modules/plot/session.h"
#include "modules/plot/waveform.h"

int main() {
    modules::plot::PlotSession session;
    auto plot = session.make<modules::plot::Plot>();
    auto incoming = session.mailbox<std::vector<modules::plot::Series>>();
    modules::plot::Series line;
    line.data = modules::plot::Data({0, 1}, {-2, 3});
    incoming->publish({line});
    plot->setSeries(std::move(*incoming->take()));
    modules::plot::WaveformBuffer buffer(modules::plot::WaveformBuffer::Config::forDuration(2, 1000, 1));
    const bool valid = plot->axes().y.range().min == -2 && plot->axes().y.range().max == 3 &&
                       buffer.reservedBytes() < 1024 * 1024;
    session.shutdownHandler()();
    return valid ? 0 : 1;
}
