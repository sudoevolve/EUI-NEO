#include "core/platform/platform.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#if defined(__linux__)
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {
void require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "platform_dialog: %s\n", message);
        std::exit(1);
    }
}
}

int main() {
#if defined(_WIN32)
    for (const auto mode : {core::platform::FileDialogMode::OpenFile, core::platform::FileDialogMode::SaveFile,
                            core::platform::FileDialogMode::Directory}) {
        std::atomic<bool> stop{false};
        std::atomic<bool> closed{false};
        std::thread dismiss([&] {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
            while (!stop && std::chrono::steady_clock::now() < deadline) {
                EnumWindows([](HWND window, LPARAM context) -> BOOL {
                    DWORD process = 0;
                    GetWindowThreadProcessId(window, &process);
                    wchar_t title[128]{};
                    GetWindowTextW(window, title, 128);
                    if (process == GetCurrentProcessId() && std::wstring(title) == L"EUI dialog regression") {
                        PostMessageW(window, WM_CLOSE, 0, 0);
                        reinterpret_cast<std::atomic<bool>*>(context)->store(true);
                    }
                    return TRUE;
                }, reinterpret_cast<LPARAM>(&closed));
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
        });
        core::platform::FileDialogOptions options;
        options.prompt = "EUI dialog regression";
        options.mode = mode;
        options.defaultFileName = "eui-test.txt";
        const auto result = core::platform::openFileDialog(options);
        stop = true;
        dismiss.join();
        require(closed && result.status == core::platform::FileDialogStatus::Cancelled, "native cancellation failed");
    }
    core::platform::FileDialogOptions invalid;
    invalid.defaultFileName = std::string(65536, 'a');
    require(core::platform::saveFileDialog(invalid).status == core::platform::FileDialogStatus::Failed,
            "oversized default name was accepted");
    return 0;
#elif defined(__linux__)
    namespace fs = std::filesystem;
    const fs::path directory = fs::temp_directory_path() /
        ("eui-platform-dialog-" + std::to_string(static_cast<long long>(getpid())));
    fs::create_directories(directory);
    const fs::path executable = directory / "zenity";
    const fs::path selected = directory / "selected.txt";
    { std::ofstream file(selected); file << "selected"; }
    const char* oldPathValue = std::getenv("PATH");
    const std::string oldPath = oldPathValue ? oldPathValue : "";
    const std::string path = directory.string() + ":" + oldPath;
    require(setenv("PATH", path.c_str(), 1) == 0, "could not set test PATH");

    core::platform::FileDialogOptions options;
    const auto run = [&](const char* body) {
        std::ofstream script(executable);
        script << "#!/bin/sh\n" << body << '\n';
        script.close();
        require(chmod(executable.c_str(), 0700) == 0, "could not make zenity test stub executable");
        return core::platform::openFileDialog(options);
    };

    auto result = run(("echo 'Gtk-Message: harmless diagnostic' >&2; printf '%s\\n' '" +
                       selected.string() + "'; exit 0").c_str());
        require(result.status == core::platform::FileDialogStatus::Selected, "zenity success was not selected");
        require(result.paths.size() == 1 && result.paths.front() == selected.string(),
            "stderr diagnostic was parsed as a selected path");

    result = run("echo 'Gtk-ERROR: dialog backend failed' >&2; exit 2");
    require(result.status == core::platform::FileDialogStatus::Failed, "zenity diagnostic was not a failure");
    require(result.error.find("Gtk-ERROR: dialog backend failed") != std::string::npos,
            "zenity failure diagnostic was lost");

    result = run("echo 'Gtk-ERROR: no selected path' >&2; exit 0");
    require(result.status == core::platform::FileDialogStatus::Failed, "empty successful output was not a failure");
    require(result.error.find("Gtk-ERROR: no selected path") != std::string::npos,
            "empty success diagnostic was lost");

    result = run("echo 'Gtk-Message: harmless warning during cancel' >&2; exit 1");
    require(result.status == core::platform::FileDialogStatus::Cancelled,
            "cancel with stderr warning was not treated as cancellation");

    options.mode = core::platform::FileDialogMode::SaveFile;
    options.defaultFileName = "new file.txt";
    options.initialDirectory = directory.string();
    result = run(("case \" $* \" in *' --save --confirm-overwrite '*) printf '%s\\n' '" + selected.string() +
                  "';; *) exit 2;; esac").c_str());
    require(result.selected(), "zenity save arguments missing");
    options.mode = core::platform::FileDialogMode::Directory;
    result = run(("case \" $* \" in *' --directory '*) printf '%s\\n' '" + directory.string() +
                  "';; *) exit 2;; esac").c_str());
    require(result.selected(), "zenity directory arguments missing");

    setenv("PATH", oldPath.c_str(), 1);
    fs::remove_all(directory);
    return 0;
#else
    return 77;
#endif
}
