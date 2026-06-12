include_guard(GLOBAL)

function(eui_make_tray_target include_dir)
    if(NOT TARGET eui::tray)
        add_library(eui_tray INTERFACE)
        target_include_directories(eui_tray INTERFACE "${include_dir}")
        add_library(eui::tray ALIAS eui_tray)
    endif()
endfunction()

function(eui_resolve_tray)
    if(TARGET eui::tray)
        return()
    endif()

    set(EUI_TRAY_INCLUDE_DIR "" CACHE PATH "Directory containing tray.h.")
    if(EUI_TRAY_INCLUDE_DIR AND NOT EXISTS "${EUI_TRAY_INCLUDE_DIR}/tray.h")
        message(STATUS "Ignoring stale EUI_TRAY_INCLUDE_DIR: ${EUI_TRAY_INCLUDE_DIR}")
        unset(EUI_TRAY_INCLUDE_DIR CACHE)
        unset(EUI_TRAY_INCLUDE_DIR)
    endif()

    if(EUI_TRAY_INCLUDE_DIR AND EXISTS "${EUI_TRAY_INCLUDE_DIR}/tray.h")
        eui_make_tray_target("${EUI_TRAY_INCLUDE_DIR}")
        return()
    endif()

    find_path(EUI_TRAY_INCLUDE_DIR tray.h)
    if(EUI_TRAY_INCLUDE_DIR AND EXISTS "${EUI_TRAY_INCLUDE_DIR}/tray.h")
        eui_make_tray_target("${EUI_TRAY_INCLUDE_DIR}")
        return()
    endif()

    eui_fetch_allowed(_can_fetch EUI_FETCH_TRAY)
    if(_can_fetch)
        eui_require_cpm()
        CPMAddPackage(
            NAME tray
            URL https://github.com/zserge/tray/archive/8dd1358b92562faf7c032cf5362fa97cbc7e13e9.zip
            DOWNLOAD_ONLY YES
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        eui_make_tray_target("${tray_SOURCE_DIR}")
        return()
    endif()

    eui_missing_dependency("tray" "EUI_FETCH_TRAY" "tray.h is required for Windows tray support and Linux AppIndicator tray support.")
endfunction()
