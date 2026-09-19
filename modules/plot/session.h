#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace modules::plot {

/** Thread-safe latest-display mailbox. Replaces superseded display snapshots, NOT acquisition history.
 * publish/take/close may run concurrently. No callbacks run under the mutex. Does not wake the UI;
 * consume at the application's display cadence (e.g. onTimer at 60 Hz).
 */
template <class T> class LatestValue {
  public:
    bool publish(T value) {
        std::optional<T> next(std::move(value));
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (closed_)
                return false;
            latest_.swap(next);
        }
        return true;
    }
    std::optional<T> take() {
        std::optional<T> result;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            result.swap(latest_);
        }
        return result;
    }
    void close() {
        std::optional<T> previous;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            closed_ = true;
            previous.swap(latest_);
        }
    }

  private:
    std::mutex mutex_;
    std::optional<T> latest_;
    bool closed_ = false;
};

/** One session per window. Create charts on its UI thread; register shutdownHandler() on that
 * window's onShutdown hook. The handler closes mailboxes, stops producers, then releases all GPU
 * resources before the window/device disappears. No worker may call chart methods directly.
 * Shared chart handles remain CPU objects after shutdown; do not compose them again.
 */
class PlotSession {
    struct State {
        std::thread::id owner = std::this_thread::get_id();
        bool closed = false;
        std::vector<std::function<void()>> releases, closes;
        void check() const {
            if (owner != std::this_thread::get_id())
                throw std::logic_error("plot: session operation requires its UI thread");
        }
        void shutdown(const std::function<void()>& stop) {
            check();
            if (closed)
                return;
            closed = true;
            for (auto& close : closes)
                close();
            closes.clear();
            // The callback must stop/join application-owned workers and must not throw.
            if (stop)
                stop();
            for (auto& release : releases)
                release();
            releases.clear();
        }
    };

  public:
    PlotSession() : state_(std::make_shared<State>()) {}
    PlotSession(const PlotSession&) = delete;
    PlotSession& operator=(const PlotSession&) = delete;
    template <class Chart, class... Args> std::shared_ptr<Chart> make(Args&&... args) {
        checkOpen();
        auto chart = std::make_shared<Chart>(std::forward<Args>(args)...);
        state_->releases.push_back([chart] { chart->releaseGpu(); });
        return chart;
    }
    template <class T> std::shared_ptr<LatestValue<T>> mailbox() {
        checkOpen();
        auto result = std::make_shared<LatestValue<T>>();
        state_->closes.push_back([result] { result->close(); });
        return result;
    }
    std::function<void()> shutdownHandler(std::function<void()> stopProducers = {}) const {
        checkOpen();
        return [state = state_, stop = std::move(stopProducers)] { state->shutdown(stop); };
    }

  private:
    void checkOpen() const {
        state_->check();
        if (state_->closed)
            throw std::logic_error("plot: session is closed");
    }
    std::shared_ptr<State> state_;
};
} // namespace modules::plot
