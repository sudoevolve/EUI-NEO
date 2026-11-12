# 文档

从[集成指南](guides/integration.md)开始接入；日常使用查阅组件、布局和事件。本文档对应当前源码，发布版本以 [GitHub Releases](https://github.com/sudoevolve/EUI-NEO/releases) 的 tag 为准。

## 使用指南

| 文档 | 内容 |
| --- | --- |
| [集成指南](guides/integration.md) | FetchContent、源码接入、安装 SDK、后端选择 |
| [组件](guides/components.md) | 控件、虚拟表格、实时曲线、长文本输入 |
| [键盘与控件语义](guides/keyboard-accessibility.md) | Tab 导航、焦点、弹层、语义快照 |
| [DSL](guides/dsl.md) | Builder、图元、属性、应用入口 |
| [状态](guides/state.md) | 页面状态、Signal、状态作用域 |
| [布局](guides/layout.md) | Row、Column、Stack、Flow、尺寸和滚动 |
| [事件](guides/events.md) | 键盘、指针、文本、IME、事件派发 |
| [动画](guides/animation.md) | 过渡、缓动、变换 |
| [图片](guides/images.md) | 图片、SVG、GIF、缓存 |
| [动态纹理](guides/dynamic-textures.md) | 图像流、视频像素格式、GPU 图像 |
| [音频](guides/audio.md) | 播放器、资源和同步 |
| [网络](guides/network.md) | 文本与图片请求、结果缓存 |
| [异步任务](guides/async.md) | 执行、取消、重启和回调生命周期 |
| [平台能力](guides/platform.md) | 文件打开/保存、目录选择、窗口、托盘 |
| [可选模块](guides/modules.md) | 键盘、串口模块的构建和接入 |
| [Markdown 视口](guides/markdown-viewport.md) | 只构建可见文本及缓存边界 |
| [Shadertoy](guides/shadertoy.md) | Pass graph、uniform、双后端契约 |

## 架构与开发

- [渲染架构](architecture/rendering.md)：窗口、Runtime 和 GPU 后端边界。
- [保留层缓存](architecture/retained-layers.md)：缓存生命周期、资源和限制。
- [开发与发布](development/build-and-release.md)：构建、测试、SDK、发布流程。
- [Linux IME](platform/linux-ime-cursor.md)：X11 候选框定位与验证方法。
- [UI 设计技能](skills/eui-neo-ui-replicator/SKILL.md)：辅助生成与复刻界面的工作流。
- [贡献规范](https://github.com/sudoevolve/EUI-NEO/blob/main/CONTRIBUTING.md)。

## 目录与命名约定

`docs/` 只维护面向使用者和贡献者的现行说明。实施计划、历史审查、测试验收和性能原始数据放在仓库的 [internal/](https://github.com/sudoevolve/EUI-NEO/tree/main/internal)，不进入官网文档目录。

- `guides/`：使用方法与 API 契约。
- `architecture/`：现行架构与设计边界。
- `development/`：开发、构建和发布流程。
- `platform/`：特定平台的接入与排查。
- `assets/images/`：文档截图。
- `skills/`：可分发的技能说明，保留工具要求的 `SKILL.md` 文件名。
- `releases/`：历史版本说明，供发布流程使用；官网现行文档目录不收录，发布入口为 GitHub Releases。

文件名统一使用小写英文 `kebab-case.md`，目录索引使用 `README.md`。历史记录使用 `YYYY-MM-DD-topic.md`；未实现方案只放 `internal/plans/`，不得写成正式功能介绍。移动文档时必须同步 README、相互引用、网站目录、`llms.txt` 和 sitemap。
