#include "core/platform/platform.h"
#include "core/render/shadertoy.h"

#include <filesystem>
#include <iostream>
#include <string>

namespace {

bool requireResource(const std::string& relativePath,
                     std::string& resolvedPath) {
    resolvedPath = core::platform::resolveResourcePath(relativePath);
    if (!resolvedPath.empty()) {
        return true;
    }
    std::cerr << "packaged resource did not resolve: " << relativePath << "\n";
    return false;
}

bool requirePreset(const std::string& name) {
    const std::string relative =
        "assets/shaders/shadertoy/" + name + "/config.json";
    std::string configPath;
    if (!requireResource(relative, configPath)) {
        return false;
    }

    core::render::ShaderToyGraph graph;
    core::render::ShaderToyError error;
    if (!core::render::loadShaderToyGraphJson(configPath, graph, error)) {
        std::cerr << "packaged preset failed to load: " << error.message << "\n";
        return false;
    }
    if (graph.passes.empty()) {
        std::cerr << "preset contains no passes: " << name << "\n";
        return false;
    }
    for (const core::render::ShaderToyPass& pass : graph.passes) {
        std::error_code errorCode;
        if (!std::filesystem::exists(
                std::filesystem::u8path(pass.fragmentPath), errorCode) ||
            errorCode) {
            std::cerr << "packaged fragment did not resolve: "
                      << pass.fragmentPath << "\n";
            return false;
        }
    }
    return true;
}

bool rejectPackagedIntermediates() {
    std::string rootPath;
    if (!requireResource("assets/shaders/shadertoy", rootPath)) {
        return false;
    }
    std::error_code error;
    for (std::filesystem::recursive_directory_iterator iterator(
             std::filesystem::u8path(rootPath), error), end;
         iterator != end && !error; iterator.increment(error)) {
        if (iterator->path().filename().u8string().find(".wrapped.frag") !=
            std::string::npos) {
            std::cerr << "build intermediate leaked into packaged assets: "
                      << iterator->path().u8string() << "\n";
            return false;
        }
    }
    if (error) {
        std::cerr << "failed to inspect packaged Shadertoy assets\n";
        return false;
    }
    return true;
}

#if defined(EUI_TEST_EXPECT_SHADERTOY_SPIRV)
bool requireSpirvResources() {
    std::string demoSpirv;
    if (!requireResource("assets/shaders/shadertoy/demo.frag.spv", demoSpirv)) {
        return false;
    }
    for (const char* name : {"blackhole", "fish"}) {
        std::string configPath;
        if (!requireResource(
                std::string("assets/shaders/shadertoy/") + name + "/config.json",
                configPath)) {
            return false;
        }
        core::render::ShaderToyGraph graph;
        core::render::ShaderToyError error;
        if (!core::render::loadShaderToyGraphJson(configPath, graph, error)) {
            return false;
        }
        for (const core::render::ShaderToyPass& pass : graph.passes) {
            std::error_code fileError;
            if (!std::filesystem::exists(
                    std::filesystem::u8path(pass.spirvPath), fileError) ||
                fileError) {
                std::cerr << "packaged SPIR-V did not resolve: "
                          << pass.spirvPath << "\n";
                return false;
            }
        }
    }
    return true;
}
#endif

} // namespace

int main() {
    std::error_code error;
    const std::filesystem::path original =
        std::filesystem::current_path(error);
    if (error) {
        std::cerr << "failed to capture current directory\n";
        return 1;
    }

    const std::filesystem::path unrelated =
        std::filesystem::temp_directory_path(error) /
        "eui_neo_shadertoy_packaged_assets_test";
    if (error) {
        std::cerr << "failed to locate temporary directory\n";
        return 1;
    }
    std::filesystem::remove_all(unrelated, error);
    if (error) {
        std::cerr << "failed to clear temporary directory\n";
        return 1;
    }
    std::filesystem::create_directories(unrelated, error);
    if (error) {
        std::cerr << "failed to create temporary directory\n";
        return 1;
    }
    std::filesystem::current_path(unrelated, error);
    if (error) {
        std::cerr << "failed to change current directory\n";
        return 1;
    }

    std::string resolved;
    const bool valid =
        requireResource("assets/shaders/shadertoy/demo.frag", resolved) &&
        requireResource(
            "assets/shaders/shadertoy/blackhole/color_noise.png",
            resolved) &&
        requirePreset("blackhole") &&
        requirePreset("fish") &&
        rejectPackagedIntermediates()
#if defined(EUI_TEST_EXPECT_SHADERTOY_SPIRV)
        && requireSpirvResources()
#endif
        ;

    std::filesystem::current_path(original, error);
    if (error) {
        std::cerr << "failed to restore current directory\n";
        return 1;
    }
    std::filesystem::remove_all(unrelated, error);
    if (error) {
        std::cerr << "failed to remove temporary directory\n";
        return 1;
    }
    return valid ? 0 : 1;
}
