include_guard(GLOBAL)

function(eui_resolve_sdl2)
    if(TARGET SDL2::SDL2)
        return()
    endif()

    find_package(SDL2 CONFIG QUIET)
    if(TARGET SDL2::SDL2)
        return()
    endif()
    if(TARGET SDL2::SDL2-static)
        eui_create_forward_target(SDL2::SDL2 SDL2::SDL2-static)
        return()
    endif()

    eui_fetch_allowed(_can_fetch EUI_FETCH_SDL2)
    if(_can_fetch)
        eui_require_cpm()
        eui_set_third_party_option(SDL_SHARED OFF "Build a shared version of SDL2")
        eui_set_third_party_option(SDL_STATIC ON "Build a static version of SDL2")
        eui_set_third_party_option(SDL_TEST OFF "Build the SDL2_test library")
        eui_set_third_party_option(SDL_TESTS OFF "Build the SDL2 test programs")
        eui_set_third_party_option(SDL2_DISABLE_INSTALL ON "Disable installation of SDL2")
        eui_set_third_party_option(SDL2_DISABLE_UNINSTALL ON "Disable uninstallation of SDL2")
        CPMAddPackage(
            NAME SDL2
            URL https://www.libsdl.org/release/SDL2-2.32.10.tar.gz
            URL_HASH SHA256=5f5993c530f084535c65a6879e9b26ad441169b3e25d789d83287040a9ca5165
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        if(TARGET SDL2::SDL2)
            return()
        endif()
        if(TARGET SDL2::SDL2-static)
            eui_create_forward_target(SDL2::SDL2 SDL2::SDL2-static)
            return()
        endif()
    endif()

    eui_missing_dependency("SDL2" "EUI_FETCH_SDL2" "SDL2 is required only when EUI_WINDOW_BACKEND=sdl2.")
endfunction()
