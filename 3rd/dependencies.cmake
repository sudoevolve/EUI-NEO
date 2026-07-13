set(EUI_DEPS_MODE "auto" CACHE STRING "Third-party dependency mode: bundled, auto, or fetch")
set_property(CACHE EUI_DEPS_MODE PROPERTY STRINGS bundled auto fetch)
if(NOT EUI_DEPS_MODE MATCHES "^(bundled|auto|fetch)$")
    message(FATAL_ERROR "EUI_DEPS_MODE must be one of: bundled, auto, fetch")
endif()
option(EUI_ENABLE_MARKDOWN "Enable MD4C Markdown parsing support" ON)

set(EUI_THIRD_PARTY_DIR "${CMAKE_CURRENT_LIST_DIR}")
include(FetchContent)

file(GLOB _eui_third_party_entries
    CONFIGURE_DEPENDS
    LIST_DIRECTORIES TRUE
    "${EUI_THIRD_PARTY_DIR}/*"
)
set(EUI_THIRD_PARTY_SOURCE_DIRS "")
foreach(_eui_candidate IN LISTS _eui_third_party_entries)
    if(IS_DIRECTORY "${_eui_candidate}")
        list(APPEND EUI_THIRD_PARTY_SOURCE_DIRS "${_eui_candidate}")
    endif()
endforeach()
unset(_eui_candidate)
unset(_eui_third_party_entries)

function(eui_find_bundled_dependency out_var dep_name)
    if(EUI_DEPS_MODE STREQUAL "fetch")
        set(${out_var} "" PARENT_SCOPE)
        return()
    endif()

    set(matches "")
    foreach(candidate IN LISTS EUI_THIRD_PARTY_SOURCE_DIRS)
        set(is_match TRUE)
        foreach(marker IN LISTS ARGN)
            if(NOT EXISTS "${candidate}/${marker}")
                set(is_match FALSE)
                break()
            endif()
        endforeach()
        if(is_match)
            list(APPEND matches "${candidate}")
        endif()
    endforeach()

    list(LENGTH matches match_count)
    if(match_count GREATER 1)
        list(JOIN matches "', '" match_list)
        message(FATAL_ERROR
            "Multiple bundled source directories match '${dep_name}': '${match_list}'. "
            "Keep only one matching source directory under ${EUI_THIRD_PARTY_DIR}."
        )
    endif()

    if(match_count EQUAL 1)
        list(GET matches 0 source_dir)
        message(STATUS "Using bundled ${dep_name} source: ${source_dir}")
        set(${out_var} "${source_dir}" PARENT_SCOPE)
        return()
    endif()

    if(EUI_DEPS_MODE STREQUAL "bundled")
        list(JOIN ARGN "', '" marker_list)
        message(FATAL_ERROR
            "Bundled dependency '${dep_name}' was not found under ${EUI_THIRD_PARTY_DIR}. "
            "Add one source directory containing '${marker_list}', or configure with "
            "-DEUI_DEPS_MODE=auto/fetch to allow a network fetch."
        )
    endif()

    set(${out_var} "" PARENT_SCOPE)
endfunction()

function(eui_prepare_pkg_config_stub out_var)
    if(WIN32)
        set(stub_path "${CMAKE_CURRENT_BINARY_DIR}/eui-pkg-config-stub.bat")
        file(WRITE "${stub_path}" "@echo 0.0.0\r\n@if \"%1\"==\"--version\" exit /b 0\r\n@exit /b 1\r\n")
    else()
        set(stub_path "${CMAKE_CURRENT_BINARY_DIR}/eui-pkg-config-stub.sh")
        file(WRITE "${stub_path}" "#!/bin/sh\nif [ \"$1\" = \"--version\" ]; then echo 0.0.0; exit 0; fi\nexit 1\n")
        file(CHMOD "${stub_path}"
            PERMISSIONS
                OWNER_READ OWNER_WRITE OWNER_EXECUTE
                GROUP_READ GROUP_EXECUTE
                WORLD_READ WORLD_EXECUTE
        )
    endif()

    set(${out_var} "${stub_path}" PARENT_SCOPE)
endfunction()

function(eui_set_third_party_option name value description)
    set(${name} ${value} CACHE BOOL "${description}" FORCE)
endfunction()

macro(eui_begin_quiet_third_party_config)
    set(EUI_PREV_CMAKE_DISABLE_FIND_PACKAGE_PkgConfig "${CMAKE_DISABLE_FIND_PACKAGE_PkgConfig}")
    set(EUI_PREV_CMAKE_MESSAGE_LOG_LEVEL "${CMAKE_MESSAGE_LOG_LEVEL}")
    set(EUI_PREV_PKG_CONFIG_EXECUTABLE "${PKG_CONFIG_EXECUTABLE}")
    set(EUI_PREV_PkgConfig_FIND_QUIETLY "${PkgConfig_FIND_QUIETLY}")
    eui_prepare_pkg_config_stub(EUI_PKG_CONFIG_STUB)

    if(COMMAND cmake_diagnostic)
        cmake_diagnostic(GET CMD_DEPRECATED EUI_PREV_CMAKE_CMD_DEPRECATED_DIAGNOSTIC)
        cmake_diagnostic(SET CMD_DEPRECATED IGNORE RECURSE)
    else()
        set(EUI_PREV_CMAKE_WARN_DEPRECATED "${CMAKE_WARN_DEPRECATED}")
        set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "Suppress bundled third-party CMake deprecation warnings" FORCE)
    endif()
    set(CMAKE_DISABLE_FIND_PACKAGE_PkgConfig TRUE CACHE BOOL "Suppress bundled third-party pkg-config probes" FORCE)
    set(CMAKE_MESSAGE_LOG_LEVEL WARNING)
    set(PKG_CONFIG_EXECUTABLE "${EUI_PKG_CONFIG_STUB}" CACHE FILEPATH "Bundled third-party pkg-config stub" FORCE)
    set(PkgConfig_FIND_QUIETLY TRUE)
endmacro()

macro(eui_end_quiet_third_party_config)
    if(COMMAND cmake_diagnostic)
        cmake_diagnostic(SET CMD_DEPRECATED "${EUI_PREV_CMAKE_CMD_DEPRECATED_DIAGNOSTIC}" RECURSE)
    else()
        set(CMAKE_WARN_DEPRECATED "${EUI_PREV_CMAKE_WARN_DEPRECATED}" CACHE BOOL "Suppress deprecated functionality warnings" FORCE)
    endif()
    set(CMAKE_DISABLE_FIND_PACKAGE_PkgConfig "${EUI_PREV_CMAKE_DISABLE_FIND_PACKAGE_PkgConfig}" CACHE BOOL "Disable find_package(PkgConfig)" FORCE)
    set(CMAKE_MESSAGE_LOG_LEVEL "${EUI_PREV_CMAKE_MESSAGE_LOG_LEVEL}")
    set(PKG_CONFIG_EXECUTABLE "${EUI_PREV_PKG_CONFIG_EXECUTABLE}" CACHE FILEPATH "pkg-config executable" FORCE)
    set(PkgConfig_FIND_QUIETLY "${EUI_PREV_PkgConfig_FIND_QUIETLY}")
    unset(EUI_PREV_CMAKE_CMD_DEPRECATED_DIAGNOSTIC)
    unset(EUI_PREV_CMAKE_WARN_DEPRECATED)
endmacro()

macro(eui_begin_static_third_party_config)
    set(EUI_PREV_BUILD_SHARED_LIBS "${BUILD_SHARED_LIBS}")
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build bundled third-party libraries as static libraries" FORCE)
endmacro()

macro(eui_end_static_third_party_config)
    set(BUILD_SHARED_LIBS "${EUI_PREV_BUILD_SHARED_LIBS}" CACHE BOOL "Build shared libraries" FORCE)
endmacro()

set(EUI_FIND_MODULE_DIR "${CMAKE_CURRENT_BINARY_DIR}/_deps/eui-find-modules")
file(MAKE_DIRECTORY "${EUI_FIND_MODULE_DIR}")
file(WRITE "${EUI_FIND_MODULE_DIR}/FindZLIB.cmake" [=[
if(TARGET ZLIB::ZLIB AND DEFINED EUI_ZLIB_DIR AND EXISTS "${EUI_ZLIB_DIR}/zlib.h")
    set(ZLIB_FOUND TRUE)
    set(ZLIB_INCLUDE_DIRS "${EUI_ZLIB_DIR}")
    set(ZLIB_LIBRARIES ZLIB::ZLIB)
    set(ZLIB_LIBRARY ZLIB::ZLIB)
    set(ZLIB_INCLUDE_DIR "${EUI_ZLIB_DIR}")

    file(STRINGS "${EUI_ZLIB_DIR}/zlib.h" _eui_zlib_version_line REGEX "^#define ZLIB_VERSION ")
    string(REGEX REPLACE "^#define ZLIB_VERSION \"([^\"]+)\".*$" "\\1" ZLIB_VERSION_STRING "${_eui_zlib_version_line}")
    set(ZLIB_VERSION "${ZLIB_VERSION_STRING}")
else()
    include("${CMAKE_ROOT}/Modules/FindZLIB.cmake")
endif()
]=])
file(WRITE "${EUI_FIND_MODULE_DIR}/FindPNG.cmake" [=[
if(TARGET PNG::PNG)
    set(PNG_FOUND TRUE)
    set(PNG_LIBRARIES PNG::PNG)
    get_target_property(PNG_INCLUDE_DIRS PNG::PNG INTERFACE_INCLUDE_DIRECTORIES)
    if(NOT PNG_INCLUDE_DIRS)
        set(PNG_INCLUDE_DIRS "")
    endif()
    set(PNG_DEFINITIONS "")
else()
    include("${CMAKE_ROOT}/Modules/FindPNG.cmake")
endif()
]=])
list(PREPEND CMAKE_MODULE_PATH "${EUI_FIND_MODULE_DIR}")

set(EUI_OWNS_GLFW_TARGET OFF)
set(EUI_OWNS_GLAD_TARGET OFF)

function(eui_fetch_dependency source_dir_var content_name source_url)
    FetchContent_Declare(
        ${content_name}
        URL "${source_url}"
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        TIMEOUT 30
    )
    FetchContent_GetProperties(${content_name})
    if(NOT ${content_name}_POPULATED)
        if(POLICY CMP0169)
            cmake_policy(PUSH)
            cmake_policy(SET CMP0169 OLD)
        endif()
        FetchContent_Populate(${content_name})
        if(POLICY CMP0169)
            cmake_policy(POP)
        endif()
    endif()
    set(${source_dir_var} "${${content_name}_SOURCE_DIR}" PARENT_SCOPE)
endfunction()

function(eui_silence_third_party_warnings target_name)
    if(NOT TARGET ${target_name})
        return()
    endif()

    if(MSVC)
        target_compile_options(${target_name} PRIVATE $<$<NOT:$<CONFIG:Debug>>:/w>)
    else()
        target_compile_options(${target_name} PRIVATE $<$<NOT:$<CONFIG:Debug>>:-w>)
    endif()
endfunction()

if(EUI_WINDOW_BACKEND STREQUAL "glfw")
    eui_set_third_party_option(GLFW_BUILD_EXAMPLES OFF "Build the GLFW example programs")
    eui_set_third_party_option(GLFW_BUILD_TESTS OFF "Build the GLFW test programs")
    eui_set_third_party_option(GLFW_BUILD_DOCS OFF "Build the GLFW documentation")
    eui_set_third_party_option(GLFW_INSTALL OFF "Generate installation target")

    if(TARGET glfw)
        message(STATUS "Using existing GLFW target: glfw")
    else()
        if(EUI_DEPS_MODE STREQUAL "auto")
            find_package(glfw3 CONFIG QUIET)
        endif()

        if(NOT TARGET glfw)
            eui_find_bundled_dependency(
                EUI_GLFW_DIR
                "GLFW"
                "include/GLFW/glfw3.h"
                "src/context.c"
                "CMakeLists.txt"
            )

            eui_begin_static_third_party_config()
            if(EUI_GLFW_DIR)
                add_subdirectory("${EUI_GLFW_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/_deps/glfw-bundled-build" EXCLUDE_FROM_ALL)
            else()
                FetchContent_Declare(
                    glfw
                    URL https://github.com/glfw/glfw/archive/refs/tags/3.4.zip
                    URL_HASH SHA256=a133ddc3d3c66143eba9035621db8e0bcf34dba1ee9514a9e23e96afd39fd57a
                    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
                    TIMEOUT 30
                )
                FetchContent_MakeAvailable(glfw)
                set(EUI_GLFW_DIR "${glfw_SOURCE_DIR}")
            endif()
            eui_end_static_third_party_config()
            set(EUI_OWNS_GLFW_TARGET ON)
        else()
            message(STATUS "Using package GLFW target: glfw")
        endif()
    endif()

    if(NOT TARGET glfw)
        message(FATAL_ERROR
            "EUI_WINDOW_BACKEND=glfw requires a CMake target named 'glfw'. "
            "Provide one before adding EUI-NEO, install glfw3, or allow EUI-NEO to use bundled/fetched GLFW."
        )
    endif()
endif()

if(EUI_WINDOW_BACKEND STREQUAL "sdl2")
    if(EUI_DEPS_MODE STREQUAL "bundled")
        message(FATAL_ERROR
            "SDL2 is not vendored under 3rd/. Use -DEUI_DEPS_MODE=auto to prefer a system SDL2 package "
            "and fall back to fetching SDL2, or use -DEUI_DEPS_MODE=fetch to always download SDL2."
        )
    endif()

    set(EUI_FETCH_SDL2 OFF)
    if(EUI_DEPS_MODE STREQUAL "auto")
        find_package(SDL2 CONFIG QUIET)
        if(NOT TARGET SDL2::SDL2)
            message(STATUS "System SDL2 package was not found; fetching SDL2 instead.")
            set(EUI_FETCH_SDL2 ON)
        endif()
    elseif(EUI_DEPS_MODE STREQUAL "fetch")
        set(EUI_FETCH_SDL2 ON)
    endif()

    if(EUI_FETCH_SDL2)
        eui_set_third_party_option(SDL_SHARED OFF "Build a shared version of SDL2")
        eui_set_third_party_option(SDL_STATIC ON "Build a static version of SDL2")
        eui_set_third_party_option(SDL_TEST OFF "Build the SDL2_test library")
        eui_set_third_party_option(SDL_TESTS OFF "Build the SDL2 test programs")
        eui_set_third_party_option(SDL2_DISABLE_INSTALL ON "Disable installation of SDL2")
        eui_set_third_party_option(SDL2_DISABLE_UNINSTALL ON "Disable uninstallation of SDL2")

        FetchContent_Declare(
            sdl2
            URL https://www.libsdl.org/release/SDL2-2.32.10.tar.gz
            URL_HASH SHA256=5f5993c530f084535c65a6879e9b26ad441169b3e25d789d83287040a9ca5165
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
            TIMEOUT 30
        )
        FetchContent_MakeAvailable(sdl2)
        eui_silence_third_party_warnings(SDL2)
        eui_silence_third_party_warnings(SDL2-static)
    endif()
endif()

if(EUI_RESOLVED_RENDER_BACKEND STREQUAL "opengl")
    if(TARGET glad)
        message(STATUS "Using existing glad target: glad")
    else()
        if(EUI_DEPS_MODE STREQUAL "auto")
            find_package(glad CONFIG QUIET)
        endif()

        if(NOT TARGET glad)
            eui_find_bundled_dependency(
                EUI_GLAD_DIR
                "glad"
                "src/glad.c"
                "include/glad/glad.h"
            )

            if(EUI_GLAD_DIR)
                set(glad_SOURCE_DIR "${EUI_GLAD_DIR}")
            else()
                FetchContent_Declare(
                    glad
                    URL https://github.com/libigl/libigl-glad/archive/651a425101365aa6e8504988ef9bb363d066c5ee.zip
                    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
                    TIMEOUT 30
                )
                FetchContent_GetProperties(glad)
                if(NOT glad_POPULATED)
                    if(POLICY CMP0169)
                        cmake_policy(PUSH)
                        cmake_policy(SET CMP0169 OLD)
                    endif()
                    FetchContent_Populate(glad)
                    if(POLICY CMP0169)
                        cmake_policy(POP)
                    endif()
                endif()
            endif()
            add_library(glad "${glad_SOURCE_DIR}/src/glad.c")
            target_include_directories(glad PUBLIC
                $<BUILD_INTERFACE:${glad_SOURCE_DIR}/include>
                $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/eui-neo/3rd/glad/include>
            )
            if(NOT WIN32)
                target_link_libraries(glad PUBLIC ${CMAKE_DL_LIBS})
            endif()
            set_target_properties(glad PROPERTIES POSITION_INDEPENDENT_CODE ON)
            set(EUI_OWNS_GLAD_TARGET ON)
        else()
            message(STATUS "Using package glad target: glad")
        endif()
    endif()

    if(NOT TARGET glad)
        message(FATAL_ERROR
            "OpenGL builds require a CMake target named 'glad'. "
            "Provide one before adding EUI-NEO, install glad, or allow EUI-NEO to use bundled/fetched glad."
        )
    endif()
endif()

eui_find_bundled_dependency(
    TRAY_INCLUDE_DIR
    "tray"
    "tray.h"
)

if(NOT TRAY_INCLUDE_DIR)
    FetchContent_Declare(
        tray
        URL https://github.com/zserge/tray/archive/8dd1358b92562faf7c032cf5362fa97cbc7e13e9.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        TIMEOUT 30
    )
    FetchContent_MakeAvailable(tray)
    set(TRAY_INCLUDE_DIR "${tray_SOURCE_DIR}")
endif()

eui_find_bundled_dependency(
    EUI_FREETYPE_DIR
    "FreeType"
    "include/freetype/freetype.h"
    "src/base/ftsystem.c"
    "CMakeLists.txt"
)

eui_find_bundled_dependency(
    EUI_ZLIB_DIR
    "zlib"
    "zlib.h"
    "zconf.h"
    "inflate.c"
)

eui_find_bundled_dependency(
    EUI_LIBPNG_DIR
    "libpng"
    "png.h"
    "pngconf.h"
    "png.c"
    "CMakeLists.txt"
)

if(EUI_ENABLE_MARKDOWN)
    eui_find_bundled_dependency(
        EUI_MD4C_DIR
        "MD4C"
        "src/md4c.c"
        "src/md4c.h"
    )
endif()

if(EUI_RESOLVED_RENDER_BACKEND STREQUAL "opengl")
    find_package(OpenGL QUIET)
    if(NOT OpenGL_FOUND)
        message(FATAL_ERROR
            "Default EUI-NEO builds use the OpenGL render backend, but OpenGL development files were not found. "
            "Install platform OpenGL/windowing development files, or configure a Vulkan build with "
            "a Vulkan build directory, for example: cmake -S . -B build-vk"
        )
    endif()
endif()
find_package(Threads REQUIRED)

if(NOT EUI_ZLIB_DIR)
    eui_fetch_dependency(
        EUI_ZLIB_DIR
        eui_zlib
        "https://github.com/madler/zlib/archive/refs/tags/v1.3.1.zip"
    )
endif()

add_library(eui_zlib STATIC
    "${EUI_ZLIB_DIR}/adler32.c"
    "${EUI_ZLIB_DIR}/compress.c"
    "${EUI_ZLIB_DIR}/crc32.c"
    "${EUI_ZLIB_DIR}/deflate.c"
    "${EUI_ZLIB_DIR}/gzclose.c"
    "${EUI_ZLIB_DIR}/gzlib.c"
    "${EUI_ZLIB_DIR}/gzread.c"
    "${EUI_ZLIB_DIR}/gzwrite.c"
    "${EUI_ZLIB_DIR}/inflate.c"
    "${EUI_ZLIB_DIR}/infback.c"
    "${EUI_ZLIB_DIR}/inftrees.c"
    "${EUI_ZLIB_DIR}/inffast.c"
    "${EUI_ZLIB_DIR}/trees.c"
    "${EUI_ZLIB_DIR}/uncompr.c"
    "${EUI_ZLIB_DIR}/zutil.c"
)
target_include_directories(eui_zlib PUBLIC
    $<BUILD_INTERFACE:${EUI_ZLIB_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/eui-neo/3rd/zlib>
)
if(UNIX)
    target_compile_definitions(eui_zlib PRIVATE HAVE_UNISTD_H)
endif()
set_target_properties(eui_zlib PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(NOT TARGET ZLIB::ZLIB)
    add_library(ZLIB::ZLIB ALIAS eui_zlib)
endif()

if(NOT EUI_LIBPNG_DIR)
    eui_fetch_dependency(
        EUI_LIBPNG_DIR
        eui_libpng
        "https://github.com/pnggroup/libpng/archive/refs/tags/v1.6.43.zip"
    )
endif()

eui_set_third_party_option(PNG_SHARED OFF "Build libpng as a shared library")
eui_set_third_party_option(PNG_STATIC ON "Build libpng as a static library")
eui_set_third_party_option(PNG_FRAMEWORK OFF "Build libpng as a framework bundle")
eui_set_third_party_option(PNG_TESTS OFF "Build the libpng tests")
eui_set_third_party_option(PNG_TOOLS OFF "Build the libpng tools")
eui_set_third_party_option(PNG_EXECUTABLES ON "Deprecated libpng tools option; keep default to avoid compatibility warning")
eui_set_third_party_option(PNG_BUILD_ZLIB OFF "Use find_package(ZLIB) for libpng")
eui_set_third_party_option(PNG_HARDWARE_OPTIMIZATIONS OFF "Disable libpng hardware optimizations for portable bundled builds")
eui_set_third_party_option(SKIP_INSTALL_ALL ON "Disable install rules for bundled libpng")
eui_begin_quiet_third_party_config()
add_subdirectory("${EUI_LIBPNG_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/_deps/libpng-bundled-build" EXCLUDE_FROM_ALL)
eui_end_quiet_third_party_config()
if(TARGET png_static)
    target_include_directories(png_static PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/_deps/libpng-bundled-build>")
endif()
if(TARGET png_static AND NOT TARGET PNG::PNG)
    add_library(PNG::PNG ALIAS png_static)
endif()

eui_set_third_party_option(FT_DISABLE_ZLIB ON "Disable zlib support in bundled FreeType")
eui_set_third_party_option(FT_DISABLE_BZIP2 ON "Disable bzip2 support in bundled FreeType")
eui_set_third_party_option(FT_DISABLE_PNG OFF "Disable png support in bundled FreeType")
eui_set_third_party_option(FT_REQUIRE_PNG ON "Require png support in bundled FreeType")
eui_set_third_party_option(FT_DISABLE_HARFBUZZ ON "Disable HarfBuzz support in bundled FreeType")
eui_set_third_party_option(FT_DISABLE_BROTLI ON "Disable brotli support in bundled FreeType")
if(NOT EUI_FREETYPE_DIR)
    eui_fetch_dependency(
        EUI_FREETYPE_DIR
        eui_freetype
        "https://download.savannah.gnu.org/releases/freetype/freetype-2.13.3.tar.xz"
    )
endif()

eui_begin_quiet_third_party_config()
add_subdirectory("${EUI_FREETYPE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/_deps/freetype-bundled-build" EXCLUDE_FROM_ALL)
eui_end_quiet_third_party_config()
if(TARGET freetype AND NOT TARGET Freetype::Freetype)
    add_library(Freetype::Freetype ALIAS freetype)
endif()
if(TARGET freetype)
    eui_silence_third_party_warnings(freetype)
endif()
if(MSVC AND TARGET freetype)
    target_compile_options(freetype PRIVATE /utf-8)
endif()

if(NOT WIN32)
    find_package(CURL REQUIRED)
else()
    find_package(CURL QUIET)
endif()

if(EUI_ENABLE_MARKDOWN)
    if(NOT EUI_MD4C_DIR)
        eui_fetch_dependency(
            EUI_MD4C_DIR
            eui_md4c_source
            "https://github.com/mity/md4c/archive/refs/tags/release-0.5.3.zip"
        )
    endif()

    add_library(eui_md4c STATIC "${EUI_MD4C_DIR}/src/md4c.c")
    target_include_directories(eui_md4c PUBLIC
        $<BUILD_INTERFACE:${EUI_MD4C_DIR}/src>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/eui-neo/3rd/md4c/src>
    )
    set_target_properties(eui_md4c PROPERTIES POSITION_INDEPENDENT_CODE ON)
    eui_silence_third_party_warnings(eui_md4c)
    if(NOT TARGET md4c::md4c)
        add_library(md4c::md4c ALIAS eui_md4c)
    endif()
endif()

if(UNIX AND NOT APPLE)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(EUI_APPINDICATOR QUIET appindicator3-0.1)
        pkg_check_modules(EUI_GTK3 QUIET gtk+-3.0)
    endif()
endif()
