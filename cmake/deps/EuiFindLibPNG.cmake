include_guard(GLOBAL)

function(eui_resolve_png)
    if(TARGET PNG::PNG)
        return()
    endif()

    find_package(PNG QUIET)
    if(TARGET PNG::PNG)
        return()
    endif()

    eui_fetch_allowed(_can_fetch EUI_FETCH_LIBPNG)
    if(_can_fetch)
        eui_resolve_zlib()
        eui_require_cpm()
        eui_set_third_party_option(PNG_SHARED OFF "Build libpng as a shared library")
        eui_set_third_party_option(PNG_STATIC ON "Build libpng as a static library")
        eui_set_third_party_option(PNG_FRAMEWORK OFF "Build libpng as a framework bundle")
        eui_set_third_party_option(PNG_TESTS OFF "Build the libpng tests")
        eui_set_third_party_option(PNG_TOOLS OFF "Build the libpng tools")
        eui_set_third_party_option(PNG_EXECUTABLES ON "Deprecated libpng tools option")
        eui_set_third_party_option(PNG_BUILD_ZLIB OFF "Use find_package(ZLIB) for libpng")
        eui_set_third_party_option(PNG_HARDWARE_OPTIMIZATIONS OFF "Disable libpng hardware optimizations")
        eui_set_third_party_option(SKIP_INSTALL_ALL ON "Disable install rules for third-party builds")
        CPMAddPackage(
            NAME eui_libpng_source
            URL https://github.com/pnggroup/libpng/archive/refs/tags/v1.6.43.zip
            DOWNLOAD_ONLY YES
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        eui_begin_quiet_third_party_config()
        add_subdirectory("${eui_libpng_source_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/_deps/libpng-build" EXCLUDE_FROM_ALL)
        eui_end_quiet_third_party_config()
        if(TARGET png_static)
            target_include_directories(png_static PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/_deps/libpng-build>")
        endif()
        if(TARGET png_static AND NOT TARGET PNG::PNG)
            add_library(PNG::PNG ALIAS png_static)
        endif()
        if(TARGET PNG::PNG)
            return()
        endif()
    endif()

    eui_missing_dependency("libpng" "EUI_FETCH_LIBPNG" "libpng is required for FreeType PNG support.")
endfunction()
