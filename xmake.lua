-- xmake.lua for EUI-NEO
-- A cross-platform, high-performance, low-overhead C++17 GPUI framework.
-- Converted from CMakeLists.txt to an xmake build.
-- Quick start:
--   xmake f -m release --apps=y --user_apps=y && xmake build gallery
--   xmake run gallery
-- Backend selection:
--   xmake f --window_backend=sdl2 --render_backend=vulkan
--   xmake f --shared=y

set_project("EUI-NEO")
set_version("0.5.6")
set_xmakever("2.9.0")
set_languages("c11", "cxx17")
set_config("builddir", ".xmake/build")

option("window_backend")
    set_default("glfw")
    set_values("glfw", "sdl2")
    set_showmenu(true)
    set_description("Window backend: glfw or sdl2")
option_end()

option("render_backend")
    set_default("opengl")
    set_values("auto", "opengl", "vulkan")
    set_showmenu(true)
    set_description("Render backend: auto, opengl, or vulkan")
option_end()

option("shared")
    set_default(false)
    set_showmenu(true)
    set_description("Build eui_neo as a shared library instead of a static library.")
option_end()

option("apps")
    set_default(true)
    set_showmenu(true)
    set_description("Build bundled EUI-NEO example applications.")
option_end()

option("user_apps")
    set_default(true)
    set_showmenu(true)
    set_description("Build user applications from apps/.")
option_end()

option("modules")
    set_default(true)
    set_showmenu(true)
    set_description("Build optional EUI-NEO modules when their directories are present.")
option_end()

option("markdown")
    set_default(true)
    set_showmenu(true)
    set_description("Enable MD4C Markdown parsing support.")
option_end()

option("vulkan_low_latency")
    set_default(false)
    set_showmenu(true)
    set_description("Prefer low-latency Vulkan presentation when available.")
option_end()

function eui_apply_compile_options(target)
    if not is_mode("debug") then
        target:add("defines", "NDEBUG")
    end
    if target:is_plat("windows") and not target:is_plat("mingw") then
        target:add("cxflags", "/utf-8")
        if not is_mode("debug") then
            target:add("cxflags", "/O1", "/GS-", "/sdl-", "/wd4819")
        end
    elseif not is_mode("debug") then
        target:add("cxxflags", "-Os", "-fno-exceptions", "-fno-rtti")
    end
end

function eui_apply_app_options(target)
    if not is_mode("debug") then
        target:add("defines", "NDEBUG")
    end
    if target:is_plat("windows") and not target:is_plat("mingw") then
        target:add("cxflags", "/utf-8")
        if not is_mode("debug") then
            target:add("cxflags", "/O1", "/GS-", "/sdl-", "/wd4819")
        end
    elseif not is_mode("debug") then
        target:add("cxxflags", "-Os", "-fno-exceptions", "-fno-rtti")
    end

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

    if target:is_plat("windows") and os.exists("assets/icon.ico") then
        local icon_resource = path.join(target:autogendir(), "eui_app_icon.rc")
        local icon_path = path.absolute("assets/icon.ico", os.projectdir()):gsub("\\", "/")
        os.mkdir(path.directory(icon_resource))
        local icon_file = io.open(icon_resource, "w")
        icon_file:write('IDI_APP_ICON ICON "' .. icon_path .. '"\n')
        icon_file:close()
        target:add("files", icon_resource)
    end
end

-- =============================================================================
-- Resolve backends
-- =============================================================================

local render_backend = get_config("render_backend") or "opengl"
if render_backend == "auto" then
    if find_package("vulkan") then
        render_backend = "vulkan"
    else
        render_backend = "opengl"
        print("Vulkan SDK not found; falling back to OpenGL.")
    end
end

local window_backend = get_config("window_backend") or "glfw"
local build_shared  = get_config("shared") and true or false
local build_apps    = get_config("apps") and true or false
local build_user    = get_config("user_apps") and true or false
local build_modules = get_config("modules") and true or false
local enable_markdown = get_config("markdown") and true or false
local vk_low_latency  = get_config("vulkan_low_latency") and true or false

print("EUI render backend: requested=%s, resolved=%s", get_config("render_backend") or "opengl", render_backend)
print("EUI window backend: %s", window_backend)

local app_main_source
if window_backend == "sdl2" then
    app_main_source = "core/app/sdl2_app_main.cpp"
else
    app_main_source = "core/app/glfw_app_main.cpp"
end

-- =============================================================================
-- External dependencies (resolved through xrepo)
-- =============================================================================

add_requires("freetype", {configs = {png = true, zlib = true, bzip2 = false, harfbuzz = false, brotli = false}})
add_requires("libpng", "zlib")

if window_backend == "glfw" then
    add_requires("glfw", {configs = {shared = false}})
end
if window_backend == "sdl2" then
    add_requires("libsdl2", {configs = {shared = false}})
end
if render_backend == "vulkan" then
    add_requires("vulkansdk", {system = true})
end
if not is_plat("windows", "mingw") then
    add_requires("libcurl", {configs = {shared = false}})
end

local bridge_source_flags = {}
if is_plat("macosx") then
    table.insert(bridge_source_flags, "-x")
    table.insert(bridge_source_flags, "objective-c")
end
-- =============================================================================
-- Vendored single-file third-party libraries (built directly from 3rd/)
-- =============================================================================

if render_backend == "opengl" then
    target("eui_glad")
        set_kind("static")
        add_files("3rd/glad/src/glad.c", {sourcekind = "cc"})
        add_includedirs("3rd/glad/include", {public = true})
    target_end()
end

if enable_markdown then
    target("eui_md4c")
        set_kind("static")
        add_files("3rd/md4c/src/md4c.c", {sourcekind = "cc"})
        add_includedirs("3rd/md4c/src", {public = true})
    target_end()
end
-- =============================================================================
-- Core library: eui_neo
-- =============================================================================

target("eui_neo")
    set_kind(build_shared and "shared" or "static")
    set_group("framework")

    add_files(
        "core/platform/async.cpp",
        "core/platform/json.cpp",
        "core/platform/network.cpp",
        "core/platform/performance_stats.cpp",
        "core/platform/platform.cpp",
        "core/render/image.cpp",
        "core/render/image_facade.cpp",
        "core/render/image_source.cpp",
        "core/render/primitive.cpp",
        "core/render/render_backend.cpp",
        "core/render/shadertoy.cpp",
        "core/render/shadertoy_json.cpp",
        "core/render/shadertoy_primitive.cpp",
        "core/render/stb_image_impl.cpp",
        "core/render/text.cpp",
        "core/window/window_backend.cpp",
        "core/window/window_input_backend.cpp"
    )

    add_files("3rd/yyjson-0.12.0/src/yyjson.c", {sourcekind = "cc"})
    add_files("core/platform/native_bridge.c", "core/platform/tray_bridge.c",
        {sourcekind = "cc", force = {cxflags = bridge_source_flags}})

    if render_backend == "opengl" then
        add_files(
            "core/render/opengl/opengl_backend.cpp",
            "core/render/opengl/opengl_image.cpp",
            "core/render/opengl/opengl_primitives.cpp",
            "core/render/opengl/opengl_shadertoy.cpp",
            "core/render/opengl/opengl_text.cpp"
        )
    elseif render_backend == "vulkan" then
        add_files(
            "core/render/vulkan/vulkan_backend.cpp",
            "core/render/vulkan/vulkan_cache.cpp",
            "core/render/vulkan/vulkan_primitives.cpp",
            "core/render/vulkan/vulkan_polygon.cpp",
            "core/render/vulkan/vulkan_shadertoy.cpp",
            "core/render/vulkan/vulkan_text.cpp",
            "core/render/vulkan/vulkan_image.cpp"
        )
    end

    if window_backend == "glfw" then
        add_files("core/platform/ime_bridge.c",
            {sourcekind = "cc", force = {cxflags = bridge_source_flags}})
    end

    add_includedirs("include", ".", "3rd/tray", {public = true})
    add_includedirs("3rd/yyjson-0.12.0/src")
    add_includedirs("3rd")

    add_defines("YYJSON_DISABLE_WRITER=1")
    if render_backend == "opengl" then
        add_defines("EUI_RENDER_BACKEND_OPENGL=1", {public = true})
    elseif render_backend == "vulkan" then
        add_defines("EUI_RENDER_BACKEND_VULKAN=1", {public = true})
        if vk_low_latency then
            add_defines("EUI_VULKAN_LOW_LATENCY_PRESENT=1")
        end
    end
    if window_backend == "sdl2" then
        add_defines("EUI_WINDOW_BACKEND_SDL2=1", {public = true})
    end

    if is_plat("windows") then
        add_defines("EUI_TRAY_WINAPI=1", "NOMINMAX", {public = true})
        add_syslinks("winmm", "urlmon", "shell32", "user32", "imm32", "pdh", "comdlg32", {public = true})
    elseif is_plat("macosx") then
        add_defines("EUI_TRAY_APPKIT=1", {public = true})
        add_frameworks("Cocoa", {public = true})
        add_syslinks("objc", {public = true})
    end

    if enable_markdown then
        add_defines("EUI_HAS_MD4C=1", {public = true})
        add_deps("eui_md4c")
    end

    add_packages("freetype", "libpng", "zlib", {public = true})
    if render_backend == "opengl" then
        add_deps("eui_glad")
        if is_plat("windows") then
            add_syslinks("opengl32", {public = true})
        elseif is_plat("linux") then
            add_syslinks("GL", {public = true})
        elseif is_plat("macosx") then
            add_frameworks("OpenGL", {public = true})
        end
    elseif render_backend == "vulkan" then
        add_packages("vulkansdk", {public = true})
    end
    if window_backend == "glfw" then
        add_packages("glfw", {public = true})
    elseif window_backend == "sdl2" then
        add_packages("libsdl2", {public = true})
    end
    add_packages("libcurl", {public = true, optional = true})
    if not is_plat("windows", "mingw") and has_package("libcurl") then
        add_defines("EUI_HAS_CURL=1", {public = true})
    end

    if not is_plat("windows", "mingw") then
        add_syslinks("pthread", {public = true})
    end

    -- Install rules (for `xmake install` and xrepo packaging)
    add_installfiles("include/(**)", {prefixdir = "include"})
    add_installfiles("components/(**.h)", {prefixdir = "include/components"})
    add_installfiles("core/(**.h)", {prefixdir = "include/core"})
    add_installfiles("3rd/stb_image.h", "3rd/nanosvg.h", "3rd/nanosvgrast.h", {prefixdir = "include/3rd"})
    add_installfiles("3rd/tray/tray.h", {prefixdir = "include/3rd/tray"})
    add_installfiles("scripts/EuiShaderToy.cmake", "scripts/generate_shadertoy_spirv.py",
        {prefixdir = "share/eui-neo/scripts"})
    if render_backend == "opengl" then
        add_installfiles("3rd/glad/include/(**.h)", {prefixdir = "include"})
    end
    if enable_markdown then
        add_installfiles("3rd/md4c/src/md4c.h", {prefixdir = "include"})
    end
    add_installfiles("assets/(**)", {prefixdir = "share/eui-neo/assets"})

    if build_shared and is_plat("windows", "mingw") then
        add_rules("utils.symbols.export_all")
    end
    on_config(eui_apply_compile_options)
target_end()

if render_backend == "vulkan" then
    target("eui_shadertoy_wrap")
        set_kind("binary")
        set_group("framework")
        add_files("scripts/shadertoy_wrap.cpp", "core/render/shadertoy.cpp")
        add_includedirs(".")
        if is_plat("mingw") then
            add_ldflags("-municode", {force = true})
        end
        on_config(eui_apply_compile_options)
    target_end()
end

target("eui_app")
    set_kind("static")
    set_group("framework")
    add_files(app_main_source)
    add_includedirs("include", ".")
    add_deps("eui_neo", {public = true})
    on_config(eui_apply_compile_options)
target_end()

target("eui_runtime_assets")
    set_kind("phony")
    set_group("framework")
    on_build(function(target)
        local assets_dir = path.join(os.projectdir(), "assets")
        if os.isdir(assets_dir) then
            local dest = path.join(target:targetdir(), "assets")
            os.tryrm(dest)
            os.cp(assets_dir, dest)
        end
    end)
target_end()
-- =============================================================================
-- Optional modules
-- =============================================================================

if build_modules then
    if os.exists("modules/keyboard/keyboard.h") then
        target("eui_module_keyboard")
            set_kind("headeronly")
            set_group("modules")
            add_includedirs("modules/keyboard", {public = true})
            add_deps("eui_neo")
            add_installfiles("modules/keyboard/(**.h)", {prefixdir = "include/modules/keyboard"})
        target_end()
    end

    if os.exists("modules/media/media.h") then
        target("eui_module_media")
            set_kind("headeronly")
            set_group("modules")
            add_includedirs("modules/media", {public = true})
            add_deps("eui_neo")
            add_installfiles("modules/media/(**.h)", {prefixdir = "include/modules/media"})
        target_end()
    end

    if os.exists("modules/serial/serial.h") then
        target("eui_module_serial")
            set_kind("static")
            set_group("modules")
            add_files("modules/serial/serial.cpp")
            add_includedirs("modules/serial", {public = true})
            add_deps("eui_neo")
            add_installfiles("modules/serial/(**.h)", {prefixdir = "include/modules/serial"})
            on_config(eui_apply_compile_options)
        target_end()
    end
end

rule("eui.app")
    on_load(function(target)
        target:add("deps", "eui_app", "eui_runtime_assets")
    end)
    on_config(eui_apply_app_options)
rule_end()

function eui_build_shadertoy_assets(target)
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

    local function compile(source, output)
        local source_path = path.absolute(source, os.projectdir())
        local output_path = path.absolute(output, target:targetdir())
        local wrapped_path = path.join(target:autogendir(), "shadertoy", path.filename(output) .. ".wrapped.frag")
        os.mkdir(path.directory(output_path))
        os.mkdir(path.directory(wrapped_path))
        os.vrunv(wrapper:targetfile(), {"--input", source_path, "--output", wrapped_path})
        os.vrunv(validator.program, {"-V", "-S", "frag", wrapped_path, "-o", output_path})
    end

    if target:name() == "gallery" then
        compile("assets/shaders/shadertoy/demo.frag",
            "assets/shaders/shadertoy/gallery_demo.frag.spv")
        return
    end

    compile("assets/shaders/shadertoy/demo.frag",
        "assets/shaders/shadertoy/demo.frag.spv")

    local python = find_tool("python3") or find_tool("python")
    assert(python, "Python 3 is required to generate Shadertoy SPIR-V from a config")
    for _, config in ipairs(os.files("assets/shaders/shadertoy/*/config.json")) do
        local stamp = path.join(target:autogendir(), "shadertoy",
            path.basename(path.directory(config)) .. ".config.stamp")
        os.mkdir(path.directory(stamp))
        os.vrunv(python.program, {
            path.join(os.projectdir(), "scripts", "generate_shadertoy_spirv.py"),
            "--config", path.absolute(config, os.projectdir()),
            "--asset-root", path.absolute("assets/shaders/shadertoy", os.projectdir()),
            "--output-root", path.absolute("assets/shaders/shadertoy", target:targetdir()),
            "--wrapper", wrapper:targetfile(),
            "--validator", validator.program,
            "--stamp", stamp
        })
    end
end

-- =============================================================================
-- Bundled example applications (examples/*.cpp)
-- =============================================================================

if build_apps then
    for _, file in ipairs(os.files("examples/*.cpp")) do
        local name = path.basename(file)
        if name == "keyboard" and not (build_modules and os.exists("modules/keyboard/keyboard.h")) then
            print("Skipping keyboard example (module not available).")
            goto continue
        end
        target(name)
            set_kind("binary")
            set_group("examples")
            add_files(file)
            add_rules("eui.app")
            add_includedirs("include", ".")
            if name == "serial_tool" and build_modules and os.exists("modules/serial/serial.h") then
                add_deps("eui_module_serial")
            end
            -- Shadertoy preset asset paths
            if name == "shadertoy" then
                add_defines(
                    "EUI_SHADERTOY_DEMO_SOURCE=\"assets/shaders/shadertoy/demo.frag\"",
                    "EUI_SHADERTOY_DEMO_SPIRV=\"assets/shaders/shadertoy/demo.frag.spv\"",
                    "EUI_SHADERTOY_PRESETS_DIR=\"assets/shaders/shadertoy\""
                )
                if render_backend == "vulkan" then
                    add_deps("eui_shadertoy_wrap")
                    after_build(eui_build_shadertoy_assets)
                end
            end
            if name == "gallery" then
                add_defines(
                    "EUI_GALLERY_SHADERTOY_SOURCE=\"assets/shaders/shadertoy/demo.frag\"",
                    "EUI_GALLERY_SHADERTOY_NOISE=\"assets/shaders/shadertoy/blackhole/color_noise.png\"",
                    "EUI_GALLERY_SHADERTOY_SPIRV=\"assets/shaders/shadertoy/gallery_demo.frag.spv\""
                )
                if render_backend == "vulkan" then
                    add_deps("eui_shadertoy_wrap")
                    after_build(eui_build_shadertoy_assets)
                end
            end
        target_end()
        ::continue::
    end
end

function eui_copy_user_app_assets(target)
    local app_assets = target:values("eui_app_assets")
    if app_assets and os.exists(app_assets) then
        os.cp(app_assets, path.join(target:targetdir(), "assets"))
    end
end

-- =============================================================================
-- User applications (apps/*.cpp and apps/<name>/app.cpp)
-- =============================================================================

if build_user then
    for _, file in ipairs(os.files("apps/*.cpp")) do
        local name = path.basename(file)
        target(name)
            set_kind("binary")
            set_group("apps")
            add_files(file)
            add_rules("eui.app")
            add_includedirs("include", ".")
        target_end()
    end

    for _, dir in ipairs(os.dirs("apps/*")) do
        local appfile = path.join(dir, "app.cpp")
        if os.exists(appfile) then
            local name = path.basename(dir)
            target(name)
                set_kind("binary")
                set_group("apps")
                add_files(path.join(dir, "**.cpp"))
                add_rules("eui.app")
                add_includedirs("include", ".", dir)
                set_values("eui_app_assets", path.absolute(path.join(dir, "assets"), os.projectdir()))
                after_build(eui_copy_user_app_assets)
            target_end()
        end
    end
end
