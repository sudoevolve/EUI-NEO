# 由模块自己维护公开 API 与交付材料，核心不列举 plot 的内部文件。
install(FILES axes.h contour.h data.h export.h field.h figure.h geometry.h gpu_scene3d.h
    plot.h plot3d.h renderer.h scene3d.h session.h vector.h volume.h waveform.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/eui-neo/modules/plot)

set(plot_install_dir "${CMAKE_INSTALL_DATADIR}/eui-neo/modules/plot")
install(FILES README.md DESTINATION "${plot_install_dir}")
install(DIRECTORY docs/ DESTINATION "${plot_install_dir}/docs"
    FILES_MATCHING PATTERN "*.md" PATTERN "*.json")
install(DIRECTORY examples/ DESTINATION "${plot_install_dir}/examples"
    FILES_MATCHING PATTERN "*.cpp" PATTERN "*.h" PATTERN "CMakeLists.txt")
install(DIRECTORY licenses/ DESTINATION "${plot_install_dir}/licenses"
    FILES_MATCHING PATTERN "*.txt" PATTERN "*.md")

get_filename_component(plot_framework_root "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(plot_dependency_notices "${CMAKE_INSTALL_DATADIR}/eui-neo/licenses")
install(FILES "${plot_framework_root}/LICENSE" DESTINATION "${plot_dependency_notices}")
install(FILES "${plot_framework_root}/docs/guides/components.md" "${plot_framework_root}/docs/architecture/rendering.md"
    "${plot_framework_root}/docs/guides/dynamic-textures.md"
    DESTINATION ${CMAKE_INSTALL_DATADIR}/eui-neo/docs)
install(FILES "${EUI_FREETYPE_DIR}/LICENSE.TXT" DESTINATION "${plot_dependency_notices}/freetype")
install(FILES licenses/freetype-FTL.txt DESTINATION "${plot_dependency_notices}/freetype" RENAME FTL.TXT)
install(FILES "${EUI_FREETYPE_DIR}/src/bdf/README" DESTINATION "${plot_dependency_notices}/freetype" RENAME BDF-README)
install(FILES "${EUI_FREETYPE_DIR}/src/pcf/README" DESTINATION "${plot_dependency_notices}/freetype" RENAME PCF-README)
install(FILES "${plot_framework_root}/3rd/yyjson-0.12.0/LICENSE" DESTINATION "${plot_dependency_notices}/yyjson")
install(FILES "${EUI_ZLIB_DIR}/LICENSE" DESTINATION "${plot_dependency_notices}/zlib")
install(FILES "${EUI_LIBPNG_DIR}/LICENSE" DESTINATION "${plot_dependency_notices}/libpng")
install(FILES "${TRAY_INCLUDE_DIR}/LICENSE" DESTINATION "${plot_dependency_notices}/tray")
if(EUI_OWNS_GLFW_TARGET AND TARGET glfw)
    install(FILES "${EUI_GLFW_DIR}/LICENSE.md" DESTINATION "${plot_dependency_notices}/glfw")
endif()
if(TARGET eui_md4c)
    install(FILES "${EUI_MD4C_DIR}/LICENSE.md" DESTINATION "${plot_dependency_notices}/md4c")
endif()
