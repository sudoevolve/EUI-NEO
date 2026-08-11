#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

namespace core::async {

enum class Status {
    Idle,
    Running,
    Done,
    Failed,
    Canceled
};

struct CancelToken {
    std::shared_ptr<std::atomic<bool>> flag;

    bool canceled() const {
        return flag && flag->load();
    }
};

template <typename T>
struct Result {
    bool ok = false;
    T value{};
    std::string error;
};

template <>
struct Result<void> {
    bool ok = false;
    std::string error;
};

namespace detail {

struct TaskStart {
    bool accepted = false;
    std::uint64_t generation = 0;
    CancelToken token;
};

TaskStart beginTask(const std::string& key, bool restart);
void enqueueWorker(std::function<void()> work);
bool finishTask(const std::string& key, std::uint64_t generation, Status finalStatus, std::function<void()> then);

template <typename T>
struct IsResult : std::false_type {};

template <typename T>
struct IsResult<Result<T>> : std::true_type {};

template <typename T>
struct ResultValue {
    using Type = T;
};

template <typename T>
struct ResultValue<Result<T>> {
    using Type = T;
};

template <>
struct ResultValue<void> {
    using Type = void;
};

template <typename Raw>
using ResultValueType = typename ResultValue<std::decay_t<Raw>>::Type;

template <typename WorkFn, bool TakesToken>
struct WorkRawType {
    using Type = std::invoke_result_t<WorkFn>;
};

template <typename WorkFn>
struct WorkRawType<WorkFn, true> {
    using Type = std::invoke_result_t<WorkFn, const CancelToken&>;
};

template <typename Raw>
Result<ResultValueType<Raw>> normalizeResult(Raw&& value) {
    using Decayed = std::decay_t<Raw>;
    using Value = ResultValueType<Raw>;
    if constexpr (IsResult<Decayed>::value) {
        return std::forward<Raw>(value);
    } else {
        Result<Value> result;
        result.ok = true;
        result.value = std::forward<Raw>(value);
        return result;
    }
}

template <typename Value, typename WorkFn>
Result<Value> invokeWork(WorkFn& work, const CancelToken& token) {
    constexpr bool TakesToken = std::is_invocable_v<WorkFn, const CancelToken&>;
    if constexpr (TakesToken) {
        if constexpr (std::is_void_v<std::invoke_result_t<WorkFn, const CancelToken&>>) {
            work(token);
            Result<void> result;
            result.ok = true;
            return result;
        } else {
            return normalizeResult(work(token));
        }
    } else {
        if constexpr (std::is_void_v<std::invoke_result_t<WorkFn>>) {
            work();
            Result<void> result;
            result.ok = true;
            return result;
        } else {
            return normalizeResult(work());
        }
    }
}

template <typename WorkFn, typename ThenFn>
bool start(std::string key, bool restart, WorkFn work, ThenFn then) {
    using Work = std::decay_t<WorkFn>;
    constexpr bool takesToken = std::is_invocable_v<Work, const CancelToken&>;
    using Raw = typename WorkRawType<Work, takesToken>::Type;
    using Value = ResultValueType<Raw>;

    const TaskStart task = beginTask(key, restart);
    if (!task.accepted) {
        return false;
    }

    enqueueWorker([
        key = std::move(key),
        generation = task.generation,
        token = task.token,
        work = std::move(work),
        then = std::move(then)
    ]() mutable {
        if (token.canceled()) {
            finishTask(key, generation, Status::Canceled, {});
            return;
        }

        Result<Value> result = invokeWork<Value>(work, token);
        Status finalStatus = result.ok ? Status::Done : Status::Failed;
        if (token.canceled()) {
            finalStatus = Status::Canceled;
        }

        std::function<void()> completion;
        if (finalStatus != Status::Canceled) {
            completion = [then = std::move(then), result = std::move(result)]() mutable {
                then(result);
            };
        }
        finishTask(key, generation, finalStatus, std::move(completion));
    });

    return true;
}

} // namespace detail

template <typename T>
Result<std::decay_t<T>> success(T&& value) {
    Result<std::decay_t<T>> result;
    result.ok = true;
    result.value = std::forward<T>(value);
    return result;
}

inline Result<void> success() {
    Result<void> result;
    result.ok = true;
    return result;
}

template <typename T>
Result<T> failure(std::string error) {
    Result<T> result;
    result.ok = false;
    result.error = std::move(error);
    return result;
}

inline Result<void> failure(std::string error) {
    Result<void> result;
    result.ok = false;
    result.error = std::move(error);
    return result;
}

template <typename WorkFn, typename ThenFn>
bool runOnce(std::string key, WorkFn work, ThenFn then) {
    return detail::start(std::move(key), false, std::move(work), std::move(then));
}

template <typename WorkFn, typename ThenFn>
bool restart(std::string key, WorkFn work, ThenFn then) {
    return detail::start(std::move(key), true, std::move(work), std::move(then));
}

Status status(const std::string& key);
bool running(const std::string& key);
bool cancel(const std::string& key);
bool forget(const std::string& key);
bool dispatchReady();
void shutdown();

} // namespace core::async
