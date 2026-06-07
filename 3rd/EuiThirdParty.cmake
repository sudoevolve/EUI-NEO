function(eui_prepare_pkg_config_stub out_var)
    if(WIN32)
        set(stub_path "${CMAKE_CURRENT_BINARY_DIR}/eui-pkg-config-stub.bat")
        file(WRITE "${stub_path}" "@echo 0.0.0\r\n@if \"%1\"==\"--version\" exit /b 0\r\n@exit /b 1\r\n")
    else()
        set(stub_path "${CMAKE_CURRENT_BINARY_DIR}/eui-pkg-config-stub.sh")
        file(WRITE "${stub_path}" "#!/bin/sh\nif [ \"$1\" = \"--version\" ]; then echo 0.0.0; exit 0; fi\nexit 1\n")
        file(CHMOD "${stub_path}"
            PERMISSIONS
                OWNER_READ OWNER_WRITE OWNER_EXECUTE
                GROUP_READ GROUP_EXECUTE
                WORLD_READ WORLD_EXECUTE
        )
    endif()

    set(${out_var} "${stub_path}" PARENT_SCOPE)
endfunction()

function(eui_set_third_party_option name value description)
    set(${name} ${value} CACHE BOOL "${description}" FORCE)
endfunction()

macro(eui_begin_quiet_third_party_config)
    set(EUI_PREV_CMAKE_WARN_DEPRECATED "${CMAKE_WARN_DEPRECATED}")
    set(EUI_PREV_CMAKE_DISABLE_FIND_PACKAGE_PkgConfig "${CMAKE_DISABLE_FIND_PACKAGE_PkgConfig}")
    set(EUI_PREV_CMAKE_MESSAGE_LOG_LEVEL "${CMAKE_MESSAGE_LOG_LEVEL}")
    set(EUI_PREV_PKG_CONFIG_EXECUTABLE "${PKG_CONFIG_EXECUTABLE}")
    set(EUI_PREV_PkgConfig_FIND_QUIETLY "${PkgConfig_FIND_QUIETLY}")
    eui_prepare_pkg_config_stub(EUI_PKG_CONFIG_STUB)

    set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "Suppress bundled third-party CMake deprecation warnings" FORCE)
    set(CMAKE_DISABLE_FIND_PACKAGE_PkgConfig TRUE CACHE BOOL "Suppress bundled third-party pkg-config probes" FORCE)
    set(CMAKE_MESSAGE_LOG_LEVEL WARNING)
    set(PKG_CONFIG_EXECUTABLE "${EUI_PKG_CONFIG_STUB}" CACHE FILEPATH "Bundled third-party pkg-config stub" FORCE)
    set(PkgConfig_FIND_QUIETLY TRUE)
endmacro()

macro(eui_end_quiet_third_party_config)
    set(CMAKE_WARN_DEPRECATED "${EUI_PREV_CMAKE_WARN_DEPRECATED}" CACHE BOOL "Suppress deprecated functionality warnings" FORCE)
    set(CMAKE_DISABLE_FIND_PACKAGE_PkgConfig "${EUI_PREV_CMAKE_DISABLE_FIND_PACKAGE_PkgConfig}" CACHE BOOL "Disable find_package(PkgConfig)" FORCE)
    set(CMAKE_MESSAGE_LOG_LEVEL "${EUI_PREV_CMAKE_MESSAGE_LOG_LEVEL}")
    set(PKG_CONFIG_EXECUTABLE "${EUI_PREV_PKG_CONFIG_EXECUTABLE}" CACHE FILEPATH "pkg-config executable" FORCE)
    set(PkgConfig_FIND_QUIETLY "${EUI_PREV_PkgConfig_FIND_QUIETLY}")
endmacro()

macro(eui_begin_static_third_party_config)
    set(EUI_PREV_BUILD_SHARED_LIBS "${BUILD_SHARED_LIBS}")
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build bundled third-party libraries as static libraries" FORCE)
endmacro()

macro(eui_end_static_third_party_config)
    set(BUILD_SHARED_LIBS "${EUI_PREV_BUILD_SHARED_LIBS}" CACHE BOOL "Build shared libraries" FORCE)
endmacro()
