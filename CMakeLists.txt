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
    TLS_VERIFY OFF # 解决 SSL 证书错误
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
    TLS_VERIFY OFF # 解决 SSL 证书错误
)
FetchContent_MakeAvailable(glad)

FetchContent_Declare(
    stb
    URL https://github.com/nothings/stb/archive/refs/heads/master.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    TIMEOUT 30
    TLS_VERIFY OFF
)
FetchContent_MakeAvailable(stb)

# 自动检测并查找系统已安装的 OpenGL
find_package(OpenGL REQUIRED)
find_package(CURL QUIET)

# 包含 src 目录和第三方库目录
include_directories(src)
include_directories(third_party)

# 自动收集 src 下的所有 cpp 文件 (包含所有组件如 InputBox, ComboBox, Panel 等)
file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS "src/*.cpp")
file(GLOB APP_ENTRY_FILES CONFIGURE_DEPENDS "app/*.cpp")

add_library(eui_neo_core STATIC ${SRC_FILES})
target_include_directories(eui_neo_core PUBLIC src third_party ${stb_SOURCE_DIR})
target_link_libraries(eui_neo_core PUBLIC glfw glad OpenGL::GL)
if(CURL_FOUND)
    target_link_libraries(eui_neo_core PUBLIC CURL::libcurl)
    target_compile_definitions(eui_neo_core PUBLIC EUI_HAS_CURL=1)
elseif(WIN32)
    target_link_libraries(eui_neo_core PUBLIC urlmon)
endif()

set(EUI_FONT_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/font")
if(EXISTS "${EUI_FONT_SOURCE_DIR}")
    add_custom_target(eui_copy_fonts
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/font"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${EUI_FONT_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/font"
    )
endif()

function(configure_eui_target target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /GS- /sdl- /utf-8)
        target_link_options(${target_name} PRIVATE /OPT:REF /OPT:ICF /ENTRY:mainCRTStartup /SUBSYSTEM:WINDOWS)
    else()
        target_compile_options(${target_name} PRIVATE -Os -fno-exceptions -fno-rtti)
        target_link_options(${target_name} PRIVATE -Wl,--gc-sections -s)
    endif()

    target_link_libraries(${target_name} PRIVATE eui_neo_core)
    if(TARGET eui_copy_fonts)
        add_dependencies(${target_name} eui_copy_fonts)
    endif()

    if(WIN32)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_RUNTIME_DLLS:${target_name}>
            $<TARGET_FILE_DIR:${target_name}>
            COMMAND_EXPAND_LISTS
        )
    endif()
endfunction()

add_executable(eui_neo main.cpp)
configure_eui_target(eui_neo)

foreach(app_source IN LISTS APP_ENTRY_FILES)
    get_filename_component(app_name "${app_source}" NAME_WE)
    string(MAKE_C_IDENTIFIER "${app_name}" app_id)
    set(app_target "app_${app_id}")
    add_executable(${app_target} "${app_source}")
    configure_eui_target(${app_target})
endforeach()
