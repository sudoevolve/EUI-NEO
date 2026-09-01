# FFmpeg 视频播放示例

`ffmpeg_video_player` 是一个可选示例，演示应用如何在后台线程使用 FFmpeg 解码视频，
将每帧整理为 NV12 并提交到 `eui::ImageStream`。UI 线程只消费和绘制最新帧；示例循环播放
视频且不处理音频。

该示例用于验证应用层解码器与动态纹理 API 的衔接，不是框架的视频解码功能。EUI-NEO 核心库
和默认示例不依赖 FFmpeg。

## 构建

先使用当前平台的包管理器安装 FFmpeg 开发文件。以 vcpkg 为例：

```powershell
& "$env:VCPKG_ROOT\vcpkg.exe" install ffmpeg
cmake -S . -B build-video `
  -DEUI_BUILD_APPS=OFF `
  -DEUI_BUILD_FFMPEG_VIDEO_EXAMPLE=ON `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake"
cmake --build build-video --config Release --target ffmpeg_video_player --parallel 4
```

使用其他包管理器时，确保 CMake 能通过 `find_package(FFMPEG)` 找到 FFmpeg 的头文件和库。

## 运行

未设置 `EUI_VIDEO_PATH` 时，示例会播放一个约 2.8 MB、时长约 5 秒的公开 MP4 测试地址：

```text
https://samplelib.com/lib/preview/mp4/sample-5s.mp4
```

该 URL 只用于示例启动后的网络读取，不属于框架资源，不会复制到构建产物或安装目录。生产应用
应显式指定自有的本地文件路径或媒体 URL。PowerShell 示例：

```powershell
$env:EUI_VIDEO_PATH = "https://samplelib.com/lib/preview/mp4/sample-5s.mp4"
.\build-video\Release\ffmpeg_video_player.exe
```

网络 URL 需要所安装的 FFmpeg 启用相应网络协议支持；示例会在 15 秒后终止超时的打开或读取操作，
并在窗口中显示具体失败原因。不可用时可改为本地媒体文件进行验证。
