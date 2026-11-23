# 科学绘图 Vulkan 发布验收

本记录中的实测环境为 Windows x64、MSVC、GLFW。2026-11-01 的基线二维与三维视口均由 CPU
渲染后上传；2026-11-03 起二维三角批次使用 Vulkan GPU 光栅化，三维/体视口仍为 CPU 参考路径。
验收脚本使用 Python 3 标准库和 CMake，不绑定操作系统或 CMake 生成器；当前实测证据仍只覆盖上述环境，不能据此承诺所有设备、平台及长期运行可靠性。
2026-11-01 的结论是当前配置技术门禁通过，可以交付限定支持范围的候选 SDK；现有证据不足以
宣称全部 Vulkan 设备、所有操作系统或长期运行可靠性均已认证。

## 可复现验收

安装 Vulkan SDK 和 Khronos 验证层，确保 `vulkaninfo`、CMake 与 Python 3 在 PATH 中，在交互桌面运行：

```sh
python3 modules/plot/scripts/test_plot_vulkan_release.py
```

可使用 `--build-dir`、`--output-dir`、`--parallel` 和 `--probe-repeats` 配置构建、证据目录、并行度与探针重复次数。脚本构建全部 plot 单元
与性能测试、Vulkan 和外部图像 probe，强制启用 Khronos 验证层和同步验证，重复运行两个图形
probe，并构建安装包消费者。OutputDir 必须是新路径，脚本不清理已有目录。

脚本关闭 EUI_INSTALL_DEMO_ASSETS，避免默认安装演示音乐、图片、字体。窗口应用可使用系统
字体回退，最终字体和图标显示需在目标机器验证。安装前缀包含静态库、必要头文件、CMake
目标、示例、文档和依赖声明；不是独立 plot DLL 或稳定 C ABI。Vulkan SDK 开发文件是客户
编译依赖，运行最终应用只需要对应驱动/loader 与 MSVC 运行库。

每次运行生成 configure/build/CTest/重复探针/体数据交互/安装/消费者日志、SDK 文件 SHA256
清单和 result.json。技术门禁不等于跨驱动认证或商务授权批准。

## 2026-11-01 实测与修复

基础提交为 9fc6b188f03c5eb2f7b02fb03f0a71d90ad98f43，测试包含工作区尚未提交的修改，
不是该提交本身的发布认证。Windows x64，MSVC 19.51，Visual Studio 18 2026，Release，
GLFW，Vulkan SDK 1.4.350.0；设备 Intel(R) Arc(TM) Graphics，vendor 0x8086 / device 0x7d55，
驱动 101.8826，设备 API 1.4.348，maxImageDimension2D 为 16384。

| 实际缺陷 | 修复与回归证据 |
| --- | --- |
| 首次交换链布局转换没有正确等待 acquire，画面可见仍触发 WRITE-AFTER-READ | 等待阶段和首次转换源阶段同时覆盖 COLOR_ATTACHMENT_OUTPUT / TRANSFER；Khronos 同步验证通过 |
| 相邻三角形公共边重复混合 alpha，半透明矩形对角线变深 | 1/256 像素量化及 top-left 边覆盖；顺/逆绕序、1/1.25/1.5/2 倍 DPI、真实重叠、上下行顺序单元回归 |
| uploadRgba 把左上起点输入倒置，影响直接上传和三维图像 | 直接上传保留行顺序；二维 render 保留既有下起点约定；窗口绿上蓝下像素断言 |
| 图像设备上限没有提前检查 | 创建 VkImage 前检查设备 maxImageDimension2D；超限抛 length_error |
| 极小正尺寸与 DPI 相乘下溢为零 | 拒绝无效物理尺寸；错误输入保留已提交纹理和 revision |

本轮全部 22 项通过，其中 21 项以 plot_ 开头，另一项为 gpu_image_probe。验证环境为
VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation、VK_LAYER_VALIDATE_SYNC=1；两个图形
probe 各重复 5 次均通过，没有 Validation Error、VUID 或 SYNC-HAZARD。
plot_vulkan_probe 还覆盖同尺寸复用、尺寸改变、64 次创建/释放、外部持有图像及已呈现纹理
再次更新。gpu_image_probe 包含外部图像所有权和交换链故障注入。

独立消费者只指向新安装前缀，数据样例 CTest 1/1 通过，窗口样例成功编译链接。另行在有效
桌面启动窗口样例，捕获 900×600 图表及系统回退文字，正常关闭返回 0，同步验证无错误。
本次额外检查 OpenGL 的 numeric、precision、export、raster、runtime_probe 共 5 项均通过。
旧 OpenGL 颜色条测试仅检查红通道阈值，标准 Viridis 替换后失效，已改为逐段 RGBA 像素
与 ColorScale 映射结果比较，通道容差为 1。

完整原始证据位于仓库忽略目录 temp/plot-vulkan-gate-final3-20261101；其中 SDK 不含演示资产，
CMake 导出未引用源目录，五个 plot 私有实现头未安装。sdk-manifest.json 逐文件记录 SHA256。
该目录供本地复核，永久证据摘要为本文，正式发布应把原始日志与交付包一同归档。

## 性能样本与产品范围

以下是同机短测，启用了验证层。二维默认及显式 125k 用例各 20 轮；体绘制 12 轮，
只适合评估本配置的交互代价，不能作为客户机器上的帧率保证。

| 场景 | 拖动整帧中位 ms | P95 ms | 补充 |
| --- | ---: | ---: | --- |
| 二维每序列 125000 点，1920×1080，默认用例 | 11.0781 | 21.7309 | 拖动时半分辨率预览 |
| 同规模显式 125k 用例 | 10.0412 | 12.8968 | 与上一行分别运行 |
| 21³ 体数据，512×384，步长 0.16 | 9.5905 | 10.3733 | 拖动四分之一分辨率；松手全分辨率重绘 90.1584 ms |

建议产品说明写为：提供科学绘图基础接口；Windows x64 / GLFW / Vulkan 的二维三角图层已支持
GPU 光栅化，数据映射和三角化由 CPU 完成，三维/体数据视口仍使用 CPU 参考渲染。数值精度以
[数值契约](科学绘图精度与数值契约.md)为准。

当前未完成 NVIDIA/AMD、多版本 Intel 驱动、SDL2/Vulkan、Linux/macOS 的同等验收；也没有
数小时/数天的进程内存、GPU 内存及句柄增长报告。Vulkan 三维已检查 CPU 回退合成和交互
基准，但没有把所有三维/体场景逐像素读回对照；不能把 OpenGL 的图像对照结果直接算作
Vulkan 已通过。大体数据、松手完整重绘和同步上传仍可能阻塞 UI。

销售材料不得把三维 CPU 回退描述为 GPU 加速，也不得承诺任意精度或恒定实时帧率。
现有 Apache-2.0 权利、额外商业授权权属、第三方再分发义务和 ABI 交付要求见
[二进制 SDK 与商业交付](科学绘图二进制SDK与商业交付.md)。尚未签发商业合同或添加激活机制。

## 本轮代码复核

复核覆盖交换链等待与布局转换、上传行顺序、alpha 公共边、错误输入状态保留、资源所有权及
SDK 安装边界。没有新增第三方运行时依赖，没有新增业务接口；光栅实现头保持私有。同步修复
影响核心 Vulkan 呈现，因此同时运行外部 GPU 图像探针及其故障注入，而不只检查 plot 首帧。
2026-11-01 验收时 git diff --check 通过，修改当时尚未提交或推送；长期资源增长和其他设备矩阵
仍是明确的后续验收项。

## 2026-11-03 Vulkan 二维 GPU 光栅化

`Renderer` 的 Vulkan 分支新增离屏图形管线：CPU 双精度数据映射与 CPU 三角化保持不变，顶点经
host-visible Vulkan buffer 上传，由 GPU 混合绘制至同时用于 color attachment 和 UI sampling 的 RGBA8
图像；同尺寸复用图像和顶点缓冲。绘制前后通过图像布局转换与访问屏障同步 color attachment 和 UI sampling。
设备
`subPixelPrecisionBits < 8` 时回退原软件光栅器，保留 1/256 像素量化和 top-left 覆盖契约。
`uploadRgba()` 仍是独立 CPU 图像上传 API。Plot3D、体数据、拾取和导出路径未改为 Vulkan GPU 执行。

同机 Windows x64 / MSVC / GLFW Release / Intel Arc Graphics（驱动 101.8826，API 1.4.348）下，
Vulkan 发布门禁 22 项通过，两个同步/绘图探针各重复 5 次；OpenGL 27 项 plot 回归通过。
验证层未报告 Validation Error、VUID 或 SYNC-HAZARD。SDK 示例和独立 API 消费者测试各 1/1 通过，
安装包含 16 个 plot 公开头文件，文档链接检查通过。

125k 点/序列、1920×1080、20 轮同机基准中，Vulkan pan compose 中位/P95 为 5.3827/5.8845 ms，
present+GPU wait 为 2.9273/3.703 ms，端到端拖动为 8.3526/9.2005 ms；OpenGL 对照分别为
5.3146/6.7416、2.3600/4.0562、7.7521/9.5619 ms。此测量不证明 Vulkan 整体更快；它记录新 GPU
执行路径的代价，CPU 几何生成仍占显著时间。原始日志为仓库忽略目录
`temp/plot-vulkan-gpu-gate-final2-20261103/`、`temp/plot-vulkan-gpu-opengl-test.log` 与两个
`temp/plot-vulkan-gpu-*-benchmark.json` 文件。结果只适用于该设备组合。

## 2026-11-02 模块目录迁移回归

文档、示例、测试和发布脚本迁入 `modules/plot/` 后，重新通过全部 22 项 Vulkan 回归，两个
图形探针各重复 5 次通过，没有 Validation Error、VUID 或 SYNC-HAZARD。安装包示例消费者
和独立 API 消费者分别通过 CTest 1/1，窗口消费者编译链接通过；此次没有重做上述窗口手测。
SDK 模块材料位于 `share/eui-neo/modules/plot/`，公开头文件数量为 16，文档本地链接检查通过。

同轮 OpenGL 全部 27 项 plot 测试和 6 个示例构建通过。主分支独立通过 `gpu_image_probe` 的
Khronos 同步验证，框架同步修复与通用构建能力已提交到主分支；详见
[模块目录与主分支约定](模块目录与主分支约定.md)。

原始 Vulkan 门禁记录位于 `temp/plot-module-final-gate-20261102/`，记录基于 `c5c55be` 及当时
待提交的迁移修改；不是对基础提交本身的认证。历史性能样本和未验证矩阵保持原有范围。
