include_guard(GLOBAL)

set(EUI_CPM_BOOTSTRAP_URL
    "https://github.com/cpm-cmake/CPM.cmake/releases/latest/download/get_cpm.cmake"
    CACHE STRING "URL used to bootstrap CPM.cmake when dependency fetching is enabled."
)
set(EUI_CPM_BOOTSTRAP_FILE
    "${CMAKE_BINARY_DIR}/_deps/cpm/get_cpm.cmake"
    CACHE FILEPATH "Local path for the downloaded CPM bootstrap script."
)

function(eui_require_cpm)
    if(COMMAND CPMAddPackage)
        return()
    endif()

    get_filename_component(_eui_cpm_dir "${EUI_CPM_BOOTSTRAP_FILE}" DIRECTORY)
    file(MAKE_DIRECTORY "${_eui_cpm_dir}")
    if(NOT EXISTS "${EUI_CPM_BOOTSTRAP_FILE}")
        message(STATUS "Downloading CPM.cmake bootstrap: ${EUI_CPM_BOOTSTRAP_URL}")
        file(DOWNLOAD
            "${EUI_CPM_BOOTSTRAP_URL}"
            "${EUI_CPM_BOOTSTRAP_FILE}"
            TLS_VERIFY ON
            STATUS _eui_cpm_status
        )
        list(GET _eui_cpm_status 0 _eui_cpm_status_code)
        list(GET _eui_cpm_status 1 _eui_cpm_status_message)
        if(NOT _eui_cpm_status_code EQUAL 0)
            file(REMOVE "${EUI_CPM_BOOTSTRAP_FILE}")
            message(FATAL_ERROR
                "Failed to download CPM.cmake bootstrap: ${_eui_cpm_status_message}"
            )
        endif()
    endif()

    include("${EUI_CPM_BOOTSTRAP_FILE}")
endfunction()
