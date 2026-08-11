include(CMakeParseArguments)

set(EUI_SHADERTOY_SCRIPT_DIR "${CMAKE_CURRENT_LIST_DIR}")

function(eui_compile_shadertoy target_name)
    set(one_value_args SOURCE OUTPUT)
    set(multi_value_args UNIFORMS)
    cmake_parse_arguments(EUI_ST "" "${one_value_args}" "${multi_value_args}" ${ARGN})

    if(NOT TARGET ${target_name})
        message(FATAL_ERROR "eui_compile_shadertoy target does not exist: ${target_name}")
    endif()
    if(NOT EUI_ST_SOURCE OR NOT EUI_ST_OUTPUT)
        message(FATAL_ERROR "eui_compile_shadertoy requires SOURCE and OUTPUT")
    endif()
    if(EUI_ST_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "eui_compile_shadertoy received unsupported arguments: ${EUI_ST_UNPARSED_ARGUMENTS}")
    endif()
    if(TARGET eui_shadertoy_wrap)
        set(wrapper_target eui_shadertoy_wrap)
    elseif(TARGET eui::shadertoy_wrap)
        set(wrapper_target eui::shadertoy_wrap)
    else()
        message(FATAL_ERROR "Shadertoy SPIR-V generation is available only in a Vulkan build")
    endif()
    if(NOT Vulkan_GLSLANG_VALIDATOR_EXECUTABLE)
        message(FATAL_ERROR "Vulkan SDK glslangValidator is required to generate Shadertoy SPIR-V")
    endif()

    get_filename_component(source "${EUI_ST_SOURCE}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    get_filename_component(output "${EUI_ST_OUTPUT}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}")
    get_filename_component(output_dir "${output}" DIRECTORY)
    string(MD5 shader_id "${target_name}:${output}")
    set(wrapped_dir "${CMAKE_CURRENT_BINARY_DIR}/shadertoy-wrapped")
    set(wrapped "${wrapped_dir}/${shader_id}.frag")
    set(uniform_arguments)
    foreach(uniform IN LISTS EUI_ST_UNIFORMS)
        list(APPEND uniform_arguments --uniform "${uniform}")
    endforeach()
    add_custom_command(
        OUTPUT "${output}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${output_dir}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${wrapped_dir}"
        COMMAND $<TARGET_FILE:${wrapper_target}>
                --input "${source}"
                --output "${wrapped}"
                ${uniform_arguments}
        COMMAND "${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE}"
                -V -S frag "${wrapped}" -o "${output}"
        DEPENDS "${source}" ${wrapper_target}
        COMMENT "Compiling Shadertoy SPIR-V: ${source}"
        VERBATIM
    )
    set(shader_target "${target_name}_shadertoy_${shader_id}")
    add_custom_target(${shader_target} DEPENDS "${output}")
    add_dependencies(${target_name} ${shader_target})
endfunction()

function(eui_compile_shadertoy_inline target_name)
    set(one_value_args CONTENT OUTPUT)
    set(multi_value_args UNIFORMS)
    cmake_parse_arguments(PARSE_ARGV 1 EUI_ST ""
        "${one_value_args}" "${multi_value_args}")
    if(NOT DEFINED EUI_ST_CONTENT OR NOT EUI_ST_OUTPUT)
        message(FATAL_ERROR "eui_compile_shadertoy_inline requires CONTENT and OUTPUT")
    endif()
    if(EUI_ST_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "eui_compile_shadertoy_inline received unsupported arguments: ${EUI_ST_UNPARSED_ARGUMENTS}")
    endif()

    string(MD5 source_id "${target_name}:${EUI_ST_CONTENT}")
    set(inline_source
        "${CMAKE_CURRENT_BINARY_DIR}/shadertoy-inline/${source_id}.frag")
    get_filename_component(inline_source_dir "${inline_source}" DIRECTORY)
    file(MAKE_DIRECTORY "${inline_source_dir}")
    file(WRITE "${inline_source}" "${EUI_ST_CONTENT}")
    eui_compile_shadertoy(${target_name}
        SOURCE "${inline_source}"
        OUTPUT "${EUI_ST_OUTPUT}"
        UNIFORMS ${EUI_ST_UNIFORMS}
    )
endfunction()

function(eui_compile_shadertoy_config target_name)
    set(one_value_args CONFIG ASSET_ROOT OUTPUT_ROOT)
    cmake_parse_arguments(PARSE_ARGV 1 EUI_ST_CONFIG ""
                          "${one_value_args}" "")

    if(NOT TARGET ${target_name})
        message(FATAL_ERROR "eui_compile_shadertoy_config target does not exist: ${target_name}")
    endif()
    if(NOT EUI_ST_CONFIG_CONFIG OR NOT EUI_ST_CONFIG_ASSET_ROOT OR
       NOT EUI_ST_CONFIG_OUTPUT_ROOT)
        message(FATAL_ERROR
            "eui_compile_shadertoy_config requires CONFIG, ASSET_ROOT and OUTPUT_ROOT")
    endif()
    if(NOT Python3_Interpreter_FOUND OR NOT Python3_EXECUTABLE)
        find_package(Python3 COMPONENTS Interpreter QUIET)
    endif()
    if(NOT Python3_Interpreter_FOUND OR NOT Python3_EXECUTABLE)
        message(FATAL_ERROR
            "Python 3 is required to generate Shadertoy SPIR-V from a config")
    endif()
    if(TARGET eui_shadertoy_wrap)
        set(wrapper_target eui_shadertoy_wrap)
    elseif(TARGET eui::shadertoy_wrap)
        set(wrapper_target eui::shadertoy_wrap)
    else()
        message(FATAL_ERROR "Shadertoy SPIR-V generation is available only in a Vulkan build")
    endif()
    if(NOT Vulkan_GLSLANG_VALIDATOR_EXECUTABLE)
        message(FATAL_ERROR "Vulkan SDK glslangValidator is required to generate Shadertoy SPIR-V")
    endif()

    get_filename_component(config "${EUI_ST_CONFIG_CONFIG}" ABSOLUTE
                           BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    get_filename_component(asset_root "${EUI_ST_CONFIG_ASSET_ROOT}" ABSOLUTE
                           BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    get_filename_component(output_root "${EUI_ST_CONFIG_OUTPUT_ROOT}" ABSOLUTE
                           BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}")
    get_filename_component(config_dir "${config}" DIRECTORY)
    file(GLOB_RECURSE config_sources CONFIGURE_DEPENDS
         "${config_dir}/*.frag" "${config_dir}/*.json" "${config}")
    string(MD5 config_id "${target_name}:${config}:${output_root}")
    set(stamp "${CMAKE_CURRENT_BINARY_DIR}/shadertoy-config/${config_id}.stamp")
    add_custom_command(
        OUTPUT "${stamp}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${output_root}"
        COMMAND "${Python3_EXECUTABLE}"
                "${EUI_SHADERTOY_SCRIPT_DIR}/generate_shadertoy_spirv.py"
                --config "${config}"
                --asset-root "${asset_root}"
                --output-root "${output_root}"
                --wrapper "$<TARGET_FILE:${wrapper_target}>"
                --validator "${Vulkan_GLSLANG_VALIDATOR_EXECUTABLE}"
                --stamp "${stamp}"
        DEPENDS ${config_sources} ${wrapper_target}
                "${EUI_SHADERTOY_SCRIPT_DIR}/generate_shadertoy_spirv.py"
        COMMENT "Compiling Shadertoy graph: ${config}"
        VERBATIM
    )
    set(shader_target "${target_name}_shadertoy_config_${config_id}")
    add_custom_target(${shader_target} DEPENDS "${stamp}")
    add_dependencies(${target_name} ${shader_target})
endfunction()
