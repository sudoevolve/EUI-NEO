add_library(eui_neo_module_keyboard INTERFACE)
add_library(eui::module_keyboard ALIAS eui_neo_module_keyboard)

target_link_libraries(eui_neo_module_keyboard INTERFACE eui::neo)
