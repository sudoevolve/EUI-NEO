# EUI-NEO 贡献与提交规范

EUI-NEO 的首要目标是**简单、快速、易上手**。贡献应降低用户的接入成本、运行开销或维护成本，而不是扩大长期维护面。

提交 Issue 或 Pull Request（PR）前，请先阅读本规范。未满足强制要求的 PR 可以直接关闭，维护者不负责代为补齐设计、测试或性能数据。

## 优先接受的贡献

项目优先接受以下改动：

- 可复现的 Bug 修复，并包含能防止问题再次出现的测试。
- 面向多数应用、符合现有 DSL 和主题体系的通用组件。
- 有同机、同配置、可重复数据证明的性能优化。
- 与上述改动直接相关的测试、示例和文档修正。

一个 PR 只解决一个明确问题。功能、重构、构建调整和无关格式化不得混在同一个 PR 中。

## 默认不接受的改动

以下改动在编码前必须先提交 Issue，说明用户收益、维护成本、替代方案和体积影响，并获得维护者明确同意；未经同意提交的 PR 默认关闭：

- 新增与 CMake 并行的构建系统或包管理体系，例如 Meson、Bazel、XMake、Conan、vcpkg 或 WrapDB。
- 为已有能力增加第二套配置、发布、依赖解析或 CI 流程。
- 大规模重构、全局 API 重设计或与当前问题无关的抽象层。
- 复制第三方源码、生成代码、构建产物、大型二进制或媒体文件。
- 只服务单个业务页面、品牌样式或演示效果的基础组件。
- 没有测量方法和前后数据的“性能优化”。
- 一个 PR 同时处理多个互不依赖的目标。

CMake 是项目的主要构建入口。新增工具必须减少现有复杂度，并说明替换或删除哪些旧路径；仅增加一种可选方式不构成收益。

## 仓库体积约束

源码库增长是一项需要说明的成本。每个 PR 必须在描述中列出：

- 修改、新增和删除的文件数量及代码行数。
- 仓库追踪文件的净体积变化。
- 新增依赖、资源、生成物及其必要性。

出现以下任一情况，必须在开发前通过 Issue 获得维护者明确同意：

- 自有源码净增超过 500 行。
- 仓库追踪内容净增超过 100 KiB。
- 引入新的外部依赖、第三方源码、二进制文件或生成文件。

上述门槛是预审条件，不是自动获准条件，也不得通过拆分 PR、拆分文件或移动代码规避。评审时优先选择复用现有能力、删除重复代码、缩小 API 和减少依赖的实现。

## Bug 修复要求

Bug 修复 PR 必须写明：

1. 最小复现步骤、预期行为和实际行为。
2. 根因以及受影响的代码路径。
3. 为什么当前改动是解决根因的最小修改。
4. 新增或更新的回归测试。
5. 四种前后端组合的本机验证结果。

不得通过删除示例、跳过失败路径、关闭功能、吞掉错误或扩大裁剪区域来掩盖问题。

## 组件贡献要求

新增组件必须满足以下条件：

- 组件可被不同应用复用，不持有业务数据源，不绑定单个页面。
- 遵守现有目录边界：通用控件放在 `components/`，高度定制的创意控件放在 `components/workshop/`，独立可选能力放在 `modules/`。
- API 遵循现有 Builder 和 lowerCamelCase 命名风格，并复用主题、布局、状态和事件能力。
- 完整处理适用的 default、hover、pressed、disabled、focus 等状态。
- 在不同窗口尺寸下布局稳定，文字不溢出、不遮挡，固定格式控件具有稳定尺寸约束。
- 稳定公共组件通过 `components/components.h` 导出。
- 提供最小可运行示例、必要的自动化测试或视觉 fixture，并更新对应文档。
- 不为局部便利引入新的依赖或重复实现 core 能力。

具体 API 和目录约定见 [组件文档](docs/组件.md)。

## 性能优化要求

性能 PR 必须提供修改前后的原始数据和复现方法。两次测试必须使用同一台机器、相同数据规模、相同编译器、相同 Release 配置和相同前后端组合，并记录：

- 操作系统、CPU、GPU、内存和编译器版本。
- 测试场景、输入规模、预热方式、采样次数和统计方法。
- 与改动相关的指标，例如 CPU 时间、帧时间、峰值内存、GPU 内存、分配次数或产物体积。
- 行为和视觉结果没有退化的验证方式。

仅凭主观感受、单次采样或不同机器间的数据对比，不能证明性能提升。

## 四种前后端组合必须本机验证

所有修改源代码、组件、构建配置、依赖或性能行为的 PR，贡献者必须在自己的本机使用独立构建目录完成以下四种组合：

| 窗口后端 | 渲染后端 | CMake 选项 |
| --- | --- | --- |
| GLFW | OpenGL | `-DEUI_WINDOW_BACKEND=glfw -DEUI_RENDER_BACKEND=opengl` |
| SDL2 | OpenGL | `-DEUI_WINDOW_BACKEND=sdl2 -DEUI_RENDER_BACKEND=opengl` |
| GLFW | Vulkan | `-DEUI_WINDOW_BACKEND=glfw -DEUI_RENDER_BACKEND=vulkan` |
| SDL2 | Vulkan | `-DEUI_WINDOW_BACKEND=sdl2 -DEUI_RENDER_BACKEND=vulkan` |

每种组合都必须执行配置、Release 构建和测试。以 GLFW + OpenGL 为例：

```powershell
cmake -S . -B build-contrib-glfw-opengl `
  -DEUI_WINDOW_BACKEND=glfw `
  -DEUI_RENDER_BACKEND=opengl `
  -DEUI_BUILD_TEST_FIXTURES=ON
cmake --build build-contrib-glfw-opengl --config Release --parallel
ctest --test-dir build-contrib-glfw-opengl -C Release --output-on-failure
```

其余组合分别使用 `build-contrib-sdl2-opengl`、`build-contrib-glfw-vulkan` 和 `build-contrib-sdl2-vulkan`，并替换表中的两个 CMake 选项。涉及界面、布局、输入或渲染的改动，还必须启动相关示例或 fixture 进行实际交互和视觉检查。

PR 描述必须逐项记录四种组合的结果，以及本机的操作系统、CPU、GPU 和编译器。CI 不能代替本机验证；任一组合未执行或失败时，代码 PR 不满足提交条件。仅修改文字且不改变代码、构建或资源的 PR 可免于四组合验证，但仍须检查链接和格式。

## 提交消息规范

提交消息使用带 scope 的 Conventional Commits：

```text
type(scope): subject
```

- `type` 和 `scope` 使用小写英文。
- `subject` 使用简短英文祈使句，不加句号。
- 可用类型：`feat`、`fix`、`docs`、`style`、`refactor`、`perf`、`test`、`build`、`ci`、`chore`、`revert`。
- 禁止使用 `fix bug`、`update files`、`change code` 等无法说明实际改动的描述。

示例：

```text
fix(layout): center wrapped text vertically
feat(components): add reusable segmented control
perf(render): reuse unchanged retained layers
docs(contributing): define pull request requirements
```

## PR 描述清单

提交 PR 时请完整填写以下内容：

```markdown
## 问题
- 复现方式或用户需求：
- 根因：

## 修改
- 实现方案：
- 为什么符合简单、快速、易上手的原则：

## 影响
- 修改/新增/删除文件数：
- 代码净增行数：
- 仓库追踪内容净增体积：
- 新增依赖或资源：无 / 请说明

## 本机环境
- OS：
- CPU：
- GPU：
- 编译器：

## 验证
- [ ] GLFW + OpenGL：Release 构建和测试通过
- [ ] SDL2 + OpenGL：Release 构建和测试通过
- [ ] GLFW + Vulkan：Release 构建和测试通过
- [ ] SDL2 + Vulkan：Release 构建和测试通过
- [ ] 已运行与改动相关的示例或 fixture
- [ ] Bug 修复包含回归测试，或已解释无法添加的原因
- [ ] 性能声明包含修改前后数据和复现方法
```

勾选清单代表贡献者已经实际完成对应验证。不得填写未执行的结果。

## 合并判断

维护者会围绕以下问题评审贡献：

- 普通用户是否能更简单地接入或使用 EUI-NEO？
- 默认构建、启动或运行是否保持快速？
- 新增代码和依赖是否与普遍收益相称？
- 是否增加了长期并行维护的配置、API 或工具链？
- 能否通过更小的改动、复用或删除代码达到同样目的？

当长期维护成本大于多数用户能获得的收益时，PR 不会合并。
