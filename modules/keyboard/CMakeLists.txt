add_library(eui_neo_module_keyboard INTERFACE)
add_library(eui::module_keyboard ALIAS eui_neo_module_keyboard)
set_target_properties(eui_neo_module_keyboard PROPERTIES EXPORT_NAME module_keyboard)

target_link_libraries(eui_neo_module_keyboard INTERFACE eui::neo)

if(EUI_ENABLE_INSTALL)
    install(TARGETS eui_neo_module_keyboard EXPORT EuiNeoTargets)
endif()
