-- Shared build helpers for EUI-NEO's xmake build.
--
-- xmake 3.x executes target and rule callbacks (on_config, after_build, ...)
-- in isolated script scopes, so project-level global functions resolve to nil
-- inside them. Keep the implementations in this module and import() them from
-- every callback; the thin global wrappers in xmake.lua only exist so
-- pass-by-reference sites like on_config(eui_apply_compile_options) keep
-- working.

function apply_compile_options(target)
    if target:is_plat("windows") and not target:is_plat("mingw") then
        target:add("cxflags", "/utf-8")
        if not is_mode("debug") then
            target:add("cxflags", "/O1", "/GS-", "/sdl-", "/wd4819")
        end
    elseif not is_mode("debug") then
        target:add("cxxflags", "-Os", "-fno-exceptions", "-fno-rtti")
    end
end

function apply_app_link_options(target)
    if target:is_plat("mingw") then
        target:add("ldflags", "-mwindows", {force = true})
        if not is_mode("debug") then
            target:add("ldflags", "-Wl,--gc-sections", "-s")
        end
    elseif target:is_plat("windows") then
        target:add("ldflags", "/SUBSYSTEM:WINDOWS", "/ENTRY:mainCRTStartup", {force = true})
        if not is_mode("debug") then
            target:add("ldflags", "/OPT:REF", "/OPT:ICF", "/INCREMENTAL:NO", {force = true})
        end
    elseif target:is_plat("macosx") then
        if not is_mode("debug") then
            target:add("ldflags", "-Wl,-dead_strip")
        end
    elseif not is_mode("debug") then
        target:add("ldflags", "-Wl,--gc-sections", "-s")
    end
end

function compile_shadertoy(target, source, output)
    local wrapper = target:dep("eui_shadertoy_wrap")
    assert(wrapper, "missing eui_shadertoy_wrap dependency")

    import("lib.detect.find_tool")
    local search_paths = {}
    local vulkan_sdk = os.getenv("VULKAN_SDK")
    if vulkan_sdk then
        table.insert(search_paths, path.join(vulkan_sdk, "Bin"))
        table.insert(search_paths, path.join(vulkan_sdk, "bin"))
    end
    local validator = find_tool("glslangValidator", {paths = search_paths})
    assert(validator, "Vulkan SDK glslangValidator is required to generate Shadertoy SPIR-V")

    local source_path = path.absolute(source, os.projectdir())
    local output_path = path.absolute(output, target:targetdir())
    local wrapped_path = path.join(target:autogendir(), "shadertoy", path.filename(output) .. ".wrapped.frag")
    os.mkdir(path.directory(output_path))
    os.mkdir(path.directory(wrapped_path))
    os.vrunv(wrapper:targetfile(), {"--input", source_path, "--output", wrapped_path})
    os.vrunv(validator.program, {"-V", "-S", "frag", wrapped_path, "-o", output_path})
end

function compile_shadertoy_config(target, config, asset_root, output_root)
    local wrapper = target:dep("eui_shadertoy_wrap")
    assert(wrapper, "missing eui_shadertoy_wrap dependency")

    import("lib.detect.find_tool")
    local python = find_tool("python3") or find_tool("python")
    assert(python, "Python 3 is required to generate Shadertoy SPIR-V from a config")
    local validator = find_tool("glslangValidator", {paths = (function()
        local paths = {}
        local vulkan_sdk = os.getenv("VULKAN_SDK")
        if vulkan_sdk then
            table.insert(paths, path.join(vulkan_sdk, "Bin"))
            table.insert(paths, path.join(vulkan_sdk, "bin"))
        end
        return paths
    end)()})
    assert(validator, "Vulkan SDK glslangValidator is required to generate Shadertoy SPIR-V")

    local config_path = path.absolute(config, os.projectdir())
    local asset_root_path = path.absolute(asset_root, os.projectdir())
    local output_root_path = path.absolute(output_root, target:targetdir())
    local stamp = path.join(target:autogendir(), "shadertoy",
        path.basename(path.directory(config)) .. ".config.stamp")
    os.mkdir(path.directory(stamp))
    os.vrunv(python.program, {
        path.join(os.projectdir(), "scripts", "generate_shadertoy_spirv.py"),
        "--config", config_path,
        "--asset-root", asset_root_path,
        "--output-root", output_root_path,
        "--wrapper", wrapper:targetfile(),
        "--validator", validator.program,
        "--stamp", stamp
    })
end
