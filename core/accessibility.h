#pragma once

#include "core/render/render_types.h"
#include <string>

namespace core {

/** @brief 后端无关的控件语义；系统屏幕阅读器桥接可消费相同信息。 */
enum class AccessibleRole {
    None, Button, CheckBox, Radio, Switch, Slider, TextInput, ComboBox,
    ListItem, Tab, Dialog, Table, ColumnHeader, Cell
};

/** @brief 当前 UI 的语义快照，不持有组件或回调引用。bounds 为布局逻辑坐标，未叠加动画与滚动变换。 */
struct AccessibleNode {
    std::string id;
    std::string parentId;
    AccessibleRole role = AccessibleRole::None;
    std::string name;
    std::string value;
    Rect bounds;
    bool checked = false;
    bool expanded = false;
    bool disabled = false;
    bool focused = false;
};

} // namespace core
