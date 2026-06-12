include_guard(GLOBAL)

function(eui_prepare_pkg_config_stub out_var)
    if(WIN32)
        set(_stub_path "${CMAKE_CURRENT_BINARY_DIR}/eui-pkg-config-stub.bat")
        file(WRITE "${_stub_path}" "@echo 0.0.0\r\n@if \"%1\"==\"--version\" exit /b 0\r\n@exit /b 1\r\n")
    else()
        set(_stub_path "${CMAKE_CURRENT_BINARY_DIR}/eui-pkg-config-stub.sh")
        file(WRITE "${_stub_path}" "#!/bin/sh\nif [ \"$1\" = \"--version\" ]; then echo 0.0.0; exit 0; fi\nexit 1\n")
        file(CHMOD "${_stub_path}"
            PERMISSIONS
                OWNER_READ OWNER_WRITE OWNER_EXECUTE
                GROUP_READ GROUP_EXECUTE
                WORLD_READ WORLD_EXECUTE
        )
    endif()

    set(${out_var} "${_stub_path}" PARENT_SCOPE)
endfunction()

function(eui_set_third_party_option name value description)
    if(DEFINED ${name})
        return()
    endif()
    set(${name} ${value} CACHE BOOL "${description}")
endfunction()

function(eui_fetch_allowed out_var fetch_option)
    if(EUI_DEPS_ALLOW_FETCH AND ${fetch_option})
        set(${out_var} TRUE PARENT_SCOPE)
    else()
        set(${out_var} FALSE PARENT_SCOPE)
    endif()
endfunction()

function(eui_missing_dependency dep_name fetch_option hint)
    message(FATAL_ERROR
        "${dep_name} was not found. Provide it via a parent target, toolchain/package manager, "
        "or an installed CMake package. To let EUI-NEO download it with CPM, configure with "
        "-DEUI_DEPS_ALLOW_FETCH=ON -D${fetch_option}=ON. ${hint}"
    )
endfunction()

function(eui_silence_third_party_warnings target_name)
    if(NOT TARGET ${target_name})
        return()
    endif()

    if(MSVC)
        target_compile_options(${target_name} PRIVATE /w)
    else()
        target_compile_options(${target_name} PRIVATE -w)
    endif()
endfunction()

function(eui_create_forward_target target_name link_target)
    if(TARGET ${target_name})
        return()
    endif()

    add_library(${target_name} INTERFACE IMPORTED GLOBAL)
    set_target_properties(${target_name} PROPERTIES
        INTERFACE_LINK_LIBRARIES "${link_target}"
    )
endfunction()

macro(eui_save_variable variable_name)
    if(DEFINED ${variable_name})
        set(EUI_PREV_${variable_name}_DEFINED TRUE)
        set(EUI_PREV_${variable_name}_VALUE "${${variable_name}}")
    else()
        set(EUI_PREV_${variable_name}_DEFINED FALSE)
    endif()
endmacro()

macro(eui_restore_variable variable_name)
    set(_eui_prev_defined_var "EUI_PREV_${variable_name}_DEFINED")
    set(_eui_prev_value_var "EUI_PREV_${variable_name}_VALUE")
    if(${_eui_prev_defined_var})
        set(${variable_name} "${${_eui_prev_value_var}}")
    else()
        unset(${variable_name})
    endif()
endmacro()

macro(eui_save_cache_variable variable_name)
    if(DEFINED CACHE{${variable_name}})
        set(EUI_PREV_${variable_name}_CACHE_DEFINED TRUE)
        get_property(EUI_PREV_${variable_name}_CACHE_VALUE CACHE ${variable_name} PROPERTY VALUE)
        get_property(EUI_PREV_${variable_name}_CACHE_TYPE CACHE ${variable_name} PROPERTY TYPE)
        get_property(EUI_PREV_${variable_name}_CACHE_HELPSTRING CACHE ${variable_name} PROPERTY HELPSTRING)
    else()
        set(EUI_PREV_${variable_name}_CACHE_DEFINED FALSE)
    endif()
    eui_save_variable(${variable_name})
endmacro()

macro(eui_restore_cache_variable variable_name)
    set(_eui_prev_cache_defined_var "EUI_PREV_${variable_name}_CACHE_DEFINED")
    set(_eui_prev_cache_value_var "EUI_PREV_${variable_name}_CACHE_VALUE")
    set(_eui_prev_cache_type_var "EUI_PREV_${variable_name}_CACHE_TYPE")
    set(_eui_prev_cache_help_var "EUI_PREV_${variable_name}_CACHE_HELPSTRING")
    if(${_eui_prev_cache_defined_var})
        set(${variable_name} "${${_eui_prev_cache_value_var}}" CACHE ${${_eui_prev_cache_type_var}} "${${_eui_prev_cache_help_var}}" FORCE)
    else()
        unset(${variable_name} CACHE)
    endif()
    eui_restore_variable(${variable_name})
endmacro()

macro(eui_begin_pkg_config_cache_guard)
    eui_save_cache_variable(PKG_CONFIG_EXECUTABLE)
    eui_save_cache_variable(PKG_CONFIG_ARGN)
endmacro()

macro(eui_end_pkg_config_cache_guard)
    eui_restore_cache_variable(PKG_CONFIG_ARGN)
    eui_restore_cache_variable(PKG_CONFIG_EXECUTABLE)
endmacro()

macro(eui_begin_quiet_third_party_config)
    eui_save_cache_variable(CMAKE_WARN_DEPRECATED)
    eui_save_cache_variable(CMAKE_DISABLE_FIND_PACKAGE_PkgConfig)
    eui_save_variable(CMAKE_MESSAGE_LOG_LEVEL)
    eui_begin_pkg_config_cache_guard()
    eui_save_variable(PkgConfig_FIND_QUIETLY)
    eui_prepare_pkg_config_stub(EUI_PKG_CONFIG_STUB)

    set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "Suppress bundled third-party CMake deprecation warnings" FORCE)
    set(CMAKE_DISABLE_FIND_PACKAGE_PkgConfig TRUE CACHE BOOL "Suppress bundled third-party pkg-config probes" FORCE)
    set(CMAKE_MESSAGE_LOG_LEVEL WARNING)
    set(PKG_CONFIG_EXECUTABLE "${EUI_PKG_CONFIG_STUB}" CACHE FILEPATH "Bundled third-party pkg-config stub" FORCE)
    set(PkgConfig_FIND_QUIETLY TRUE)
endmacro()

macro(eui_end_quiet_third_party_config)
    eui_restore_cache_variable(CMAKE_WARN_DEPRECATED)
    eui_restore_cache_variable(CMAKE_DISABLE_FIND_PACKAGE_PkgConfig)
    eui_restore_variable(CMAKE_MESSAGE_LOG_LEVEL)
    eui_end_pkg_config_cache_guard()
    eui_restore_variable(PkgConfig_FIND_QUIETLY)
endmacro()

macro(eui_begin_static_third_party_config)
    eui_save_cache_variable(BUILD_SHARED_LIBS)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build third-party libraries as static libraries" FORCE)
endmacro()

macro(eui_end_static_third_party_config)
    eui_restore_cache_variable(BUILD_SHARED_LIBS)
endmacro()
