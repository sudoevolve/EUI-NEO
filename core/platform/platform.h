#pragma once

#include <string>
#include <vector>

#include "core/window/window_backend.h"

namespace core::platform {

struct TrayOptions {
    std::string tooltip;
    std::string iconPath;
};

/** @brief 文件系统对话框模式。 */
enum class FileDialogMode { OpenFile, SaveFile, Directory };

struct FileDialogOptions {
    std::string prompt;
    std::vector<std::string> allowedExtensions;
    std::string initialDirectory;
    std::string filterName;
    bool allowMultiple = false;
    FileDialogMode mode = FileDialogMode::OpenFile;
    std::string defaultFileName;
    window::Handle parent = nullptr;
};

enum class FileDialogStatus {
    Selected,
    Cancelled,
    Failed
};

struct FileDialogResult {
    FileDialogStatus status = FileDialogStatus::Cancelled;
    std::vector<std::string> paths;
    std::string error;

    bool selected() const {
        return status == FileDialogStatus::Selected && !paths.empty();
    }
};

bool repairCurrentWorkingDirectory();
std::string resolveResourcePath(const std::string& path);
bool openUrl(const std::string& url);
FileDialogResult openFileDialog(const FileDialogOptions& options = {});
/** @brief 保存路径选择；确认后仍由调用方写入文件。 */
FileDialogResult saveFileDialog(const FileDialogOptions& options = {});
/** @brief 选择本地目录。 */
FileDialogResult selectDirectoryDialog(const FileDialogOptions& options = {});
std::string chooseSaveFile(const FileDialogOptions& options = {});
std::string chooseDirectory(const FileDialogOptions& options = {});
std::string chooseFile(const FileDialogOptions& options = {});
std::vector<std::string> chooseFiles(const FileDialogOptions& options = {});
bool initializeTray(const TrayOptions& options);
bool isTrayInitialized();
void pollTray(bool blocking = false);
bool consumeTrayShowRequested();
bool consumeTrayExitRequested();
void shutdownTray();
void setImeCursorRect(window::Handle window, float x, float y, float width, float height);
void requestFrame();
void requestUiUpdate();
bool consumeUiUpdate();
bool consumeFrameRequest();

} // namespace core::platform
