option(EUI_ENABLE_PLOT "Build the optional scientific plotting module." ON)
# 关闭模块时也不让顶层把私有实现、测试和示例头文件装进 SDK。
set_property(GLOBAL APPEND PROPERTY EUI_NEO_MODULE_OWN_HEADER_DIRS plot)
if(NOT EUI_ENABLE_PLOT)
    return()
endif()

add_library(eui_neo_module_plot data.cpp waveform.cpp axes.cpp field.cpp contour.cpp geometry.cpp vector.cpp export.cpp renderer.cpp plot.cpp figure.cpp
    scene3d.cpp volume.cpp plot3d.cpp gpu_scene3d.cpp)
add_library(eui::module_plot ALIAS eui_neo_module_plot)
set_target_properties(eui_neo_module_plot PROPERTIES EXPORT_NAME module_plot)
target_link_libraries(eui_neo_module_plot PUBLIC eui::neo)
target_link_libraries(eui_neo_module_plot PRIVATE PNG::PNG)
eui_apply_compile_options(eui_neo_module_plot)
# Public APIs report invalid data and resource limits through C++ exceptions, including in consumers
# whose application helper otherwise enables the core's exception-free Release policy.
if(MSVC)
    target_compile_options(eui_neo_module_plot PUBLIC $<$<COMPILE_LANGUAGE:CXX>:/EHsc> $<$<COMPILE_LANGUAGE:CXX>:/utf-8>)
else()
    target_compile_options(eui_neo_module_plot PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-fexceptions>)
endif()

set_property(GLOBAL APPEND PROPERTY EUI_NEO_MODULE_BUILD_SCRIPTS
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Build.cmake")

if(EUI_ENABLE_INSTALL)
    install(TARGETS eui_neo_module_plot EXPORT EuiNeoTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    include(cmake/Install.cmake)
endif()
