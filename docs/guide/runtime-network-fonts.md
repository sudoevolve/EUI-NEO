# 运行时、网络与字体

## DslAppConfig

```cpp
EUINEO::DslAppConfig config;
config.title = "EUI Demo";
config.width = 800;
config.height = 600;
config.pageId = "demo";
config.fps = 120;
```

- `fps <= 0`：使用 VSync
- `fps > 0`：关闭 VSync 并按目标帧率限帧

## 运行时 helper

- `UseDslLightTheme(background)`
- `UseDslDarkTheme(background)`
- `SetDslBackground(color)`
- `OpenDslUrl(url)`

## API 文本

```cpp
EUINEO::DslApiTextOptions options;
options.fallback = "加载中...";
options.refreshSeconds = 60.0f;
options.trim = true;

const std::string text = EUINEO::UseDslApiText(
    "demo.hitokoto",
    "https://v1.hitokoto.cn/?c=f&encode=text",
    options
);
```

## 图片来源

- 本地路径
- HTTP/HTTPS URL
- `bing://daily?...`

## 图片加载示例

```cpp
ui.image("avatar.local")
    .position(24.0f, 120.0f)
    .size(120.0f, 120.0f)
    .path("assets/avatar.png")
    .rounding(60.0f)
    .build();

ui.image("banner.remote")
    .position(160.0f, 120.0f)
    .size(320.0f, 180.0f)
    .url("https://picsum.photos/640/360")
    .build();

ui.image("daily.bing")
    .position(496.0f, 120.0f)
    .size(320.0f, 180.0f)
    .bingDaily(0, "zh-CN")
    .build();
```

说明：

- 图片下载和解码为异步流程
- 成功后会触发刷新，无需手动切页
- 网络失败时可先放本地占位图，保证首屏稳定

## 字体

默认字体目录：

- `src/font/`
- `font/`

常用配置入口：`main.cpp`（UI 字体、图标字体、SDF 加载尺寸）。
