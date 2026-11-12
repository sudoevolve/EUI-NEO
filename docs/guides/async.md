# 异步任务

页面代码通过 `app::async` 启动后台任务。任务在框架线程池里执行，完成回调会回到 UI 主线程，回调里可以安全修改页面状态。自建后台线程修改页面状态后，应调用 `app::requestUpdate()` 唤醒 UI。

## 推荐 API

```cpp
namespace app::async {

enum class Status {
    Idle,
    Running,
    Done,
    Failed,
    Canceled
};

struct CancelToken {
    bool canceled() const;
};

template <typename T>
struct Result {
    bool ok = false;
    T value{};
    std::string error;
};

bool runOnce(std::string key, WorkFn work, ThenFn then);
bool restart(std::string key, WorkFn work, ThenFn then);
Status status(const std::string& key);
bool running(const std::string& key);
bool cancel(const std::string& key);
template <typename T> Result<T> success(T value);
template <typename T> Result<T> failure(std::string error);

} // namespace app::async
```

`runOnce`、`restart`、`status`、`cancel` 是核心接口；`running` 是 `status(key) == Status::Running` 的便捷写法。

## runOnce

`runOnce` 适合页面初始化加载、只请求一次的数据、缓存构建等场景。同一个 key 已经启动过后，后续 compose 再调用不会重复启动。

```cpp
app::async::runOnce(
    "load.config",
    [] {
        return readConfigFile();
    },
    [](const app::async::Result<std::string>& result) {
        if (result.ok) {
            configText = result.value;
        } else {
            configError = result.error;
        }
    });
```

## restart

`restart` 适合搜索、刷新、重新选择文件等场景。它会取消同 key 的旧任务结果，并启动一次新任务。

```cpp
components::button(ui, "refresh")
    .text(app::async::running("refresh.data") ? "Loading..." : "Refresh")
    .onClick([] {
        app::async::restart(
            "refresh.data",
            [] {
                return loadData();
            },
            [](const app::async::Result<std::string>& result) {
                if (result.ok) {
                    dataText = result.value;
                }
            });
    })
    .build();
```

## 失败结果

后台函数可以直接返回值，也可以返回 `app::async::Result<T>` 来表达失败。

```cpp
app::async::restart(
    "parse.file",
    [] {
        std::string text = readFile("data.json");
        if (text.empty()) {
            return app::async::failure<std::string>("File is empty.");
        }
        return app::async::success(text);
    },
    [](const app::async::Result<std::string>& result) {
        parseMessage = result.ok ? result.value : result.error;
    });
```

## 取消

`cancel(key)` 是协作取消，不会强杀正在执行的系统调用。长循环任务可以接收 `CancelToken` 并主动退出。

```cpp
app::async::restart(
    "scan.files",
    [](const app::async::CancelToken& token) {
        std::string summary;
        for (const std::string& path : filesToScan) {
            if (token.canceled()) {
                return app::async::failure<std::string>("Canceled.");
            }
            summary += scanOneFile(path);
        }
        return app::async::success(summary);
    },
    [](const app::async::Result<std::string>& result) {
        if (result.ok) {
            scanSummary = result.value;
        }
    });

components::button(ui, "cancel.scan")
    .text("Cancel")
    .onClick([] {
        app::async::cancel("scan.files");
    })
    .build();
```

如果任务被 `cancel` 或新的 `restart` 替换，旧任务完成后的回调不会再更新 UI。

这也适用于后台已经完成、但 UI 尚未派发的回调：派发前会核对任务身份。`cancel` 不会中断已经开始执行的回调；`forget` 可释放 Done、Failed、Canceled 状态，之后相同 key 可以重新启动，旧身份不会复用。取消、重启与 UI 回调的执行顺序应由 UI 线程协调。

## 框架内部复用

`core/network` 的文本请求、`core/image` 的网络图片和 Bing daily 下载都复用同一个异步线程池。任务完成后会唤醒 GLFW 主循环，主循环派发完成回调并触发重新 compose 或重绘。
