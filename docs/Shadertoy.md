# Shadertoy 底层图元

OpenGL 和 Vulkan 使用同一套 `mainImage()` 源码、Pass graph、通道、
uniform、预设和图元属性。

```glsl
void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = fragCoord / iResolution.xy;
    fragColor = vec4(uv, 0.5 + 0.5 * sin(iTime), 1.0);
}
```

EUI 提供固定的全屏四边形顶点着色器，并围绕 `mainImage()` 生成 fragment 入口。
`rect` 仍表示四边形几何图元，不是公开的顶点着色器 API。

## 支持范围

当前公开契约保持精简：

- 文件或 inline `mainImage()` 源码；
- 按顺序执行的多 Pass；
- 每个 Pass 固定四路 `sampler2D`；
- `Image`、`Buffer`、`Self`、`None` 四种通道；
- 固定 RGBA32F ping-pong 离屏目标和 feedback；
- 标准 Shadertoy uniform；
- 图片通道、预设 JSON 导入和开发期热更新；
- `float`、`int`、`vec2`、`vec3`、`vec4` 自定义 uniform；
- 普通 UI 的布局、裁剪、圆角、透明度、变换、命中测试和 dirty rect 语义。

两个渲染后端实现同一份契约，不存在跨后端 fallback，也不会用固定 Shader 冒充用户代码。

## 最小 Graph

```cpp
eui::ShaderToyGraph graph;
graph.addPass("bufferA", "shaders/buffer.frag",
              "generated/buffer.frag.spv");
graph.addInlinePass(
    "image",
    R"GLSL(void mainImage(out vec4 color, in vec2 p) {
        color = texture(iChannel0, p / iResolution.xy);
    })GLSL",
    "generated/image.inline.spv",
    "image-inline.frag");

graph.setChannel("bufferA", 0, eui::ShaderToyChannel::self());
graph.setChannel("image", 0,
                 eui::ShaderToyChannel::buffer("bufferA"));
graph.setChannel("image", 1,
                 eui::ShaderToyChannel::image("assets/noise.png"));
graph.setUniform("uAmount", 0.75f);
graph.setUniform("uOffset", eui::Vec2{0.2f, 0.4f});

ui.shadertoy("effect")
    .size(640.0f, 360.0f)
    .graph(std::move(graph))
    .radius(8.0f)
    .opacity(0.95f)
    .resolutionScale(1.0f)
    .timeScale(1.0f)
    .paused(false)
    .resetKey(revision)
    .onCompileError([](const eui::ShaderToyError& error) {
        // 可读取 elementId、passName、stage、sourcePath、line 和 message。
    })
    .build();
```

`ShadertoyBuilder` 继承普通 shape 和 layout 属性，包括位置、尺寸、margin、padding、
布局 fill、z-index、ignore-layout、圆角、透明度、transform、transform origin、
命中测试和 pointer 回调。最终 Pass 通过普通 UI image 合成路径绘制，并应用祖先裁剪。

`resolutionScale()` 为整个 graph 设置一份基础离屏尺寸，所有 Pass 使用相同尺寸。
`timeScale()` 缩放图元局部时钟，`paused()` 冻结输出和时间。修改 `resetKey()` 会重置
时间、帧号和 feedback；图元尺寸或 `resolutionScale()` 变化会重建目标并重置帧号和
feedback，但保留当前时间。graph 资源替换使用新的 feedback，并保留当前时间和帧号。
修改圆角、透明度、裁剪或 transform 不会重置 feedback。

## Pass 和通道语义

执行顺序就是 `ShaderToyGraph::passes` 的顺序。`Buffer` 引用较早 Pass 时读取当前
graph 帧；`Self` 以及对相同或较晚 Pass 的引用读取上一 graph 帧。上一帧不存在时，
这些输入绑定 1x1 空纹理。最后一个 Pass 是 graph 输出。

每个 Pass 持有两张 RGBA32F 目标纹理，每完成一帧 graph 后交换一次。所有 Pass 的目标
尺寸一致：

```text
图元尺寸 * resolutionScale
```

采样策略固定，不提供配置项：

| 通道 | GLSL 类型 | 采样策略 |
| --- | --- | --- |
| `Image` | `sampler2D` | linear，repeat，加载时垂直翻转 |
| `Buffer` | `sampler2D` | linear，clamp-to-edge |
| `Self` | `sampler2D` | linear，clamp-to-edge |
| `None` | `sampler2D` | 1x1 空纹理，linear，clamp-to-edge |

图片复用 EUI-NEO 现有图片解码器和缓存。Shadertoy 不增加第二套图片栈或媒体依赖。

## 文件、Inline 源码和 Vulkan

OpenGL 由驱动编译文件或 inline GLSL。Vulkan 运行时不启动或链接 Shader 编译器；
每个 Pass 使用有效 SPIR-V 路径。文件 Pass 未显式指定时使用 `<source>.spv`，inline
Pass 在 Vulkan 配置中必须显式提供路径。

只构建 OpenGL 应用时，直接把 `.frag` 文件路径交给 graph 即可，不需要为 Shader
修改 CMake：

```cpp
eui::ShaderToyGraph graph;
graph.addPass("image", "assets/shaders/shadertoy/demo.frag");
```

同一应用需要支持 Vulkan 时，才需要在构建期生成 SPIR-V。对于 JSON 配置，推荐
使用配置级 helper；它会读取所有 Pass 和 uniform，不需要在构建脚本中重复列出 shader
文件：

```cmake
eui_compile_shadertoy_config(my_app
    CONFIG "${CMAKE_CURRENT_SOURCE_DIR}/shaders/effect/config.json"
    ASSET_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
    OUTPUT_ROOT "${CMAKE_CURRENT_BINARY_DIR}/assets/shaders")
```

`OUTPUT_ROOT` 对应运行时资源树的 `ASSET_ROOT`，生成的 `.spv` 路径与配置中的相对
`spirv` 路径一致。`.frag` 和图片仍由普通资源部署逻辑复制，不需要通过 CMake 宏传入。

对于不使用 JSON 的单个 Shader，仍可直接使用低级 helper：

```cmake
if(EUI_NEO_RENDER_BACKEND STREQUAL "vulkan" OR
   EUI_RESOLVED_RENDER_BACKEND STREQUAL "vulkan")
    eui_compile_shadertoy(my_app
        SOURCE "shaders/buffer.frag"
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/buffer.frag.spv"
        UNIFORMS "uAmount:float")

    set(inline_shader [[void mainImage(out vec4 c, in vec2 p) {
        c = vec4(p / iResolution.xy, 0.5, 1.0);
    }]])
    eui_compile_shadertoy_inline(my_app
        CONTENT "${inline_shader}"
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/generated/image.inline.spv")
endif()
```

配置 helper 需要 Python 3 和 Vulkan SDK 的 `glslangValidator`；这些工具只属于构建机，
不链接到 `eui::neo`，
不复制到运行包，也不会被纯 OpenGL 配置探测。`UNIFORMS` 使用 `name:type`，名称和
类型必须与 graph 一致。

文件源码热更新采用候选替换。OpenGL 在 GLSL 变化时构建候选 program；Vulkan 在 SPIR-V
变化时构建候选 pipeline。候选失败时报告结构化错误，并继续显示最后一版有效画面。

## JSON 和预设

`eui::loadShaderToyGraphJson()` 加载 UTF-8 文件；
`eui::parseShaderToyGraphJson()` 解析文本并接收明确的 base directory。
fragment、SPIR-V 和图片的相对路径都以该目录为基准。

EUI Shadertoy 只有一种 JSON 配置契约，不使用版本字段。配置文件是唯一的 Pass 图描述，
CMake 和 xmake 会从该文件自动发现所有 Pass，并在 Vulkan 构建中生成对应的 SPIR-V 文件。

```json
{
  "$schema": "../eui-shadertoy.schema.json",
  "passes": [
    {
      "name": "A",
      "source": "A.frag",
      "channels": {
        "0": "image:noise.png",
        "2": "self"
      }
    },
    {
      "name": "Image",
      "source": "Image.frag",
      "channels": {
        "0": "A",
        "1": "buffer:A"
      }
    }
  ]
}
```

`channels` 是按 channel 下标索引的对象，未填写的槽位自动为 `none`。通道值支持
`none`、`self`、`buffer:PassName`、`image:path/to/image.png`；裸字符串也按 Buffer
名称处理，因此 `"A"` 等价于 `"buffer:A"`。Pass 顺序是执行顺序，最后一个 Pass
是 graph 输出。每个 Pass 必须提供唯一的 `name`，并且只能提供 `source` 或
`inlineSource` 其中之一。自定义 uniform 支持 `float`、`int`、`vec2`、`vec3`、`vec4`，
最多 16 个。

`spirv` 对文件 Pass 可省略，默认值为 `<source>.spv`；inline Pass 在 Vulkan 构建中
必须显式提供 `spirv`。相对路径以配置文件所在目录为基准。

机器可读 schema 位于
[`assets/shaders/shadertoy/eui-shadertoy.schema.json`](../assets/shaders/shadertoy/eui-shadertoy.schema.json)。
仓库内 Blackhole 和 Fish 预设均使用该契约。根对象只需要 `passes`，不得提供
`version` 字段；旧数组格式不属于当前公共接口。

## Uniform 契约

| Uniform | 契约 |
| --- | --- |
| `iResolution` | graph 目标像素尺寸，z=1 |
| `iTime` | 应用 `timeScale` 后的局部时钟，暂停时停止 |
| `iTimeDelta` | 两次有效 Shader 帧之间的时间 |
| `iFrame` | 从 0 开始，每完成一帧 graph 增加一次 |
| `iFrameRate` | `1 / iTimeDelta`，无有效 delta 时为 0 |
| `iDate` | 年、月、日、本地午夜以来的秒数 |
| `iMouse.xy` | 图元局部像素，原点在左下角 |
| `iMouse.zw` | 按下时为当前指针位置，释放后为最近点击起点的负值 |
| `iChannel0`-`iChannel3` | 四路固定 `sampler2D` |
| `iChannelTime[4]` | 四项都等于当前图元局部时间 |
| `iChannelResolution[4]` | 实际绑定的 2D 纹理尺寸，z=1 |
| `iSampleRate` | 固定为 `44100.0` |

只有指针位于变换、裁剪后的 Shadertoy 图元内部时才更新坐标。若按下起点位于图元内，
则保持捕获直到释放，使拖动语义连续。UI 其他区域的鼠标活动不会驱动 `iMouse`。

自定义 uniform 名称不能与标准契约冲突。仅更新值不会重建 GPU 资源；修改名称或类型时
才需要重建。

## 双后端兼容矩阵

| 能力 | OpenGL | Vulkan |
| --- | --- | --- |
| 文件 `mainImage()` | 驱动编译 | 预生成或构建期 SPIR-V |
| Inline `mainImage()` | 驱动从内存编译 | 显式预生成或构建期 SPIR-V |
| JSON 预设导入 | 支持 | 支持，构建期自动生成每个 Pass 的 SPIR-V |
| Image / Buffer / Self / None | 四路 `sampler2D` | 四路 `sampler2D` |
| 多 Pass feedback | 固定 RGBA32F ping-pong | 固定 RGBA32F ping-pong |
| 标准和自定义 uniform | 支持 | 支持 |
| 热更新 | GLSL 候选替换 | SPIR-V 候选替换 |
| 运行时 Shader 编译器依赖 | 仅 OpenGL 驱动 | 无 |

Shader 输出使用 straight alpha，并通过普通 UI image 路径应用圆角、透明度、transform、
祖先 clip 和 z-order。

## 明确非目标

以下能力不在当前范围内，也不属于公共 API：

- Keyboard texture；
- Audio FFT 或 waveform texture；
- Video channel 或独立通道时钟；
- Sound pass；
- Cubemap / `samplerCube`；
- Volume / `sampler3D`；
- 动态纹理数据、GPU 纹理导入或 YUV 多平面；
- 每通道可配置的 filter、wrap、mipmap、垂直翻转或色彩空间；
- 每 Pass 分辨率或目标格式。

排除这些扩展可保持框架 API 精简、OpenGL/Vulkan 同步和依赖体积稳定。

完整可运行示例位于
[`examples/shadertoy.cpp`](../examples/shadertoy.cpp)，可切换 inline Demo 和内置的
Blackhole、Fish。预设源码、图片、配置和 schema 都位于
[`assets/shaders/shadertoy/`](../assets/shaders/shadertoy/)。
