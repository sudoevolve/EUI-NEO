#include "core/platform/async.h"
#include "core/platform/platform.h"

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace core::async {
namespace {

struct TaskState {
    Status status = Status::Idle;
    std::uint64_t generation = 0;
    std::shared_ptr<std::atomic<bool>> cancelFlag;
};

std::mutex gMutex;
std::condition_variable gWorkerCv;
std::deque<std::function<void()>> gWorkerQueue;
std::deque<std::function<void()>> gMainQueue;
std::unordered_map<std::string, TaskState> gTasks;
std::vector<std::thread> gWorkers;
bool gStopping = false;

unsigned int workerCount() {
    const unsigned int hardware = std::thread::hardware_concurrency();
    if (hardware <= 1) {
        return 1;
    }
    return std::max(1u, std::min(4u, hardware - 1u));
}

void workerLoop() {
    while (true) {
        std::function<void()> work;
        {
            std::unique_lock<std::mutex> lock(gMutex);
            gWorkerCv.wait(lock, [] {
                return gStopping || !gWorkerQueue.empty();
            });
            if (gStopping && gWorkerQueue.empty()) {
                return;
            }
            work = std::move(gWorkerQueue.front());
            gWorkerQueue.pop_front();
        }

        if (work) {
            work();
        }
    }
}

void ensureWorkersLocked() {
    if (!gWorkers.empty() || gStopping) {
        return;
    }
    const unsigned int count = workerCount();
    gWorkers.reserve(count);
    for (unsigned int i = 0; i < count; ++i) {
        gWorkers.emplace_back(workerLoop);
    }
}

void postReadyEvent() {
    platform::requestFrame();
}

} // namespace

namespace detail {

TaskStart beginTask(const std::string& key, bool restart) {
    if (key.empty()) {
        return {};
    }

    std::lock_guard<std::mutex> lock(gMutex);
    if (gStopping) {
        return {};
    }

    TaskState& state = gTasks[key];
    if (!restart && state.status != Status::Idle) {
        return {};
    }

    if (state.cancelFlag) {
        state.cancelFlag->store(true);
    }

    state.status = Status::Running;
    state.cancelFlag = std::make_shared<std::atomic<bool>>(false);
    ++state.generation;

    return {true, state.generation, CancelToken{state.cancelFlag}};
}

void enqueueWorker(std::function<void()> work) {
    if (!work) {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(gMutex);
        if (gStopping) {
            return;
        }
        ensureWorkersLocked();
        gWorkerQueue.push_back(std::move(work));
    }
    gWorkerCv.notify_one();
}

bool finishTask(const std::string& key, std::uint64_t generation, Status finalStatus, std::function<void()> then) {
    bool shouldPost = false;
    {
        std::lock_guard<std::mutex> lock(gMutex);
        if (gStopping) {
            return false;
        }

        const auto it = gTasks.find(key);
        if (it == gTasks.end() || it->second.generation != generation) {
            return false;
        }

        TaskState& state = it->second;
        if (state.cancelFlag && state.cancelFlag->load()) {
            finalStatus = Status::Canceled;
        }
        state.status = finalStatus;

        if (finalStatus != Status::Running) {
            state.cancelFlag.reset();
        }

        if (then && finalStatus != Status::Canceled) {
            gMainQueue.push_back(std::move(then));
            shouldPost = true;
        }
    }

    if (shouldPost) {
        postReadyEvent();
    }
    return true;
}

} // namespace detail

Status status(const std::string& key) {
    std::lock_guard<std::mutex> lock(gMutex);
    const auto it = gTasks.find(key);
    if (it == gTasks.end()) {
        return Status::Idle;
    }
    return it->second.status;
}

bool running(const std::string& key) {
    return status(key) == Status::Running;
}

bool cancel(const std::string& key) {
    std::lock_guard<std::mutex> lock(gMutex);
    const auto it = gTasks.find(key);
    if (it == gTasks.end()) {
        return false;
    }

    TaskState& state = it->second;
    if (state.cancelFlag) {
        state.cancelFlag->store(true);
    }
    if (state.status == Status::Running) {
        state.status = Status::Canceled;
        ++state.generation;
        state.cancelFlag.reset();
    }
    return true;
}

bool forget(const std::string& key) {
    std::lock_guard<std::mutex> lock(gMutex);
    const auto it = gTasks.find(key);
    if (it == gTasks.end() ||
        (it->second.status != Status::Done && it->second.status != Status::Failed)) {
        return false;
    }
    gTasks.erase(it);
    return true;
}

bool dispatchReady() {
    std::deque<std::function<void()>> callbacks;
    {
        std::lock_guard<std::mutex> lock(gMutex);
        callbacks.swap(gMainQueue);
    }

    for (std::function<void()>& callback : callbacks) {
        if (callback) {
            callback();
        }
    }
    return !callbacks.empty();
}

void shutdown() {
    {
        std::lock_guard<std::mutex> lock(gMutex);
        gStopping = true;
        for (auto& item : gTasks) {
            if (item.second.cancelFlag) {
                item.second.cancelFlag->store(true);
            }
        }
        gWorkerQueue.clear();
        gMainQueue.clear();
    }
    gWorkerCv.notify_all();

    for (std::thread& worker : gWorkers) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    {
        std::lock_guard<std::mutex> lock(gMutex);
        gWorkers.clear();
        gTasks.clear();
    }
}

} // namespace core::async
