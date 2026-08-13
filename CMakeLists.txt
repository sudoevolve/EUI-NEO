cmake_minimum_required(VERSION 3.14)
project(EUI-NEO VERSION 0.5.6)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# -----------------------------------------------------------------------------
# Options
# -----------------------------------------------------------------------------

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(EUI_NEO_TOP_LEVEL_DEFAULT ON)
else()
    set(EUI_NEO_TOP_LEVEL_DEFAULT OFF)
endif()
option(EUI_BUILD_APPS "Build EUI-NEO bundled example applications" ${EUI_NEO_TOP_LEVEL_DEFAULT})
option(EUI_BUILD_USER_APPS "Build user applications from apps/." ${EUI_NEO_TOP_LEVEL_DEFAULT})
option(EUI_BUILD_TEST_FIXTURES "Build internal test fixture applications." OFF)
option(EUI_BUILD_SHARED "Build eui_neo as a shared library instead of a static library." OFF)
option(EUI_ENABLE_INSTALL "Install EUI-NEO library, headers, and CMake package files." ${EUI_NEO_TOP_LEVEL_DEFAULT})
option(EUI_ENABLE_MODULES "Build optional EUI-NEO modules when their directories are present." ON)
option(EUI_VULKAN_LOW_LATENCY_PRESENT "Prefer low-latency Vulkan presentation when available." OFF)

set(EUI_DEFAULT_WINDOW_BACKEND "glfw")
set(EUI_DEFAULT_RENDER_BACKEND "opengl")
get_filename_component(EUI_BUILD_DIR_NAME "${CMAKE_BINARY_DIR}" NAME)
string(TOLOWER "${EUI_BUILD_DIR_NAME}" EUI_BUILD_DIR_NAME_LOWER)
if(EUI_BUILD_DIR_NAME_LOWER MATCHES "(^|[-_])sdl2($|[-_])")
    set(EUI_DEFAULT_WINDOW_BACKEND "sdl2")
endif()
if(EUI_BUILD_DIR_NAME_LOWER MATCHES "(^|[-_])(vk|vulkan)($|[-_])")
    set(EUI_DEFAULT_RENDER_BACKEND "vulkan")
endif()

set(EUI_WINDOW_BACKEND "${EUI_DEFAULT_WINDOW_BACKEND}" CACHE STRING "Window backend: glfw or sdl2")
set_property(CACHE EUI_WINDOW_BACKEND PROPERTY STRINGS glfw sdl2)
set(EUI_RENDER_BACKEND "${EUI_DEFAULT_RENDER_BACKEND}" CACHE STRING "Render backend: auto, opengl, or vulkan")
set_property(CACHE EUI_RENDER_BACKEND PROPERTY STRINGS auto opengl vulkan)
if(NOT EUI_WINDOW_BACKEND MATCHES "^(glfw|sdl2)$")
    message(FATAL_ERROR "EUI_WINDOW_BACKEND must be one of: glfw, sdl2")
endif()
if(NOT EUI_RENDER_BACKEND MATCHES "^(auto|opengl|vulkan)$")
    message(FATAL_ERROR "EUI_RENDER_BACKEND must be one of: auto, opengl, vulkan")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED TRUE)

# windres uses a shell pipe for preprocessing by default. On Windows that can
# intermittently lose quoting when the MinGW toolchain path contains spaces.
if(MINGW AND CMAKE_RC_COMPILER MATCHES "windres(\\.exe)?$")
    if(NOT CMAKE_RC_FLAGS MATCHES "(^|[ \\t])--use-temp-file([ \\t]|$)")
        string(APPEND CMAKE_RC_FLAGS " --use-temp-file")
    endif()
endif()

set(EUI_MINGW_STATIC_LINK_OPTIONS -static)
if(MINGW AND NOT EUI_BUILD_SHARED)
    include(CheckCXXSourceCompiles)
    set(EUI_MINGW_STATIC_LINK_PROBE [=[
        #include <cstdio>
        #include <thread>
        int main() {
            char buffer[8]{};
            std::snprintf(buffer, sizeof(buffer), "%s", "ok");
            std::thread worker([] {});
            worker.join();
            return buffer[0] == 'o' ? 0 : 1;
        }
    ]=])
    set(EUI_SAVED_REQUIRED_LINK_OPTIONS "${CMAKE_REQUIRED_LINK_OPTIONS}")
    set(EUI_SAVED_REQUIRED_QUIET "${CMAKE_REQUIRED_QUIET}")
    set(CMAKE_REQUIRED_QUIET TRUE)
    set(CMAKE_REQUIRED_LINK_OPTIONS -static)
    check_cxx_source_compiles("${EUI_MINGW_STATIC_LINK_PROBE}"
        EUI_MINGW_DEFAULT_STATIC_RUNTIME_LINKS)
    if(NOT EUI_MINGW_DEFAULT_STATIC_RUNTIME_LINKS)
        set(CMAKE_REQUIRED_LINK_OPTIONS -static -mcrtdll=ucrt)
        check_cxx_source_compiles("${EUI_MINGW_STATIC_LINK_PROBE}"
            EUI_MINGW_UCRT_STATIC_RUNTIME_LINKS)
        if(EUI_MINGW_UCRT_STATIC_RUNTIME_LINKS)
            list(APPEND EUI_MINGW_STATIC_LINK_OPTIONS -mcrtdll=ucrt)
            message(STATUS "MinGW static runtime requires UCRT link compatibility")
        else()
            message(FATAL_ERROR
                "The selected MinGW toolchain cannot link its static runtime. "
                "Both the default CRT and UCRT compatibility mode failed.")
        endif()
    endif()
    set(CMAKE_REQUIRED_LINK_OPTIONS "${EUI_SAVED_REQUIRED_LINK_OPTIONS}")
    set(CMAKE_REQUIRED_QUIET "${EUI_SAVED_REQUIRED_QUIET}")
endif()

if(MINGW AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 12)
    message(FATAL_ERROR
        "EUI-NEO requires MinGW-w64 GCC 12 or newer; "
        "detected GCC ${CMAKE_CXX_COMPILER_VERSION}. "
        "Use a current MinGW-w64 distribution instead of legacy TDM-GCC.")
endif()

if(APPLE)
    enable_language(OBJC)
    set_source_files_properties(core/platform/native_bridge.c PROPERTIES LANGUAGE OBJC)
    set_source_files_properties(core/platform/tray_bridge.c PROPERTIES LANGUAGE OBJC)
    set_source_files_properties(core/platform/ime_bridge.c PROPERTIES LANGUAGE OBJC)
endif()

function(eui_apply_compile_options target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE
            /utf-8
            $<$<NOT:$<CONFIG:Debug>>:/O1>
            $<$<NOT:$<CONFIG:Debug>>:/GS->
            $<$<NOT:$<CONFIG:Debug>>:/sdl->
            $<$<NOT:$<CONFIG:Debug>>:/wd4819>
        )
    elseif(APPLE)
        target_compile_options(${target_name} PRIVATE
            $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<NOT:$<CONFIG:Debug>>>:-Os>
            $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<NOT:$<CONFIG:Debug>>>:-fno-exceptions>
            $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<NOT:$<CONFIG:Debug>>>:-fno-rtti>
        )
    else()
        target_compile_options(${target_name} PRIVATE
            $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<NOT:$<CONFIG:Debug>>>:-Os>
            $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<NOT:$<CONFIG:Debug>>>:-fno-exceptions>
            $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<NOT:$<CONFIG:Debug>>>:-fno-rtti>
        )
    endif()
endfunction()

function(eui_apply_app_link_options target_name)
    if(MSVC)
        set_target_properties(${target_name} PROPERTIES VS_GLOBAL_VcpkgXUseBuiltInApplocalDeps true)
        target_link_options(${target_name} PRIVATE
            $<$<NOT:$<CONFIG:Debug>>:/OPT:REF>
            $<$<NOT:$<CONFIG:Debug>>:/OPT:ICF>
            $<$<NOT:$<CONFIG:Debug>>:/INCREMENTAL:NO>
            /ENTRY:mainCRTStartup
            /SUBSYSTEM:WINDOWS
        )
    elseif(MINGW)
        target_link_options(${target_name} PRIVATE
            $<$<NOT:$<CONFIG:Debug>>:-Wl,--gc-sections>
            $<$<NOT:$<CONFIG:Debug>>:-s>
            -mwindows
        )
        if(NOT EUI_BUILD_SHARED)
            target_link_options(${target_name} PRIVATE ${EUI_MINGW_STATIC_LINK_OPTIONS})
        endif()
    elseif(APPLE)
        target_link_options(${target_name} PRIVATE $<$<NOT:$<CONFIG:Debug>>:-Wl,-dead_strip>)
    else()
        target_link_options(${target_name} PRIVATE
            $<$<NOT:$<CONFIG:Debug>>:-Wl,--gc-sections>
            $<$<NOT:$<CONFIG:Debug>>:-s>
        )
    endif()
endfunction()

# -----------------------------------------------------------------------------
# Dependencies and backend selection
# -----------------------------------------------------------------------------

set(EUI_VULKAN_BACKEND_SOURCES
    core/render/vulkan/vulkan_backend.cpp
    core/render/vulkan/vulkan_cache.cpp
    core/render/vulkan/vulkan_primitives.cpp
    core/render/vulkan/vulkan_polygon.cpp
    core/render/vulkan/vulkan_shadertoy.cpp
    core/render/vulkan/vulkan_text.cpp
    core/render/vulkan/vulkan_image.cpp
)
if(EUI_RENDER_BACKEND STREQUAL "auto" OR EUI_RENDER_BACKEND STREQUAL "vulkan")
    find_package(Vulkan QUIET)
endif()

set(EUI_RESOLVED_RENDER_BACKEND "${EUI_RENDER_BACKEND}")
if(EUI_RENDER_BACKEND STREQUAL "auto")
    if(Vulkan_FOUND)
        set(EUI_RESOLVED_RENDER_BACKEND "vulkan")
    else()
        set(EUI_RESOLVED_RENDER_BACKEND "opengl")
        message(STATUS "Vulkan SDK not found; falling back to OpenGL.")
    endif()
elseif(EUI_RENDER_BACKEND STREQUAL "vulkan")
    if(NOT Vulkan_FOUND)
        message(FATAL_ERROR "EUI_RENDER_BACKEND=vulkan requested, but Vulkan SDK was not found.")
    endif()
endif()
message(STATUS "EUI render backend: requested=${EUI_RENDER_BACKEND}, resolved=${EUI_RESOLVED_RENDER_BACKEND}")

set(EUI_NEO_LIBRARY_TYPE STATIC)
if(EUI_BUILD_SHARED)
    set(EUI_NEO_LIBRARY_TYPE SHARED)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/3rd/dependencies.cmake")

find_package(Python3 COMPONENTS Interpreter QUIET)

# -----------------------------------------------------------------------------
# Core library sources
# -----------------------------------------------------------------------------

set(CORE_SOURCES
    core/platform/async.cpp
    core/platform/json.cpp
    core/platform/network.cpp
    core/platform/native_bridge.c
    core/platform/performance_stats.cpp
    core/platform/platform.cpp
    core/render/image.cpp
    core/render/image_facade.cpp
    core/render/image_source.cpp
    core/render/primitive.cpp
    core/render/render_backend.cpp
    core/render/shadertoy.cpp
    core/render/shadertoy_json.cpp
    core/render/shadertoy_primitive.cpp
    core/render/stb_image_impl.cpp
    core/render/text.cpp
    core/platform/tray_bridge.c
    core/window/window_backend.cpp
    core/window/window_input_backend.cpp
)

if(EUI_RESOLVED_RENDER_BACKEND STREQUAL "opengl")
    list(APPEND CORE_SOURCES
        core/render/opengl/opengl_backend.cpp
        core/render/opengl/opengl_image.cpp
        core/render/opengl/opengl_primitives.cpp
        core/render/opengl/opengl_shadertoy.cpp
        core/render/opengl/opengl_text.cpp
    )
elseif(EUI_RESOLVED_RENDER_BACKEND STREQUAL "vulkan")
    list(APPEND CORE_SOURCES ${EUI_VULKAN_BACKEND_SOURCES})
endif()

if(EUI_WINDOW_BACKEND STREQUAL "glfw")
    list(APPEND CORE_SOURCES core/platform/ime_bridge.c)
endif()

# -----------------------------------------------------------------------------
# Core library target
# -----------------------------------------------------------------------------

add_library(eui_neo ${EUI_NEO_LIBRARY_TYPE} ${CORE_SOURCES})
add_library(eui::neo ALIAS eui_neo)
set_target_properties(eui_neo PROPERTIES EXPORT_NAME neo)
if(EUI_BUILD_SHARED AND WIN32)
    set_target_properties(eui_neo PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()
target_include_directories(eui_neo
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${TRAY_INCLUDE_DIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/eui-neo>
)
target_include_directories(eui_neo PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/3rd/yyjson-0.12.0/src")
target_link_libraries(eui_neo PUBLIC Threads::Threads)
target_compile_features(eui_neo PUBLIC cxx_std_17)
target_compile_definitions(eui_neo PRIVATE YYJSON_DISABLE_WRITER=1)
eui_apply_compile_options(eui_neo)

if(EUI_RESOLVED_RENDER_BACKEND STREQUAL "vulkan")
    add_executable(eui_shadertoy_wrap
        scripts/shadertoy_wrap.cpp
        core/render/shadertoy.cpp
    )
    target_include_directories(eui_shadertoy_wrap PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_features(eui_shadertoy_wrap PRIVATE cxx_std_17)
    eui_apply_compile_options(eui_shadertoy_wrap)
    if(MINGW)
        target_link_options(eui_shadertoy_wrap PRIVATE -municode)
    endif()
    set_target_properties(eui_shadertoy_wrap PROPERTIES
        EXPORT_NAME shadertoy_wrap)
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/scripts/EuiShaderToy.cmake")

set(EUI_YYJSON_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/3rd/yyjson-0.12.0/src/yyjson.c")
if(EUI_BUILD_SHARED)
    add_library(eui_yyjson STATIC "${EUI_YYJSON_SOURCE}")
    target_include_directories(eui_yyjson PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/3rd/yyjson-0.12.0/src")
    target_compile_definitions(eui_yyjson PRIVATE YYJSON_DISABLE_WRITER=1)
    set_target_properties(eui_yyjson PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        C_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )
    eui_silence_third_party_warnings(eui_yyjson)
    target_link_libraries(eui_neo PRIVATE eui_yyjson)
else()
    target_sources(eui_neo PRIVATE "${EUI_YYJSON_SOURCE}")
endif()

if(EUI_RESOLVED_RENDER_BACKEND STREQUAL "opengl")
    target_compile_definitions(eui_neo PUBLIC EUI_RENDER_BACKEND_OPENGL=1)
    target_link_libraries(eui_neo PUBLIC glad OpenGL::GL)
elseif(EUI_RESOLVED_RENDER_BACKEND STREQUAL "vulkan")
    target_compile_definitions(eui_neo PUBLIC EUI_RENDER_BACKEND_VULKAN=1)
    if(EUI_VULKAN_LOW_LATENCY_PRESENT)
        target_compile_definitions(eui_neo PRIVATE EUI_VULKAN_LOW_LATENCY_PRESENT=1)
    endif()
    target_link_libraries(eui_neo PUBLIC Vulkan::Vulkan)
endif()

if(EUI_WINDOW_BACKEND STREQUAL "sdl2")
    if(NOT TARGET SDL2::SDL2)
        find_package(SDL2 CONFIG REQUIRED)
    endif()
    target_compile_definitions(eui_neo PUBLIC EUI_WINDOW_BACKEND_SDL2=1)
    target_link_libraries(eui_neo PUBLIC SDL2::SDL2)
    set(EUI_APP_MAIN_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/core/app/sdl2_app_main.cpp")
else()
    target_link_libraries(eui_neo PUBLIC glfw)
    set(EUI_APP_MAIN_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/core/app/glfw_app_main.cpp")
endif()

if(UNIX AND NOT APPLE)
    # window_backend.cpp's dragWindow() speaks _NET_WM_MOVERESIZE directly,
    # which needs libX11. Neither GLFW nor SDL2 forwards X11 to the app.
    find_package(X11 QUIET)
    if(X11_FOUND)
        target_link_libraries(eui_neo PRIVATE X11::X11)
    endif()
endif()

target_link_libraries(eui_neo PUBLIC Freetype::Freetype)

# -----------------------------------------------------------------------------
# Platform capabilities
# -----------------------------------------------------------------------------

if(WIN32)
    target_compile_definitions(eui_neo PUBLIC EUI_TRAY_WINAPI=1 NOMINMAX)
    target_link_libraries(eui_neo PUBLIC winmm urlmon shell32 user32 imm32 pdh)
elseif(APPLE)
    target_compile_definitions(eui_neo PUBLIC EUI_TRAY_APPKIT=1)
    target_link_libraries(eui_neo PUBLIC "-framework Cocoa" objc)
elseif(UNIX AND EUI_APPINDICATOR_FOUND AND EUI_GTK3_FOUND)
    target_compile_definitions(eui_neo PUBLIC EUI_TRAY_APPINDICATOR=1)
    target_include_directories(eui_neo PUBLIC ${EUI_APPINDICATOR_INCLUDE_DIRS} ${EUI_GTK3_INCLUDE_DIRS})
    target_compile_options(eui_neo PRIVATE ${EUI_APPINDICATOR_CFLAGS_OTHER} ${EUI_GTK3_CFLAGS_OTHER})
    target_link_directories(eui_neo PUBLIC ${EUI_APPINDICATOR_LIBRARY_DIRS} ${EUI_GTK3_LIBRARY_DIRS})
    target_link_libraries(eui_neo PUBLIC ${EUI_APPINDICATOR_LIBRARIES} ${EUI_GTK3_LIBRARIES})
endif()

if(TARGET CURL::libcurl)
    target_link_libraries(eui_neo PUBLIC CURL::libcurl)
    target_compile_definitions(eui_neo PUBLIC EUI_HAS_CURL=1)
endif()

if(TARGET md4c::md4c)
    target_link_libraries(eui_neo PUBLIC md4c::md4c)
    target_compile_definitions(eui_neo PUBLIC EUI_HAS_MD4C=1)
endif()

if(EUI_ENABLE_MODULES AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/modules/CMakeLists.txt")
    add_subdirectory(modules)
endif()

# -----------------------------------------------------------------------------
# App target helpers
# -----------------------------------------------------------------------------

set(EUI_APP_ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets")
set(EUI_APP_RUNTIME_DIR "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}")
set(EUI_SHADERTOY_RUNTIME_ASSET_DIR "assets/shaders/shadertoy")

set(APP_ICON_RESOURCE)
if(WIN32)
    set(APP_ICON_ICO "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon.ico")
    set(APP_ICON_RC "${CMAKE_CURRENT_BINARY_DIR}/eui_app_icon.rc")
    file(TO_NATIVE_PATH "${APP_ICON_ICO}" APP_ICON_ICO_NATIVE)
    string(REPLACE "\\" "\\\\" APP_ICON_ICO_ESCAPED "${APP_ICON_ICO_NATIVE}")
    file(WRITE "${APP_ICON_RC}" "IDI_APP_ICON ICON \"${APP_ICON_ICO_ESCAPED}\"\n")
    set(APP_ICON_RESOURCE "${APP_ICON_RC}")
endif()

set(EUI_SHARED_ASSET_TARGET)
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR AND EXISTS "${EUI_APP_ASSETS_DIR}")
    add_custom_target(eui_neo_runtime_assets
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "${EUI_APP_RUNTIME_DIR}/assets"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${EUI_APP_ASSETS_DIR}"
            "${EUI_APP_RUNTIME_DIR}/assets"
        VERBATIM
    )
    set(EUI_SHARED_ASSET_TARGET eui_neo_runtime_assets)
endif()

function(eui_neo_copy_assets target_name)
    if(EUI_SHARED_ASSET_TARGET)
        add_dependencies(${target_name} ${EUI_SHARED_ASSET_TARGET})
    elseif(EXISTS "${EUI_APP_ASSETS_DIR}")
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:${target_name}>/assets"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${EUI_APP_ASSETS_DIR}"
                "$<TARGET_FILE_DIR:${target_name}>/assets"
            VERBATIM
        )
    endif()
endfunction()

function(eui_neo_copy_runtime_dependencies target_name)
    if(EUI_BUILD_SHARED AND EUI_WINDOW_BACKEND STREQUAL "glfw")
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:glfw>
                $<TARGET_FILE_DIR:${target_name}>
        )
    endif()
endfunction()

function(eui_neo_copy_app_assets target_name asset_dir)
    if(EXISTS "${asset_dir}")
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:${target_name}>/assets
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${asset_dir}" $<TARGET_FILE_DIR:${target_name}>/assets
        )
    endif()
endfunction()

function(eui_neo_configure_app target_name)
    target_sources(${target_name} PRIVATE "${EUI_APP_MAIN_SOURCE}" ${APP_ICON_RESOURCE})
    target_link_libraries(${target_name} PRIVATE eui::neo)
    eui_apply_compile_options(${target_name})
    eui_apply_app_link_options(${target_name})
    eui_neo_copy_assets(${target_name})
    eui_neo_copy_runtime_dependencies(${target_name})
endfunction()

# -----------------------------------------------------------------------------
# Bundled apps and internal tests
# -----------------------------------------------------------------------------

if(EUI_BUILD_APPS)
    file(GLOB APP_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/examples/*.cpp")
    foreach(app_source ${APP_SOURCES})
        get_filename_component(app_name "${app_source}" NAME_WE)
        if(app_name STREQUAL "keyboard" AND NOT TARGET eui::module_keyboard)
            message(STATUS "Skipping keyboard example because the keyboard module is not available.")
            continue()
        endif()
        if(app_name STREQUAL "serial_tool" AND NOT TARGET eui::module_serial)
            message(STATUS "Skipping serial_tool example because the serial module is not available.")
            continue()
        endif()
        add_executable(${app_name} "${app_source}")
        eui_neo_configure_app(${app_name})
        if(app_name STREQUAL "shadertoy")
            set(shadertoy_demo_spirv
                "${EUI_APP_RUNTIME_DIR}/${EUI_SHADERTOY_RUNTIME_ASSET_DIR}/demo.frag.spv")
            set(shadertoy_demo_source
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/shadertoy/demo.frag")
            set(shadertoy_presets_dir
                "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders/shadertoy")
            target_compile_definitions(shadertoy PRIVATE
                EUI_SHADERTOY_DEMO_SOURCE="${EUI_SHADERTOY_RUNTIME_ASSET_DIR}/demo.frag"
                EUI_SHADERTOY_DEMO_SPIRV="${EUI_SHADERTOY_RUNTIME_ASSET_DIR}/demo.frag.spv"
                EUI_SHADERTOY_PRESETS_DIR="${EUI_SHADERTOY_RUNTIME_ASSET_DIR}")
            if(EUI_RESOLVED_RENDER_BACKEND STREQUAL "vulkan")
                eui_compile_shadertoy(shadertoy
                    SOURCE "${shadertoy_demo_source}"
                    OUTPUT "${shadertoy_demo_spirv}")
                if(EXISTS "${shadertoy_presets_dir}")
                    file(GLOB preset_configs CONFIGURE_DEPENDS
                         "${shadertoy_presets_dir}/*/config.json")
                    foreach(preset_config IN LISTS preset_configs)
                        eui_compile_shadertoy_config(shadertoy
                            CONFIG "${preset_config}"
                            ASSET_ROOT "${shadertoy_presets_dir}"
                            OUTPUT_ROOT "${EUI_APP_RUNTIME_DIR}/${EUI_SHADERTOY_RUNTIME_ASSET_DIR}")
                    endforeach()
                endif()
            endif()
        endif()
        if(app_name STREQUAL "gallery")
            set(gallery_shadertoy_spirv
                "${EUI_APP_RUNTIME_DIR}/${EUI_SHADERTOY_RUNTIME_ASSET_DIR}/gallery_demo.frag.spv")
            target_compile_definitions(gallery PRIVATE
                EUI_GALLERY_SHADERTOY_SOURCE="${EUI_SHADERTOY_RUNTIME_ASSET_DIR}/demo.frag"
                EUI_GALLERY_SHADERTOY_NOISE="${EUI_SHADERTOY_RUNTIME_ASSET_DIR}/blackhole/color_noise.png"
                EUI_GALLERY_SHADERTOY_SPIRV="${EUI_SHADERTOY_RUNTIME_ASSET_DIR}/gallery_demo.frag.spv")
            if(EUI_RESOLVED_RENDER_BACKEND STREQUAL "vulkan")
                eui_compile_shadertoy(gallery
                    SOURCE "assets/shaders/shadertoy/demo.frag"
                    OUTPUT "${gallery_shadertoy_spirv}")
            endif()
        endif()
        if(app_name STREQUAL "keyboard")
            target_link_libraries(${app_name} PRIVATE eui::module_keyboard)
        elseif(app_name STREQUAL "serial_tool")
            target_link_libraries(${app_name} PRIVATE eui::module_serial)
        endif()
    endforeach()
endif()

if(EUI_BUILD_USER_APPS)
    file(GLOB USER_APP_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/apps/*.cpp")
    foreach(app_source ${USER_APP_SOURCES})
        get_filename_component(app_name "${app_source}" NAME_WE)
        if(TARGET ${app_name})
            message(FATAL_ERROR "Duplicate EUI app target name '${app_name}'. Rename one app source or folder.")
        endif()
        add_executable(${app_name} "${app_source}")
        eui_neo_configure_app(${app_name})
    endforeach()

    file(GLOB USER_APP_ENTRY_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/apps/*/app.cpp")
    foreach(app_source ${USER_APP_ENTRY_SOURCES})
        get_filename_component(app_dir "${app_source}" DIRECTORY)
        get_filename_component(app_name "${app_dir}" NAME)
        if(TARGET ${app_name})
            message(FATAL_ERROR "Duplicate EUI app target name '${app_name}'. Rename one app source or folder.")
        endif()
        file(GLOB_RECURSE app_local_sources CONFIGURE_DEPENDS "${app_dir}/*.cpp")
        list(SORT app_local_sources)
        add_executable(${app_name} ${app_local_sources})
        target_include_directories(${app_name} PRIVATE "${app_dir}")
        eui_neo_configure_app(${app_name})
        eui_neo_copy_app_assets(${app_name} "${app_dir}/assets")
    endforeach()
endif()

if(EUI_BUILD_TEST_FIXTURES)
    enable_testing()

    file(GLOB TEST_FIXTURE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/*.cpp")
    foreach(fixture_source ${TEST_FIXTURE_SOURCES})
        get_filename_component(fixture_name "${fixture_source}" NAME_WE)
        add_executable(${fixture_name} "${fixture_source}")
        eui_neo_configure_app(${fixture_name})
    endforeach()

    file(GLOB TEST_PROBE_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/probes/*.cpp")
    foreach(probe_source ${TEST_PROBE_SOURCES})
        get_filename_component(probe_name "${probe_source}" NAME_WE)
        add_executable(${probe_name} "${probe_source}")
        target_link_libraries(${probe_name} PRIVATE eui::neo)
        eui_apply_compile_options(${probe_name})
        eui_neo_copy_assets(${probe_name})
        eui_neo_copy_runtime_dependencies(${probe_name})
        if(MSVC)
            target_link_options(${probe_name} PRIVATE /SUBSYSTEM:CONSOLE)
        elseif(MINGW)
            target_link_options(${probe_name} PRIVATE -mconsole)
        endif()
        target_compile_definitions(${probe_name} PRIVATE EUI_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
        add_test(NAME ${probe_name} COMMAND ${probe_name} --frames 3)
        set_tests_properties(${probe_name} PROPERTIES LABELS "probe")
    endforeach()

    if(EUI_RESOLVED_RENDER_BACKEND STREQUAL "vulkan" AND TARGET shadertoy_probe)
        set(shadertoy_probe_assets_dir
            "${CMAKE_CURRENT_BINARY_DIR}/shadertoy-test-assets")
        eui_compile_shadertoy(shadertoy_probe
            SOURCE "tests/assets/shadertoy/feedback.frag"
            OUTPUT "${shadertoy_probe_assets_dir}/feedback.frag.spv"
        )
        eui_compile_shadertoy(shadertoy_probe
            SOURCE "tests/assets/shadertoy/buffer.frag"
            OUTPUT "${shadertoy_probe_assets_dir}/buffer.frag.spv"
            UNIFORMS "uValue:float"
        )
        eui_compile_shadertoy(shadertoy_probe
            SOURCE "tests/assets/shadertoy/channels.frag"
            OUTPUT "${shadertoy_probe_assets_dir}/channels.frag.spv"
            UNIFORMS "uValue:float"
        )
        eui_compile_shadertoy(shadertoy_probe
            SOURCE "tests/assets/shadertoy/pass_contract.frag"
            OUTPUT "${shadertoy_probe_assets_dir}/pass_contract.frag.spv"
        )
        set(EUI_SHADERTOY_INLINE_TEST_SOURCE
            "void mainImage(out vec4 color, in vec2 coord) { color = vec4(0.3, 0.4, 0.5, 1.0); }")
        eui_compile_shadertoy_inline(shadertoy_probe
            CONTENT "${EUI_SHADERTOY_INLINE_TEST_SOURCE}"
            OUTPUT "${shadertoy_probe_assets_dir}/inline.frag.spv"
        )
        set(EUI_SHADERTOY_HOT_RELOAD_INITIAL_SOURCE
            "void mainImage(out vec4 color, in vec2 coord) { color = vec4(0.2, 0.0, 0.0, 1.0); }")
        eui_compile_shadertoy_inline(shadertoy_probe
            CONTENT "${EUI_SHADERTOY_HOT_RELOAD_INITIAL_SOURCE}"
            OUTPUT "${shadertoy_probe_assets_dir}/hot_reload_initial.frag.spv"
        )
        set(EUI_SHADERTOY_HOT_RELOAD_RECOVERED_SOURCE
            "void mainImage(out vec4 color, in vec2 coord) { color = vec4(0.7, 0.0, 0.0, 1.0); }")
        eui_compile_shadertoy_inline(shadertoy_probe
            CONTENT "${EUI_SHADERTOY_HOT_RELOAD_RECOVERED_SOURCE}"
            OUTPUT "${shadertoy_probe_assets_dir}/hot_reload_recovered.frag.spv"
        )
        target_compile_definitions(shadertoy_probe PRIVATE
            EUI_TEST_SHADERS_DIR="${shadertoy_probe_assets_dir}")
    endif()

    file(GLOB TEST_UNIT_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/unit/*.cpp")
    foreach(unit_source ${TEST_UNIT_SOURCES})
        get_filename_component(unit_name "${unit_source}" NAME_WE)
        add_executable(${unit_name} "${unit_source}")
        target_link_libraries(${unit_name} PRIVATE eui::neo)
        eui_apply_compile_options(${unit_name})
        if(unit_name STREQUAL "shadertoy_packaged_assets")
            eui_neo_copy_assets(${unit_name})
            if(TARGET shadertoy)
                add_dependencies(${unit_name} shadertoy)
                if(EUI_RESOLVED_RENDER_BACKEND STREQUAL "vulkan")
                    target_compile_definitions(${unit_name} PRIVATE
                        EUI_TEST_EXPECT_SHADERTOY_SPIRV=1)
                endif()
            endif()
        endif()
        add_test(NAME ${unit_name} COMMAND ${unit_name})
        set_tests_properties(${unit_name} PROPERTIES LABELS "unit")
    endforeach()
endif()

if(Python3_Interpreter_FOUND)
    add_custom_target(eui_check_vulkan_shaders
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_vulkan_shaders.py" --check
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        COMMENT "Checking generated Vulkan shader headers"
        VERBATIM
    )
endif()

# -----------------------------------------------------------------------------
# Install and CMake package export
# -----------------------------------------------------------------------------

if(EUI_ENABLE_INSTALL)
    set(EUI_NEO_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/EuiNeo" CACHE STRING "Install path for EUI-NEO CMake package files")

    install(TARGETS eui_neo
        EXPORT EuiNeoTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
    install(FILES "${EUI_APP_MAIN_SOURCE}"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/eui-neo/app"
        RENAME app_main.cpp
    )
    install(DIRECTORY assets/
        DESTINATION "${CMAKE_INSTALL_DATADIR}/eui-neo/assets"
    )
    if(TARGET eui_shadertoy_wrap)
        install(TARGETS eui_shadertoy_wrap
            EXPORT EuiNeoTargets
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
        install(FILES scripts/EuiShaderToy.cmake scripts/generate_shadertoy_spirv.py
            DESTINATION ${EUI_NEO_INSTALL_CMAKEDIR})
    endif()
    set(EUI_NEO_INSTALL_DEP_TARGETS freetype png_static eui_zlib eui_md4c SDL2-static)
    if(EUI_OWNS_GLAD_TARGET)
        list(APPEND EUI_NEO_INSTALL_DEP_TARGETS glad)
    endif()
    if(EUI_OWNS_GLFW_TARGET)
        list(APPEND EUI_NEO_INSTALL_DEP_TARGETS glfw)
    endif()
    foreach(optional_target ${EUI_NEO_INSTALL_DEP_TARGETS})
        if(TARGET ${optional_target})
            install(TARGETS ${optional_target}
                EXPORT EuiNeoTargets
                ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
                LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
                RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            )
        endif()
    endforeach()

    install(DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h"
        PATTERN "detail" EXCLUDE
    )
    install(FILES include/eui/detail/dsl_app_impl.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/eui/detail
    )
    set(EUI_NEO_INSTALL_HEADER_DIRS components core)
    if(EUI_ENABLE_MODULES AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/modules")
        list(APPEND EUI_NEO_INSTALL_HEADER_DIRS modules)
    endif()
    install(DIRECTORY ${EUI_NEO_INSTALL_HEADER_DIRS}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/eui-neo
        FILES_MATCHING PATTERN "*.h"
    )
    if(EUI_OWNS_GLFW_TARGET AND TARGET glfw)
        install(DIRECTORY "${EUI_GLFW_DIR}/include/GLFW"
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
            FILES_MATCHING PATTERN "*.h"
        )
    endif()
    if(TARGET freetype)
        install(DIRECTORY "${EUI_FREETYPE_DIR}/include/"
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/freetype2
            FILES_MATCHING PATTERN "*.h"
        )
    endif()
    if(TARGET png_static)
        install(FILES
            "${EUI_LIBPNG_DIR}/png.h"
            "${EUI_LIBPNG_DIR}/pngconf.h"
            "${CMAKE_CURRENT_BINARY_DIR}/_deps/libpng-bundled-build/pnglibconf.h"
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libpng16
        )
    endif()
    install(FILES
        3rd/stb_image.h
        3rd/nanosvg.h
        3rd/nanosvgrast.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/eui-neo/3rd
    )
    install(FILES "${TRAY_INCLUDE_DIR}/tray.h"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/eui-neo/3rd/tray
    )
    if(EUI_OWNS_GLAD_TARGET AND TARGET glad)
        install(DIRECTORY "${glad_SOURCE_DIR}/include/"
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/eui-neo/3rd/glad/include
            FILES_MATCHING PATTERN "*.h"
        )
    endif()
    install(FILES
        "${EUI_ZLIB_DIR}/zconf.h"
        "${EUI_ZLIB_DIR}/zlib.h"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/eui-neo/3rd/zlib
    )
    if(TARGET eui_md4c)
        install(FILES "${EUI_MD4C_DIR}/src/md4c.h"
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/eui-neo/3rd/md4c/src
        )
    endif()

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/EuiNeoConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )
    set(EUI_NEO_CONFIG_TEMPLATE "${CMAKE_CURRENT_BINARY_DIR}/EuiNeoConfig.cmake.in")
    string(JOIN " " EUI_MINGW_STATIC_LINK_OPTIONS_CONFIG
        ${EUI_MINGW_STATIC_LINK_OPTIONS})
    file(WRITE "${EUI_NEO_CONFIG_TEMPLATE}" [=[
@PACKAGE_INIT@

include(CMakeFindDependencyMacro)

set(EUI_NEO_WINDOW_BACKEND "@EUI_WINDOW_BACKEND@")
set(EUI_NEO_RENDER_BACKEND "@EUI_RESOLVED_RENDER_BACKEND@")
set(EUI_NEO_BUILD_SHARED @EUI_BUILD_SHARED@)
set(EUI_NEO_DEPS_MODE "@EUI_DEPS_MODE@")
set(EUI_NEO_ENABLE_MARKDOWN @EUI_ENABLE_MARKDOWN@)
set(EUI_NEO_ENABLE_MODULES @EUI_ENABLE_MODULES@)
set(EUI_APP_MAIN_SOURCE "${PACKAGE_PREFIX_DIR}/@CMAKE_INSTALL_DATADIR@/eui-neo/app/app_main.cpp")
set(EUI_APP_ASSETS_DIR "${PACKAGE_PREFIX_DIR}/@CMAKE_INSTALL_DATADIR@/eui-neo/assets")

find_dependency(Threads)

if(EUI_NEO_RENDER_BACKEND STREQUAL "opengl")
    find_dependency(OpenGL)
    if(NOT @EUI_OWNS_GLAD_TARGET@)
        find_dependency(glad CONFIG)
    endif()
elseif(EUI_NEO_RENDER_BACKEND STREQUAL "vulkan")
    find_dependency(Vulkan)
endif()

if(EUI_NEO_WINDOW_BACKEND STREQUAL "glfw" AND NOT @EUI_OWNS_GLFW_TARGET@)
    find_dependency(glfw3 CONFIG)
endif()

if(EUI_NEO_WINDOW_BACKEND STREQUAL "sdl2")
    find_dependency(SDL2 CONFIG)
endif()

if(NOT WIN32)
    find_dependency(CURL)
endif()

include("${CMAKE_CURRENT_LIST_DIR}/EuiNeoTargets.cmake")

function(eui_neo_copy_assets target_name)
    if(EXISTS "${EUI_APP_ASSETS_DIR}")
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:${target_name}>/assets"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${EUI_APP_ASSETS_DIR}"
                "$<TARGET_FILE_DIR:${target_name}>/assets"
            VERBATIM
        )
    endif()
endfunction()

function(eui_neo_configure_app target_name)
    target_sources(${target_name} PRIVATE "${EUI_APP_MAIN_SOURCE}")
    target_link_libraries(${target_name} PRIVATE eui::neo)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /utf-8)
        target_link_options(${target_name} PRIVATE /ENTRY:mainCRTStartup /SUBSYSTEM:WINDOWS)
    elseif(MINGW)
        target_link_options(${target_name} PRIVATE -mwindows)
        if(NOT EUI_NEO_BUILD_SHARED)
            target_link_options(${target_name} PRIVATE @EUI_MINGW_STATIC_LINK_OPTIONS_CONFIG@)
        endif()
    endif()
    eui_neo_copy_assets(${target_name})
endfunction()

if(EUI_NEO_RENDER_BACKEND STREQUAL "vulkan")
    include("${CMAKE_CURRENT_LIST_DIR}/EuiShaderToy.cmake")
endif()

check_required_components(EuiNeo)
]=])

    configure_package_config_file(
        "${EUI_NEO_CONFIG_TEMPLATE}"
        "${CMAKE_CURRENT_BINARY_DIR}/EuiNeoConfig.cmake"
        INSTALL_DESTINATION "${EUI_NEO_INSTALL_CMAKEDIR}"
    )

    install(EXPORT EuiNeoTargets
        FILE EuiNeoTargets.cmake
        NAMESPACE eui::
        DESTINATION "${EUI_NEO_INSTALL_CMAKEDIR}"
    )
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/EuiNeoConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/EuiNeoConfigVersion.cmake"
        DESTINATION "${EUI_NEO_INSTALL_CMAKEDIR}"
    )
endif()
