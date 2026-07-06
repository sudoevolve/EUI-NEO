if(NOT DEFINED EUI_NEO_ROOT)
    get_filename_component(EUI_NEO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
endif()

if(NOT DEFINED EUI_ANDROID_SDL2_DIR OR NOT EXISTS "${EUI_ANDROID_SDL2_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "EUI_ANDROID_SDL2_DIR must point to an SDL2 source directory.")
endif()

if(NOT DEFINED EUI_ANDROID_APP_SOURCE)
    set(EUI_ANDROID_APP_SOURCE "${CMAKE_CURRENT_LIST_DIR}/../app/src/main/cpp/android_app.cpp")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SDL_SHARED ON CACHE BOOL "Build SDL2 shared library for Android" FORCE)
set(SDL_STATIC OFF CACHE BOOL "Do not build SDL2 static library for Android" FORCE)
set(SDL_TEST OFF CACHE BOOL "Disable SDL2 tests" FORCE)
set(SDL_TESTS OFF CACHE BOOL "Disable SDL2 test programs" FORCE)
set(SDL2_DISABLE_INSTALL ON CACHE BOOL "Disable SDL2 install rules" FORCE)
set(SDL2_DISABLE_UNINSTALL ON CACHE BOOL "Disable SDL2 uninstall rules" FORCE)
add_subdirectory("${EUI_ANDROID_SDL2_DIR}" "${CMAKE_BINARY_DIR}/sdl2" EXCLUDE_FROM_ALL)

set(EUI_WINDOW_BACKEND sdl2 CACHE STRING "Android uses SDL2" FORCE)
set(EUI_RENDER_BACKEND vulkan CACHE STRING "Android MVP uses Vulkan" FORCE)
set(EUI_DEPS_MODE auto CACHE STRING "Use predeclared SDL2 and bundled/fetched EUI deps" FORCE)
set(EUI_BUILD_APPS OFF CACHE BOOL "Do not build desktop examples for Android" FORCE)
set(EUI_BUILD_USER_APPS OFF CACHE BOOL "Do not auto-build desktop user apps for Android" FORCE)
set(EUI_ENABLE_INSTALL OFF CACHE BOOL "Disable install rules for Android" FORCE)

add_subdirectory("${EUI_NEO_ROOT}" "${CMAKE_BINARY_DIR}/eui-neo" EXCLUDE_FROM_ALL)

add_library(main SHARED
    "${EUI_NEO_ROOT}/core/app/sdl2_app_main.cpp"
    "${EUI_ANDROID_APP_SOURCE}"
)

target_include_directories(main PRIVATE "${EUI_NEO_ROOT}")
target_link_libraries(main PRIVATE eui::neo SDL2::SDL2)
eui_neo_configure_app(main)
