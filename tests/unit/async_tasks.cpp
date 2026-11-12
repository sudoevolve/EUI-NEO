#include "core/platform/async.h"
#include <chrono>
#include <iostream>
#include <thread>

namespace {
bool done(const char* key) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (core::async::status(key) != core::async::Status::Done) {
        if (std::chrono::steady_clock::now() > deadline) return false;
        std::this_thread::yield();
    }
    return true;
}
}

int main() {
    // 不初始化窗口后端，覆盖后台任务先于 SDL 事件系统运行的唤醒路径。
    using namespace core::async;
    bool ok = true;
    int old = 0, current = 0, canceled = 0, forgotten = 0, later = 0;
    runOnce("replace", [] { return 1; }, [&](const Result<int>&) { ++old; });
    ok &= done("replace");
    restart("replace", [] { return 2; }, [&](const Result<int>&) { ++current; });
    ok &= done("replace");
    dispatchReady();
    ok &= old == 0 && current == 1;
    runOnce("cancel", [] {}, [&](const Result<void>&) { ++canceled; });
    ok &= done("cancel") && cancel("cancel");
    dispatchReady();
    ok &= canceled == 0 && forget("cancel");
    runOnce("reuse", [] {}, [&](const Result<void>&) { ++forgotten; });
    ok &= done("reuse") && forget("reuse");
    runOnce("reuse", [] {}, [&](const Result<void>&) { ++current; });
    ok &= done("reuse");
    dispatchReady();
    ok &= forgotten == 0 && current == 2;
    runOnce("first", [] {}, [&](const Result<void>&) { cancel("later"); });
    ok &= done("first");
    runOnce("later", [] {}, [&](const Result<void>&) { ++later; });
    ok &= done("later");
    dispatchReady();
    ok &= later == 0;
    shutdown();
    if (!ok) std::cerr << "async completion lifetime regression\n";
    return ok ? 0 : 1;
}
