#include "modules/plot/geometry.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {
using Clock = std::chrono::steady_clock;
double milliseconds(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}
} // namespace
int main(int argc, char** argv) {
    using namespace modules::plot;
    const std::size_t count = argc > 1 ? std::strtoull(argv[1], nullptr, 10) : 10000;
    if (count != 10000 && count != 100000 && count != 1000000)
        return 2;
    try {
        std::vector<double> x(count), y(count);
        for (std::size_t i = 0; i < count; ++i) {
            x[i] = 100.0 * i / (count - 1);
            y[i] = std::sin(x[i]);
        }
        auto begin = Clock::now();
        Series series;
        series.data = Data(x, y);
        Axes axes;
        axes.fit({series.data});
        const double load = milliseconds(begin);
        const Viewport viewport{0, 0, 1000, 600};
        // 固定操作次数、预热后测量，避免帧率或机器速度改变工作量。
        for (int i = 0; i < 3; ++i)
            tessellate(series, axes, viewport);
        std::vector<double> pan, zoom, update;
        std::size_t peakVertices = 0;
        for (int round = 0; round < 20; ++round) {
            begin = Clock::now();
            axes.x.setRange({round * 0.1, 100 + round * 0.1});
            auto geometry = tessellate(series, axes, viewport);
            pan.push_back(milliseconds(begin));
            peakVertices = std::max(peakVertices, geometry.size());
            begin = Clock::now();
            axes.x.setRange({25 - round * 0.01, 75 + round * 0.01});
            geometry = tessellate(series, axes, viewport);
            zoom.push_back(milliseconds(begin));
            begin = Clock::now();
            const auto changed = series.data.replace(count / 2, {x[count / 2]}, {0.5});
            if (changed.blockIdentity(0) != series.data.blockIdentity(0))
                throw std::runtime_error("local update copied prefix");
            update.push_back(milliseconds(begin));
        }
        PointIndex index;
        begin = Clock::now();
        index.rebuild(series.data, axes, viewport);
        const double indexBuild = milliseconds(begin);
        begin = Clock::now();
        for (int i = 0; i < 1000; ++i)
            index.nearest({double(i), 300}, 8);
        const double pick = milliseconds(begin) / 1000;
        const auto percentile = [](std::vector<double> values, double p) {
            std::sort(values.begin(), values.end());
            return values[std::size_t(std::ceil(p * values.size())) - 1];
        };
        std::cout << "{\"samples\":" << count << ",\"rounds\":20,\"load_ms\":" << load
                  << ",\"pan_p95_ms\":" << percentile(pan, 0.95)
                  << ",\"pan_p99_ms\":" << percentile(pan, 0.99)
                  << ",\"zoom_p95_ms\":" << percentile(zoom, 0.95)
                  << ",\"zoom_p99_ms\":" << percentile(zoom, 0.99)
                  << ",\"update_p95_ms\":" << percentile(update, 0.95) << ",\"index_build_ms\":" << indexBuild
                  << ",\"pick_mean_ms\":" << pick
                  << ",\"peak_vertex_bytes\":" << peakVertices * sizeof(Vertex)
                  << ",\"gpu_ms\":null,\"process_gpu_bytes\":null,\"idle_redraws\":null}\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
