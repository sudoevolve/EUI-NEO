#pragma once

#include "core/dsl.h"

namespace components {

/** @brief 复合组件的外部布局属性；只作用于根节点，不改变组件内部的排布。 */
template <typename Derived>
class ComponentLayout {
public:
    /** @brief 在父容器中显式定位；未设置的坐标仍由 Row/Column/Stack 布局决定。 */
    Derived& x(float value) { x_ = value; hasX_ = true; return self(); }
    Derived& y(float value) { y_ = value; hasY_ = true; return self(); }
    Derived& position(float xValue, float yValue) { return x(xValue).y(yValue); }
    /** @brief 设置根节点层级；内部子节点维持组件定义的相对顺序。 */
    Derived& zIndex(int value) { z_ = value; hasZ_ = true; return self(); }
    Derived& margin(float value) { return margin(value, value, value, value); }
    Derived& margin(float horizontal, float vertical) { return margin(horizontal, vertical, horizontal, vertical); }
    Derived& margin(float left, float top, float right, float bottom) {
        margin_ = {left, top, right, bottom}; hasMargin_ = true; return self();
    }
    /** @brief 参与父 Row/Column 的剩余空间分配。 */
    Derived& flexGrow(float value = 1.0f) { grow_ = std::max(0.0f, value); return self(); }
    Derived& flexShrink(float value = 1.0f) { shrink_ = std::max(0.0f, value); return self(); }
    Derived& ignoreLayout(bool value = true) { ignoreLayout_ = value; return self(); }

protected:
    template <typename Builder>
    Builder layoutRoot(Builder builder) const {
        if (hasX_) builder.x(x_);
        if (hasY_) builder.y(y_);
        if (hasZ_) builder.zIndex(z_);
        if (hasMargin_) builder.margin(margin_.left, margin_.top, margin_.right, margin_.bottom);
        if (grow_ >= 0) builder.flexGrow(grow_);
        if (shrink_ >= 0) builder.flexShrink(shrink_);
        if (ignoreLayout_) builder.ignoreLayout();
        return builder;
    }

private:
    Derived& self() { return static_cast<Derived&>(*this); }
    float x_ = 0, y_ = 0, grow_ = -1, shrink_ = -1;
    int z_ = 0;
    core::EdgeInsets margin_{};
    bool hasX_ = false, hasY_ = false, hasZ_ = false, hasMargin_ = false, ignoreLayout_ = false;
};

/** @brief 常见浮层的默认层级；自定义层级仍可通过 zIndex 覆盖。 */
namespace layer {
inline constexpr int Content = 0;
inline constexpr int Popup = 100;
inline constexpr int Dialog = 1000;
inline constexpr int Menu = 1050;
}

} // namespace components
