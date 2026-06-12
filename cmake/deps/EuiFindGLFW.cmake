include_guard(GLOBAL)

function(eui_resolve_glfw)
    if(TARGET glfw)
        return()
    endif()

    find_package(glfw3 CONFIG QUIET)
    if(TARGET glfw)
        return()
    endif()
    if(TARGET glfw3 AND NOT TARGET glfw)
        add_library(glfw ALIAS glfw3)
        return()
    endif()

    eui_fetch_allowed(_can_fetch EUI_FETCH_GLFW)
    if(_can_fetch)
        eui_require_cpm()
        eui_set_third_party_option(GLFW_BUILD_EXAMPLES OFF "Build the GLFW example programs")
        eui_set_third_party_option(GLFW_BUILD_TESTS OFF "Build the GLFW test programs")
        eui_set_third_party_option(GLFW_BUILD_DOCS OFF "Build the GLFW documentation")
        eui_set_third_party_option(GLFW_INSTALL OFF "Generate installation target")
        eui_begin_static_third_party_config()
        CPMAddPackage(
            NAME glfw
            URL https://github.com/glfw/glfw/archive/refs/tags/3.4.zip
            URL_HASH SHA256=a133ddc3d3c66143eba9035621db8e0bcf34dba1ee9514a9e23e96afd39fd57a
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        eui_end_static_third_party_config()
        if(TARGET glfw)
            return()
        endif()
    endif()

    eui_missing_dependency("GLFW" "EUI_FETCH_GLFW" "The default window backend requires GLFW.")
endfunction()
