add_library(eui_neo_module_serial INTERFACE)
add_library(eui::module_serial ALIAS eui_neo_module_serial)

target_link_libraries(eui_neo_module_serial INTERFACE eui::neo)
