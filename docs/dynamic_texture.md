# 动态纹理与图像流

`eui::ImageStream` 是 EUI-NEO 的动态图像输入能力。解码器、屏幕采集或网络线程将
CPU 内存中的帧提交到流，渲染线程在 `image(...).stream(...)` 上消费最新帧。它适用于
远程桌面、视频播放、摄像头预览和实时画面。

## 快速开始

创建流后，将它绑定到图片元素。一个流可由任意生产线程提交，而图片元素只在 UI
组合阶段绑定一次。

```cpp
#include "eui_neo.h"

auto desktop = std::make_shared<eui::ImageStream>(2);

void compose(eui::Ui& ui, const eui::Screen& screen) {
    ui.image("remote.desktop")
        .size(screen.width, screen.height)
        .stream(desktop)
        .fit(eui::ImageFit::Contain)
        .build();
}
```

在采集、解码或网络线程中提交帧：

```cpp
const std::uint32_t width = 1920;
const std::uint32_t height = 1080;
const std::uint32_t stride = width * 4;
auto pixels = std::make_shared<std::vector<std::uint8_t>>(stride * height);

// 将采集到的 RGBA 像素写入 pixels。提交之后不得再修改这块内存。
desktop->submit({pixels, width, height, stride,
                 eui::ImagePixelFormat::RGBA8, frameSequence++});
```

`submit()` 返回 `false` 表示帧的尺寸、步长、平面或缓冲区大小不合法；正常情况下不会因
渲染端暂时落后而失败。

## 帧队列与线程模型

`ImageStream` 是线程安全的有界最新帧邮箱，默认容量为 2。生产端超过容量时会丢弃最旧
帧；渲染端每次只取最新帧，并清除积压帧。该策略优先保证低延迟，不保证逐帧显示。

- 可以从采集、视频解码、网络工作线程调用 `submit()`。
- GPU 纹理创建和更新始终发生在渲染线程，生产端不应调用渲染后端 API。
- `ImageFrame` 保存 `shared_ptr<const std::vector<std::uint8_t>>`，提交后缓冲区必须保持
  有效且内容不可变，直至该帧不再被引用。推荐为每帧分配新缓冲区，或从不会重写仍在
  使用缓冲区的对象池获取缓冲区。
- 流绑定在图片元素期间会保持渲染循环活跃，以便及时消费新帧。隐藏页面或不再使用时，
  应移除该元素或将其改回静态图片，避免无意义的持续渲染。

框架不会主动产生远程桌面帧。生产者由应用接入的采集器、编解码器或传输协议负责。
`examples/dynamic_texture.cpp` 为了演示画面变化，使用元素的 `onFrame()` 人工生成帧；
真实远程桌面应由独立生产线程提交，不需要在 `compose()` 中重复提交。

## 像素格式

所有 `stride` 都以字节为单位。`pixels` 是打包图像的首平面，或 YUV 图像的 Y 平面。

| 格式 | 平面布局 | 步长要求 | 典型用途 |
| --- | --- | --- | --- |
| `RGBA8` | `pixels`，每像素 R/G/B/A 各 8 位 | `stride >= width * 4`，且为 4 的倍数 | 通用 UI 截图、软件渲染输出 |
| `BGRA8` | `pixels`，每像素 B/G/R/A 各 8 位 | `stride >= width * 4`，且为 4 的倍数 | Windows 桌面捕获、部分系统图形 API |
| `NV12` | `pixels` 为 8 位 Y；`plane1` 为交错 U/V | Y: `>= width`；UV: `>= ceil(width / 2) * 2`，且为 2 的倍数 | H.264/H.265 硬解、视频和远程桌面 |
| `I420` | `pixels` 为 8 位 Y；`plane1` 为 U；`plane2` 为 V | Y: `>= width`；U/V: `>= ceil(width / 2)` | WebRTC、软件编解码 |
| `P010` | `pixels` 为 10 位 Y（每样本 16 位存储）；`plane1` 为交错 U/V | Y: `>= width * 2`；UV: `>= ceil(width / 2) * 4` | 10-bit 视频、HDR 输入扩展 |

YUV 帧还必须指定颜色空间和范围：

```cpp
stream->submit({yPlane, width, height, yStride,
                eui::ImagePixelFormat::NV12, frameSequence++,
                uvPlane, nullptr, uvStride, 0,
                eui::ImageColorSpace::BT709,
                eui::ImageColorRange::Limited});
```

可选颜色空间为 `BT601`、`BT709`、`BT2020`；范围为 `Limited` 与 `Full`。不要用默认值
猜测编码器输出：SD/HD 视频、全范围桌面内容和 BT.2020 内容都可能使用不同组合。

P010 采用常见的高位对齐表示，每个 16 位样本的低 6 位为零，U/V 仍按 4:2:0 交错排列。

## 后端行为与 HDR 边界

OpenGL 和 Vulkan 后端对 `NV12`、`I420` 和 `P010` 使用原生多平面纹理上传，并在图片
着色器中完成 YUV 到 RGB 的转换。尺寸不变时复用纹理存储，仅更新内容。两端均支持
BT.601、BT.709、BT.2020 以及 Full/Limited range；P010 使用 `R16/RG16` UNORM 平面并保持
高位对齐的 10-bit 样本语义。

不支持原生 YUV 动态纹理的后端，或 Vulkan 设备不具备所需 `R8/RG8/R16/RG16` 格式的
采样与传输能力时，会先在 CPU 转换为 RGBA8，再复用普通动态纹理。因此 API 和显示结果
保持一致，但 CPU 使用量与内存带宽会更高。`RGBA8` 和 `BGRA8` 也通过普通 RGBA 动态纹理
路径更新。

动态纹理后端只负责帧上传、GPU 色彩转换和绘制，不负责视频解码或应用主循环调度。播放器
示例中的软件解码、帧复制、持续重绘和关闭交换间隔都会影响 GPU/CPU 占用；应用可以使用硬件
解码、按新帧请求重绘并自行选择呈现策略。系统播放器通常还会使用专用视频呈现路径，因此
不能仅凭窗口 GPU 百分比比较两者的纹理后端开销。

`P010` 已支持 10-bit 输入、BT.2020 元数据和显示转换；但当前 EUI-NEO 的窗口帧缓冲与
图片输出仍为 8-bit SDR。它不是完整的 HDR 呈现管线，不能保留 HDR 亮度范围，也没有
PQ/HLG 色调映射与显示器 HDR 元数据协商。

外部 GPU 纹理目前不属于该 API。它需要明确的平台句柄所有权、上下文一致性以及生产者
和消费者之间的 fence 同步；不要用 `void*` 绕过这些约束。

## 远程桌面接入建议

远程桌面链路通常是“捕获/接收 -> 解码 -> `ImageStream::submit()` -> `image.stream()`”。
优先将解码器输出直接映射为 `NV12` 或 `I420`，避免额外转换为 RGBA；Windows 捕获直接
得到 BGRA 时可提交 `BGRA8`。流容量通常保持为 2，容量增大会提高平滑度，但也会增加
输入延迟。

分辨率变化可直接提交新尺寸的帧，框架会释放旧纹理并创建匹配的新纹理。应用仍应让
图片元素尺寸按窗口或远端画面比例重新布局，以获得期望的显示效果。

## 测试与示例

动态纹理格式、校验、最新帧丢弃和 CPU RGBA 回退转换由
`tests/unit/image_stream.cpp` 覆盖；`tests/unit/image_stream_primitive.cpp` 覆盖图片元素的
原生动态纹理创建、格式切换、更新失败与 RGBA 回退。构建并运行测试：

```powershell
cmake -S . -B build-test -DEUI_BUILD_APPS=OFF -DEUI_BUILD_TEST_FIXTURES=ON
cmake --build build-test --config Release --target image_stream image_stream_primitive --parallel 4
ctest --test-dir build-test -C Release -R "^image_stream(_primitive)?$" --output-on-failure
```

可运行 `dynamic_texture` 示例观察画面持续移动。示例每两秒依次提交 NV12、I420、P010，
同时覆盖 BT.709 Limited、BT.601 Full、BT.2020 Limited：

```powershell
cmake -S . -B build
cmake --build build --config Release --target dynamic_texture --parallel 4
.\build\Release\dynamic_texture.exe
```

示例是动态流 API 的可视化验证，不是远程桌面传输协议的实现。

## 可选视频播放示例

仓库提供可选的 `ffmpeg_video_player`，用于验证 FFmpeg 解码后以 NV12 提交至 `ImageStream`
的完整链路。它不属于核心库，也不增加核心库依赖；构建、运行及网络测试媒体说明见
[FFmpeg 视频播放示例](../examples/ffmpeg_video_player.md)。
