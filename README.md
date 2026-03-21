# EUI-NEO

EUI-NEO 是一个极简、现代、高性能的 OpenGL 2D GUI 框架。该项目致力于在最低的内存和 CPU 占用下，实现美观的现代用户界面。

![EUI-NEO Preview](1.jpg)

## 🌟 项目特色

- **极致轻量**：极少的内存占用，动态链接 GLFW，禁用 MSAA，大幅降低显存开销。
- **现代化渲染**：基于 SDF（符号距离场）实现高质量的圆角矩形渲染，支持真正的抗锯齿边缘。
- **SDF 文本渲染**：使用 `stb_truetype` 加载 TrueType 字体，利用正交投影实现精准、清晰的文本绘制。
- **智能刷新（脏区渲染）**：只在有交互或动效时重绘（Dirty-region / Smart Repaint），平时保持静默休眠，大幅节省 CPU/GPU 占用。
- **平滑动效**：内置简单的 Lerp 线性插值函数支持悬浮、点击、拖拽等动效反馈。
- **极简布局**：易用的九宫格锚点（Anchor）布局系统。
- **组件化架构**：核心渲染与业务逻辑分离，所有 UI 控件独立封装（位于 `components` 目录），通过 `pages` 进行页面级管理。
- **丰富的输入控件**：内置 `Button`, `Slider`, `ProgressBar`, `SegmentedControl`, `InputBox` (输入框) 和 `ComboBox` (下拉框) 等现代化交互组件。
- **字体图标支持**：支持加载 Font Awesome 等图标字体，并内置 Unicode UTF-8 解码与渲染，可通过指定 `fontSize` 进行任意大小无损缩放。

---

## 🎨 使用自绘图元制作组件

EUI-NEO 抛弃了传统的贴图 UI，而是通过纯 GPU Shader（片段着色器）来计算形状。这样不仅节省了大量纹理内存，还可以随时动态改变形状的大小和圆角。

### 核心图元 API

```cpp
// 绘制带有圆角的矩形
Renderer::DrawRect(float x, float y, float w, float h, const Color& color, float rounding = 0.0f);

// 绘制文本
Renderer::DrawTextStr(const std::string& text, float x, float y, const Color& color, float scale = 1.0f);
```

### 制作自定义组件

要制作一个自己的组件，你只需要继承 `Widget` 基类，并重写 `Update`（处理逻辑与动效）和 `Draw`（调用图元 API 渲染）方法：

```cpp
#include "EUINEO.h"

namespace EUINEO {

class MyCustomCard : public Widget {
public:
    MyCustomCard() = default; // 推荐提供默认构造函数
    
    MyCustomCard(float x, float y, float w, float h) {
        this->x = x; this->y = y; 
        this->width = w; this->height = h;
    }

    void Update() override {
        // 1. 处理鼠标交互 (如悬停)
        if (IsHovered()) {
            // 可以通过 RequestRepaint 触发连续动效重绘
            Renderer::RequestRepaint(0.1f);
        }
    }

    void Draw() override {
        // 1. 获取根据锚点计算后的绝对屏幕坐标
        float absX, absY;
        GetAbsoluteBounds(absX, absY);

        // 2. 组合使用自绘图元
        float cornerRadius = 12.0f;
        
        // 绘制卡片底色背景
        Renderer::DrawRect(absX, absY, width, height, CurrentTheme->surface, cornerRadius);
        
        // 绘制卡片内的一个小点缀方块
        Renderer::DrawRect(absX + 10, absY + 10, 20, 20, CurrentTheme->primary, 4.0f);
        
        // 绘制文本 (利用父类自带的 fontSize 进行缩放计算)
        float textScale = fontSize / 24.0f;
        Renderer::DrawTextStr("Hello Card", absX + 40, absY + 25, CurrentTheme->text, textScale);
    }
};

} // namespace EUINEO
```

通过组合简单的 `DrawRect` 和 `DrawTextStr`，你可以构建出 `Slider`（轨道加滑块）、`ProgressBar`（两层重叠矩形）、`SegmentedControl` 等各种复杂的现代化 UI 组件。

---

## 📐 布局系统

EUI-NEO 提供了一个非常轻量但实用的**锚点（Anchor）布局系统**。

每个 `Widget` 都有一个 `anchor` 属性，它决定了组件的 `x` 和 `y` 坐标是相对于屏幕的哪个位置进行偏移的。

### 可用锚点

- `Anchor::TopLeft`（默认）：相对屏幕左上角
- `Anchor::TopCenter`：相对屏幕顶部水平居中
- `Anchor::TopRight`：相对屏幕右上角
- `Anchor::CenterLeft`：相对屏幕左侧垂直居中
- `Anchor::Center`：完全居中
- `Anchor::CenterRight`：相对屏幕右侧垂直居中
- `Anchor::BottomLeft`：相对屏幕左下角
- `Anchor::BottomCenter`：相对屏幕底部水平居中
- `Anchor::BottomRight`：相对屏幕右下角

### 布局使用示例

```cpp
EUINEO::Button btn("Start", 0, 50, 120, 40);

// 将按钮水平居中，距离屏幕顶部 50 像素
btn.anchor = EUINEO::Anchor::TopCenter;

// 在渲染或交互时，组件内部会自动调用：
// float absX, absY;
// GetAbsoluteBounds(absX, absY);
// 这会将锚点加上你设置的相对坐标 (0, 50)，转换为真正的屏幕渲染坐标。
```

当窗口调整大小时，只要你的组件是在每一帧实时调用 `GetAbsoluteBounds`，所有使用非 `TopLeft` 锚点的组件都会自动跟随窗口尺寸变化而重新排版对齐。

---

## 📝 声明式组件初始化

为了提供极佳的开发者体验，EUI-NEO 的所有组件都提供了默认构造函数，允许你在页面的构造函数中以类似声明式（高内聚）的方式配置组件的全部属性、样式和事件，无需在头文件和源文件之间反复横跳：

```cpp
MainPage::MainPage() {
    // 按钮配置：创建、定位、样式、字体大小和事件一次性写完
    btnPrimary = Button("Primary", -70, 70, 120, 40);
    btnPrimary.anchor = Anchor::TopCenter;
    btnPrimary.style = ButtonStyle::Primary;
    btnPrimary.fontSize = 20.0f;
    btnPrimary.onClick = [this]() {
        progBar.value += 0.1f;
        CurrentTheme = (CurrentTheme == &DarkTheme) ? &LightTheme : &DarkTheme;
    };
    
    // 输入框配置
    inputBox = InputBox("Type something...", 0, 100, 300, 35);
    inputBox.anchor = Anchor::Center;
    inputBox.fontSize = 20.0f;
}
```

---

## 📝 字体与图标

EUI-NEO 提供了强大的字体和图标支持，并且非常易于调整。

### 加载字体

你可以在初始化后加载 `.ttf` 或 `.otf` 格式的字体，并指定字体的 Unicode 区间来加载各种语言或图标集：

```cpp
// 加载常规英文字体
EUINEO::Renderer::LoadFont("src/font/Medicinal Compound.otf", 24.0f, 32, 128);

// 加载 Font Awesome 图标字体 (PUA 私有使用区)
EUINEO::Renderer::LoadFont("src/font/Font Awesome 7 Free-Solid-900.otf", 24.0f, 0xE000, 0xF900);
```

### 调整字体大小

在 EUI-NEO 中，所有继承自 `Widget` 的组件都内置了 `fontSize` 属性（默认基础大小为 24.0f）。你只需要简单地修改该属性，框架就会在渲染时自动进行缩放，无需重新生成字体纹理图集：

```cpp
EUINEO::Label titleLabel("EUI-NEO", 0, 20);
titleLabel.fontSize = 32.0f; // 直接放大标题字体

EUINEO::Button btnIcon("Icon \xEF\x80\x93", 0, 130, 120, 40);
btnIcon.fontSize = 20.0f;    // 调整按钮字体和图标的大小
```
