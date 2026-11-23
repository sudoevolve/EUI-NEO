# 桌面表单、菜单与数据选择

完整示例见 [`examples/desktop_settings.cpp`](../../examples/desktop_settings.cpp)，构建目标为 `desktop_settings`。它演示菜单栏、表格多选和设置对话框，展示各控件如何共用表单行。

## 尺寸与布局

compose 中的几何尺寸、字号和主题 metrics 都使用同一套逻辑单位。Runtime 统一应用 `dpiScale * uiScale`，不要再次给坐标或字号乘 DPI。

```cpp
app::DslAppConfig{}.logicalWindowSize(960, 720).uiScale(1.0f);
app::DslWindowConfig{}.logicalWindowSize(640, 480);
```

`logicalWindowSize` 让初始客户区接近指定的 compose 尺寸，主窗口和子窗口都支持。创建后根据显示器 DPI、framebuffer/原生坐标比例和 uiScale 换算。操作系统尺寸限制、最大化和取整可能影响最终尺寸，布局应读取 `screen.width/height`。

已有 `windowSize/windowWidth/windowHeight` 保持原生窗口单位，在 Windows 上通常是客户区像素，在 macOS 上通常是点。`windowSize` 切回原生模式；单独设置 width/height 保持当前模式。`windowPosition/minWindowSize/maxWindowSize` 继续使用原生单位。

普通复合组件统一支持 `x/y/position/zIndex/margin/flexGrow/flexShrink/ignoreLayout`，覆盖 Button、Input、Checkbox、Radio、Switch、Slider、Stepper、Dropdown、Tabs、Segmented、DataTable、Card、导航、图表、滚动容器和 Markdown。属性只作用于组件根节点：

```cpp
components::input(ui, "address").position(24, 80).size(320, 32).zIndex(2).build();
ui.row("actions").size(500, 32).gap(12).content([&] {
    components::button(ui, "save").size(100, 32).text("Save").build();
    components::input(ui, "search").size(200, 32).flexGrow().build();
}).build();
```

Row/Column 中通常不用显式 position。Dialog、DatePicker 等屏幕级弹层仍通过 screen、size、anchor 等专用 API 定位。

## Form 与标签

```cpp
components::form(ui, "settings.form")
    .position(16, 48).width(600).theme(tokens)
    .labelWidth(124).controlHeight(28).gap(12).padding(16)
    .row("address", "Address", [&](auto& u, float w, float h) {
        components::input(u, "settings.address").size(w, h).bind(address).build();
    })
    .row("proxy", "Proxy type", [&](auto& u, float w, float h) {
        components::dropdown(u, "settings.proxy").size(w, h)
            .items({"System", "HTTP", "SOCKS5"}).bind(proxy).build();
    })
    .actions("buttons", [&](auto& u, float w, float h) {
        components::button(u, "settings.apply").size(100, h).text("Apply").onClick(apply).build();
    }).build();
```

Form 按内容计算总高度，标签与控件共用行框。回调在 build 时同步接收控件可用尺寸；`row` 的最后一个参数可单独指定行高。`fullRow` 占整行，适合说明和复选框；`actions` 与控件列对齐。行 id 在表单内必须唯一，内容中组件 id 在页面内必须唯一。

`FormMetrics` 可同时配置标签宽、控件高、行间距、列间距、内边距。窄表单会缩小标签列并限制文字绘制区域；长标签可增加行高。主题字号、padding、控件尺寸也可从 `tokens.metrics` 读取。

`components::label(ui, id)` 返回默认垂直居中的 TextBuilder，适合分类行和控件标签。文字框应与行背景同 y、同高度。基础 `ui.text` 继续默认 Top，适用于多行正文。标准 Button 的标题已经居中。

## Dropdown 与弹层

- `.bind(selection)` 只控制选中项。
- `.defaultOpen(true)` 或 `.open(true)` 设置非受控模式的初始展开值，后续 compose 保留内部状态。
- `.bindOpen(openSignal)`，或 `.open(value).onOpenChange(callback)` 持续控制展开状态。
- 点击字段或 Enter/Space 展开，方向键调整选中值；选择、Escape 和外部按下关闭。外部点击继续送达目标控件。

根节点只占字段高度。popup 是树中的普通节点，通过 ignoreLayout 不参与表单测量。展开时使用 `layer::Popup` 提升所在子树，关闭时不保留弹层层级。自定义高层级页面可用 `popupZIndex(value)` 调整；`zIndex(value)` 设置字段根节点层级。

默认层级为 `layer::Content=0`、`Popup=100`、`Dialog=1000`、`Menu=1050`。弹层仍遵守祖先 clip；需要超出滚动视口的菜单，应放到无该 clip 的外层。

Dialog 根节点建立 stackingContext，内部高 z 子节点不会把整个对话框提升到另一个对话框之上。自定义模态容器可使用：

```cpp
ui.stack("modal").size(screen.width, screen.height).zIndex(components::layer::Dialog)
    .stackingContext().content([&] {
        ui.rect("modal.bg").size(500, 400).color(background).build();
        components::label(ui, "modal.title").size(460, 32).text("Settings").build();
    }).build();
```

背景与内容放在同一子树，无需给每个文字重复指定全局 z。基础 DSL 的 `onOutsidePress` 在点击目标不属于该子树时调用，不吞掉目标点击；`requestFocus(id)` 在下次 compose 后请求可见且位于活动 scope 中的焦点目标。

## Slider 业务范围

```cpp
components::slider(ui, "limit").size(320, 28)
    .range(0, 500).step(5).bind(limitMiBPerSecond).build();
```

`value/bind/onChange` 都使用业务值，拖动中的视觉位置也吸附步长。步长相对最小值，两个端点总是可达；0 步长表示连续。方向键移动一步，连续模式为范围的 1%；PageUp/Down 移动十步，连续模式为 10%；Home/End 到端点。range 与 bind 的调用先后不影响结果。

默认范围仍为 0..1；逆序端点自动交换，相同端点表示固定值并禁用交互。

## 类型化表格与选择

```cpp
struct Download { std::string id, name, status; };
// rows 为 shared_ptr<vector<Download>> 或 shared_ptr<const vector<Download>>。
components::dataTable(ui, "downloads").size(700, 400)
    .data<Download>(rows, {
        {"Name", [](const Download& row) { return row.name; }},
        {"Status", [](const Download& row) { return row.status; }}
    }, revision)
    .rowKey([rows](std::size_t index) { return (*rows)[index].id; })
    .selectionMode(components::DataTableSelectionMode::Multiple)
    .bindSelection(selectedKeys).build();
```

类型化列按数据 identity/revision 和列标题缓存，普通 compose 不重复格式化。源对象、getter 或 key 规则改变时递增 revision，不能与 compose 并发修改源 vector。小数据可用 `.rows<Download>(values, columns)` 提供拥有所有权的快照；原有字符串行 API 保持可用。

默认不启用选择；bindSelection 默认启用单选，显式 selectionMode 可选择 None、Single、Multiple。selectedKeys 与 onSelectionChange 支持受控选择；不绑定时，已启用的选择保存在组件状态中。

多选支持普通点击、Ctrl/Command 切换、Shift 按视图顺序选范围，以及 Ctrl/Command+A 选择当前筛选结果。方向键、Home/End 移动选择并滚动到目标行，Space 选择，Enter 调用行点击回调。选择通过 `AccessibleNode.selected` 导出。

key 必须唯一。稳定 key 保证排序、过滤和源数据重排不串行；过滤保留隐藏行选择，删除源行清除无效 key 并同步受控选择。键盘焦点也跟随业务 key；手动滚动使焦点行离开已构建的虚拟窗口时清除该焦点，保留选择，再次点击或 Tab 进入可见行后继续导航。未提供 rowKey 时使用原始行索引，源数据插入/删除/重排时应改用业务 ID。onRowClick 继续返回原始数据行索引。

## MenuBar

```cpp
components::menuBar(ui, "menu").screen(screen.width, screen.height).theme(tokens)
    .menus({
        {"File", {{"Open", openFile}, components::ContextMenuItem::separatorItem(), {"Save", saveFile}}},
        {"Help", {{"About", showAbout}}}
    }).build();
```

这是客户区菜单栏，复用 ContextMenu 的级联子菜单。动作存入 ContextMenuItem，也可通过 `onSelect(menuIndex, path)` 统一处理。子项 enabled=false 禁用动作，separatorItem 创建不可聚焦的分隔线。

点击标题展开，展开后悬停另一个标题切换。Tab 进入菜单栏，Down/Enter/Space 展开，Up/Down 移动菜单焦点，Right 进入子菜单，Left 返回或切换顶层菜单；Escape 或外部点击关闭。移动焦点不会执行动作。系统级快捷键仍通过应用 onKeyEvent 注册。

## 设置状态与验证

组件绑定到同一份编辑 draft；Apply/OK 从 draft 提交，Cancel 从已保存值恢复 draft。不要让复选框修改 options，随后又从另一份旧 state 覆盖 options。示例中的 State::apply/reset 展示这一流程，组件不猜测业务保存策略。

`desktop_controls` 覆盖表单几何、弹层顺序、层级隔离、范围/步长、菜单状态和尺寸换算；`datatable_selection` 覆盖类型化缓存、排序筛选后的选择、删除和重排；`desktop_controls_probe` 通过真实 Runtime 输入与渲染检查 100%/150%/200% 缩放。OpenGL 路径直接读取 framebuffer 像素，不依赖 PrintWindow。
