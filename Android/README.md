# EUI-NEO Android

`Android/` 是 EUI-NEO 的 APK 打包工程。它不替换主项目源码，只负责把 EUI-NEO、SDL2、Vulkan 后端和 Android gallery 示例打进 APK。

## 编译条件

- Windows 10/11
- Android Studio，已安装 bundled JBR
- Android SDK Platform 36
- Android SDK Build-Tools，跟 Android Studio 默认安装即可
- Android NDK `27.0.12077973`
- CMake `3.22.1` 或 Android Studio 自带 CMake
- 一台支持 Vulkan 的 Android 设备，最低 Android API 26
- 首次构建需要联网下载 SDL2，或者手动提供 SDL2 源码目录

当前 Android 构建固定使用：

- Window backend: SDL2
- Render backend: Vulkan
- ABI: `arm64-v8a`
- SDL2: `2.32.10`
- Gradle project: `Android/`
- App source: `Android/app/src/main/cpp/android_app.cpp`

## 快速开始

在仓库根目录执行：

```powershell
.\Android\scripts\build-apk.ps1
```

输出 debug APK：

```text
Android/app/build/outputs/apk/debug/app-debug.apk
```

安装到已连接手机：

```powershell
$adb = Join-Path $env:LOCALAPPDATA 'Android\Sdk\platform-tools\adb.exe'
& $adb install -r Android\app\build\outputs\apk\debug\app-debug.apk
```

启动：

```powershell
& $adb shell am start -n com.sudoevolve.euineo/.MainActivity
```

## Release APK

生成可安装 release APK：

```powershell
.\Android\scripts\build-apk.ps1 -Task ":app:assembleRelease"
```

输出路径：

```text
Android/app/build/outputs/apk/release/app-release.apk
```

当前 `release` 使用 Android debug keystore 签名，方便本地安装测试。正式发布前请在 `Android/app/build.gradle` 里替换成自己的 release keystore。

## GitHub Actions

仓库包含一个单独的 Android APK workflow：

```text
.github/workflows/android-apk.yml
```

你可以在 GitHub 页面手动运行：

1. 打开仓库的 `Actions`。
2. 选择 `android apk`。
3. 点击 `Run workflow`。

这个 workflow 会构建两个 APK artifact：

```text
eui-neo-debug-apk
eui-neo-release-apk
```

它会在 GitHub runner 上安装 Android SDK Platform 36、Build-Tools 36.0.0、NDK `27.0.12077973` 和 CMake `3.22.1`，然后分别运行：

```bash
Android/gradlew -p Android :app:assembleDebug
Android/gradlew -p Android :app:assembleRelease
```

## Android Studio

1. 打开 Android Studio。
2. 选择 `Open`。
3. 打开仓库里的 `Android/` 目录，不是仓库根目录。
4. 等 Gradle 同步完成。
5. 选择 `app` 配置运行。

如果 IDE 里提示 NDK/CMake 缺失，在 `SDK Manager` 里安装：

- NDK `27.0.12077973`
- CMake `3.22.1`
- Android SDK Platform 36

## SDL2 配置

默认构建会把 SDL2 `2.32.10` 下载到：

```text
Android/.deps/SDL2-2.32.10
```

如果你已经有 SDL2 源码，可以传入目录：

```powershell
.\Android\scripts\build-apk.ps1 -Sdl2Dir C:\deps\SDL2-2.32.10
```

也可以设置环境变量：

```powershell
$env:SDL2_DIR="C:\deps\SDL2-2.32.10"
.\Android\scripts\build-apk.ps1
```

SDL2 目录必须包含 `CMakeLists.txt`。

## 资源

Gradle 会把仓库根目录的 `assets/` 复制到 APK assets：

```text
Android/app/build/generated/euiAssets/assets
```

启动时 `MainActivity` 会把 APK assets 复制到应用内部文件目录，所以 EUI 代码里的路径可以继续使用：

```text
assets/icon.png
assets/Font Awesome 7 Free-Solid-900.otf
assets/mona-loading-default.gif
```

Launcher 图标也来自仓库根目录：

```text
assets/icon.png
```

Gradle 会在构建时把它复制成 generated Android resource：

```text
Android/app/build/generated/euiLauncherIcon/drawable/app_icon.png
```

Manifest 使用：

```xml
android:icon="@drawable/app_icon"
android:roundIcon="@drawable/app_icon"
```

## 网络

Android APK 已声明：

```xml
<uses-permission android:name="android.permission.INTERNET" />
```

Android 端网络下载走 `MainActivity` 里的 `HttpURLConnection` 桥接，C++ 的 `core::network::downloadUrlToString` 和 `downloadUrlToFile` 会调用这个桥接。Bing 文本和 Bing 图片都依赖这条链路。

## 常用命令

清理并重新构建 debug：

```powershell
.\Android\gradlew.bat -p .\Android clean :app:assembleDebug
```

构建 release：

```powershell
.\Android\scripts\build-apk.ps1 -Task ":app:assembleRelease"
```

查看连接设备：

```powershell
$adb = Join-Path $env:LOCALAPPDATA 'Android\Sdk\platform-tools\adb.exe'
& $adb devices
```

抓日志：

```powershell
& $adb logcat -d -t 500
```

截图：

```powershell
& $adb shell screencap -p /sdcard/eui.png
& $adb pull /sdcard/eui.png Android\screen.png
```

## 常见问题

`SDL2 was not found`

SDL2 没下载成功，或者 `SDL2_DIR` 指向的不是 SDL2 源码目录。删除 `Android/.deps` 后重试，或手动传 `-Sdl2Dir`。

`Vulkan SDK was not found`

确认 Android SDK/NDK 安装完整。Android NDK 自带 Vulkan headers/library，通常重新同步 Gradle 即可。

APK 打开黑屏

先确认设备支持 Vulkan。当前 Android 分支已经处理 SDL2 Android 生命周期，切后台再回来会重建 Vulkan surface。

输入框不弹输入法

Android 端通过 SDL2 text input 启动系统输入法。若输入法没有弹出，先点输入框本体，不要点外层滚动区域。

Bing 图片或文本不显示

确认手机网络可访问 `www.bing.com`。如果设备或网络环境拦截 Bing，组件会停留在 loading 或显示失败文本。
