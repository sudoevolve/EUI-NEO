include_guard(GLOBAL)

if(DEFINED EUI_DEPS_MODE)
    message(FATAL_ERROR
        "EUI_DEPS_MODE has been removed. EUI-NEO now uses system/package-manager dependencies by default. "
        "Use -DEUI_DEPS_ALLOW_FETCH=ON together with per-library options such as -DEUI_FETCH_GLFW=ON "
        "when CPM downloads are intended."
    )
endif()

option(EUI_DEPS_ALLOW_FETCH "Allow EUI-NEO to download third-party dependencies with CPM.cmake." OFF)
option(EUI_FETCH_GLFW "Allow CPM download of GLFW." OFF)
option(EUI_FETCH_SDL2 "Allow CPM download of SDL2." OFF)
option(EUI_FETCH_ZLIB "Allow CPM download of zlib." OFF)
option(EUI_FETCH_LIBPNG "Allow CPM download of libpng." OFF)
option(EUI_FETCH_FREETYPE "Allow CPM download of FreeType." OFF)
option(EUI_FETCH_HARFBUZZ "Allow CPM download of HarfBuzz." OFF)
option(EUI_FETCH_MD4C "Allow CPM download of MD4C." OFF)
option(EUI_FETCH_NANOSVG "Allow CPM download of NanoSVG." OFF)
option(EUI_FETCH_TRAY "Allow CPM download of zserge/tray." OFF)
option(EUI_ENABLE_HARFBUZZ "Enable HarfBuzz shaping for complex text." ON)
option(EUI_ENABLE_MARKDOWN "Enable MD4C Markdown parsing support." ON)

set(EUI_CMAKE_DIR "${CMAKE_CURRENT_LIST_DIR}")
set(EUI_DEPENDENCY_MODULE_DIR "${EUI_CMAKE_DIR}/deps")
get_filename_component(EUI_PROJECT_SOURCE_DIR "${EUI_CMAKE_DIR}/.." ABSOLUTE)
set(EUI_THIRD_PARTY_DIR "${EUI_PROJECT_SOURCE_DIR}/3rd")

set(EUI_PREV_CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}")
list(PREPEND CMAKE_MODULE_PATH "${EUI_CMAKE_DIR}/Modules")

include("${EUI_CMAKE_DIR}/EuiDependencyHelpers.cmake")
include("${EUI_CMAKE_DIR}/EuiCpm.cmake")
include("${EUI_DEPENDENCY_MODULE_DIR}/EuiFindGLFW.cmake")
include("${EUI_DEPENDENCY_MODULE_DIR}/EuiFindSDL2.cmake")
include("${EUI_DEPENDENCY_MODULE_DIR}/EuiFindGlad.cmake")
include("${EUI_DEPENDENCY_MODULE_DIR}/EuiFindZLIB.cmake")
include("${EUI_DEPENDENCY_MODULE_DIR}/EuiFindLibPNG.cmake")
include("${EUI_DEPENDENCY_MODULE_DIR}/EuiFindFreeType.cmake")
include("${EUI_DEPENDENCY_MODULE_DIR}/EuiFindHarfBuzz.cmake")
include("${EUI_DEPENDENCY_MODULE_DIR}/EuiFindMD4C.cmake")
include("${EUI_DEPENDENCY_MODULE_DIR}/EuiFindNanoSVG.cmake")
include("${EUI_DEPENDENCY_MODULE_DIR}/EuiFindTray.cmake")

find_package(Threads REQUIRED)

if(EUI_RESOLVED_RENDER_BACKEND STREQUAL "opengl")
    eui_resolve_glad()
    find_package(OpenGL QUIET)
    if(NOT OpenGL_FOUND)
        message(FATAL_ERROR
            "OpenGL development files were not found. Install platform OpenGL/windowing development files, "
            "or configure a Vulkan build with cmake -S . -B build-vk."
        )
    endif()
endif()

if(EUI_WINDOW_BACKEND STREQUAL "glfw")
    eui_resolve_glfw()
elseif(EUI_WINDOW_BACKEND STREQUAL "sdl2")
    eui_resolve_sdl2()
endif()

eui_resolve_freetype()
eui_resolve_nanosvg()

if(EUI_ENABLE_HARFBUZZ)
    eui_resolve_harfbuzz()
endif()

if(EUI_ENABLE_MARKDOWN)
    eui_resolve_md4c()
endif()

if(NOT WIN32)
    find_package(CURL REQUIRED)
else()
    eui_begin_pkg_config_cache_guard()
    find_package(CURL QUIET)
    eui_end_pkg_config_cache_guard()
endif()

if(UNIX AND NOT APPLE)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(EUI_APPINDICATOR QUIET appindicator3-0.1)
        pkg_check_modules(EUI_GTK3 QUIET gtk+-3.0)
    endif()
endif()

if(WIN32 OR (UNIX AND NOT APPLE AND EUI_APPINDICATOR_FOUND AND EUI_GTK3_FOUND))
    eui_resolve_tray()
endif()

set(CMAKE_MODULE_PATH "${EUI_PREV_CMAKE_MODULE_PATH}")
