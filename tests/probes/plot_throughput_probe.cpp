// Synthetic replay benchmark; --production exercises the public WaveformBuffer API.
#include "modules/plot/plot.h"
#include "modules/plot/waveform.h"
#include "core/dsl_runtime.h"
#include "core/render/render_backend.h"
#include "core/window/window_backend.h"
#include <glad/glad.h>
#if defined(EUI_WINDOW_BACKEND_SDL2)
#define SDL_MAIN_HANDLED
#include <SDL.h>
#else
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#endif
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <functional>
#include <iostream>
#include <thread>
#include "tests/probes/plot_stream_experiment.h"

namespace {
using Clock = std::chrono::steady_clock;
using namespace modules::plot;
constexpr double sampleRate = 125000000.;
double ms(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}
double percentile(std::vector<double> values, double p) {
    std::sort(values.begin(), values.end());
    return values.at(std::size_t(std::ceil(p * values.size())) - 1);
}
void stats(const char* name, const std::vector<double>& values) {
    std::cout << '"' << name << "\":{\"n\":" << values.size() << ",\"median_ms\":" << percentile(values, .5)
              << ",\"p95_ms\":" << percentile(values, .95) << ",\"p99_ms\":" << percentile(values, .99)
              << ",\"max_ms\":" << percentile(values, 1) << '}';
}
std::vector<std::uint8_t> sourceBytes() {
    std::vector<std::uint8_t> bytes(64 * 1024 * 1024);
    std::uint32_t state = 123456789;
    for (std::size_t i = 0; i < bytes.size() / 2; ++i) {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        bytes[2 * i] = std::uint8_t(32 + ((i / 31) % 160) + (state & 15));
        bytes[2 * i + 1] = std::uint8_t(24 + ((i / 71) % 180) + ((state >> 8) & 15));
        if (i % 104729 == 0)
            bytes[2 * i] = 255;
        if (i % 65537 == 0)
            bytes[2 * i + 1] = 0;
    }
    return bytes;
}
struct Context {
    core::window::Handle window{};
    std::unique_ptr<core::render::RenderBackend> backend;
    std::unique_ptr<core::render::ScopedRenderBackend> scope;
    core::dsl::Runtime runtime;
    core::dsl::Ui* ui = nullptr;
    Context() {
        core::render::initializeRenderBackendLoader();
#if defined(EUI_WINDOW_BACKEND_SDL2)
        SDL_SetMainReady();
        if (SDL_Init(SDL_INIT_VIDEO) != 0)
            throw std::runtime_error("SDL init failed");
#else
        if (!glfwInit())
            throw std::runtime_error("GLFW init failed");
#endif
        core::window::WindowCreateRequest request;
        request.width = 1920;
        request.height = 1080;
        request.title = "250 MB/s dual-channel line workload";
        request.renderApi = core::window::RenderApi::OpenGL;
        window = core::window::createWindow(request);
        backend = core::render::createRenderBackend(window);
        if (!window || !backend || !backend->initialize())
            throw std::runtime_error("OpenGL init failed");
        backend->makeCurrent();
        scope = std::make_unique<core::render::ScopedRenderBackend>(*backend);
#if defined(EUI_WINDOW_BACKEND_SDL2)
        SDL_GL_SetSwapInterval(0);
#else
        glfwSwapInterval(0);
#endif
        runtime.initialize(window);
        std::cout << "{\"gpu\":\"" << glGetString(GL_RENDERER)
                  << "\",\"viewport\":[1920,1080],\"sample_rate_per_channel\":125000000,\"input_bytes_per_"
                     "second\":250000000}\n";
    }
    void compose(Plot& plot, double dpi = 1) {
        runtime.compose("throughput", 1920, 1080, [&](core::dsl::Ui& value, const core::dsl::Screen&) {
            ui = &value;
            plot.compose(value, "scope", 1920, 1080, dpi);
        });
    }
    void present() {
        runtime.update(window, .016f, 1, 1);
        backend->beginFrame({window, core::window::nativeWindowInfo(window), 1920, 1080, 1});
        runtime.render(1920, 1080, 1);
        backend->present();
        glFinish();
#if defined(EUI_WINDOW_BACKEND_SDL2)
        SDL_PumpEvents();
#else
        glfwPollEvents();
#endif
    }
    ~Context() {
        runtime.shutdown();
        glFinish();
        scope.reset();
        backend.reset();
        core::window::destroyWindow(window);
#if defined(EUI_WINDOW_BACKEND_SDL2)
        SDL_Quit();
#else
        glfwTerminate();
#endif
    }
};
void baseline(Context& context, const std::vector<std::uint8_t>& raw) {
    for (std::size_t count : {65536u, 125000u, 1250000u}) {
        Plot plot;
        Axes axes;
        axes.x.setRange({0, count / sampleRate});
        axes.y.setRange({0, 255});
        plot.setAxes(axes);
        auto start = Clock::now();
        std::vector<double> x(count), a(count), b(count);
        for (std::size_t i = 0; i < count; ++i) {
            x[i] = i / sampleRate;
            a[i] = raw[(i * 2) % raw.size()];
            b[i] = raw[(i * 2 + 1) % raw.size()];
        }
        Series sa, sb;
        sa.style.color = {0.2f, .8f, 1, 1};
        sb.style.color = {1, .65f, .2f, 1};
        sa.data = Data(x, a);
        sb.data = Data(x, b);
        const double expand = ms(start);
        start = Clock::now();
        plot.setSeries({sa, sb});
        double fit = ms(start);
        start = Clock::now();
        context.compose(plot);
        context.present();
        double first = ms(start);
        std::vector<double> pan, zoom;
        int rounds = count > 2000000 ? 6 : 20;
        for (int i = 0; i < rounds + 2; ++i) {
            start = Clock::now();
            plot.pan(i % 2 ? .002 : -.002, 0);
            context.compose(plot);
            context.present();
            if (i >= 2)
                pan.push_back(ms(start));
            start = Clock::now();
            plot.zoom({.5, .5}, i % 2 ? 1.02 : 1. / 1.02);
            context.compose(plot);
            context.present();
            if (i >= 2)
                zoom.push_back(ms(start));
        }
        std::cout << "{\"mode\":\"existing_plot\",\"channels\":2,\"samples_per_channel\":" << count
                  << ",\"retention_ms\":" << count / sampleRate * 1000 << ",\"raw_bytes\":" << count * 2
                  << ",\"xy_payload_bytes\":" << count * 2 * sizeof(Point)
                  << ",\"expand_and_snapshot_ms\":" << expand
                  << ",\"equivalent_raw_MB_s\":" << count * 2 / expand / 1000 << ",\"set_series_ms\":" << fit
                  << ",\"first_frame_ms\":" << first << ',';
        stats("pan", pan);
        std::cout << ',';
        stats("zoom", zoom);
        std::cout << "}\n" << std::flush;
        plot.releaseGpu();
    }
}
void streaming(Context& context, const std::vector<std::uint8_t>& raw, bool reduced, double dpi,
               double seconds, std::size_t historyBlocks = 1) {
    using namespace stream_experiment;
    Store store(historyBlocks);
    Plot plot;
    Axes axes;
    axes.x.setRange({0, 65536});
    axes.y.setRange({0, 255});
    plot.setAxes(axes);
    context.compose(plot, dpi);
    context.present();
    // Remove shader/font startup from sustained timing, but retain real UI input/layout/draw/present.
    const double prefill = historyBlocks > 1 ? double(historyBlocks) * blockBytes / 250000000. + .2 : 0;
    store.start(raw, seconds + prefill);
    while (store.accepted < historyBlocks * blockBytes && !store.done) {
        context.present();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    const auto start = Clock::now();
    auto nextFrame = start;
    Deadline frameDeadline;
    std::vector<double> frames, preparation, gpu, age, intervals, eventDelay;
    std::uint64_t events = 0, lastSequence = 0, skippedDisplayBlocks = 0;
    std::size_t displayed = 0, overBudget = 0;
    bool pressed = false;
    int previousMode = -1;
    std::size_t dragChanges = 0, zoomChanges = 0;
    double smallestSpan = 65536, largestSpan = 65536;
    auto previousFrame = start;
    while (!store.done) {
        frameDeadline.wait(nextFrame);
        const auto begin = Clock::now();
        const double elapsed = std::chrono::duration<double>(begin - start).count();
        if (elapsed > seconds + 3)
            throw std::runtime_error("stream experiment timed out");
        auto bounds = context.ui->find("scope.input")->frame;
        const double x = bounds.x + bounds.width * .5, y = bounds.y + bounds.height * .5;
        int mode = int(elapsed) % 2;
        if (mode != previousMode) {
            if (pressed)
                core::queuePointerButton(context.window, x, y, core::PointerButton::Left,
                                         core::PointerAction::Release, {});
            core::queuePointerMotion(context.window, x, y, {}, {});
            pressed = mode == 0;
            if (pressed)
                core::queuePointerButton(context.window, x, y, core::PointerButton::Left,
                                         core::PointerAction::Press, {});
            previousMode = mode;
        }
        // Inject all events due on a 1000 Hz synthetic timeline. Runtime may coalesce motion.
        const auto due = std::uint64_t(elapsed * 1000);
        while (events < due) {
            const double when = double(events) / 1000;
            eventDelay.push_back((elapsed - when) * 1000);
            core::queuePointerMotion(context.window, x + 60 * std::sin(when * 12), y,
                                     pressed ? core::PointerButton::Left : core::PointerButton::None, {});
            if (!pressed)
                core::queueScrollInput(context.window, 0, .003 * std::sin(when * 4));
            ++events;
        }
        const auto beforeInput = plot.axes().x.range();
        context.runtime.update(context.window, 1.f / 60, 1, 1);
        const auto afterInput = plot.axes().x.range();
        if (pressed && afterInput.min != beforeInput.min)
            ++dragChanges;
        if (!pressed && (afterInput.max - afterInput.min) != (beforeInput.max - beforeInput.min))
            ++zoomChanges;
        smallestSpan = std::min(smallestSpan, afterInput.max - afterInput.min);
        largestSpan = std::max(largestSpan, afterInput.max - afterInput.min);
        auto snapshot = store.snapshot();
        if (snapshot.empty())
            continue;
        const auto& latest = *snapshot.back();
        if (displayed && latest.first > lastSequence + blockSamples)
            skippedDisplayBlocks += (latest.first - lastSequence) / blockSamples - 1;
        lastSequence = latest.first;
        const auto prepare = Clock::now();
        std::vector<Series> series(2);
        series[0].style.color = {.2f, .8f, 1, 1};
        series[1].style.color = {1, .65f, .2f, 1};
        std::vector<double> xs[2], ys[2];
        // Seek the middle of retained history while acquisition wraps the ring.
        const auto base =
            historyBlocks > 1 ? snapshot.front()->first + (historyBlocks / 2) * blockSamples : latest.first;
        if (reduced) {
            const auto range = plot.axes().x.range();
            const auto first = std::uint64_t(std::max(0., double(base) + std::floor(range.min) - 1));
            const auto end = std::uint64_t(std::max(0., double(base) + std::ceil(range.max) + 2));
            auto selected = display(snapshot, first, end, std::size_t(std::ceil(bounds.width * dpi)));
            for (int ch = 0; ch < 2; ++ch)
                for (auto p : selected[ch]) {
                    xs[ch].push_back(double(p.index) - double(base));
                    ys[ch].push_back(p.value);
                }
        } else {
            const auto& selectedBlock =
                *snapshot[std::size_t((base - snapshot.front()->first) / blockSamples)];
            for (int ch = 0; ch < 2; ++ch) {
                xs[ch].resize(blockSamples);
                ys[ch].resize(blockSamples);
                for (std::size_t i = 0; i < blockSamples; ++i) {
                    xs[ch][i] = double(i);
                    ys[ch][i] = selectedBlock.raw[i * 2 + ch];
                }
            }
        }
        std::size_t selectedCount = 0;
        for (int ch = 0; ch < 2; ++ch) {
            selectedCount += xs[ch].size();
            series[ch].data = Data(xs[ch], ys[ch]);
        }
        plot.setSeries(std::move(series));
        preparation.push_back(ms(prepare));
        GLuint query;
        glGenQueries(1, &query);
        glBeginQuery(GL_TIME_ELAPSED, query);
        context.compose(plot, dpi);
        context.present();
        glEndQuery(GL_TIME_ELAPSED);
        glFinish();
        GLuint64 nanos;
        glGetQueryObjectui64v(query, GL_QUERY_RESULT, &nanos);
        glDeleteQueries(1, &query);
        gpu.push_back(nanos / 1e6);
        frames.push_back(ms(begin));
        intervals.push_back(std::chrono::duration<double, std::milli>(begin - previousFrame).count());
        age.push_back(ms(latest.ready));
        previousFrame = begin;
        overBudget += frames.back() > 1000. / 60;
        ++displayed;
        nextFrame += std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1. / 60));
        if (nextFrame < Clock::now())
            nextFrame = Clock::now();
        if (selectedCount > 8 * std::size_t(std::ceil(bounds.width * dpi)) && reduced)
            throw std::runtime_error("display exceeded per-pixel bound");
    }
    if (store.producer.joinable())
        store.producer.join();
    if (pressed)
        core::queuePointerButton(context.window, 0, 0, core::PointerButton::Left,
                                 core::PointerAction::Release, {});
    context.runtime.update(context.window, .016f, 1, 1);
    const double duration = std::chrono::duration<double>(Clock::now() - start).count();
    std::cout << "{\"mode\":\"" << (reduced ? "uint8_index_prototype" : "existing_plot_latest_frame")
              << "\",\"dpi\":" << dpi << ",\"requested_seconds\":" << seconds
              << ",\"prefill_seconds\":" << prefill << ",\"ingest_seconds\":" << store.elapsed
              << ",\"accepted_bytes\":" << store.accepted
              << ",\"planned_bytes\":" << store.blocksPlanned * blockBytes
              << ",\"accepted_MB_s\":" << double(store.accepted) / store.elapsed / 1e6
              << ",\"input_integrity_errors\":" << store.integrityErrors
              << ",\"pool_stalls\":" << store.allocationStalls
              << ",\"max_source_backlog_ms\":" << store.maxBacklogMs
              << ",\"preallocated_store_bytes\":" << store.bytes()
              << ",\"retained_samples_per_channel\":" << historyBlocks * blockSamples
              << ",\"frames\":" << displayed << ",\"observed_frames_per_second\":" << displayed / duration
              << ",\"frames_over_16_67ms\":" << overBudget << ",\"injected_motion_events\":" << events
              << ",\"frames_with_drag_change\":" << dragChanges
              << ",\"frames_with_zoom_change\":" << zoomChanges
              << ",\"visible_sample_span_min\":" << smallestSpan
              << ",\"visible_sample_span_max\":" << largestSpan
              << ",\"superseded_display_blocks\":" << skippedDisplayBlocks << ',';
    stats("frame", frames);
    std::cout << ',';
    stats("preparation", preparation);
    std::cout << ',';
    stats("gpu_query", gpu);
    std::cout << ',';
    stats("latest_block_to_completed_frame", age);
    std::cout << ',';
    stats("ingest_block", store.ingestion);
    if (!eventDelay.empty()) {
        std::cout << ',';
        stats("event_schedule_to_dispatch", eventDelay);
    }
    std::cout << "}\n" << std::flush;
    plot.releaseGpu();
    if (store.accepted != store.blocksPlanned * blockBytes || store.integrityErrors || store.allocationStalls)
        throw std::runtime_error("source data was lost or corrupted");
    if (!dragChanges || !zoomChanges)
        throw std::runtime_error("synthetic input did not change the view");
    auto retained = store.snapshot();
    if (retained.size() != historyBlocks || retained.back()->first + blockSamples != store.accepted / 2)
        throw std::runtime_error("incorrect retained time span");
    for (std::size_t i = 1; i < retained.size(); ++i)
        if (retained[i]->first != retained[i - 1]->first + blockSamples)
            throw std::runtime_error("gap in history ring");
    for (const auto& block : retained) {
        const auto sourceBlock = (block->first / blockSamples) % (raw.size() / blockBytes);
        if (std::memcmp(block->raw.data(), raw.data() + sourceBlock * blockBytes, blockBytes))
            throw std::runtime_error("raw samples changed while stored in history");
    }
    if (reduced && historyBlocks > 1) {
        for (std::uint64_t span : {128ull, 65536ull, 1250000ull, 125000000ull, 625000000ull}) {
            std::vector<double> queryTimes;
            std::size_t count = 0;
            for (int i = 0; i < 32; ++i) {
                auto queryStart = Clock::now();
                const auto first =
                    retained.front()->first + (i * 137) % (historyBlocks * blockSamples - span + 1);
                auto points = display(retained, first, first + span, std::size_t(1920 * dpi));
                queryTimes.push_back(ms(queryStart));
                count = points[0].size() + points[1].size();
                for (int ch = 0; ch < 2; ++ch)
                    for (const auto& p : points[ch]) {
                        const auto offset = p.index - retained.front()->first;
                        if (retained[offset / blockSamples]->raw[(offset % blockSamples) * 2 + ch] != p.value)
                            throw std::runtime_error("history display invented a sample");
                    }
            }
            std::cout << "{\"mode\":\"history_lod_query\",\"dpi\":" << dpi
                      << ",\"visible_samples_per_channel\":" << span
                      << ",\"output_points_both_channels\":" << count << ',';
            stats("query", queryTimes);
            std::cout << "}\n";
        }
    }
}

void production(Context& context, const std::vector<std::uint8_t>& raw, double dpi) {
    using stream_experiment::Deadline;
    constexpr std::size_t samples = 65536, bytes = samples * 2;
    auto config = WaveformBuffer::Config::dual125MS();
    WaveformBuffer buffer(config);
    const auto planned = std::size_t(std::ceil(10.2 * 250000000. / bytes));
    std::atomic<std::size_t> accepted{0}, stalls{0};
    std::atomic<bool> done{false};
    std::exception_ptr failure;
    std::vector<double> ingestion;
    double ingestSeconds = 0, backlog = 0;
    Plot plot;
    Axes axes;
    axes.x.setRange({0, samples / sampleRate});
    axes.y.setRange({0, 255});
    plot.setAxes(axes);
    context.compose(plot, dpi);
    context.present();
    std::thread producer([&] {
        try {
            Deadline deadline;
            const auto start = Clock::now();
            ingestion.reserve(planned);
            for (std::size_t i = 0; i < planned; ++i) {
                const auto target =
                    start + std::chrono::duration_cast<Clock::duration>(
                                std::chrono::duration<double>(double(i * bytes) / 250000000.));
                deadline.wait(target);
                backlog = std::max(backlog, ms(target));
                const auto begin = Clock::now();
                if (!buffer.tryAppend(raw.data() + (i * bytes) % raw.size(), bytes)) {
                    ++stalls;
                    throw std::runtime_error("production buffer backpressure at target rate");
                }
                ingestion.push_back(ms(begin));
                accepted += bytes;
            }
            ingestSeconds = std::chrono::duration<double>(Clock::now() - start).count();
        } catch (...) {
            failure = std::current_exception();
        }
        done = true;
    });
    struct Join {
        std::thread& thread;
        ~Join() {
            if (thread.joinable())
                thread.join();
        }
    } join{producer};
    const auto keep = (config.retainedSamples + samples - 1) / samples * samples;
    while (accepted < keep * 2 && !done) {
        context.present();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    const auto start = Clock::now();
    auto nextFrame = start;
    Deadline deadline;
    std::vector<double> frames, snapshots, draw, dispatch;
    std::size_t events = 0, dragChanges = 0, zoomChanges = 0, overBudget = 0;
    bool pressed = false;
    int previousMode = -1;
    double minSpan = 65536, maxSpan = 65536;
    // Start in the middle of the five-second retained history. There is no per-frame window extraction.
    {
        auto snap = buffer.snapshot();
        axes.x.setRange({(snap.firstSample + keep / 2) / sampleRate,
                         (snap.firstSample + keep / 2 + samples) / sampleRate});
        plot.setAxes(axes);
    }
    while (!done) {
        deadline.wait(nextFrame);
        const auto begin = Clock::now();
        const double elapsed = std::chrono::duration<double>(begin - start).count();
        auto bounds = context.ui->find("scope.input")->frame;
        const double x = bounds.x + bounds.width * .5, y = bounds.y + bounds.height * .5;
        const int mode = int(elapsed) % 2;
        if (mode != previousMode) {
            if (pressed)
                core::queuePointerButton(context.window, x, y, core::PointerButton::Left,
                                         core::PointerAction::Release, {});
            core::queuePointerMotion(context.window, x, y, {}, {});
            pressed = mode == 0;
            if (pressed)
                core::queuePointerButton(context.window, x, y, core::PointerButton::Left,
                                         core::PointerAction::Press, {});
            previousMode = mode;
        }
        const auto due = std::size_t(elapsed * 1000);
        while (events < due) {
            const double when = double(events) / 1000;
            dispatch.push_back((elapsed - when) * 1000);
            core::queuePointerMotion(context.window, x + 60 * std::sin(when * 12), y,
                                     pressed ? core::PointerButton::Left : core::PointerButton::None, {});
            if (!pressed)
                core::queueScrollInput(context.window, 0, .003 * std::sin(when * 4));
            ++events;
        }
        auto before = plot.axes().x.range();
        context.runtime.update(context.window, 1.f / 60, 1, 1);
        auto after = plot.axes().x.range();
        dragChanges += pressed && before.min != after.min;
        zoomChanges += !pressed && before.max - before.min != after.max - after.min;
        minSpan = std::min(minSpan, (after.max - after.min) * sampleRate);
        maxSpan = std::max(maxSpan, (after.max - after.min) * sampleRate);
        const auto prepare = Clock::now();
        auto snap = buffer.snapshot();
        std::vector<Series> series(2);
        for (int ch = 0; ch < 2; ++ch)
            series[ch].data = snap.channels[ch];
        series[0].style.color = {.2f, .8f, 1, 1};
        series[1].style.color = {1, .65f, .2f, 1};
        plot.setSeries(std::move(series));
        // Follow the middle of the ring while keeping drag displacement and zoom width.
        // Rebase the drag capture between epochs only through released inputs, otherwise the
        // drag's original axes would restore an expired window. Keep the current window until
        // its history is about to expire, then release and seek to retained midpoint.
        if (after.min < double(snap.firstSample + samples) / sampleRate) {
            if (pressed)
                core::queuePointerButton(context.window, x, y, core::PointerButton::Left,
                                         core::PointerAction::Release, {});
            context.runtime.update(context.window, 1.f / 60, 1, 1);
            pressed = false;
            previousMode = -1;
            auto moved = plot.axes();
            const auto span = after.max - after.min;
            const double center = double(snap.firstSample + keep / 2) / sampleRate;
            moved.x.setRange({center, center + span});
            plot.setAxes(moved);
        }
        snapshots.push_back(ms(prepare));
        const auto render = Clock::now();
        context.compose(plot, dpi);
        context.present();
        draw.push_back(ms(render));
        frames.push_back(ms(begin));
        overBudget += frames.back() > 1000. / 60;
        nextFrame += std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1. / 60));
        if (nextFrame < Clock::now())
            nextFrame = Clock::now();
    }
    producer.join();
    if (pressed)
        core::queuePointerButton(context.window, 0, 0, core::PointerButton::Left,
                                 core::PointerAction::Release, {});
    context.runtime.update(context.window, .016f, 1, 1);
    if (failure)
        std::rethrow_exception(failure);
    const auto duration = std::chrono::duration<double>(Clock::now() - start).count();
    if (accepted != planned * bytes || stalls || !dragChanges || !zoomChanges)
        throw std::runtime_error("production ingest/input verification failed");
    std::cout << "{\"mode\":\"production_waveform\",\"dpi\":" << dpi << ",\"accepted_bytes\":" << accepted
              << ",\"planned_bytes\":" << planned * bytes
              << ",\"accepted_MB_s\":" << double(accepted) / ingestSeconds / 1e6
              << ",\"pool_stalls\":" << stalls << ",\"max_source_backlog_ms\":" << backlog
              << ",\"reserved_bytes\":" << buffer.reservedBytes()
              << ",\"retained_samples_per_channel\":" << keep << ",\"frames\":" << frames.size()
              << ",\"observed_frames_per_second\":" << frames.size() / duration
              << ",\"frames_over_16_67ms\":" << overBudget << ",\"injected_motion_events\":" << events
              << ",\"frames_with_drag_change\":" << dragChanges
              << ",\"frames_with_zoom_change\":" << zoomChanges << ",\"visible_sample_span_min\":" << minSpan
              << ",\"visible_sample_span_max\":" << maxSpan << ',';
    stats("frame", frames);
    std::cout << ',';
    stats("snapshot_and_set_series", snapshots);
    std::cout << ',';
    stats("compose_and_present", draw);
    std::cout << ',';
    stats("ingest_block", ingestion);
    std::cout << ',';
    stats("event_schedule_to_dispatch", dispatch);
    std::cout << "}\n" << std::flush;
    auto retained = buffer.snapshot();
    if (retained.nextSample != accepted / 2 || retained.nextSample - retained.firstSample != keep)
        throw std::runtime_error("production retention mismatch");
    // Verify every retained original byte, outside the timed rendering interval.
    for (std::size_t i = 0; i < keep; ++i)
        for (int ch = 0; ch < 2; ++ch)
            if (retained.channels[ch].at(i).y != raw[((retained.firstSample + i) * 2 + ch) % raw.size()])
                throw std::runtime_error("production history byte mismatch");
    std::cout << "{\"mode\":\"production_integrity\",\"dpi\":" << dpi
              << ",\"verified_retained_bytes\":" << keep * 2 << ",\"errors\":0}\n"
              << std::flush;
    for (std::size_t span : {128ull, 65536ull, 1250000ull, 125000000ull, 625000000ull}) {
        std::vector<double> timings;
        for (int i = 0; i < 16; ++i) {
            auto view = plot.axes();
            const auto first = retained.firstSample + (i * 137) % (keep - span + 1);
            view.x.setRange({double(first) / sampleRate, double(first + span) / sampleRate});
            view.y.setRange({0, 255});
            const auto begin = Clock::now();
            plot.setAxes(view);
            std::vector<Series> series(2);
            for (int ch = 0; ch < 2; ++ch)
                series[ch].data = retained.channels[ch];
            plot.setSeries(std::move(series));
            context.compose(plot, dpi);
            context.present();
            timings.push_back(ms(begin));
        }
        std::cout << "{\"mode\":\"production_history_render\",\"dpi\":" << dpi
                  << ",\"visible_samples_per_channel\":" << span << ',';
        stats("frame", timings);
        std::cout << "}\n" << std::flush;
    }
    plot.releaseGpu();
}

void interactive(Context& context, const std::vector<std::uint8_t>& raw) {
    WaveformBuffer buffer(WaveformBuffer::Config::dual125MS());
    constexpr std::size_t blockBytes = 65536 * 2;
    Plot plot;
    Axes axes;
    axes.x.setRange({0, 65536.0 / sampleRate});
    axes.y.setRange({0, 255});
    axes.x.label = "Time";
    axes.x.unit = "us";
    axes.x.formatter = [](double seconds) {
        char label[64];
        std::snprintf(label, sizeof(label), "%.3f", seconds * 1e6);
        return std::string(label);
    };
    axes.y.label = "Amplitude (UInt8)";
    plot.setAxes(axes);
    std::size_t block = 0;
    bool following = false, refresh = true, reset = false, closed = false;
    const auto publish = [&] {
        auto snapshot = buffer.snapshot();
        std::vector<Series> series(2);
        for (std::size_t ch = 0; ch < 2; ++ch) {
            series[ch].data = snapshot.channels[ch];
            series[ch].name = ch ? "CH2" : "CH1";
        }
        series[0].style.color = {.2f, .8f, 1, 1};
        series[1].style.color = {1, .65f, .2f, 1};
        plot.setSeries(std::move(series));
        if (following || reset) {
            auto view = plot.axes();
            const double span = reset ? 65536.0 / sampleRate : view.x.range().max - view.x.range().min;
            const double end = double(snapshot.nextSample) / sampleRate;
            view.x.setRange({end - span, end});
            if (reset) view.y.setRange({0, 255});
            plot.setAxes(view);
        }
        reset = false;
    };
    // Frozen at startup: original samples and axes remain untouched while the user inspects them.
    buffer.tryAppend(raw.data(), blockBytes);
    ++block;
    publish();
    std::cout << "Manual waveform preview. Auto-follow OFF; drag/scroll to inspect.\n" << std::flush;
    stream_experiment::Deadline deadline;
    while (!closed) {
        const auto frameStart = Clock::now();
        int ww = 0, wh = 0, fw = 0, fh = 0;
        float sx = 1, sy = 1;
#if defined(EUI_WINDOW_BACKEND_SDL2)
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT || (event.type == SDL_WINDOWEVENT &&
                event.window.event == SDL_WINDOWEVENT_CLOSE)) closed = true;
        }
        auto* nativeWindow = static_cast<SDL_Window*>(context.window);
        SDL_GetWindowSize(nativeWindow, &ww, &wh);
        SDL_GL_GetDrawableSize(nativeWindow, &fw, &fh);
#else
        auto* nativeWindow = static_cast<GLFWwindow*>(context.window);
        glfwPollEvents();
        if (glfwWindowShouldClose(nativeWindow)) break;
        glfwGetWindowSize(nativeWindow, &ww, &wh);
        glfwGetFramebufferSize(nativeWindow, &fw, &fh);
        glfwGetWindowContentScale(nativeWindow, &sx, &sy);
#endif
        if (closed) break;
        if (ww <= 0 || wh <= 0 || fw <= 0 || fh <= 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            continue;
        }
        const float dpi = std::max(1.5f, sx);
        const float pointerScale = float(fw) / ww;
        const float width = fw / dpi, height = fh / dpi;
        context.runtime.update(context.window, 1.f / 60, pointerScale, dpi);
        if (following) {
            if (buffer.tryAppend(raw.data() + (block * blockBytes) % raw.size(), blockBytes)) {
                ++block;
                refresh = true;
            }
        }
        if (refresh || reset) { publish(); refresh = false; }
        context.runtime.compose("waveform-preview", width, height,
            [&](core::dsl::Ui& ui, const core::dsl::Screen&) {
                context.ui = &ui;
                ui.stack("preview").size(width, height).content([&] {
                    const auto button = [&](const std::string& id, const std::string& text,
                                            float x, float w, std::function<void()> action) {
                        ui.rect(id).position(x, 10).size(w, 34).color({.12f,.22f,.32f,1})
                            .onClick(std::move(action)).build();
                        ui.text(id+".text").position(x+8, 15).size(w-16, 24).fontSize(15)
                            .text(text).color({.95f,.97f,1,1}).build();
                    };
                    button("follow", following ? "Auto-follow: ON" : "Auto-follow: OFF", 16, 180,
                           [&] { following = !following; });
                    button("reset", "Reset view", 208, 120, [&] { reset = true; });
                    ui.text("help").position(344, 17).size(std::max(0.f,width-360), 26)
                        .fontSize(14).text(following ? "Live preview | OFF freezes samples and view"
                            : "Frozen | Left drag: pan | Wheel: zoom | Shift+drag: box zoom")
                        .color({.8f,.85f,.9f,1}).build();
                    if (width >= 180 && height >= 200)
                        ui.stack("chart").position(8, 58).size(width-16,height-70).content([&] {
                            plot.compose(ui, "scope", width-16, height-70, dpi * 2);
                        });
                });
            });
        // Update the newly composed elements, then clear the framebuffer before drawing.
        // The benchmark's three-argument direct render does not clear old axis labels.
        context.runtime.update(context.window, 0, pointerScale, dpi);
        context.backend->beginFrame({context.window, core::window::nativeWindowInfo(context.window), fw, fh, dpi});
        context.runtime.render(fw, fh, dpi, {0.025f, 0.035f, 0.05f, 1});
        context.backend->present();
        deadline.wait(frameStart + std::chrono::microseconds(16667));
    }
    plot.releaseGpu();
}
} // namespace
int main(int argc, char** argv) {
    try {
        stream_experiment::validate();
        if (argc < 2 || (std::string(argv[1]) != "--baseline" && std::string(argv[1]) != "--stream" &&
                         std::string(argv[1]) != "--history" && std::string(argv[1]) != "--production" &&
                         std::string(argv[1]) != "--interactive")) {
            std::cout << "UInt8 extrema and sample zoom checks passed. Manual workload: --baseline, --stream "
                         "or --history / --production / --interactive.\n";
            return 0;
        }
        auto raw = sourceBytes();
        Context context;
        if (std::string(argv[1]) == "--baseline")
            baseline(context, raw);
        else if (std::string(argv[1]) == "--production") {
            for (double dpi : {1., 2.})
                production(context, raw, dpi);
        } else if (std::string(argv[1]) == "--interactive") {
            interactive(context, raw);
        } else {
            const std::size_t keep =
                std::string(argv[1]) == "--history"
                    ? std::size_t(std::ceil(5. * 250000000. / stream_experiment::blockBytes))
                    : 1;
            for (double dpi : {1., 2.})
                for (bool reduced : {false, true})
                    streaming(context, raw, reduced, dpi, 5, keep);
        }
        if (glGetError() != GL_NO_ERROR)
            throw std::runtime_error("OpenGL error");
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
