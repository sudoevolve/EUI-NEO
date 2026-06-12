include_guard(GLOBAL)

function(eui_resolve_md4c)
    if(TARGET md4c::md4c)
        return()
    endif()

    find_package(md4c CONFIG QUIET)
    if(TARGET md4c::md4c)
        return()
    endif()
    if(TARGET md4c)
        add_library(md4c::md4c ALIAS md4c)
        return()
    endif()

    eui_fetch_allowed(_can_fetch EUI_FETCH_MD4C)
    if(_can_fetch)
        eui_require_cpm()
        CPMAddPackage(
            NAME eui_md4c_source
            URL https://github.com/mity/md4c/archive/refs/tags/release-0.5.3.zip
            DOWNLOAD_ONLY YES
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        add_library(eui_md4c STATIC "${eui_md4c_source_SOURCE_DIR}/src/md4c.c")
        target_include_directories(eui_md4c PUBLIC "${eui_md4c_source_SOURCE_DIR}/src")
        set_target_properties(eui_md4c PROPERTIES POSITION_INDEPENDENT_CODE ON)
        eui_silence_third_party_warnings(eui_md4c)
        add_library(md4c::md4c ALIAS eui_md4c)
        return()
    endif()

    eui_missing_dependency("MD4C" "EUI_FETCH_MD4C" "Disable Markdown parsing with -DEUI_ENABLE_MARKDOWN=OFF if it is not needed.")
endfunction()
