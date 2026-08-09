-- xmake.lua for EUI-NEO
-- A cross-platform, high-performance, low-overhead C++17 GPUI framework.
-- Converted from CMakeLists.txt to an xmake build.
-- Quick start:
--   xmake f -m release && xmake
--   xmake run gallery
-- Backend selection:
--   xmake f --window_backend=sdl2 --render_backend=vulkan
--   xmake f --shared=y

set_project("EUI-NEO")
set_version("0.5.6")
set_xmakever("2.9.0")
set_languages("c99", "cxx17")

-- xmake 3.x runs target/rule callbacks in isolated scopes where project
-- globals are nil, so shared helpers live in this module and are reached via
-- import() inside callbacks. See xmake/eui_helpers.lua.
add_moduledirs("xmake")

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
    import("eui_helpers").apply_compile_options(target)
end

function eui_apply_app_link_options(target)
    import("eui_helpers").apply_app_link_options(target)
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
    add_requires("sdl2", {configs = {shared = false}})
end
if render_backend == "vulkan" then
    add_requires("vulkan")
end
if not is_plat("windows", "mingw") then
    add_requires("libcurl", {configs = {shared = false}})
end

local c_source_flags = {}
if is_plat("windows") then
    table.insert(c_source_flags, "/TC")
end

local bridge_source_flags = table.copy(c_source_flags)
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
        add_files("3rd/glad/src/glad.c", {force = {cxflags = c_source_flags}})
        add_includedirs("3rd/glad/include", {public = true})
    target_end()
end

if enable_markdown then
    target("eui_md4c")
        set_kind("static")
        add_files("3rd/md4c/src/md4c.c", {force = {cxflags = c_source_flags}})
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

    add_files("3rd/yyjson-0.12.0/src/yyjson.c", {force = {cxflags = c_source_flags}})
    add_files("core/platform/native_bridge.c", "core/platform/tray_bridge.c",
        {force = {cxflags = bridge_source_flags}})

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
            {force = {cxflags = bridge_source_flags}})
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
        add_packages("vulkan", {public = true})
    end
    if window_backend == "glfw" then
        add_packages("glfw", {public = true})
    elseif window_backend == "sdl2" then
        add_packages("sdl2", {public = true})
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
    on_config(function(target)
        import("eui_helpers").apply_compile_options(target)
        import("eui_helpers").apply_app_link_options(target)
    end)
rule_end()

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
                    after_build(function(target)
                        local helpers = import("eui_helpers")
                        helpers.compile_shadertoy(target,
                            "assets/shaders/shadertoy/demo.frag",
                            "assets/shaders/shadertoy/demo.frag.spv")
                        for _, config in ipairs(os.files(
                            "assets/shaders/shadertoy/*/config.json")) do
                            helpers.compile_shadertoy_config(target,
                                config,
                                "assets/shaders/shadertoy",
                                "assets/shaders/shadertoy")
                        end
                    end)
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
                    after_build(function(target)
                        import("eui_helpers").compile_shadertoy(target,
                            "assets/shaders/shadertoy/demo.frag",
                            "assets/shaders/shadertoy/gallery_demo.frag.spv")
                    end)
                end
            end
        target_end()
        ::continue::
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
                after_build(function(target)
                    local app_assets = path.join(os.projectdir(), dir, "assets")
                    if os.exists(app_assets) then
                        os.cp(app_assets, path.join(target:targetdir(), "assets"))
                    end
                end)
            target_end()
        end
    end
end
