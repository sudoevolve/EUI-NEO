set(EUI_DEPS_MODE "auto" CACHE STRING "Third-party dependency mode: bundled, auto, or fetch")
set_property(CACHE EUI_DEPS_MODE PROPERTY STRINGS bundled auto fetch)
if(NOT EUI_DEPS_MODE MATCHES "^(bundled|auto|fetch)$")
    message(FATAL_ERROR "EUI_DEPS_MODE must be one of: bundled, auto, fetch")
endif()
option(EUI_ENABLE_HARFBUZZ "Enable HarfBuzz shaping for complex text" ON)
option(EUI_ENABLE_MARKDOWN "Enable MD4C Markdown parsing support" ON)

set(EUI_THIRD_PARTY_DIR "${CMAKE_CURRENT_LIST_DIR}")
list(PREPEND CMAKE_MODULE_PATH "${EUI_THIRD_PARTY_DIR}")

include(FetchContent)
include("${CMAKE_CURRENT_LIST_DIR}/EuiThirdParty.cmake")

set(EUI_OWNS_GLFW_TARGET OFF)
set(EUI_OWNS_GLAD_TARGET OFF)

function(eui_use_bundled_dependency out_var dep_name source_dir marker_file)
    if(EUI_DEPS_MODE STREQUAL "fetch")
        set(${out_var} FALSE PARENT_SCOPE)
        return()
    endif()

    if(EXISTS "${source_dir}/${marker_file}")
        set(${out_var} TRUE PARENT_SCOPE)
        return()
    endif()

    if(EUI_DEPS_MODE STREQUAL "bundled")
        message(FATAL_ERROR
            "Bundled dependency '${dep_name}' is missing at ${source_dir}. "
            "Restore the vendored source, or configure with -DEUI_DEPS_MODE=auto/fetch to allow network fetches."
        )
    endif()

    set(${out_var} FALSE PARENT_SCOPE)
endfunction()

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
            eui_use_bundled_dependency(
                EUI_USE_BUNDLED_GLFW
                "GLFW"
                "${EUI_THIRD_PARTY_DIR}/glfw"
                "CMakeLists.txt"
            )

            eui_begin_static_third_party_config()
            if(EUI_USE_BUNDLED_GLFW)
                set(EUI_GLFW_DIR "${EUI_THIRD_PARTY_DIR}/glfw")
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

if(EUI_WINDOW_BACKEND STREQUAL "sdl3")
    if(EUI_DEPS_MODE STREQUAL "bundled")
        message(FATAL_ERROR
            "SDL3 is not vendored under 3rd/. Use -DEUI_DEPS_MODE=auto to prefer a system SDL3 package "
            "and fall back to fetching SDL3, or use -DEUI_DEPS_MODE=fetch to always download SDL3."
        )
    endif()

    set(EUI_FETCH_SDL3 OFF)
    if(EUI_DEPS_MODE STREQUAL "auto")
        find_package(SDL3 CONFIG QUIET COMPONENTS SDL3)
        if(NOT TARGET SDL3::SDL3)
            message(STATUS "System SDL3 package was not found; fetching SDL3 instead.")
            set(EUI_FETCH_SDL3 ON)
        endif()
    elseif(EUI_DEPS_MODE STREQUAL "fetch")
        set(EUI_FETCH_SDL3 ON)
    endif()

    if(EUI_FETCH_SDL3)
        eui_set_third_party_option(SDL_SHARED OFF "Build a shared version of SDL3")
        eui_set_third_party_option(SDL_STATIC ON "Build a static version of SDL3")
        eui_set_third_party_option(SDL_TESTS OFF "Build the SDL3 test programs")
        eui_set_third_party_option(SDL_EXAMPLES OFF "Build the SDL3 example programs")
        eui_set_third_party_option(SDL_INSTALL OFF "Generate installation target")

        FetchContent_Declare(
            sdl3
            URL https://github.com/libsdl-org/SDL/releases/download/release-3.4.10/SDL3-3.4.10.tar.gz
            URL_HASH SHA256=12b34280415ec8418c864408b93d008a20a6530687ee613d60bfbd20411f2785
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
            TIMEOUT 30
        )
        FetchContent_MakeAvailable(sdl3)
        eui_silence_third_party_warnings(SDL3-static)
        eui_silence_third_party_warnings(SDL3-shared)
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
            eui_use_bundled_dependency(
                EUI_USE_BUNDLED_GLAD
                "glad"
                "${EUI_THIRD_PARTY_DIR}/glad"
                "src/glad.c"
            )

            if(EUI_USE_BUNDLED_GLAD)
                set(glad_SOURCE_DIR "${EUI_THIRD_PARTY_DIR}/glad")
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

eui_use_bundled_dependency(
    EUI_USE_BUNDLED_TRAY
    "tray"
    "${EUI_THIRD_PARTY_DIR}/tray"
    "tray.h"
)

if(EUI_USE_BUNDLED_TRAY)
    set(TRAY_INCLUDE_DIR "${EUI_THIRD_PARTY_DIR}/tray")
else()
    FetchContent_Declare(
        tray
        URL https://github.com/zserge/tray/archive/8dd1358b92562faf7c032cf5362fa97cbc7e13e9.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        TIMEOUT 30
    )
    FetchContent_MakeAvailable(tray)
    set(TRAY_INCLUDE_DIR "${tray_SOURCE_DIR}")
endif()

eui_use_bundled_dependency(
    EUI_USE_BUNDLED_FREETYPE
    "FreeType"
    "${EUI_THIRD_PARTY_DIR}/freetype"
    "CMakeLists.txt"
)

eui_use_bundled_dependency(
    EUI_USE_BUNDLED_ZLIB
    "zlib"
    "${EUI_THIRD_PARTY_DIR}/zlib-1.3.1"
    "zlib.h"
)

eui_use_bundled_dependency(
    EUI_USE_BUNDLED_LIBPNG
    "libpng"
    "${EUI_THIRD_PARTY_DIR}/libpng-1.6.43"
    "png.h"
)

if(EUI_ENABLE_HARFBUZZ)
    eui_use_bundled_dependency(
        EUI_USE_BUNDLED_HARFBUZZ
        "HarfBuzz"
        "${EUI_THIRD_PARTY_DIR}/harfbuzz"
        "CMakeLists.txt"
    )
endif()

if(EUI_ENABLE_MARKDOWN)
    eui_use_bundled_dependency(
        EUI_USE_BUNDLED_MD4C
        "MD4C"
        "${EUI_THIRD_PARTY_DIR}/md4c"
        "src/md4c.c"
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

if(EUI_USE_BUNDLED_ZLIB)
    set(EUI_ZLIB_DIR "${EUI_THIRD_PARTY_DIR}/zlib-1.3.1")
else()
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
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/eui-neo/3rd/zlib-1.3.1>
)
if(UNIX)
    target_compile_definitions(eui_zlib PRIVATE HAVE_UNISTD_H)
endif()
set_target_properties(eui_zlib PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(NOT TARGET ZLIB::ZLIB)
    add_library(ZLIB::ZLIB ALIAS eui_zlib)
endif()

if(EUI_USE_BUNDLED_LIBPNG)
    set(EUI_LIBPNG_DIR "${EUI_THIRD_PARTY_DIR}/libpng-1.6.43")
else()
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
if(EUI_USE_BUNDLED_FREETYPE)
    set(EUI_FREETYPE_DIR "${EUI_THIRD_PARTY_DIR}/freetype")
else()
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

if(EUI_ENABLE_HARFBUZZ)
    if(EUI_USE_BUNDLED_HARFBUZZ)
        set(EUI_HARFBUZZ_DIR "${EUI_THIRD_PARTY_DIR}/harfbuzz")
    else()
        eui_fetch_dependency(
            EUI_HARFBUZZ_DIR
            eui_harfbuzz
            "https://github.com/harfbuzz/harfbuzz/archive/refs/tags/8.5.0.zip"
        )
    endif()

    eui_set_third_party_option(HB_HAVE_CAIRO OFF "Disable Cairo support in bundled HarfBuzz")
    eui_set_third_party_option(HB_HAVE_GRAPHITE2 OFF "Disable Graphite2 support in bundled HarfBuzz")
    eui_set_third_party_option(HB_HAVE_GLIB OFF "Disable GLib support in bundled HarfBuzz")
    eui_set_third_party_option(HB_HAVE_GOBJECT OFF "Disable GObject support in bundled HarfBuzz")
    eui_set_third_party_option(HB_HAVE_ICU OFF "Disable ICU support in bundled HarfBuzz")
    eui_set_third_party_option(HB_HAVE_INTROSPECTION OFF "Disable introspection support in bundled HarfBuzz")
    eui_set_third_party_option(HB_BUILD_UTILS OFF "Disable HarfBuzz utility tools")
    eui_set_third_party_option(HB_BUILD_SUBSET OFF "Disable HarfBuzz subset library")
    eui_begin_quiet_third_party_config()
    add_subdirectory("${EUI_HARFBUZZ_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/_deps/harfbuzz-bundled-build" EXCLUDE_FROM_ALL)
    eui_end_quiet_third_party_config()
    if(TARGET harfbuzz AND NOT TARGET harfbuzz::harfbuzz)
        add_library(harfbuzz::harfbuzz ALIAS harfbuzz)
    endif()
    if(TARGET harfbuzz)
        eui_silence_third_party_warnings(harfbuzz)
    endif()
    if(MSVC AND TARGET harfbuzz)
        target_compile_options(harfbuzz PRIVATE /utf-8)
    endif()
    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
endif()

if(EUI_ENABLE_MARKDOWN)
    if(EUI_USE_BUNDLED_MD4C)
        set(EUI_MD4C_DIR "${EUI_THIRD_PARTY_DIR}/md4c")
    else()
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
