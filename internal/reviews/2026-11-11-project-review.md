# EUI-NEO 项目审查与优化建议

审查日期：2026-11-11。范围：自有 core、公共头文件、components、modules、测试、CMake、CI、发布和主要文档；第三方源码未逐行审计。本次为代码审查与定向验证，不代表全部平台和后端已经重新验收。

2026-11-12 实施更新：本文保留初次审查时的发现与验证边界。P1 三项、键盘/焦点/语义、虚拟表格与排序筛选/列宽、保存与目录选择、撤销字节预算和实时曲线已经在 dev 分步实现并提交。最新四后端结果、性能原始数据和剩余平台验收范围见 [数据工具变更验证](../validation/2026-11-12-data-tools.md)。原文其他建议仍作为后续路线参考。

## 总体判断

项目已经形成较完整的桌面 UI 框架：GLFW/SDL2 与 OpenGL/Vulkan 分层、DSL、状态作用域、布局、事件、动画、局部重绘、retained layer、文本与图片缓存、虚拟列表、动态纹理、音频、网络、SDK 安装消费和多平台发布均已有实现。下一阶段应优先完善正确性、交互完整性和目标设备验证。

值得保留的设计：

- 窗口、Runtime、渲染后端职责明确，同一套组件复用两种 GPU 后端。
- 已有 dirty rect、绘制顺序缓存、保留层和虚拟化，性能优化不是空白。
- 图片缓存已有数量和字节上限，不能笼统地把缓存高水位当成泄漏。
- 现有 Markdown benchmark 记录了同机对照、原始数据和测量限制。
- Linux CI 已覆盖四种窗口/渲染组合，并测试安装后的 SDK 消费方。
- 当前有 28 个 unit 源文件、10 个 backend probe 源文件及一批交互 fixture；应重点补齐未覆盖的契约。

以下将实测问题、静态审查发现和功能建议分开。P1 表示建议最先处理，P2 表示随后完善，P3 表示由应用需求决定，不表示它们已经进入项目承诺。

## P1：已经复现的正确性问题

### 1. 异步任务进入完成队列后，restart/cancel 无法阻止旧回调

依据：`core/platform/async.cpp:119` 的 `finishTask()` 在入队前检查 generation，但 `gMainQueue` 只保存裸回调；`:199` 的 `dispatchReady()` 执行时不再检查任务身份。任务已经 Done、回调尚未派发时，restart 或 cancel 都无法撤销它。

复现步骤：

1. `runOnce(key, workA, thenA)`，等待 `status(key) == Done`，暂不派发回调。
2. `restart(key, workB, thenB)`，等待新任务 Done。
3. 调用 `dispatchReady()`，旧、新回调都会执行。
4. 另一任务在 Done 后、派发前调用 `cancel(key)`，返回 true，但其回调仍会执行。

Windows / MinGW GCC 16.2.0 临时探针输出：

```text
restart: stale_callback=1 current_callback=1
cancel: returned=1 queued_callback=1
```

这与 `docs/guides/async.md` 中旧任务不会继续更新 UI 的契约不一致。搜索框快速输入、页面刷新或页面销毁时，旧回调可能更新过期状态；若回调捕获已销毁对象，还可能访问无效引用。

建议：完成队列携带 key、generation 或独立任务身份，执行前重新校验有效性；同时定义 Done 但尚未派发时的取消语义。若允许 forget 后复用 key，任务身份还应避免 generation 从头计数造成误认。测试覆盖“旧任务先完成、新任务仍运行”和“本轮派发中的前一个回调取消后一个回调”。

### 2. 串口打开失败后，仍保留有效句柄并报告已打开

依据：`modules/serial/serial.cpp:91` 直接将系统打开结果写入成员变量，后续参数配置失败时没有统一回滚；重复调用 open 也会覆盖原有资源。

在 Windows 上对系统 `NUL` 设备调用 `open("NUL", 9600)`：系统打开成功，串口参数查询失败，探针得到：

```text
serial NUL: open_ok=0 is_open=1
```

`NUL` 仅用于安全触发配置失败路径，不代表真实串口设备的验收结果。相同的资源管理问题也出现在参数不受支持等提前返回路径。POSIX 路径还没有检查 `tcgetattr`、`tcsetattr` 等调用的失败。

建议：用局部 RAII 资源完成全部配置后再提交给对象；失败返回时关闭新资源，并明确重复 open 的处理策略。覆盖打开失败、配置失败、重复打开和析构测试；Linux 用伪终端验证，无需把所有基础测试绑定到真实串口硬件。

### 3. SerialPort 可复制，复制后两个对象拥有同一系统资源

依据：`modules/serial/serial.h:79` 有析构函数和 HANDLE/fd 成员，却未限制复制。类型探针确认：

```text
serial: copy_constructible=1 copy_assignable=1
```

复制构造会共享底层句柄；一个对象关闭后，另一个对象仍保留旧句柄。复制赋值还会覆盖目标原有资源。句柄或 fd 被系统复用后，再次 close 可能影响其他资源。

建议：删除复制构造和复制赋值；有明确容器/返回值需求时实现 noexcept 移动语义。测试覆盖移动后源对象为空、资源只释放一次、对自身移动赋值的约定。

## P1/P2：静态审查发现的可靠性缺口

### 4. libcurl 初始化并发和下载失败处理

依据：`core/platform/network.cpp:74` 的 `ensureCurlReady()` 用普通静态 bool 做初始化判断。静态变量自身的初始化安全，不代表后续 `if (!initialized)` 及赋值具备线程同步；线程池中多个首次请求可并发访问这两个变量。此项为静态发现，未运行 ThreadSanitizer 复现。

另外，`:45` 的文件写回调不检查流状态，总是返回请求写入的字节数；`:139` 关闭文件后也未检查写入结果。磁盘满或写入失败时，即使 HTTP 和 CURL 均成功，也可能返回成功。下载直接截断目标文件，非取消类失败还会留下部分文件。Windows 无 CURL 路径使用 `URLDownloadToFileA`，传输中的协作取消和超时控制与 CURL 路径也不同。

建议：采用线程安全的一次初始化并明确 cleanup 生命周期；写入失败向 CURL 返回失败；先写临时文件，完整成功后替换目标。增加本地 HTTP 服务测试，覆盖空响应、404、重定向、中途断开、取消、并发首次请求和写入失败。文档列明平台差异。

### 5. 文本“shaping”与文档承诺不一致

依据：`core/render/text.cpp:885` 的 `shapeWithFallback()` 逐 Unicode codepoint 调用 `FT_Get_Char_Index`，`:912` 的 shaping 入口直接调用它。未找到完整文字 shaping、双向文本重排及 grapheme cluster 分割实现；`docs/guides/components.md:474` 却声明适配阿拉伯连字。

字体回退、单个 emoji 和组合符号的部分处理已有实现，但不能据此认定阿拉伯文、印地语、ZWJ 家庭 emoji、旗帜及混合 RTL 文本已经完整支持。当前光标边界也主要与 codepoint 的 UTF-8 字节边界对应。

建议：先收紧文档声明，补充真实字符串的测量、渲染、选区和删除用例；国际化需求明确后，再评估可选 shaping 与 bidi 能力及其体积代价。不能只接入 shaping 而忽略编辑、换行和光标映射。

### 6. 应用编译策略由框架接管，源码接入与 SDK 接入行为不同

依据：`CMakeLists.txt:112` 的编译选项函数在非 Debug 下对 GCC/Clang 应用目标施加 `-Os -fno-exceptions -fno-rtti`；MSVC 还设置优化及 `/GS-` 等选项。`:471` 的源码版 `eui_neo_configure_app()` 会调用它，`:909` 的安装包版 helper 则只设置必要的平台配置。

结果是同一应用从 add_subdirectory 换成 find_package 后，编译策略发生变化。业务代码或依赖若需要异常、RTTI，源码接入可能直接遇到编译限制。当前 `vcd_viewer`、`gpu_image_probe` 的目标名特判也说明通用策略已经存在例外。

建议：将框架内部优化与消费方编译策略分开；对业务应用只添加必要设置，体积优化采用显式选项。用同一个消费方分别验证源码接入、FetchContent 和已安装 SDK，包括使用异常/RTTI 的最小应用。此项尚未逐编译器复现。

## P2：最值得补齐的产品功能

| 方向 | 当前依据与限制 | 推荐最小范围 |
| --- | --- | --- |
| 键盘导航 | Runtime 有 focus 和按键回调；button、checkbox、radio、slider 等未提供统一键盘行为 | Tab/Shift+Tab、Enter/Space、方向键、焦点可视化、弹窗焦点约束与关闭后恢复 |
| 无障碍 | 自有源码未找到系统无障碍语义桥接实现 | 先定义 role/name/value/state/action，再按目标平台接入 UI Automation、AT-SPI 等；无需一次覆盖全部平台 |
| 实用表格与树 | dataTable 接收完整字符串二维数组，逐行创建节点，并将行高压入固定区域；现有 virtualList 可复用 | 稳定行 key、可见行虚拟化、排序/筛选、选中、列宽；TreeView 按真实需求再做 |
| 文件对话框 | 文档明确当前仅打开文件，阻塞式，无保存、目录选择、父窗口绑定 | 保存文件、目录选择、父窗口/modal、统一错误；评估 Linux portal 和沙箱场景 |
| 网络请求 | 公共 API 主要是下载和一次性文本 GET | 按实际业务增加 method/headers/body/status/error、超时及取消；先做好 HTTP，再决定其他协议 |
| 页面异步生命周期 | task key 是全局字符串，页面状态可被释放；现有状态文档也提出异步应跟随页面取消 | 可选页面/窗口任务作用域、销毁时失效、明确多窗口 key 命名和引用捕获规则 |

不建议把 MQTT、CAN、Modbus、WebSocket 全部塞进 core。若设备应用需要，放到独立 modules 或应用适配层，复用现有 UI 线程派发入口。

## 嵌入式 Linux：先做目标设备闭环

当前窗口代码请求 OpenGL 3.3 Core，shader 使用 GLSL 330；CMake 只选择 OpenGL/Vulkan。桌面 Linux 可运行不等于常见 ARM 工控板已经可运行。优先确定一块具体开发板、显示分辨率、GPU 驱动与内存预算，再选择后端路线。

1. **先做 ARM64 交叉编译和部署示例。** 提供 CMake toolchain/sysroot 用法、依赖裁剪、字体部署与最小运行包，并记录真实板端结果。
2. **按硬件决定 OpenGL ES/EGL。** 常见嵌入式 GPU 更偏向 GLES；需要核对 shader、纹理格式、FBO 和扩展。无桌面部署再考虑 DRM/KMS/输入接入，不能仅改 shader 版本号就宣称支持。
3. **保留 CPU 后端为独立方向。** `internal/plans/software-renderer.md` 是规划，尚无实际后端。若目标是无 GPU 设备，优先实现基本几何、文字、图片、裁剪和呈现，再明确效果降级范围。
4. **补齐触摸产品行为。** SDL2 已有单指映射和滚动；手势仲裁、多指、方向冲突、软键盘遮挡和真实触摸屏验证仍需完善，详见 `docs/guides/events.md`。
5. **验证原生 Wayland。** 当前 CI 显式关闭 GLFW Wayland，IME 记录主要针对 X11；候选框位置、窗口移动/缩放、剪贴板、DPI、弹窗与多显示器需要独立验证。
6. **提供部署裁剪。** 音频当前始终加入 core，图片/网络等也缺少统一的按需能力组合。先测量静态链接实际裁剪效果、依赖和 assets 体积，再决定增加哪些选项，避免配置组合爆炸。

面向求职，优先完成一个可运行的“嵌入式 Linux 设备面板”：串口真实/模拟数据、实时曲线、告警、配置保存、断线恢复、触摸操作、ARM64 部署，以及 CPU/内存/帧耗时报告。EUI-NEO 能体现 C++ 框架和图形能力；若主投 Qt 岗位，再配一个独立 Qt 示例展示 Qt 的模型视图、线程和信号槽经验。

## 性能优化：按证据选择

以下是值得测量的候选项，本次没有生成性能提升结论：

- **输入撤销的字节预算。** `components/input_model.h:328` 保存完整文本快照，`:354` 限制 128 层，但没有按字节限制。1 MiB 文本若保留约 128 份相近大小快照，仅历史文本就约 128 MiB；该值是容量估算，不是实测峰值。先测长文本连续编辑，再考虑字节上限、编辑合并和增量历史。
- **图表数据规模。** `components/linechart.h` 当前为每个点、线段、标签和 tooltip 建节点，数值按 0..1 映射；适合轻量仪表展示。采集曲线优先做可见范围筛选、峰值保留降采样和批量绘制。`internal/plans/scientific-plotting.md` 已有规划，不应将其当成现有能力。
- **内存预算可配置。** 图片已有 64 MiB retained CPU 图片缓存上限；小内存设备可需要更低预算。统一观察字体 atlas、CPU 图片、GPU 纹理、render cache 和 retained layer 占用，并定义主动释放/空闲收缩，而非盲目删除缓存。
- **页面重组与输入遍历。** 检查 compose/layout/命中测试的 p95 耗时和分配数，确定是否需要子树更新或索引。已有虚拟化与 hover 缓存，应在实际瓶颈上继续优化。
- **编译和 SDK 头依赖。** 公共 facade 直接引用 core DSL，大量实现位于头文件，安装包也暴露相应 core 目录。先测最小应用的干净/增量编译时间与预处理体积，再考虑将非模板实现移入 cpp；当前属于源码 API 边界，不宜承诺稳定二进制 ABI。

## 测试、发布与维护

- **补异步、网络、串口测试。** 当前没有独立的对应 unit 源文件；优先覆盖本次发现的契约和失败路径。
- **拆开纯单测与窗口 fixture 开关。** 所有 unit 和 probe 都位于 `EUI_BUILD_TEST_FIXTURES` 条件内，开启纯单测也会生成交互 fixture 目标。可以沿现有 CMake 增加独立测试开关或细分现有开关。
- **加强持续验证。** 常规 CI 是 Linux 四组合；Windows/macOS 的发布流程会跑 unit，但未形成同等的日常后端 probe 矩阵。优先增加 Windows/macOS 日常 smoke，并逐步补图像验证、真实 GPU 和 Wayland，避免把 Xvfb 软件渲染当成硬件验收。
- **加入完整 core 的诊断构建。** 已有部分 ASAN/UBSAN 测量记录，但没有常规 CI sanitizer 配置。先加 Linux Debug + ASAN/UBSAN；并发部分再评估 TSAN，UTF-8、JSON/importer 和图像输入适合定向 fuzz。
- **验证最低 CMake。** README 声明 3.14+，依赖脚本使用较新 CMake 的 `DOWNLOAD_EXTRACT_TIMESTAMP`。本次未实际运行 CMake 3.14，建议用最低版本验证 configure/fetch/consumer；据结果采用版本保护或调整最低版本。
- **共享库与离线契约纳入现有 CI。** 当前日常矩阵主要是默认静态库和 auto 依赖，建议定期验证 shared、严格 bundled/offline、无 Markdown/无 tray 等关键配置，不必穷举全部开关。
- **持续维护依赖清单。** 现有第三方说明和固定版本是良好基础；进一步记录版本、来源、许可、修改补丁和升级验证范围。未核对漏洞公告前，不应仅凭版本较旧断言存在漏洞。
- **局部拆分维护热点。** text、Markdown、DSL、Vulkan Shadertoy 和 backend 文件承担较多职责。只在改动触及对应职责时提取边界，避免为减少文件行数发起全库重构或新增构建体系。

## 推荐顺序与完成标准

| 顺序 | 工作 | 完成标准 |
| --- | --- | --- |
| 1 | 修异步回调失效、串口 ownership/失败回滚、网络初始化与写失败处理 | 失败路径有回归测试，相关平台/后端通过项目规定验证 |
| 2 | 校正文档支持范围，统一消费方编译策略，补三类基础测试 | 文档与实际行为一致，源码/SDK 消费方行为有对照 |
| 3 | 键盘导航、文件对话框、虚拟表格 | 一个实用桌面应用能完整操作，焦点与大数据交互有测试 |
| 4 | 一块 ARM64 板卡的完整设备面板 | 可重复构建部署，有真实设备、触摸和资源测量记录 |
| 5 | 根据用户需求选择 GLES/CPU、复杂排版或科学绘图 | 每次只选一条路线，有明确能力边界和验收场景 |

## 本次验证与改动边界

- 修改中英文 README 开头的求职说明，方向为嵌入式、Linux 应用开发、Qt 开发。
- 新增本分析文档；本次未修改框架逻辑、API、构建系统或依赖，也未执行提交和发布。
- 临时探针使用原始 `core/platform/async.cpp`、`modules/serial/serial.cpp`，通过公开异步 API 验证排队回调问题，通过类型 traits 和 `NUL` 失败路径验证串口问题；仅将平台唤醒函数替换为空实现，不创建 GUI。
- 原有 `dirty_rects` 单测以 MinGW 独立编译运行通过。`pointer_input` 独立链接需要窗口后端实现，未完成该独立运行；这不构成其行为失败的证据。
- 当前已有 build 关闭了 `EUI_BUILD_TEST_FIXTURES`，本次未改其配置、未运行完整 CTest，也未宣称完成四种后端组合、真实串口、Linux/ARM 或视觉回归验证。
- 网络并发、磁盘写失败、复杂文字和编译策略问题的结论来自静态代码审查，仍需各自的回归复现与平台测试。
