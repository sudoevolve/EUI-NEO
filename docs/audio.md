# 音频

EUI-NEO 提供基于 miniaudio 的轻量音频播放，用于界面提示音、媒体时间轴、配乐同步和宣传片等场景。

## 构建

```sh
cmake -S . -B build
cmake --build build --parallel
```

项目内置的单头文件依赖位于 `3rd/miniaudio.h`。CMake 会优先使用仓库内版本，再根据 `EUI_DEPS_MODE`（`auto`、`bundled` 或 `fetch`）处理依赖。

## 公共 API

```cpp
#include "eui/audio.h"

#include <iostream>

eui::audio::Player player;
if (!player.load("assets/music/theme.mp3")) {
    std::cerr << player.error() << '\\n';
    return;
}
player.play();
// player.pause();       // unavailable时返回 false，并可通过 error() 获取原因
// player.seek(12.5);
// player.stop();        // stops and rewinds to zero
// player.unload();
```

`Player` 只能移动不能复制，方法应在 UI 线程调用。播放器通过 miniaudio 引擎流式读取音频，支持当前构建启用的 MP3、WAV、FLAC、OGG 等格式。

`play()`、`pause()`、`stop()` 和 `seek()` 均返回 `bool`；返回 `false` 时通过 `error()` 获取原因。`pause()` 会保留当前播放位置，`stop()` 会停止并回到开头。

状态和时间接口：

- `loaded()`：音频源已成功打开。
- `playing()`：当前正在播放。
- `finished()`：已经播放到结尾。
- `positionSeconds()`：当前播放位置，单位为秒。
- `durationSeconds()`：音频总时长，单位为秒。
- `error()`：最近一次操作的错误信息。

## 资源路径

```cpp
const std::string path = eui::platform::resolveResourcePath(
    "assets/music/theme.mp3");
player.load(path.empty() ? "assets/music/theme.mp3" : path);
```

`eui_neo_configure_app()` 会把顶层 `assets/` 目录部署到可执行文件旁边。加载前应解析资源路径，使程序从仓库目录、构建目录和安装目录启动时都能工作。

## 配乐同步动画

使用 `positionSeconds()` 作为唯一时间轴，不要再维护第二套计时器，也不要在每帧重置动画阶段：

```cpp
const float t = static_cast<float>(player.positionSeconds());
const float intro = smoothStep(0.0f, 0.6f, t);
```

如果需要卡点，先分析配乐并把拍点时间映射到动画 cue。`apps/promo` 示例从开始按钮启动音频，所有分镜时间都读取播放器游标。

## 错误处理与释放

`load()` 在路径为空、输出设备不可用、文件不存在或格式损坏/不支持时返回 `false`，失败后应立即检查 `error()`。销毁 `Player` 会释放 miniaudio 声音对象和引擎；替换曲目时也可以先显式调用 `unload()`。
