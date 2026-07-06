#include "core/platform/platform.h"

#include "core/platform/tray_bridge.h"
#include "core/window/window_backend.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#if defined(EUI_WINDOW_BACKEND_SDL2)
#include <SDL.h>
#endif

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include <commdlg.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <sys/wait.h>
#elif defined(__linux__)
#include <unistd.h>
#include <sys/wait.h>
#else
#include <sys/wait.h>
#endif

namespace core::platform {

namespace {

#if !defined(_WIN32)
std::string shellQuote(const std::string& value) {
    std::string result = "'";
    for (char ch : value) {
        if (ch == '\'') {
            result += "'\\''";
        } else {
            result.push_back(ch);
        }
    }
    result += "'";
    return result;
}
#endif

struct TrayState {
    bool initialized = false;
    std::string iconPath;
};

TrayState& trayState() {
    static TrayState state;
    return state;
}

std::atomic<bool>& frameRequested() {
    static std::atomic<bool> requested{false};
    return requested;
}

std::atomic<bool>& uiUpdateRequested() {
    static std::atomic<bool> requested{false};
    return requested;
}

std::filesystem::path executableDirectory() {
#if defined(_WIN32)
    std::vector<char> buffer(MAX_PATH);
    DWORD length = 0;
    while (true) {
        length = GetModuleFileNameA(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            return {};
        }
        if (length < buffer.size() - 1) {
            break;
        }
        buffer.resize(buffer.size() * 2);
    }
    return std::filesystem::path(buffer.data()).parent_path();
#elif defined(__APPLE__)
    std::vector<char> buffer(4096);
    uint32_t size = static_cast<uint32_t>(buffer.size());
    if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
        buffer.resize(size);
        if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
            return {};
        }
    }
    std::error_code error;
    return std::filesystem::absolute(std::filesystem::path(buffer.data()), error).parent_path();
#elif defined(__ANDROID__) && defined(EUI_WINDOW_BACKEND_SDL2)
    const char* path = SDL_AndroidGetInternalStoragePath();
    return path != nullptr ? std::filesystem::path(path) : std::filesystem::path{};
#elif defined(__linux__)
    std::vector<char> buffer(4096);
    const ssize_t length = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
    if (length <= 0) {
        return {};
    }
    buffer[static_cast<std::size_t>(length)] = '\0';
    std::error_code error;
    return std::filesystem::absolute(std::filesystem::path(buffer.data()), error).parent_path();
#else
    return {};
#endif
}

std::string joinExtensions(const std::vector<std::string>& extensions, const std::string& separator) {
    std::string result;
    for (std::size_t i = 0; i < extensions.size(); ++i) {
        if (i > 0) {
            result += separator;
        }
        result += extensions[i];
    }
    return result;
}

std::string trimWhitespace(std::string value) {
    const auto first = std::find_if(value.begin(), value.end(), [](unsigned char ch) {
        return ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r';
    });
    const auto last = std::find_if(value.rbegin(), value.rend(), [](unsigned char ch) {
        return ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r';
    }).base();
    if (first >= last) {
        return {};
    }
    return {first, last};
}

std::string extensionName(std::string value) {
    value = trimWhitespace(std::move(value));
    if (value.rfind("*.", 0) == 0) {
        value.erase(0, 2);
    } else if (!value.empty() && value.front() == '.') {
        value.erase(value.begin());
    }
    return value;
}

std::vector<std::string> extensionNames(const std::vector<std::string>& extensions) {
    std::vector<std::string> result;
    for (const std::string& extension : extensions) {
        std::string name = extensionName(extension);
        if (name.empty() || name == "*") {
            continue;
        }
        result.push_back(std::move(name));
    }
    return result;
}

std::string extensionPattern(const std::string& extensionName) {
    return "*." + extensionName;
}

std::vector<std::string> extensionPatterns(const std::vector<std::string>& names) {
    std::vector<std::string> result;
    result.reserve(names.size());
    for (const std::string& name : names) {
        result.push_back(extensionPattern(name));
    }
    return result;
}

std::string fileFilterName(const FileDialogOptions& options) {
    return options.filterName.empty() ? "Allowed files" : options.filterName;
}

std::string appleScriptString(const std::string& value) {
    std::string result;
    result.reserve(value.size() + 8);
    for (char ch : value) {
        if (ch == '\\' || ch == '"') {
            result.push_back('\\');
        }
        result.push_back(ch);
    }
    return result;
}

std::vector<std::string> splitLines(const std::string& value) {
    std::vector<std::string> lines;
    std::istringstream stream(value);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!line.empty()) {
            lines.push_back(std::move(line));
        }
    }
    return lines;
}

FileDialogResult selectedFiles(std::vector<std::string> paths) {
    FileDialogResult result;
    if (paths.empty()) {
        result.status = FileDialogStatus::Failed;
        result.error = "File dialog returned no path.";
        return result;
    }
    result.status = FileDialogStatus::Selected;
    result.paths = std::move(paths);
    return result;
}

FileDialogResult cancelledFileDialog() {
    FileDialogResult result;
    result.status = FileDialogStatus::Cancelled;
    return result;
}

FileDialogResult failedFileDialog(std::string error) {
    FileDialogResult result;
    result.status = FileDialogStatus::Failed;
    result.error = std::move(error);
    return result;
}

std::string trimTrailingWhitespace(std::string value) {
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r' || value.back() == ' ' || value.back() == '\t')) {
        value.pop_back();
    }
    return value;
}

struct CommandResult {
    bool started = false;
    int exitCode = -1;
    std::string output;
};

int decodeProcessExitCode(int status) {
    if (status < 0) {
        return -1;
    }
#if defined(_WIN32)
    return status;
#else
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return -1;
#endif
}

CommandResult runCommand(const std::string& command) {
    CommandResult result;
    if (command.empty()) {
        return result;
    }

#if defined(_WIN32)
    FILE* pipe = _popen(command.c_str(), "r");
#else
    FILE* pipe = popen(command.c_str(), "r");
#endif
    if (pipe == nullptr) {
        return result;
    }
    result.started = true;

    char buffer[256];
    while (std::fgets(buffer, static_cast<int>(sizeof(buffer)), pipe) != nullptr) {
        result.output += buffer;
    }

#if defined(_WIN32)
    const int status = _pclose(pipe);
#else
    const int status = pclose(pipe);
#endif
    result.exitCode = decodeProcessExitCode(status);
    result.output = trimTrailingWhitespace(std::move(result.output));
    return result;
}

#if defined(_WIN32)
std::wstring utf8ToWide(const std::string& value) {
    if (value.empty()) {
        return {};
    }
    int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
    if (size <= 0) {
        size = MultiByteToWideChar(CP_ACP, 0, value.c_str(), -1, nullptr, 0);
        if (size <= 0) {
            return {};
        }
        std::wstring result(static_cast<std::size_t>(size - 1), L'\0');
        MultiByteToWideChar(CP_ACP, 0, value.c_str(), -1, result.data(), size);
        return result;
    }

    std::wstring result(static_cast<std::size_t>(size - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, result.data(), size);
    return result;
}

std::string wideToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return {};
    }
    std::string result(static_cast<std::size_t>(size - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, result.data(), size, nullptr, nullptr);
    return result;
}

std::wstring windowsFilterStorage(const FileDialogOptions& options, const std::vector<std::string>& extensionNames) {
    std::wstring storage;
    const auto appendFilter = [&](const std::string& label, const std::string& pattern) {
        storage += utf8ToWide(label);
        storage.push_back(L'\0');
        storage += utf8ToWide(pattern);
        storage.push_back(L'\0');
    };

    if (!extensionNames.empty()) {
        const std::vector<std::string> patterns = extensionPatterns(extensionNames);
        appendFilter(
            fileFilterName(options) + " (" + joinExtensions(patterns, ";") + ")",
            joinExtensions(patterns, ";"));
    }

    appendFilter("All files (*.*)", "*.*");
    storage.push_back(L'\0');
    return storage;
}

std::vector<std::string> parseWindowsSelection(const std::vector<wchar_t>& buffer) {
    std::vector<std::string> paths;
    if (buffer.empty() || buffer[0] == L'\0') {
        return paths;
    }

    const wchar_t* cursor = buffer.data();
    const std::wstring first = cursor;
    cursor += first.size() + 1;
    if (*cursor == L'\0') {
        paths.push_back(wideToUtf8(first));
        return paths;
    }

    const std::filesystem::path directory(first);
    while (*cursor != L'\0') {
        const std::wstring filename = cursor;
        paths.push_back((directory / filename).u8string());
        cursor += filename.size() + 1;
    }
    return paths;
}

std::string windowsDialogError(DWORD errorCode) {
    if (errorCode == 0) {
        return {};
    }
    return "GetOpenFileNameW failed with error code " + std::to_string(errorCode) + ".";
}
#else
std::string osascriptCommand(const std::vector<std::string>& lines) {
    std::string command = "osascript";
    for (const std::string& line : lines) {
        command += " -e ";
        command += shellQuote(line);
    }
    command += " 2>&1";
    return command;
}

std::string appleFileDialogCommand(const FileDialogOptions& options, const std::vector<std::string>& extensionNames) {
    const std::string prompt = options.prompt.empty() ? "Select file" : options.prompt;
    std::string choose = "set eui_selection to choose file with prompt \"" + appleScriptString(prompt) + "\"";
    if (!options.initialDirectory.empty()) {
        choose += " default location POSIX file \"" + appleScriptString(options.initialDirectory) + "\"";
    }
    if (!extensionNames.empty()) {
        choose += " of type {";
        for (std::size_t i = 0; i < extensionNames.size(); ++i) {
            if (i > 0) {
                choose += ", ";
            }
            choose += "\"" + appleScriptString(extensionNames[i]) + "\"";
        }
        choose += "}";
    }
    if (options.allowMultiple) {
        choose += " with multiple selections allowed";
    }

    return osascriptCommand({
        choose,
        "if class of eui_selection is list then",
        "set eui_paths to {}",
        "repeat with eui_file in eui_selection",
        "set end of eui_paths to POSIX path of eui_file",
        "end repeat",
        "set AppleScript's text item delimiters to linefeed",
        "eui_paths as text",
        "else",
        "POSIX path of eui_selection",
        "end if"
    });
}

bool commandWasCancelled(const CommandResult& command) {
    return command.exitCode != 0 &&
        (command.output.empty() ||
            command.output.find("-128") != std::string::npos ||
            command.output.find("User canceled") != std::string::npos ||
            command.output.find("cancelled") != std::string::npos ||
            command.output.find("canceled") != std::string::npos);
}

FileDialogResult resultFromCommand(const CommandResult& command, const std::string& toolName) {
    if (!command.started) {
        return failedFileDialog(toolName + " could not be started.");
    }
    if (command.exitCode == 0) {
        return selectedFiles(splitLines(command.output));
    }
    if (commandWasCancelled(command)) {
        return cancelledFileDialog();
    }
    std::string error = command.output.empty()
        ? toolName + " failed with exit code " + std::to_string(command.exitCode) + "."
        : command.output;
    return failedFileDialog(std::move(error));
}

std::string linuxFilterArgument(const FileDialogOptions& options, const std::vector<std::string>& extensionNames) {
    if (extensionNames.empty()) {
        return {};
    }
    const std::vector<std::string> patterns = extensionPatterns(extensionNames);
    return fileFilterName(options) + " | " + joinExtensions(patterns, " ");
}

FileDialogResult runZenityFileDialog(const FileDialogOptions& options, const std::vector<std::string>& extensionNames) {
    const std::string prompt = options.prompt.empty() ? "Select file" : options.prompt;
    std::string command = "if command -v zenity >/dev/null 2>&1; then zenity --file-selection";
    command += " --title=" + shellQuote(prompt);
    if (!options.initialDirectory.empty()) {
        command += " --filename=" + shellQuote(options.initialDirectory);
    }
    if (options.allowMultiple) {
        command += " --multiple --separator=" + shellQuote("\n");
    }
    const std::string filter = linuxFilterArgument(options, extensionNames);
    if (!filter.empty()) {
        command += " --file-filter=" + shellQuote(filter);
        command += " --file-filter=" + shellQuote("All files | *");
    }
    command += "; else exit 127; fi 2>&1";

    const CommandResult result = runCommand(command);
    if (result.exitCode == 127) {
        return failedFileDialog("zenity is not available.");
    }
    return resultFromCommand(result, "zenity");
}

std::string kdialogFilterArgument(const FileDialogOptions& options, const std::vector<std::string>& extensionNames) {
    if (extensionNames.empty()) {
        return {};
    }
    const std::vector<std::string> patterns = extensionPatterns(extensionNames);
    return joinExtensions(patterns, " ") + "|" + fileFilterName(options);
}

FileDialogResult runKdialogFileDialog(const FileDialogOptions& options, const std::vector<std::string>& extensionNames) {
    const std::string prompt = options.prompt.empty() ? "Select file" : options.prompt;
    const std::string startDirectory = options.initialDirectory.empty() ? "." : options.initialDirectory;
    std::string command = "if command -v kdialog >/dev/null 2>&1; then kdialog";
    command += " --title " + shellQuote(prompt);
    if (options.allowMultiple) {
        command += " --multiple --separate-output";
    }
    command += " --getopenfilename " + shellQuote(startDirectory);
    command += " " + shellQuote(kdialogFilterArgument(options, extensionNames));
    command += "; else exit 127; fi 2>&1";

    const CommandResult result = runCommand(command);
    if (result.exitCode == 127) {
        return failedFileDialog("kdialog is not available.");
    }
    return resultFromCommand(result, "kdialog");
}
#endif

std::filesystem::path resolveIconPath(const std::string& iconPath) {
    namespace fs = std::filesystem;
    if (iconPath.empty()) {
        return {};
    }

    std::error_code error;
    const fs::path requested(iconPath);
    const fs::path current = fs::current_path(error);
    std::vector<fs::path> candidates;
    candidates.push_back(requested);
    if (!error) {
        candidates.push_back(current / requested);
        candidates.push_back(current / "assets" / requested.filename());
    }

    fs::path executableDir;
#if defined(__APPLE__)
    char executablePath[4096];
    uint32_t executablePathSize = sizeof(executablePath);
    if (_NSGetExecutablePath(executablePath, &executablePathSize) == 0) {
        error.clear();
        executableDir = fs::absolute(fs::path(executablePath), error).parent_path();
    }
#elif defined(_WIN32)
    char executablePath[MAX_PATH];
    const DWORD executablePathSize = GetModuleFileNameA(nullptr, executablePath, MAX_PATH);
    if (executablePathSize > 0 && executablePathSize < MAX_PATH) {
        error.clear();
        executableDir = fs::absolute(fs::path(executablePath), error).parent_path();
    }
#elif defined(__linux__)
    char executablePath[4096];
    const ssize_t executablePathSize = readlink("/proc/self/exe", executablePath, sizeof(executablePath) - 1);
    if (executablePathSize > 0) {
        executablePath[executablePathSize] = '\0';
        error.clear();
        executableDir = fs::absolute(fs::path(executablePath), error).parent_path();
    }
#endif
    if (!executableDir.empty()) {
        candidates.push_back(executableDir / requested);
        candidates.push_back(executableDir / "assets" / requested.filename());
    }

#if defined(_WIN32)
    const std::size_t originalCount = candidates.size();
    std::vector<fs::path> icoCandidates;
    for (std::size_t i = 0; i < originalCount; ++i) {
        fs::path ico = candidates[i];
        if (ico.extension() != ".ico") {
            ico.replace_extension(".ico");
        }
        icoCandidates.push_back(std::move(ico));
    }
    icoCandidates.insert(icoCandidates.end(), candidates.begin(), candidates.end());
    candidates = std::move(icoCandidates);
#endif

    for (const fs::path& candidate : candidates) {
        error.clear();
        if (fs::exists(candidate, error) && !error) {
            error.clear();
            fs::path absolute = fs::absolute(candidate, error);
            return error ? candidate : absolute;
        }
    }
    return {};
}

} // namespace

bool repairCurrentWorkingDirectory() {
#if defined(__ANDROID__) && defined(EUI_WINDOW_BACKEND_SDL2)
    const std::filesystem::path fallback = executableDirectory();
    if (fallback.empty()) {
        return false;
    }
    std::error_code error;
    std::filesystem::current_path(fallback, error);
    return !error;
#else
    std::error_code error;
    (void)std::filesystem::current_path(error);
    if (!error) {
        return true;
    }

    const std::filesystem::path fallback = executableDirectory();
    if (fallback.empty()) {
        return false;
    }

    error.clear();
    std::filesystem::current_path(fallback, error);
    return !error;
#endif
}

bool openUrl(const std::string& url) {
    if (url.empty()) {
        return false;
    }

#if defined(_WIN32)
    HINSTANCE result = ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<std::intptr_t>(result) > 32;
#elif defined(__APPLE__)
    const std::string command = "open " + shellQuote(url) + " >/dev/null 2>&1 &";
    return std::system(command.c_str()) == 0;
#elif defined(__ANDROID__) && defined(EUI_WINDOW_BACKEND_SDL2)
    return SDL_OpenURL(url.c_str()) == 0;
#elif defined(__ANDROID__)
    return false;
#else
    const std::string command = "xdg-open " + shellQuote(url) + " >/dev/null 2>&1 &";
    return std::system(command.c_str()) == 0;
#endif
}

FileDialogResult openFileDialog(const FileDialogOptions& options) {
    const std::vector<std::string> names = extensionNames(options.allowedExtensions);
#if defined(_WIN32)
    std::vector<wchar_t> buffer(65536, L'\0');
    const std::wstring title = utf8ToWide(options.prompt.empty() ? "Select file" : options.prompt);
    const std::wstring initialDirectory = utf8ToWide(options.initialDirectory);
    const std::wstring filterStorage = windowsFilterStorage(options, names);

    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.lpstrFile = buffer.data();
    dialog.nMaxFile = static_cast<DWORD>(buffer.size());
    dialog.lpstrTitle = title.c_str();
    dialog.lpstrFilter = filterStorage.c_str();
    dialog.nFilterIndex = 1;
    if (!initialDirectory.empty()) {
        dialog.lpstrInitialDir = initialDirectory.c_str();
    }
    dialog.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (options.allowMultiple) {
        dialog.Flags |= OFN_ALLOWMULTISELECT;
    }
    if (!GetOpenFileNameW(&dialog)) {
        const DWORD error = CommDlgExtendedError();
        if (error == 0) {
            return cancelledFileDialog();
        }
        return failedFileDialog(windowsDialogError(error));
    }
    return selectedFiles(parseWindowsSelection(buffer));
#elif defined(__APPLE__)
    return resultFromCommand(runCommand(appleFileDialogCommand(options, names)), "osascript");
#else
    FileDialogResult zenityResult = runZenityFileDialog(options, names);
    if (zenityResult.status != FileDialogStatus::Failed) {
        return zenityResult;
    }

    FileDialogResult kdialogResult = runKdialogFileDialog(options, names);
    if (kdialogResult.status != FileDialogStatus::Failed) {
        return kdialogResult;
    }
    if (zenityResult.error == "zenity is not available." && kdialogResult.error == "kdialog is not available.") {
        return failedFileDialog("No supported file dialog tool found. Install zenity or kdialog.");
    }
    return !zenityResult.error.empty() ? zenityResult : kdialogResult;
#endif
}

std::string chooseFile(const FileDialogOptions& options) {
    FileDialogOptions singleOptions = options;
    singleOptions.allowMultiple = false;
    const FileDialogResult result = openFileDialog(singleOptions);
    return result.selected() ? result.paths.front() : std::string{};
}

std::vector<std::string> chooseFiles(const FileDialogOptions& options) {
    FileDialogOptions multipleOptions = options;
    multipleOptions.allowMultiple = true;
    FileDialogResult result = openFileDialog(multipleOptions);
    if (!result.selected()) {
        return {};
    }
    return std::move(result.paths);
}

bool initializeTray(const TrayOptions& options) {
    TrayState& state = trayState();
    if (state.initialized) {
        return true;
    }

    const std::filesystem::path resolvedIcon = resolveIconPath(options.iconPath);
    state.iconPath = resolvedIcon.empty() ? options.iconPath : resolvedIcon.string();

    if (!eui_tray_init(state.iconPath.c_str())) {
        state = {};
        return false;
    }

    (void)options.tooltip;
    state.initialized = true;
    return true;
}

bool isTrayInitialized() {
    return trayState().initialized && eui_tray_is_initialized();
}

void pollTray(bool blocking) {
    if (isTrayInitialized()) {
        eui_tray_poll(blocking ? 1 : 0);
    }
}

bool consumeTrayShowRequested() {
    return eui_tray_consume_show_requested() != 0;
}

bool consumeTrayExitRequested() {
    return eui_tray_consume_exit_requested() != 0;
}

void shutdownTray() {
    TrayState& state = trayState();
    if (state.initialized) {
        eui_tray_shutdown();
    }
    state = {};
}

void setTextInputActive(window::Handle window, bool active) {
    (void)window;
#if defined(EUI_WINDOW_BACKEND_SDL2)
    const bool currentlyActive = SDL_IsTextInputActive() == SDL_TRUE;
    if (active && !currentlyActive) {
        SDL_StartTextInput();
    } else if (!active && currentlyActive) {
        SDL_StopTextInput();
    }
#else
    (void)active;
#endif
}

void setImeCursorRect(window::Handle window, float x, float y, float width, float height) {
    core::window::setImeCursorRect(window, x, y, width, height);
}

void requestFrame() {
    frameRequested().store(true);
    window::postEmptyEvent();
}

void requestUiUpdate() {
    uiUpdateRequested().store(true);
    requestFrame();
}

bool consumeUiUpdate() {
    return uiUpdateRequested().exchange(false);
}

bool consumeFrameRequest() {
    return frameRequested().exchange(false);
}

} // namespace core::platform
