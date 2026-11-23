# 键盘与控件语义

点击控件默认参与 Tab / Shift+Tab 导航，Enter / Space 激活。导航按声明顺序循环，跳过 disabled、全透明及 `accessibleHidden()` 子树。`.tabStop(false)` 仅排除 Tab 停靠；`.focusable(false)` 取消键盘焦点。键盘导航或 Enter / Space 操作显示统一焦点框，鼠标或触摸按下时隐藏；仅移动鼠标不隐藏。鼠标和触摸仍可设置焦点，输入框自身的聚焦样式、文本输入和控件语义不受影响。弹层自动聚焦和关闭后的焦点恢复沿用当前输入方式。

- checkbox、switch：Space 切换；radio 可用 `.group("name")` 启用方向键组内切换。
- tabs、segmented：方向键切换并选中同组项目。
- slider：方向键移动一个合法刻度，PageUp / PageDown 移动十个刻度，Home / End 到两端。连续模式分别移动范围的 1% / 10%；最大值不在步长网格上时仍是一个合法端点，不会跳过相邻刻度。
- dropdown：上下方向键选择，Home / End 到首尾，Enter / Space 展开或收起，Escape 收起。
- datePicker、timePicker：打开后约束焦点，选择列使用方向键或 PageUp / PageDown 调整，Tab 到确认按钮提交；Escape 取消。colorPicker 的色板使用方向键切换，通道复用 slider 键盘行为。
- contextMenu、sidebar：打开时约束焦点，Escape 收起并恢复原焦点；菜单项可用 Tab 或方向键切换，Enter 激活。
- menuBar：Tab 进入标题，Down / Enter / Space 展开；Up / Down 跳过禁用项和分隔线，Right 进入子菜单，Left 返回或切换顶层菜单，Escape 关闭。移动焦点不会执行菜单动作。
- dataTable：启用行选择后，上下方向键和 Home / End 移动选择并滚动到目标行，Space 选择，Enter 执行行点击；多选支持 Shift 范围选择和 Ctrl / Command+A。焦点跟随业务 key，行离开已构建的虚拟窗口后清除焦点并保留选择，点击或 Tab 进入可见行后继续导航。
- dialog：打开时保存原焦点并约束 Tab，Escape 关闭，关闭后恢复仍有效的原焦点。自定义弹层可组合 `.focusScope(open).onDismiss(...)`。
- 自定义控件：用 `.onKeyEvent(...)` 实现特定按键，返回 true 表示已处理；用 `.accessible(role, name, value)`、`.accessibleChecked(...)`、`.accessibleExpanded(...)` 声明语义。

关闭焦点范围时，只有焦点仍在关闭的子树内或已失效才恢复原焦点。点击外部输入框关闭 Dropdown 时保留新的输入焦点，可以直接键入。`ui.requestFocus(id)` 在 compose 后从当前可用的 Tab 焦点顺序中校验目标，`tabStop(false)` 的目标不会接受该请求；自定义范围通过树中归属判断焦点，不要求 ID 使用范围前缀。

button、checkbox、radio、switch、slider、input、dropdown 支持 `.accessibleName("用途")`。仅有图标的按钮应显式填写名称。其他可点击控件默认导出 Button 角色，可用底层 Builder 补充具体名称和角色。

`ui.accessibilityTree()` 返回值类型快照，包含父节点、角色、名称、值、选中/展开/禁用/焦点状态。请在布局后读取 bounds；bounds 表示布局逻辑坐标，不包含滚动和动画变换。隐藏子树不导出，禁用状态向后代传递。该阶段提供后端无关的键盘、焦点和语义能力；系统屏幕阅读器桥接尚未实现。

回归验证：`accessibility_probe` 使用真实窗口验证声明顺序、禁用跳过、激活、焦点框、方向键、下拉框与模态焦点恢复；`desktop_controls_probe` 补充外部输入焦点、MenuBar 级联导航和 DataTable 重排与虚拟行滚动后的焦点验证。
