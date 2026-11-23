# 在框架 app helpers 就绪后执行；只注册本模块目录中的可选目标。
get_filename_component(plot_source_dir "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
get_target_property(plot_binary_dir eui_neo_module_plot BINARY_DIR)
if(EUI_BUILD_APPS)
    add_subdirectory("${plot_source_dir}/examples" "${plot_binary_dir}/examples")
endif()
if(EUI_BUILD_TEST_FIXTURES)
    add_subdirectory("${plot_source_dir}/tests" "${plot_binary_dir}/tests")
endif()
