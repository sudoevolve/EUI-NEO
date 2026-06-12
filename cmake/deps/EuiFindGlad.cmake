include_guard(GLOBAL)

function(eui_resolve_glad)
    if(TARGET glad)
        return()
    endif()

    set(_glad_dir "${EUI_THIRD_PARTY_DIR}/glad")
    if(NOT EXISTS "${_glad_dir}/src/glad.c")
        message(FATAL_ERROR "Local glad source is missing at ${_glad_dir}. Restore 3rd/glad.")
    endif()
    add_subdirectory("${_glad_dir}" "${CMAKE_CURRENT_BINARY_DIR}/_deps/glad-local-build" EXCLUDE_FROM_ALL)
endfunction()
