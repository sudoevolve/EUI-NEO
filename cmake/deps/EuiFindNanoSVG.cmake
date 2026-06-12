include_guard(GLOBAL)

function(eui_make_nanosvg_targets include_dir)
    if(NOT TARGET NanoSVG::nanosvg)
        add_library(eui_nanosvg INTERFACE)
        target_include_directories(eui_nanosvg INTERFACE "${include_dir}")
        add_library(NanoSVG::nanosvg ALIAS eui_nanosvg)
    endif()
    if(NOT TARGET NanoSVG::nanosvgrast)
        add_library(eui_nanosvgrast INTERFACE)
        target_include_directories(eui_nanosvgrast INTERFACE "${include_dir}")
        add_library(NanoSVG::nanosvgrast ALIAS eui_nanosvgrast)
    endif()
endfunction()

function(eui_resolve_nanosvg)
    if(TARGET NanoSVG::nanosvg AND TARGET NanoSVG::nanosvgrast)
        return()
    endif()

    find_package(NanoSVG CONFIG QUIET)
    if(TARGET NanoSVG::nanosvg AND TARGET NanoSVG::nanosvgrast)
        return()
    endif()

    if(EUI_NANOSVG_INCLUDE_DIR AND NOT EXISTS "${EUI_NANOSVG_INCLUDE_DIR}/nanosvg.h")
        message(STATUS "Ignoring stale EUI_NANOSVG_INCLUDE_DIR: ${EUI_NANOSVG_INCLUDE_DIR}")
        unset(EUI_NANOSVG_INCLUDE_DIR CACHE)
        unset(EUI_NANOSVG_INCLUDE_DIR)
    endif()

    find_path(EUI_NANOSVG_INCLUDE_DIR nanosvg.h)
    if(EUI_NANOSVG_INCLUDE_DIR AND EXISTS "${EUI_NANOSVG_INCLUDE_DIR}/nanosvg.h")
        eui_make_nanosvg_targets("${EUI_NANOSVG_INCLUDE_DIR}")
        return()
    endif()

    eui_fetch_allowed(_can_fetch EUI_FETCH_NANOSVG)
    if(_can_fetch)
        eui_require_cpm()
        CPMAddPackage(
            NAME nanosvg
            GITHUB_REPOSITORY memononen/nanosvg
            GIT_TAG "48120e91e64b2f409ed600cdfd6d790a49ba11ab"
            DOWNLOAD_ONLY YES
        )
        eui_make_nanosvg_targets("${nanosvg_SOURCE_DIR}/src")
        return()
    endif()

    eui_missing_dependency("NanoSVG" "EUI_FETCH_NANOSVG" "NanoSVG is required for SVG image loading.")
endfunction()
