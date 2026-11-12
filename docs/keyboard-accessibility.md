# 键盘与控件语义

点击控件默认参与 Tab / Shift+Tab 导航，Enter / Space 激活。导航按声明顺序循环，跳过 disabled、全透明及 `accessibleHidden()` 子树。`.tabStop(false)` 仅排除 Tab 停靠；`.focusable(false)` 取消键盘焦点。焦点使用统一边框反馈。

- checkbox、switch：Space 切换；radio 可用 `.group("name")` 启用方向键组内切换。
- tabs、segmented：方向键切换并选中同组项目。
- slider：方向键微调，PageUp / PageDown 大步调整，Home / End 到两端。
- dropdown：上下方向键选择，Home / End 到首尾，Enter / Space 展开或收起，Escape 收起。
- dialog：打开时保存原焦点并约束 Tab，Escape 关闭，关闭后恢复仍有效的原焦点。自定义弹层可组合 `.focusScope(open).onDismiss(...)`。
- 自定义控件：用 `.onKeyEvent(...)` 实现特定按键，返回 true 表示已处理；用 `.accessible(role, name, value)`、`.accessibleChecked(...)`、`.accessibleExpanded(...)` 声明语义。

button、checkbox、radio、switch、slider、input、dropdown 支持 `.accessibleName("用途")`。仅有图标的按钮应显式填写名称。其他可点击控件默认导出 Button 角色，可用底层 Builder 补充具体名称和角色。

`ui.accessibilityTree()` 返回值类型快照，包含父节点、角色、名称、值、选中/展开/禁用/焦点状态。请在布局后读取 bounds；bounds 表示布局逻辑坐标，不包含滚动和动画变换。隐藏子树不导出，禁用状态向后代传递。该阶段提供后端无关的键盘、焦点和语义能力；系统屏幕阅读器桥接尚未实现。

回归验证：`accessibility_probe` 使用真实窗口验证声明顺序、禁用跳过、激活、焦点框、方向键、下拉框与模态焦点恢复。
