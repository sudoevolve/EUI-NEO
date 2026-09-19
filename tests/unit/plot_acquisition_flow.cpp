// Exercise the actual starter's acquisition/mailbox/pause flow across multiple history wraps.
#include "examples/scientific_plot_acquisition.cpp"
#include <iostream>

int main() {
    auto& app = app::model();
    auto shutdown = app.session.shutdownHandler([&] { app.stop(); });
    try {
        const auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(2400)) {
            // Deliberately much slower than the production 60 Hz UI.
            std::this_thread::sleep_for(std::chrono::milliseconds(120));
            app.update();
        }
        if (!app.error.empty() || app.latestSample < 400000)
            throw std::runtime_error("starter stalled after history wrap / slow UI");
        app.acquiring = false;
        app.follow = false;
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        app.update();
        const auto frozen = app.latestSample;
        auto axes = app.waveform->axes();
        axes.x.setRange({.01, .02});
        app.waveform->setAxes(axes);
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        app.update();
        if (app.latestSample != frozen || app.waveform->axes().x.range().min != .01)
            throw std::runtime_error("paused starter overwrote manual view");
        app.follow = true;
        app.acquiring = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        app.update();
        if (app.latestSample <= frozen)
            throw std::runtime_error("starter did not resume");
        shutdown();
        shutdown();
        std::cout << "Acquisition starter: wrap, slow UI, pause/manual view, resume and shutdown passed\n";
        return 0;
    } catch (const std::exception& e) {
        shutdown();
        std::cerr << e.what() << '\n';
        return 1;
    }
}
