# Optional EUI-NEO feature modules.
#
# A module is built only when its directory exists and contains CMakeLists.txt.
# This keeps downstream copies free to omit modules they do not ship.

set(EUI_NEO_KNOWN_MODULES
    keyboard
    media
    serial
)

foreach(module_name IN LISTS EUI_NEO_KNOWN_MODULES)
    if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/${module_name}/CMakeLists.txt")
        add_subdirectory("${module_name}")
    endif()
endforeach()

file(GLOB EUI_NEO_MODULE_DIRS
    LIST_DIRECTORIES TRUE
    RELATIVE "${CMAKE_CURRENT_LIST_DIR}"
    "${CMAKE_CURRENT_LIST_DIR}/*"
)

foreach(module_name IN LISTS EUI_NEO_MODULE_DIRS)
    if(module_name IN_LIST EUI_NEO_KNOWN_MODULES)
        continue()
    endif()
    if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/${module_name}/CMakeLists.txt")
        add_subdirectory("${module_name}")
    endif()
endforeach()
