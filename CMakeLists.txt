cmake_minimum_required(VERSION 3.14)
project(EUI-NEO VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 禁用 HTTPS 证书验证，解决国内或代理环境下下载 Github 源码失败的问题
set(CMAKE_TLS_VERIFY OFF)

# 禁用 GLFW 测试、示例和文档以加快构建速度
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build the GLFW example programs" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "Build the GLFW test programs" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "Build the GLFW documentation" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "Generate installation target" FORCE)

# 使用 FetchContent 从 GitHub 源码库下载，避免直接下载 Releases 可能出现的跳转/损坏问题
include(FetchContent)
FetchContent_Declare(
    glfw
    URL https://github.com/glfw/glfw/archive/refs/tags/3.4.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    TIMEOUT 30 # 设置30秒超时
)

# 强制 GLFW 构建为动态库 (DLL) 以节省内存
set(BUILD_SHARED_LIBS ON CACHE BOOL "Build shared libraries" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build the GLFW example programs" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "Build the GLFW test programs" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "Build the GLFW documentation" FORCE)

FetchContent_MakeAvailable(glfw)

# GLAD 使用静态库编译，因为动态库在 MSVC 下如果不加 dllexport 会不生成 .lib
# 这也是你之前遇到 LNK1181 无法打开 glad.lib 的原因
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)

# 获取预编译的 GLAD（用于访问可编程管线和现代 OpenGL 函数）
FetchContent_Declare(
    glad
    URL https://github.com/libigl/libigl-glad/archive/refs/heads/master.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    TIMEOUT 30 # 设置30秒超时
)
FetchContent_MakeAvailable(glad)

# 自动检测并查找系统已安装的 OpenGL
find_package(OpenGL REQUIRED)

# 包含 src 目录和第三方库目录
include_directories(src)
include_directories(third_party)

# 自动收集 src 下的所有 cpp 文件 (包含所有组件如 InputBox, ComboBox 等)
file(GLOB_RECURSE SRC_FILES "src/*.cpp")

# 添加可执行文件 (加入 EUINEO 和各个组件、页面)
add_executable(eui_neo main.cpp ${SRC_FILES})

# 压缩可执行文件大小和内存优化标志（针对 MSVC/GCC 等）
if(MSVC)
    target_compile_options(eui_neo PRIVATE /O1 /GS- /sdl- /utf-8)
    target_link_options(eui_neo PRIVATE /OPT:REF /OPT:ICF /ENTRY:mainCRTStartup /SUBSYSTEM:WINDOWS)
else()
    target_compile_options(eui_neo PRIVATE -Os -fno-exceptions -fno-rtti)
    target_link_options(eui_neo PRIVATE -Wl,--gc-sections -s)
endif()

# 链接依赖库
target_link_libraries(eui_neo PRIVATE glfw glad OpenGL::GL)

# 由于我们将 GLFW 编译为了动态库 (DLL)，在 Windows 下需要将其拷贝到可执行文件所在目录
if(WIN32)
    add_custom_command(TARGET eui_neo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    $<TARGET_FILE:glfw>
    $<TARGET_FILE_DIR:eui_neo>
)
endif()
