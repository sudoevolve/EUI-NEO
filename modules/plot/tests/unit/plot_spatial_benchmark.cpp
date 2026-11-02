#include "modules/plot/export.h"
#include "modules/plot/volume.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace modules::plot;
int main(int argc, char **argv) {
    try {
        const double step = argc > 1 ? std::stod(argv[1]) : 0.08;
        const std::string kind = argc > 2 ? argv[2] : "volume";
        if (kind != "volume" && kind != "slice" && kind != "isosurface")
            throw std::invalid_argument("unknown benchmark scene");
        VolumeLayout layout{{25, 25, 25}, {1.0 / 12, 1.0 / 12, 1.0 / 12}, {-1, -1, -1}};
        auto data = std::make_shared<VolumeData>(
            layout,
            [layout](const VolumeBrick &b, std::vector<double> &out) {
                for (std::size_t z = 0; z < b.size[2]; ++z)
                    for (std::size_t y = 0; y < b.size[1]; ++y)
                        for (std::size_t x = 0; x < b.size[0]; ++x) {
                            const auto p = layout.origin + Vec3{(x + b.first[0]) * layout.spacing.x,
                                                                (y + b.first[1]) * layout.spacing.y,
                                                                (z + b.first[2]) * layout.spacing.z};
                            out[(z * b.size[1] + y) * b.size[0] + x] = std::exp(-dot(p, p) * 5);
                        }
            },
            1024 * 1024, 8);
        Scene3D scene;
        scene.bounds = layout.bounds();
        scene.showAxes = true;
        if (kind == "volume") {
            auto layer = std::make_shared<VolumeLayer>();
            layer->data = data;
            layer->referenceStep = 0.1;
            layer->transfer = TransferFunction({{0, {0, 0, 0, 0}},
                                                {0.05, {0, 0, 1, 0}},
                                                {0.3, {0.1f, 0.6f, 1, 0.1f}},
                                                {1, {1, 0.3f, 0, 0.5f}}});
            scene.volume = layer;
        } else {
            auto geometry = kind == "slice" ? volumeSlice(*data, SliceLink(layout).plane(2))
                                            : isoSurface(*data, 0.3, 8 * 1024 * 1024);
            Material3D material;
            material.color = {1, 1, 1, 1};
            material.scalarColors = true;
            scene.objects.push_back({std::make_shared<Geometry3D>(std::move(geometry)), material, true});
        }
        RenderSettings3D settings;
        settings.volumeStep = step;
        settings.workingBytes = 16 * 1024 * 1024;
        SceneRenderer3D renderer(scene, settings);
        Camera3D camera;
        camera.fit(scene.bounds);
        for (int i = 0; i < 2; ++i)
            renderer.render(camera, 160, 120);
        std::vector<double> timings;
        Rendered3D frame;
        for (int i = 0; i < 8; ++i) {
            camera.orbit(0.02, 0);
            frame = renderer.render(camera, 160, 120);
            timings.push_back(frame.milliseconds);
        }
        std::sort(timings.begin(), timings.end());
        std::cout << "{\"scene\":\"" << kind
                  << "\",\"dimensions\":[25,25,25],\"viewport\":[160,120],\"step\":" << step
                  << ",\"repeats\":8,\"median_ms\":" << timings[4] << ",\"p95_ms\":" << timings[7]
                  << ",\"p99_ms\":" << timings[7] << ",\"volume_samples\":" << frame.volumeSamples
                  << ",\"cache_bytes\":" << data->cacheBytes() << ",\"brick_loads\":" << data->loads()
                  << ",\"working_budget_bytes\":" << settings.workingBytes
                  << ",\"gpu_ms\":null,\"process_gpu_bytes\":null}\n";
        if (argc > 3)
            writePng(argv[3], frame.width, frame.height, frame.rgba);
        // A 1 GiB logical Float64 source stays within a 1 MiB cache during sparse sampling.
        VolumeLayout large{{512, 512, 512}, {1, 1, 1}, {}};
        VolumeData streamed(
            large,
            [](const VolumeBrick &b, std::vector<double> &out) {
                for (std::size_t z = 0; z < b.size[2]; ++z)
                    for (std::size_t y = 0; y < b.size[1]; ++y)
                        for (std::size_t x = 0; x < b.size[0]; ++x)
                            out[(z * b.size[1] + y) * b.size[0] + x] =
                                double(x + b.first[0] + y + b.first[1] + z + b.first[2]);
            },
            1024 * 1024, 16);
        for (int i = 0; i < 128; ++i) {
            const Vec3 p{double((i * 17) % 511) + 0.25, double((i * 31) % 511) + 0.25,
                         double((i * 47) % 511) + 0.25};
            const auto v = streamed.sample(p);
            if (!v || std::abs(*v - p.x - p.y - p.z) > 1e-8 || streamed.cacheBytes() > 1024 * 1024)
                throw std::runtime_error("streaming budget/value mismatch");
        }
        std::cout << "{\"scene\":\"sparse_large_volume\",\"logical_bytes\":" << large.sampleCount() * 8
                  << ",\"cache_bytes\":" << streamed.cacheBytes() << ",\"loads\":" << streamed.loads()
                  << "}\n";
        streamed.clearCache();
        data->clearCache();
        if (streamed.cacheBytes() || data->cacheBytes())
            throw std::runtime_error("cache not released");
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "plot_spatial_benchmark: " << e.what() << '\n';
        return 1;
    }
}
