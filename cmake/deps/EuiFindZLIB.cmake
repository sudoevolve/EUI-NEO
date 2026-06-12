include_guard(GLOBAL)

function(eui_resolve_zlib)
    if(TARGET ZLIB::ZLIB)
        return()
    endif()

    find_package(ZLIB QUIET)
    if(TARGET ZLIB::ZLIB)
        return()
    endif()

    eui_fetch_allowed(_can_fetch EUI_FETCH_ZLIB)
    if(_can_fetch)
        eui_require_cpm()
        eui_set_third_party_option(ZLIB_BUILD_EXAMPLES OFF "Enable zlib examples")
        eui_set_third_party_option(SKIP_INSTALL_ALL ON "Disable install rules for third-party builds")
        CPMAddPackage(
            NAME eui_zlib_source
            URL https://github.com/madler/zlib/archive/refs/tags/v1.3.1.zip
            DOWNLOAD_ONLY YES
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        eui_begin_quiet_third_party_config()
        add_subdirectory("${eui_zlib_source_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/_deps/zlib-build" EXCLUDE_FROM_ALL)
        eui_end_quiet_third_party_config()
        if(TARGET zlibstatic)
            set_target_properties(zlibstatic PROPERTIES POSITION_INDEPENDENT_CODE ON)
            eui_silence_third_party_warnings(zlibstatic)
        endif()
        if(TARGET zlibstatic AND NOT TARGET ZLIB::ZLIB)
            add_library(ZLIB::ZLIB ALIAS zlibstatic)
        endif()
        if(TARGET ZLIB::ZLIB)
            return()
        endif()
    endif()

    eui_missing_dependency("zlib" "EUI_FETCH_ZLIB" "zlib is required for PNG/FreeType dependency resolution.")
endfunction()
