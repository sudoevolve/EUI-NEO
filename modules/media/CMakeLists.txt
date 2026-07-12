add_library(eui_neo_module_media INTERFACE)
add_library(eui::module_media ALIAS eui_neo_module_media)

target_link_libraries(eui_neo_module_media INTERFACE eui::neo)
