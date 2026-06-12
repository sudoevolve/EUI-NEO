include_guard(GLOBAL)

function(eui_resolve_freetype)
    if(TARGET Freetype::Freetype)
        return()
    endif()

    find_package(Freetype QUIET)
    if(TARGET Freetype::Freetype)
        return()
    endif()

    eui_fetch_allowed(_can_fetch EUI_FETCH_FREETYPE)
    if(_can_fetch)
        eui_resolve_png()
        eui_require_cpm()
        eui_set_third_party_option(FT_DISABLE_ZLIB ON "Disable zlib support in FreeType")
        eui_set_third_party_option(FT_DISABLE_BZIP2 ON "Disable bzip2 support in FreeType")
        eui_set_third_party_option(FT_DISABLE_PNG OFF "Disable png support in FreeType")
        eui_set_third_party_option(FT_REQUIRE_PNG ON "Require png support in FreeType")
        eui_set_third_party_option(FT_DISABLE_HARFBUZZ ON "Disable HarfBuzz support in FreeType")
        eui_set_third_party_option(FT_DISABLE_BROTLI ON "Disable brotli support in FreeType")
        CPMAddPackage(
            NAME eui_freetype_source
            URL https://download.savannah.gnu.org/releases/freetype/freetype-2.13.3.tar.xz
            DOWNLOAD_ONLY YES
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        eui_begin_quiet_third_party_config()
        add_subdirectory("${eui_freetype_source_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/_deps/freetype-build" EXCLUDE_FROM_ALL)
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
        if(TARGET Freetype::Freetype)
            return()
        endif()
    endif()

    eui_missing_dependency("FreeType" "EUI_FETCH_FREETYPE" "FreeType is required for text rendering.")
endfunction()
