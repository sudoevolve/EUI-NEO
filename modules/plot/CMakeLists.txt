option(EUI_ENABLE_PLOT "Build the optional scientific plotting module." ON)
if(NOT EUI_ENABLE_PLOT)
    return()
endif()

add_library(eui_neo_module_plot data.cpp axes.cpp field.cpp contour.cpp geometry.cpp vector.cpp export.cpp renderer.cpp plot.cpp figure.cpp)
add_library(eui::module_plot ALIAS eui_neo_module_plot)
set_target_properties(eui_neo_module_plot PROPERTIES EXPORT_NAME module_plot)
target_link_libraries(eui_neo_module_plot PUBLIC eui::neo)
target_link_libraries(eui_neo_module_plot PRIVATE PNG::PNG)
eui_apply_compile_options(eui_neo_module_plot)

if(EUI_ENABLE_INSTALL)
    install(TARGETS eui_neo_module_plot EXPORT EuiNeoTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
