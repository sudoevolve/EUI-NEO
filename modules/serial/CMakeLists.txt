add_library(eui_neo_module_serial
    serial.h
    serial.cpp
)
add_library(eui::module_serial ALIAS eui_neo_module_serial)
set_target_properties(eui_neo_module_serial PROPERTIES EXPORT_NAME module_serial)

target_link_libraries(eui_neo_module_serial PUBLIC eui::neo)
eui_apply_compile_options(eui_neo_module_serial)

if(EUI_ENABLE_INSTALL)
    install(TARGETS eui_neo_module_serial
        EXPORT EuiNeoTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()
